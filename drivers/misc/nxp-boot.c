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

#include <linux/gpio.h>

#include "draw_lcd.c"
#include "draw_bmp.c"

#define MAX_BMP_FILES	1//40
#define FPS_BMP			10

#define PREFIX_NAME		"/logo"

#define DISP_MODULE 0
#define MLC_RGB_SCREEN_LAYER	0

#define OS_VERSION_DISPLAY_X        840
#define OS_VERSION_DISPLAY_Y        580
#define OS_VERSION_DISPLAY_TEXT_COLOR   0xFFFFFFFF
#define OS_VERSION_DISPLAY_BACK_COLOR   0xFF000000
#define OS_VERSION_DISPLAY_ALPHA    1

struct bmp_alloc_context {
    struct ion_handle *ion_handle;
    struct dma_buf    *dma_buf;
    dma_addr_t         dma_addr;
    void              *virt;
};


static struct nxp_boot_context {
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

    int img_width;
    int img_height;
    int img_count;

    /* thread */
    struct task_struct *anim_task;
    struct task_struct *load_task;

    bool is_load;
    bool is_load_fail;
    bool is_valid;

    /* backward camera */
#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
    struct platform_device *camera;
#endif

    /* os version */
    char *os_version;

    /* user booting image */
    bool use_user_boot_image;
    bool load_user_boot_image;
} _context;

extern struct ion_device *get_global_ion_device(void);
#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
extern bool is_backward_camera_on(void);
#endif

