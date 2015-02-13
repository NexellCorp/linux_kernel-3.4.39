/*
 * Based on linux/drivers/serial/pxa.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

//////////////////////////////////////////
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
//#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/gpio.h>
//#include <linux/android_pmem.h>
//#include <pthread.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/err.h>
/////////////////////////////////////////

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial_reg.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/gpio.h>

#include <cfg_gpio.h>


#include <mach/platform.h>
#include <mach/devices.h>
#include "spi-xr20m1172.h"



#define spi_port_num 1 //2

#define XR20M1172_CLOCK			14745600 //24000000//14709800 
#define PRESCALER 				2 
#define PORT_BAUD_RATE			9600 
#define SAMPLE_RATE				8 //16 


static struct mxc_xr20m xr20m_drv_data[spi_port_num];

//static atomic_t recv_timeout = ATOMIC_INIT(0);

#define SPI_WORD_LENGTH  1

dev_t devno_0 ;
dev_t devno_1 ;


struct class *xrm_class_0;
struct class *xrm_class_1;

static struct task_struct *spi_task;

int flag_queue_ready = 0;

DECLARE_WAIT_QUEUE_HEAD(read_wq);

#define RD_BUX_MAX     2048
unsigned char rx_buf_0[RD_BUX_MAX];
unsigned char rx_buf_1[RD_BUX_MAX];

unsigned int rx_tail_0 = 0;
unsigned int rx_head_0 = 0;
unsigned int rx_tail_1 = 0;
unsigned int rx_head_1 = 0;

unsigned int rx_buf_count_0 = 0;
unsigned int rx_buf_count_1 = 0;

spinlock_t rd_buf_lock_0; //= SPIN_LOCK_UNLOCKED;
spinlock_t rd_buf_lock_1; //= SPIN_LOCK_UNLOCKED

//static DEFINE_SPINLOCK(spi_port_lock);
struct semaphore spi_port_sem;


static int spi_rw(struct spi_device *spi, u8 *tx, u8 *rx, int len)
{
	struct spi_message spi_msg;
	struct spi_transfer spi_xfer;
	int real_len;
	int ret;

	
	real_len = len;
	
	/* Initialize SPI ,message */
	spi_message_init(&spi_msg);

	/* Initialize SPI transfer */
	memset(&spi_xfer, 0, sizeof spi_xfer);
	spi_xfer.len = real_len;
	spi_xfer.tx_buf = tx;
	spi_xfer.rx_buf = rx;
	spi_xfer.speed_hz = 4 * 1000 * 1000 ; //MAX3107_SPI_SPEED;

	/* Add SPI transfer to SPI message */
	spi_message_add_tail(&spi_xfer, &spi_msg);

	/* Perform synchronous SPI transfer */
	//spin_lock(&spi_port_lock);
	//down_interruptible(&spi_port_sem);
	ret = spi_sync(spi, &spi_msg);
	//spin_unlock(&spi_port_lock);
	//up(&spi_port_sem);
	if (ret) {
		printk("spi_sync failure\n");
		return -EIO;
	}

	return 0;
}


int xr20m_spi_setup(struct spi_device *spi)
{
    int xr20m;
    xr20m =(int)spi->dev.platform_data;
    xr20m_drv_data[xr20m].spi = spi;
	
    
    spi->mode = SPI_MODE_0 ;
    spi->bits_per_word = (SPI_WORD_LENGTH * 8);

    return spi_setup(spi);

}


unsigned char SPI_ReadReg(unsigned char devid, unsigned char offset )
{


	unsigned char command[2];
	unsigned char reg_val[2];

	if( devid > 1 ) {
		printk(" dev id error %d \n", devid);
		return 0;
	}

	if (SPI_WORD_LENGTH == 1){
		command[0] = (0x80 | offset | (devid << 1));
		command[1] = 0;
		spi_rw(xr20m_drv_data[0].spi, (u8 *)command, (u8 *)reg_val , 2);
		
		return reg_val[1];
	}	
	else {
		command[0] = (0x80 | offset | (devid << 1));
		command[1] = 0;
		spi_rw(xr20m_drv_data[0].spi, (u8 *)command, reg_val , 2);

		return reg_val[1];
	}


}



