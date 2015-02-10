/*
 * linux/drivers/input/keyboard/tcc-keys.c
 *
 * Based on:     	drivers/input/keyboard/bf54x-keys.c
 * Author: <linux@telechips.com>
 * Created: June 10, 2008
 * Description: Keypad ADC driver on Telechips TCC Series
 *
 * Copyright (C) 2008-2009 Telechips 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
	1. Quick Boot mode no operation Issue
	   - 2014.02.26 : CONFIG_HAS_EARLYSUSPEND 설정에서 Suspend / Resume이 동작이 되어야 하나 동작하지 않게 구현된 부분 수정 
*/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/input-polldev.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_SMARTA_BATTERY
#include <linux/power_supply.h>
#endif

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/scatterlist.h>
#include <asm/mach-types.h>

//ischoi#include <mach/hardware.h>
#include <mach/irqs.h>
//ischoi#include <mach/bsp.h>
//ischoi#include <mach/tca_ckc.h>
#include <mach/gpio.h>
//ischoi#include <mach/tcc_adc.h>

//#include <mach/TCC88xx_Structures.h>
//#include <mach/reg_physical.h>
//ischoi#include <linux/Fine_gpio_map.h>

#include <linux/wakelock.h>

/* eroum:dhkim:20140904 - start - update-gui */
/* eroum:dhkim:20140918 - start : RENEW UPDATE */
//ischoi#include "../../../bootable/bootloader/lk/lib/dev_dna_info/dev_dna_info.h"
//ischoi#include "../../../bootable/bootloader/lk/platform/tcc_shared/include/lcd/update_image.h"
/* eroum:dhkim:20140918 - end : RENEW UPDATE */
#include <linux/syscalls.h>
#include <linux/io.h>
//ischoi#include <mach/structures_smu_pmu.h>
//ischoi#include <mach/reg_physical.h>

/* eroum:dhkim:20141010 - start - bootcount-renewal */
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

/* eroum:dhkim:20141023 - start - error-thread */
#include <linux/kthread.h>
/* eroum:dhkim:20141023 - end - error-thread */

/* eroum:dhkim:20141103 - start
   indi-mmc */
#if defined(CONFIG_FINE_MMC_INDI)
#include <linux/delay.h>
#endif /* CONFIG_FINE_MMC_INDI */
/* eroum:dhkim:20141103 - end
   indi-mmc */

//#define AUTOUPDATE_RENEWAL
//#define SHOW_ERROR_CODE
/* eroum:dhkim:20141010 - end - bootcount-renewal */

#define NUMBER(n) reset_0##n

#define BUFF_SIZE 4

#define AUTO_RECOVERY_MODE   3
/* eroum:dhkim:20140904 - end - update-gui */

/* For id.version */
#define TCCKEYVERSION        0x0001
#define DRV_NAME             "tcc-keypad"


//#define BUTTON_DELAY    ((20 * HZ) / 1000) // 20ms
#define BUTTON_DELAY   msecs_to_jiffies(20) 

#define KEY_RELEASED    0
#define KEY_PRESSED     1


//#define REBOOT_TEST		//reboot test


#define TRACE_UART 0 //salary block - prevent tearing when AV in - !!!Do not enable this define!!!
#define TRACE_BATT 0


#define BOTTOM_THREAD_TYPE 			//salary - process MCU UART packet in bottom thread when it has received packet
#define REMOCON_PACKET_TIMEOUT		//salary - apply timeout control for Remocon UART packet
#define REMOCON_POWER_LONG_OFF


#define HwINT1_UART 		Hw15		// R/W, UART interrupt enable

#if TRACE_UART
#define PRINT_UART(x...) printk(KERN_INFO "[UART] " x)
#else
#define PRINT_UART(x...) do {} while (0)
#endif


#define SMARTA_MCUCPU_DEV_NAME		"smarta_mcucpu_drv"
static int MajorNum;

#define MCU_UART_PORT 6
#define MCU_UART_BAUD 38400 // 115200

//IOCTLs
#define IOCTL_MCU_FORCE_REPAIR			0x10
#define IOCTL_MCU_FORCE_REPAIR_OK		0x20
#define IOCTL_MCU_POWER_DOWN			0x30
#define IOCTL_MCU_BOOT_SUCCESS			0x40
#define IOCTL_MCU_VER_CHECK				0x50
#define IOCTL_MCU_MESSAGE_ACK			0x60
#define IOCTL_MCU_MESSAGE_RECALL		0x70
#define IOCTL_MCU_CPU_RESET				0x80
#define IOCTL_POWER_SHORT_KEY_CHECK	0x90
#define IOCTL_FIRMWARE_UPDATE			0x100
#define IOCTL_MCU_AUTO_RECOVERY		0x200
//hh20140602
#define IOCTL_MCU_ALIVE_ACK_CHANGE		0x110

//Finedigital:hh:20140710 MCU Watchdog Disable
#define IOCTL_MCU_ALIVE_DISABLE		0x120

/* Finedigital:hh:20140924 - start
   Booting Fail Check */	
#define IOCTL_MCU_BOOTWAIT_CHANGE		0x130
#define IOCTL_MCU_BOOTWAIT_GETCOUNT		0x131
/* end */

#if 1 //Eroum:hh.shin:140821 - Recovery debug
#define IOCTL_MCU_PRINTK_FUNCTION		0x11
#endif  //Eroum:hh.shin:140821 - Recovery debug
/* eroum:dhkim:20140904 - start - update-gui */
#define IOCTL_MCU_UPDATE_DRAW           0x12
#define IOCTL_MCU_SET_USSTATUS          0x13
#define IOCTL_MCU_DETECT_SDCARD         0x14
#define IOCTL_MCU_UPDATE_BOOTCOUNT      0x15
#define IOCTL_MCU_AUDIO_SHUTDOWN        0x16
#define IOCTL_MCU_CURR_PERCENTAGE       0x17
#define IOCTL_MCU_PRINTF                0x18
#define IOCTL_MCU_SET_PERCENTAGE        0x19
/* eroum:dhkim:20140904 - end - update-gui */


#define MAKE_HEADER(s, g) (s|(((g+s)%3)<<2)|(g<<4))

#define GROUP_REMOCON		0x0a		//b1010
#define GROUP_BUTTON		0x05		//b0101
#define GROUP_GPIO			0x0c		//b1100
#define GROUP_BATTERY		0x03		//b0011
//hh:20140530 / iQ7000MCU
#define GROUP_ALIVE_CHECK		0x09		//b1001
//hh20140602
#define GROUP_ALIVE_RESET		0x06
#define GROUP_ALIVE_CHANGE		0x0b

//Finedigital:hh:20140626 
#define MCU_ACC					0x08


#define GROUP_DC_IN			0x0f		//b1111
//#define GROUP_CPU			0x06		//b0110
#define GROUP_CPU			0x03		//b0110


#define CPU_HEADER MAKE_HEADER(3, GROUP_CPU)

#define KEY_RELEASED    0
#define KEY_PRESSED     1

/* Finedigital:hh:20140924 - start
   Booting Fail Check */	
#define COMPLETEBOOTING  1
#if COMPLETEBOOTING
int nBootCnt=0;
int nBootFlg=1;
#endif
/* end */
/* Finedigital:hh:20141013 - start
   MCU Ver Check */	
#define MCUVERSION  1
#if MCUVERSION
static int nMCU_Ver=0;
#endif

// jhhong : 2014.02.26 (QB)
#if defined(CONFIG_HIBERNATION)
extern unsigned int do_hibernate_boot;
#endif

bool				g_bAliveAckFlag;

#if defined(REMOCON_PACKET_TIMEOUT) //salary - apply timeout control for Remocon UART packet
#define REMOCON_PACKET_TIMEOUT_MS		500

static struct timer_list mcucpu_uart_packet_timer;
int mcucpu_uart_packet_timer_is_expired = 1;
#endif //salary - apply timeout control for Remocon UART packet

#if defined(BOTTOM_THREAD_TYPE) //salary - process MCU UART packet in bottom thread when it has received packet
static struct work_struct mcucpu_uart_packet_work_q;

#define MCU_CPU_PACKET_BUFFER_SIZE		100

unsigned char mcu_cpu_packet_buffer[MCU_CPU_PACKET_BUFFER_SIZE] = {0x0,};

static int mcu_cpu_packet_buffer_read_index = 0;
static int mcu_cpu_packet_buffer_write_index = 0;


#ifdef REMOCON_POWER_LONG_OFF
static int rep_cnt = 0;
static int pwr_press = 0;
#endif

static int mPercentage = 0;

void mcu_cpu_put_packet(unsigned char packet)
{
	int temp_index;

	temp_index = mcu_cpu_packet_buffer_write_index +1;
	if(temp_index == MCU_CPU_PACKET_BUFFER_SIZE) {
		temp_index = 0;
	}
	if(temp_index == mcu_cpu_packet_buffer_read_index) {
		return;
	}

	mcu_cpu_packet_buffer[mcu_cpu_packet_buffer_write_index] = packet;
	mcu_cpu_packet_buffer_write_index++;
	
	if(mcu_cpu_packet_buffer_write_index == MCU_CPU_PACKET_BUFFER_SIZE) {
		mcu_cpu_packet_buffer_write_index = 0;
	}
}
int mcu_cpu_get_packet(void)
{
	int ret = 0;
	
	if(mcu_cpu_packet_buffer_read_index == mcu_cpu_packet_buffer_write_index) {
		return -1;
	}
	ret = mcu_cpu_packet_buffer[mcu_cpu_packet_buffer_read_index];
	mcu_cpu_packet_buffer_read_index++;

	if(mcu_cpu_packet_buffer_read_index == MCU_CPU_PACKET_BUFFER_SIZE) {
		mcu_cpu_packet_buffer_read_index = 0;
	}
	
	return ret;
}
#endif //BOTTOM_THREAD_TYPE //salary - process MCU UART packet in bottom thread when it has received packet



struct smarta_private
{
	struct input_dev *input_dev;
	struct input_polled_dev *poll_dev;
	struct platform_device *pdev;
	int key_pressed;
	int old_key;
	short status;
};

static struct input_dev *gInputDev = NULL;
static int gKeyRepeat = 0;
static int gLastKeyCode = -1;
static struct timer_list gKeyCheckTimer;
#define REPEAT_DELAY 150
#define REPEAT_PERIOD 110

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend early_suspend;
#endif

typedef enum _eRX_STATUS
{
	eRXHeader = 0,
	eRXData,
}eRX_STATUS;

typedef struct _UARTPacket {
	union
	{
		char group;
		char header;
	};
	char data[3];
} UARTPacket;

static UARTPacket gRxPacketTable[] = 
{
	{{GROUP_REMOCON}, {0xa5, 0xff-0xa5, 0, }},	//eHWR_REPEAT
	{{GROUP_REMOCON}, {0x00, 0xff-0x00, 0, }},	//eHWR_POWER
	{{GROUP_REMOCON}, {0x80, 0xff-0x80, 0, }},	//eHWR_SR
	{{GROUP_REMOCON}, {0x98, 0xff-0x98, 0, }},	//eHWR_UP
	{{GROUP_REMOCON}, {0x58, 0xff-0x58, 0, }},	//eHWR_PREV
	{{GROUP_REMOCON}, {0x68, 0xff-0x68, 0, }},	//eHWR_SEL
	{{GROUP_REMOCON}, {0xe8, 0xff-0xe8, 0, }},	//eHWR_CANCEL
	{{GROUP_REMOCON}, {0x38, 0xff-0x38, 0, }},	//eHWR_DOWN
	{{GROUP_REMOCON}, {0xf0, 0xff-0xf0, 0, }},	//eHWR_VOLINC
	{{GROUP_REMOCON}, {0x70, 0xff-0x70, 0, }},	//eHWR_VOLDEC
	{{GROUP_REMOCON}, {0xb0, 0xff-0xb0, 0, }},	//eHWR_MYHOUSE
	{{GROUP_REMOCON}, {0xa8, 0xff-0xa8, 0, }},	//eHWR_MUTE
	{{GROUP_REMOCON}, {0xd0, 0xff-0xd0, 0, }},	//eHWR_CURPOS
	{{GROUP_REMOCON}, {0x08, 0xff-0x08, 0, }},	//eHWR_CHAINC
	{{GROUP_REMOCON}, {0x88, 0xff-0x88, 0, }},	//eHWR_NAVI
	{{GROUP_REMOCON}, {0xc8, 0xff-0xc8, 0, }},	//eHWR_CHADEC
	{{GROUP_REMOCON}, {0xb8, 0xff-0xb8, 0, }},	//eHWR_DMB
	{{GROUP_REMOCON}, {0x02, 0xff-0x02, 0, }},	//eHWR_OILINFO
	{{GROUP_REMOCON}, {0x42, 0xff-0x42, 0, }},	//eHWR_MUSIC
	{{GROUP_REMOCON}, {0x82, 0xff-0x82, 0, }},	//eHWR_COMPA
	{{GROUP_REMOCON}, {0xc2, 0xff-0xc2, 0, }},	//eHWR_RESEARCH
	{{GROUP_REMOCON}, {0x22, 0xff-0x22, 0, }},	//eHWR_WAYCANCEL
	{{GROUP_REMOCON}, {0x92, 0xff-0x92, 0, }},	//eHWR_UP_CH
	{{GROUP_REMOCON}, {0x32, 0xff-0x32, 0, }},	//eHWR_DOWN_CH
	{{GROUP_REMOCON}, {0xe2, 0xff-0xe2, 0, }},	//eHWR_PLUS_VOL
	{{GROUP_REMOCON}, {0x52, 0xff-0x52, 0, }},	//eHWR_MINUS_VOL
	{{GROUP_REMOCON}, {0xf2, 0xff-0xf2, 0, }},	//eHWR_DST
	{{GROUP_REMOCON}, {0x72, 0xff-0x72, 0, }},	//eHWR_MYMENU

	{{GROUP_BUTTON}, {0x2d, 0xff-0x2d, 0, }},		//eBUTTON_ONKEY_LONG
	{{GROUP_BUTTON}, {0x11, 0xff-0x11, 0, }},		//eBUTTON_ONKEY_SHORT

	{{GROUP_GPIO}, {0x44, 0xff-0x44, 0, }},		//eGPIO_DC_IN
	{{GROUP_GPIO}, {0x55, 0xff-0x55, 0, }},		//eGPIO_DC_OUT
	{{GROUP_GPIO}, {0x66, 0xff-0x66, 0, }},		//eGPIO_LOW_EX_BAT_WARNING
	{{GROUP_GPIO}, {0x69, 0xff-0x69, 0, }},		//eGPIO_LOW_EX_BAT_OK

	{{GROUP_BATTERY}, {0, }},		//eHWR_BAT_RES

	//hh20140530 firmware->alive check 변경
	{{GROUP_ALIVE_CHECK}, {0, }},	//eHWR_ALIVE_CHECK
	{{GROUP_ALIVE_RESET}, {0, }},	//eHWR_ALIVE_RESET
	{{GROUP_ALIVE_CHANGE}, {0, }},	//eHWR_ALIVE_CHANGE

	{{GROUP_DC_IN}, {0, }},	//eDC_IN

	//Finddigital:hh:20140626
	{{MCU_ACC}, {0, }},	//eMCU_ACC
};

