#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/compat.h>
#include <linux/delay.h>

#include <asm/uaccess.h>

#include <linux/version.h>

/* machine */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
#include <nexell/nxp-deinterlacer.h>
#include <nexell/platform.h>
#else
#include <mach/nxp-deinterlacer.h>
#include <mach/platform.h>
#endif

/* prototype */
#include <nx_rstcon.h>
#include <nx_clkgen.h>
#include <nx_deinterlace.h>

#include "nxp-deinterlacer.h"

#define	DEVICE_NAME	"deinterlacer"

#define TIMEOUT_START     (HZ/5) /* 200ms */
#define TIMEOUT_STOP      (HZ/10) /* 100ms */

static nxp_deinterlace *_deinterlace	=	NULL;

static void _set_irq_number(void)
{
    _deinterlace->irq = NX_DEINTERLACE_GetInterruptNumber();
#if defined(CONFIG_ARCH_S5P6818)
    _deinterlace->irq += 32;
#endif
}

static void _hw_initialize(void)
{
    NX_DEINTERLACE_Initialize();
    NX_DEINTERLACE_SetBaseAddress( (void *)IO_ADDRESS(NX_DEINTERLACE_GetPhysicalAddress()) );

    NX_CLKGEN_SetBaseAddress( NX_DEINTERLACE_GetClockNumber(), (void *)IO_ADDRESS(NX_CLKGEN_GetPhysicalAddress(NX_DEINTERLACE_GetClockNumber())));
    NX_CLKGEN_SetClockBClkMode( NX_DEINTERLACE_GetClockNumber(), NX_BCLKMODE_ALWAYS );

#if defined(CONFIG_ARCH_S5P6818)
    NX_RSTCON_SetRST(NX_DEINTERLACE_GetResetNumber(), RSTCON_NEGATE);
#elif defined(CONFIG_ARCH_S5P4418)
    NX_RSTCON_SetnRST(NX_DEINTERLACE_GetResetNumber(), RSTCON_nENABLE);
#endif

#if defined(CONFIG_ARCH_S5P4418)
    NX_DEINTERLACE_SetClockPClkMode( NX_PCLKMODE_ALWAYS );
#endif

    NX_DEINTERLACE_OpenModule();
#if defined(CONFIG_ARCH_S5P6818)
    NX_DEINTERLACE_SetInterruptEnable(0, CTRUE);
#elif defined(CONFIG_ARCH_S5P4418)
    NX_DEINTERLACE_SetInterruptEnable(3, CTRUE);
#endif
    NX_DEINTERLACE_ClearInterruptPendingAll();
}

static void _hw_deinitialize(void)
{
    NX_DEINTERLACE_SetInterruptEnableAll(CFALSE);
    NX_DEINTERLACE_ClearInterruptPendingAll();
}

