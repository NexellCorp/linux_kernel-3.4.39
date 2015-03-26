#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <asm/uaccess.h>

#include <linux/dma-buf.h>
#include <linux/nxp_ion.h>
#include <linux/ion.h>
#include <linux/cma.h>
#include <../drivers/gpu/ion/ion_priv.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>


#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>
#include <mach/nxp-backward-camera.h>

extern struct ion_device *get_global_ion_device(void);

#define SPLASH_TAG			"splash"
#define SPLASH_TAG_SIZE		6
#define	IMAGE_SIZE_MAX		40
#define HEADER_SIZE         2048

#define USE_BZIP2
/*#define USE_LZ4*/

#if defined(USE_BZIP2)
#include <linux/decompress/bunzip2.h>
#elif defined(USE_LZ4)
#include <linux/decompress/unlz4.h>
#else
#error "You must define USE_BZIP2 or USE_LZ4"
#endif

typedef struct SPLASH_IMAGE_INFO{
	unsigned char	ucImageName[16];
	unsigned int 	ulImageAddr;
	unsigned int	ulImageSize;
	unsigned int	ulImageWidth;
	unsigned int	ulImageHeight;
	unsigned int 	Padding;
	unsigned char	ucRev[12];
} SPLASH_IMAGE_INFO;

typedef struct SPLASH_IMAGE_Header {
	unsigned char ucPartition[8];
	unsigned int  ulNumber;
	unsigned char ucRev[4];
} SPLASH_IMAGE_Header;

static struct fine_boot_context {
    /* ion allocation */
    struct ion_client *ion_client;
    struct ion_handle *ion_handle;
    struct dma_buf    *dma_buf;
    dma_addr_t         dma_addr;
    void              *virt;
    int                alloc_size;

    /* image info */
    char header[HEADER_SIZE];
    SPLASH_IMAGE_INFO *splash_info;

    int img_width;
    int img_height;
    int img_count;

    /* thread */
    struct task_struct *anim_task;
    struct task_struct *load_task;

    bool is_valid;

    /* backward camera */
    struct platform_device *camera;
} _context;


static int _alloc_ion_buffer(struct fine_boot_context *me, bool include_header)
{
    struct ion_device *ion_dev = get_global_ion_device();
    int size = 0;
    struct ion_buffer *ion_buffer;

    me->ion_client = ion_client_create(ion_dev, "fine-boot");
    if (IS_ERR(me->ion_client)) {
        pr_err("%s Error: ion_client_create()\n", __func__);
        return -EINVAL;
    }

    size = me->img_width * me->img_height * me->img_count * 4;
    if (include_header)
        size += HEADER_SIZE;
    me->alloc_size = PAGE_ALIGN(size);
    printk("%s: allocation size %d/%d\n", __func__, me->alloc_size, size);

    me->ion_handle = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
    if (IS_ERR(me->ion_handle)) {
         pr_err("%s Error: ion_alloc()\n", __func__);
         return -ENOMEM;
    }

    me->dma_buf = ion_share_dma_buf(me->ion_client, me->ion_handle);
    if (IS_ERR_OR_NULL(me->dma_buf)) {
         pr_err("%s Error: fail to ion_share_dma_buf()\n", __func__);
         return -EINVAL;
    }

    ion_buffer = me->dma_buf->priv;
    me->dma_addr = ion_buffer->priv_phys;
    me->virt = cma_get_virt(me->dma_addr, me->alloc_size, 1);
    printk("%s: dma_addr 0x%x, virt %p\n", __func__, me->dma_addr, me->virt);

    return 0;
}

static void _free_ion_buffer(struct fine_boot_context *me)
{
    if (me->dma_buf != NULL) {
        dma_buf_put(me->dma_buf);
        me->dma_buf = NULL;
        ion_free(me->ion_client, me->ion_handle);
        me->ion_handle = NULL;
    }
}

