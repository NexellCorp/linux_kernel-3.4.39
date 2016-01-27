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
#include "draw_lcd.c"
#include "draw_bmp.c"


extern struct ion_device *get_global_ion_device(void);

#define HEADER_SIZE         2048

#define DISP_MODULE 0
#define MAX_BMP_FILES   2

#define OS_VERSION_DISPLAY_X        604
#define OS_VERSION_DISPLAY_Y        458
#define OS_VERSION_DISPLAY_TEXT_COLOR   0xFFFFFFFF
#define OS_VERSION_DISPLAY_BACK_COLOR   0xFF000000
#define OS_VERSION_DISPLAY_ALPHA    1




struct bmp_alloc_context {
    struct ion_handle *ion_handle;
    struct dma_buf    *dma_buf;
    dma_addr_t         dma_addr;
    void              *virt;
};


static struct android_boot_context {
    /* ion allocation */
    struct ion_client *ion_client;
    struct ion_handle *ion_handle;
    struct dma_buf    *dma_buf;
    dma_addr_t         dma_addr;
    void              *virt;
    int                alloc_size;

	 /* buffer for bmp */
    int bmp_index;
    struct bmp_alloc_context bmp_alloc_context[MAX_BMP_FILES];

    /* image info */
    char header[HEADER_SIZE];

    int img_width;
    int img_height;
    int img_count;

    /* thread */
    struct task_struct *anim_task;
    struct task_struct *load_task;

    bool is_valid;

    /* backward camera */
    struct platform_device *camera;
    /* os version */
    char *os_version;

} _context;


static int _alloc_ion_buffer(struct android_boot_context *me, bool include_header)
{
    struct ion_device *ion_dev = get_global_ion_device();
    int size = 0;
    struct ion_buffer *ion_buffer;

    me->ion_client = ion_client_create(ion_dev, "android-boot");
    if (IS_ERR(me->ion_client)) {
        pr_err("%s Error: ion_client_create()\n", __func__);
        return -EINVAL;
    }

    size = me->img_width * me->img_height * 4;
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

static void _free_ion_buffer(struct android_boot_context *me)
{
    if (me->dma_buf != NULL) {
        dma_buf_put(me->dma_buf);
        me->dma_buf = NULL;
        ion_free(me->ion_client, me->ion_handle);
        me->ion_handle = NULL;
    }
}
static int _load_bmp(char *filename, char **pbuf)
{
    struct file *filp = NULL;
    int ret = 0;
    char *buf = NULL;

    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);

    printk("%s, %s\n", __func__, filename);
    filp = filp_open(filename, O_RDONLY | O_LARGEFILE, 0);
    if (IS_ERR(filp)) {
        printk("%s: open failed %s\n", __func__, filename);
        ret = -EINVAL;
        goto OUT;
    } else {
        int file_size = 0;
        vfs_llseek(filp, 0, SEEK_SET);
        file_size = vfs_llseek(filp, 0, SEEK_END);
        printk("filesize: %d\n", file_size);
        buf = kmalloc(file_size, GFP_KERNEL);
        if (!buf) {
            printk("%s: failed to alloc file buffer\n", __func__);
            ret = -ENOMEM;
            goto OUT;
        }

        vfs_llseek(filp, 0, SEEK_SET);
        ret = vfs_read(filp, buf, file_size, &filp->f_pos);
        if (ret != file_size) {
            printk("%s error: read size %d/%d\n", __func__, ret, file_size);
            ret = -EINVAL;
            goto OUT;
        }
    }

    *pbuf= buf;
    ret = 0;

OUT:
    if (filp)
        filp_close(filp, NULL);
    set_fs(old_fs);

    return ret;
}
static int _alloc_bmp_buffer(struct android_boot_context *me, int index)
{
    int size = 0;
    struct ion_buffer *ion_buffer;
    struct bmp_alloc_context *alloc_context = &me->bmp_alloc_context[index];

    if (!me->ion_client) {
        me->ion_client = ion_client_create(get_global_ion_device(), "android-boot");
        if (IS_ERR(me->ion_client)) {
            pr_err("%s Error: ion_client_create()\n", __func__);
            return -EINVAL;
        }
    }

    size = PAGE_ALIGN(CFG_DISP_PRI_RESOL_WIDTH * CFG_DISP_PRI_RESOL_HEIGHT * 4);
    printk("%s: allocation size %d\n", __func__, size);

   alloc_context->ion_handle = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
    if (IS_ERR(alloc_context->ion_handle)) {
         pr_err("%s Error: ion_alloc()\n", __func__);
         return -ENOMEM;
    }

    alloc_context->dma_buf = ion_share_dma_buf(me->ion_client, alloc_context->ion_handle);
    if (IS_ERR_OR_NULL(alloc_context->dma_buf)) {
         pr_err("%s Error: fail to ion_share_dma_buf()\n", __func__);
         return -EINVAL;
    }

    ion_buffer = alloc_context->dma_buf->priv;
    alloc_context->dma_addr = ion_buffer->priv_phys;
    alloc_context->virt = cma_get_virt(alloc_context->dma_addr, size, 1);
    printk("%s: dma_addr 0x%x, virt %p\n", __func__, alloc_context->dma_addr, alloc_context->virt);

    return 0;
}

