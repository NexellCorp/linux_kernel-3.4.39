

#ifndef __XR20m1172_520_H__
#define __XR20m1172_520_H__




#undef DEBUG_VIA_PROC_FS	//if enabled, only XM0 works
//#define DEBUG_VIA_PROC_FS	//if enabled, only XM0 works

#define        POWER_RUN            0
#define        POWER_PRE_SUSPEND    1
#define        POWER_SUSPEND        2
#define        POWER_PRE_RESUME     3

#define TRUNC(a)                        ((unsigned int)(a))
#define ROUND(a)                        ((unsigned int)(a+0.5))

#define BIT0                            0x1
#define BIT1                            0x2
#define BIT2                            0x4
#define BIT3                            0x8
#define BIT4                            0x10
#define BIT5                            0x20
#define BIT6                            0x40
#define BIT7                            0x80

#define XR20M1170REG_RHR            0
#define XR20M1170REG_THR            1
#define XR20M1170REG_DLL            2
#define XR20M1170REG_DLM            3
#define XR20M1170REG_DLD            4
#define XR20M1170REG_IER            5
#define XR20M1170REG_ISR            6
#define XR20M1170REG_FCR            7
#define XR20M1170REG_LCR            8
#define XR20M1170REG_MCR            9
#define XR20M1170REG_LSR            10
#define XR20M1170REG_MSR            11
#define XR20M1170REG_SPR            12
#define XR20M1170REG_TCR            13
#define XR20M1170REG_TLR            14
#define XR20M1170REG_TXLVL          15
#define XR20M1170REG_RXLVL          16
#define XR20M1170REG_IODIR          17
#define XR20M1170REG_IOSTATE        18
#define XR20M1170REG_IOINTENA       19
#define XR20M1170REG_IOCONTROL      20
#define XR20M1170REG_EFCR           21
#define XR20M1170REG_EFR            22
#define XR20M1170REG_XON1           23
#define XR20M1170REG_XON2           24
#define XR20M1170REG_XOFF1          25
#define XR20M1170REG_XOFF2          26



#define READ_TEST  0

#define XRM_CDEV_MAJOR 231

#define XRM_SPI_DRIVER_NAME_0 "xrmport0"
#define XRM_SPI_DRIVER_NAME_1 "xrmport1"

#define MAX_BUF (128)


#define INCBUF(x,mod)   ((++(x)) & ((mod)-1))

struct xr20m1172_port {
	struct cdev cdev; 
	struct uart_port port; 
	struct spi_device *spi;
	//struct 
	unsigned devid; 
	unsigned ier; 
	unsigned char lcr; 
	unsigned char mcr; 
	 
	unsigned char out_level;  /*ext gpio level*/ 
	
	unsigned int rx_head,rx_tail; 
	 
	unsigned char rx_buf[MAX_BUF]; 
	 
	unsigned int tx_head, tx_tail; 
	 
	unsigned char tx_buf[MAX_BUF]; 
	wait_queue_head_t  rx_wait; 
	wait_queue_head_t  tx_wait; 
	wait_queue_head_t  isr_wait; 
};


//////////////////////////////////////////////////
struct mxc_xr20m{

struct spi_device *spi;

};

//////////////////////////////////////////////////
//extern void pxa3xx_enable_ssp2_pins(void);
//extern void pxa_set_cken(int clock, int enable);


#endif




