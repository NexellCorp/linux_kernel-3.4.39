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
#include <linux/gpio.h>

//#define FEATURE_SPLASH_IMAGE


#define MAX_BMP_FILES   2

#define DISP_MODULE 0
#define SECOND_STAGE_START_FRAME    8
#define SECOND_STAGE_FRAME_COUNT    12
#define OS_VERSION_DISPLAY_X        840
#define OS_VERSION_DISPLAY_Y        580
#define OS_VERSION_DISPLAY_TEXT_COLOR   0xFFFFFFFF
#define OS_VERSION_DISPLAY_BACK_COLOR   0xFF000000
#define OS_VERSION_DISPLAY_ALPHA    1

#define USE_BZIP2
/*#define USE_LZ4*/


#if defined(USE_BZIP2)
#include <linux/decompress/bunzip2.h>
#elif defined(USE_LZ4)
#include <linux/decompress/unlz4.h>
#else
#error "You must define USE_BZIP2 or USE_LZ4"
#endif

#ifdef FEATURE_SPLASH_IMAGE
#define SPLASH_TAG			"splash"
#define SPLASH_TAG_SIZE		6
#define	IMAGE_SIZE_MAX		50
#define HEADER_SIZE         2048

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
#endif

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

#ifdef FEATURE_SPLASH_IMAGE
    /* image info */
    char header[HEADER_SIZE];
    SPLASH_IMAGE_INFO *splash_info;
#endif

    int img_width;
    int img_height;
    int img_count;

    /* thread */
    struct task_struct *anim_task;
    struct task_struct *load_task;

    bool is_valid;

    /* backward camera */
//#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
    struct platform_device *camera;
//#endif

    /* os version */
    char *os_version;

    /* user booting image */
    bool use_user_boot_image;
    bool load_user_boot_image;
} _context;


extern struct ion_device *get_global_ion_device(void);

//#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
#if 0
extern bool is_preview_display_on(void);
#endif