static void _free_bmp_buffer(struct android_boot_context *me)
{
    int i;
    struct bmp_alloc_context *alloc_context = NULL;
    for (i = 0; i < MAX_BMP_FILES; i++) {
        alloc_context = &me->bmp_alloc_context[i];
        if (alloc_context->dma_buf != NULL) {
            dma_buf_put(alloc_context->dma_buf);
            alloc_context->dma_buf = NULL;
            ion_free(me->ion_client, alloc_context->ion_handle);
            alloc_context->ion_handle = NULL;
        }
    }
}

static void _display_on(struct android_boot_context *me)
{
    nxp_soc_disp_device_enable_all(DISP_MODULE, 1);
}

static void _display_bmp(struct android_boot_context *me, char *filename)
{
    char *buf = NULL;
    int ret;
    bool first = me->bmp_index == 0;

    if (0 == _load_bmp(filename, &buf)) {
        u32 layer = CFG_DISP_PRI_SCREEN_LAYER;
        u32 width = CFG_DISP_PRI_RESOL_WIDTH;
        u32 height = CFG_DISP_PRI_RESOL_HEIGHT;

        ret = _alloc_bmp_buffer(me, me->bmp_index);
        if (ret) {
            printk(KERN_ERR "%s: failed to _alloc_bmp_buffer for %d\n", __func__, me->bmp_index);
            goto OUT;
        }

        lcd_set_logo_bmp_addr((unsigned long)buf);
        lcd_draw_boot_logo((unsigned long)me->bmp_alloc_context[me->bmp_index].virt, CFG_DISP_PRI_RESOL_WIDTH, CFG_DISP_PRI_RESOL_HEIGHT, 4);
        nxp_soc_disp_rgb_set_format(DISP_MODULE, layer, NX_MLC_RGBFMT_X8R8G8B8, width, height, 4);
        nxp_soc_disp_rgb_set_address(DISP_MODULE, layer, me->bmp_alloc_context[me->bmp_index].dma_addr, 4, width * 4, 1);
        if (first) {
            _display_on(me);
            first = false;
        }
    }

OUT:
    if (buf)
        kfree(buf);

    me->bmp_index++;
}

static void _display_bmp_loaded(struct android_boot_context *me, int index)
{
    u32 layer = CFG_DISP_PRI_SCREEN_LAYER;
    u32 width = CFG_DISP_PRI_RESOL_WIDTH;
    nxp_soc_disp_rgb_set_address(DISP_MODULE, layer, me->bmp_alloc_context[index].dma_addr, 4, width * 4, 1);
}


static int _load_thread(void *arg)
{
    struct android_boot_context *me = (struct android_boot_context *)arg;
    int ret;
    printk("%s entered\n", __func__);
        ret = _alloc_ion_buffer(me, false);
        if (ret < 0) {
            printk("%s: failed to out buffer\n", __func__);
            goto OUT;
        }

OUT:
    me->load_task = NULL;
    return 0;
}

#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
extern bool is_backward_camera_on(void);
extern void backward_camera_external_on(void);
#endif

#ifdef CONFIG_PLAT_S5P4418_X1DASH
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
struct pwm_device {
	struct list_head list;
	struct device *dev;
	const char *label;
	unsigned int period_ns;
	unsigned int duty_ns;
	unsigned char use_count;
	unsigned char pwm_id;
};
static void pwm_set_init(void)
{
	struct pwm_device pwm;
	int brightness = 100;
	int max = 255;

	pwm.pwm_id = CFG_LCD_PRI_PWM_CH;
	brightness = (brightness * (1000000000/CFG_LCD_PRI_PWM_FREQ) / max);

	pwm_config(&pwm, brightness, (1000000000/CFG_LCD_PRI_PWM_FREQ));
	pwm_enable(&pwm);

	return;
}