typedef enum _eDataType
{
	eHWR_REPEAT = 0,
	eHWR_POWER,
	eHWR_SR,
	eHWR_UP,
	eHWR_PREV,
	eHWR_SEL,
	eHWR_CANCEL,
	eHWR_DOWN,
	eHWR_VOLINC,
	eHWR_VOLDEC,
	eHWR_MYHOUSE,
	eHWR_MUTE,
	eHWR_CURPOS,
	eHWR_CHAINC,
	eHWR_NAVI,
	eHWR_CHADEC,
	eHWR_DMB,
	eHWR_OILINFO,
	eHWR_MUSIC,
	eHWR_COMPA,
	eHWR_RESEARCH,
	eHWR_WAYCANCEL,
	eHWR_UP_CH,
	eHWR_DOWN_CH,
	eHWR_PLUS_VOL,
	eHWR_MINUS_VOL,
	eHWR_DST,
	eHWR_MYMENU,
	eBUTTON_ONKEY_LONG,
	eBUTTON_ONKEY_SHORT,
	eGPIO_DC_IN,
	eGPIO_DC_OUT,
	eGPIO_LOW_EX_BAT_WARNING,
	eGPIO_LOW_EX_BAT_OK,
	eHWR_BAT_RES,
	eHWR_ALIVE_CHECK,
	eHWR_ALIVE_RESET,
	eHWR_ALIVE_CHANGE,
	eDC_IN,
	eMCU_ACC,
	eHWR_UNKNOWN
}eDataType;

static int gKeyVCodeTable[] = 
{
	KEY_POWER,
	KEY_BACK,
	KEY_HOME,
	KEY_ENTER,
	KEY_CANCEL,
	KEY_UP,
	KEY_DOWN,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_MUTE,
#if 0 // ischoi
	KEY_SMARTA_F1,				//HWR_SR
	KEY_SMARTA_F2,				//HWR_MYHOUSE
	KEY_SMARTA_F3,				//HWR_CURPOS
	KEY_SMARTA_F4,				//HWR_CHAINC
	KEY_SMARTA_F5,				//HWR_NAVI
	KEY_SMARTA_F6,				//HWR_CHADEC
	KEY_SMARTA_F7,				//HWR_DMB
	KEY_SMARTA_F8,				//HWR_OILINFO
	KEY_SMARTA_F9,				//HWR_MUSIC
	KEY_SMARTA_F10,				//HWR_COMPA
	KEY_SMARTA_F11,				//HWR_RESEARCH
	KEY_SMARTA_F12,				//HWR_WAYCANCEL
	KEY_SMARTA_F13,				//HWR_DST
	KEY_SMARTA_F14,				//HWR_MYMENU
	KEY_SMARTA_F15,				//BUTTON_ONKEY_LONG
	KEY_SMARTA_F16,				//HWR_SEL
	KEY_SMARTA_F17,
	KEY_SMARTA_F18,
	KEY_SMARTA_F19,
	KEY_SMARTA_F20,
	KEY_SMARTA_F21,
	KEY_SMARTA_F22,
	KEY_SMARTA_F23,
	KEY_SMARTA_F24,
	KEY_SMARTA_F25,
	KEY_SMARTA_F26,
	KEY_SMARTA_F27,
	KEY_SMARTA_F28,
	KEY_SMARTA_F29,
	KEY_SMARTA_F30,
	KEY_SMARTA_F31,
	KEY_SMARTA_F32,
	KEY_SMARTA_F33,
	KEY_SMARTA_F34,
	KEY_SMARTA_F35,
	KEY_SMARTA_F36,
	KEY_SMARTA_F37,
	KEY_SMARTA_F38,
	KEY_SMARTA_F39,
	KEY_SMARTA_F40,
	KEY_SMARTA_F41,
	KEY_SMARTA_F42,
	KEY_SMARTA_F43,
	KEY_SMARTA_F44,
	KEY_SMARTA_F45,
	KEY_SMARTA_F46,
	KEY_SMARTA_F47,
	KEY_SMARTA_F48,
	KEY_SMARTA_F49,
	KEY_SMARTA_F50,
/* eroum:dhkim:20140722 - start
   touch LCD ON */
    KEY_IQ9000V_LCDON,
/* eroum:dhkim:20140722 - end
   touch LCD ON */
#endif //#if 0 // ischoi
};

typedef struct _KeyMap {
	eDataType dateType;
	int keyCode;
} KeyMap;

static KeyMap gKeyMapTable[] = {	//   it relates to phoneWindowManager.java &  telechips_keypad.kl & keycodelabel.h
	{eBUTTON_ONKEY_LONG, KEY_POWER},
	{eHWR_DOWN, KEY_DOWN},
	{eHWR_UP, KEY_UP},
	{eHWR_VOLINC, KEY_VOLUMEUP},
	{eHWR_VOLDEC, KEY_VOLUMEDOWN},
	{eHWR_PLUS_VOL, KEY_VOLUMEUP},
	{eHWR_MINUS_VOL, KEY_VOLUMEDOWN},
	{eHWR_MUTE, KEY_MUTE},
#if 0 // ischoi
	{eHWR_SR, KEY_SMARTA_F1},
	{eHWR_MYHOUSE, KEY_SMARTA_F2},
	{eHWR_CURPOS, KEY_SMARTA_F3},
	{eHWR_CHAINC, KEY_SMARTA_F4},
	{eHWR_NAVI, KEY_SMARTA_F5},
	{eHWR_CHADEC, KEY_SMARTA_F6},
	{eHWR_UP_CH, KEY_SMARTA_F4},
	{eHWR_DOWN_CH, KEY_SMARTA_F6},
	{eHWR_DMB, KEY_SMARTA_F7},
	{eHWR_OILINFO, KEY_SMARTA_F8},
	{eHWR_MUSIC, KEY_SMARTA_F9},
	{eHWR_COMPA, KEY_SMARTA_F10},
	{eHWR_RESEARCH, KEY_SMARTA_F11},
	{eHWR_WAYCANCEL, KEY_SMARTA_F12},
	{eHWR_DST, KEY_SMARTA_F13},
	{eHWR_MYMENU, KEY_SMARTA_F14},
	{eHWR_SEL, KEY_SMARTA_F16},
	{eHWR_PREV, KEY_SMARTA_F23},
	{eHWR_CANCEL, KEY_SMARTA_F25},
	{eHWR_POWER, KEY_SMARTA_F42},
#endif //#if 0 // ischoi
};

static spinlock_t gLock;
static int gPowerShortKey = 0;

#if defined(REMOCON_PACKET_TIMEOUT) //salary - apply timeout control for Remocon UART packet
static eRX_STATUS rxStatus = eRXHeader;
static UARTPacket curRxPacket;
static int check = 0;
static int group = 0, checksum = 0, dataSize = 0;
#endif //salary - apply timeout control for Remocon UART packet

/* Finedigital:hh:20141031 - start
   Remocon Long Press */	
int gLastType;
/* Finedigital:hh:20141031 - end
   Remocon Long Press */


//function proto types
extern void send_dummy_touch_position(unsigned int x, unsigned int y);

static int smarta_mcucpu_ioctl(struct inode *inode, /*struct file * filp,*/ unsigned int cmd, unsigned long arg);
static int smarta_mcucpu_open(struct inode *inode, struct file *flip);
static int smarta_mcucpu_release(struct inode *inode, struct file *filp);


struct file_operations smarta_mcucpu_fops=
{
	.owner		= THIS_MODULE,
	.unlocked_ioctl			= smarta_mcucpu_ioctl,
	.open		= smarta_mcucpu_open,
	.release 		= smarta_mcucpu_release,
};

struct uart_stat {
	unsigned long base;
	int ch;
};

static struct uart_stat uart[] = {
#if 0 // ischoi
	{ HwUART0_BASE, 0 },
	{ HwUART1_BASE, 1 },
	{ HwUART2_BASE, 2 },
	{ HwUART3_BASE, 3 },
	{ HwUART4_BASE, 4 },
	{ HwUART5_BASE, 5 },
	{ HwUART6_BASE, 6 },
	{ HwUART7_BASE, 7 },
#endif //#if 0 // ischoi
};

#define uart_reg_write(p, a, v)	tcc_writel(v, uart[p].base + (a))
#define uart_reg_read(p, a)		tcc_readl(uart[p].base + (a))

#define UART_RBR	0x00		/* receiver buffer register */
#define UART_THR	0x00		/* transmitter holding register */
#define UART_DLL	0x00		/* divisor latch (LSB) */
#define UART_IER	0x04		/* interrupt enable register */
#define UART_DLM	0x04		/* divisor latch (MSB) */
#define UART_IIR	0x08		/* interrupt ident. register */
#define UART_FCR	0x08		/* FIFO control register */
#define UART_LCR	0x0C		/* line control register */
#define UART_MCR	0x10		/* MODEM control register */
#define UART_LSR	0x14		/* line status register */
#define UART_MSR	0x18		/* MODEM status register */
#define UART_SCR	0x1C		/* scratch register */
#define UART_AFT	0x20		/* AFC trigger level register */
#define UART_SIER	0x50		/* interrupt enable register */

#define LCR_WLS_MSK	0x03		/* character length select mask */
#define LCR_WLS_5	0x00		/* 5 bit character length */
#define LCR_WLS_6	0x01		/* 6 bit character length */
#define LCR_WLS_7	0x02		/* 7 bit character length */
#define LCR_WLS_8	0x03		/* 8 bit character length */
#define LCR_STB		0x04		/* Number of stop Bits, off = 1, on = 1.5 or 2) */
#define LCR_PEN		0x08		/* Parity eneble */
#define LCR_EPS		0x10		/* Even Parity Select */
#define LCR_STKP	0x20		/* Stick Parity */
#define LCR_SBRK	0x40		/* Set Break */
#define LCR_BKSE	0x80		/* Bank select enable */

#define FCR_FIFO_EN	0x01		/* FIFO enable */
#define FCR_RXFR	0x02		/* receiver FIFO reset */
#define FCR_TXFR	0x04		/* transmitter FIFO reset */

#define MCR_RTS		0x02

#define LSR_DR		0x01
#define LSR_THRE	0x20		/* transmitter holding register empty */
#define LSR_TEMT	0x40		/* transmitter empty */

#define UART_CLK		147456

static int gSuspendTest = 0;
void smarta_mcucpu_send_suspend_started(void);
void smarta_mcucpu_send_suspend_done(void);
void smarta_mcucpu_send_wakeup_done(void);
void smarta_mcucpu_send_dc_state_req(void);

static void smarta_mcucpu_early_suspend(struct early_suspend *h);
static void smarta_mcucpu_late_resume(struct early_suspend *h);

#ifdef CONFIG_SMARTA_BATTERY
#define BATTERY_MAX_VALUE 645
#define BATTERY_MIN_VALUE 535

static int smarta_battery_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val);
static ssize_t smarta_battery_show_property(struct device *dev, struct device_attribute *attr, char *buf);
static int smarta_power_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val);


static unsigned int cache_time = 1000;

#if TRACE_BATT
#define BATT(x...) printk(KERN_INFO "[BATT] " x)
#else
#define BATT(x...) do {} while (0)
#endif

typedef enum {
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC
} charger_type_t;

#define SMARTA_BATTERY_ATTR(_name)						\
{									\
	.attr = { .name = #_name, .mode = S_IRUGO, .owner = THIS_MODULE }, \
	.show = smarta_battery_show_property,				\
	.store = NULL,							\
}

static struct device_attribute smarta_battery_attrs[] = {
#if 0 // ischoi
	SMARTA_BATTERY_ATTR(batt_id),
	SMARTA_BATTERY_ATTR(batt_vol),
	SMARTA_BATTERY_ATTR(batt_temp),
	SMARTA_BATTERY_ATTR(batt_current),
	SMARTA_BATTERY_ATTR(charging_source),
	SMARTA_BATTERY_ATTR(charging_enabled),
	SMARTA_BATTERY_ATTR(full_bat),
#endif //#if 0 // ischoi
};

enum {
	BATT_ID = 0,
	BATT_VOL,
	BATT_TEMP,
	BATT_CURRENT,
	CHARGING_SOURCE,
	CHARGING_ENABLED,
	FULL_BAT,
};


struct battery_info_reply {
	u32 batt_id;			/* Battery ID from ADC */
	u32 batt_vol;			/* Battery voltage from ADC */
	u32 batt_temp;			/* Battery Temperature (C) from formula and ADC */
	u32 batt_current;		/* Battery current from ADC */
	u32 level;			/* formula */
	u32 charging_source;		/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;		/* 0: Disable, 1: Enable */
	u32 full_bat;			/* Full capacity of battery (mAh) */
};

struct smarta_battery_info {
	/* lock to protect the battery info */
	struct platform_device *pdev;
	struct mutex lock;
	struct clk *clk;
	struct battery_info_reply rep;
	struct work_struct changed_work;
	int present;
	unsigned long update_time;
};

static enum power_supply_property smarta_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property smarta_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

static struct power_supply smarta_power_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = smarta_battery_properties,
		.num_properties = ARRAY_SIZE(smarta_battery_properties),
		.get_property = smarta_battery_get_property
	},
	{
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = smarta_power_properties,
		.num_properties = ARRAY_SIZE(smarta_power_properties),
		.get_property = smarta_power_get_property,
	},
	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = smarta_power_properties,
		.num_properties = ARRAY_SIZE(smarta_power_properties),
		.get_property = smarta_power_get_property,
	},
};
static struct smarta_battery_info smarta_batt_info;

