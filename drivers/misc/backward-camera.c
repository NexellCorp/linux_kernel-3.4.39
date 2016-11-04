#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/switch.h>

#include <linux/dma-buf.h>
#include <linux/nxp_ion.h>
#include <linux/ion.h>
#include <linux/cma.h>

#include <linux/kfifo.h>

#include <mach/platform.h>
#include <mach/soc.h>
#include <mach/nxp-backward-camera.h>

#include <nx_deinterlace.h>

#if defined(CONFIG_ARCH_S5P6818)
#include <mach/nxp-deinterlacer.h>
#endif

#include <../drivers/gpu/ion/ion_priv.h>

#ifndef MLC_LAYER_RGB_OVERLAY
#define MLC_LAYER_RGB_OVERLAY 0
#endif

#define DEBUG_POINT printk(KERN_ERR "%s - Line : %d\n", __func__, __LINE__);

#define MAX_BUFFER_COUNT	4
#define CAPTURE_CLIPPER_INT 2UL

#ifndef ALIGN
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#endif

#define ENABLE_UEVENT  0

#if ENABLE_UEVENT
extern struct switch_dev *backgear_switch;
#endif

#define DEFAULT_BRIGHTNESS      0x0
#define DEFAULT_ANGLE           540

#define YUV_STRIDE_ALIGN_FACTOR		64
#define YUV_VSTRIDE_ALIGN_FACTOR	16

#define YUV_STRIDE(w)	ALIGN(w, YUV_STRIDE_ALIGN_FACTOR)
#define YUV_YSTRIDE(w)	(ALIGN(w/2, YUV_STRIDE_ALIGN_FACTOR) * 2)
#define YUV_VSTRIDE(h)	ALIGN(h, YUV_VSTRIDE_ALIGN_FACTOR)	

#define DEINTER_DISPLAY         1
#define DROP_INTERLACE_FRAME    1

#define FILE_SAVE               0

#if FILE_SAVE
static int save_idx = 0;

struct save_file_buf {
    unsigned char *pdata;
    unsigned int size;
};

#define FILE_SAVE_CNT 12
#define DATA_QUEUE_SIZE 10240 * FILE_SAVE_CNT

struct save_file_buf file_bufs[FILE_SAVE_CNT];
#endif


#ifndef MLC_LAYER_VIDEO
#define MLC_LAYER_VIDEO     3
#endif

//#define __DEBUG__

#if defined( __DEBUG__ )
    #define debug_msg(args...) \
        printk(KERN_INFO args);
    #define debug_stream(args...) \
        printk(KERN_ERR args);
#else
    #define debug_msg(args...) 
    #define debug_stream(args...) 
#endif

struct nxp_video_frame_buf {
	unsigned long frame_num;

	 /* video */
    struct ion_handle *ion_handle;
    struct dma_buf    *dma_buf;

	/* video data */
	void *virt_video;

	u32	lu_addr;
	u32 cb_addr;
	u32 cr_addr;
	u32 lu_stride;
	u32 cb_stride;
	u32 cr_stride;
	u32 lu_size;
	u32 cb_size;
	u32 cr_size;
};


//////////////////
// common_queue //
////////////////////////////////////////////////////////////////////////////////////////////////////
struct queue_entry {
    struct list_head head;
	void *data;
};

struct common_queue {
    struct list_head head;
    int buf_count;
	spinlock_t slock;
	unsigned long flags;

	void (*init)(struct common_queue *);
    void (*enqueue)(struct common_queue *, struct queue_entry *);
    struct queue_entry *(*dequeue)(struct common_queue *);
	struct queue_entry *(*peek)(struct common_queue *, int);
    int (*clear)(struct common_queue *);
    void (*lock)(struct common_queue *);
	void (*unlock)(struct common_queue *);
	int (*size)(struct common_queue *);
};

static void _init_queue(struct common_queue *q)
{
	INIT_LIST_HEAD(&q->head);
	spin_lock_init(&q->slock);
	q->buf_count = 0;
}

static void _enqueue(struct common_queue *q, struct queue_entry *buf)
{
	q->lock(q);
	list_add_tail(&buf->head, &q->head);
	q->buf_count++;
	q->unlock(q);
}

static struct queue_entry *_dequeue(struct common_queue *q)
{
    struct queue_entry *ent = NULL;
    struct nxp_video_frame_buf *buf = NULL;

	q->lock(q);
	if (!list_empty(&q->head)) {
        ent = list_first_entry(&q->head, struct queue_entry, head);
        buf = (struct nxp_video_frame_buf *)(ent->data);
		list_del_init(&ent->head);
		q->buf_count--;

        if (q->buf_count < 0) q->buf_count = 0; 
	}
	q->unlock(q);

	return ent;
}

static struct queue_entry *_peekqueue(struct common_queue *q, int pos)
{
	struct queue_entry *ent = NULL;
	int idx=0;

	q->lock(q);
	if((q->buf_count>0) && (q->buf_count>=(pos+1))) {
		list_for_each_entry(ent, &q->head, head) {
            if(idx == pos)
                break;
            else
                idx++;
	  	}
	}
	q->unlock(q);

	if(idx > pos) ent = NULL;

	return ent;
}

static int _clearqueue(struct common_queue *q)
{
    while( q->dequeue(q) );
    return 0;
}

static int _sizequeue(struct common_queue *q)
{
    int count=0;

	q->lock(q);
	count = q->buf_count;
	q->unlock(q);

	return count;
}

static void _lockqueue(struct common_queue *q)
{
	spin_lock_irqsave(&q->slock, q->flags);
}

static void _unlockqueue(struct common_queue *q)
{
	spin_unlock_irqrestore(&q->slock, q->flags);
}