#endif

#define SECOND_STAGE_START_FRAME    8
#define SECOND_STAGE_FRAME_COUNT    12
static int _anim_thread(void *arg)
{
    struct android_boot_context *me = (struct android_boot_context *)arg;
    int count = 0;
    dma_addr_t address;
    bool first = true;
    int loop_count = 0;

    // wait 200ms
    schedule_timeout_interruptible(HZ/5);

    printk("%s: %dx%d, buffer 0x%x\n", __func__, me->img_width, me->img_height, me->dma_addr);
    nxp_soc_disp_rgb_set_format(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, NX_MLC_RGBFMT_X8R8G8B8, me->img_width, me->img_height, 4);

	_display_bmp(me, "logo.bmp");

	while(1) {
        if (kthread_should_stop()) {
            goto OUT_ANIM;
        }
	}
OUT_ANIM:
        nxp_soc_disp_rgb_set_format(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, NX_MLC_RGBFMT_A8R8G8B8, me->img_width, me->img_height, 4);
        nxp_soc_disp_rgb_set_color(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, RGB_COLOR_ALPHA, 0x0, false);

        _free_ion_buffer(me);
        _free_bmp_buffer(me);

    return 0;
}

static int _start_load(void *arg)
{
    struct android_boot_context *me = &_context;
	char *os_version = NULL;

    me->img_width = CFG_DISP_PRI_RESOL_WIDTH;
    me->img_height = CFG_DISP_PRI_RESOL_HEIGHT;

	//TODO add read os version function

    if (!os_version) {
		os_version = "2016.00.00 00:00:00";
	}

    me->is_valid = true;
    me->load_task = kthread_run(_load_thread, me, "android-bootload");
    return 0;
}


void start_android_logo_load(void)
{
    struct android_boot_context *me = &_context;
    _start_load(&_context);
    if (me->camera)
        platform_device_register(me->camera);
}

void start_android_logo_animation(void)
{
    struct android_boot_context *me = &_context;
    printk("start android logo boot\n");
    if (me->is_valid)
        me->anim_task = kthread_run(_anim_thread, me, "android-boot-animation");
}

void stop_android_boot(void)
{
    struct android_boot_context *me = &_context;
    printk("stop android logo boot\n");
    if (me->anim_task) {
        printk(KERN_ERR "stop android logo boot animation\n");
        kthread_stop(me->anim_task);
        me->anim_task = NULL;
    }
    if (me->load_task) {
        printk(KERN_ERR "stop android logo boot loading\n");
        kthread_stop(me->load_task);
        me->load_task = NULL;
    }
}

void register_backward_camera(struct platform_device *device)
{
    struct android_boot_context *me = &_context;
    me->camera = device;
}

bool is_android_boot_animation_run(void)
{
    struct android_boot_context *me = &_context;
    if (me->anim_task)
        return true;
    return false;
}

EXPORT_SYMBOL(start_android_logo_load);
EXPORT_SYMBOL(start_android_logo_animation);
EXPORT_SYMBOL(stop_android_boot);
EXPORT_SYMBOL(register_backward_camera);
EXPORT_SYMBOL(is_android_boot_animation_run);

static ssize_t _stop_boot_animation(struct device *pdev,
        struct device_attribute *attr, const char *buf, size_t n)
{
    stop_android_boot();
    return n;
}

static struct device_attribute android_logo_attr = __ATTR(stop, 0664, NULL, _stop_boot_animation);
static struct attribute *attrs[] = {
    &android_logo_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = (struct attribute **)attrs,
};

static int _create_sysfs(void)
{
    struct kobject *kobj = NULL;
    int ret = 0;

    kobj = kobject_create_and_add("android_logo", &platform_bus.kobj);
    if (! kobj) {
        printk(KERN_ERR "Fail, create kobject for android_logo\n");
        return -ret;
    }

    ret = sysfs_create_group(kobj, &attr_group);
    if (ret) {
        printk(KERN_ERR "Fail, create sysfs group for android_logo\n");
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