static int _check_header(struct fine_boot_context *me)
{
    SPLASH_IMAGE_Header *splash_header;
    SPLASH_IMAGE_INFO *splash_info;
    int count, width, height, address;
    struct file *filp = NULL;
    int ret = 0;
    char *filename = "splash.img";

    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);

    filp = filp_open(filename, O_RDONLY | O_LARGEFILE, 0);
    if (IS_ERR(filp)) {
        printk(KERN_ERR "%s: open failed %s\n", __func__, filename);
        ret = -1;
        goto OUT;
    }

    ret = vfs_read(filp, me->header, HEADER_SIZE, &filp->f_pos);
    if (ret != HEADER_SIZE) {
        ret = -1;
        printk(KERN_ERR "%s error: read size %d/%d\n", __func__, ret, HEADER_SIZE);
        goto OUT;
    }

    splash_header = (SPLASH_IMAGE_Header *)me->header;
    if (strncmp(SPLASH_TAG, (char*)splash_header->ucPartition, SPLASH_TAG_SIZE)) {
        printk(KERN_ERR "can't find splash image at %p ...\n", splash_header);
        ret = -1;
        goto OUT;
    }

    count = splash_header->ulNumber;
    if (count > IMAGE_SIZE_MAX) {
        printk(KERN_ERR "splash images %d is over max %d ...\n", count, IMAGE_SIZE_MAX);
        ret = -1;
        goto OUT;
    }

    splash_info = (SPLASH_IMAGE_INFO*)(me->header + sizeof(*splash_header));
    width = splash_info->ulImageWidth;
    height = splash_info->ulImageHeight;
    address = splash_info->ulImageAddr;

    printk("splash %d * %d (%dEA) starting from offset %d... ret %d\n", width, height, count, address, ret);

    me->splash_info = splash_info;
    me->img_count = count;
    me->img_width = width;
    me->img_height = height;

OUT:
    if (filp)
        filp_close(filp, NULL);
    set_fs(old_fs);

    return ret;
}

static int _load_thread(void *arg)
{
    struct fine_boot_context *me = (struct fine_boot_context *)arg;
    struct file *filp = NULL;
#ifdef USE_BZIP2
    char *filename = "splash.img.bz2";
#else
    char *filename = "splash.img.lz4";
#endif
    int ret;
    int decompress_size = 0;
    char *buf = NULL;

    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);

    printk("%s entered\n", __func__);
    filp = filp_open(filename, O_RDONLY | O_LARGEFILE, 0);
    if (IS_ERR(filp)) {
        printk("%s: open failed %s\n", __func__, filename);
    } else {
        int file_size = 0;
        vfs_llseek(filp, 0, SEEK_SET);
        file_size = vfs_llseek(filp, 0, SEEK_END);
        printk("filesize: %d\n", file_size);
        buf = kmalloc(file_size, GFP_KERNEL);
        if (!buf) {
            printk("%s: failed to alloc file buffer\n", __func__);
            goto OUT;
        }

        vfs_llseek(filp, 0, SEEK_SET);
        ret = vfs_read(filp, buf, file_size, &filp->f_pos);
        if (ret != file_size) {
            printk("%s error: read size %d/%d\n", __func__, ret, file_size);
            goto OUT;
        }

        ret = _alloc_ion_buffer(me, true);
        if (ret < 0) {
            printk("%s: failed to out buffer\n", __func__);
            goto OUT;
        }

#ifdef USE_BZIP2
        ret = bunzip2(buf, file_size, NULL, NULL, me->virt, &decompress_size, NULL);
        if (ret < 0) {
            printk("%s: failed to bunzip2(ret: %d)\n", __func__, ret);
            goto OUT;
        }
#else
        ret = unlz4(buf, file_size, NULL, NULL, me->virt, &decompress_size, NULL);
        if (ret < 0) {
            printk("%s: failed to unlz4(ret: %d)\n", __func__, ret);
            me->load_task = NULL;
            goto OUT;
        }
#endif

        printk("%s: succeed to decompress, size %d\n", __func__, decompress_size);
    }

OUT:
    me->load_task = NULL;
    if (buf)
        kfree(buf);
    if (filp)
        filp_close(filp, NULL);
    set_fs(old_fs);

    return 0;
}