//int pmic_write(int reg_num, unsigned int reg_val)
void SPI_WriteReg(unsigned char devid, unsigned char offset,unsigned char value)
{

	unsigned char command[3];

	if( devid > 1 ) {
		printk(" dev id error %d \n", devid);
		return;
	}

	if (SPI_WORD_LENGTH == 1){
		//command[0] = ( offset | (devid << 1));
		//command[1] =  value;
		command[0] = ( offset | (devid << 1));
		command[1] =  value;
		command[2] = value;
		
		spi_rw(xr20m_drv_data[0].spi, (u8 *)command, NULL , 2);
	}	
	else {
		command[0] = (offset | (devid << 1));
		command[1] = value;
		spi_rw(xr20m_drv_data[0].spi, (u8 *)command, NULL , 1*SPI_WORD_LENGTH);
	}


}




/////////////////////////////////////////////////////////////////////////////////////////////

static u8 cached_lcr[2];
static u8 cached_efr[2];
static u8 cached_mcr[2];
//static spinlock_t xr20m1172_lock = SPIN_LOCK_UNLOCKED;
//static unsigned long xr20m1172_flags;



/*
 * meaning of the pair:
 * first: the subaddress (physical offset<<3) of the register
 * second: the access constraint:
 * 10: no constraint
 * 20: lcr[7] == 0
 * 30: lcr == 0xbf
 * 40: lcr != 0xbf
 * 50: lcr[7] == 1 , lcr != 0xbf, efr[4] = 1
 * 60: lcr[7] == 1 , lcr != 0xbf,
 * 70: lcr != 0xbf,  and (efr[4] == 0 or efr[4] =1, mcr[2] = 0)
 * 80: lcr != 0xbf,  and (efr[4] = 1, mcr[2] = 1)
 * 90: lcr[7] == 0, efr[4] =1
 * 100: lcr!= 0xbf, efr[4] =1
 * third:  1: readonly
 * 2: writeonly
 * 3: read/write
 */
static const int reg_info[27][3] = {
	{0x0, 20, 1},		//RHR
	{0x0, 20, 2},		//THR
	{0x0, 60, 3},		//DLL
	{0x8, 60, 3},		//DLM
	{0x10, 50, 3},		//DLD
	{0x8, 20, 3},		//IER:bit[4-7] needs efr[4] ==1,but we dont' access them now
	{0x10, 20, 1},		//ISR:bit[4/5] needs efr[4] ==1,but we dont' access them now
	{0x10, 20, 2},		//FCR :bit[4/5] needs efr[4] ==1,but we dont' access them now
	{0x18, 10, 3},		//LCR
	{0x20, 40, 3},		//MCR :bit[2/5/6] needs efr[4] ==1
	{0x28, 40, 1},		//LSR
	{0x30, 70, 1},		//MSR
	{0x38, 70, 3},		//SPR
	{0x30, 80, 3},		//TCR
	{0x38, 80, 3},		//TLR
	{0x40, 20, 1},		//TXLVL
	{0x48, 20, 1},		//RXLVL
	{0x50, 20, 3},		//IODir
	{0x58, 20, 3},		//IOState
	{0x60, 20, 3},		//IOIntEna
	{0x70, 20, 3},		//IOControl
	{0x78, 20, 3},		//EFCR
	{0x10, 30, 3},		//EFR
	{0x20, 30, 3},		//Xon1
	{0x28, 30, 3},		//Xon2
	{0x30, 30, 3},		//Xoff1
	{0x38, 30, 3},		//Xoff2
};



struct xr20m1172_port *xrm_spi_port[2]; 