static void _set_deinterlace( U16 Height, U16 Width,
        U32 Y_PrevAddr,  U32 Y_CurrAddr,   U32 Y_NextAddr, U32 Y_SrcStride, U32 Y_DstAddr, U32 Y_DstStride,
        U32 CB_CurrAddr, U32 CB_SrcStride, U32 CB_DstAddr, U32 CB_DstStride,
        U32 CR_CurrAddr, U32 CR_SrcStride, U32 CR_DstAddr, U32 CR_DstStride,
        int IsODD)
{
    U32 YDstFieldStride     = (Y_DstStride*2);
    U32 CbDstFieldStride    = (CB_DstStride*2);
    U32 CrDstFieldStride    = (CR_DstStride*2);
    U16 CWidth              = (U16)(Width/2);
    U32 CHeight             = (U16)(Height/2);

    // Y Register Setting
    NX_DEINTERLACE_SetYSrcImageSize ( Height, Width );
    NX_DEINTERLACE_SetYSrcAddrPrev  ( Y_PrevAddr );
    NX_DEINTERLACE_SetYSrcAddrCurr  ( Y_CurrAddr );
    NX_DEINTERLACE_SetYSrcAddrNext  ( Y_NextAddr );
#if defined(CONFIG_ARCH_S5P6818)
    NX_DEINTERLACE_SetYSrcStride    ( Y_SrcStride );
#elif defined(CONFIG_ARCH_S5P4418)
    NX_DEINTERLACE_SetYSrcStride    ( Y_SrcStride, 1 );
    NX_DEINTERLACE_SetYSrcControl   ( NX_DEINTERLACE_DIR_ENABLE, NX_DEINTERLACE_BIT8, 0x0, 0x0 );
#endif
    NX_DEINTERLACE_SetYDestStride   ( YDstFieldStride );

    // CB Regiseter Setting
    NX_DEINTERLACE_SetCBSrcImageSize( CHeight, CWidth );
    NX_DEINTERLACE_SetCBSrcAddrCurr ( CB_CurrAddr );
#if defined(CONFIG_ARCH_S5P6818)
    NX_DEINTERLACE_SetCBSrcStride   ( CB_SrcStride );
#elif defined(CONFIG_ARCH_S5P4418)
    NX_DEINTERLACE_SetCBSrcStride   ( CB_SrcStride, 1 );
    NX_DEINTERLACE_SetCBSrcControl  ( NX_DEINTERLACE_DIR_ENABLE, NX_DEINTERLACE_BIT8, 0x0, 0x0 );
#endif
    NX_DEINTERLACE_SetCBDestStride  ( CbDstFieldStride );

    // CR Regiseter Setting
    NX_DEINTERLACE_SetCRSrcImageSize( CHeight, CWidth );
    NX_DEINTERLACE_SetCRSrcAddrCurr ( CR_CurrAddr );
#if defined(CONFIG_ARCH_S5P6818)
    NX_DEINTERLACE_SetCRSrcStride   ( CR_SrcStride );
#elif defined(CONFIG_ARCH_S5P4418)
    NX_DEINTERLACE_SetCRSrcStride   ( CR_SrcStride, 1 );
    NX_DEINTERLACE_SetCRSrcControl  ( NX_DEINTERLACE_DIR_ENABLE, NX_DEINTERLACE_BIT8, 0x0, 0x0 );
#endif
    NX_DEINTERLACE_SetCRDestStride  ( CrDstFieldStride );

    // Parameter Setting
    NX_DEINTERLACE_SetASParameter   (  10,  18 );
    NX_DEINTERLACE_SetMDSADParameter(   8,  16 );
    NX_DEINTERLACE_SetMIParameter   (  50, 306 );
    NX_DEINTERLACE_SetYSParameter   ( 434, 466 );
    NX_DEINTERLACE_SetBLENDParameter(        3 );


    if (IsODD) {
        // Y Register Set
        NX_DEINTERLACE_SetYDestAddrDIT  ( (Y_DstAddr+Y_DstStride ) );
        NX_DEINTERLACE_SetYDestAddrFil  (  Y_DstAddr               );
        // CB Register Set
        NX_DEINTERLACE_SetCBDestAddrDIT ( (CB_DstAddr+CB_DstStride) );
        NX_DEINTERLACE_SetCBDestAddrFil (  CB_DstAddr               );
        // CR Register Set
        NX_DEINTERLACE_SetCRDestAddrDIT ( (CR_DstAddr+CR_DstStride) );
        NX_DEINTERLACE_SetCRDestAddrFil (  CR_DstAddr               );
        // Start
        NX_DEINTERLACE_SetYCBCREnable   ( CTRUE, CTRUE, CTRUE );
        NX_DEINTERLACE_SetYCBCRField    ( NX_DEINTERLACE_FIELD_EVEN, NX_DEINTERLACE_FIELD_EVEN, NX_DEINTERLACE_FIELD_EVEN );
    } else {
        // Y Register Set
        NX_DEINTERLACE_SetYDestAddrDIT  (  Y_DstAddr              );
        NX_DEINTERLACE_SetYDestAddrFil  ( (Y_DstAddr+Y_DstStride) );
        // CB Register Set
        NX_DEINTERLACE_SetCBDestAddrDIT (  CB_DstAddr               );
        NX_DEINTERLACE_SetCBDestAddrFil ( (CB_DstAddr+CB_DstStride) );
        // CR Register Set
        NX_DEINTERLACE_SetCRDestAddrDIT (  CR_DstAddr               );
        NX_DEINTERLACE_SetCRDestAddrFil ( (CR_DstAddr+CR_DstStride) );
        // Start
        NX_DEINTERLACE_SetYCBCREnable   ( CTRUE, CTRUE, CTRUE );
        NX_DEINTERLACE_SetYCBCRField    ( NX_DEINTERLACE_FIELD_ODD, NX_DEINTERLACE_FIELD_ODD, NX_DEINTERLACE_FIELD_ODD );
    }
}