#define SAMPLE 100

struct battery_info_reply smarta_cur_battery_info =
{
	.batt_id = 0,			/* Battery ID from ADC */
	.batt_vol = 1100,			/* Battery voltage from ADC */
	.batt_temp = SAMPLE,			/* Battery Temperature (C) from formula and ADC */
	.batt_current = SAMPLE,		/* Battery current from ADC */
	.level = 100,				/* formula */
	.charging_source = 0,	/* 0: no cable, 1:usb, 2:AC */
	.charging_enabled  = 0,	/* 0: Disable, 1: Enable */
	.full_bat = SAMPLE			/* Full capacity of battery (mAh) */
};

/* ADC raw data Update to Android framework */
static void smarta_read_battery(struct battery_info_reply *info)
{
	memcpy(info, &smarta_cur_battery_info, sizeof(struct battery_info_reply));

	BATT("read battery level=%d, charging_source=%d, charging_enable=%d, full=%d\n", 
	     info->level, info->charging_source, info->charging_enabled, info->full_bat);
	
	return;
}

static int smarta_cable_status_update(int status)
{
	int rc = 0;
	unsigned last_source;

	//mutex_lock(&smarta_batt_info.lock);

	switch(status) {
	case CHARGER_BATTERY:
		BATT("cable NOT PRESENT\n");
		if(smarta_batt_info.rep.charging_source != CHARGER_BATTERY)
			smarta_batt_info.rep.charging_source = CHARGER_BATTERY;
		break;
	case CHARGER_USB:
		BATT("cable USB\n");
		if(smarta_batt_info.rep.charging_source != CHARGER_USB) {
			smarta_batt_info.rep.charging_source = CHARGER_USB;
		}
		break;
	case CHARGER_AC:
		BATT("cable AC\n");
		if(smarta_batt_info.rep.charging_source != CHARGER_AC) {
			smarta_batt_info.rep.charging_source = CHARGER_AC;
		}
		break;
	default:
		BATT(KERN_ERR "%s: Not supported cable status received!\n", __FUNCTION__);
		rc = -EINVAL;
	}

	last_source = smarta_batt_info.rep.charging_source;
	//mutex_unlock(&smarta_batt_info.lock);
	
	/* if the power source changes, all power supplies may change state */
	power_supply_changed(&smarta_power_supplies[CHARGER_BATTERY]);
	power_supply_changed(&smarta_power_supplies[CHARGER_USB]);
	power_supply_changed(&smarta_power_supplies[CHARGER_AC]);

	return rc;
}



static int smarta_battery_create_attrs(struct device * dev);
static int smarta_get_battery_info(struct battery_info_reply *buffer);

#endif

static void uart_init_port(int port, uint baud)
{
	uint16_t baud_divisor = (UART_CLK*100 / 16 / baud);
	
#if 0 // ischoi
	tca_ckc_setperi(PERI_UART6, ENABLE, UART_CLK);
	uart_reg_write(port, UART_LCR, LCR_EPS | LCR_STB | LCR_WLS_8);	/* 8 data, 1 stop, no parity */
	uart_reg_write(port, UART_IER, 0x01);
	uart_reg_write(port, UART_LCR, LCR_BKSE | LCR_EPS | LCR_STB | LCR_WLS_8);	/* 8 data, 1 stop, no parity */
	uart_reg_write(port, UART_DLL, baud_divisor & 0xff);
	uart_reg_write(port, UART_DLM, (baud_divisor >> 8) & 0xff);
	uart_reg_write(port, UART_FCR, FCR_FIFO_EN | FCR_RXFR | FCR_TXFR | Hw4 | Hw5);
	uart_reg_write(port, UART_LCR, LCR_EPS | LCR_STB | LCR_WLS_8);	/* 8 data, 1 stop, no parity */
#endif //#if 0 // ischoi
}

static int uart_putc(int port, char c )
{
#if 0 // ischoi
	/* wait for the last char to get out */
	while (!(uart_reg_read(port, UART_LSR) & LSR_THRE))
		;
	uart_reg_write(port, UART_THR, c);
#endif //#if 0 // ischoi
	return 0;
}

static int uart_getc(int port, bool wait)  /* returns -1 if no data available */
{
#if 0 // ischoi
	if (wait) {
		/* wait for data to show up in the rx fifo */
		while (!(uart_reg_read(port, UART_LSR) & LSR_DR))
			;
	} else {
		if (!(uart_reg_read(port, UART_LSR) & LSR_DR))
			return -1;
	}
	return uart_reg_read(port, UART_RBR);
#endif //#if 0 // ischoi
}

static void uart_flush_tx(int port)
{
#if 0 // ischoi
	/* wait for the last char to get out */
	while (!(uart_reg_read(port, UART_LSR) & LSR_TEMT))
		;
#endif //#if 0 // ischoi
}

static void uart_flush_rx(int port)
{

#if 0 // ischoi
	/* empty the rx fifo */
	while (uart_reg_read(port, UART_LSR) & LSR_DR) {
		volatile char c = uart_reg_read(port, UART_RBR);
		(void)c;
	}
#endif //#if 0 // ischoi
}

void smarta_send_rear_camera_event(int onoff)
{
	printk("[smarta_send_rear_camera_event] onoff : %d\r\n",onoff);
	
	if(gInputDev) {
#if 0 // ischoi
		if(onoff) {
			input_report_key(gInputDev, KEY_SMARTA_F21,1);
			input_sync(gInputDev);
			msleep(10);
			input_report_key(gInputDev, KEY_SMARTA_F21,0);
			input_sync(gInputDev);
		} else {
			input_report_key(gInputDev, KEY_SMARTA_F22,1);
			input_sync(gInputDev);
			msleep(10);
			input_report_key(gInputDev, KEY_SMARTA_F22,0);
			input_sync(gInputDev);
		}
#endif //#if 0 // ischoi
	} else {
		printk("mcucpu driver is not ready yet!\r\n");
	}
}
EXPORT_SYMBOL(smarta_send_rear_camera_event);

static void smarta_mcucpu_send_key_code(int keycode, int pressed)
{
	input_report_key(gInputDev, keycode, pressed);
	input_sync(gInputDev);
}

static eDataType smarta_mcucpu_get_data_type(int group, char* pdata, int size)
{
	eDataType i;
	for(i=0; i<eHWR_UNKNOWN; i++)
	{
		if(gRxPacketTable[i].group == group)
		{
			if(group == GROUP_BATTERY || group == GROUP_ALIVE_CHECK || group == GROUP_DC_IN
				|| group == GROUP_ALIVE_RESET|| group == GROUP_ALIVE_CHANGE|| group == MCU_ACC)
				break;
			else
			{
				int j, failed = 0;
				for(j=0; j<size-1; j++)
				{
					if(gRxPacketTable[i].data[j] != pdata[j])
					{
						failed = 1;
						break;
					}
				}

				if(!failed)
					break;
			}
		}
	}

	return i;
}

static void smarta_mcucpu_request_sleep_to_framework()
{
#if 0 // ischoi
	smarta_mcucpu_send_key_code(KEY_SMARTA_F15, KEY_PRESSED);
	mdelay(10);
	smarta_mcucpu_send_key_code(KEY_SMARTA_F15, KEY_RELEASED);
#endif // #if 0 // ischoi
}

static void smarta_mcucpu_input_repeat_key(unsigned long data)
{
	if(gKeyRepeat)
	{
#if 1 //Eroum:hh.shin:140808 - Prevent deep sleep mode(do not send KEY_POWER short key to android framework)
		if(gKeyRepeat != 0xFF)
#endif //Eroum:hh.shin:14088 - Prevent deep sleep mode(do not send KEY_POWER short key to android framework)
			gKeyRepeat = 0;
		
		mod_timer(&gKeyCheckTimer, jiffies + msecs_to_jiffies(REPEAT_PERIOD));
	}
	else
	{
		smarta_mcucpu_send_key_code(gLastKeyCode, KEY_RELEASED);
		gLastKeyCode = -1;
	}
}

//Eroum:hh.shin:141110 - headphone, ext mic insert event
void mcucpu_send_headphone_attach_event(int insert)
{
#if 0 // ischoi
	if(insert) {
		smarta_mcucpu_send_key_code(KEY_SMARTA_F47, KEY_PRESSED);
		smarta_mcucpu_send_key_code(KEY_SMARTA_F47, KEY_RELEASED);
	} else {
		smarta_mcucpu_send_key_code(KEY_SMARTA_F48, KEY_PRESSED);
		smarta_mcucpu_send_key_code(KEY_SMARTA_F48, KEY_RELEASED);
	}
#endif //#if 0 // ischoi
	printk("\033[33m\033[1m[%s] headphone insert event(%s)\033[0m\r\n",__FUNCTION__,insert?"ATTACH":"DETACH");
}

void mcucpu_send_extmic_attach_event(int insert)
{
#if 0 // ischoi
	if(insert) {
		smarta_mcucpu_send_key_code(KEY_SMARTA_F49, KEY_PRESSED);
		smarta_mcucpu_send_key_code(KEY_SMARTA_F49, KEY_RELEASED);
	} else {
		smarta_mcucpu_send_key_code(KEY_SMARTA_F50, KEY_PRESSED);
		smarta_mcucpu_send_key_code(KEY_SMARTA_F50, KEY_RELEASED);
	}
#endif //#if 0 // ischoi
	printk("\033[33m\033[1m[%s] ext mic insert event(%s)\033[0m\r\n",__FUNCTION__,insert?"ATTACH":"DETACH");
}
//Eroum:hh.shin:141110 - headphone, ext mic insert event


static char* smarta_mcucpu_get_packet_type_string(eDataType type)
{
	char* ret = NULL;
	
	switch(type)
	{
		case eHWR_REPEAT:
			ret = "HWR_REPEAT";
			break;
		case eHWR_POWER:
			ret = "HWR_POWER";
			break;
		case eHWR_SR:
			ret = "HWR_SR";
			break;
		case eHWR_UP:
			ret = "HWR_UP";
			break;
		case eHWR_PREV:
			ret = "HWR_PREV";
			break;
		case eHWR_SEL:
			ret = "HWR_SEL";
			break;
		case eHWR_CANCEL:
			ret = "HWR_CANCEL";
			break;
		case eHWR_DOWN:
			ret = "HWR_DOWN";
			break;
		case eHWR_VOLINC:
			ret = "HWR_VOLINC";
			break;
		case eHWR_VOLDEC:
			ret = "HWR_VOLDEC";
			break;
		case eHWR_MYHOUSE:
			ret = "HWR_MYHOUSE";
			break;
		case eHWR_MUTE:
			ret = "HWR_MUTE";
			break;
		case eHWR_CURPOS:
			ret = "HWR_CURPOS";
			break;
		case eHWR_CHAINC:
			ret = "HWR_CHAINC";
			break;
		case eHWR_NAVI:
			ret = "HWR_NAVI";
			break;
		case eHWR_CHADEC:
			ret = "HWR_CHADEC";
			break;
		case eHWR_DMB:
			ret = "HWR_DMB";
			break;
		case eHWR_OILINFO:
			ret = "HWR_OILINFO";
			break;
		case eHWR_MUSIC:
			ret = "HWR_MUSIC";
			break;
		case eHWR_COMPA:
			ret = "HWR_COMPA";
			break;
		case eHWR_RESEARCH:
			ret = "HWR_RESEARCH";
			break;
		case eHWR_WAYCANCEL:
			ret = "eHWR_WAYCANCEL";
			break;
		case eHWR_UP_CH:
			ret = "HWR_UP_CH";
			break;
		case eHWR_DOWN_CH:
			ret = "HWR_DOWN_CH";
			break;
		case eHWR_PLUS_VOL:
			ret = "HWR_PLUS_VOL";
			break;
		case eHWR_MINUS_VOL:
			ret = "HWR_MINUS_VOL";
			break;
		case eHWR_DST:
			ret = "HWR_DST";
			break;
		case eHWR_MYMENU:
			ret = "HWR_MYMENU";
			break;
		case eBUTTON_ONKEY_LONG:
			ret = "BUTTON_ONKEY_LONG";
			break;
		case eBUTTON_ONKEY_SHORT:
			ret = "BUTTON_ONKEY_SHORT";
			break;
		case eGPIO_DC_IN:
			ret = "GPIO_DC_IN";
			break;
		case eGPIO_DC_OUT:
			ret = "GPIO_DC_OUT";
			break;
		case eGPIO_LOW_EX_BAT_WARNING:
			ret = "GPIO_LOW_EX_BAT_WARNING";
			break;
		case eGPIO_LOW_EX_BAT_OK:
			ret = "GPIO_LOW_EX_BAT_OK";
			break;
		case eHWR_BAT_RES:
			ret = "HWR_BAT_RES";
			break;
		case eHWR_ALIVE_CHECK:
			ret = "HWR_ALIVE_CHECK";
			break;
		case eHWR_ALIVE_RESET:
			ret = "HWR_ALIVE_RESET";
			break;
		case eHWR_ALIVE_CHANGE:
			ret = "HWR_ALIVE_CHANGE";
			break;
		case eDC_IN:
			ret = "DC_IN";
			break;
		case eMCU_ACC:
			ret = "MCU_ACC";
			break;
		default:
			ret = "UNKNOWN";
	}

	return ret;
}