static void EnterConstraint(unsigned char devid, unsigned char regaddr)
{
	switch (reg_info[regaddr][1]) {
			//10: no contraint
		case 20:		//20: lcr[7] == 0
			if (cached_lcr[devid] & BIT7)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid] & ~BIT7);
		break;
		case 30:		//30: lcr == 0xbf
			if (cached_lcr[devid] != 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0xbf);
		break;
		case 40:		//40: lcr != 0xbf
			if (cached_lcr[devid] == 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0x3f);
		break;
		case 50:		//50: lcr[7] == 1 , lcr != 0xbf, efr[4] = 1
			if (!(cached_efr[devid] & BIT4)) {
				if (cached_lcr[devid] != 0xbf)
					SPI_WriteReg(devid,
						     reg_info[XR20M1170REG_LCR][0],
						     0xbf);
				SPI_WriteReg(devid, reg_info[XR20M1170REG_EFR][0],
					     cached_efr[devid] | BIT4);
			}
			if ((cached_lcr[devid] == 0xbf)
			    || (!(cached_lcr[devid] & BIT7)))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     (cached_lcr[devid] | BIT7) & ~BIT0);
		break;
		case 60:		//60: lcr[7] == 1 , lcr != 0xbf,
			if ((cached_lcr[devid] == 0xbf)
			    || (!(cached_lcr[devid] & BIT7)))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     (cached_lcr[devid] | BIT7) & ~BIT0);
		break;
		case 70:		//lcr != 0xbf,  and (efr[4] == 0 or efr[4] =1, mcr[2] = 0)
			if (cached_lcr[devid] == 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0x3f);
			if ((cached_efr[devid] & BIT4) && (cached_mcr[devid] & BIT2))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_MCR][0],
					     cached_mcr[devid] & ~BIT2);
		break;
		case 80:		//lcr != 0xbf,  and (efr[4] = 1, mcr[2] = 1)
			if (cached_lcr[devid] != 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0], 0xbf);
			if (!(cached_efr[devid] & BIT4))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_EFR][0],
					     cached_efr[devid] | BIT4);
			SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid] & ~BIT7);
			if (!(cached_mcr[devid] & BIT2))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_MCR][0],
					     cached_mcr[devid] | BIT2);
		break;
		case 90:		//90: lcr[7] == 0, efr[4] =1
			if (!(cached_efr[devid] & BIT4)) {
				if (cached_lcr[devid] != 0xbf)
					SPI_WriteReg(devid,
						     reg_info[XR20M1170REG_LCR][0],
						     0xbf);
				SPI_WriteReg(devid, reg_info[XR20M1170REG_EFR][0],
					     cached_efr[devid] | BIT4);
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid] & ~BIT7);
			} else if (cached_lcr[devid] & BIT7)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid] & ~BIT7);
		break;
		case 100:		//100: lcr!= 0xbf, efr[4] =1
			if (!(cached_efr[devid] & BIT4)) {
				if (cached_lcr[devid] != 0xbf)
					SPI_WriteReg(devid,
						     reg_info[XR20M1170REG_LCR][0],
						     0xbf);
				SPI_WriteReg(devid, reg_info[XR20M1170REG_EFR][0],
					     cached_efr[devid] | BIT4);
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0x3f);
			} else if (cached_lcr[devid] == 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0x3f);
		break;
	}
}

static void ExitConstraint(unsigned char devid, unsigned char regaddr)
{
	//restore
	switch (reg_info[regaddr][1]) {
			//10: no contraint
		case 20:		//20: lcr[7] == 0
			if (cached_lcr[devid] & BIT7)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid]);
			break;
		case 30:		//30: lcr == 0xbf
			if (cached_lcr[devid] != 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid]);
			break;
		case 40:		//40: lcr != 0xbf
			if (cached_lcr[devid] == 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0xbf);
			break;
		case 50:		//50: lcr[7] == 1 , lcr != 0xbf, efr[4] = 1
			if ((cached_efr[devid] & BIT4) == 0) {
				if (cached_lcr[devid] != 0xbf)
					SPI_WriteReg(devid,
						     reg_info[XR20M1170REG_LCR][0],
						     0xbf);
				SPI_WriteReg(devid, reg_info[XR20M1170REG_EFR][0],
					     cached_efr[devid]);
			}
			SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid]);
			break;
		case 60:		//60: lcr[7] == 1 , lcr != 0xbf,
			if ((cached_lcr[devid] == 0xbf)
			    || (!(cached_lcr[devid] & BIT7)))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid]);
			break;
		case 70:		//lcr != 0xbf,  and (efr[4] == 0 or efr[4] =1, mcr[2] = 0)
			if ((cached_efr[devid] & BIT4) && (cached_mcr[devid] & BIT2))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_MCR][0],
					     cached_mcr[devid]);
			if (cached_lcr[devid] == 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid]);
			break;
		case 80:		//lcr != 0xbf,  and (efr[4] = 1, mcr[2] = 1)
			if (!(cached_mcr[devid] & BIT2))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_MCR][0],
					     cached_mcr[devid]);
			SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0], 0xbf);
			if (!(cached_efr[devid] & BIT4))
				SPI_WriteReg(devid, reg_info[XR20M1170REG_EFR][0],
					     cached_efr[devid]);
			if (cached_lcr[devid] != 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid]);
			break;
		case 90:		//90: lcr[7] == 0, efr[4] =1 (for ier bit 4-7)
			if (!(cached_efr[devid] & BIT4)) {
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0xbf);
				SPI_WriteReg(devid, reg_info[XR20M1170REG_EFR][0],
					     cached_efr[devid]);
				if (cached_lcr[devid] != 0xbf)
					SPI_WriteReg(devid,
						     reg_info[XR20M1170REG_LCR][0],
						     cached_lcr[devid]);
			} else if (cached_lcr[devid] & BIT7)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid]);
			break;
		case 100:		//100: lcr!= 0xbf, efr[4] =1
			if (!(cached_efr[devid] & BIT4)) {
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0xbf);
				SPI_WriteReg(devid, reg_info[XR20M1170REG_EFR][0],
					     cached_efr[devid]);
				if (cached_lcr[devid] != 0xbf)
					SPI_WriteReg(devid,
						     reg_info[XR20M1170REG_LCR][0],
						     cached_lcr[devid]);
			} else if (cached_lcr[devid] == 0xbf)
				SPI_WriteReg(devid, reg_info[XR20M1170REG_LCR][0],
					     0xbf);
			break;
	}
}