static void _set_and_run(frame_data_info *frame)
{
    unsigned long src_prev_y_data_phy, src_curr_y_data_phy, src_next_y_data_phy, src_curr_cb_data_phy, src_curr_cr_data_phy;
    unsigned long dst_y_data_phy, dst_cb_data_phy, dst_cr_data_phy;
    int width, height, src_y_stride, src_c_stride, dst_y_stride, dst_c_stride;

    int dst_y_data_size		=	0;
    int dst_cb_data_size	=	0;
    int dst_cr_data_size	=	0;

    width			= frame->width;
    height			=	frame->height;

    src_y_stride	=	frame->src_bufs[0].plane3.src_stride[0];
    src_c_stride	=	frame->src_bufs[0].plane3.src_stride[1];

    dst_y_stride	=	frame->dst_bufs[0].plane3.dst_stride[0];
    dst_c_stride	=	frame->dst_bufs[0].plane3.dst_stride[1];

    src_prev_y_data_phy		=	frame->src_bufs[0].plane3.phys[0];
    src_curr_y_data_phy		=	frame->src_bufs[1].plane3.phys[0];
    src_next_y_data_phy		=	frame->src_bufs[2].plane3.phys[0];

    src_curr_cb_data_phy	=	frame->src_bufs[1].plane3.phys[1];
    src_curr_cr_data_phy	=	frame->src_bufs[1].plane3.phys[2];

    dst_y_data_phy	=	frame->dst_bufs[0].plane3.phys[0];
    dst_cb_data_phy	=	frame->dst_bufs[0].plane3.phys[1];
    dst_cr_data_phy	=	frame->dst_bufs[0].plane3.phys[2];

    dst_y_data_size = frame->dst_bufs[0].plane3.sizes[0];
    dst_cb_data_size = frame->dst_bufs[0].plane3.sizes[1];
    dst_cr_data_size = frame->dst_bufs[0].plane3.sizes[2];

    _set_deinterlace(height, width,
            src_prev_y_data_phy, src_curr_y_data_phy, src_next_y_data_phy, src_y_stride, dst_y_data_phy, dst_y_stride,
            src_curr_cb_data_phy, src_c_stride, dst_cb_data_phy, dst_c_stride,
            src_curr_cr_data_phy, src_c_stride, dst_cr_data_phy, dst_c_stride,
            frame->src_bufs[1].frame_num % 2);

    NX_DEINTERLACE_DeinterlaceStart();


#if 0
    printk(KERN_ERR "\nRESOLUTION - WIDTH : %d, HEIGHT : %d\n", width, height);

#if 0
	printk(KERN_ERR "SIZES - PREV SRC Y  : %ld\n", frame->src_bufs[0].plane3.sizes[0]);
	printk(KERN_ERR "SIZES - PREV SRC CB : %ld\n", frame->src_bufs[0].plane3.sizes[1]);
	printk(KERN_ERR "SIZES - PREV SRC CR : %ld\n", frame->src_bufs[0].plane3.sizes[2]);

	printk(KERN_ERR "SIZES - CURR SRC Y  : %ld\n", frame->src_bufs[1].plane3.sizes[0]);
	printk(KERN_ERR "SIZES - CURR SRC CB : %ld\n", frame->src_bufs[1].plane3.sizes[1]);
	printk(KERN_ERR "SIZES - CURR SRC CR : %ld\n", frame->src_bufs[1].plane3.sizes[2]);

	printk(KERN_ERR "SIZES - NEXT SRC Y  : %ld\n", frame->src_bufs[2].plane3.sizes[0]);
	printk(KERN_ERR "SIZES - NEXT SRC CB : %ld\n", frame->src_bufs[2].plane3.sizes[1]);
	printk(KERN_ERR "SIZES - NEXT SRC CR : %ld\n", frame->src_bufs[2].plane3.sizes[2]);

	printk(KERN_ERR "SIZES - NEXT DST Y  : %d\n", dst_y_data_size);
	printk(KERN_ERR "SIZES - NEXT DST CB : %d\n", dst_cb_data_size);
	printk(KERN_ERR "SIZES - NEXT DST CR : %d\n", dst_cr_data_size);
#endif

	printk(KERN_ERR "[%s]PHY - SRC PREV Y  : 0x%lX\n", __func__, src_prev_y_data_phy);
	printk(KERN_ERR "[%s]PHY - SRC CURR Y  : 0x%lX\n", __func__, src_curr_y_data_phy);
	printk(KERN_ERR "[%s]PHY - SRC NEXT Y  : 0x%lX\n", __func__, src_next_y_data_phy);
	printk(KERN_ERR "[%s]PHY - SRC CURR CB : 0x%lX\n", __func__, src_curr_cb_data_phy);
	printk(KERN_ERR "[%s]PHY - SRC CURR CR : 0x%lX\n", __func__, src_curr_cr_data_phy);
	printk(KERN_ERR "STRIDE - SRC Y : %d, SRC C : %d, DST Y : %d, DST C : %d\n\n",
                            src_y_stride, src_c_stride, dst_y_stride, dst_c_stride);
#endif

}