static void smarta_mcucpu_send_key_to_framework(eDataType type)
{
	int i;

	if(gLastKeyCode != -1)
		return;
	
	for (i = 0; i < ARRAY_SIZE(gKeyMapTable); i++)
	{
		if(type == gKeyMapTable[i].dateType)
		{
			gLastKeyCode = gKeyMapTable[i].keyCode;
			gLastType= gKeyMapTable[i].keyCode;
			smarta_mcucpu_send_key_code(gKeyMapTable[i].keyCode, KEY_PRESSED);
			break;
		}
	}
}

/* eroum:dhkim:20141010 - start - bootcount-renewal */
#ifdef AUTOUPDATE_RENEWAL
unsigned int readBootCount(void)
{
    char buffer[BUFF_SIZE];
    int i;
    mm_segment_t old_fs;
    struct file *mFilp = NULL;
    unsigned int dwBootCount;

    for(i = 0; i < BUFF_SIZE; i++) buffer[i] = 0;

    mFilp = filp_open(DNA_DEVICE_NAME, O_RDWR | O_LARGEFILE, 0);

    if (mFilp == NULL)
    {
        printk("[readBootCount] filp_open error!!.\n");
        return -1;
    }
    
    old_fs = get_fs();
    set_fs(get_ds());
    mFilp->f_op->llseek(mFilp, FINE_MODE_ADDR + (sizeof(unsigned int) * 4), SEEK_SET);
    mFilp->f_op->read(mFilp, buffer, BUFF_SIZE, &mFilp->f_pos);
    set_fs(old_fs);

    memcpy((char *)&dwBootCount, buffer, sizeof(unsigned int));

    printk("[readBootCount] BootCount = %d\n", dwBootCount);

    filp_close(mFilp, NULL);
    
    return dwBootCount;
}

void writeBootCount(unsigned int mBootCount)
{
    char buffer[BUFF_SIZE];
    int i;
    mm_segment_t old_fs;
    struct file *mFilp = NULL;

    for(i = 0; i < BUFF_SIZE; i++) buffer[i] = 0;

    mFilp = filp_open(DNA_DEVICE_NAME, O_RDWR | O_LARGEFILE, 0);

    if (mFilp == NULL)
    {
        printk("[writeBootCount] filp_open error!!.\n");
        return;
    }

    memcpy(buffer, (char *)&mBootCount, sizeof(unsigned int));
    
    old_fs = get_fs();
    set_fs(get_ds());
    mFilp->f_op->llseek(mFilp, FINE_MODE_ADDR + (sizeof(unsigned int) * 4), SEEK_SET);
    mFilp->f_op->write(mFilp, buffer, BUFF_SIZE, &mFilp->f_pos);
    set_fs(old_fs);

    printk("[writeBootCount] BootCount = %d\n", mBootCount);

    filp_close(mFilp, NULL);
}
#endif
/* eroum:dhkim:20141010 - end - bootcount-renewal */

/* eroum:dhkim:20141103 - start
   indi-mmc */
#if defined(CONFIG_FINE_MMC_INDI)
struct task_struct *tThread;
int DISABLE_tThread = 0;
void cleanup_tThread(void);
void UPDATE_MMC_STATUS(int onoff);

void LED_CTL(int onoff)
{
    tcc_gpio_config(EL_EN, GPIO_FN(0));
    gpio_request(EL_EN, "LED_PWR_EN");
    gpio_direction_output(EL_EN, onoff?1:0);
    UPDATE_MMC_STATUS(onoff);
}

int tThreadloop(void *data)
{
    while(1)
    {
        msleep(1000);
        LED_CTL(0);

        if(DISABLE_tThread == 1)
            cleanup_tThread();

        if(kthread_should_stop())
            break;
    }    

    return 0;
}

int init_tThread(void)
{
    tThread = kthread_run(tThreadloop, NULL, "kthread");
    return 0;
}

void cleanup_tThread(void)
{
    kthread_stop(tThread);
}
#endif /* CONFIG_FINE_MMC_INDI */
/* eroum:dhkim:20141103 - end
   indi-mmc */

unsigned int readPercentage(void)
{
    char buffer[BUFF_SIZE];
    int i;
    mm_segment_t old_fs;
    struct file *mFilp = NULL;
    unsigned int dwPercentage;

#if 0 //ischoi
    for(i = 0; i < BUFF_SIZE; i++) buffer[i] = 0;

    mFilp = filp_open(DNA_DEVICE_NAME, O_RDWR | O_LARGEFILE, 0);

    if (mFilp == NULL)
    {
        printk("[readPercentage] filp_open error!!.\n");
        return -1;
    }
    
    old_fs = get_fs();
    set_fs(get_ds());
    mFilp->f_op->llseek(mFilp, FINE_MODE_ADDR + (sizeof(unsigned int) * 2), SEEK_SET);
    mFilp->f_op->read(mFilp, buffer, BUFF_SIZE, &mFilp->f_pos);
    set_fs(old_fs);

    memcpy((char *)&dwPercentage, buffer, sizeof(unsigned int));

    printk("[readPercentage] Percentage = %d\n", dwPercentage);

    filp_close(mFilp, NULL);
#endif //ischoi

    return dwPercentage;
}

void writePercentage(unsigned int mPercentage)
{
    char buffer[BUFF_SIZE];
    int i;
    mm_segment_t old_fs;
    struct file *mFilp = NULL;
#if 0 //ischoi
    for(i = 0; i < BUFF_SIZE; i++) buffer[i] = 0;

    mFilp = filp_open(DNA_DEVICE_NAME, O_RDWR | O_LARGEFILE, 0);

    if (mFilp == NULL)
    {
        printk("[writePercentage] filp_open error!!.\n");
        return;
    }

    memcpy(buffer, (char *)&mPercentage, sizeof(unsigned int));
    
    old_fs = get_fs();
    set_fs(get_ds());
    mFilp->f_op->llseek(mFilp, FINE_MODE_ADDR + (sizeof(unsigned int) * 2), SEEK_SET);
    mFilp->f_op->write(mFilp, buffer, BUFF_SIZE, &mFilp->f_pos);
    set_fs(old_fs);

    printk("[writePercentage] Percentage = %d\n", mPercentage);

    filp_close(mFilp, NULL);
#endif //ischoi
}

unsigned int readCurrent(void)
{
    char buffer[BUFF_SIZE];
    int i;
    mm_segment_t old_fs;
    struct file *mFilp = NULL;
    unsigned int dwCurrent;
#if 0//ischoi
    for(i = 0; i < BUFF_SIZE; i++) buffer[i] = 0;

    mFilp = filp_open(DNA_DEVICE_NAME, O_RDWR | O_LARGEFILE, 0);

    if (mFilp == NULL)
    {
        printk("[readCurrent] filp_open error!!.\n");
        return -1;
    }
    
    old_fs = get_fs();
    set_fs(get_ds());
    mFilp->f_op->llseek(mFilp, FINE_MODE_ADDR + (sizeof(unsigned int)), SEEK_SET);
    mFilp->f_op->read(mFilp, buffer, BUFF_SIZE, &mFilp->f_pos);
    set_fs(old_fs);

    memcpy((char *)&dwCurrent, buffer, sizeof(unsigned int));

    printk("[readCurrent] Current = %d\n", dwCurrent);

    filp_close(mFilp, NULL);
#endif //ischoi
    return dwCurrent;
}

void writeCurrent(unsigned int mCurrent)
{
    char buffer[BUFF_SIZE];
    int i;
    mm_segment_t old_fs;
    struct file *mFilp = NULL;
#if 0 //ischoi
    for(i = 0; i < BUFF_SIZE; i++) buffer[i] = 0;

    mFilp = filp_open(DNA_DEVICE_NAME, O_RDWR | O_LARGEFILE, 0);

    if (mFilp == NULL)
    {
        printk("[writeCurrent] filp_open error!!.\n");
        return;
    }

    memcpy(buffer, (char *)&mCurrent, sizeof(unsigned int));
    
    old_fs = get_fs();
    set_fs(get_ds());
    mFilp->f_op->llseek(mFilp, FINE_MODE_ADDR + (sizeof(unsigned int)), SEEK_SET);
    mFilp->f_op->write(mFilp, buffer, BUFF_SIZE, &mFilp->f_pos);
    set_fs(old_fs);

    printk("[writeCurrent] Current = %d\n", mCurrent);

    filp_close(mFilp, NULL);
#endif //ischoi
}

/* eroum dhkim - start : auto-recovery */
#ifdef AUTOUPDATE_RENEWAL
void Set_Count(unsigned int value){
    printk("[MCU_COM     ] Set_Count++ value= %d\r\n", value);
    writeBootCount(value);
}

int Get_Count(){
    printk("[MCU_COM     ] Get_Count++ \r\n");
    return readBootCount();
}

void Set_Update(){
    ((PPMU)tcc_p2v(HwPMU_BASE))->PMU_USSTATUS.nREG = AUTO_RECOVERY_MODE;
    writePercentage(PERCENTAGE_OS);
    writeCurrent(CURRENT_AUTO);
}
#else
int Set_Count(int value){
    static char * cmdpath = "/system/bin/bootcount";
	char *argv0[] = { cmdpath, "-s", "0", "BOOTCOUNT", NULL};
	char *argv1[] = { cmdpath, "-s", "1", "BOOTCOUNT", NULL};
    static char *envp[] = { "HOME=/",
                        "TERM=linux",
                        "PATH=/sbin:/usr/inssbin:/bin:/usr/bin",
                        NULL };	
	char * temp;
	if(value==0)
   	 	return call_usermodehelper(cmdpath, argv0, envp, UMH_WAIT_EXEC);
	else
    	return call_usermodehelper(cmdpath, argv1, envp, UMH_WAIT_EXEC);
	return 0;
}

int Get_Count(){
    static char * cmdpath = "/system/bin/bootcount";
    char *argv[] = { cmdpath, "-g", "BOOTCOUNT", NULL};
    static char *envp[] = { "HOME=/",
                        "TERM=linux",
                        "PATH=/sbin:/usr/inssbin:/bin:/usr/bin",
                        NULL };
    return call_usermodehelper(cmdpath, argv, envp, UMH_WAIT_EXEC);
}

int Set_Update(){
    static char * cmdpath = "/system/bin/bootcount";
    char *argv[] = { cmdpath, "-a", "BOOTCOUNT", NULL};
    static char *envp[] = { "HOME=/",
                        "TERM=linux",
                        "PATH=/sbin:/usr/inssbin:/bin:/usr/bin",
                        NULL };
#if 0 // ischoi
    ((PPMU)tcc_p2v(HwPMU_BASE))->PMU_USSTATUS.nREG = AUTO_RECOVERY_MODE;
#endif //#if 0 // ischoi
    return call_usermodehelper(cmdpath, argv, envp, UMH_WAIT_EXEC);
}
#endif
/* eroum dhkim - end : auto-recovery */

void smarta_mcucpu_send_dummy_key_event(void)
{
	printk(KERN_INFO "smarta_mcucpu_send_dummy_key_event\n");
	smarta_mcucpu_send_key_code(KEY_BACK, KEY_PRESSED);
	msleep(5);
	smarta_mcucpu_send_key_code(KEY_BACK, KEY_RELEASED);
}