static unsigned char serial_in(unsigned char devid, unsigned char regaddr)
{
	unsigned char ret;

	if (!(reg_info[regaddr][2] & 0x1)) {
		printk("Reg can not Read\n");
		return 0;
	}


	switch (regaddr) {
	case XR20M1170REG_LCR:
		ret = cached_lcr[devid];
		break;
	case XR20M1170REG_EFR:
		ret = cached_efr[devid];
		break;
	case XR20M1170REG_MCR:
		ret = cached_mcr[devid];
		break;
	default:
//		spin_lock_irqsave(&xr20m1172_lock, xr20m1172_flags);
		EnterConstraint(devid, regaddr);
		ret = SPI_ReadReg(devid, reg_info[regaddr][0]);
		ExitConstraint(devid, regaddr);
//		spin_unlock_irqrestore(&xr20m1172_lock, xr20m1172_flags);
	}
;

	return ret;
}


static void serial_out(unsigned char devid, unsigned char regaddr, unsigned char data)
{
	if (!(reg_info[regaddr][2] & 0x2)) {
		printk("Reg not writeable\n");
		return;
	}

	switch (regaddr) {
		case XR20M1170REG_LCR:
			if (data == cached_lcr[devid])
				return;
			cached_lcr[devid] = data;
			break;
		case XR20M1170REG_EFR:
			if (data == cached_efr[devid])
				return;
			cached_efr[devid] = data;
			break;
		case XR20M1170REG_MCR:
			if (data == cached_mcr[devid])
				return;
			cached_mcr[devid] = data;
			break;
	}


//	spin_lock_irqsave(&xr20m1172_lock, xr20m1172_flags);
	EnterConstraint(devid, regaddr);
	SPI_WriteReg(devid, reg_info[regaddr][0], data);
	ExitConstraint(devid, regaddr);
//	spin_unlock_irqrestore(&xr20m1172_lock, xr20m1172_flags);
}


unsigned int spi_set_baudrate(unsigned  int baudrate) 
{ 
 	 
 	unsigned char  dld_reg, cached_lcr,lcr_reg; 
 	unsigned int   baud = baudrate; 
 	unsigned int   temp; 
 

 	unsigned long   required_divisor2; 
 	unsigned short  required_divisor ; 
 	 
 	required_divisor2  = (unsigned long) ((XR20M1172_CLOCK * 16)/(PRESCALER * SAMPLE_RATE * baud)); 
 	required_divisor    = required_divisor2 / 16; 
 	dld_reg	=(char)(required_divisor2   - required_divisor*16); 
 	dld_reg   &= ~(0x3 << 4);  //16X 

	cached_lcr = SPI_ReadReg(0,reg_info[XR20M1170REG_LCR][0]);
	if( cached_lcr == 0xbf ) lcr_reg = (0x1 << 7);
	else lcr_reg = (0x1 << 7) | cached_lcr ;
	SPI_WriteReg(0, reg_info[XR20M1170REG_LCR][0], lcr_reg);
	

	SPI_WriteReg(0, reg_info[XR20M1170REG_DLM][0], required_divisor >> 8); 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_DLM][0]); 
 

 	SPI_WriteReg(0, reg_info[XR20M1170REG_DLL][0], required_divisor &0xff); 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_DLL][0]); 
 

 	SPI_WriteReg(0, reg_info[XR20M1170REG_DLD][0], dld_reg); 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_DLD][0]); 


	SPI_WriteReg(0, reg_info[XR20M1170REG_LCR][0], cached_lcr);
 	return 0; 
} 

static irqreturn_t spi_irq_handler(int irq, void *dev_id, struct pt_regs *reg)
{
	#if READ_TEST
	printk(" IRQ ON ");
	#endif
	flag_queue_ready = 1;
 	wake_up_interruptible(&read_wq); 
 	 
 	return IRQ_HANDLED; 
}

