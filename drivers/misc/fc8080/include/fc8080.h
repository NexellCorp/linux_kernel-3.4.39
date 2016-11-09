#ifndef __FC8080_H__
#define __FC8080_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/list.h>

#include "fci_types.h"
#include "fci_ringbuffer.h"

#define CTL_TYPE  0
#define FIC_TYPE  1
#define MSC_TYPE  2

#define MAX_OPEN_NUM 		8

#define IOCTL_MAGIC	't'

struct ioctl_info {
	u32 buff[4];
};

#define IOCTL_MAXNR			45

#define IOCTL_DMB_RESET				_IO(IOCTL_MAGIC, 0)
#define IOCTL_DMB_PROBE				_IO(IOCTL_MAGIC, 1)
#define IOCTL_DMB_INIT		 		_IO(IOCTL_MAGIC, 2)
#define IOCTL_DMB_DEINIT	 		_IO(IOCTL_MAGIC, 3)

#define IOCTL_DMB_BYTE_READ 		_IOWR(IOCTL_MAGIC, 4, struct ioctl_info)
#define IOCTL_DMB_WORD_READ 		_IOWR(IOCTL_MAGIC, 5, struct ioctl_info)
#define IOCTL_DMB_LONG_READ 		_IOWR(IOCTL_MAGIC, 6, struct ioctl_info)
#define IOCTL_DMB_BULK_READ 		_IOWR(IOCTL_MAGIC, 7, struct ioctl_info)

#define IOCTL_DMB_BYTE_WRITE 		_IOW(IOCTL_MAGIC, 8, struct ioctl_info)
#define IOCTL_DMB_WORD_WRITE 		_IOW(IOCTL_MAGIC, 9, struct ioctl_info)
#define IOCTL_DMB_LONG_WRITE 		_IOW(IOCTL_MAGIC, 10, struct ioctl_info)
#define IOCTL_DMB_BULK_WRITE 		_IOW(IOCTL_MAGIC, 11, struct ioctl_info)

#define IOCTL_DMB_TUNER_SELECT	 	_IOW(IOCTL_MAGIC, 12, struct ioctl_info)

#define IOCTL_DMB_TUNER_READ		_IOWR(IOCTL_MAGIC, 13, struct ioctl_info)
#define IOCTL_DMB_TUNER_WRITE	 	_IOW(IOCTL_MAGIC, 14, struct ioctl_info)

#define IOCTL_DMB_TUNER_SET_FREQ 	_IOW(IOCTL_MAGIC, 15, struct ioctl_info)
#define IOCTL_DMB_SCAN_STATUS		_IO(IOCTL_MAGIC, 16)

#define IOCTL_DMB_TYPE_SET			_IOW(IOCTL_MAGIC, 17, struct ioctl_info)

#define IOCTL_DMB_CHANNEL_SELECT	_IOW(IOCTL_MAGIC, 18, struct ioctl_info)
#define IOCTL_DMB_FIC_SELECT		_IO(IOCTL_MAGIC, 19)
#define IOCTL_DMB_VIDEO_SELECT		_IOW(IOCTL_MAGIC, 20, struct ioctl_info)
#define IOCTL_DMB_AUDIO_SELECT		_IOW(IOCTL_MAGIC, 21, struct ioctl_info)
#define IOCTL_DMB_DATA_SELECT		_IOW(IOCTL_MAGIC, 22, struct ioctl_info)

#define IOCTL_DMB_CHANNEL_DESELECT	_IOW(IOCTL_MAGIC, 23, struct ioctl_info)
#define IOCTL_DMB_FIC_DESELECT 		_IO(IOCTL_MAGIC, 24)
#define IOCTL_DMB_VIDEO_DESELECT	_IO(IOCTL_MAGIC, 25)
#define IOCTL_DMB_AUDIO_DESELECT	_IO(IOCTL_MAGIC, 26)
#define IOCTL_DMB_DATA_DESELECT		_IO(IOCTL_MAGIC, 27)

#define IOCTL_DMB_TUNER_GET_RSSI 	_IOR(IOCTL_MAGIC, 28, struct ioctl_info)

#define IOCTL_DMB_POWER_ON			_IO(IOCTL_MAGIC, 29)
#define IOCTL_DMB_POWER_OFF			_IO(IOCTL_MAGIC, 30)

#define IOCTL_DMB_GET_BER		_IOR(IOCTL_MAGIC, 31, struct ioctl_info)

struct DMB_OPEN_INFO_T {
	HANDLE				*hInit;
	struct list_head		hList;
	struct fci_ringbuffer		RingBuffer;
	u8				*buf;
	u8				subChId;
	u8				dmbtype;
};

struct DMB_INIT_INFO_T {
	struct list_head		hHead;
};

//#define TAURUS

#ifdef __cplusplus
}
#endif

#endif