static int smarta_mcucpu_packet_process(eDataType type, char* pdata)
{
	PRINT_UART("smarta_mcucpu_packet_process type: %s\n", smarta_mcucpu_get_packet_type_string(type));

	switch(type)
	{
		case eHWR_SR:
		case eHWR_PREV:
		case eHWR_CANCEL:
		case eHWR_VOLINC:
		case eHWR_PLUS_VOL:
		case eHWR_VOLDEC:
		case eHWR_MINUS_VOL:
		case eHWR_MUTE:
		case eHWR_CURPOS:
		case eHWR_CHAINC:
		case eHWR_UP_CH:
		case eHWR_NAVI:
		case eHWR_CHADEC:
		case eHWR_DOWN_CH:
		case eHWR_DMB:
		case eHWR_OILINFO:
		case eHWR_MUSIC:
		case eHWR_COMPA:
		case eHWR_MYHOUSE:
		case eHWR_RESEARCH:
		case eHWR_DST:
		case eHWR_MYMENU:
		case eHWR_UP:
		case eHWR_DOWN:
		case eHWR_SEL:
		case eHWR_WAYCANCEL:
		{
			smarta_mcucpu_send_key_to_framework(type);
			mod_timer(&gKeyCheckTimer, jiffies + msecs_to_jiffies(REPEAT_DELAY));
		}
		break;	
		case eHWR_REPEAT:
		{
			gKeyRepeat = 1;
#ifdef REMOCON_POWER_LONG_OFF	
			rep_cnt ++;
			/* Finedigital:hh:20141031 - start
			   Remocon Long Press */
			if(rep_cnt %3==0 )
			{
				PRINT_UART("smarta_mcucpu_send_key_code gLastType: %d\n", gLastType);
				if(gLastType==KEY_VOLUMEUP||gLastType==KEY_VOLUMEDOWN)
				{
					smarta_mcucpu_send_key_code(gLastType, KEY_PRESSED);
					msleep(10);
					smarta_mcucpu_send_key_code(gLastType, KEY_RELEASED);
				}
			}
			/* Finedigital:hh:20141031 - start
			   Remocon Long Press */

			if(rep_cnt > 4 && pwr_press == 1)
			{
				gLastKeyCode = -1;
				smarta_mcucpu_send_key_to_framework(eBUTTON_ONKEY_LONG);				

#if 1 //Eroum:hh.shin:140808 - Prevent deep sleep mode(do not send KEY_POWER short key to android framework)
				//Must be send power off
				gKeyRepeat = 0xFF;
#else
				rep_cnt = 0;
				pwr_press = 0;
#endif //Eroum:hh.shin:14088 - Prevent deep sleep mode(do not send KEY_POWER short key to android framework)

			}
#endif
		}
		break;
		case eBUTTON_ONKEY_LONG:
		{
//			smarta_mcucpu_send_key_to_framework(eHWR_POWER);
			smarta_mcucpu_send_key_to_framework(eBUTTON_ONKEY_LONG);
		}
		break;
#ifdef REMOCON_POWER_LONG_OFF	
		case eHWR_POWER:
		{
			smarta_mcucpu_send_key_to_framework(eHWR_POWER);
			mod_timer(&gKeyCheckTimer, jiffies + msecs_to_jiffies(REPEAT_DELAY));
			pwr_press = 1;

		}		
		break;
#else
		case eHWR_POWER:
#endif
		case eBUTTON_ONKEY_SHORT:
		{
			smarta_mcucpu_send_key_to_framework(eHWR_POWER);
			mod_timer(&gKeyCheckTimer, jiffies + msecs_to_jiffies(REPEAT_DELAY));
		}
		break;
		case eGPIO_DC_IN:
		{
#ifdef CONFIG_SMARTA_BATTERY
			smarta_cable_status_update(CHARGER_AC);
#endif
		}
		break;
		case eGPIO_DC_OUT:
		{
#ifdef CONFIG_SMARTA_BATTERY		
			smarta_mcucpu_request_sleep_to_framework();
#endif
		}
		break;
		case eGPIO_LOW_EX_BAT_WARNING:
		{
			
		}
		break;
		case eGPIO_LOW_EX_BAT_OK:
		{
			
		}
		break;
		case eHWR_BAT_RES:
		{
#ifdef CONFIG_SMARTA_BATTERY
			struct battery_info_reply info;

			memcpy(&info, &smarta_batt_info.rep, sizeof(struct battery_info_reply));
			info.level = (((pdata[0]<<8|pdata[1])-BATTERY_MIN_VALUE)*100)/(BATTERY_MAX_VALUE-BATTERY_MIN_VALUE);
			
			if((int)info.level < 0)
				info.level = 0;
			else if(info.level > 100)
				info.level = 100;
			
#if 0 // ischoi
			info.charging_enabled = !gpio_get_value(BAT_CHG);
#endif //#if 0 // ischoi

			memcpy(&smarta_cur_battery_info, &info, sizeof(struct battery_info_reply));
			power_supply_changed(&smarta_power_supplies[CHARGER_BATTERY]);
#endif
		}
		break;
		//hh20140530
		case eHWR_ALIVE_CHECK:
		{
			//printk(KERN_INFO "mcu version ==> 0x%x\n", pdata[0]);
			//printk(KERN_INFO "eHWR_ALIVE_CHECK============\n");
			if(g_bAliveAckFlag)
			{
				uart_putc(MCU_UART_PORT, CPU_HEADER);
				uart_putc(MCU_UART_PORT, 0x19);
				uart_putc(MCU_UART_PORT, 0xE6);
			}
/* Finedigital:hh:20141017 - start
   Booting Fail Check */	
#if COMPLETEBOOTING
			if(nBootFlg)
			{
				nBootCnt++;
				if(nBootCnt>6)
				{
					extern void machine_restart(char *cmd);
#ifdef AUTOUPDATE_RENEWAL
					int temp = Get_Count();
	            	if(temp == 1)
					{
						Set_Count(0);
						Set_Update();
						printk("boot fail count = %d\n",temp+1);
					}
					else if(temp == 0)
					{
						Set_Count(1);
						printk("boot fail count = %d\n",temp+1);
					}
#else
				Get_Count();
#endif

				msleep(3000);
				machine_restart(NULL);
				}
				
			}
#endif
/* Finedigital:hh:20141017 - end
   Booting Fail Check */		
		}
		break;//*/
		
		//hh20140602
		case eHWR_ALIVE_RESET:
		{
			printk("[MCU_COM     ] No ACK Signal during 30Sec ==> Reset Now!! \r\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0xA1);
			uart_putc(MCU_UART_PORT, 0x5E);
		}
		break;			

		case eHWR_ALIVE_CHANGE:
		{
			if(g_bAliveAckFlag)
				g_bAliveAckFlag = false;
			else
				g_bAliveAckFlag = true;
			
			printk("[MCU_COM     ] ACK Signal Current State(%d) \r\n",g_bAliveAckFlag);
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0x07);
			uart_putc(MCU_UART_PORT, 0xF8);
		}	
		break;		
			//*/
		case eDC_IN:
		{
#if	MCUVERSION
			//printk(KERN_INFO "mcu version ==> 0x%x\n", pdata[0]);
			nMCU_Ver = (int)pdata[0];
#endif
#ifdef CONFIG_SMARTA_BATTERY
			if(pdata[0] == 0x01)
				smarta_cable_status_update(CHARGER_AC);
			else if(pdata[0] == 0x00)
				smarta_cable_status_update(CHARGER_BATTERY);
#endif
		}
		break;

		//Finedigital:hh:20140626 - ACC Detect
		case eMCU_ACC:
		{
			printk("[MCU_COM     ] eMCU_ACC \r\n");
			smarta_mcucpu_send_key_to_framework(eBUTTON_ONKEY_LONG);	
		}	
		break;	
	}

	return 0;
}

static int smarta_mcucpu_packet_parser(UARTPacket* pPacket)
{
	int group, checksum, dataSize;
	eDataType type;
	int dataSum, i, tempSize;
	
	group = (pPacket->header & 0xf0) >> 4;
	checksum = (pPacket->header & 0x0c) >> 2;
	dataSize = pPacket->header & 0x03;
	tempSize = dataSize?dataSize:4;

	for(i=0; i<tempSize-1; i++)
		dataSum += pPacket->data[i];

	if((dataSum != 0xff))
	{
		printk(KERN_INFO "!!! mcu -> cpu uart error1 !!!\n");
		return -1;
	}
	if(checksum != ((group+dataSize)%3))
	{
		printk(KERN_INFO "!!! mcu -> cpu uart error2 !!!\n");
		return -1;
	}

	type = smarta_mcucpu_get_data_type(group, pPacket->data, dataSize);
	smarta_mcucpu_packet_process(type, pPacket->data);

	return 0;
}

static irqreturn_t smarta_mcucpu_interrupt_handler(int irq, void *dev_id)
{
	PRINT_UART("smarta_mcucpu_interrupt_handler  \n");
#if defined(BOTTOM_THREAD_TYPE) //salary - process MCU UART packet in bottom thread when it has received packet
	int data = -1;

#if 0 // ischoi
	if(uart_reg_read(MCU_UART_PORT, UART_IIR) & LSR_DR)
		return IRQ_NONE;

	while((data = uart_getc(MCU_UART_PORT, 0)) != -1)
	{
		mcu_cpu_put_packet(data);
	}
#endif //#if 0 // ischoi

	schedule_work(&mcucpu_uart_packet_work_q);

#if defined(REMOCON_PACKET_TIMEOUT) //salary - apply timeout control for Remocon UART packet
	if(!mcucpu_uart_packet_timer_is_expired ) {
//		mcucpu_uart_packet_timer.expires = jiffies + msecs_to_jiffies(REMOCON_PACKET_TIMEOUT_MS);
		mod_timer(&mcucpu_uart_packet_timer,jiffies + msecs_to_jiffies(REMOCON_PACKET_TIMEOUT_MS));
	} else {
		mcucpu_uart_packet_timer.expires = jiffies + msecs_to_jiffies(REMOCON_PACKET_TIMEOUT_MS);
		mcucpu_uart_packet_timer_is_expired = 0;
		add_timer(&mcucpu_uart_packet_timer);
	}
#endif //salary - apply timeout control for Remocon UART packet


	
#else
	spinlock_t* plock = (spinlock_t *)dev_id;
	int data = -1;

	static eRX_STATUS rxStatus = eRXHeader;
	static UARTPacket curRxPacket;
	static int check = 0;
	static int group = 0, checksum = 0, dataSize = 0;
	
	if(uart_reg_read(MCU_UART_PORT, UART_IIR) & LSR_DR)
		return IRQ_NONE;

	spin_lock(plock);

	while((data = uart_getc(MCU_UART_PORT, 0)) != -1)
	{
#if 0 // ischoi
		extern int tw9900_boot_complete;
		
		if(!tw9900_boot_complete)
			continue;
#endif //#if 0 // ischoi
	
		PRINT_UART("smarta_mcucpu_interrupt_handler  0x%x\n", data);
		
		switch(rxStatus)
		{
			case eRXHeader:
				curRxPacket.header = (char)data;
				group = (curRxPacket.header & 0xf0) >> 4;
				checksum = (curRxPacket.header & 0x0c) >> 2;
				dataSize = curRxPacket.header & 0x03;
				if(!dataSize)
					dataSize = 4;
				rxStatus = eRXData;
				break;
			case eRXData:
				if(check > 3) {
					printk("check error\r\n\n\n\n\n\n\n\n");
					check = 0;
				}
				curRxPacket.data[check++] = (char)data;
				if(check == dataSize-1)
				{
					smarta_mcucpu_packet_parser(&curRxPacket);
					rxStatus = eRXHeader;
					check = 0;
				}
				break;
		}
	}

	spin_unlock(plock);
#endif //BOTTOM_THREAD_TYPE //salary - process MCU UART packet in bottom thread when it has received packet

	return IRQ_HANDLED;
}

#if defined(REBOOT_TEST)
static struct timer_list restart_ts_timer;

static void restart_ts_timer_handler(unsigned long data)
{ //reboot test
			extern void machine_restart(char *cmd);
		
			printk("System reboot");
			machine_restart(NULL);
}
#endif //REBOOT_TEST

/* eroum:dhkim:20140811 - start
   recovery-renewal */
void smarta_mcu_force_repair() {
    printk(KERN_INFO "IOCTL_MCU_FORCE_REPAIR\n");
    uart_putc(MCU_UART_PORT, CPU_HEADER);
    uart_putc(MCU_UART_PORT, 0x3c);
    uart_putc(MCU_UART_PORT, 0xc3);
}
/* eroum:dhkim:20140811 - end
   recovery-renewal */

/* eroum:dhkim:20140904 - start - update-gui */
#define OBD_DATA_ADDR       (0x87000000)
#define OBD_CMD_ADDR        (0x1000FFF0)
#define OBD_INDEX_ADDR      (OBD_DATA_ADDR + 0x100000)
#define BOOT_MOTION_ADDR    (OBD_INDEX_ADDR + 0x100000)
#define UNZIPBUF_1_ADDR     (BOOT_MOTION_ADDR + 0x100000)
#define DNA_DEVICE_NAME     "/dev/block/mmcblk0p13"

#if 0 //ischoi
/* eroum:dhkim:20140918 - end : RENEW UPDATE */
#define NUMBER_CASE1 1+(PERCENTAGE_CASE1_END - PERCENTAGE_CASE1_START) // 26
#define NUMBER_CASE2 1+(PERCENTAGE_CASE2_END - PERCENTAGE_CASE2_START) // 25
#define NUMBER_CASE3 1+(PERCENTAGE_CASE3_END - PERCENTAGE_CASE3_START) // 25
#define NUMBER_CASE4 1+(PERCENTAGE_CASE4_END - PERCENTAGE_CASE4_START) // 25

#define NUMBER_CASE5 1+(PERCENTAGE_CASE2_END - PERCENTAGE_CASE1_START) // 51
#else
/* eroum:dhkim:20140918 - end : RENEW UPDATE */
#define NUMBER_CASE1  26
#define NUMBER_CASE2  25
#define NUMBER_CASE3  25
#define NUMBER_CASE4  25

#define NUMBER_CASE5  51
#endif //ischoi

int case1[NUMBER_CASE1] = {
    88,168,248,328,408,488,568,648,728,808,
    888,968,1048,1128,1208,1288,1368,1448,1528,1608,
    1688,1768,1848,1928,2008,2088
};

int case2[NUMBER_CASE2] = {
    84,168,252,336,420,504,588,668,752,836,
    920,1004,1088,1172,1256,1336,1420,1504,1588,1672,
    1756,1840,1924,2004,2088
};

int case3[NUMBER_CASE3] = {
    84,168,252,336,420,504,588,668,752,836,
    920,1004,1088,1172,1256,1336,1420,1504,1588,1672,
    1756,1840,1924,2004,2088
};

int case4[NUMBER_CASE4] = {
    84,168,252,336,420,504,588,668,752,836,
    920,1004,1088,1172,1256,1336,1420,1504,1588,1672,
    1756,1840,1924,2004,2088
};

int case5[NUMBER_CASE5] = {
    88,128,168,208,248,288,328,368,408,448,
    488,528,568,608,648,688,728,768,808,848,
    888,928,968,1008,1048,1088,1128,1168,1208,1248,
    1288,1328,1368,1408,1448,1488,1528,1568,1608,1648,
    1688,1728,1768,1808,1848,1888,1928,1968,2008,2048,
    2088
};

#if 0 // ischoi
char buffer[IMAGE_OFFSET];
void update_number(void * base_address, unsigned char * buffer, int width, int height)
{
    int x, y;
    
    x = 0;
    for(y = height; y < (height + NUMBER_HEIGHT); y++){
        memcpy(base_address + 800*y*4 + width, &buffer[x], NUMBER_WIDTH*4);
        x += NUMBER_WIDTH*4;
    }
}