//------------------------ vport part +
#if READ_TEST
int read_test( unsigned char *buf , int size);
#endif
static int xrm_spi_open_0(struct inode *inode, struct file *filp) 
{ 

 	 
 	filp->private_data = xrm_spi_port[0]; 
 	 
 	printk(" xrmport0 open succed !\n"); 
	
 	return 0; 
} 


static int xrm_spi_open_1(struct inode *inode, struct file *filp) 
{ 

 	filp->private_data = xrm_spi_port[1]; 
 	 
 	printk(" xrmport1 open succed !\n"); 
	
 	return 0; 
} 



//static ssize_t xrm_spi_write_0(struct file *filp, const char __user *buf, size_t size, loff_t *ppos) 
static ssize_t xrm_spi_write_0(struct file *filp, const char  __user *buf, size_t size, loff_t *ppos)
{ 
 	
 	unsigned char buf2[MAX_BUF]; 
 	unsigned int count = 0; 
 	unsigned char temp, ier; 
	unsigned long ret;
 	
	//DECLARE_WAITQUEUE(wait2, current); 
	
 	//add_wait_queue(&spi_port->tx_wait, &wait2); 
	
	
 	if( size >  MAX_BUF ) {
		printk(" size tooo big %d\n",size);
		return 0;
 	}

	ret = copy_from_user(buf2, (void *) buf, size); 

	if(ret) {
		printk("Error copy_from_user \n");
		return 0;
	}
	

	ier = serial_in(0, XR20M1170REG_IER);
	ier |= (0x1 << 1);
	serial_out(0, XR20M1170REG_IER, ier);
	
    
 retry: 
 
	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_LSR][0]);
	
 	while(temp & 0x20) 
 	{ 
 			temp = *(buf2 + count); 	 
 	 
 			SPI_WriteReg(0, reg_info[XR20M1170REG_THR][0], temp); 
 			count += 1; 
 			if(count == size ) 
 			{ 
 				goto out; 
 			}
			temp = SPI_ReadReg(0, reg_info[XR20M1170REG_LSR][0]);
 	} 
 	 
 	goto retry; 

out: 
 	#if READ_TEST
 	{
 	unsigned char test[MAX_BUF];
	int ret;
 	ret = read_test(test, MAX_BUF);

	printk(" size %d    %s \n", ret , test);
	}
	#endif
 	return count; 
} 

static ssize_t xrm_spi_write_1(struct file *filp, const char  __user *buf, size_t size, loff_t *ppos)
{ 
 	unsigned char buf2[MAX_BUF]; 
 	unsigned int count = 0; 
 	unsigned char temp; 
	unsigned long ret;
	
	
	if( size >  MAX_BUF ) {
		printk(" size tooo big %d\n",size);
		return 0;
 	}

	ret = copy_from_user(buf2, (void *) buf, size); 

	if(ret) {
		printk("Error copy_from_user \n");
		return 0;
	}
	
    
 retry: 
 
	temp = SPI_ReadReg(1, reg_info[XR20M1170REG_LSR][0]);
 	while(temp & 0x20) 
 	{ 
 			temp = *(buf2 + count); 	 
 	 
 			SPI_WriteReg(1, reg_info[XR20M1170REG_THR][0], temp); 
 			count += 1; 
 			if(count == size ) 
 			{ 
 				goto out; 
 			}
			temp = SPI_ReadReg(1, reg_info[XR20M1170REG_LSR][0]);
 	} 
 	 
 	//interruptible_sleep_on(&spi_port->tx_wait); 
 	goto retry; 

out: 
 	return count; 
} 



//wait_queue_head_t  read_isr_wait;