static inline int _get_status(nxp_deinterlace *me)
{
    return atomic_read(&me->status);
}

static inline void _set_status(nxp_deinterlace *me, int status)
{
    atomic_set(&me->status, status);
}

static irqreturn_t _irq_handler_deinterlace(int irq, void *param)
{
    nxp_deinterlace *me = (nxp_deinterlace *)param;

    NX_DEINTERLACE_ClearInterruptPendingAll();

    _set_status(me, PROCESSING_STOP);

    wake_up_interruptible(&me->wq_end);

    return IRQ_HANDLED;
}

static inline int _wait_stop(nxp_deinterlace *me)
{
    while (_get_status(me) != PROCESSING_STOP) {
        if (!wait_event_interruptible_timeout(me->wq_end, _get_status(me) == PROCESSING_STOP, TIMEOUT_STOP)) {
            printk("Wait timeout for stop\n");
            return -EBUSY;
        }
    }
    return 0;
}

static int _handle_set_and_run(unsigned long arg)
{
    int ret = 0;
    frame_data_info frame_info;
    nxp_deinterlace *me = _deinterlace;

    if (copy_from_user(&frame_info, (frame_data_info *)arg, sizeof(frame_data_info))) {
        printk(KERN_ERR "%s: failed to copy_from_user\n", __func__);
        _set_status(me, PROCESSING_STOP);
        return -EFAULT;
    }

    _set_status(me, PROCESSING_START);

    _set_and_run(&frame_info);

#if 1
    ret = _wait_stop(me);
    if ( ret < 0 ) {
        printk(KERN_ERR "failed to _wait_stop\n");
        _set_status(me, PROCESSING_STOP);
    }
#else
    mdelay(100);
#endif

    return 0;
}

static long deinterlace_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = -EINVAL;

    switch(cmd) {
        case IOCTL_DEINTERLACE_SET_AND_RUN:
            mutex_lock(&_deinterlace->mutex);
            ret = _handle_set_and_run(arg);
            mutex_unlock(&_deinterlace->mutex);
            break;

        default:
            break;
    }

    return ret;
}

#ifdef CONFIG_COMPAT
struct compat_frame_data {
    compat_int_t frame_num;
    compat_int_t plane_num;
    compat_int_t frame_type;
    compat_int_t frame_factor;

    union {
        struct {
            compat_uptr_t virt[MAX_BUFFER_PLANES];
            compat_ulong_t sizes[MAX_BUFFER_PLANES];
            compat_ulong_t src_stride[MAX_BUFFER_PLANES];
            compat_ulong_t dst_stride[MAX_BUFFER_PLANES];