void update_draw(unsigned char* pFrameBuffer, unsigned long dwSize, unsigned long arg)
{
    mm_segment_t old_fs;
    struct file *mFilp = NULL;
    void *obd_remapped_base_address;
    int x, y;

    obd_remapped_base_address = (void *)ioremap_nocache(pFrameBuffer, dwSize);
    
    dgbprintk("\033[31m\033[1m[%3d]\033[0m\b\b\b\b\b", arg);
    mPercentage = arg;
    if(NULL != obd_remapped_base_address)
    {
        if((arg > PERCENTAGE_CASE4_END) && (arg < PERCENTAGE_CASE5_START))
        {
            mFilp = filp_open(DNA_DEVICE_NAME, O_RDWR | O_LARGEFILE, 0);
            
            if (mFilp == NULL)
            {
                printk("[update_draw] filp_open error!!.\n");
                return -1;
            }
            
            old_fs = get_fs();
            set_fs(get_ds());
            mFilp->f_op->llseek(mFilp, FINE_ERROR_IMAGE_ADDR, SEEK_SET);
            mFilp->f_op->read(mFilp, buffer, IMAGE_OFFSET, &mFilp->f_pos);
            set_fs(old_fs);
            
            memcpy(obd_remapped_base_address, buffer, IMAGE_OFFSET);
            
            filp_close(mFilp, NULL);

#ifdef SHOW_ERROR_CODE
            if(arg == ERROR_CASE_101)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_102)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(2), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_103)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_104)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(4), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_105)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(5), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_106)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(6), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_107)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(7), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_108)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(8), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_109)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(9), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_110)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_111)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_112)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(2), NUMBER_X_3_ERR, NUMBER_Y);
            }
#endif
        }
        else if((arg > (ERROR_CASE_301 - 1)) && (arg < (ERROR_CASE_318 + 1)))
        {
            mFilp = filp_open(DNA_DEVICE_NAME, O_RDWR | O_LARGEFILE, 0);
            
            if (mFilp == NULL)
            {
                printk("[update_draw] filp_open error!!.\n");
                return -1;
            }
            
            old_fs = get_fs();
            set_fs(get_ds());
            mFilp->f_op->llseek(mFilp, FINE_ERROR2_IMAGE_ADDR, SEEK_SET);
            mFilp->f_op->read(mFilp, buffer, IMAGE_OFFSET, &mFilp->f_pos);
            set_fs(old_fs);
            
            memcpy(obd_remapped_base_address, buffer, IMAGE_OFFSET);
            
            filp_close(mFilp, NULL);

#ifdef SHOW_ERROR_CODE
            if(arg == ERROR_CASE_301)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_302)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(2), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_303)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_304)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(4), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_305)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(5), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_306)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(6), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_307)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(7), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_308)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(8), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_309)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(9), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_310)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_311)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_312)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(2), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_313)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_314)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(4), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_315)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(5), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_316)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(6), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_317)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(7), NUMBER_X_3_ERR, NUMBER_Y);
            }
            else if(arg == ERROR_CASE_318)
            {
                update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_1_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2_ERR, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(8), NUMBER_X_3_ERR, NUMBER_Y);
            }