int push_rd_buffer(int ch, char in){

int ret;
	if(ch == 0) {
		if( rx_buf_count_0 == RD_BUX_MAX ){
			ret = 0;
		}
		else {
			rx_buf_0[rx_tail_0] = in;
			rx_tail_0 = (rx_tail_0 + 1) % RD_BUX_MAX;
			rx_buf_count_0++;
			ret = 1;
		}
	}
	else {
		if( rx_buf_count_1 == RD_BUX_MAX ){
			ret = 0;
		}
		else {
			rx_buf_1[rx_tail_1] = in;
			rx_tail_1 = (rx_tail_1 + 1) % RD_BUX_MAX;
			rx_buf_count_1++;
			ret = 1;
		}
		
	}
#if READ_TEST
	printk("\n IN PUSH buf0 num  %d  buf1 num %d \n",rx_buf_count_0, rx_buf_count_1);
#endif 
	return ret;
}
static int xr20m_read_thread(void *arg)
{
	unsigned char  buf1[MAX_BUF];
	u8 lsr;
	int count,i;
	
	
 	daemonize("xr20m-read-thread"); 

	spin_lock_init(&rd_buf_lock_0);
	spin_lock_init(&rd_buf_lock_1);

	rx_buf_count_0 = 0;
	rx_buf_count_1 = 0;
 	//interruptible_sleep_on(&xrm_spi_port[0]->isr_wait); 
	
  	while(1)
   	{
	retry:  
 		
 		wait_event_interruptible(read_wq, (flag_queue_ready == 1));  
		flag_queue_ready = 0;

		lsr = serial_in( 0, XR20M1170REG_LSR);
		count = 0;
		while( (lsr & 0x3) ){
			buf1[count++] = serial_in(0, XR20M1170REG_RHR);
			lsr = serial_in( 0, XR20M1170REG_LSR);
		}
		

		spin_lock(&rd_buf_lock_0);
		for(i = 0 ; i < count ; i++){
			push_rd_buffer(0, buf1[i]);
		}
		#if READ_TEST
			printk(" IN THREAD0 read %d  count %d \n",count, rx_buf_count_0);
		#endif
		spin_unlock(&rd_buf_lock_0);


		lsr = serial_in( 1, XR20M1170REG_LSR);
		count = 0;
		while( (lsr & 0x3) ){
			buf1[count++] = serial_in(1, XR20M1170REG_RHR);
			lsr = serial_in( 1, XR20M1170REG_LSR);
		}
		

		spin_lock(&rd_buf_lock_1);
		for(i = 0 ; i < count ; i++){
			push_rd_buffer(1, buf1[i]);
		}
		spin_unlock(&rd_buf_lock_1);

 		
 		goto retry; 			 
   	}
  	printk(KERN_ALERT "@ %s() : kthread_should_stop() called. Bye.\n", __FUNCTION__);
  	return 0;
}





static ssize_t xrm_spi_read_0(struct file *filp, 
							const char  __user *buf, 
							size_t size, 
							loff_t *ppos)
{ 
	unsigned int   count=0;
	unsigned char  buf1[MAX_BUF];

	unsigned long ret = 0;


	count = 0;
	spin_lock(&rd_buf_lock_0);

	while( (count < size ) && rx_buf_count_0 ){
		buf1[count] = rx_buf_0[rx_head_0];
		count++;
		rx_head_0 = (rx_head_0 + 1) % RD_BUX_MAX;
		rx_buf_count_0 = rx_buf_count_0 - 1;	
	}
	spin_unlock(&rd_buf_lock_0);
	
	if(count) ret = copy_to_user((void *)buf, buf1, count); 

	if( ret ) {
		printk(" Error copy_to_user \n");
		return 0;
	}
	
	return count; 
} 



static ssize_t xrm_spi_read_1(struct file *filp, 
							const char  __user *buf, 
							size_t size, 
							loff_t *ppos)
{ 
	unsigned int   count=0;
	unsigned char  buf1[MAX_BUF];
	unsigned long ret = 0;

	count = 0;
	spin_lock(&rd_buf_lock_1);

	while( (count < size ) && rx_buf_count_1 ){
		buf1[count] = rx_buf_0[rx_head_0];
		count++;
		rx_head_1 = (rx_head_1 + 1) % RD_BUX_MAX;
		rx_buf_count_1 = rx_buf_count_1 - 1;	
	}
	spin_unlock(&rd_buf_lock_1);
	
	if(count) ret = copy_to_user((void *)buf, buf1, count); 

	if( ret ) {
		printk(" Error copy_to_user \n");
		return 0;
	} 
	
	return count; 
} 

#if READ_TEST
int read_test( unsigned char *buf , int size)
{
unsigned int   count=0;
unsigned char  buf1[MAX_BUF];
int not_empty;

	not_empty = 1;
	count = 0;
	
	spin_lock(&rd_buf_lock_0);

	while( (count < size ) && rx_buf_count_0){
		buf1[count] = rx_buf_0[rx_head_0];
		count++;
		rx_head_0 = (rx_head_0 + 1) % RD_BUX_MAX;
		rx_buf_count_0 = rx_buf_count_0 - 1;	
	}
	printk(" read test done  out %d  %d \n", count, rx_buf_count_0);
	spin_unlock(&rd_buf_lock_0);

	
	if(count) memcpy( buf, buf1, count); 
		 
	return count; 
}
#endif
//----------------------- vport part -


 static struct file_operations xrm_fops_0 ={ 
 	.owner	= 		THIS_MODULE, 
 	.open	= 		xrm_spi_open_0, //i2c_gpio_open, 
 	.read	=	 	(void *)xrm_spi_read_0, //i2c_gpio_read, 
 	.write	= 		xrm_spi_write_0, //i2c_gpio_write, 
 	.release	= 	NULL, //i2c_gpio_release, 
 }; 


