/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080.c

	Description : API source file of dmb baseband module

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	History :
	----------------------------------------------------------------------
*******************************************************************************/

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/irq.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "platform.h"
#include "fc8080.h"
#include "bbm.h"
#include "fci_oal.h"
#include "fc8080_regs.h"
#include "fic.h"
#ifdef CONFIG_ARCH_EXYNOS4
#include <linux/gpio.h>
#include <mach/gpio-exynos4.h>
#endif

//#include <mach/platform.h>
//#include <mach/devices.h>
//#include <mach/soc.h>
//


#define FC8080_NAME		"dmb"

#define RING_BUFFER_SIZE	(128 * 1024)





static s32 power_pin, reset_pin;
static u32 irq_num;
struct DMB_INIT_INFO_T *hInit;
u8 fc8080_tx_data[32];

static DEFINE_MUTEX(ring_buffer_lock);

static DECLARE_WAIT_QUEUE_HEAD(dmb_isr_wait);

static u8 dmb_isr_sig;
static struct task_struct *dmb_kthread;

static int dmb_open(struct inode *inode, struct file *filp);
static int dmb_release(struct inode *inode, struct file *filp);
static ssize_t dmb_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static long dmb_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static unsigned int dmb_poll(struct file *file, poll_table *wait);

extern void fc8080_isr(HANDLE hDevice);

static irqreturn_t dmb_irq(int irq, void *dev_id)
{

//	print_log(hInit, "*"); 
	dmb_isr_sig = 1;
	wake_up_interruptible(&dmb_isr_wait);

	return IRQ_HANDLED;
}

int fic_fci_callback(u32 hDevice, u8 *data, int len)
{
	struct DMB_INIT_INFO_T *hInit;
	struct list_head *temp;

	hInit = (struct DMB_INIT_INFO_T *)hDevice;

	list_for_each(temp, &(hInit->hHead))
	{
		struct DMB_OPEN_INFO_T *hOpen;

		hOpen = list_entry(temp, struct DMB_OPEN_INFO_T, hList);

		if (hOpen->dmbtype == FIC_TYPE)	{
			
			mutex_lock(&ring_buffer_lock);

			if(fci_ringbuffer_free(&hOpen->RingBuffer) < len + 8){ // FIC buffer is full

				FCI_RINGBUFFER_SKIP(&hOpen->RingBuffer, len + 8);
				print_log(hInit, "f_f\n"); 

				mutex_unlock(&ring_buffer_lock);
				wake_up_interruptible(&(hOpen->RingBuffer.queue));				
				return 0;		

            }else{
                print_log(hInit, "n_f\n");

            }

		print_log(0, "FIC 0x%x, 0x%x, 0x%x, 0x%x\n", data[0], data[1], data[2], data[3]);
			FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len >> 8);
			FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len & 0xff);
			fci_ringbuffer_write(&hOpen->RingBuffer, data, len);
            
			mutex_unlock(&ring_buffer_lock);
			
			wake_up_interruptible(&(hOpen->RingBuffer.queue));
		}

	}

	return 0;
}

int msc_fci_callback(u32 hDevice, u8 subChId, u8 *data, int len)
{
	struct DMB_INIT_INFO_T *hInit;
	struct list_head *temp;


	hInit = (struct DMB_INIT_INFO_T *)hDevice;

	list_for_each(temp, &(hInit->hHead))
	{
		struct DMB_OPEN_INFO_T *hOpen;

		hOpen = list_entry(temp, struct DMB_OPEN_INFO_T, hList);

		if ((hOpen->dmbtype == MSC_TYPE) && (hOpen->subChId == subChId)) {
			
 			mutex_lock(&ring_buffer_lock);
			
 			if (fci_ringbuffer_free(&hOpen->RingBuffer) < len + 8){ // MSC buffer is full

				FCI_RINGBUFFER_SKIP(&hOpen->RingBuffer, len + 8);
				print_log(hInit, "m_f\n");
				
				mutex_unlock(&ring_buffer_lock);
				wake_up_interruptible(&(hOpen->RingBuffer.queue));				
				return 0;					
				
             }
      //if(data[0] != 0x47)
		//    print_log(0, "0x%x, 0x%x, 0x%x, 0x%x\n", data[0], data[1], data[2], data[3]);
		
        
        FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len >> 8);
			FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len & 0xff);
			fci_ringbuffer_write(&hOpen->RingBuffer, data, len);

			mutex_unlock(&ring_buffer_lock);
			
			wake_up_interruptible(&(hOpen->RingBuffer.queue));
		}
	}

	return 0;
}