#ifdef FEATURE_SPLASH_IMAGE
static int _alloc_ion_buffer(struct nxp_boot_context *me, bool include_header)
{
	int size = 0;
	struct ion_buffer *ion_buffer;

	if (!me->ion_client) {
		me->ion_client = ion_client_create(get_global_ion_device(), "nxp-boot");
		if (IS_ERR(me->ion_client)) {
			pr_err("%s Error: ion_client_create()\n", __func__);
			return -EINVAL;
		}
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

static void _free_ion_buffer(struct nxp_boot_context *me)
{
	if (me->dma_buf != NULL) {
		dma_buf_put(me->dma_buf);
		me->dma_buf = NULL;
		ion_free(me->ion_client, me->ion_handle);
		me->ion_handle = NULL;
	}
}

static int _check_header(struct nxp_boot_context *me)
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
		filp = NULL;
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
	struct nxp_boot_context *me = (struct nxp_boot_context *)arg;
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

	if (IS_ERR(filp)) 
	{
		int i = 0;
		unsigned char *cpu_address;
		SPLASH_IMAGE_INFO *splash;

		printk(KERN_ERR"%s: open failed %s ===> use splash.img\n", __func__, filename);
		filename = "splash.img";

		me->use_user_boot_image = true;

		filp = filp_open(filename, O_RDONLY | O_LARGEFILE, 0);
		if (IS_ERR(filp)) {
			printk(KERN_ERR "%s: open failed %s\n", __func__, filename);
			ret = -1;
			filp = NULL;
			goto OUT;
		}

		ret = _alloc_ion_buffer(me, true);
		if (ret < 0) {
			printk(KERN_ERR "%s: failed to out buffer\n", __func__);
			goto OUT;
		}

		while(i < me->img_count) {
			/*while(1) {*/
			splash = &me->splash_info[i];
			cpu_address = (unsigned char *)(me->virt + splash->ulImageAddr);
			vfs_llseek(filp, splash->ulImageAddr, 0);
			ret = vfs_read(filp, cpu_address, me->img_width * me->img_height * 4, &filp->f_pos);
			/*lcd_set_framebuffer((unsigned int)cpu_address);*/
			/*lcd_draw_text(me->os_version, OS_VERSION_DISPLAY_X, OS_VERSION_DISPLAY_Y, 2, 2, 1);*/
			i++;
		}
		me->load_user_boot_image = true;;
	}
	else 
	{
		int file_size = 0;
		me->use_user_boot_image = false;
		vfs_llseek(filp, 0, SEEK_SET);
		file_size = vfs_llseek(filp, 0, SEEK_END);
		printk("filesize: %d\n", file_size);
		buf = kmalloc(file_size, GFP_KERNEL);
		if (!buf) {
			printk(KERN_ERR"%s: failed to alloc file buffer\n", __func__);
			goto OUT;
		}

		ret = _alloc_ion_buffer(me, true);
		if (ret < 0) {
			printk(KERN_ERR"%s: failed to out buffer\n", __func__);
			goto OUT;
		}

		vfs_llseek(filp, 0, SEEK_SET);
		ret = vfs_read(filp, buf, file_size, &filp->f_pos);
		if (ret != file_size) {
			printk(KERN_ERR"%s error: read size %d/%d\n", __func__, ret, file_size);
			goto OUT;
		}

#ifdef USE_BZIP2
		ret = bunzip2(buf, file_size, NULL, NULL, me->virt, &decompress_size, NULL);
		if (ret < 0) {
			printk(KERN_ERR"%s: failed to bunzip2(ret: %d)\n", __func__, ret);
			goto OUT;
		}
#else
		ret = unlz4(buf, file_size, NULL, NULL, me->virt, &decompress_size, NULL);
		if (ret < 0) {
			printk(KERN_ERR"%s: failed to unlz4(ret: %d)\n", __func__, ret);
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

#include "draw_bmp.c"
static void _display_on(struct nxp_boot_context *me)
{
	nxp_soc_disp_device_enable_all(DISP_MODULE, 1);
	//nxp_soc_gpio_set_out_value(PAD_GPIO_A + 25, 1);
	/*nxp_soc_gpio_set_out_value(PAD_GPIO_D + 1, 1);*/
}

static int _display_bmp(struct nxp_boot_context *me, char *filename)
{
	char *buf = NULL;
	int ret;
	bool first = me->bmp_index == 0;

	if (0 == _load_bmp(filename, &buf)) 
	{
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

		// draw os version
		/*
		{
			unsigned int cpu_address;
			lcd_info lcd = {
				.bit_per_pixel = 32,
				.lcd_width = CFG_DISP_PRI_RESOL_WIDTH,
				.lcd_height = CFG_DISP_PRI_RESOL_HEIGHT,
				.back_color = OS_VERSION_DISPLAY_BACK_COLOR,
				.text_color = OS_VERSION_DISPLAY_TEXT_COLOR,
				.alphablend = OS_VERSION_DISPLAY_ALPHA,
			};
			lcd_debug_init(&lcd);

			cpu_address = (unsigned int)(me->bmp_alloc_context[me->bmp_index].virt);
			lcd_set_framebuffer(cpu_address);
			lcd_draw_text(me->os_version, OS_VERSION_DISPLAY_X, OS_VERSION_DISPLAY_Y, 1, 1, 1);
		}
		*/

		nxp_soc_disp_rgb_set_format(DISP_MODULE, layer, NX_MLC_RGBFMT_X8R8G8B8, width, height, 4);
		nxp_soc_disp_rgb_set_address(DISP_MODULE, layer, me->bmp_alloc_context[me->bmp_index].dma_addr, 4, width * 4, 1);

		if (first) 
		{
			_display_on(me);
			first = false;
		}
	}
	else
	{
		printk(KERN_ERR "%s: failed %s\n", __func__, filename);
		return -1;
	}

OUT:
	if (buf)
		kfree(buf);

	me->bmp_index++;
	return 0;
}

static int _anim_thread(void *arg)
{
	struct nxp_boot_context *me = (struct nxp_boot_context *)arg;
#ifdef FEATURE_SPLASH_IMAGE
	int count = 0;
	dma_addr_t address;
	SPLASH_IMAGE_INFO *splash;

	schedule_timeout_interruptible(HZ);

	nxp_soc_disp_rgb_set_format(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, NX_MLC_RGBFMT_X8R8G8B8, me->img_width, me->img_height, 4);
	while(1) {
		splash = &me->splash_info[count++%me->img_count];
		address = me->dma_addr + splash->ulImageAddr;
		nxp_soc_disp_rgb_set_address(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, address, 4, me->img_width * 4, 1);
		schedule_timeout_interruptible(HZ/100);
		if (kthread_should_stop()) {
			break;
		}
	}
	nxp_soc_disp_rgb_set_format(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, NX_MLC_RGBFMT_A8R8G8B8, me->img_width, me->img_height, 4);
	nxp_soc_disp_rgb_set_color(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, RGB_COLOR_ALPHA, 0x0, false);

	_free_ion_buffer(me);
#else
	static bool bmp_load = false;
	int ret = 0;
	
	me->bmp_index = 0;

	while (1) 
	{
		if(
//#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
#if 0
			!is_preview_display_on() && 
#endif
			!bmp_load)
		{
			ret = _display_bmp(me, "logo.bmp");
			if(ret < 0) break;
			bmp_load = true;
		}

		if (kthread_should_stop()) 
		{
			nxp_soc_disp_rgb_set_format(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, NX_MLC_RGBFMT_A8R8G8B8, me->img_width, me->img_height, 4);
			nxp_soc_disp_rgb_set_color(DISP_MODULE, CFG_DISP_PRI_SCREEN_LAYER, RGB_COLOR_ALPHA, 0x0, false);
			_free_bmp_buffer(me);
			break;
		}

		schedule_timeout_interruptible(HZ/100);
	}
#endif

	return 0;
}

static int _start_load(void *arg)
{
	struct nxp_boot_context *me = &_context;
	char *os_version = "2015.11.13 v1.0";

	me->os_version = os_version;
	printk("%s: os_version %s\n", __func__, os_version);

#ifdef FEATURE_SPLASH_IMAGE
	if (_check_header(me) < 0) {
		printk(KERN_ERR "invalid image?\n");
		return -1;
	}
	me->is_valid = true;
	me->load_task = kthread_run(_load_thread, me, "nxp-bootload");
#endif
	return 0;
}

void start_nxp_load(void)
{
	struct nxp_boot_context *me = &_context;

	_start_load(&_context);
	if (me->camera)
		platform_device_register(me->camera);
}


void start_nxp_animation(void)
{
	struct nxp_boot_context *me = &_context;
#ifdef FEATURE_SPLASH_IMAGE
	if (me->is_valid)
#endif
		me->anim_task = kthread_run(_anim_thread, me, "nxp-boot-animation");
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

void register_backward_camera(struct platform_device *device)
{
	struct nxp_boot_context *me = &_context;
	me->camera = device;
}

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
EXPORT_SYMBOL(register_backward_camera);
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