static struct file_operations xrm_fops_1 ={ 
 	.owner	= 		THIS_MODULE, 
 	.open	= 		xrm_spi_open_1, //i2c_gpio_open, 
 	.read	=	 	(void *)xrm_spi_read_1, //i2c_gpio_read, 
 	.write	= 		xrm_spi_write_1, //i2c_gpio_write, 
 	.release	= 	NULL, //i2c_gpio_release, 
 }; 



static int serial_xr20m1172_probe(struct spi_device *dev)
{
    int ret=0;
	int result0, result1, retval;
	unsigned char temp;



	sema_init(&spi_port_sem, 1);


    ret = xr20m_spi_setup(dev);
	
    if(ret < 0)  return -1;
	
    spi_set_drvdata(dev, NULL);


    if (ret) {
        pr_err("xr20m1172 driver init: \
                    fail to start event thread\n");
        kfree(spi_get_drvdata(dev));
        spi_set_drvdata(dev, NULL);
        return -1;
    }


	NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_SPI232_RST), PAD_GET_BITNO(CFG_IO_SPI232_RST), CTRUE);
	NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_SPI232_RST), PAD_GET_BITNO(CFG_IO_SPI232_RST), CFALSE);
    mdelay(1);
	NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_SPI232_RST), PAD_GET_BITNO(CFG_IO_SPI232_RST), CTRUE);
	

    cached_lcr[0] = SPI_ReadReg(0, reg_info[XR20M1170REG_LCR][0]);
    cached_efr[0] = SPI_ReadReg(0, reg_info[XR20M1170REG_EFR][0]);
    cached_mcr[0] = SPI_ReadReg(0, reg_info[XR20M1170REG_MCR][0]);
    cached_lcr[1] = SPI_ReadReg(1, reg_info[XR20M1170REG_LCR][0]);
    cached_efr[1] = SPI_ReadReg(1, reg_info[XR20M1170REG_EFR][0]);
    cached_mcr[1] = SPI_ReadReg(1, reg_info[XR20M1170REG_MCR][0]);

	//------------------- start XR20M init ----------------------------

 	temp = 0xBF; 
 	SPI_WriteReg(0, reg_info[XR20M1170REG_LCR][0], temp); 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_LCR][0]); 

 	 
	 /*EFR  */ 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_EFR][0]); 
 	temp  |=( 1 << 4) ; 
 	 
 	SPI_WriteReg(0, reg_info[XR20M1170REG_EFR][0], temp); 
	
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_EFR][0]); 
	
	SPI_WriteReg(0, reg_info[XR20M1170REG_LCR][0], 0x83);

	/*MCR */ 
	temp = (1 << 3);
	
	SPI_WriteReg(0, reg_info[XR20M1170REG_MCR][0], temp);
	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_MCR][0]); 


	spi_set_baudrate(PORT_BAUD_RATE);
	
	/*LCR         0000 0011 */ 
 	SPI_WriteReg(0, reg_info[XR20M1170REG_LCR][0], 0x03); 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_LCR][0]); 

	/*IER&*/ 
 	temp =0x0f; //3; 
 	SPI_WriteReg(0, reg_info[XR20M1170REG_IER][0], temp); 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_IER][0]); 


	/*FCR*/ 
 	SPI_WriteReg(0, reg_info[XR20M1170REG_FCR][0], 0x1); //0x47
 	SPI_WriteReg(0, reg_info[XR20M1170REG_FCR][0], 0x5); //0x47
 	

	SPI_ReadReg(0, reg_info[XR20M1170REG_IOCONTROL][0]); 

	/*IOCRL*/ 
 	 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_IOCONTROL][0]); 
 	 

	SPI_WriteReg(0, reg_info[XR20M1170REG_IODIR][0], 0xff);	 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_IODIR][0]); 

  
 	SPI_WriteReg(0, reg_info[XR20M1170REG_IOSTATE][0], 0xaa); 
 	 
 	temp = SPI_ReadReg(0, reg_info[XR20M1170REG_IOSTATE][0]); 

	temp =  SPI_ReadReg(0, (0x8 << 3));

	temp =  SPI_ReadReg(0, (0x2 << 3));

	//-------------------- end XR20M init -----------------------------

	xrm_spi_port[0] = kmalloc(sizeof(struct xr20m1172_port ),GFP_KERNEL); 
	xrm_spi_port[1] = kmalloc(sizeof(struct xr20m1172_port ),GFP_KERNEL); 

	memset(xrm_spi_port[0], 0,sizeof(struct xr20m1172_port));
	memset(xrm_spi_port[1], 0,sizeof(struct xr20m1172_port));

	result0 = alloc_chrdev_region(&devno_0,0,1,XRM_SPI_DRIVER_NAME_0); 
	result1 = alloc_chrdev_region(&devno_1,0,1,XRM_SPI_DRIVER_NAME_1);

	xrm_class_0 = class_create( THIS_MODULE, XRM_SPI_DRIVER_NAME_0 );
	xrm_class_1 = class_create( THIS_MODULE, XRM_SPI_DRIVER_NAME_1 );


	cdev_init(&xrm_spi_port[0]->cdev,&xrm_fops_0);
	cdev_init(&xrm_spi_port[1]->cdev,&xrm_fops_1);

	xrm_spi_port[0]->cdev.owner = THIS_MODULE; 
 	xrm_spi_port[0]->cdev.ops   = &xrm_fops_0; 
	xrm_spi_port[1]->cdev.owner = THIS_MODULE; 
 	xrm_spi_port[1]->cdev.ops   = &xrm_fops_1; 

	result0 = cdev_add(&xrm_spi_port[0]->cdev,devno_0,1); 
	result1 = cdev_add(&xrm_spi_port[1]->cdev,devno_1,1);

	device_create( xrm_class_0, NULL, devno_0, (void *)NULL, XRM_SPI_DRIVER_NAME_0 );
	device_create( xrm_class_1, NULL, devno_1, (void *)NULL, XRM_SPI_DRIVER_NAME_1 );

	xrm_spi_port[0]->rx_head = xrm_spi_port[0]->rx_tail =0; 
 	xrm_spi_port[0]->tx_head = xrm_spi_port[0]->tx_tail = 0; 
 	init_waitqueue_head(&xrm_spi_port[0]->rx_wait); 
 	init_waitqueue_head(&xrm_spi_port[0]->tx_wait); 
 	init_waitqueue_head(&xrm_spi_port[0]->isr_wait);

	xrm_spi_port[1]->rx_head = xrm_spi_port[1]->rx_tail =0; 
 	xrm_spi_port[1]->tx_head = xrm_spi_port[1]->tx_tail = 0; 
 	init_waitqueue_head(&xrm_spi_port[1]->rx_wait); 
 	init_waitqueue_head(&xrm_spi_port[1]->tx_wait); 
 	init_waitqueue_head(&xrm_spi_port[1]->isr_wait);

	retval = request_irq((CFG_IO_SPI232_INT + IRQ_GPIO_START),(void *)spi_irq_handler,
					IRQF_TRIGGER_RISING ,"spi_int" ,NULL); 
 	if(retval) 
 	{ 
 			printk("request xrm_spi_0 IRQ failed !"); 
 	} 
 	else 
 	{ 
 			printk("request xrm_spi_0 IRQ success !"); 
 			 
 	}

 	spi_task = kthread_run(xr20m_read_thread, NULL, "xr20m-read-thread");
	return 0;
}