#endif
        }
        else
        {
            if(arg == 5)
            {
                x = 0;
                for(y = TEXT_S_Y; y < (TEXT_S_Y + TEXT_S_HEIGHT); y++){
                    memcpy(obd_remapped_base_address + 800*y*4 + TEXT_S_X, &text_os_s[x], TEXT_S_WIDTH*4);
                    x += TEXT_S_WIDTH*4;
                }
            }

            if((arg >= PERCENTAGE_CASE1_START) && (arg <= PERCENTAGE_CASE1_END)) // 0~29 2184
            {
                memcpy(obd_remapped_base_address + 800*PROGRESS_Y_1*4 + PROGRESS_X, img_progress_02, case1[arg]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+1)*4 + PROGRESS_X, img_progress_02, case1[arg]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+2)*4 + PROGRESS_X, img_progress_02, case1[arg]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+3)*4 + PROGRESS_X, img_progress_02, case1[arg]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+4)*4 + PROGRESS_X, img_progress_02, case1[arg]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+5)*4 + PROGRESS_X, img_progress_02, case1[arg]);
            }
            else if((arg >= PERCENTAGE_CASE2_START) && (arg <= PERCENTAGE_CASE2_END)) // 30~43(0~13)
            {
                memcpy(obd_remapped_base_address + 800*PROGRESS_Y_1*4 + PROGRESS_X, img_progress_02, case2[arg-PERCENTAGE_CASE2_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+1)*4 + PROGRESS_X, img_progress_02, case2[arg-PERCENTAGE_CASE2_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+2)*4 + PROGRESS_X, img_progress_02, case2[arg-PERCENTAGE_CASE2_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+3)*4 + PROGRESS_X, img_progress_02, case2[arg-PERCENTAGE_CASE2_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+4)*4 + PROGRESS_X, img_progress_02, case2[arg-PERCENTAGE_CASE2_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+5)*4 + PROGRESS_X, img_progress_02, case2[arg-PERCENTAGE_CASE2_START]);
            }
            else if((arg >= PERCENTAGE_CASE3_START) && (arg <= PERCENTAGE_CASE3_END)) // 44~73(0~29)
            {
                memcpy(obd_remapped_base_address + 800*PROGRESS_Y_1*4 + PROGRESS_X, img_progress_02, case3[arg-PERCENTAGE_CASE3_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+1)*4 + PROGRESS_X, img_progress_02, case3[arg-PERCENTAGE_CASE3_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+2)*4 + PROGRESS_X, img_progress_02, case3[arg-PERCENTAGE_CASE3_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+3)*4 + PROGRESS_X, img_progress_02, case3[arg-PERCENTAGE_CASE3_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+4)*4 + PROGRESS_X, img_progress_02, case3[arg-PERCENTAGE_CASE3_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+5)*4 + PROGRESS_X, img_progress_02, case3[arg-PERCENTAGE_CASE3_START]);
            }
            else if((arg >= PERCENTAGE_CASE4_START) && (arg <= PERCENTAGE_CASE4_END)) // 74~100(0~26)
            {
                memcpy(obd_remapped_base_address + 800*PROGRESS_Y_1*4 + PROGRESS_X, img_progress_02, case4[arg-PERCENTAGE_CASE4_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+1)*4 + PROGRESS_X, img_progress_02, case4[arg-PERCENTAGE_CASE4_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+2)*4 + PROGRESS_X, img_progress_02, case4[arg-PERCENTAGE_CASE4_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+3)*4 + PROGRESS_X, img_progress_02, case4[arg-PERCENTAGE_CASE4_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+4)*4 + PROGRESS_X, img_progress_02, case4[arg-PERCENTAGE_CASE4_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+5)*4 + PROGRESS_X, img_progress_02, case4[arg-PERCENTAGE_CASE4_START]);
            }
            else if((arg >= PERCENTAGE_CASE5_START) && (arg <= PERCENTAGE_CASE5_END))
            {
                memcpy(obd_remapped_base_address + 800*PROGRESS_Y_1*4 + PROGRESS_X, img_progress_02, case5[arg-PERCENTAGE_CASE5_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+1)*4 + PROGRESS_X, img_progress_02, case5[arg-PERCENTAGE_CASE5_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+2)*4 + PROGRESS_X, img_progress_02, case5[arg-PERCENTAGE_CASE5_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+3)*4 + PROGRESS_X, img_progress_02, case5[arg-PERCENTAGE_CASE5_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+4)*4 + PROGRESS_X, img_progress_02, case5[arg-PERCENTAGE_CASE5_START]);
                memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_1+5)*4 + PROGRESS_X, img_progress_02, case5[arg-PERCENTAGE_CASE5_START]);
                arg -= PERCENTAGE_CASE5_START;
            }

            /* 127 ~ 546 : 419 ~ 420 : 105 class */
            memcpy(obd_remapped_base_address + 800*PROGRESS_Y_2*4 + PROGRESS_X, img_progress_02, major_pg[arg - 1]);
            memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_2+1)*4 + PROGRESS_X, img_progress_02, major_pg[arg - 1]);
            memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_2+2)*4 + PROGRESS_X, img_progress_02, major_pg[arg - 1]);
            memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_2+3)*4 + PROGRESS_X, img_progress_02, major_pg[arg - 1]);
            memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_2+4)*4 + PROGRESS_X, img_progress_02, major_pg[arg - 1]);
            memcpy(obd_remapped_base_address + 800*(PROGRESS_Y_2+5)*4 + PROGRESS_X, img_progress_02, major_pg[arg - 1]);

            if(arg == 100)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_1, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_3, NUMBER_Y);
            }
            else if(arg == 10)
            {
                update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2, NUMBER_Y);
                update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_3, NUMBER_Y);
            }
            else if(arg > 10)
            {
                switch((arg-(arg%10))/10)
                {
                    case 0:
                    {
                        update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 1:
                    {
                        update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 2:
                    {
                        update_number(obd_remapped_base_address, NUMBER(2), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 3:
                    {
                        update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 4:
                    {
                        update_number(obd_remapped_base_address, NUMBER(4), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 5:
                    {
                        update_number(obd_remapped_base_address, NUMBER(5), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 6:
                    {
                        update_number(obd_remapped_base_address, NUMBER(6), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 7:
                    {
                        update_number(obd_remapped_base_address, NUMBER(7), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 8:
                    {
                        update_number(obd_remapped_base_address, NUMBER(8), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                    case 9:
                    {
                        update_number(obd_remapped_base_address, NUMBER(9), NUMBER_X_2, NUMBER_Y);
                    }
                    break;
                }
            }

            switch(arg%10)
            {
                case 0:
                {
                    update_number(obd_remapped_base_address, NUMBER(0), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 1:
                {
                    update_number(obd_remapped_base_address, NUMBER(1), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 2:
                {
                    update_number(obd_remapped_base_address, NUMBER(2), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 3:
                {
                    update_number(obd_remapped_base_address, NUMBER(3), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 4:
                {
                    update_number(obd_remapped_base_address, NUMBER(4), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 5:
                {
                    update_number(obd_remapped_base_address, NUMBER(5), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 6:
                {
                    update_number(obd_remapped_base_address, NUMBER(6), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 7:
                {
                    update_number(obd_remapped_base_address, NUMBER(7), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 8:
                {
                    update_number(obd_remapped_base_address, NUMBER(8), NUMBER_X_3, NUMBER_Y);
                }
                break;
                case 9:
                {
                    update_number(obd_remapped_base_address, NUMBER(9), NUMBER_X_3, NUMBER_Y);
                }
                break;
            }
        }
    }else{
        printk("\033[31m\033[1mCM3 ADDRESS is NULL\033[0m\r\n");
    }
}
/* eroum:dhkim:20140918 - end : RENEW UPDATE */
/* eroum:dhkim:20140904 - end - update-gui */
#endif //ischoi

/* eroum:dhkim:20141023 - start - error-thread */
#if 0 // ischoi
struct task_struct *ts;
#endif //#if 0 // ischoi
extern int DISABLE_NONBOOT_CPUS;

int threadloop(void *data)
{
    int count = 0;
    unsigned int readC, readP;

    while(1)
    {
        count++;
#if TRACE_UART
        dgbprintk("\033[31m\033[1mthreadloop [%d]\033[0m\r\n", count);
#endif
        msleep(1000);

        if(DISABLE_NONBOOT_CPUS == 1)
            cleanup_module();

        if(count > 15)
        {
            readC = readCurrent();
            readP = readPercentage();
#if 0 // ischoi
            if((readC != 0) && (readP != 0))
                update_draw(UNZIPBUF_1_ADDR, 800*480*4, 101);
#endif //#if 0 // ischoi
            cleanup_module();
        }

        if(kthread_should_stop())
            break;
    }    

    return 0;
}

int init_module(void)
{
#if TRACE_UART
    printk("\033[31m\033[1minit_module\033[0m\r\n");
#endif
#if 0 // ischoi
    ts = kthread_run(threadloop, NULL, "kthread");
#endif //#if 0 // ischoi
    return 0;
}

void cleanup_module(void)
{
#if TRACE_UART
    printk("\033[31m\033[1mcleanup_module\033[0m\r\n");
#endif
#if 0 // ischoi
    kthread_stop(ts);
#endif //#if 0 // ischoi
}
/* eroum:dhkim:20141023 - end - error-thread */

static int smarta_mcucpu_ioctl(struct inode *inode,/* struct file * filp,*/ unsigned int cmd, unsigned long arg)
{
//	printk(KERN_INFO "smarta_mcucpu_ioctl cmd: 0x%x  arg: 0x%x\n", cmd, (unsigned int)arg);
	
	switch(cmd)
	{
#if 1 //Eroum:hh.shin:140821 - Recovery debug
		case IOCTL_MCU_PRINTK_FUNCTION:
		{
			if(arg) {
				printk("%s",arg);
			} else {
				printk("\033[31m\033[1m[%s] arg is null\033[0m\r\n",__FUNCTION__);//salary - error
				return -1;
			}
		}
		break;
#endif  //Eroum:hh.shin:140821 - Recovery debug

/* eroum:dhkim:20140904 - start - update-gui */
        case IOCTL_MCU_UPDATE_DRAW:
#if 0 // ischoi
        {
            if ((arg == (PERCENTAGE_CASE1_START + 1)) || (arg == (PERCENTAGE_CASE2_START + 1)) || (arg == (PERCENTAGE_CASE3_START + 1)) || (arg == (PERCENTAGE_CASE4_START + 1))) {
                void *cmd_remapped_base_address;
              
                cmd_remapped_base_address = (void *)ioremap_nocache(OBD_CMD_ADDR, 1);
                *(volatile unsigned int*)cmd_remapped_base_address = 0;
                printk("\033[31m\033[1mobd_remapped_base_address is 0\033[0m\r\n");
            }
            update_draw(UNZIPBUF_1_ADDR, 800*480*4, arg);
/* eroum:dhkim:20141023 - start - error-thread */
            if(arg == (PERCENTAGE_CASE4_START + PERCENTAGE_CASE4_OFFSET))
            {
                init_module();
/* eroum:dhkim:20141103 - start
   indi-mmc */
#if defined(CONFIG_FINE_MMC_INDI)
                init_tThread();
#endif /* CONFIG_FINE_MMC_INDI */
/* eroum:dhkim:20141103 - end
   indi-mmc */
            }
/* eroum:dhkim:20141023 - end - error-thread */
        }
#endif //#if 0 // ischoi
        break;

        case IOCTL_MCU_SET_USSTATUS:
        {
#if 0 // ischoi
            if(arg){
                dgbprintk("\033[31m\033[1mIOCTL_MCU_SET_USSTATUS=[%d]\033[0m\r\n", arg);
                ((PPMU)tcc_p2v(HwPMU_BASE))->PMU_USSTATUS.nREG = arg;
            }
#endif //#if 0 // ischoi
        }
        break;

        case IOCTL_MCU_CURR_PERCENTAGE:
        {
            int gCurrPer = mPercentage;

#if 0 // ischoi
            dgbprintk("\033[31m\033[1mIOCTL_MCU_CURR_PERCENTAGE=[%d]\033[0m\r\n", gCurrPer);
            if(copy_to_user(arg, &gCurrPer, sizeof(int)))
                return -EFAULT;
#endif //#if 0 // ischoi
        }
        break;

        case IOCTL_MCU_PRINTF:
        {
#if 0 // ischoi
            dgbprintk("\033[31m\033[1m[MSG]=[%d]\033[0m\r\n", arg);
#endif //#if 0 // ischoi
        }
        break;        
/* eroum:dhkim:20140904 - end - update-gui */

/* eroum dhkim - sdcard detect */
        case IOCTL_MCU_DETECT_SDCARD:
        {
#if 0 // ischoi
            int gSDCard = gpio_get_value(SD0_CD);

            dgbprintk("\033[31m\033[1mIOCTL_MCU_DETECT_SDCARD=[%d]\033[0m\r\n", gSDCard);
            if(copy_to_user(arg, &gSDCard, sizeof(int)))
                return -EFAULT;
#endif //#if 0 // ischoi
        }
        break;
/* eroum dhkim - sdcard detect */

/* eroum dhkim - start : auto-recovery */
        case IOCTL_MCU_UPDATE_BOOTCOUNT:
        {
#if 0 // ischoi
            dgbprintk("\033[31m\033[1mIOCTL_MCU_UPDATE_BOOTCOUNT=[%d]\033[0m\r\n", arg);
#endif //#if 0 // ischoi
            // TODO:]
#ifdef AUTOUPDATE_RENEWAL
#else
#if COMPLETEBOOTING
            if(nBootFlg)
            {
                if(arg == 1)
                {
                    Set_Count(0);
                    Set_Update();
                    printk("boot fail count = %d\n",arg+1);
                }
                else if(arg == 0)
                {
                    Set_Count(1);
                    printk("boot fail count = %d\n",arg+1);
                }
            }
            else
            {
                if(arg == 1)
                {
                    Set_Count(0);
                    printk("boot fail count = 0\n");
                }
            }
#endif
#endif
        }
        break;

        case IOCTL_MCU_AUDIO_SHUTDOWN:
        {
#if 0 // ischoi
            extern void rt5631_power_off(void);
            dgbprintk("\033[31m\033[1mIOCTL_MCU_AUDIO_SHUTDOWN\033[0m\r\n");
            rt5631_power_off();
#endif //#if 0 // ischoi
        }
        break;
/* eroum dhkim - end : auto-recovery */

		case IOCTL_MCU_FORCE_REPAIR:
		{
			printk(KERN_INFO "IOCTL_MCU_FORCE_REPAIR\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0x3c);
			uart_putc(MCU_UART_PORT, 0xc3);
		}
		break;
		case IOCTL_MCU_FORCE_REPAIR_OK:
		{
			printk(KERN_INFO "IOCTL_MCU_FORCE_REPAIR_OK\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0x2d);
			uart_putc(MCU_UART_PORT, 0xd2);
		}
		break;
		case IOCTL_MCU_POWER_DOWN:
		{
			printk(KERN_INFO "IOCTL_MCU_POWER_DOWN\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0x77);
			uart_putc(MCU_UART_PORT, 0x88);
		}
		break;
		case IOCTL_MCU_BOOT_SUCCESS:
		{
			printk(KERN_INFO "IOCTL_MCU_BOOT_SUCCESS\n");
#if 0 // ischoi
			extern int tw9900_boot_complete;
			//void check_first_rear_camera_state(void);

			tw9900_boot_complete = 1; //salary for rear camera
#endif //#if 0 // ischoi
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0xcc);
			uart_putc(MCU_UART_PORT, 0x33);

			//smarta_mcucpu_send_dc_state_req();
			//send_dummy_touch_position(800, 40);

			//check rear camera in boot time
			//check_first_rear_camera_state();
			
#if defined(REBOOT_TEST)
			//reboot test
			init_timer(&restart_ts_timer);
			restart_ts_timer.function = restart_ts_timer_handler;
			restart_ts_timer.expires = jiffies+msecs_to_jiffies(10000);
			printk("System reboot in 10 secs\r\n");
			add_timer(&restart_ts_timer);
#endif //REBOOT_TEST			
		}
		break;
		case IOCTL_MCU_VER_CHECK:
		{
			printk(KERN_INFO "IOCTL_MCU_VER_CHECK\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0xbb);
			uart_putc(MCU_UART_PORT, 0x44);
#if	MCUVERSION
			msleep(100);
			return nMCU_Ver;
#endif
		}
		break;
		case IOCTL_MCU_MESSAGE_ACK:
		{
			printk(KERN_INFO "IOCTL_MCU_MESSAGE_ACK\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0x99);
			uart_putc(MCU_UART_PORT, 0x66);
		}
		break;
		case IOCTL_MCU_MESSAGE_RECALL:
		{
			printk(KERN_INFO "IOCTL_MCU_MESSAGE_RECALL\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0xaa);
			uart_putc(MCU_UART_PORT, 0x55);
		}
		break;
		case IOCTL_MCU_CPU_RESET:
		{
			printk(KERN_INFO "IOCTL_MCU_CPU_RESET\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0x85);
			uart_putc(MCU_UART_PORT, 0x7a);
		}
		break;
		case IOCTL_POWER_SHORT_KEY_CHECK:
		{
#if 0 // ischoi
			gPowerShortKey = gpio_get_value(MCU_FR);
#endif //#if 0 // ischoi
			printk(KERN_INFO "gPowerShortKey   %d\n", gPowerShortKey);
			if (copy_to_user(arg, &gPowerShortKey, sizeof(gPowerShortKey)))
				return -EFAULT;
		}
		break;
		case IOCTL_FIRMWARE_UPDATE:
		{
			//kernel_restart("recovery");
		}
		break;
		case IOCTL_MCU_AUTO_RECOVERY:
		{
			#define TCC_IOBUSCFG_BASE	0xF05F5000
			#define IOBUSCFG_HCLKEN0		(TCC_IOBUSCFG_BASE + 0x10)
			#define IOBUSCFG_HCLKEN1		(TCC_IOBUSCFG_BASE + 0x14)
			#define TCC_PMU_BASE			0xf0404000
			#define PMU_WATCHDOG		(TCC_PMU_BASE + 0x0C)
			#define PMU_CONFIG1			(TCC_PMU_BASE + 0x14)

			printk( "IOCTL_MCU_AUTO_RECOVERY \n");
#if 0 // ischoi
			tcc_writel((tcc_readl(PMU_CONFIG1) & 0xFFFFFF00) | 3, PMU_CONFIG1);
			tcc_writel(0xFFFFFFFF, IOBUSCFG_HCLKEN0);
			tcc_writel(0xFFFFFFFF, IOBUSCFG_HCLKEN1);

			while (1) {
				tcc_writel((1 << 31) | 1, PMU_WATCHDOG);
			}
#endif //#if 0 // ischoi
			break;
		}
		//hh20140602
		case IOCTL_MCU_ALIVE_ACK_CHANGE:
		{			
			printk(KERN_INFO "IOCTL_MCU_ALIVE_ACK_CHANGE\n");
			if(g_bAliveAckFlag){
				g_bAliveAckFlag = false;
				printk(KERN_INFO " IOCTL_MCU_ALIVE_ACK_CHANGE  ACK is (OFF)\r\n");
			}
			else
			{
				g_bAliveAckFlag = true;	
				printk(KERN_INFO " IOCTL_MCU_ALIVE_ACK_CHANGE  ACK is (ON)\r\n");			
			}
			break;//*/
		}
		//Finedigital:hh:20140710 MCU Watchdog Disable
		case IOCTL_MCU_ALIVE_DISABLE:		
		{						
			printk(KERN_INFO "IOCTL_MCU_ALIVE_DISABLE\n");
			uart_putc(MCU_UART_PORT, CPU_HEADER);
			uart_putc(MCU_UART_PORT, 0x73);
			uart_putc(MCU_UART_PORT, 0x8C);		
		}
		break;//*/	
		/* Finedigital:hh:20140924 - start
		   Booting Fail Check */	
#if COMPLETEBOOTING
		case IOCTL_MCU_BOOTWAIT_CHANGE:		
		{						
			printk(KERN_INFO "IOCTL_MCU_BOOTWAIT_CHANGE\n");
			nBootFlg = 0;			
#ifdef AUTOUPDATE_RENEWAL
			if(Get_Count()==1)				
				Set_Count(0);
#else
				Get_Count();
#endif
		}
		break;
		/* end */
#endif
		case 0xff:
		{
			char text[255] = {0, };
			if (copy_from_user(text, arg, sizeof(text)))
				return -EFAULT;
			printk(KERN_INFO "[text] %s\n", text);
		}
		break;
		default:
		{
			printk(KERN_INFO "[MCU] Not Supported Code\n");
	
		}
		break;

		
	}

	return 0;
}

/* eroum:dhkim:20140722 - start
   touch LCD ON */
void SendDummyKey(void)
{
    if(!gInputDev) {
        printk("Keypad driver not initialize yet\r\n");
        return;
    }

#if 0 // ischoi
    smarta_mcucpu_send_key_code(KEY_IQ9000V_LCDON, KEY_PRESSED);
    smarta_mcucpu_send_key_code(KEY_IQ9000V_LCDON, KEY_RELEASED);
#endif //#if 0 // ischoi
}
EXPORT_SYMBOL(SendDummyKey);
/* eroum:dhkim:20140722 - end
   touch LCD ON */

static int smarta_mcucpu_open(struct inode *inode, struct file *flip)
{
//	printk(KERN_INFO "smarta_mcucpu_open\n");
	
	return 0;
}

static int smarta_mcucpu_release(struct inode *inode, struct file *filp)
{
//	printk(KERN_INFO "smarta_mcucpu_release\n");
	
	return 0;
}

static int smarta_mcucpu_uart_init(void)
{
	int result;
	char description[] = {"smarta-mcucpu"};

#if 0 // ischoi
	tcc_gpio_config(MCU_TXD, GPIO_FN(6));
	tcc_gpio_config(MCU_RXD, GPIO_FN(6));
	spin_lock_init(&gLock);

	result = request_irq(INT_UART6, smarta_mcucpu_interrupt_handler, IRQF_SHARED, "mcucpu", &gLock);

	if (result < 0) {
		printk("[MCU]unable to request ERROR IRQ\n");
	}

	PRINT_UART("smarta_mcucpu_uart_init result = %d\r\n",result);

	tcc_gpio_input_buffer_set(MCU_TXD, 1);
	tcc_gpio_input_buffer_set(MCU_RXD, 1);
	uart_init_port(MCU_UART_PORT, MCU_UART_BAUD);
#endif //#if 0 // ischoi
	
	return 0;
}

static int smarta_mcucpu_key_event_init(void)
{
	int i;
	int ret = 0;

	gInputDev = input_allocate_device();
	gInputDev->evbit[0] = BIT(EV_KEY);
	gInputDev->name = "smarta mcu cpu key event";

	for (i = 0; i < ARRAY_SIZE(gKeyVCodeTable); i++)
		set_bit(gKeyVCodeTable[i] & KEY_MAX, gInputDev->keybit);

	ret = input_register_device(gInputDev);

	init_timer(&gKeyCheckTimer);
	gKeyCheckTimer.function = smarta_mcucpu_input_repeat_key;
	
	return ret;
}

void smarta_mcucpu_send_suspend_started(void)
{
	printk(KERN_INFO "smarta_mcucpu_send_suspend_started\n");
	
	uart_putc(MCU_UART_PORT, CPU_HEADER);
	uart_putc(MCU_UART_PORT, 0xf1);
	uart_putc(MCU_UART_PORT, 0x0e);
}

void smarta_mcucpu_send_suspend_done(void)
{
	printk(KERN_INFO "smarta_mcucpu_send_suspend_done\n");
	
	uart_putc(MCU_UART_PORT, CPU_HEADER);
	uart_putc(MCU_UART_PORT, 0xf2);
	uart_putc(MCU_UART_PORT, 0x0d);
}

void smarta_mcucpu_send_wakeup_done(void)
{
	printk(KERN_INFO "smarta_mcucpu_send_wakeup_done\n");
	
	uart_putc(MCU_UART_PORT, CPU_HEADER);
	uart_putc(MCU_UART_PORT, 0xf4);
	uart_putc(MCU_UART_PORT, 0x0b);
}

void smarta_mcucpu_send_dc_state_req(void)
{
	printk(KERN_INFO "smarta_mcucpu_send_dc_state_req\n");
	
	uart_putc(MCU_UART_PORT, CPU_HEADER);
	uart_putc(MCU_UART_PORT, 0xe1);
	uart_putc(MCU_UART_PORT, 0x1e);
}

void smarta_mcucpu_send_power_off(void)
{
	printk(KERN_INFO "smarta_mcucpu_send_power_off\n");
	
	uart_putc(MCU_UART_PORT, CPU_HEADER);
	uart_putc(MCU_UART_PORT, 0x77);
	uart_putc(MCU_UART_PORT, 0x88);
}


#ifdef CONFIG_SMARTA_BATTERY

static int smarta_battery_init(struct platform_device *pdev)
{
	int i, err;

#if 0 // ischoi
	//battery charge enable
	gpio_request(CHARGE_EN, "CHRG_EN");
	tcc_gpio_config(CHARGE_EN, GPIO_FN(0));
	gpio_direction_output(CHARGE_EN, 0);
	
	gpio_request(BAT_CHG, "BAT_CHG");
	tcc_gpio_config(BAT_CHG, GPIO_FN(0));
	gpio_direction_input(BAT_CHG);
	
	mutex_init(&smarta_batt_info.lock);
	for (i = 0; i < ARRAY_SIZE(smarta_power_supplies); i++)
	{
		err = power_supply_register(&pdev->dev, &smarta_power_supplies[i]);
		if(err)
			printk(KERN_ERR "Failed to register power supply (%d)\n", err);
	}

	smarta_battery_create_attrs(smarta_power_supplies[CHARGER_BATTERY].dev);

	// Get current Battery info
	if (smarta_get_battery_info(&smarta_batt_info.rep) < 0)
		printk(KERN_ERR "%s: get info failed\n", __FUNCTION__);

	smarta_batt_info.present = 1;
	smarta_batt_info.pdev   = pdev;
	smarta_batt_info.update_time = jiffies;

	smarta_cable_status_update(CHARGER_AC);

#endif //#if 0 // ischoi
	return 0;
}

static int smarta_battery_get_charging_status(void)
{
	int ret;
	charger_type_t charger;
	u32 level;

	mutex_lock(&smarta_batt_info.lock);
	charger = smarta_batt_info.rep.charging_source;

	switch (charger)
	{
	case CHARGER_BATTERY:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case CHARGER_USB:
	case CHARGER_AC:
		level = smarta_batt_info.rep.level;
		if (level == 100)
			ret = POWER_SUPPLY_STATUS_FULL;
		else
			ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
		
	}
	mutex_unlock(&smarta_batt_info.lock);

	return ret;
}

static int smarta_get_battery_info(struct battery_info_reply *buffer)
{
	struct battery_info_reply info;
//	int rc;

	if (buffer == NULL) 
		return -EINVAL;

	smarta_read_battery(&info);
                             
	//mutex_lock(&smarta_batt_info.lock);
	buffer->batt_id                 = (info.batt_id);
	buffer->batt_vol                = (info.batt_vol);
	buffer->batt_temp               = (info.batt_temp);
	buffer->batt_current            = (info.batt_current);
	buffer->level                   = (info.level);
	/* Move the rules of charging_source to cable_status_update. */
	/* buffer->charging_source      = be32_to_cpu(rep.info.charging_source); */
	buffer->charging_enabled        = (info.charging_enabled);
	buffer->full_bat                = (info.full_bat);
	//mutex_unlock(&smarta_batt_info.lock);
	
	return 0;
}

static int smarta_battery_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smarta_battery_get_charging_status();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval =  smarta_batt_info.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mutex_lock(&smarta_batt_info.lock);
		val->intval = smarta_batt_info.rep.level;
		mutex_unlock(&smarta_batt_info.lock);
		break;
	default:		
		return -EINVAL;
	}
	
	return 0;
}

static int smarta_battery_create_attrs(struct device * dev)
{
	int i, rc;
	for (i = 0; i < ARRAY_SIZE(smarta_battery_attrs); i++) {
		rc = device_create_file(dev, &smarta_battery_attrs[i]);
		if (rc)
			goto tcc_attrs_failed;
	}

	goto succeed;
     
tcc_attrs_failed:
	while (i--)
		device_remove_file(dev, &smarta_battery_attrs[i]);

succeed:        
	return rc;
}

static ssize_t smarta_battery_show_property(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int i = 0;
	const ptrdiff_t off = attr - smarta_battery_attrs;

	mutex_lock(&smarta_batt_info.lock);
	/* check cache time to decide if we need to update */
	if (smarta_batt_info.update_time &&
		time_before(jiffies, smarta_batt_info.update_time + msecs_to_jiffies(cache_time)))
		goto dont_need_update;

	if (smarta_get_battery_info(&smarta_batt_info.rep) < 0) {
		printk(KERN_ERR "%s: rpc failed!!!\n", __FUNCTION__);
	} else {
		smarta_batt_info.update_time = jiffies;
	}
dont_need_update:
	mutex_unlock(&smarta_batt_info.lock);

	mutex_lock(&smarta_batt_info.lock);
	switch (off) {
	case BATT_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   smarta_batt_info.rep.batt_id);
		break;
	case BATT_VOL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   smarta_batt_info.rep.batt_vol);
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   smarta_batt_info.rep.batt_temp);
		break;
	case BATT_CURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   smarta_batt_info.rep.batt_current);
		break;
	case CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   smarta_batt_info.rep.charging_source);
		break;
	case CHARGING_ENABLED:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   smarta_batt_info.rep.charging_enabled);
		break;
	case FULL_BAT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   smarta_batt_info.rep.full_bat);
		break;
	default:
		i = -EINVAL;
	}
	mutex_unlock(&smarta_batt_info.lock);

	return i;
}

static int smarta_power_get_property(struct power_supply *psy, 
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	charger_type_t charger;
	
	mutex_lock(&smarta_batt_info.lock);
	charger = smarta_batt_info.rep.charging_source;
	mutex_unlock(&smarta_batt_info.lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = (charger ==  CHARGER_AC ? 1 : 0);
		else if (psy->type == POWER_SUPPLY_TYPE_USB)
		{
//#ifdef CONFIG_MACH_TCC8800              // 101125 jckim temporary code for preventing to enter deep sleep
#if (0)
			val->intval = 1;
#else
			val->intval = (charger ==  CHARGER_USB ? 1 : 0);
#endif
		}
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


#endif

static int __devinit smarta_mcucpu_probe(struct platform_device *pdev)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);

#if 0 // ischoi
	gpio_request(MCU_FR, "MCU_FR");
	tcc_gpio_config(MCU_FR, GPIO_FN(0));
	gpio_direction_input(MCU_FR);

#ifdef CONFIG_SMARTA_BATTERY
	smarta_battery_init(pdev);
#endif
	smarta_mcucpu_uart_init();
	smarta_mcucpu_key_event_init();

#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.suspend = smarta_mcucpu_early_suspend;
	early_suspend.resume 	= smarta_mcucpu_late_resume;
	early_suspend.level 	= EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&early_suspend);
#endif
#endif //#if 0 // ischoi

	return 0;
}

static int smarta_mcucpu_remove(struct platform_device *pdev)
{
	int i;
	printk(KERN_INFO "%s\n", __FUNCTION__);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif	
	input_unregister_device(gInputDev);
	del_timer(&gKeyCheckTimer);

#if 0 // ischoi
	free_irq(INT_UART6, &gLock);
#endif //#if 0 // ischoi

#ifdef CONFIG_SMARTA_BATTERY
	mutex_destroy(&smarta_batt_info.lock);
	for (i = 0; i < ARRAY_SIZE(smarta_battery_attrs); i++) 
	{
		device_remove_file(smarta_power_supplies[CHARGER_BATTERY].dev, &smarta_battery_attrs[i]);
	}
	for (i = 0; i < ARRAY_SIZE(smarta_power_supplies); i++) 
	{
		power_supply_unregister(&smarta_power_supplies[i]);
	}
#endif
	
	
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

static void smarta_mcucpu_early_suspend(struct early_suspend *h)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);

#ifdef CONFIG_SMARTA_BATTERY
	smarta_mcucpu_send_suspend_started();
#endif
}
// jhhong : 2014.02.26 (Quick Boot Issue)
#endif	

static int smarta_mcucpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);

#ifdef CONFIG_SMARTA_BATTERY	
	smarta_mcucpu_send_suspend_done();
#endif
	
	return 0;
}
// jhhong : 2014.02.26 (Quick Boot Issue)
//#endif

static int smarta_mcucpu_resume(struct platform_device *pdev)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	
#ifdef CONFIG_SMARTA_BATTERY
		smarta_mcucpu_send_wakeup_done();
		smarta_mcucpu_send_dc_state_req();
#endif

// jhhong : 2014.02.26  (QB)
#if defined(CONFIG_HIBERNATION)
	if( do_hibernate_boot) 
	{
		gpio_request(MCU_FR, "MCU_FR");
		tcc_gpio_config(MCU_FR, GPIO_FN(0));
		gpio_direction_input(MCU_FR);
			
#ifdef CONFIG_SMARTA_BATTERY
		smarta_battery_init(pdev);
#endif
		smarta_mcucpu_uart_init();
		smarta_mcucpu_key_event_init();
			
	}
#endif	

	smarta_mcucpu_send_dummy_key_event();

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void smarta_mcucpu_late_resume(struct early_suspend *h)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);

#ifdef CONFIG_SMARTA_BATTERY
	smarta_mcucpu_send_wakeup_done();
	smarta_mcucpu_send_dc_state_req();
#endif
}
#endif

static struct platform_driver smarta_mcucpu_driver = {
       .probe          = smarta_mcucpu_probe,
       .remove         = smarta_mcucpu_remove,
// jhhong : 2014.02.26 (해당 부분이 Define 되지 않아 Qb mode로 진입시 Suspend / Resume 실행도지 않음
// 그래서 해당 Define 주석 막음.
//#ifdef CONFIG_HAS_EARLYSUSPEND
       .suspend        = smarta_mcucpu_suspend,
       .resume         = smarta_mcucpu_resume,
//#endif
       .driver		= {
		.owner	= THIS_MODULE,
		.name	= "smarta-mcucpu",
	},
};


static struct class *smarta_mcucpu_class;
static struct wake_lock mcu_device_wake_lock;


#if defined(REMOCON_PACKET_TIMEOUT) //salary - apply timeout control for Remocon UART packet
static void mcucpu_uart_packet_timer_handler(unsigned long data)
{
	mcucpu_uart_packet_timer_is_expired = 1;
	if(rxStatus != eRXHeader) {
		rxStatus = eRXHeader;
		check = group = checksum = dataSize = 0;
		PRINT_UART("initialize parser variable\r\n");
	}
#ifdef REMOCON_POWER_LONG_OFF
	rep_cnt = 0;
	pwr_press = 0;
#endif
	PRINT_UART("mcu uart timeout\r\n");
}
#endif //salary - apply timeout control for Remocon UART packet


#if defined(BOTTOM_THREAD_TYPE) //salary - process MCU UART packet in bottom thread when it has received packet
static void mcucpu_uart_packet_fetch_thread(struct work_struct *work)
{
	int data = -1;

#if !defined(REMOCON_PACKET_TIMEOUT) //salary - apply timeout control for Remocon UART packet
	static eRX_STATUS rxStatus = eRXHeader;
	static UARTPacket curRxPacket;
	static int check = 0;
	static int group = 0, checksum = 0, dataSize = 0;
#endif //salary - apply timeout control for Remocon UART packet

	while((data = mcu_cpu_get_packet()) != -1)
	{
#if 0 // ischoi
		extern int tw9900_boot_complete;
		if(!tw9900_boot_complete)
			continue;
#endif //#if 0 // ischoi
	
		PRINT_UART("smarta_mcucpu_interrupt_handler  0x%x\n", data);
		
		switch(rxStatus)
		{
			case eRXHeader:
				curRxPacket.header = (char)data;
				group = (curRxPacket.header & 0xf0) >> 4;
				checksum = (curRxPacket.header & 0x0c) >> 2;
				dataSize = curRxPacket.header & 0x03;
				if(!dataSize)
					dataSize = 4;
				rxStatus = eRXData;
				break;
			case eRXData:
				if(check > 3) {
					printk("check error\r\n\n\n\n\n\n\n\n");
					check = 0;
				}
				curRxPacket.data[check++] = (char)data;
				if(check == dataSize-1)
				{
					smarta_mcucpu_packet_parser(&curRxPacket);
					rxStatus = eRXHeader;
					check = 0;
				}
				break;
		}
	}
}
#endif //BOTTOM_THREAD_TYPE //salary - process MCU UART packet in bottom thread when it has received packet

static int __init smarta_mcucpu_init(void)
{
	int result;

	printk(KERN_INFO "smarta_mcucpu_init\n");
	g_bAliveAckFlag = true; 

	MajorNum = register_chrdev(0, SMARTA_MCUCPU_DEV_NAME, &smarta_mcucpu_fops);//register char device
	if(MajorNum < 0)
	{
		printk("[%s] smarta_mcucpu comm device register error![%d]\r\n", __FUNCTION__, MajorNum);
		return -1;
	}

	smarta_mcucpu_class = class_create(THIS_MODULE, SMARTA_MCUCPU_DEV_NAME);
	device_create(smarta_mcucpu_class, NULL, MKDEV(MajorNum,0), NULL, SMARTA_MCUCPU_DEV_NAME);

#if defined(REMOCON_PACKET_TIMEOUT) //salary - apply timeout control for Remocon UART packet
	init_timer(&mcucpu_uart_packet_timer);
	mcucpu_uart_packet_timer.function = mcucpu_uart_packet_timer_handler;
#endif //salary - apply timeout control for Remocon UART packet

#if defined(BOTTOM_THREAD_TYPE) //salary - process MCU UART packet in bottom thread when it has received packet
		INIT_WORK(&mcucpu_uart_packet_work_q, mcucpu_uart_packet_fetch_thread);
#endif //BOTTOM_THREAD_TYPE //salary - process MCU UART packet in bottom thread when it has received packet

	result = platform_driver_register(&smarta_mcucpu_driver);
	if(result)
	{
		printk("fail : platrom driver %s (%d) \n", smarta_mcucpu_driver.driver.name, result);
		return result;
	}


	wake_lock_init(&mcu_device_wake_lock, WAKE_LOCK_SUSPEND, "mcu_wake_lock");
	wake_lock(&mcu_device_wake_lock);

	return 0;
}

static void __exit smarta_mcucpu_exit(void)
{
	printk(KERN_INFO "smarta_mcucpu_exit\n");

	device_destroy(smarta_mcucpu_class, MKDEV(MajorNum, 0));
	class_destroy(smarta_mcucpu_class);
	unregister_chrdev(MajorNum, SMARTA_MCUCPU_DEV_NAME);
	platform_driver_unregister(&smarta_mcucpu_driver);
}

module_init(smarta_mcucpu_init);
module_exit(smarta_mcucpu_exit);


MODULE_AUTHOR("dw.choi@e-roum.com");
MODULE_DESCRIPTION("SmartA MCU CPU Communication Module");
MODULE_LICENSE("GPL");