static int _load_bmp(char *filename, char **pbuf)
{
	struct file *filp = NULL;
	int ret = 0;
	char *buf = NULL;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	printk("%s, %s\n", __func__, filename);

	filp = filp_open(filename, O_RDONLY | O_LARGEFILE, 0);
	if (IS_ERR(filp))
	{
		printk(KERN_ERR"%s: open failed %s\n", __func__, filename);
		ret = -EINVAL;
		filp = NULL;
		goto OUT;
	}
	else
	{
		int file_size = 0;
		vfs_llseek(filp, 0, SEEK_SET);
		file_size = vfs_llseek(filp, 0, SEEK_END);
		printk("filesize: %d\n", file_size);
		buf = kmalloc(file_size, GFP_KERNEL);
		if (!buf) {
			printk(KERN_ERR"%s: failed to alloc file buffer\n", __func__);
			ret = -ENOMEM;
			goto OUT;
		}

		vfs_llseek(filp, 0, SEEK_SET);
		ret = vfs_read(filp, buf, file_size, &filp->f_pos);
		if (ret != file_size) {
			printk(KERN_ERR"%s error: read size %d/%d\n", __func__, ret, file_size);
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

static int _alloc_bmp_buffer(struct nxp_boot_context *me, int index)
{
	int size = 0;
	struct ion_buffer *ion_buffer;
	struct bmp_alloc_context *alloc_context = &me->bmp_alloc_context[index];

	if (!me->ion_client) {
		me->ion_client = ion_client_create(get_global_ion_device(), "nxp-boot");
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
	memset(alloc_context->virt, 0x00, size);
	printk("%s: dma_addr 0x%x, virt %p\n", __func__, alloc_context->dma_addr, alloc_context->virt);

	return 0;
}

static void _free_bmp_buffer(struct nxp_boot_context *me)
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


static void _display_bmp_index(struct nxp_boot_context *me, int bmp_index)
{
	u32 layer = MLC_RGB_SCREEN_LAYER;
	u32 width = CFG_DISP_PRI_RESOL_WIDTH;
	u32 height = CFG_DISP_PRI_RESOL_HEIGHT;

#if 0
	// draw os version
	{
		unsigned int cpu_address;
		lcd_info lcd = {
			.bit_per_pixel = 32,
			.lcd_width = 1024,
			.lcd_height = 600,
			.back_color = OS_VERSION_DISPLAY_BACK_COLOR,
			.text_color = OS_VERSION_DISPLAY_TEXT_COLOR,
			.alphablend = OS_VERSION_DISPLAY_ALPHA,
		};
		lcd_debug_init(&lcd);

		cpu_address = (unsigned int)(me->bmp_alloc_context[bmp_index].virt);
		lcd_set_framebuffer(cpu_address);
		lcd_draw_text(me->os_version, OS_VERSION_DISPLAY_X, OS_VERSION_DISPLAY_Y, 1, 1, 1);
	}
#endif

	nxp_soc_disp_rgb_set_format(DISP_MODULE, layer, NX_MLC_RGBFMT_X8R8G8B8, width, height, 4);
	nxp_soc_disp_rgb_set_address(DISP_MODULE, layer, me->bmp_alloc_context[bmp_index].dma_addr, 4, width * 4, 1);
}


static int _load_thread_bmp(void *arg)
{
	struct nxp_boot_context *me = (struct nxp_boot_context *)arg;
	char filename[32];
	char *buf = NULL;
	int ret = 0;
	int i = 0;
    BITMAPINFOHEADER  BMPInfo  = { 0, };

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	for (me->bmp_index=0; me->bmp_index<MAX_BMP_FILES; me->bmp_index++)
	{
		sprintf(filename, "%s%02d.bmp", PREFIX_NAME, i);

		ret = _load_bmp(filename, &buf);

		if (ret == 0)
		{
			ret = _alloc_bmp_buffer(me, me->bmp_index);
			if (ret) {
				printk(KERN_ERR "%s: failed to _alloc_bmp_buffer for %d\n", __func__, me->bmp_index);
				goto OUT;
			}

			lcd_set_logo_bmp_addr((unsigned long)buf);
			lcd_draw_boot_logo((unsigned long)me->bmp_alloc_context[me->bmp_index].virt, CFG_DISP_PRI_RESOL_WIDTH, CFG_DISP_PRI_RESOL_HEIGHT, 4, &BMPInfo);
		}
		else
		{
			printk(KERN_ERR "%s: \e[31m%s load fail. \e[0m \n", __FUNCTION__, filename);
			me->is_load_fail = true;
			break;
		}

		if(buf != NULL)
			kfree(buf);
	}

OUT:
	me->load_task = NULL;
	set_fs(old_fs);

	return 0;
}
static int _anim_thread(void *arg)
{
	struct nxp_boot_context *me = (struct nxp_boot_context *)arg;
	int bmp_index = 0;

	printk(KERN_ERR "## [%s():%s:%d\t] start boot animation.\n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__);

	while (1) {
		if ((me->bmp_index >= MAX_BMP_FILES)
#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
			&& !is_backward_camera_on()
#endif
			) {

			if (me->bmp_index == MAX_BMP_FILES)
				_display_bmp_index(me, 0);
			else
				_display_bmp_index(me, bmp_index);

			if (bmp_index == 0) {
				nxp_soc_disp_rgb_set_color(DISP_MODULE, MLC_RGB_SCREEN_LAYER, RGB_COLOR_ALPHA, 255, 1);
				nxp_soc_disp_rgb_set_enable(DISP_MODULE, MLC_RGB_SCREEN_LAYER, 1);
				nxp_soc_disp_rgb_set_enable(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, 0);
			}

			if (bmp_index <= (MAX_BMP_FILES-1))
				bmp_index++;
		}

		if (kthread_should_stop()
			|| (me->is_load_fail == true)
			) {
#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
			if(!is_backward_camera_on())
#endif
			{
				nxp_soc_disp_rgb_set_enable(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, 1);
				nxp_soc_disp_rgb_set_format(DISP_MODULE, MLC_RGB_SCREEN_LAYER, NX_MLC_RGBFMT_A8R8G8B8, me->img_width, me->img_height, 4);
				nxp_soc_disp_rgb_set_color(DISP_MODULE, MLC_RGB_SCREEN_LAYER, RGB_COLOR_ALPHA, 0x10, true);
				nxp_soc_disp_rgb_set_enable(DISP_MODULE, MLC_RGB_SCREEN_LAYER, 0);
			}
			_free_bmp_buffer(me);
			printk(KERN_ERR "## [%s():%s:%d\t] stop boot animation.\n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__);
			break;
		}

		schedule_timeout_interruptible(HZ/FPS_BMP);
	}

	return 0;
}

static int _start_load(void *arg)
{
	struct nxp_boot_context *me = &_context;
	char *os_version = "2016.07.27 v2.0";

	if (me->load_task == NULL) {
		me->os_version = os_version;
		printk("%s: os_version %s\n", __func__, os_version);
		me->is_valid = true;
		me->is_load_fail = false;
		me->load_task = kthread_run(_load_thread_bmp, me, "nxp-bootload");
	}

	return 0;
}

void start_nxp_load(void)
{
	struct nxp_boot_context *me = &_context;

	_start_load(&_context);
#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
	if (me->camera)
		platform_device_register(me->camera);
#endif
}


void start_nxp_animation(void)
{
	struct nxp_boot_context *me = &_context;

	if (me->anim_task == NULL) {
		me->anim_task = kthread_run(_anim_thread, me, "nxp-boot-animation");
	}
}

void stop_nxp_boot(void)
{
	struct nxp_boot_context *me = &_context;

	printk("stop nxp boot\n");
	if (me->anim_task) {
		printk(KERN_ERR "stop nxp boot animation\n");
		kthread_stop(me->anim_task);
		me->anim_task = NULL;
	}
	if (me->load_task) {
		printk(KERN_ERR "stop nxp boot loading\n");
		kthread_stop(me->load_task);
		me->load_task = NULL;
	}
}

#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
void register_backward_camera(struct platform_device *device)
{
	struct nxp_boot_context *me = &_context;
	me->camera = device;
}
#endif

bool is_nxp_boot_animation_run(void)
{
	struct nxp_boot_context *me = &_context;
	if (me->anim_task)
		return true;
	return false;
}

EXPORT_SYMBOL(start_nxp_load);
EXPORT_SYMBOL(start_nxp_animation);
EXPORT_SYMBOL(stop_nxp_boot);
#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
EXPORT_SYMBOL(register_backward_camera);
#endif
EXPORT_SYMBOL(is_nxp_boot_animation_run);

static ssize_t _stop_boot_animation(struct device *pdev,
        struct device_attribute *attr, const char *buf, size_t n)
{
	stop_nxp_boot();
	return n;
}

static struct device_attribute nxpboot_attr = __ATTR(stop, 0664, NULL, _stop_boot_animation);
static struct attribute *attrs[] = {
	&nxpboot_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = (struct attribute **)attrs,
};

static int _create_sysfs(void)
{
	struct kobject *kobj = NULL;
	int ret = 0;

	kobj = kobject_create_and_add("nxpboot", &platform_bus.kobj);
	if (! kobj) {
		printk(KERN_ERR "Fail, create kobject for nxpboot\n");
		return -ret;
	}

	ret = sysfs_create_group(kobj, &attr_group);
	if (ret) {
		printk(KERN_ERR "Fail, create sysfs group for nxpboot\n");
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