static void register_queue_func(
							struct common_queue *q,  
							void (*init)(struct common_queue *),
    						void (*enqueue)(struct common_queue *, struct queue_entry *),
							struct queue_entry *(*dequeue)(struct common_queue *),
							struct queue_entry *(*peek)(struct common_queue *, int pos),
                            int (*clear)(struct common_queue *), 
							void (*lock)(struct common_queue *),
							void (*unlock)(struct common_queue *),
							int (*size)(struct common_queue *)
						)
{
	q->init = init;
	q->enqueue = enqueue;
	q->dequeue = dequeue;
	q->peek = peek;	
    q->clear = clear;
	q->lock = lock;
	q->unlock = unlock;
	q->size = size;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define MININUM_INQUEUE_COUNT 2
#define MININUM_INQUEUE_COUNT_NO_MIXED 3 

enum
{
	PROCESSING_STOP,
	PROCESSING_START
};

enum 
{
	FIELD_EVEN,
	FIELD_ODD
};

enum
{
    DISPLAY_START,
    DISPLAY_FINISH,
    DISPLAY_STOP
};

enum FRAME_KIND {
    Y,
    CB,
    CR
};

extern struct switch_dev *backgear_switch;

struct nxp_rgb_frame_buf {
    /* rgb */
    struct ion_handle *ion_handle_rgb;
    struct dma_buf    *dma_buf_rgb;

	/* rgb data  */		
	void *virt_rgb;
	u32 rgb_addr;
};

struct nxp_frame_set {
	u32 width;
	u32 height;

	int cur_mode;
	struct queue_entry *cur_entry_vip;
	struct queue_entry *cur_entry_deinter;
	struct queue_entry *cur_entry_display;

	struct nxp_video_frame_buf vip_bufs[MAX_BUFFER_COUNT];
	struct queue_entry vip_queue_entry[MAX_BUFFER_COUNT];

	struct nxp_video_frame_buf deinter_bufs[MAX_BUFFER_COUNT];
	struct queue_entry deinter_queue_entry[MAX_BUFFER_COUNT];

	struct nxp_rgb_frame_buf rgb_buf;
};

struct nxp_deinter_run_ctx {
	struct nxp_video_frame_buf src[3];
    // TODO : change dst to pointer
	struct nxp_video_frame_buf *dst;
};

int register_backward_irq_tw9900(void);
int register_backward_irq_tw9992(void);

extern int register_backward_irq_tw9900(void);
extern int register_backward_irq_tw9992(void);

static struct nxp_backward_camera_context {
    struct nxp_backward_camera_platform_data *plat_data;
    int irq;
    int vip_irq;
    bool running;
    bool backgear_on;

	struct work_struct work_deinter;
	struct workqueue_struct *wq_deinter;

    bool is_first;
    struct delayed_work work;
    struct i2c_client *client;

#ifdef CONFIG_PM
    struct delayed_work resume_work;
#endif

    /* ion allocation */
    struct ion_client *ion_client;
    /* video */
    struct ion_handle *ion_handle_video;
    struct dma_buf    *dma_buf_video;
    void              *virt_video;

    /* rgb */
    struct ion_handle *ion_handle_rgb;
    struct dma_buf    *dma_buf_rgb;
    void              *virt_rgb;

    /* for remove */
    struct platform_device *my_device;
	bool is_on;
	bool is_remove;
	bool removed;

	bool is_detected;

	// overlay
    int angle;
    struct ion_handle *ion_handle_db;
    struct dma_buf    *dma_buf_db;
    void *db_buffer;

	/* frame set */
	struct nxp_frame_set frame_set;

	/* vip */	
	struct common_queue q_vip_empty;
	struct common_queue q_vip_done;
	spinlock_t vip_lock;

	/* deinterlace */
	struct common_queue q_deinter_empty;
	struct common_queue q_deinter_done;
    struct mutex deinter_lock;

	wait_queue_head_t wq_start;
	wait_queue_head_t wq_deinter_end;

	atomic_t	status;

	int deinter_irq;

	uint32_t deinter_count;

	int (*callback)(void *);
	int (*return_to_empty)(struct common_queue *, struct common_queue *);

    bool is_display_on;
    bool is_mlc_on;

    bool mlc_on_first;

    spinlock_t deinit_lock;
    struct mutex decide_work_lock;
} _context;

static void _mlc_dump_register(int module)
{
#define DBGOUT(args...)  printk(args)
    struct NX_MLC_RegisterSet *pREG = 
    (struct NX_MLC_RegisterSet*)NX_MLC_GetBaseAddress(module);

    DBGOUT("BASE ADDRESS: %p\n", pREG);
#if defined(CONFIG_ARCH_S5P4418)
    DBGOUT(" MLC_MLCCONTROLT    = 0x%04x\r\n", pREG->MLCCONTROLT);
#endif

}

static void _vip_dump_register(int module)
{
#define DBGOUT(args...)  printk(args)
    NX_VIP_RegisterSet *pREG =
        (NX_VIP_RegisterSet*)NX_VIP_GetBaseAddress(module);

    DBGOUT("BASE ADDRESS: %p\n", pREG);
#if defined(CONFIG_ARCH_S5P4418)
    DBGOUT(" VIP_CONFIG     = 0x%04x\r\n", pREG->VIP_CONFIG);
    DBGOUT(" VIP_HVINT      = 0x%04x\r\n", pREG->VIP_HVINT);
    DBGOUT(" VIP_SYNCCTRL   = 0x%04x\r\n", pREG->VIP_SYNCCTRL);
    DBGOUT(" VIP_SYNCMON    = 0x%04x\r\n", pREG->VIP_SYNCMON);
    DBGOUT(" VIP_VBEGIN     = 0x%04x\r\n", pREG->VIP_VBEGIN);
    DBGOUT(" VIP_VEND       = 0x%04x\r\n", pREG->VIP_VEND);
    DBGOUT(" VIP_HBEGIN     = 0x%04x\r\n", pREG->VIP_HBEGIN);
    DBGOUT(" VIP_HEND       = 0x%04x\r\n", pREG->VIP_HEND);
    DBGOUT(" VIP_FIFOCTRL   = 0x%04x\r\n", pREG->VIP_FIFOCTRL);
    DBGOUT(" VIP_HCOUNT     = 0x%04x\r\n", pREG->VIP_HCOUNT);
    DBGOUT(" VIP_VCOUNT     = 0x%04x\r\n", pREG->VIP_VCOUNT);
    DBGOUT(" VIP_CDENB      = 0x%04x\r\n", pREG->VIP_CDENB);
    DBGOUT(" VIP_ODINT      = 0x%04x\r\n", pREG->VIP_ODINT);
    DBGOUT(" VIP_IMGWIDTH   = 0x%04x\r\n", pREG->VIP_IMGWIDTH);
    DBGOUT(" VIP_IMGHEIGHT  = 0x%04x\r\n", pREG->VIP_IMGHEIGHT);
    DBGOUT(" CLIP_LEFT      = 0x%04x\r\n", pREG->CLIP_LEFT);
    DBGOUT(" CLIP_RIGHT     = 0x%04x\r\n", pREG->CLIP_RIGHT);
    DBGOUT(" CLIP_TOP       = 0x%04x\r\n", pREG->CLIP_TOP);
    DBGOUT(" CLIP_BOTTOM    = 0x%04x\r\n", pREG->CLIP_BOTTOM);
    DBGOUT(" DECI_TARGETW   = 0x%04x\r\n", pREG->DECI_TARGETW);
    DBGOUT(" DECI_TARGETH   = 0x%04x\r\n", pREG->DECI_TARGETH);
    DBGOUT(" DECI_DELTAW    = 0x%04x\r\n", pREG->DECI_DELTAW);
    DBGOUT(" DECI_DELTAH    = 0x%04x\r\n", pREG->DECI_DELTAH);
    DBGOUT(" DECI_CLEARW    = 0x%04x\r\n", pREG->DECI_CLEARW);
    DBGOUT(" DECI_CLEARH    = 0x%04x\r\n", pREG->DECI_CLEARH);
    DBGOUT(" DECI_LUSEG     = 0x%04x\r\n", pREG->DECI_LUSEG);
    DBGOUT(" DECI_CRSEG     = 0x%04x\r\n", pREG->DECI_CRSEG);
    DBGOUT(" DECI_CBSEG     = 0x%04x\r\n", pREG->DECI_CBSEG);
    DBGOUT(" DECI_FORMAT    = 0x%04x\r\n", pREG->DECI_FORMAT);
    DBGOUT(" DECI_ROTFLIP   = 0x%04x\r\n", pREG->DECI_ROTFLIP);
    DBGOUT(" DECI_LULEFT    = 0x%04x\r\n", pREG->DECI_LULEFT);
    DBGOUT(" DECI_CRLEFT    = 0x%04x\r\n", pREG->DECI_CRLEFT);
    DBGOUT(" DECI_CBLEFT    = 0x%04x\r\n", pREG->DECI_CBLEFT);
    DBGOUT(" DECI_LURIGHT   = 0x%04x\r\n", pREG->DECI_LURIGHT);
    DBGOUT(" DECI_CRRIGHT   = 0x%04x\r\n", pREG->DECI_CRRIGHT);
    DBGOUT(" DECI_CBRIGHT   = 0x%04x\r\n", pREG->DECI_CBRIGHT);
    DBGOUT(" DECI_LUTOP     = 0x%04x\r\n", pREG->DECI_LUTOP);
    DBGOUT(" DECI_CRTOP     = 0x%04x\r\n", pREG->DECI_CRTOP);
    DBGOUT(" DECI_CBTOP     = 0x%04x\r\n", pREG->DECI_CBTOP);
    DBGOUT(" DECI_LUBOTTOM  = 0x%04x\r\n", pREG->DECI_LUBOTTOM);
    DBGOUT(" DECI_CRBOTTOM  = 0x%04x\r\n", pREG->DECI_CRBOTTOM);
    DBGOUT(" DECI_CBBOTTOM  = 0x%04x\r\n", pREG->DECI_CBBOTTOM);
    DBGOUT(" CLIP_LUSEG     = 0x%04x\r\n", pREG->CLIP_LUSEG);
    DBGOUT(" CLIP_CRSEG     = 0x%04x\r\n", pREG->CLIP_CRSEG);
    DBGOUT(" CLIP_CBSEG     = 0x%04x\r\n", pREG->CLIP_CBSEG);
    DBGOUT(" CLIP_FORMAT    = 0x%04x\r\n", pREG->CLIP_FORMAT);
    DBGOUT(" CLIP_ROTFLIP   = 0x%04x\r\n", pREG->CLIP_ROTFLIP);
    DBGOUT(" CLIP_LULEFT    = 0x%04x\r\n", pREG->CLIP_LULEFT);
    DBGOUT(" CLIP_CRLEFT    = 0x%04x\r\n", pREG->CLIP_CRLEFT);
    DBGOUT(" CLIP_CBLEFT    = 0x%04x\r\n", pREG->CLIP_CBLEFT);
    DBGOUT(" CLIP_LURIGHT   = 0x%04x\r\n", pREG->CLIP_LURIGHT);
    DBGOUT(" CLIP_CRRIGHT   = 0x%04x\r\n", pREG->CLIP_CRRIGHT);
    DBGOUT(" CLIP_CBRIGHT   = 0x%04x\r\n", pREG->CLIP_CBRIGHT);
    DBGOUT(" CLIP_LUTOP     = 0x%04x\r\n", pREG->CLIP_LUTOP);
    DBGOUT(" CLIP_CRTOP     = 0x%04x\r\n", pREG->CLIP_CRTOP);
    DBGOUT(" CLIP_CBTOP     = 0x%04x\r\n", pREG->CLIP_CBTOP);
    DBGOUT(" CLIP_LUBOTTOM  = 0x%04x\r\n", pREG->CLIP_LUBOTTOM);
    DBGOUT(" CLIP_CRBOTTOM  = 0x%04x\r\n", pREG->CLIP_CRBOTTOM);
    DBGOUT(" CLIP_CBBOTTOM  = 0x%04x\r\n", pREG->CLIP_CBBOTTOM);
    DBGOUT(" VIP_SCANMODE   = 0x%04x\r\n", pREG->VIP_SCANMODE);
    DBGOUT(" CLIP_YUYVENB   = 0x%04x\r\n", pREG->CLIP_YUYVENB);
    DBGOUT(" CLIP_BASEADDRH = 0x%04x\r\n", pREG->CLIP_BASEADDRH);
    DBGOUT(" CLIP_BASEADDRL = 0x%04x\r\n", pREG->CLIP_BASEADDRL);
    DBGOUT(" CLIP_STRIDEH   = 0x%04x\r\n", pREG->CLIP_STRIDEH);
    DBGOUT(" CLIP_STRIDEL   = 0x%04x\r\n", pREG->CLIP_STRIDEL);
    DBGOUT(" VIP_VIP1       = 0x%04x\r\n", pREG->VIP_VIP1);
#elif defined(CONFIG_ARCH_S5P6818)
    DBGOUT(" VIP_CONFIG     = 0x%04x\r\n", pREG->VIP_CONFIG);
    DBGOUT(" VIP_HVINT      = 0x%04x\r\n", pREG->VIP_HVINT);
    DBGOUT(" VIP_SYNCCTRL   = 0x%04x\r\n", pREG->VIP_SYNCCTRL);
    DBGOUT(" VIP_SYNCMON    = 0x%04x\r\n", pREG->VIP_SYNCMON);
    DBGOUT(" VIP_VBEGIN     = 0x%04x\r\n", pREG->VIP_VBEGIN);
    DBGOUT(" VIP_VEND       = 0x%04x\r\n", pREG->VIP_VEND);
    DBGOUT(" VIP_HBEGIN     = 0x%04x\r\n", pREG->VIP_HBEGIN);
    DBGOUT(" VIP_HEND       = 0x%04x\r\n", pREG->VIP_HEND);
    DBGOUT(" VIP_FIFOCTRL   = 0x%04x\r\n", pREG->VIP_FIFOCTRL);
    DBGOUT(" VIP_HCOUNT     = 0x%04x\r\n", pREG->VIP_HCOUNT);
    DBGOUT(" VIP_VCOUNT     = 0x%04x\r\n", pREG->VIP_VCOUNT);
    DBGOUT(" VIP_PADCLK_SEL = 0x%04x\r\n", pREG->VIP_PADCLK_SEL);
    DBGOUT(" VIP_INFIFOCLR  = 0x%04x\r\n", pREG->VIP_INFIFOCLR);
    DBGOUT(" VIP_CDENB      = 0x%04x\r\n", pREG->VIP_CDENB);
    DBGOUT(" VIP_ODINT      = 0x%04x\r\n", pREG->VIP_ODINT);
    DBGOUT(" VIP_IMGWIDTH   = 0x%04x\r\n", pREG->VIP_IMGWIDTH);
    DBGOUT(" VIP_IMGHEIGHT  = 0x%04x\r\n", pREG->VIP_IMGHEIGHT);
    DBGOUT(" CLIP_LEFT      = 0x%04x\r\n", pREG->CLIP_LEFT);
    DBGOUT(" CLIP_RIGHT     = 0x%04x\r\n", pREG->CLIP_RIGHT);
    DBGOUT(" CLIP_TOP       = 0x%04x\r\n", pREG->CLIP_TOP);
    DBGOUT(" CLIP_BOTTOM    = 0x%04x\r\n", pREG->CLIP_BOTTOM);
    DBGOUT(" DECI_TARGETW   = 0x%04x\r\n", pREG->DECI_TARGETW);
    DBGOUT(" DECI_TARGETH   = 0x%04x\r\n", pREG->DECI_TARGETH);
    DBGOUT(" DECI_DELTAW    = 0x%04x\r\n", pREG->DECI_DELTAW);
    DBGOUT(" DECI_DELTAH    = 0x%04x\r\n", pREG->DECI_DELTAH);
    DBGOUT(" DECI_CLEARW    = 0x%04x\r\n", pREG->DECI_CLEARW);
    DBGOUT(" DECI_CLEARH    = 0x%04x\r\n", pREG->DECI_CLEARH);
    DBGOUT(" DECI_FORMAT    = 0x%04x\r\n", pREG->DECI_FORMAT);
    DBGOUT(" DECI_LUADDR    = 0x%04x\r\n", pREG->DECI_LUADDR);
    DBGOUT(" DECI_LUSTRIDE  = 0x%04x\r\n", pREG->DECI_LUSTRIDE);
    DBGOUT(" DECI_CRADDR    = 0x%04x\r\n", pREG->DECI_CRADDR);
    DBGOUT(" DECI_CRSTRIDE  = 0x%04x\r\n", pREG->DECI_CRSTRIDE);
    DBGOUT(" DECI_CBADDR    = 0x%04x\r\n", pREG->DECI_CBADDR);
    DBGOUT(" DECI_CBSTRIDE  = 0x%04x\r\n", pREG->DECI_CBSTRIDE);
    DBGOUT(" CLIP_FORMAT    = 0x%04x\r\n", pREG->CLIP_FORMAT);
    DBGOUT(" CLIP_LUADDR    = 0x%04x\r\n", pREG->CLIP_LUADDR);
    DBGOUT(" CLIP_LUSTRIDE  = 0x%04x\r\n", pREG->CLIP_LUSTRIDE);
    DBGOUT(" CLIP_CRADDR    = 0x%04x\r\n", pREG->CLIP_CRADDR);
    DBGOUT(" CLIP_CRSTRIDE  = 0x%04x\r\n", pREG->CLIP_CRSTRIDE);
    DBGOUT(" CLIP_CBADDR    = 0x%04x\r\n", pREG->CLIP_CBADDR);
    DBGOUT(" CLIP_CBSTRIDE  = 0x%04x\r\n", pREG->CLIP_CBSTRIDE);
    DBGOUT(" VIP_SCANMODE   = 0x%04x\r\n", pREG->VIP_SCANMODE);
    DBGOUT(" VIP_VIP1       = 0x%04x\r\n", pREG->VIP_VIP1);
#endif
}

#if defined(CONFIG_ARCH_S5P6818)
static void _vip_hw_set_addr(int module, struct nxp_backward_camera_platform_data *param, u32 lu_addr, u32 cb_addr, u32 cr_addr);
static void _vip_hw_set_clock(int module, struct nxp_backward_camera_platform_data *param, bool on);
static void _vip_hw_set_sensor_param(int module, struct nxp_backward_camera_platform_data *param);
#endif

extern struct ion_device *get_global_ion_device(void);
static void _set_vip_interrupt(struct nxp_backward_camera_context *, bool);
static int _video_allocation_memory(struct nxp_backward_camera_context *, struct nxp_video_frame_buf *);
static void _release_display_irq_callback(struct nxp_backward_camera_context *);
static void _reset_queue(struct nxp_backward_camera_context *);
static void _init_hw_mlc(struct nxp_backward_camera_context *);
static int _init_worker(struct nxp_backward_camera_context *);
static void _cancel_worker(struct nxp_backward_camera_context *);
static int _enqueue_all_buffer_to_empty_queue(struct nxp_backward_camera_context *);

#if defined(CONFIG_ARCH_S5P6818)
static void _init_hw_deinter(struct nxp_backward_camera_context *);
static void _set_deinter_interrupt(struct nxp_backward_camera_context *, bool );
#endif

static void _reset_values(struct nxp_backward_camera_context *);
static int _stride_cal(struct nxp_backward_camera_context *, enum FRAME_KIND);


static void _vip_hw_set_clock(int module, struct nxp_backward_camera_platform_data *param, bool on)
{
#if defined(CONFIG_ARCH_S5P4418)
    U32 ClkSrc = 2; 
    U32 Divisor = 2; 
#elif defined(CONFIG_ARCH_S5P6818)
    U32 ClkSrc = 0; 
    U32 Divisor = 8; 
#endif

    if (on) {
        volatile void *clkgen_base = (volatile void *)IO_ADDRESS(NX_CLKGEN_GetPhysicalAddress(NX_VIP_GetClockNumber(module)));
        NX_CLKGEN_SetBaseAddress(NX_VIP_GetClockNumber(module), (void *)clkgen_base);
        NX_CLKGEN_SetClockDivisorEnable(NX_VIP_GetClockNumber(module), CTRUE);
        NX_CLKGEN_SetClockBClkMode(NX_VIP_GetClockNumber(module), NX_BCLKMODE_DYNAMIC);
#if defined(CONFIG_ARCH_S5P4418)
        NX_RSTCON_SetnRST(NX_VIP_GetResetNumber(module), RSTCON_nDISABLE);
        NX_RSTCON_SetnRST(NX_VIP_GetResetNumber(module), RSTCON_nENABLE);
#elif defined(CONFIG_ARCH_S5P6818)
        NX_RSTCON_SetRST(NX_VIP_GetResetNumber(module), RSTCON_ASSERT);
        NX_RSTCON_SetRST(NX_VIP_GetResetNumber(module), RSTCON_NEGATE);
#endif
        if (param->is_mipi) {
            NX_CLKGEN_SetClockSource(NX_VIP_GetClockNumber(module), 0, ClkSrc); /* external PCLK */
            NX_CLKGEN_SetClockDivisor(NX_VIP_GetClockNumber(module), 0, Divisor);
            NX_CLKGEN_SetClockDivisorEnable(NX_VIP_GetClockNumber(module), CTRUE);
        } else {
            NX_CLKGEN_SetClockSource(NX_VIP_GetClockNumber(module), 0, 4 + param->port); /* external PCLK */
            NX_CLKGEN_SetClockDivisor(NX_VIP_GetClockNumber(module), 0, 1);
            NX_CLKGEN_SetClockDivisorEnable(NX_VIP_GetClockNumber(module), CTRUE);
        }

        NX_VIP_SetBaseAddress(module, (void *)IO_ADDRESS(NX_VIP_GetPhysicalAddress(module)));
    }    
}

static void _vip_hw_set_sensor_param(int module, struct nxp_backward_camera_platform_data *param)
{
    if (param->is_mipi) {
        NX_VIP_SetInputPort(module, NX_VIP_INPUTPORT_B);
        NX_VIP_SetDataMode(module, NX_VIP_DATAORDER_CBY0CRY1, 16);
        NX_VIP_SetFieldMode(module, CFALSE, NX_VIP_FIELDSEL_BYPASS, CFALSE, CFALSE);
        NX_VIP_SetDValidMode(module, CTRUE, CTRUE, CTRUE);
        NX_VIP_SetFIFOResetMode(module, NX_VIP_FIFORESET_ALL);

        NX_VIP_SetHVSyncForMIPI(module,
                param->h_active * 2,
                param->v_active,
                param->h_syncwidth,
                param->h_frontporch,
                param->h_backporch,
                param->v_syncwidth,
                param->v_frontporch,
                param->v_backporch);
    } else {
        NX_VIP_RegisterSet *pREG = NULL;

        NX_VIP_SetDataMode(module, param->data_order, 8);
        NX_VIP_SetFieldMode(module,
                CFALSE,
                0,
                param->interlace,
                CFALSE);
        {
            pREG = (NX_VIP_RegisterSet*)NX_VIP_GetBaseAddress(module);
        }

        NX_VIP_SetDValidMode(module,
                CFALSE,
                CFALSE,
                CFALSE);
        NX_VIP_SetFIFOResetMode(module, NX_VIP_FIFORESET_ALL);
        NX_VIP_SetInputPort(module, (NX_VIP_INPUTPORT)param->port);

        NX_VIP_SetHVSync(module,
                param->external_sync,
                param->h_active*2,
                param->interlace ? param->v_active >> 1 : param->v_active,
                param->h_syncwidth,
                param->h_frontporch,
                param->h_backporch,
                param->v_syncwidth,
                param->v_frontporch,
                param->v_backporch);
    }

#if defined(CONFIG_ARCH_S5P4418)
    NX_VIP_SetClipperFormat(module, NX_VIP_FORMAT_420, 0, 0, 0);
#else
    NX_VIP_SetClipperFormat(module, NX_VIP_FORMAT_420);
#endif

    NX_VIP_SetClipRegion(module,
            0,
            0,
            param->h_active,
            param->interlace ? param->v_active >> 1 : param->v_active);
}


static void _vip_hw_set_addr(int module, struct nxp_backward_camera_platform_data *param, u32 lu_addr, u32 cb_addr, u32 cr_addr)
{
#if 0
    U32 Width = param->h_active-(param->h_active%32);
    U32 Height = param->v_active-(param->v_active%32);
#else
    U32 Width = param->h_active;
    U32 Height = param->v_active;
#endif

    if (param->is_mipi)
    {
        NX_VIP_SetClipperAddr(module, NX_VIP_FORMAT_420,
            Width,
            Height,
            lu_addr, cb_addr, cr_addr,
            //param->interlace ? ALIGN(Width, 64) : Width,
            //param->interlace ? ALIGN(Width/2, 64) : Width/2);
            param->interlace ? ALIGN(Width, 64) : param->lu_stride,
            param->interlace ? ALIGN(Width/2, 64) : param->cb_stride);
    }
    else
    {
        NX_VIP_SetClipperAddr(module, NX_VIP_FORMAT_420, param->h_active, param->v_active,
                lu_addr, cb_addr, cr_addr,
                param->lu_stride,
                param->cb_stride);
               // param->interlace ? ALIGN(param->h_active, 64)   : param->h_active,
               // param->interlace ? ALIGN(param->h_active/2, 64) : param->h_active/2);
    }
}


static void _vip_run(int module)
{
    struct nxp_backward_camera_context *me = &_context;

    debug_msg("+++ %s +++\n", __func__);

    _vip_hw_set_clock(module, me->plat_data, true);
    _vip_hw_set_sensor_param(module, me->plat_data);

    if (!me->plat_data->use_deinterlacer) {
        u32 lu_addr = me->plat_data->lu_addr;
        u32 cb_addr = me->plat_data->cb_addr;
        u32 cr_addr = me->plat_data->cr_addr;

        _vip_hw_set_addr(module, me->plat_data, lu_addr, cb_addr, cr_addr);
    } else {
        struct queue_entry *ent = NULL; 
        struct nxp_video_frame_buf *buf = NULL;
        int vip_module_num = me->plat_data->vip_module_num;

        ent = me->q_vip_empty.dequeue(&me->q_vip_empty);
        if (ent) {
            buf = (struct nxp_video_frame_buf *)(ent->data);
            _vip_hw_set_addr(vip_module_num, me->plat_data, buf->lu_addr, buf->cb_addr, buf->cr_addr);
            me->frame_set.cur_entry_vip = ent;
        } else {
            BUG();
        }
    }
    NX_VIP_SetVIPEnable(module, CTRUE, CTRUE, CTRUE, CFALSE);
    /* _vip_dump_register(module); */

    debug_msg("--- %s ---\n", __func__);
}

static void _vip_stop(int module)
{
#if defined(CONFIG_ARCH_S5P6818)
#if 0
    {
        int intnum = 0;
        /*int intnum = 2; ODINT*/
        NX_VIP_SetInterruptEnable( module, intnum, CTRUE );
        while (CFALSE == NX_VIP_GetInterruptPending( module, intnum ));
        NX_VIP_ClearInterruptPendingAll( module );
    }
#endif
#endif
    NX_VIP_SetVIPEnable(module, CFALSE, CFALSE, CFALSE, CFALSE);
#if defined(CONFIG_ARCH_S5P6818)
    NX_RSTCON_SetRST(NX_VIP_GetResetNumber(module), RSTCON_ASSERT);
    NX_RSTCON_SetRST(NX_VIP_GetResetNumber(module), RSTCON_NEGATE);
#endif
}

static void _mlc_video_run(int module)
{
    NX_MLC_SetTopDirtyFlag(module);
    NX_MLC_SetVideoLayerLineBufferPowerMode(module, CTRUE);
    NX_MLC_SetVideoLayerLineBufferSleepMode(module, CFALSE);
    NX_MLC_SetLayerEnable(module, MLC_LAYER_VIDEO, CTRUE);
    NX_MLC_SetDirtyFlag(module, MLC_LAYER_VIDEO);
}

static void _mlc_video_stop(int module)
{
    NX_MLC_SetTopDirtyFlag(module);
    NX_MLC_SetVideoLayerLineBufferPowerMode(module, CFALSE);
    NX_MLC_SetVideoLayerLineBufferSleepMode(module, CTRUE);
    NX_MLC_SetLayerEnable(module, MLC_LAYER_VIDEO, CFALSE);
    NX_MLC_SetDirtyFlag(module, MLC_LAYER_VIDEO);

	//_mlc_dump_register(module);
}

static void _mlc_overlay_run(int module)
{
    u32 layer = MLC_LAYER_RGB_OVERLAY;
    NX_MLC_SetLayerEnable(module, layer, CTRUE);
    NX_MLC_SetDirtyFlag(module, layer);
}

static void _mlc_overlay_stop(int module)
{
    u32 layer = MLC_LAYER_RGB_OVERLAY;
    NX_MLC_SetLayerEnable(module, layer, CFALSE);
    NX_MLC_SetDirtyFlag(module, layer);
}

static void _mlc_video_set_param(int module, struct nxp_backward_camera_platform_data *param)
{
    int srcw = param->h_active;
    int srch = param->v_active;
    int dstw, dsth, hf, vf;

    NX_MLC_GetScreenSize(module, &dstw, &dsth);

    hf = 1, vf = 1;

    if (srcw == dstw && srch == dsth)
        hf = 0, vf = 0;

    NX_MLC_SetFormatYUV(module, NX_MLC_YUVFMT_420);
    NX_MLC_SetVideoLayerScale(module, srcw, srch, dstw, dsth,
            (CBOOL)hf, (CBOOL)hf, (CBOOL)vf, (CBOOL)vf);
    NX_MLC_SetPosition(module, MLC_LAYER_VIDEO,
            0, 0, dstw - 1, dsth - 1);
#if 0 //keun 2015. 08. 17
    NX_MLC_SetLayerPriority(module, 0);
#else
    NX_MLC_SetLayerPriority(module, 1);
#endif
    NX_MLC_SetDirtyFlag(module, MLC_LAYER_VIDEO);
}

static void _mlc_video_set_addr(int module, u32 lu_a, u32 cb_a, u32 cr_a, u32 lu_s, u32 cb_s, u32 cr_s)
{
    NX_MLC_SetVideoLayerStride (module, lu_s, cb_s, cr_s);
    NX_MLC_SetVideoLayerAddress(module, lu_a, cb_a, cr_a);
    NX_MLC_SetVideoLayerLineBufferPowerMode(module, CTRUE);
    NX_MLC_SetVideoLayerLineBufferSleepMode(module, CFALSE);
    NX_MLC_SetDirtyFlag(module, MLC_LAYER_VIDEO);
}

static inline u32 _get_pixel_byte(u32 nxp_rgb_format)
{
    switch (nxp_rgb_format) {
        case NX_MLC_RGBFMT_R5G6B5:
        case NX_MLC_RGBFMT_B5G6R5:
        case NX_MLC_RGBFMT_X1R5G5B5:
        case NX_MLC_RGBFMT_X1B5G5R5:
        case NX_MLC_RGBFMT_X4R4G4B4:
        case NX_MLC_RGBFMT_X4B4G4R4:
        case NX_MLC_RGBFMT_X8R3G3B2:
        case NX_MLC_RGBFMT_X8B3G3R2:
        case NX_MLC_RGBFMT_A1R5G5B5:
        case NX_MLC_RGBFMT_A1B5G5R5:
        case NX_MLC_RGBFMT_A4R4G4B4:
        case NX_MLC_RGBFMT_A4B4G4R4:
        case NX_MLC_RGBFMT_A8R3G3B2:
        case NX_MLC_RGBFMT_A8B3G3R2:
            return 2;

        case NX_MLC_RGBFMT_R8G8B8:
        case NX_MLC_RGBFMT_B8G8R8:
            return 3;

        case NX_MLC_RGBFMT_A8R8G8B8:
        case NX_MLC_RGBFMT_A8B8G8R8:
            return 4;

        default:
            pr_err("%s: invalid nxp_rgb_format(0x%x)\n", __func__, nxp_rgb_format);
            return 0;
    }
}

static void _mlc_rgb_overlay_set_param(int module, struct nxp_backward_camera_platform_data *param)
{
    u32 format = param->rgb_format;
    u32 pixelbyte = _get_pixel_byte(format);
    u32 stride = param->width * pixelbyte;
    u32 layer = MLC_LAYER_RGB_OVERLAY;
    CBOOL EnAlpha = CFALSE;

    if (format == MLC_RGBFMT_A1R5G5B5 ||
        format == MLC_RGBFMT_A1B5G5R5 ||
        format == MLC_RGBFMT_A4R4G4B4 ||
        format == MLC_RGBFMT_A4B4G4R4 ||
        format == MLC_RGBFMT_A8R3G3B2 ||
        format == MLC_RGBFMT_A8B3G3R2 ||
        format == MLC_RGBFMT_A8R8G8B8 ||
        format == MLC_RGBFMT_A8B8G8R8)
        EnAlpha = CTRUE;

    NX_MLC_SetColorInversion(module, layer, CFALSE, 0);
    NX_MLC_SetAlphaBlending(module, layer, EnAlpha, 0);
    NX_MLC_SetFormatRGB(module, layer, (NX_MLC_RGBFMT)format);
    NX_MLC_SetRGBLayerInvalidPosition(module, layer, 0, 0, 0, 0, 0, CFALSE);
    NX_MLC_SetRGBLayerInvalidPosition(module, layer, 1, 0, 0, 0, 0, CFALSE);
    NX_MLC_SetPosition(module, layer, 0, 0, param->width-1, param->height-1);

    NX_MLC_SetRGBLayerStride (module, layer, pixelbyte, stride);
    NX_MLC_SetRGBLayerAddress(module, layer, param->rgb_addr);
    NX_MLC_SetDirtyFlag(module, layer);
}

static void _mlc_rgb_overlay_draw(int module, struct nxp_backward_camera_platform_data *me, void *mem)
{
    if (me->draw_rgb_overlay)
        me->draw_rgb_overlay(me, (void *)me->vendor_context, mem);
}

static int _get_i2c_client(struct nxp_backward_camera_context *me)
{
    struct i2c_client *client;
    struct i2c_adapter *adapter = i2c_get_adapter(me->plat_data->i2c_bus);

    if (!adapter) {
        pr_err("%s: unable to get i2c adapter %d\n", __func__, me->plat_data->i2c_bus);
        return -EINVAL;
    }

    client = kzalloc(sizeof *client, GFP_KERNEL);
    if (!client) {
        pr_err("%s: can't allocate i2c_client\n", __func__);
        return -ENOMEM;
    }

    client->adapter = adapter;
    client->addr = me->plat_data->chip_addr;
    client->flags = 0;

    me->client = client;

    return 0;
}

static int _camera_sensor_run(struct nxp_backward_camera_context *me)
{
    struct reg_val *reg_val;

    if (me->plat_data->power_enable)
        me->plat_data->power_enable(true);

    if (me->plat_data->setup_io)
        me->plat_data->setup_io();

    if (me->plat_data->set_clock)
        me->plat_data->set_clock(me->plat_data->clk_rate);

    reg_val = me->plat_data->reg_val;
    while (reg_val->reg != 0xff) {
        i2c_smbus_write_byte_data(me->client, reg_val->reg, reg_val->val);
        reg_val++;
    }

    if (me->plat_data->init_func)
        me->plat_data->init_func();

    return 0;
}

static void _turn_on(struct nxp_backward_camera_context *me)
{
    debug_msg("+++ %s +++\n", __func__);

    if (me->is_on)
		return;

    me->is_on = true;

    if (me->plat_data->pre_turn_on) {
        if (!me->plat_data->pre_turn_on(me->plat_data->vendor_context)) {
            printk(KERN_ERR "failed to platform pre_turn_on() function for backward camera\n");
            return;
        }
    }

    if (me->is_first == true) {
        if (!me->plat_data->use_deinterlacer) {
            //_vip_run(me->plat_data->vip_module_num);
            _mlc_video_set_param(me->plat_data->mlc_module_num, me->plat_data);
            _mlc_video_set_addr(me->plat_data->mlc_module_num,
                    me->plat_data->lu_addr,
                    me->plat_data->cb_addr,
                    me->plat_data->cr_addr,
                    me->plat_data->lu_stride,
                    me->plat_data->cb_stride,
                    me->plat_data->cr_stride);

            _mlc_rgb_overlay_set_param(me->plat_data->mlc_module_num, me->plat_data);
            _mlc_rgb_overlay_draw(me->plat_data->mlc_module_num, me->plat_data, me->virt_rgb);
        }
		//me->is_first = false;
    }

    if (!me->plat_data->use_deinterlacer) {
		//_mlc_dump_register(me->plat_data->mlc_module_num);
        _mlc_video_run(me->plat_data->mlc_module_num);
        _mlc_overlay_run(me->plat_data->mlc_module_num);
    } else {
        _reset_values(me);
        _enqueue_all_buffer_to_empty_queue(me);
		//_vip_run(me->plat_data->vip_module_num);

        /**
         * HACK : real start here!!!
         */
#if defined(CONFIG_ARCH_S5P6818)
        _set_deinter_interrupt(me, true);
#endif
        _set_vip_interrupt(me, true);
    }

    debug_msg("--- %s ---\n", __func__);
}

static void _cleanup_deinter(struct nxp_backward_camera_context *me)
{
    mutex_lock(&me->deinter_lock);
#if defined(CONFIG_ARCH_S5P6818)
    _set_deinter_interrupt(me, false);
#endif
    mutex_unlock(&me->deinter_lock);

    _cancel_worker(me);
}

static void _turn_off(struct nxp_backward_camera_context *me)
{
    _mlc_overlay_stop(me->plat_data->mlc_module_num);
    _mlc_video_stop(me->plat_data->mlc_module_num);
    
    if (me->plat_data->use_deinterlacer) {
        _set_vip_interrupt(me, false);
        //_vip_stop(me->plat_data->vip_module_num);

        _cleanup_deinter(me);

        _release_display_irq_callback(me);
        _reset_queue(me);

        me->is_display_on = false;
    }
	me->is_on = false;

    if (me->plat_data->post_turn_off)
        me->plat_data->post_turn_off(me->plat_data->vendor_context);
}

#define THINE_I2C_RETRY_CNT             3
static int _i2c_read_byte(struct i2c_client *client, u8 addr, u8 *data)
{
    s8 i = 0;
    s8 ret = 0;
    u8 buf = 0;
    struct i2c_msg msg[2];

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = &addr;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = &buf;

    for(i=0; i<THINE_I2C_RETRY_CNT; i++) {
        ret = i2c_transfer(client->adapter, msg, 2); 
        if (likely(ret == 2)) 
            break;
    }   

    if (unlikely(ret != 2)) {
        dev_err(&client->dev, "_i2c_read_byte failed reg:0x%02x\n", addr);
        return -EIO;
    }   

    *data = buf;
    return 0;
}

static inline bool _is_backgear_on(struct nxp_backward_camera_platform_data *pdata)
{
#if 1
    bool is_on = nxp_soc_gpio_get_in_value(pdata->backgear_gpio_num);
    if (!pdata->active_high) is_on ^= 1;
    return is_on;
#else
	//tw9900 read status
	u8 data = 0;
	_i2c_read_byte(me->client, 0x01, &data);
	if( data & 0x80 )
		return 0;
	return 1;
#endif
}

static inline bool _is_running(struct nxp_backward_camera_context *me)
{
#if 0
    CBOOL vipenb, sepenb, clipenb, decenb;
    bool mlcenb;

    NX_VIP_GetVIPEnable(me->plat_data->vip_module_num, &vipenb, &sepenb, &clipenb, &decenb);
    mlcenb = NX_MLC_GetLayerEnable(me->plat_data->mlc_module_num, 3);

    return mlcenb && vipenb && sepenb && clipenb;
#else
	return NX_MLC_GetLayerEnable(me->plat_data->mlc_module_num, 3);
#endif
}


#if ENABLE_UEVENT
static void _backgear_switch(int on)
{
	if( backgear_switch != NULL )
		switch_set_state(backgear_switch, on);	
	else
		printk("%s - backgear switch is NULL!!!\n", __func__);
}
#endif

static void _decide(struct nxp_backward_camera_context *me)
{
    debug_msg("+++ %s +++\n", __func__);

    if (me->plat_data->decide) {
        if (!me->plat_data->decide(me->plat_data->vendor_context)) {
            printk(KERN_ERR "failed to platform decide() function for backward camera\n");
            return;
        }
    }

    me->running = _is_running(me);
    me->backgear_on = _is_backgear_on(me->plat_data);
    printk(KERN_ERR "%s: running %d, backgear on %d\n", __func__, me->running, me->backgear_on);
    if (me->backgear_on && !me->running) {
		_turn_on(me);
#if ENABLE_UEVENT
		_backgear_switch(1);
#endif
    } else if ((me->running && !me->backgear_on) || (!me->running && !me->backgear_on)) {
		_turn_off(me);
#if ENABLE_UEVENT
		_backgear_switch(0);
#endif
    } else if (!me->running && _is_backgear_on(me->plat_data)) {
        printk(KERN_ERR "Recheck backcamera!!!\n");
        schedule_delayed_work(&me->work, msecs_to_jiffies(me->plat_data->turn_on_delay_ms));
    }

    debug_msg("--- %s ---\n", __func__);
}

static int _hw_get_irq_num(struct nxp_backward_camera_context *me)
{
#if defined(CONFIG_ARCH_S5P4418)
	return NX_VIP_GetInterruptNumber(me->plat_data->vip_module_num);
#elif defined(CONFIG_ARCH_S5P6818)
	return NX_VIP_GetInterruptNumber(me->plat_data->vip_module_num) + 32;
#endif
}

static int wait_for_end_of_work(wait_queue_head_t *wq, atomic_t *status, int init_status, int end_work_status, int elapse)
{
	long timeout = 0;

	if (atomic_read(status) == init_status) {
		timeout = wait_event_interruptible_timeout(*wq, atomic_read(status) == end_work_status, elapse);
		if(timeout <= 0)
		{
			printk(KERN_INFO "The timeout elapsed before the condition evaluated to true!! Condition Status = %d, timeout : %ld\n", atomic_read(status), timeout);
			return -EBUSY;
		}
	}
	else
		return -1;

	return 0;
}

static void _set_display_irq_callback(struct nxp_backward_camera_context *);

static irqreturn_t deinter_irq_handler(int irq, void *devdata)
{
	struct nxp_backward_camera_context *me = devdata;

	NX_DEINTERLACE_ClearInterruptPendingAll();

	atomic_set(&me->status, PROCESSING_STOP);
	wake_up_interruptible(&me->wq_deinter_end);

	return IRQ_HANDLED;
}

#if DROP_INTERLACE_FRAME
static uint32_t _irq_count = 0;
#endif
static irqreturn_t vip_irq_handler(int irq, void *devdata)
{
	struct nxp_backward_camera_context *me = devdata;

	struct queue_entry *entry = NULL;
	struct nxp_video_frame_buf *buf = NULL;
    unsigned long flags;

#if DROP_INTERLACE_FRAME
    bool interlace = false;
    bool do_process = true;
#endif
    
    int vip_module_num = me->plat_data->vip_module_num;

	NX_VIP_ClearInterruptPendingAll(me->plat_data->vip_module_num);
    
#if DROP_INTERLACE_FRAME
    interlace = me->plat_data->interlace;
    if (interlace) {
        // patch  for odd/even sequence
#if 0
        _irq_count++;
#else
        if(_irq_count == 0) {
            // must odd
            if(CFALSE == NX_VIP_GetFieldStatus(vip_module_num)) {
                _irq_count++;
                //printk(KERN_ERR "%s - ODD!\n", __func__);
            }
        } else {
            // must even
            if(CTRUE == NX_VIP_GetFieldStatus(vip_module_num)) {
                _irq_count++;
                //printk(KERN_ERR "%s - EVEN!\n", __func__);
            }
        }
#endif

        if (_irq_count == 2) {
            _irq_count = 0;
        } else {
            do_process = false;
        }
    }

    if (!do_process) return IRQ_HANDLED;
#endif

    spin_lock_irqsave(&me->vip_lock, flags);
    entry = me->frame_set.cur_entry_vip;

    buf = (struct nxp_video_frame_buf *)(entry->data);
	debug_msg("%s - lu_addr : 0x%x", __func__, buf->lu_addr); 

    if (entry) {
        me->q_vip_done.enqueue(&me->q_vip_done, entry);
        me->frame_set.cur_entry_vip = NULL;
    }

    entry =  me->q_vip_empty.dequeue(&me->q_vip_empty);
    if (!entry) {
        printk(KERN_ERR "VIP Empty Buffer Underrun!!!!\n");
        spin_unlock_irqrestore(&me->vip_lock, flags);
	    return IRQ_HANDLED;
    } else {
        buf = (struct nxp_video_frame_buf *)(entry->data);
		_vip_hw_set_addr(vip_module_num, me->plat_data, buf->lu_addr, buf->cb_addr, buf->cr_addr);
        me->frame_set.cur_entry_vip = entry;
    }

#if DEINTER_DISPLAY
    // HACK : why?
    //if (!work_pending(&me->work_deinter))
    queue_work(me->wq_deinter, &me->work_deinter);
#else
    if (!me->is_display_on) {
        me->is_display_on = true;
        _set_display_irq_callback(me);
	}
#endif

    spin_unlock_irqrestore(&me->vip_lock, flags);

	return IRQ_HANDLED;
}

static void _release_irq_vip(void)
{
	struct nxp_backward_camera_context *me = &_context;
    if( me->vip_irq ) free_irq(me->vip_irq, &_context);
}

static void _release_irq_deinter(void)
{
	struct nxp_backward_camera_context *me = &_context;
    if( me->deinter_irq ) free_irq(me->deinter_irq, &_context);
}

static void _init_hw_vip(struct nxp_backward_camera_context *me)
{
    int ret=0;

	ret = request_irq(me->vip_irq, vip_irq_handler, IRQF_SHARED, "backward-vip", me);
	if(ret) BUG();
}

#if defined(CONFIG_ARCH_S5P6818)
static void _set_deinter_interrupt(struct nxp_backward_camera_context *me, bool on)
{
   	NX_DEINTERLACE_SetInterruptEnableAll(CFALSE);
    NX_DEINTERLACE_ClearInterruptPendingAll();

    if (on) 
	    NX_DEINTERLACE_SetInterruptEnable(0, CTRUE);
}

static void _init_hw_deinter(struct nxp_backward_camera_context *me)
{
	NX_DEINTERLACE_Initialize();
	NX_DEINTERLACE_SetBaseAddress( (void *)IO_ADDRESS(NX_DEINTERLACE_GetPhysicalAddress()) );

	NX_CLKGEN_SetBaseAddress( NX_DEINTERLACE_GetClockNumber(), (void *)IO_ADDRESS(NX_CLKGEN_GetPhysicalAddress(NX_DEINTERLACE_GetClockNumber())));
	NX_CLKGEN_SetClockBClkMode( NX_DEINTERLACE_GetClockNumber(), NX_BCLKMODE_ALWAYS );

	NX_RSTCON_SetRST(NX_DEINTERLACE_GetResetNumber(), RSTCON_NEGATE);

	NX_DEINTERLACE_OpenModule();
	NX_DEINTERLACE_SetInterruptEnableAll( CFALSE );
	NX_DEINTERLACE_ClearInterruptPendingAll();

	me->deinter_irq = NX_DEINTERLACE_GetInterruptNumber();
	me->deinter_irq += 32;

	if (request_irq(me->deinter_irq, deinter_irq_handler, IRQF_SHARED, "backward-deinter", me))
        BUG();

}

static void SetDeInterlace( U16 Height, U16 Width,
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
    NX_DEINTERLACE_SetYSrcStride    ( Y_SrcStride );
    NX_DEINTERLACE_SetYDestStride   ( YDstFieldStride );

    // CB Regiseter Setting
    NX_DEINTERLACE_SetCBSrcImageSize( CHeight, CWidth );
    NX_DEINTERLACE_SetCBSrcAddrCurr ( CB_CurrAddr );
    NX_DEINTERLACE_SetCBSrcStride   ( CB_SrcStride );
    NX_DEINTERLACE_SetCBDestStride  ( CbDstFieldStride );

    // CR Regiseter Setting
    NX_DEINTERLACE_SetCRSrcImageSize( CHeight, CWidth );
    NX_DEINTERLACE_SetCRSrcAddrCurr ( CR_CurrAddr );
    NX_DEINTERLACE_SetCRSrcStride   ( CR_SrcStride );
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

static int _make_deinter_context(struct nxp_video_frame_buf *src1, struct nxp_video_frame_buf *src2, struct nxp_deinter_run_ctx *ctx_buf)
{
	struct nxp_backward_camera_context *me = &_context;

	struct nxp_video_frame_buf *frame0 = &ctx_buf->src[0];
	struct nxp_video_frame_buf *frame1 = &ctx_buf->src[1];
	struct nxp_video_frame_buf *frame2 = &ctx_buf->src[2];

	int src_width = me->frame_set.width;
	int src_height = (me->frame_set.height/2);

	debug_msg("%s - lu_addr : 0x%x, cb_addr : 0x%x, cr_addr : 0x%x\n",
			 __func__, deinter_buf->src[0].lu_addr, deinter_buf->src[0].cb_addr, deinter_buf->src[0].cr_addr); 

    if (me->plat_data->devide_frame) {
        if( me->frame_set.cur_mode == FIELD_EVEN) {
            debug_msg("\n%s - EVEN!\n", __func__);

            // EVEN
            frame0->frame_num = 0;
            frame0->lu_addr = src1->lu_addr;
            frame0->cb_addr = src1->cb_addr;
            frame0->cr_addr = src1->cr_addr;
            frame0->lu_stride = YUV_YSTRIDE(src_width);
            frame0->cb_stride = YUV_STRIDE(src_width/2);
            frame0->cr_stride = YUV_STRIDE(src_width/2);

            // ODD
            frame1->frame_num = 1;
            frame1->lu_addr = src1->lu_addr + (src1->lu_stride * src_height);
            frame1->cb_addr = src1->cb_addr + (src1->cb_stride * (src_height/2));
            frame1->cr_addr = src1->cr_addr + (src1->cr_stride * (src_height/2));
            frame1->lu_stride = YUV_YSTRIDE(src_width);
            frame1->cb_stride = YUV_STRIDE(src_width/2);
            frame1->cr_stride = YUV_STRIDE(src_width/2);

            // EVEN
            frame2->frame_num = 0;
            frame2->lu_addr = src2->lu_addr;
            frame2->cb_addr = src2->cb_addr;
            frame2->cr_addr = src2->cr_addr;
            frame2->lu_stride = YUV_YSTRIDE(src_width);
            frame2->cb_stride = YUV_STRIDE(src_width/2);
            frame2->cr_stride = YUV_STRIDE(src_width/2);
        } else {
            debug_msg("\n%s - ODD!\n", __func__);
            // ODD
            frame0->frame_num = 1;
            frame0->lu_addr = src1->lu_addr + (src1->lu_stride * src_height);
            frame0->cb_addr = src1->cb_addr + (src1->cb_stride * (src_height/2));
            frame0->cr_addr = src1->cr_addr + (src1->cr_stride * (src_height/2));
            frame0->lu_stride = YUV_YSTRIDE(src_width);
            frame0->cb_stride = YUV_STRIDE(src_width/2);
            frame0->cr_stride = YUV_STRIDE(src_width/2);

            // EVEN
            frame1->frame_num = 0;
            frame1->lu_addr = src2->lu_addr;
            frame1->cb_addr = src2->cb_addr;
            frame1->cr_addr = src2->cr_addr;
            frame1->lu_stride = YUV_YSTRIDE(src_width);
            frame1->cb_stride = YUV_STRIDE(src_width/2);
            frame1->cr_stride = YUV_STRIDE(src_width/2);

            // ODD
            frame2->frame_num = 1;
            frame2->lu_addr = src2->lu_addr + (src2->lu_stride * src_height);
            frame2->cb_addr = src2->cb_addr + (src2->cb_stride * (src_height/2));
            frame2->cr_addr = src2->cr_addr + (src2->cr_stride * (src_height/2));
            frame2->lu_stride = YUV_YSTRIDE(src_width);
            frame2->cb_stride = YUV_STRIDE(src_width/2);
            frame2->cr_stride = YUV_STRIDE(src_width/2);
        }
    } else {  //ODD First
        if( me->frame_set.cur_mode == FIELD_ODD) {
            debug_msg("\n%s - ODD!\n", __func__);

            // EVEN
            frame0->frame_num = 0;
            frame0->lu_addr = src1->lu_addr;
            frame0->cb_addr = src1->cb_addr;
            frame0->cr_addr = src1->cr_addr;
            frame0->lu_stride = YUV_STRIDE(src_width) * 2;
            frame0->cb_stride = YUV_STRIDE(src_width/2) * 2;
            frame0->cr_stride = YUV_STRIDE(src_width/2) * 2;

            // ODD
            frame1->frame_num = 1;
            frame1->lu_addr = src1->lu_addr + (src1->lu_stride * 2);
            frame1->cb_addr = src1->cb_addr + (src1->cb_stride * 2);
            frame1->cr_addr = src1->cr_addr + (src1->cr_stride * 2);
            frame1->lu_stride = YUV_STRIDE(src_width) * 2;
            frame1->cb_stride = YUV_STRIDE(src_width/2) * 2;
            frame1->cr_stride = YUV_STRIDE(src_width/2) * 2;

            // EVEN
            frame2->frame_num = 0;
            frame2->lu_addr = src2->lu_addr;
            frame2->cb_addr = src2->cb_addr;
            frame2->cr_addr = src2->cr_addr;
            frame2->lu_stride = YUV_STRIDE(src_width) * 2;
            frame2->cb_stride = YUV_STRIDE(src_width/2) * 2;
            frame2->cr_stride = YUV_STRIDE(src_width/2) * 2;
        } else {
            debug_msg("\n%s - EVEN!\n", __func__);

            // 
            frame0->frame_num = 1;
            frame0->lu_addr = src1->lu_addr + (src1->lu_stride * 2);
            frame0->cb_addr = src1->cb_addr + (src1->cb_stride * 2);
            frame0->cr_addr = src1->cr_addr + (src1->cr_stride * 2);
            frame0->lu_stride = YUV_STRIDE(src_width) * 2;
            frame0->cb_stride = YUV_STRIDE(src_width/2) * 2;
            frame0->cr_stride = YUV_STRIDE(src_width/2) * 2;

            // ODD
            frame1->frame_num = 0;
            frame1->lu_addr = src2->lu_addr;
            frame1->cb_addr = src2->cb_addr;
            frame1->cr_addr = src2->cr_addr;
            frame1->lu_stride = YUV_STRIDE(src_width) * 2;
            frame1->cb_stride = YUV_STRIDE(src_width/2) * 2;
            frame1->cr_stride = YUV_STRIDE(src_width/2) * 2;

            // EVEN
            frame2->frame_num = 1;
            frame2->lu_addr = src2->lu_addr + (src2->lu_stride * 2);
            frame2->cb_addr = src2->cb_addr + (src2->cb_stride * 2);
            frame2->cr_addr = src2->cr_addr + (src2->cr_stride * 2);
            frame2->lu_stride = YUV_STRIDE(src_width) * 2;
            frame2->cb_stride = YUV_STRIDE(src_width/2) * 2;
            frame2->cr_stride = YUV_STRIDE(src_width/2) * 2;
        }
    }

	me->frame_set.cur_mode ^= 1;

	return 1;
}

static void _deinter_run(struct nxp_deinter_run_ctx *ctx)
{
	struct nxp_backward_camera_context *me = &_context;

	unsigned long src_prev_y_data_phy, src_curr_y_data_phy, src_next_y_data_phy, src_curr_cb_data_phy, src_curr_cr_data_phy;
	unsigned long dst_y_data_phy, dst_cb_data_phy, dst_cr_data_phy;
	int width, height, src_y_stride, src_c_stride, dst_y_stride, dst_c_stride;
	unsigned long frame_num;

	int dst_y_data_size		=	0;
	int dst_cb_data_size	=	0;
	int dst_cr_data_size	=	0;

	struct nxp_video_frame_buf *s_frame0 = NULL;
	struct nxp_video_frame_buf *s_frame1 = NULL;
	struct nxp_video_frame_buf *s_frame2 = NULL;
	struct nxp_video_frame_buf *d_frame = NULL;

	atomic_set(&me->status, PROCESSING_START);	

	width = me->frame_set.width;

    height = (me->frame_set.height/2);

	s_frame0 = &ctx->src[0];
	s_frame1 = &ctx->src[1];
	s_frame2 = &ctx->src[2];

	d_frame = ctx->dst;

	frame_num = s_frame0->frame_num;	

	/* source */	
	src_y_stride = s_frame0->lu_stride;
	src_c_stride = s_frame0->cb_stride;

	src_prev_y_data_phy	= s_frame0->lu_addr;
	
	src_curr_y_data_phy = s_frame1->lu_addr;
	src_curr_cb_data_phy = s_frame1->cb_addr;
	src_curr_cr_data_phy = s_frame1->cr_addr;

	src_next_y_data_phy  = s_frame2->lu_addr; 

	/* destination */
	dst_y_stride = d_frame->lu_stride;
	dst_c_stride = d_frame->cb_stride;

	dst_y_data_phy = d_frame->lu_addr;
	dst_cb_data_phy = d_frame->cb_addr;
	dst_cr_data_phy = d_frame->cr_addr;

	dst_y_data_size = d_frame->lu_size;
	dst_cb_data_size = d_frame->cb_size;
	dst_cr_data_size = d_frame->cr_size;

#if 0
	debug_msg("%s - width : %d, height : %d, frame_num : %d, src y stride : %d, src c stride : %d\n", 
		__func__, width, height, frame_num, src_y_stride, src_c_stride);
	debug_msg("%s - src prev y phy : 0x%x\n", __func__, src_prev_y_data_phy);
	debug_msg("%s - src curr y phy : 0x%x, src curr cb phy : 0x%x, src curr cr phy : 0x%x\n",
		__func__, src_curr_y_data_phy, src_curr_cb_data_phy, src_curr_cr_data_phy);
	debug_msg("%s - src next y phy : 0x%x\n", __func__, src_next_y_data_phy);
	debug_msg("\n");
	debug_msg("%s - dst y stride : %d, dst c stride : %d\n", __func__, dst_y_stride, dst_c_stride);
	debug_msg("%s - dst curr y phy : 0x%x, dst curr cb phy : 0x%x, dst curr cr phy : 0x%x\n",
		__func__,  dst_y_data_phy, dst_cb_data_phy, dst_cr_data_phy);
#endif

	SetDeInterlace(height,
                   width,
                   src_prev_y_data_phy,
                   src_curr_y_data_phy,
                   src_next_y_data_phy,
                   src_y_stride,
                   dst_y_data_phy,
                   dst_y_stride,
                   src_curr_cb_data_phy,
                   src_c_stride,
                   dst_cb_data_phy,
                   dst_c_stride,
                   src_curr_cr_data_phy,
                   src_c_stride,
                   dst_cr_data_phy,
                   dst_c_stride,
                   frame_num % 2);

        //dump_deinterlace_register(0);
	NX_DEINTERLACE_DeinterlaceStart();

    if (0 > wait_for_end_of_work(&me->wq_deinter_end, &me->status, PROCESSING_START, PROCESSING_STOP, HZ/10)) 
        printk(KERN_ERR "%s: CRITICAL ERROR --> Deinterlacer hw timeout(over 100ms)!!!\n", __func__);
}
#endif

static void _display_callback(void *data)
{
	struct nxp_backward_camera_context *me = (struct nxp_backward_camera_context *)data;
	struct queue_entry *entry = NULL;
	struct nxp_video_frame_buf *buf = NULL;

#if DEINTER_DISPLAY
    struct common_queue *q_deinter_empty = &me->q_deinter_empty;
    struct common_queue *q_deinter_done = &me->q_deinter_done;

    int q_size = 0;
    int i=0;

    unsigned long flags;

    spin_lock_irqsave(&me->deinit_lock, flags);

    if (q_deinter_done->size(q_deinter_done) < 1) {
        spin_unlock_irqrestore(&me->deinit_lock, flags);
        return;
    }

    q_size = q_deinter_done->size(q_deinter_done);
    entry = q_deinter_done->peek(q_deinter_done, q_size-1);
    if (!entry) {
        spin_unlock_irqrestore(&me->deinit_lock, flags);
        return;
    }

	buf = (struct nxp_video_frame_buf *)(entry->data);
	debug_msg("\n%s - lu_addr : 0x%x, cb_addr : 0x%x, cr_addr : 0x%x\n\n", __func__, buf->lu_addr, buf->cb_addr, buf->cr_addr); 
	debug_msg("\n%s - lu_stride : %d, cb_stride : %d, cr_stride : %d\n\n", __func__, buf->lu_stride, buf->cb_stride, buf->cr_stride); 
    debug_msg("\n%s - 1 : current display lu_addr : 0x%x\n", __func__, buf->lu_addr); 

	_mlc_video_set_addr(me->plat_data->mlc_module_num,
		  buf->lu_addr,
		  buf->cb_addr,
		  buf->cr_addr,
		  buf->lu_stride,
		  buf->cb_stride,
		  buf->cr_stride);

    if (!me->mlc_on_first) {
        _mlc_rgb_overlay_set_param(me->plat_data->mlc_module_num, me->plat_data);
        _mlc_rgb_overlay_draw(me->plat_data->mlc_module_num, me->plat_data, me->virt_rgb);
        me->mlc_on_first = true;
    }

    if ( !me->is_mlc_on ) {
        _mlc_video_run(me->plat_data->mlc_module_num);
        _mlc_overlay_run(me->plat_data->mlc_module_num);
        me->is_mlc_on = true;
    }

    for(i=0; i<q_size-1; i++) {
        entry = q_deinter_done->dequeue(q_deinter_done);
        buf = (struct nxp_video_frame_buf *)(entry->data);
        debug_msg("%s - 2 : enqueue deinter empty lu_addr : 0x%x\n\n", __func__, buf->lu_addr); 
        q_deinter_empty->enqueue(q_deinter_empty, entry);
    }

    spin_unlock_irqrestore(&me->deinit_lock, flags);
#else
    unsigned long flags;
    int q_size = 0;
    int i=0;

    struct common_queue *q_vip_empty = &me->q_vip_empty;
    struct common_queue *q_vip_done  = &me->q_vip_done;

    spin_lock_irqsave(&me->deinit_lock, flags);

    if (q_vip_done->size(q_vip_done) <  1) {
        spin_unlock_irqrestore(&me->deinit_lock, flags);
        return;
    }

    q_size = q_vip_done->size(q_vip_done);
    entry = q_vip_done->peek(q_vip_done, q_size-1);
    if (!entry) {
        spin_unlock_irqrestore(&me->deinit_lock, flags);
        return;
    }
   
    buf = (struct nxp_video_frame_buf *)(entry->data);
    debug_msg("%s - lu_addr : 0x%x", __func__, buf->lu_addr); 

    _mlc_video_set_addr(me->plat_data->mlc_module_num,
            buf->lu_addr,
            buf->cb_addr,
            buf->cr_addr,
            buf->lu_stride,
            buf->cb_stride,
            buf->cr_stride);

    if ( !me->is_mlc_on ) {
        _mlc_video_run(me->plat_data->mlc_module_num);
        me->is_mlc_on = true;
    }

    for (i=0 ; i<q_size-1 ; i++) {
        entry = q_vip_done->dequeue(q_vip_done);
        buf = (struct nxp_video_frame_buf *)(entry->data);
        q_vip_empty->enqueue(q_vip_empty, entry);
    }

    spin_unlock_irqrestore(&me->deinit_lock, flags);
#endif
}

static irqreturn_t _irq_handler(int irq, void *devdata)
{
    struct nxp_backward_camera_context *me = devdata;

    printk(KERN_ERR "BCI\n");

    __cancel_delayed_work(&me->work);
    if (!_is_backgear_on(me->plat_data)) {
        schedule_delayed_work(&me->work, msecs_to_jiffies(0));
    } else {
        schedule_delayed_work(&me->work, msecs_to_jiffies(me->plat_data->turn_on_delay_ms));
    }    

    return IRQ_HANDLED;
}

static void _set_vip_interrupt(struct nxp_backward_camera_context *me, bool on)
{
	NX_VIP_ClearInterruptPendingAll(me->plat_data->vip_module_num);
	NX_VIP_SetInterruptEnableAll(me->plat_data->vip_module_num, CFALSE);

    debug_msg("%s - enable : %d\n", __func__, on);

	if( on )
		NX_VIP_SetInterruptEnable(me->plat_data->vip_module_num, CAPTURE_CLIPPER_INT, CTRUE);
}

#if defined(CONFIG_ARCH_S5P6818)
static void _deinter_worker(struct work_struct *work)
{
    struct nxp_backward_camera_context *me = &_context;

    struct nxp_video_frame_buf *src1 = NULL;
    struct nxp_video_frame_buf *src2 = NULL;

    struct nxp_video_frame_buf *dst = NULL;
    struct queue_entry *ent = NULL;

    struct nxp_deinter_run_ctx deinter_ctx;

    struct queue_entry *entry = NULL;
    struct nxp_video_frame_buf *buf = NULL;

    int e_idx = 0;

    // HACK : here handle queue next...
    struct common_queue *q_vip_done = &me->q_vip_done;
    struct common_queue *q_vip_empty = &me->q_vip_empty;
    struct common_queue *q_deinter_empty = &me->q_deinter_empty;
    struct common_queue *q_deinter_done = &me->q_deinter_done;

    // TODO : spinlock --> mutex
    mutex_lock(&me->deinter_lock);

	debug_msg("%s : vip done queue size : %d\n", __func__, me->q_vip_done.size(&me->q_vip_done));

    // HACK : avoid to VIP Buffer Underrun!!
    if (me->q_vip_done.size(&me->q_vip_done) < MININUM_INQUEUE_COUNT) {
        mutex_unlock(&me->deinter_lock);
        return;
    }

    // HACK : get deinter source buffer from vip done queue
    ent = q_vip_done->peek(q_vip_done, 0);
    if (!ent) BUG();
    src1 = (struct nxp_video_frame_buf *)(ent->data);

    ent = q_vip_done->peek(q_vip_done, 1);
    if (!ent) BUG();
    src2 = (struct nxp_video_frame_buf *)(ent->data);

    if (me->plat_data->is_odd_first)
        me->frame_set.cur_mode = FIELD_ODD;
    else
        me->frame_set.cur_mode = FIELD_EVEN;

    // HACK : q is deinterlacer dest empty buffer
    while( e_idx < 2 ) {
        ent = q_deinter_empty->dequeue(q_deinter_empty);
        if (ent == NULL) {
            printk(KERN_ERR "%s - DEINTER UNDERRUN --> Check Display Callback Cycle!!!, loop %d\n", __func__, e_idx);
            mutex_unlock(&me->deinter_lock);
            return;
        }

        dst = (struct nxp_video_frame_buf *)(ent->data);
        // TODO : dst set to pointer
        deinter_ctx.dst = dst;
        _make_deinter_context(src1, src2, &deinter_ctx);
        _deinter_run(&deinter_ctx);

        if (!me->is_display_on) {
            me->is_display_on = true;
            _set_display_irq_callback(me);
        }

        // trigger display callback
        q_deinter_done->enqueue(q_deinter_done, ent);
        e_idx++;	
    }

    // queue vip src buffer to empty 
    entry = q_vip_done->dequeue(q_vip_done);
    if (!entry) BUG();
    buf = (struct nxp_video_frame_buf *)(entry->data);
    debug_msg("%s -done q dequeue  -  lu_addr : 0x%x, cb_addr : 0x%x, cr_addr : 0x%x\n", __func__, buf->lu_addr, buf->cb_addr, buf->cr_addr);
    q_vip_empty->enqueue(q_vip_empty, entry);

    mutex_unlock(&me->deinter_lock);

    debug_msg("%s end\n", __func__);
}
#endif

void nxp_mipi_csi_setting(struct nxp_backward_camera_platform_data *me, int lanes)
{
#if defined(CONFIG_VIDEO_TW9992)
	#if defined(CONFIG_ARCH_S5P4418)
		U32 ClkSrc = 2;
		U32 Divisor = 2;
	#elif defined(CONFIG_ARCH_S5P6818)
		U32 ClkSrc = 0;
		U32 Divisor = 8;
	#endif
		CBOOL EnableDataLane0 = CFALSE;
		CBOOL EnableDataLane1 = CFALSE;
		CBOOL EnableDataLane2 = CFALSE;
		CBOOL EnableDataLane3 = CFALSE;
		U32   NumberOfDataLanes = (lanes-1);
		if(NumberOfDataLanes >= 0) EnableDataLane0=CTRUE;
		if(NumberOfDataLanes >= 1) EnableDataLane1=CTRUE;
		if(NumberOfDataLanes >= 2) EnableDataLane2=CTRUE;
		if(NumberOfDataLanes >= 3) EnableDataLane3=CTRUE;
#endif

    NX_MIPI_Initialize();
    NX_TIEOFF_Set(TIEOFFINDEX_OF_MIPI0_NX_DPSRAM_1R1W_EMAA, 3);
    NX_TIEOFF_Set(TIEOFFINDEX_OF_MIPI0_NX_DPSRAM_1R1W_EMAA, 3);

    NX_MIPI_SetBaseAddress(me->vip_module_num, (void *)IO_ADDRESS(NX_MIPI_GetPhysicalAddress(me->vip_module_num)));
    NX_CLKGEN_SetBaseAddress(NX_MIPI_GetClockNumber(me->vip_module_num),
            (void *)IO_ADDRESS(NX_CLKGEN_GetPhysicalAddress(NX_MIPI_GetClockNumber(me->vip_module_num))));
    NX_CLKGEN_SetClockDivisorEnable(NX_MIPI_GetClockNumber(me->vip_module_num), CTRUE);

    nxp_soc_peri_reset_enter(RESET_ID_MIPI);
    nxp_soc_peri_reset_enter(RESET_ID_MIPI_CSI);
    nxp_soc_peri_reset_enter(RESET_ID_MIPI_PHY_S);
    nxp_soc_peri_reset_exit(RESET_ID_MIPI);
    nxp_soc_peri_reset_exit(RESET_ID_MIPI_CSI);

    NX_MIPI_OpenModule(me->vip_module_num);
#if defined(CONFIG_VIDEO_TW9992)
	#if defined(CONFIG_ARCH_S5P6818)
		{
		    volatile NX_MIPI_RegisterSet* pmipi;
		    pmipi = (volatile NX_MIPI_RegisterSet*)IO_ADDRESS(NX_MIPI_GetPhysicalAddress(me->vip_module_num));
		    pmipi->CSIS_DPHYCTRL= (5 <<24);
		}
	#endif
#endif
    NX_MIPI_SetInterruptEnableAll(me->vip_module_num, CFALSE);
    NX_MIPI_ClearInterruptPendingAll(me->vip_module_num);

    NX_CLKGEN_SetClockDivisorEnable(NX_MIPI_GetClockNumber(me->vip_module_num), CFALSE);
    /* TODO : use clk_get(), clk_get_rate() for dynamic clk binding */
#if defined(CONFIG_VIDEO_TW9992)
    NX_CLKGEN_SetClockSource(NX_MIPI_GetClockNumber(me->vip_module_num), 0, ClkSrc); // use pll2 -> current 295MHz
    NX_CLKGEN_SetClockDivisor(NX_MIPI_GetClockNumber(me->vip_module_num), 0, Divisor);
#else
    NX_CLKGEN_SetClockSource(NX_MIPI_GetClockNumber(me->vip_module_num), 0, 2); // use pll2 -> current 295MHz
    NX_CLKGEN_SetClockDivisor(NX_MIPI_GetClockNumber(me->vip_module_num), 0, 2);
#endif
    /* NX_CLKGEN_SetClockDivisor(NX_MIPI_GetClockNumber(me->vip_module_num), 0, 6); */
    NX_CLKGEN_SetClockDivisorEnable(NX_MIPI_GetClockNumber(me->vip_module_num), CTRUE);

    NX_MIPI_CSI_SetParallelDataAlignment32(me->vip_module_num, 1, CFALSE);
    NX_MIPI_CSI_SetYUV422Layout(me->vip_module_num, 1, NX_MIPI_CSI_YUV422LAYOUT_FULL);
    NX_MIPI_CSI_SetFormat(me->vip_module_num, 1, NX_MIPI_CSI_FORMAT_YUV422_8);
    NX_MIPI_CSI_EnableDecompress(me->vip_module_num, CFALSE);
    NX_MIPI_CSI_SetInterleaveMode(me->vip_module_num, NX_MIPI_CSI_INTERLEAVE_VC);
    NX_MIPI_CSI_SetTimingControl(me->vip_module_num, 1, 32, 16, 368);
    NX_MIPI_CSI_SetInterleaveChannel(me->vip_module_num, 0, 1);
    NX_MIPI_CSI_SetInterleaveChannel(me->vip_module_num, 1, 0);
/*
    vmsg("%s: width %d, height %d, lane:%d(%d,%d,%d,%d)\n", __func__, 
				me->format.width, me->format.height, 
				pdata->lanes, EnableDataLane0, EnableDataLane1, EnableDataLane2, EnableDataLane3);
*/
    NX_MIPI_CSI_SetSize(me->vip_module_num, 1, me->h_active, me->v_active);
    NX_MIPI_CSI_SetVCLK(me->vip_module_num, 1, NX_MIPI_CSI_VCLKSRC_EXTCLK);
    /* HACK!!! -> this is variation : confirm to camera sensor */
    NX_MIPI_CSI_SetPhy(me->vip_module_num,
#if defined(CONFIG_VIDEO_TW9992)
			NumberOfDataLanes,  // U32   NumberOfDataLanes (0~3)
            1,  					// CBOOL EnableClockLane
            EnableDataLane0,  	// CBOOL EnableDataLane0
            EnableDataLane1,  	// CBOOL EnableDataLane1
            EnableDataLane2,  	// CBOOL EnableDataLane2
            EnableDataLane3,  	// CBOOL EnableDataLane3
            0,  					// CBOOL SwapClockLane
            0   					// CBOOL SwapDataLane
#else
            1,  // U32   NumberOfDataLanes (0~3)
            1,  // CBOOL EnableClockLane
            1,  // CBOOL EnableDataLane0
            1,  // CBOOL EnableDataLane1
            0,  // CBOOL EnableDataLane2
            0,  // CBOOL EnableDataLane3
            0,  // CBOOL SwapClockLane
            0   // CBOOL SwapDataLane
#endif
            );
    NX_MIPI_CSI_SetEnable(me->vip_module_num, CTRUE);

    nxp_soc_peri_reset_exit(RESET_ID_MIPI_PHY_S);

    /* NX_MIPI_DSI_SetPLL( me->vip_module_num, */
    /*                     CTRUE,       // CBOOL Enable      , */
    /*                     0xFFFFFFFF,  // U32 PLLStableTimer, */
    /*                     0x33E8,      // 19'h033E8: 1Ghz  19'h043E8: 750Mhz // Use LN28LPP_MipiDphyCore1p5Gbps_Supplement. */
    /*                     0xF,         // 4'hF: 1Ghz  4'hC: 750Mhz           // Use LN28LPP_MipiDphyCore1p5Gbps_Supplement. */
    /*                     0,           // U32 M_PLLCTL      , */
    /*                                  // Refer to 10.2.2 M_PLLCTL of MIPI_D_PHY_USER_GUIDE.pdf  Default value is all "0". */
    /*                                  // If you want to change register values, it need to confirm from IP Design Team */
    /*                     0            // U32 B_DPHYCTL */
    /*                                  // Refer to 10.2.3 M_PLLCTL of MIPI_D_PHY_USER_GUIDE.pdf */
    /*                                  // or NX_MIPI_PHY_B_DPHYCTL enum or LN28LPP_MipiDphyCore1p5Gbps_Supplement. */
    /*                                  // default value is all "0". */
    /*                                  // If you want to change register values, it need to confirm from IP Design Team */
    /*                    ); */
    NX_MIPI_DSI_SetPLL( me->vip_module_num,
                        CTRUE,       // CBOOL Enable      ,
                        0xFFFFFFFF,  // U32 PLLStableTimer,
#if defined(CONFIG_VIDEO_TW9992)
	#if defined(CONFIG_ARCH_S5P4418)
                        0x43E8,      // 19'h033E8: 1Ghz  19'h043E8: 750Mhz // Use LN28LPP_MipiDphyCore1p5Gbps_Supplement.
                        0xC,         // 4'hF: 1Ghz  4'hC: 750Mhz           // Use LN28LPP_MipiDphyCore1p5Gbps_Supplement.
	#elif defined(CONFIG_ARCH_S5P6818)
                        0x33E8,      // 19'h033E8: 1Ghz  19'h043E8: 750Mhz // Use LN28LPP_MipiDphyCore1p5Gbps_Supplement.
                        0xF,         // 4'hF: 1Ghz  4'hC: 750Mhz           // Use LN28LPP_MipiDphyCore1p5Gbps_Supplement.
	#endif
#else
                        0x43E8,      // 19'h033E8: 1Ghz  19'h043E8: 750Mhz // Use LN28LPP_MipiDphyCore1p5Gbps_Supplement.
                        0xC,         // 4'hF: 1Ghz  4'hC: 750Mhz           // Use LN28LPP_MipiDphyCore1p5Gbps_Supplement.
#endif
                        0,           // U32 M_PLLCTL      ,
                                     // Refer to 10.2.2 M_PLLCTL of MIPI_D_PHY_USER_GUIDE.pdf  Default value is all "0".
                                     // If you want to change register values, it need to confirm from IP Design Team
                        0            // U32 B_DPHYCTL
                                     // Refer to 10.2.3 M_PLLCTL of MIPI_D_PHY_USER_GUIDE.pdf
                                     // or NX_MIPI_PHY_B_DPHYCTL enum or LN28LPP_MipiDphyCore1p5Gbps_Supplement.
                                     // default value is all "0".
                                     // If you want to change register values, it need to confirm from IP Design Team
                       );

    NX_MIPI_SetInterruptEnable(me->vip_module_num, NX_MIPI_INT_CSI_FrameEnd_CH1, CTRUE);
}

static void _work_handler_backgear(struct work_struct *work)
{
    struct nxp_backward_camera_context *me = &_context;

    mutex_lock(&me->decide_work_lock);
    _decide(&_context);
    mutex_unlock(&me->decide_work_lock);
}

static void _set_display_irq_callback(struct nxp_backward_camera_context *me)
{
	int dpc_module_num = me->plat_data->mlc_module_num;
	me->callback = (void *)nxp_soc_disp_register_irq_callback(dpc_module_num, _display_callback, me);
}

static void _release_display_irq_callback(struct nxp_backward_camera_context *me)
{
    unsigned long flags;
    int dpc_module_num;

    spin_lock_irqsave(&me->deinit_lock, flags);
	dpc_module_num = me->plat_data->mlc_module_num;

    if (me->callback) { 
        nxp_soc_disp_unregister_irq_callback(dpc_module_num, (struct disp_irq_callback *)me->callback);
        me->callback = NULL;
    }

    spin_unlock_irqrestore(&me->deinit_lock, flags);
}

static int _enqueue_all_buffer_to_empty_queue(struct nxp_backward_camera_context *me)
{
	int i=0;

    struct queue_entry *entry;
    struct nxp_video_frame_buf *buf;

	me->q_vip_empty.init(&me->q_vip_empty);
	me->q_vip_done.init(&me->q_vip_done);
	me->q_deinter_empty.init(&me->q_deinter_empty);
	me->q_deinter_done.init(&me->q_deinter_done);

	for(i=0; i<MAX_BUFFER_COUNT; i++) {
		entry = &me->frame_set.vip_queue_entry[i]; 
		buf = &me->frame_set.vip_bufs[i];
		debug_msg("%s: vip lu 0x%x, cb 0x%x, cr 0x%x\n",
				__func__,
				buf->lu_addr, buf->cb_addr, buf->cr_addr);

		entry->data = buf;
		me->q_vip_empty.enqueue(&me->q_vip_empty, entry);
	}

	for(i=0; i<MAX_BUFFER_COUNT; i++) {
		entry = &me->frame_set.deinter_queue_entry[i]; 
		buf = &me->frame_set.deinter_bufs[i];
    	debug_msg("%s: deinter lu 0x%x, cb 0x%x, cr 0x%x\n",
				__func__,
				buf->lu_addr, buf->cb_addr, buf->cr_addr);

		entry->data = buf;
		me->q_deinter_empty.enqueue(&me->q_deinter_empty, entry);
	}

    return 0;
}


static int _video_allocation_memory(struct nxp_backward_camera_context *me, struct nxp_video_frame_buf *buf)
{
	struct ion_device *ion_dev = get_global_ion_device();
    int size=0;
	struct ion_buffer *ion_buffer;

	if( !me->ion_client )
	{
	  me->ion_client = ion_client_create(ion_dev, "backward-camera");
	  if (IS_ERR(me->ion_client)) {
		  pr_err("%s: failed to ion_client_create()\n", __func__);
		  return -EINVAL;
	  }
	}

    me->plat_data->lu_stride = _stride_cal(me, Y);
    me->plat_data->cb_stride = _stride_cal(me, CB);
    me->plat_data->cr_stride = _stride_cal(me, CR);

	size = me->plat_data->lu_stride * me->plat_data->v_active
		+ me->plat_data->cb_stride * (me->plat_data->v_active / 2)
		+ me->plat_data->cr_stride * (me->plat_data->v_active / 2);
	size = PAGE_ALIGN(size);

	buf->ion_handle = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
	if (IS_ERR(buf->ion_handle)) {
		pr_err("%s: failed to ion_alloc() for video, size %d\n", __func__, size);
		return -ENOMEM;
	}

	buf->dma_buf = ion_share_dma_buf(me->ion_client, buf->ion_handle);
	if (IS_ERR_OR_NULL(buf->dma_buf)) {
		pr_err("%s: failed to ion_share_dma_buf() for video\n", __func__);
		return -EINVAL;
	}

	ion_buffer = buf->dma_buf->priv;

	buf->lu_addr = ion_buffer->priv_phys;
	buf->cb_addr = buf->lu_addr + me->plat_data->lu_addr + me->plat_data->lu_stride * me->plat_data->v_active;
	buf->cr_addr = buf->cb_addr + me->plat_data->cb_addr + me->plat_data->cb_stride * (me->plat_data->v_active / 2);
	buf->lu_stride = me->plat_data->lu_stride;
	buf->cb_stride = me->plat_data->cb_stride;
	buf->cr_stride = me->plat_data->cr_stride;
	buf->lu_size = buf->lu_stride * me->plat_data->v_active;
	buf->cb_size = buf->cb_stride * (me->plat_data->v_active / 2);
	buf->cr_size = buf->cr_stride * (me->plat_data->v_active / 2);

	buf->virt_video = cma_get_virt(buf->lu_addr, size, 1);

    debug_msg("%s - [addr] lu 0x%x, cb 0x%x, cr 0x%x, virt %p\n",
            __func__,
            buf->lu_addr,
            buf->cb_addr,
            buf->cr_addr,
            buf->virt_video);

    debug_msg("%s: [stride] lu %d, cb %d, cr %d\n", __func__,
            buf->lu_stride,
            buf->cb_stride,
            buf->cr_stride);

    debug_msg("%s - [size] lu %d, cb %d, cr %d\n",
            __func__,
            buf->lu_size,
            buf->cb_size,
            buf->cr_size);

	return 0;
}

static int _video_allocation_deinter_dst_memory(struct nxp_backward_camera_context *me, struct nxp_video_frame_buf *buf )
{
	struct ion_device *ion_dev = get_global_ion_device();
    int size=0;
	struct ion_buffer *ion_buffer;
    int lu_stride = 0;
    int cb_stride = 0;
    int cr_stride = 0;
    int w=0;

	if( !me->ion_client )
	{
	  me->ion_client = ion_client_create(ion_dev, "backward-camera");
	  if (IS_ERR(me->ion_client)) {
		  pr_err("%s: failed to ion_client_create()\n", __func__);
		  return -EINVAL;
	  }
	}

    w = me->plat_data->h_active;

    lu_stride = (ALIGN(w/2, 256) * 2);
    cb_stride = ALIGN(w/2, 256);
    cr_stride = ALIGN(w/2, 256);

	size = lu_stride * me->plat_data->v_active
		+ cb_stride * (me->plat_data->v_active / 2)
		+ cr_stride * (me->plat_data->v_active / 2);
	size = PAGE_ALIGN(size);

	buf->ion_handle = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
	if (IS_ERR(buf->ion_handle)) {
		pr_err("%s: failed to ion_alloc() for video, size %d\n", __func__, size);
		return -ENOMEM;
	}

	buf->dma_buf = ion_share_dma_buf(me->ion_client, buf->ion_handle);
	if (IS_ERR_OR_NULL(buf->dma_buf)) {
		pr_err("%s: failed to ion_share_dma_buf() for video\n", __func__);
		return -EINVAL;
	}

	ion_buffer = buf->dma_buf->priv;

	buf->lu_addr = ion_buffer->priv_phys;
	buf->cb_addr = buf->lu_addr + me->plat_data->lu_addr + lu_stride * me->plat_data->v_active;
	buf->cr_addr = buf->cb_addr + me->plat_data->cb_addr + cb_stride * (me->plat_data->v_active / 2);
	buf->lu_stride = lu_stride;
	buf->cb_stride = cb_stride;
	buf->cr_stride = cr_stride;
	buf->lu_size = buf->lu_stride * me->plat_data->v_active;
	buf->cb_size = buf->cb_stride * (me->plat_data->v_active / 2);
	buf->cr_size = buf->cr_stride * (me->plat_data->v_active / 2);

	buf->virt_video = cma_get_virt(buf->lu_addr, size, 1);

#if 0
		printk(KERN_ERR "%s - [addr] lu 0x%x, cb 0x%x, cr 0x%x, virt %p\n",
				__func__,
				buf->lu_addr,
				buf->cb_addr,
				buf->cr_addr,
				buf->virt_video);

		printk(KERN_ERR "%s - [size] lu %d, cb %d, cr %d\n",
				__func__,
				buf->lu_size,
				buf->cb_size,
                buf->cr_size);
#endif

	return 0;
}

static int _rgb_allocation_memory(struct nxp_backward_camera_context *me)
{
	struct ion_device *ion_dev = get_global_ion_device();
    struct nxp_rgb_frame_buf *buf = NULL;
	u32 format = me->plat_data->rgb_format;
	u32 pixelbyte = _get_pixel_byte(format);
    int size=0;
	struct ion_buffer *ion_buffer;

	if( !me->ion_client )
	{
	  me->ion_client = ion_client_create(ion_dev, "backward-camera");
	  if (IS_ERR(me->ion_client)) {
		  pr_err("%s: failed to ion_client_create()\n", __func__);
		  return -EINVAL;
	  }
	}

	size = me->plat_data->width * me->plat_data->height * pixelbyte;
	size = PAGE_ALIGN(size);

	buf = &me->frame_set.rgb_buf;

	buf->ion_handle_rgb = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
	if (IS_ERR(buf->ion_handle_rgb)) {
		 pr_err("%s: failed to ion_alloc() for rgb, size %d\n", __func__, size);
		 return -ENOMEM;
	}
	buf->dma_buf_rgb = ion_share_dma_buf(me->ion_client, buf->ion_handle_rgb);
	if (IS_ERR_OR_NULL(buf->dma_buf_rgb)) {
		pr_err("%s: failed to ion_share_dma_buf() for rgb\n", __func__);
		return -EINVAL;
	}
	ion_buffer = buf->dma_buf_rgb->priv;
	me->plat_data->rgb_addr = buf->rgb_addr = ion_buffer->priv_phys;
	me->virt_rgb = buf->virt_rgb = cma_get_virt(buf->rgb_addr, size, 1);

#if 0
	printk(KERN_ERR "%s - [addr] rgb 0x%x, virt %p\n",
			__func__,
			buf->rgb_addr,
			buf->virt_rgb);

    printk(KERN_ERR "%s - [size] rgb %d\n", __func__, size);
#endif
	return 0;
}

static void _reset_queue(struct nxp_backward_camera_context *me)
{
    me->q_deinter_done.clear(&me->q_deinter_done);
    me->q_vip_empty.clear(&me->q_vip_empty);
    me->q_deinter_empty.clear(&me->q_deinter_empty);
    me->q_vip_done.clear(&me->q_vip_done);
}

static void _reset_values(struct nxp_backward_camera_context *me)
{
	atomic_set(&me->status, PROCESSING_STOP);	

    if (me->plat_data->is_odd_first)
        me->frame_set.cur_mode = FIELD_ODD;
    else
        me->frame_set.cur_mode = FIELD_EVEN;

	me->deinter_count = 0;

	me->frame_set.cur_entry_vip = NULL;
	me->frame_set.cur_entry_deinter = NULL;
	me->frame_set.cur_entry_display = NULL;
   
    me->is_display_on = false;
    me->is_mlc_on = false;

	atomic_set(&me->status, DISPLAY_STOP);	
}

static void _init_context(struct nxp_backward_camera_context *me)
{
	init_waitqueue_head(&me->wq_start);
	init_waitqueue_head(&me->wq_deinter_end);
	
	spin_lock_init(&me->deinit_lock);
	spin_lock_init(&me->vip_lock);

    mutex_init(&me->deinter_lock);
    mutex_init(&me->decide_work_lock);

    me->is_first = true;
    me->mlc_on_first = false;
    me->is_detected = false;
    me->removed = false;
}

static void _init_queue_context(struct nxp_backward_camera_context *me)
{
	/* vip empty queue */
	register_queue_func(
							&me->q_vip_empty, 
							_init_queue, 
							_enqueue, 
							_dequeue, 
							_peekqueue, 
                            _clearqueue,
							_lockqueue, 
							_unlockqueue, 
							_sizequeue
					);

	/* vip done queue */
	register_queue_func(
							&me->q_vip_done, 
							_init_queue, 
							_enqueue, 
							_dequeue, 
							_peekqueue, 
                            _clearqueue,
							_lockqueue, 
							_unlockqueue, 
							_sizequeue
					);

	/* deinterlace empty queue */
	register_queue_func(
							&me->q_deinter_empty, 
							_init_queue, 
							_enqueue, 
							_dequeue, 
							_peekqueue, 
                            _clearqueue,
							_lockqueue, 
							_unlockqueue, 
							_sizequeue
					);

	/* deinterlace done queue */
	register_queue_func(
							&me->q_deinter_done, 
							_init_queue, 
							_enqueue, 
							_dequeue, 
							_peekqueue, 
                            _clearqueue,
							_lockqueue, 
							_unlockqueue, 
							_sizequeue
					);

	me->q_vip_empty.init(&me->q_vip_empty);
	me->q_vip_done.init(&me->q_vip_done);
	me->q_deinter_empty.init(&me->q_deinter_empty);
	me->q_deinter_done.init(&me->q_deinter_done);
}

static int _init_buffer(struct nxp_backward_camera_context *me)
{
	int i=0;

	me->frame_set.width 	= me->plat_data->h_active;
	me->frame_set.height 	= me->plat_data->v_active;

	for (i=0; i<MAX_BUFFER_COUNT; i++) {
		struct nxp_video_frame_buf *buf = &me->frame_set.vip_bufs[i];
		struct queue_entry *entry = &me->frame_set.vip_queue_entry[i]; 

		_video_allocation_memory(me, buf); 

		entry->data = buf;
	}

	for (i=0; i<MAX_BUFFER_COUNT; i++) {
		struct nxp_video_frame_buf *buf = &me->frame_set.deinter_bufs[i];
		struct queue_entry *entry = &me->frame_set.deinter_queue_entry[i]; 

        _video_allocation_deinter_dst_memory(me, buf);
		entry->data = buf;
	}

	_rgb_allocation_memory(me);

	return 0;
}

static int _init_worker(struct nxp_backward_camera_context *me)
{
#if defined(CONFIG_ARCH_S5P6818)
    INIT_WORK(&me->work_deinter, _deinter_worker);

	me->wq_deinter = create_singlethread_workqueue("wq_deinter");	
	if( !me->wq_deinter )
	{
		printk(KERN_ERR "create workquque error.\n");
		return -1;	
	}
#endif
    return 0;
}

static void _cancel_worker(struct nxp_backward_camera_context *me)
{
    cancel_work_sync(&me->work_deinter);
    flush_workqueue(me->wq_deinter);
}

static void _init_hw_mlc(struct nxp_backward_camera_context *me)
{
	_mlc_video_set_param(me->plat_data->mlc_module_num, me->plat_data);
}

static int _stride_cal(struct nxp_backward_camera_context *me, enum FRAME_KIND type)
{
    int value=0;  
    int stride=0;
    
    int width = me->plat_data->h_active;

    switch (type) {
    case Y:
        value = me->plat_data->lu_stride;
        //stride = (value > 0) ? value : YUV_YSTRIDE(width); 
        stride = (value > 0) ? value : YUV_STRIDE(width); 
        break;
    case CB:
        value = me->plat_data->cb_stride;
        stride = (value > 0) ? value : YUV_STRIDE(width/2); 
        break;
    case CR:
        value = me->plat_data->cr_stride;
        stride = (value > 0) ? value : YUV_STRIDE(width/2); 
        break;
    }

    return stride;
}

static int _allocate_memory(struct nxp_backward_camera_context *me)
{
    struct ion_device *ion_dev = get_global_ion_device();

    if (me->plat_data->lu_addr && me->plat_data->rgb_addr)
        return 0;

    me->ion_client = ion_client_create(ion_dev, "backward-camera");
    if (IS_ERR(me->ion_client)) {
        pr_err("%s: failed to ion_client_create()\n", __func__);
        return -EINVAL;
    }

    me->plat_data->lu_stride = _stride_cal(me, Y);
    me->plat_data->cb_stride = _stride_cal(me, CB);
    me->plat_data->cr_stride = _stride_cal(me, CR);

    if (!me->plat_data->lu_addr) {
        int size = me->plat_data->lu_stride * me->plat_data->v_active
            + me->plat_data->cb_stride * (me->plat_data->v_active / 2)
            + me->plat_data->cr_stride * (me->plat_data->v_active / 2);
        struct ion_buffer *ion_buffer;
        size = PAGE_ALIGN(size);

        me->ion_handle_video = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
        if (IS_ERR(me->ion_handle_video)) {
             pr_err("%s: failed to ion_alloc() for video, size %d\n", __func__, size);
             return -ENOMEM;
        }
        me->dma_buf_video = ion_share_dma_buf(me->ion_client, me->ion_handle_video);
        if (IS_ERR_OR_NULL(me->dma_buf_video)) {
            pr_err("%s: failed to ion_share_dma_buf() for video\n", __func__);
            return -EINVAL;
        }
        ion_buffer = me->dma_buf_video->priv;
        me->plat_data->lu_addr = ion_buffer->priv_phys;
        me->plat_data->cb_addr = me->plat_data->lu_addr + me->plat_data->lu_stride * me->plat_data->v_active;
        me->plat_data->cr_addr = me->plat_data->cb_addr + me->plat_data->cb_stride * (me->plat_data->v_active / 2);
        me->virt_video = cma_get_virt(me->plat_data->lu_addr, size, 1);

#if 1
        printk(KERN_ERR "%s: lu 0x%x, cb 0x%x, cr 0x%x, virt %p\n",
                __func__,
                me->plat_data->lu_addr,
                me->plat_data->cb_addr,
                me->plat_data->cr_addr,
                me->virt_video);

        printk(KERN_ERR "%s: stride lu %d, cb %d, cr %d\n", __func__,
                me->plat_data->lu_stride,
                me->plat_data->cb_stride,
                me->plat_data->cr_stride);
#endif
    }

    if (!me->plat_data->rgb_addr) {
        u32 format = me->plat_data->rgb_format;
        u32 pixelbyte = _get_pixel_byte(format);
        int size = me->plat_data->width * me->plat_data->height * pixelbyte;
        struct ion_buffer *ion_buffer;
        size = PAGE_ALIGN(size);

        me->ion_handle_rgb = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
        if (IS_ERR(me->ion_handle_rgb)) {
             pr_err("%s: failed to ion_alloc() for rgb, size %d\n", __func__, size);
             return -ENOMEM;
        }
        me->dma_buf_rgb = ion_share_dma_buf(me->ion_client, me->ion_handle_rgb);
        if (IS_ERR_OR_NULL(me->dma_buf_rgb)) {
            pr_err("%s: failed to ion_share_dma_buf() for rgb\n", __func__);
            return -EINVAL;
        }
        ion_buffer = me->dma_buf_rgb->priv;
        me->plat_data->rgb_addr = ion_buffer->priv_phys;
        me->virt_rgb = cma_get_virt(me->plat_data->rgb_addr, size, 1);

#if 0
        printk(KERN_ERR "%s: rgb 0x%x, virt %p\n",
                __func__,
                me->plat_data->rgb_addr,
                me->virt_rgb);
#endif
    }

    return 0;
}

static void _free_buffer(struct nxp_backward_camera_context *me)
{
    if (me->plat_data->use_deinterlacer) {
        int i=0;

        for (i=0; i<MAX_BUFFER_COUNT; i++) {
            struct nxp_video_frame_buf *buf = &me->frame_set.vip_bufs[i];

            if (buf) {
                if (buf->dma_buf != NULL) {
                    dma_buf_put(buf->dma_buf);
                    buf->dma_buf = NULL;
                    ion_free(me->ion_client, buf->ion_handle);
                    buf->ion_handle = NULL;
                }
            }
        }

        for (i=0; i<MAX_BUFFER_COUNT; i++) {
            struct nxp_video_frame_buf *buf = &me->frame_set.deinter_bufs[i];

            if (buf) {
                if (buf->dma_buf != NULL) {
                    dma_buf_put(buf->dma_buf);
                    buf->dma_buf = NULL;
                    ion_free(me->ion_client, buf->ion_handle);
                    buf->ion_handle = NULL;
                }

            }
        }

        if (me->frame_set.rgb_buf.dma_buf_rgb != NULL) {
            dma_buf_put(me->frame_set.rgb_buf.dma_buf_rgb);
            me->frame_set.rgb_buf.dma_buf_rgb = NULL;
            ion_free(me->ion_client, me->frame_set.rgb_buf.ion_handle_rgb);
            me->frame_set.rgb_buf.ion_handle_rgb = NULL;
        }
    } else {
        if (me->dma_buf_video != NULL) {
            dma_buf_put(me->dma_buf_video);
            me->dma_buf_video = NULL;
            ion_free(me->ion_client, me->ion_handle_video);
            me->ion_handle_video = NULL;
        }

        if (me->dma_buf_rgb != NULL) {
            dma_buf_put(me->dma_buf_rgb);
            me->dma_buf_rgb = NULL;
            ion_free(me->ion_client, me->ion_handle_rgb);
            me->ion_handle_rgb = NULL;
        }
    }
}


#ifdef CONFIG_PM
#define RESUME_CAMERA_ON_DELAY_MS   300
static void _resume_work(struct work_struct *work)
{
    struct nxp_backward_camera_context *me = &_context;
    int vip_module_num = me->plat_data->vip_module_num;
    int mlc_module_num = me->plat_data->mlc_module_num;
    u32 lu_addr = me->plat_data->lu_addr;
    u32 cb_addr = me->plat_data->cb_addr;
    u32 cr_addr = me->plat_data->cr_addr;
    u32 lu_stride = me->plat_data->lu_stride;
    u32 cb_stride = me->plat_data->cb_stride;
    u32 cr_stride = me->plat_data->cr_stride;

    if (!me->plat_data->use_deinterlacer) {
        _camera_sensor_run(me);
        _vip_hw_set_clock(vip_module_num, me->plat_data, true);
        _vip_hw_set_sensor_param(vip_module_num, me->plat_data);
        _vip_hw_set_addr(vip_module_num, me->plat_data, lu_addr, cb_addr, cr_addr);

        _mlc_video_set_param(mlc_module_num, me->plat_data);
        _mlc_video_set_addr(mlc_module_num, lu_addr, cb_addr, cr_addr, lu_stride, cb_stride, cr_stride);

        _mlc_rgb_overlay_set_param(mlc_module_num, me->plat_data);
        _mlc_rgb_overlay_draw(mlc_module_num, me->plat_data, me->virt_rgb);

        _decide(me);
    }
}

static int nxp_backward_camera_suspend(struct device *dev)
{
    struct nxp_backward_camera_context *me = &_context;
    PM_DBGOUT("+%s\n", __func__);
    me->running = false;
    PM_DBGOUT("-%s\n", __func__);
    return 0;
}

static int nxp_backward_camera_resume(struct device *dev)
{
    struct nxp_backward_camera_context *me = &_context;
    PM_DBGOUT("+%s\n", __func__);
    if (!me->client)
        _get_i2c_client(me);

    queue_delayed_work(system_nrt_wq, &me->resume_work, msecs_to_jiffies(RESUME_CAMERA_ON_DELAY_MS));

    PM_DBGOUT("-%s\n", __func__);
    return 0;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops nxp_backward_camera_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(nxp_backward_camera_suspend, nxp_backward_camera_resume)
};
#define NXP_BACKWARD_CAMERA_PMOPS       (&nxp_backward_camera_pm_ops)
#else
#define NXP_BACKWARD_CAMERA_PMOPS       NULL
#endif

bool is_backward_camera_on(void);
void backward_camera_remove(void);
int get_backward_module_num(void);

static ssize_t _stop_backward_camera(struct device *pdev, 
        struct device_attribute *attr, const char *buf, size_t n)
{
    struct nxp_backward_camera_context *me = &_context;
    int module = me->plat_data->vip_module_num;

    printk(KERN_ERR "%s : module : %d\n", __func__, module);

    if (module == get_backward_module_num()) {
        while (is_backward_camera_on()) {
            printk("wait backward camera stopping...\n");
            schedule_timeout_interruptible(HZ/5);
        }
        backward_camera_remove();
#if 0
        if (get_backward_module_num() == 0)
            register_backward_irq_tw9992();
#if defined(CONFIG_VIDEO_TW9900)
        else
            register_backward_irq_tw9900();
#endif
#endif

        printk(KERN_ERR "%s - end of backward_camera_remove()\n", __func__);
    }

    me->is_remove = true;

    return n;
}

static ssize_t _status_backward_camera(struct device *pdev,
	struct device_attribute *attr, const char *buf)
{
	int ret = 0;
	struct nxp_backward_camera_context *me = &_context;

	if (me->is_remove) ret = 1;
	else ret = 0;

	sprintf(buf, "%d", ret);
}

static struct device_attribute backward_camera_attr = __ATTR(stop, 0664,
				_status_backward_camera, _stop_backward_camera);

static struct attribute *attrs[] = {
    &backward_camera_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = (struct attribute **)attrs,
};

static int _create_sysfs(void)
{
    struct kobject *kobj = NULL;
    int ret = 0;
    
    kobj = kobject_create_and_add("backward_camera", &platform_bus.kobj);
    if (!kobj) {
        printk(KERN_ERR "Fail, create kobject for backward camera\n");
        return -ret;
    }

    ret = sysfs_create_group(kobj, &attr_group);
    if (ret) {
        printk(KERN_ERR "Fail, create sysfs group for backward camera\n");
        kobject_del(kobj);
        return -ret;
    }

    return 0;
}

static int nxp_backward_camera_probe(struct platform_device *pdev)
{
    int ret;
    struct nxp_backward_camera_platform_data *pdata = pdev->dev.platform_data;
    struct nxp_backward_camera_context *me = &_context;

    me->plat_data = pdata;
    me->irq = IRQ_GPIO_START + pdata->backgear_gpio_num;
	me->vip_irq = _hw_get_irq_num(me);

	NX_MLC_SetBaseAddress(pdata->mlc_module_num, (void *)IO_ADDRESS(NX_MLC_GetPhysicalAddress(pdata->mlc_module_num)));
	NX_VIP_SetBaseAddress(pdata->vip_module_num, (void *)IO_ADDRESS(NX_VIP_GetPhysicalAddress(pdata->vip_module_num)));

    _init_context(me);

    if (pdata->use_deinterlacer) {
        _init_queue_context(me);
        _init_buffer(me);

        _init_hw_mlc(me);

#if defined(CONFIG_ARCH_S5P6818)
        _init_hw_deinter(me);
#endif
        // TODO
        _init_hw_vip(me);
        _init_worker(me); 
    }

    _get_i2c_client(me);
    _camera_sensor_run(me);
	if(me->plat_data->is_mipi)
        nxp_mipi_csi_setting(me->plat_data, 1);

    if(!pdata->use_deinterlacer) {
        _allocate_memory(me);
	    _vip_run(me->plat_data->vip_module_num);
    } else {
        _reset_values(me);
        _enqueue_all_buffer_to_empty_queue(me);
	    _vip_run(me->plat_data->vip_module_num);
    }

    INIT_DELAYED_WORK(&me->work, _work_handler_backgear);
    
    ret = request_irq(me->irq, _irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "backward-camera", me);
    if (ret) {
        pr_err("%s: failed to request_irq (irqnum %d)\n", __func__, me->irq);
        return -1;
    }

    me->is_first = true;
	me->removed = false;
	me->is_remove = false;
    me->my_device = pdev;

    if (_create_sysfs()) {
        printk(KERN_ERR "failed to create sysfs for backward camera\n");
        return -1;
    }

#ifdef CONFIG_PM
    INIT_DELAYED_WORK(&me->resume_work, _resume_work);
#endif

    if (_is_backgear_on(me->plat_data))
        schedule_delayed_work(&me->work, msecs_to_jiffies(100));

    if (me->plat_data->alloc_vendor_context) {
        if (!me->plat_data->alloc_vendor_context(me->plat_data->vendor_context)) {
            printk(KERN_ERR "failed to platform alloc_vendor_context() function for backward camera\n");
            return -1;
        }
    }

    return 0;
}

static int nxp_backward_camera_remove(struct platform_device *pdev)
{
	struct nxp_backward_camera_context *me = &_context;
    
	if( me->removed == false ) {
		printk(KERN_ERR "%s\n", __func__);

		_mlc_overlay_stop(me->plat_data->mlc_module_num);
		_mlc_video_stop(me->plat_data->mlc_module_num);

        if (me->plat_data->use_deinterlacer) {
            _set_vip_interrupt(me, false);
            _vip_stop(me->plat_data->vip_module_num);

            _cleanup_deinter(me);

            _release_display_irq_callback(me);
            _release_irq_deinter();
            _release_irq_vip();
            _reset_queue(me);

            me->is_display_on = false;
            me->mlc_on_first = false;
        } else {
		   _vip_stop(me->plat_data->vip_module_num);	
        }
		_free_buffer(me);
		free_irq(_context.irq, &_context);

        // TODO : To this routine : may not arise event for irq restart term
		me->removed = true;
        
        if (me->plat_data->free_vendor_context) 
            me->plat_data->free_vendor_context(me->plat_data->vendor_context);
	}
	return 0;
}

static struct platform_driver backward_camera_driver = {
    .probe  = nxp_backward_camera_probe,
    .remove = nxp_backward_camera_remove,
    .driver = {
        .name  = "nxp-backward-camera",
        .owner = THIS_MODULE,
        .pm    = NXP_BACKWARD_CAMERA_PMOPS,
    },
};

bool is_backward_camera_on(void)
{
    struct nxp_backward_camera_context *me = &_context;
#if 0
    return _is_backgear_on(me);
#else
	return me->is_on;
#endif
}

void backward_camera_remove(void)
{
    struct nxp_backward_camera_context *me = &_context;
    nxp_backward_camera_remove(me->my_device);
}

int get_backward_module_num(void)
{
    struct nxp_backward_camera_context *me = &_context;
    return me->plat_data->vip_module_num;
}

EXPORT_SYMBOL(is_backward_camera_on);
EXPORT_SYMBOL(backward_camera_remove);
EXPORT_SYMBOL(get_backward_module_num);

static int __init backward_camera_init(void)
{
    return platform_driver_register(&backward_camera_driver);
}

subsys_initcall(backward_camera_init);

MODULE_AUTHOR("swpark <swpark@nexell.co.kr>");
MODULE_DESCRIPTION("Backward Camera Driver for Nexell");
MODULE_LICENSE("GPL");