#define DISP_MODULE 0
static int _anim_thread(void *arg)
{
    struct fine_boot_context *me = (struct fine_boot_context *)arg;
    int count = 0;
    dma_addr_t address;
    SPLASH_IMAGE_INFO *splash;
    bool first = true;

    // wait 200ms
    schedule_timeout_interruptible(HZ/5);

    printk("%s: %dx%d, buffer 0x%x\n", __func__, me->img_width, me->img_height, me->dma_addr);
    nxp_soc_disp_rgb_set_format(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, NX_MLC_RGBFMT_X8R8G8B8, me->img_width, me->img_height, 4);
    while(1) {
        splash = &me->splash_info[count++%me->img_count];
        address = me->dma_addr + splash->ulImageAddr;
        nxp_soc_disp_rgb_set_address(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, address, 4, me->img_width * 4, 1);
        if (first) {
            nxp_soc_disp_device_enable_all(DISP_MODULE, 1);
            first = false;
        }
        schedule_timeout_interruptible(HZ/100);
        if (kthread_should_stop()) {
            break;
        }
    }
    nxp_soc_disp_rgb_set_format(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, NX_MLC_RGBFMT_A8R8G8B8, me->img_width, me->img_height, 4);

    _free_ion_buffer(me);
    return 0;
}

static int _start_load(void *arg)
{
    struct fine_boot_context *me = &_context;
    if (_check_header(me) < 0) {
        printk(KERN_ERR "invalid image?\n");
        return -1;
    }
    me->is_valid = true;
    me->load_task = kthread_run(_load_thread, me, "fine-bootload");
    return 0;
}

void start_fine_load(void)
{
     /*kthread_run(_start_load, &_context, "fine-boot-load");*/
    struct fine_boot_context *me = &_context;
    _start_load(&_context);
    if (me->camera)
        platform_device_register(me->camera);
}

void start_fine_animation(void)
{
    struct fine_boot_context *me = &_context;
    if (me->is_valid)
        me->anim_task = kthread_run(_anim_thread, me, "fine-boot-animation");
}

void stop_fine_boot(void)
{
    struct fine_boot_context *me = &_context;
    printk("stop fine boot\n");
    if (me->anim_task) {
        printk(KERN_ERR "stop fine boot animation\n");
        kthread_stop(me->anim_task);
        me->anim_task = NULL;
    }
    if (me->load_task) {
        printk(KERN_ERR "stop fine boot loading\n");
        kthread_stop(me->load_task);
        me->load_task = NULL;
    }
}

void register_backward_camera(struct platform_device *device)
{
    struct fine_boot_context *me = &_context;
    me->camera = device;
}

EXPORT_SYMBOL(start_fine_load);
EXPORT_SYMBOL(start_fine_animation);
EXPORT_SYMBOL(stop_fine_boot);
EXPORT_SYMBOL(register_backward_camera);

static ssize_t _stop_boot_animation(struct device *pdev,
        struct device_attribute *attr, const char *buf, size_t n)
{
    stop_fine_boot();
    return n;
}

static struct device_attribute fineboot_attr = __ATTR(stop, 0664, NULL, _stop_boot_animation);
static struct attribute *attrs[] = {
    &fineboot_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = (struct attribute **)attrs,
};

static int _create_sysfs(void)
{
    struct kobject *kobj = NULL;
    int ret = 0;

    kobj = kobject_create_and_add("fineboot", &platform_bus.kobj);
    if (! kobj) {
        printk(KERN_ERR "Fail, create kobject for fineboot\n");
        return -ret;
    }

    ret = sysfs_create_group(kobj, &attr_group);
    if (ret) {
        printk(KERN_ERR "Fail, create sysfs group for fineboot\n");
        kobject_del(kobj);
        return -ret;
    }

    return 0;
}

static int __init register_stop_interface(void)
{
    if (_create_sysfs()) {
        printk(KERN_ERR "%s: failed to create sysfs()\n", __func__);
        return -1;
    }
    return 0;
}

late_initcall(register_stop_interface);