static int dmb_thread(void *hDevice)
{
	static DEFINE_MUTEX(thread_lock);
	struct DMB_INIT_INFO_T *hInit = (struct DMB_INIT_INFO_T *)hDevice;

	set_user_nice(current, -20);

	print_log(hInit, "dmb_kthread enter\n");

	bbm_com_fic_callback_register((u32)hInit, fic_fci_callback);
	bbm_com_msc_callback_register((u32)hInit, msc_fci_callback);


	while (1) {
		wait_event_interruptible
			(dmb_isr_wait, dmb_isr_sig || kthread_should_stop());

                fc8080_isr(hInit);
		dmb_isr_sig = 0;

		if (kthread_should_stop())
			break;

	}

	print_log(hInit, "dmb_kthread exit\n");

	return 0;
}

static const struct file_operations dmb_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= dmb_ioctl,
	.open		= dmb_open,
	.read		= dmb_read,
	.release	= dmb_release,
	.poll		= dmb_poll,
};

static struct miscdevice fc8080_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = FC8080_NAME,
    .fops = &dmb_fops,
};

static int dmb_open(struct inode *inode, struct file *filp)
{
	struct DMB_OPEN_INFO_T *hOpen;

	hOpen = kmalloc(sizeof(struct DMB_OPEN_INFO_T), GFP_KERNEL);

	hOpen->buf = kmalloc(RING_BUFFER_SIZE, GFP_KERNEL);
	hOpen->dmbtype = 0;

	list_add(&(hOpen->hList), &(hInit->hHead));

	hOpen->hInit = (HANDLE *)hInit;

	if (hOpen->buf == NULL)	{
		print_log(hInit, "ring buffer malloc error\n");
		return -ENOMEM;
	}

	fci_ringbuffer_init(&hOpen->RingBuffer, hOpen->buf, RING_BUFFER_SIZE);

	filp->private_data = hOpen;

	return 0;
}