            compat_int_t fds[MAX_BUFFER_PLANES];
            compat_ulong_t phys[MAX_BUFFER_PLANES];
        } plane3;
    };
};

struct compat_frame_data_info {
    compat_int_t command;
    compat_int_t width;
    compat_int_t height;
    compat_int_t plane_mode;

    struct compat_frame_data dst_bufs[DST_BUFFER_COUNT];
    struct compat_frame_data src_bufs[SRC_BUFFER_COUNT];
};

static int compat_get_frame_data(struct compat_frame_data *data32, frame_data *data)
{
    compat_int_t i32;
    /* compat_uptr_t uptr; */
    compat_ulong_t ul;
    int err;
    int i;

    err  = get_user(i32, &data32->frame_num);
    err |= put_user(i32, &data->frame_num);
    err |= get_user(i32, &data32->plane_num);
    err |= put_user(i32, &data->plane_num);
    err |= get_user(i32, &data32->frame_type);
    err |= put_user(i32, &data->frame_type);
    err |= get_user(i32, &data32->frame_factor);
    err |= put_user(i32, &data->frame_factor);

#if 0
    // TODO : not use virt
    for (i = 0; i < MAX_BUFFER_PLANES; i++) {
        err |= get_user(uptr, &data32->plane3.virt[i]);
        err |= put_user(uptr, &data->plane3.virt[i]);
    }
#endif

    for (i = 0; i < MAX_BUFFER_PLANES; i++) {
        err |= get_user(ul, &data32->plane3.sizes[i]);
        err |= put_user(ul, &data->plane3.sizes[i]);
    }

    for (i = 0; i < MAX_BUFFER_PLANES; i++) {
        err |= get_user(ul, &data32->plane3.src_stride[i]);
        err |= put_user(ul, &data->plane3.src_stride[i]);
    }

    for (i = 0; i < MAX_BUFFER_PLANES; i++) {
        err |= get_user(ul, &data32->plane3.dst_stride[i]);
        err |= put_user(ul, &data->plane3.dst_stride[i]);
    }

#if 0
    // TODO : not use fds
    for (i = 0; i < MAX_BUFFER_PLANES; i++) {
        err |= get_user(i32, &data32->plane3.fds[i]);
        err |= put_user(i32, &data->plane3.fds[i]);
    }
#endif

    for (i = 0; i < MAX_BUFFER_PLANES; i++) {
        err |= get_user(ul, &data32->plane3.phys[i]);
        err |= put_user(ul, &data->plane3.phys[i]);
    }

    return err;
}

static int compat_get_frame_data_info(struct compat_frame_data_info *data32, frame_data_info *data)
{
    compat_int_t i32;
    int i;
    int err;

    err  = get_user(i32, &data32->command);
    err |= put_user(i32, &data->command);
    err |= get_user(i32, &data32->width);
    err |= put_user(i32, &data->width);
    err |= get_user(i32, &data32->height);
    err |= put_user(i32, &data->height);
    err |= get_user(i32, &data32->plane_mode);
    err |= put_user(i32, &data->plane_mode);

    for (i = 0; i < DST_BUFFER_COUNT; i++) {
        err |= compat_get_frame_data(&data32->dst_bufs[i], &data->dst_bufs[i]);
    }

    for (i = 0; i < SRC_BUFFER_COUNT; i++) {
        err |= compat_get_frame_data(&data32->src_bufs[i], &data->src_bufs[i]);
    }

    return err;
}

static long deinterlace_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case IOCTL_DEINTERLACE_SET_AND_RUN:
    {
        struct compat_frame_data_info __user *data32;
        frame_data_info __user *data;
        int err;

        data32 = compat_ptr(arg);
        data = compat_alloc_user_space(sizeof(*data));
        if (!data) {
            printk(KERN_ERR "%s: failed to compat_alloc_user_space for frame_data\n", __func__);
            return -EFAULT;
        }

        err = compat_get_frame_data_info(data32, data);
        if (err) {
            printk(KERN_ERR "%s: failed to compat_get_frame_data\n", __func__);
            return err;
        }

        return filp->f_op->unlocked_ioctl(filp, IOCTL_DEINTERLACE_SET_AND_RUN, (unsigned long)data);
    }

    default:
        return -EINVAL;
    }
}
#endif