static int serial_xr20m1172_remove(struct spi_device *dev)
{
	unregister_chrdev_region( devno_0, 1 );
	unregister_chrdev_region( devno_1, 1 );

	cdev_del(&xrm_spi_port[0]->cdev);
	cdev_del(&xrm_spi_port[0]->cdev);

	device_destroy( xrm_class_0, devno_0 );
	device_destroy( xrm_class_1, devno_1 );

    class_destroy( xrm_class_0 );
	class_destroy( xrm_class_1 );

	if(spi_task){
      kthread_stop(spi_task);
      spi_task = NULL;
    }


	return 0;
	
}


//static struct platform_driver serial_xr20m1172_driver = {
static struct spi_driver serial_xr20m1172_driver = {
	.probe = serial_xr20m1172_probe,
	.remove = serial_xr20m1172_remove,
	.suspend = NULL,
	.resume =  NULL,
	.driver = {
		   .name = "xr20m1172",
           .bus = &spi_bus_type,
           .owner = THIS_MODULE,
		   },
};

int __init serial_xr20m1172_init(void)
{
	int ret = 0;
	
     printk("serial_xr20m1172_init start !!!\n");

    spi_register_driver(&serial_xr20m1172_driver);
        
	return ret;
}

void __exit serial_xr20m1172_exit(void)
{
    spi_unregister_driver(&serial_xr20m1172_driver);
}

module_init(serial_xr20m1172_init);
module_exit(serial_xr20m1172_exit);
MODULE_LICENSE("GPL");