static ssize_t dmb_read
	(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	s32 avail;
	s32 non_blocking = filp->f_flags & O_NONBLOCK;
	struct DMB_OPEN_INFO_T *pDMB
		= (struct DMB_OPEN_INFO_T *)filp->private_data;
	struct fci_ringbuffer *cibuf = &pDMB->RingBuffer;
	ssize_t len = 0;;

   	//print_log(hInit, "dmb read\n");


    if (!cibuf->data || !count)	{
		print_log(hInit, "return 0\n");
		return 0;
	}

    if (non_blocking && (fci_ringbuffer_empty(cibuf)))	{
		/*print_log(hInit, "return EWOULDBLOCK\n");*/ 
		return -EWOULDBLOCK;
	}

	if (wait_event_interruptible(cibuf->queue,
		!fci_ringbuffer_empty(cibuf))) {
		print_log(hInit, "return ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	mutex_lock(&ring_buffer_lock);

    avail = fci_ringbuffer_avail(cibuf);

    if (avail < 4) {
		print_log(hInit, "return 00\n");
		mutex_unlock(&ring_buffer_lock);
		return 0;
	}

    len = FCI_RINGBUFFER_PEEK(cibuf, 0) << 8;
    len |= FCI_RINGBUFFER_PEEK(cibuf, 1);

    if (avail < len + 2 || count < len)	{
		print_log(hInit, "return EINVAL\n");
		mutex_unlock(&ring_buffer_lock);
		return -EINVAL;
	}
    FCI_RINGBUFFER_SKIP(cibuf, 2);

	avail = fci_ringbuffer_read_user(cibuf, buf, len);

	mutex_unlock(&ring_buffer_lock);

	return avail;
}

static unsigned int dmb_poll(struct file *file, poll_table *wait)
{
    struct DMB_OPEN_INFO_T *pDMB = (struct DMB_OPEN_INFO_T *)file->private_data;
    struct fci_ringbuffer *cibuf = &pDMB->RingBuffer;
    u32 mask = 0;

    poll_wait(file, &cibuf->queue, wait);

    if (!fci_ringbuffer_empty(cibuf))
		mask |= (POLLIN | POLLRDNORM);

    return mask;
}

static int dmb_release(struct inode *inode, struct file *filp)
{
	struct DMB_OPEN_INFO_T *hOpen = filp->private_data;

	hOpen->dmbtype = 0;

	list_del(&(hOpen->hList));

	kfree(hOpen->buf);
	kfree(hOpen);

	return 0;
}

static long dmb_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	s32 res = BBM_NOK;
	s32 err = 0;
	s32 size = 0;
	struct DMB_OPEN_INFO_T *hOpen;

	struct ioctl_info info;


	if (_IOC_TYPE(cmd) != IOCTL_MAGIC)
		return -EINVAL;
	if (_IOC_NR(cmd) >= IOCTL_MAXNR)
		return -EINVAL;

	hOpen = filp->private_data;

	size = _IOC_SIZE(cmd);

	switch (cmd) {
	case IOCTL_DMB_RESET:
		res = bbm_com_reset(hInit);
		break;
	case IOCTL_DMB_INIT:
                /* once created, and use it; workaround in a condition of the case  */
                /* that PULL_UP for irq became void */
		res = bbm_com_init(hInit);
		break;
	case IOCTL_DMB_DEINIT:
	{
			u8 data[64]={0};
			u16 len = 64;

			res = bbm_com_deinit(hInit);

			if(fci_ringbuffer_free(&hOpen->RingBuffer) < len+8 )
			  {
				print_log(NULL,"dmb buffer deinitialization !");
			  }

			FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len >> 8);
			FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len & 0xff);
			fci_ringbuffer_write(&hOpen->RingBuffer, data, len);

			wake_up_interruptible(&(hOpen->RingBuffer.queue));

			break;
	}
	case IOCTL_DMB_BYTE_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_byte_read
			(hInit, (u16)info.buff[0]
			, (u8 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_DMB_WORD_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_word_read
			(hInit, (u16)info.buff[0]
			, (u16 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_DMB_LONG_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_long_read
			(hInit, (u16)info.buff[0]
			, (u32 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_DMB_BULK_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_bulk_read
			(hInit, (u16)info.buff[0]
			, (u8 *)(&info.buff[2]), info.buff[1]);
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_DMB_BYTE_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_byte_write
			(hInit, (u16)info.buff[0], (u8)info.buff[1]);
		break;
	case IOCTL_DMB_WORD_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_word_write
			(hInit, (u16)info.buff[0], (u16)info.buff[1]);
		break;
	case IOCTL_DMB_LONG_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_long_write
			(hInit, (u16)info.buff[0], (u32)info.buff[1]);
		break;
	case IOCTL_DMB_BULK_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_bulk_write
			(hInit, (u16)info.buff[0]
			, (u8 *)(&info.buff[2]), info.buff[1]);
		break;
	case IOCTL_DMB_TUNER_SELECT:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_select(hInit, (u32)info.buff[0], 0);
		break;
	case IOCTL_DMB_TUNER_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_read
			(hInit, (u8)info.buff[0], (u8)info.buff[1]
			, (u8 *)(&info.buff[3]), (u8)info.buff[2]);
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_DMB_TUNER_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_write
			(hInit, (u8)info.buff[0], (u8)info.buff[1]
			, (u8 *)(&info.buff[3]), (u8)info.buff[2]);
		break;
	case IOCTL_DMB_TUNER_SET_FREQ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_set_freq(hInit, (u32)info.buff[0]);
		break;
	case IOCTL_DMB_SCAN_STATUS:
		res = bbm_com_scan_status(hInit);
		break;
	case IOCTL_DMB_CHANNEL_SELECT:
		err = copy_from_user((void *)&info, (void *)arg, size);
		fci_ringbuffer_flush(&hOpen->RingBuffer);
		hOpen->dmbtype = (u8)info.buff[0];
		hOpen->subChId = (u8)info.buff[1];
		res = bbm_com_channel_select
			(hInit, (u8)info.buff[1], (u8)info.buff[2]);
		break;
	case IOCTL_DMB_TYPE_SET:
		err = copy_from_user((void *)&info, (void *)arg, size);
		fci_ringbuffer_flush(&hOpen->RingBuffer);
		hOpen->dmbtype = (u8)info.buff[0];
		hOpen->subChId = 0;
		res = BBM_OK;
		break;
	case IOCTL_DMB_FIC_SELECT:
		bbm_com_word_write(hInit, BBM_BUF_ENABLE, 0x0100);		
		fci_ringbuffer_flush(&hOpen->RingBuffer);
		hOpen->dmbtype = FIC_TYPE;
		res = BBM_OK;
		break;
	case IOCTL_DMB_VIDEO_SELECT:
		err = copy_from_user((void *)&info, (void *)arg, size);
		fci_ringbuffer_flush(&hOpen->RingBuffer);
		hOpen->dmbtype = MSC_TYPE;
		hOpen->subChId = (u8)info.buff[0];
		res = bbm_com_video_select(hInit, (u8)info.buff[0], 0, 0);
		break;
	case IOCTL_DMB_AUDIO_SELECT:
		err = copy_from_user((void *)&info, (void *)arg, size);
		fci_ringbuffer_flush(&hOpen->RingBuffer);
		hOpen->dmbtype = MSC_TYPE;
		hOpen->subChId = (u8)info.buff[0];
		res = bbm_com_audio_select(hInit, (u8)info.buff[0], 0);
		break;
	case IOCTL_DMB_DATA_SELECT:
		err = copy_from_user((void *)&info, (void *)arg, size);
		fci_ringbuffer_flush(&hOpen->RingBuffer);
		hOpen->dmbtype = MSC_TYPE;
		hOpen->subChId = (u8)info.buff[0];
		res = bbm_com_data_select(hInit, (u8)info.buff[0], 1);
		break;
	case IOCTL_DMB_CHANNEL_DESELECT:
		err = copy_from_user((void *)&info, (void *)arg, size);
		wake_up_interruptible(&(hOpen->RingBuffer.queue));
		hOpen->dmbtype = 0;
		hOpen->subChId = 0;
		res = bbm_com_channel_deselect
			(hInit, 0, (u8)info.buff[0]);
		break;
	case IOCTL_DMB_FIC_DESELECT:
		wake_up_interruptible(&(hOpen->RingBuffer.queue));
		hOpen->dmbtype = 0;
		bbm_com_word_write(hInit, BBM_BUF_ENABLE, 0x0000);
		res = BBM_OK;
		break;
	case IOCTL_DMB_VIDEO_DESELECT:
		wake_up_interruptible(&(hOpen->RingBuffer.queue));
		hOpen->dmbtype = 0;
		hOpen->subChId = 0;
		res = bbm_com_video_deselect(hInit, 0, 0, 0);
		break;
	case IOCTL_DMB_AUDIO_DESELECT:
		wake_up_interruptible(&(hOpen->RingBuffer.queue));
		hOpen->dmbtype = 0;
		hOpen->subChId = 0;
		res = bbm_com_audio_deselect(hInit, 0, 0);
		break;
	case IOCTL_DMB_DATA_DESELECT:
		wake_up_interruptible(&(hOpen->RingBuffer.queue));
		hOpen->dmbtype = 0;
		hOpen->subChId = 0;
		res = bbm_com_data_deselect(hInit, 0, 1);
		break;
	case IOCTL_DMB_TUNER_GET_RSSI:
          if (dmb_isr_sig == 1) {
            print_log(hInit,"IOCTL_DMB_TUNER_GET_RSSI\n");
            res = BBM_NOK;			
          } else {		
		res = bbm_com_tuner_get_rssi
			(hInit, (u32 *)(&info.buff[0]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
          }
		break;
	case IOCTL_DMB_POWER_ON:
		//platform_hw_init(power_pin, reset_pin);
		res = BBM_OK;
		break;
	case IOCTL_DMB_POWER_OFF:
		//platform_hw_deinit(power_pin);
		res = BBM_OK;
		break;
	case IOCTL_DMB_GET_BER:
          if (dmb_isr_sig == 1) {
            /* print_log(hInit,"IOCTL_DMB_GET_BER conflict\n"); */
            res = BBM_NOK;			
          } else {		
            res = bbm_com_ber_overrun_read((HANDLE)hInit, (u32 *)(&info.buff[0]));
            if (info.buff[1] & 0x1)
              print_log(hInit,"IOCTL_DMB_GET_BER(%d), OVERRUN(%d)\n", info.buff[0], info.buff[1]);

            err |= copy_to_user((void *)arg, (void *)&info, size);
          }
          break;

	default:
		print_log(hInit, "dmb ioctl error! cmd(0x%x, 0x%x)\n", cmd, IOCTL_DMB_WORD_READ);
		res = BBM_NOK;
		break;
	}

	if (err < 0) {
		print_log(hInit, "copy to/from user fail : %d", err);
		res = BBM_NOK;
	}

	return res;
}

int dmb_init(void)
{
	s32 res;
        u32 irq;

	power_pin =2;// EXYNOS4212_GPV2(7);
	reset_pin = -EIO;
	irq_num = 1;// EXYNOS4_GPX1(1);

	print_log(hInit, "dmb_driver_version : v1.8.1\n");
	print_log(hInit, "dmb_init - power_pin : 0x%x	reset_pin : 0x%x \
		irq_num : 0x%x	XTAL : %d\n"
		, power_pin, reset_pin, irq_num, FC8080_FREQ_XTAL);



	print_log(0, "dmb_driver_version : v1.8.1\n");
	print_log(0, "dmb_init - power_pin : 0x%x	reset_pin : 0x%x \
		irq_num : 0x%x	XTAL : %d\n"
		, power_pin, reset_pin, irq_num, FC8080_FREQ_XTAL);




    
	hInit = kzalloc(sizeof(struct DMB_INIT_INFO_T), GFP_KERNEL);

	res = misc_register(&fc8080_misc_device);

	if (res < 0) {
          print_log(hInit, "dmb init fail : %d\n", res);
          kzfree(hInit);
          return res;
	}

        INIT_LIST_HEAD(&(hInit->hHead));

	platform_hw_setting();
	platform_hw_init(power_pin, reset_pin);

	res = bbm_com_hostif_select(hInit, BBM_SPI);
	if (res) {
          print_log(hInit, "dmb host interface select fail!\n");
          kzfree(hInit);
          goto error;
        }








/*------------------------------------------------------------------------------
 * 	Description	: set gpio interrupt mode
 *	In[io]		: gpio pad number, 32*n + bit
 * 				: (n= GPIO_A:0, GPIO_B:1, GPIO_C:2, GPIO_D:3, GPIO_E:4, ALIVE:5, bit= 0 ~ 32)
 *	In[mode]	: gpio interrupt detect mode
 *				: 0 = Low level detect
 *				: 1 = High level detect
 *				: 2 = Falling edge detect
 *				: 3 = Rising edge detect
 *				: alive interrupt detect mode
 *				: 0 = async low level detect mode
 *				: 1 = async high level detect mode
 *				: 2 = sync falling edge detect mode
 *				: 3 = sync rising edge detect mode
 *				: 4 = sync low level detect mode
 *				: 5 = sync high level detect mode
 *	Return 		: none.
 */
    nxp_soc_gpio_set_int_mode(CFG_GPIO_DMB_INT, 2);




    //irq = PB_PIO_IRQ(PAD_GPIO_D + 23 ); //gpio_to_irq(irq_num);
	//irq = gpio_to_irq();
    //irq = IRQ_GPIO_D_START + 23; 
    nxp_soc_gpio_set_int_enable(CFG_GPIO_DMB_INT, 0);

//#define CFG_GPIO_DMB_INT                        (PAD_GPIO_D + 23)

    //irq_set_irq_type(gpio_to_irq(CFG_GPIO_DMB_INT), IRQF_TRIGGER_FALLING);
	//res = request_irq(gpio_to_irq(CFG_GPIO_DMB_INT),  dmb_irq, 0, FC8080_NAME, NULL);

	//res = request_irq(gpio_to_irq(CFG_GPIO_DMB_INT),  dmb_irq, IRQF_TRIGGER_FALLING , FC8080_NAME, NULL);

	res = request_irq(PB_PIO_IRQ(CFG_GPIO_DMB_INT),  dmb_irq, IRQF_TRIGGER_FALLING , FC8080_NAME, NULL);

	if (res) {
          print_log(hInit, "dmb request irq fail : %d\n", res);

          if (hInit)
            kzfree(hInit);
          goto error;

    }
    else
    {
//  		nxp_soc_gpio_set_io_pullup(CFG_GPIO_DMB_INT, true);
//		nxp_soc_gpio_set_int_mode(CFG_GPIO_DMB_INT, 4);
		nxp_soc_gpio_set_int_enable(CFG_GPIO_DMB_INT, 1);
		nxp_soc_gpio_clr_int_pend(CFG_GPIO_DMB_INT);
	
    }

	/* power has to be kept in order to make sure PULL_UP for dmb IRQ */
        /* no deinit */

	if (!dmb_kthread)
          dmb_kthread = kthread_run(dmb_thread, (void *)hInit, "dmb_thread");

	return 0;
 error:
        return -1;
}

void dmb_exit(void)
{
	print_log(hInit, "dmb exit \n");

	free_irq(irq_num, NULL);

	kthread_stop(dmb_kthread);
	dmb_kthread = NULL;

	bbm_com_hostif_deselect(hInit);

	platform_hw_deinit(power_pin);

	misc_deregister(&fc8080_misc_device);

	kfree(hInit);
}

late_initcall(dmb_init);
module_exit(dmb_exit);

MODULE_LICENSE("Dual BSD/GPL");