static int deinterlace_open(struct inode *inode, struct file *file)
{
    int ret;
    nxp_deinterlace *me = _deinterlace;
    file->private_data = me;

    if (atomic_read(&me->open_count) > 0) {
        atomic_inc(&me->open_count);
        printk("%s: open count %d\n", __func__, atomic_read(&me->open_count));
        return 0;
    }

    atomic_inc(&me->open_count);

    _hw_initialize();
    _set_irq_number();

    ret = request_irq(me->irq, _irq_handler_deinterlace, IRQF_SHARED, DEVICE_NAME, me);
    if (ret < 0) {
        pr_err("%s: failed to request_irq()\n", __func__);
        return ret;
    }

    atomic_set(&me->status, PROCESSING_STOP);

    return 0;
}

static int deinterlace_close(struct inode *inode, struct file *file)
{
    nxp_deinterlace *me = (nxp_deinterlace *)file->private_data;

    atomic_dec(&me->open_count);

    if (atomic_read(&me->open_count) == 0) {
        printk("%s ==> open count 0\n", __func__);
        free_irq(me->irq, me);
        _hw_deinitialize();
        atomic_set(&me->status, PROCESSING_STOP);
    }

    return 0;
}

static struct file_operations deinterlace_fops =
{
    .open			=	deinterlace_open,
    .release    	=	deinterlace_close,
    .unlocked_ioctl	=	deinterlace_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   =   deinterlace_compat_ioctl,
#endif
};

static struct miscdevice nxp_deinterlace_misc_device =
{
    .minor			=	MISC_DYNAMIC_MINOR,
    .name			=	DEVICE_NAME,
    .fops			=	&deinterlace_fops,
};

static int deinterlace_probe(struct platform_device *dev)
{
    nxp_deinterlace *me = NULL;

    me = (nxp_deinterlace *)kzalloc(sizeof(nxp_deinterlace), GFP_KERNEL);

    if (!me) {
        pr_err("%s: failed to alloc nxp_deinterlace\n", __func__);
        return -1;
    }

    _deinterlace = me;

    init_waitqueue_head(&(_deinterlace->wq_start));
    init_waitqueue_head(&(_deinterlace->wq_end));

    mutex_init(&me->mutex);

    return 0;
}

static int deinterlace_remove(struct platform_device *dev)
{
    if (_deinterlace) {
        kfree(_deinterlace);
        _deinterlace = NULL;
    }

    return 0;
}

static struct platform_driver deinterlace_driver = {
    .probe = deinterlace_probe,
    .remove = deinterlace_remove,
    .driver	=	{
        .name	= DEVICE_NAME,
        .owner	=	THIS_MODULE,
    },
};

static struct platform_device deinterlace_device = {
    .name			=	DEVICE_NAME,
    .id				=	0,
    .num_resources	=	0,
};

static int __init deinterlace_init(void)
{
    int ret;

    ret = misc_register(&nxp_deinterlace_misc_device);
    if (ret) {
        printk(KERN_ERR "%s: failed to misc_register()\n", __func__);
        return -ENODEV;
    }

    ret = platform_driver_register(&deinterlace_driver);
    if (!ret) {
        ret = platform_device_register(&deinterlace_device);
        if (ret) {
            printk(KERN_ERR "platform driver register error : %d\n", ret);
            platform_driver_unregister(&deinterlace_driver);
        }
    } else {
        printk(KERN_INFO DEVICE_NAME ": Error registering platform driver!\n");
    }

    return ret;
}

static void __exit deinterlace_exit(void)
{
    platform_device_unregister(&deinterlace_device);
    platform_driver_unregister(&deinterlace_driver);
    misc_deregister(&nxp_deinterlace_misc_device);
}

module_init(deinterlace_init);
module_exit(deinterlace_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jong Keun Choi <jkchoi@nexell.co.kr>");
MODULE_DESCRIPTION("deinterlace Driver");
MODULE_VERSION("v0.1");
