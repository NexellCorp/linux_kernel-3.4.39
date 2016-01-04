#ifndef _NXP_DEINTERLACER_H
#define _NXP_DEINTERLACER_H

#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/mutex.h>

typedef struct {
	atomic_t open_count;
	atomic_t	status;

	wait_queue_head_t	wq_start;
	wait_queue_head_t	wq_end;

	int irq;

    struct mutex mutex;
} nxp_deinterlace;

enum {
	PROCESSING_STOP,
	PROCESSING_START
};

#endif
