#include "INC_INCLUDES.h"
#include "INC_SPI.h"

//#include "/root/telechips_jb/kernel/arch/arm/mach-tcc893x/include/mach/gpio.h"

/////////////////////////////////////////////////////////////////////
// Naming rule : TAGET_CONFIG_$(company or maker)_$(product)
/////////////////////////////////////////////////////////////////////
//#define TARGET_CONFIG_INSIGNAL_ORIGEN  		//Insignal exynos4210(V310) EVK
//#define TARGET_CONFIG_MV_V310				//MicroVision:exynos4210
#define TARGET_CONFIG_S5P6818				// Nexell : NXP5430
/////////////////////////////////////////////////////////////////////

#include <mach/gpio.h>
#include <mach/devices.h>
#include <mach/soc.h>
//#include <mach/regs-gpio.h>
//#include <plat/gpio-cfg.h>


#if defined(TARGET_CONFIG_S5P6818)
#define CFG_DMB_POWER_EN		((PAD_GPIO_A + 22) | PAD_FUNC_ALT0)		/* GPIO */
#define CFG_DMB_RESET     		((PAD_GPIO_D + 26) | PAD_FUNC_ALT0)		/* GPIO */
#define CFG_DMB_IRQ     		(PAD_GPIO_ALV + 2)						/* IRQ */

#define SPI_RX_BUF_SIZE (4096)
#define SPI_TX_BUF_SIZE (4096) // for nexell
#else
#define SPI_RX_BUF_SIZE (CMD_SIZE+4096)
#define SPI_TX_BUF_SIZE	(CMD_SIZE+2)
#endif

#define SPI_DMA_MAX_BYTE 0x400 // 1024
//#define SPI_DMA_MAX_BYTE 0x1000 // 4096

extern ST_INC_Interrupt g_stINCInt;
extern unsigned int		g_TaskFlag;
extern struct spi_device gInc_spi;

INC_UINT8 *g_pRxBuff;
INC_UINT8 *g_pTxBuff;

#if 0
struct spidev_data {
      dev_t           devt;
	  spinlock_t      spi_lock;
	  struct spi_device   *spi;
	  struct list_head    device_entry;
	  /* buffer is NULL unless this device is open (users > 0) */
	  struct mutex        buf_lock;
	  unsigned        users;
	  u8          *buffer;
};
#endif

void INC_GPIO_DMBEnable(void)
{
#if defined(TARGET_CONFIG_MV_V310)
	s3c_gpio_cfgpin(EXYNOS4_GPL2(7), S3C_GPIO_SFN(1));
	s3c_gpio_setpull(EXYNOS4_GPL2(7),S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(EXYNOS4_GPL2(7),S5P_GPIO_DRVSTR_LV4);
	gpio_set_value(EXYNOS4_GPL2(7), 1);
	INC_MSG_PRINTF(1, "[%s]   : 1.TARGET_CONFIG_MV_V310!!! \n", __func__);
#elif defined(TARGET_CONFIG_S5P6818)
	nxp_soc_gpio_set_out_value(CFG_DMB_POWER_EN, 0);
    nxp_soc_gpio_set_io_dir(CFG_DMB_POWER_EN, 1);
    nxp_soc_gpio_set_io_func(CFG_DMB_POWER_EN, nxp_soc_gpio_get_altnum(CFG_DMB_POWER_EN));
    nxp_soc_gpio_set_out_value(CFG_DMB_POWER_EN, 1);
    INC_MSG_PRINTF(1, "[%s] : TARGET_CONFIG_S5P6818 \n", __func__);
#endif
	
	msleep(200);
}


void INC_GPIO_DMBDisable(void)
{
    //...
}

void INC_GPIO_PMU_Enable(int bStatus)
{
#ifdef TARGET_CONFIG_INSIGNAL_ORIGEN
//	s3c_gpio_cfgpin(EXYNOS4210_GPE3(4), S3C_GPIO_SFN(1));
//    s3c_gpio_setpull(EXYNOS4210_GPE3(4),S3C_GPIO_PULL_UP);
//	s5p_gpio_set_drvstr(EXYNOS4210_GPE3(4),S5P_GPIO_DRVSTR_LV4);
	tcc_gpio_config(TCC_GPB(4), GPIO_FN(0));
	gpio_direction_input(TCC_GPB(4));
	tcc_gpio_config_ext_intr(INT_EINT2, EXTINT_GPIOB_04);

    if(bStatus){
		gpio_set_value(TCC_GPB(4), 1);
		msleep(1);
	}else{
		gpio_set_value(TCC_GPB(4), 0);
        msleep(1);
	}	
#endif
}

void INC_GPIO_Reset(int bStart)
{
#ifdef TARGET_CONFIG_INSIGNAL_ORIGEN
//	s3c_gpio_cfgpin(EXYNOS4210_GPE3(5), S3C_GPIO_SFN(1));
//    s3c_gpio_setpull(EXYNOS4210_GPE3(5),S3C_GPIO_PULL_UP);
//	s5p_gpio_set_drvstr(EXYNOS4210_GPE3(5),S5P_GPIO_DRVSTR_LV4);
	tcc_gpio_config(TCC_GPC(21), GPIO_FN(0));
	gpio_request(TCC_GPC(21), NULL);
    gpio_direction_output(TCC_GPC(21),0);
   // tcc_gpio_config_ext_intr(INT_EINT2, EXTINT_GPIOB_05);


	if(bStart){
		gpio_set_value(TCC_GPC(21), 1);
		msleep(10);
		gpio_set_value(TCC_GPC(21), 0);
		msleep(10);
		gpio_set_value(TCC_GPC(21), 1);
		msleep(10);
	}else{
		gpio_set_value(TCC_GPC(21), 1);
	}	
	INC_MSG_PRINTF(1, "[%s] TARGET_CONFIG_INSIGNAL_ORIGEN[V310]   \n", __func__);
#elif defined(TARGET_CONFIG_MV_V310)
	s3c_gpio_cfgpin(EXYNOS4_GPL2(6), S3C_GPIO_SFN(1));
	s3c_gpio_setpull(EXYNOS4_GPL2(6),S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(EXYNOS4_GPL2(6),S5P_GPIO_DRVSTR_LV4);

	gpio_set_value(EXYNOS4_GPL2(6), 1);
	msleep(10);
	gpio_set_value(EXYNOS4_GPL2(6), 0);
	msleep(10);
	gpio_set_value(EXYNOS4_GPL2(6), 1);
	msleep(10);

	s3c_gpio_cfgpin(EXYNOS4_GPB(4), S3C_GPIO_SFN(2));  /**< XspiCLK_1  SPI_1_CLK */
	s3c_gpio_cfgpin(EXYNOS4_GPB(6), S3C_GPIO_SFN(2));  /**< XspiMISO_1 SPI_1_MISO */
	s3c_gpio_cfgpin(EXYNOS4_GPB(7), S3C_GPIO_SFN(2));  /**< XspiMOSI_1 SPI_1_MOSI */
	s3c_gpio_setpull(EXYNOS4_GPB(4), S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(EXYNOS4_GPB(6), S3C_GPIO_PULL_DOWN);	   // S3C_GPIO_PULL_UP
	s3c_gpio_setpull(EXYNOS4_GPB(7), S3C_GPIO_PULL_DOWN);
	INC_MSG_PRINTF(1, "[%s] TARGET_CONFIG_MV_V310 !! \n", __func__);
#elif defined(TARGET_CONFIG_S5P6818)
	nxp_soc_gpio_set_out_value(CFG_DMB_RESET, 1);
    nxp_soc_gpio_set_io_dir(CFG_DMB_RESET, 1);
    nxp_soc_gpio_set_io_func(CFG_DMB_RESET, nxp_soc_gpio_get_altnum(CFG_DMB_RESET));
	if(bStart) {
	    nxp_soc_gpio_set_out_value(CFG_DMB_RESET, 1);
		msleep(10);
		nxp_soc_gpio_set_out_value(CFG_DMB_RESET, 0);
		msleep(10);
		nxp_soc_gpio_set_out_value(CFG_DMB_RESET, 1);
		msleep(200);
		INC_MSG_PRINTF(1, "[%s] Reset High!!!\n", __func__);
		
	}
	else {
		nxp_soc_gpio_set_out_value(CFG_DMB_RESET, 0);
		msleep(10);
		INC_MSG_PRINTF(1, "[%s] Reset Low!!!\n", __func__);
	}
#else
#if 0
	tcc_gpio_config(TCC_GPC(21), GPIO_FN(0));
	gpio_request(TCC_GPC(21), NULL);
    gpio_direction_output(TCC_GPC(21),0);
   // tcc_gpio_config_ext_intr(INT_EINT2, EXTINT_GPIOB_05);


	if(bStart){
		gpio_set_value(TCC_GPC(21), 1);
		msleep(10);
		gpio_set_value(TCC_GPC(21), 0);
		msleep(10);
		gpio_set_value(TCC_GPC(21), 1);
		msleep(10);
	}else{
		gpio_set_value(TCC_GPC(21), 1);
	}
#endif
	INC_MSG_PRINTF(1, "[%s] TARGET_CONFIG_INSIGNAL_ORIGEN[V310]   \n", __func__);
#endif
}


irqreturn_t INC_isr(int irq, void *handle)
{
	//INC_MSG_PRINTF(1, "[%s] interrupt!!![0x%x] ", __func__, irq);
	if(g_TaskFlag)
		complete(&g_stINCInt.comp);
	
	return IRQ_HANDLED;
}


int INC_GPIO_set_Interrupt(void)
{
#ifdef TARGET_CONFIG_INSIGNAL_ORIGEN 
//	s3c_gpio_cfgpin(EXYNOS4_GPX0(5), S3C_GPIO_SFN(0));
//	tcc_gpio_config(TCC_GPB(4), GPIO_FN(0));
//	gpio_request(TCC_GPB(4), NULL);
//	gpio_direction_input(TCC_GPB(4));
	g_stINCInt.irq = INT_EINT8;	

#elif defined(TARGET_CONFIG_MV_V310)
	s3c_gpio_cfgpin(EXYNOS4_GPD(12), S3C_GPIO_SFN(0));
	g_stINCInt.irq = IRQ_EINT(23);
#elif defined(TARGET_CONFIG_S5P6818)
	g_stINCInt.irq = IRQ_GPIO_START + CFG_DMB_IRQ;

	if(request_irq(g_stINCInt.irq, INC_isr, IRQF_TRIGGER_FALLING, "tdmb_int", NULL)){
	INC_MSG_PRINTF(1, "[%s] request_irq(0x%X) Fail	!!! \n", __func__, g_stINCInt.irq);
		return -1;
	}
#else
	//tcc_gpio_config(TCC_GPB(15), GPIO_FN(0));
	//gpio_direction_input(TCC_GPB(15));
	//tcc_gpio_config_ext_intr(INT_EINT8, EXTINT_GPIOB_15);
	//g_stINCInt.irq = INT_EINT8;
#endif	
#if 0
	tcc_gpio_config_ext_intr(INT_EINT8, EXTINT_GPIOB_15);

	g_stINCInt.irq = INT_EINT8;

	if(request_irq(g_stINCInt.irq, INC_isr, IRQF_TRIGGER_FALLING, "tdmb_int", NULL)){
	INC_MSG_PRINTF(1, "[%s] request_irq(0x%X) Fail	!!! \n", __func__, g_stINCInt.irq);
		return -1;
	}
#endif
	INC_MSG_PRINTF(1, "[%s] request_irq(0x%X) success  !!! \n", __func__, g_stINCInt.irq);
	return 0;
}

int INC_GPIO_free_Interrupt(void)
{
	free_irq(g_stINCInt.irq, NULL);
	return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////

unsigned char INC_SPI_SETUP(void)
{
	int	res = 0;
	
	INC_MSG_PRINTF(1, "[%s]   spi_setup +++", __func__);
	res = spi_setup(&gInc_spi);
	if(res != 0) {
		INC_MSG_PRINTF(1, "[%s] can't spi_setup", __func__);
		return INC_ERROR;
	}
	INC_MSG_PRINTF(1, "[%s]   spi_setup success", __func__);
	return INC_SUCCESS;
}

unsigned char INC_SPI_DRV_OPEN(void)
{
	int	res = 0;

	INC_MSG_PRINTF(1, "[%s]   +++\n", __func__);
	
	//INC_MSG_PRINTF(1, "[%s] %s, bus[%d], cs[%d], mod[%d], %dkhz, %dbit, \n", 
	//	__func__, gInc_spi.modalias, gInc_spi.master->bus_num, gInc_spi.chip_select, 
	//	gInc_spi.mode, gInc_spi.max_speed_hz/1000, gInc_spi.bits_per_word);

	//INC_MSG_PRINTF(1, "[%s]   spi_setup +++", __func__);
	//res = spi_setup(&gInc_spi);
	//if(res != 0) {
	//	INC_MSG_PRINTF(1, "[%s] can't spi_setup", __func__);
	//	return INC_ERROR;
	//}
	INC_MSG_PRINTF(1, "[%s]   spi_setup success", __func__);

	if(INC_SPI_SETUP()   == INC_ERROR) {
		return INC_ERROR;
	}
	
	g_pRxBuff = kmalloc(SPI_RX_BUF_SIZE, GFP_KERNEL);
	if(!g_pRxBuff) return INC_ERROR;

	g_pTxBuff = kmalloc(SPI_TX_BUF_SIZE, GFP_KERNEL);
	if(!g_pTxBuff) return INC_ERROR;
		
	INC_MSG_PRINTF(1, "[%s] SUCCESS !!!\r\n", __func__);
	return INC_SUCCESS;
}

unsigned char INC_SPI_DRV_CLOSE(void)
{
	INC_MSG_PRINTF(1, "[%s]   +++\n", __func__);
	kfree(g_pRxBuff);
	kfree(g_pTxBuff);
	return INC_SUCCESS;
}

unsigned char INC_SPI_DRV_RESET(void)
{
	INC_SPI_DRV_CLOSE();
	INC_DELAY(100);
	if(INC_ERROR == INC_SPI_DRV_OPEN())
		return INC_ERROR;
	return INC_SUCCESS;
}

#if 0
static void spidev_complete(void *arg)
{
   complete(arg);
}
	    
static ssize_t spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
   DECLARE_COMPLETION_ONSTACK(done);
   int status;
   message->complete = spidev_complete;
   message->context = &done;
   //  spin_lock_irq(&spidev->spi_lock);
   if (spidev->spi == NULL)
       status = -ESHUTDOWN;
   else
       status = spi_async(spidev->spi, message);
   //  spin_unlock_irq(&spidev->spi_lock);
   if (status == 0) {
         	          wait_for_completion(&done);
					  status = message->status;
					  if (status == 0)
					     status = message->actual_length;
						  }
	return status;
}


static inline ssize_t spidev_sync_write(struct spidev_data *spidev, size_t len)
 {
      struct spi_transfer t = {
	          .tx_buf     = spidev->buffer,
			  .len        = len,
							   };
	  struct spi_message  m;
	  spi_message_init(&m);
	  spi_message_add_tail(&t, &m);
	  return spidev_sync(spidev, &m);
}
															    
static inline ssize_t spidev_sync_read(struct spidev_data *spidev, size_t len)
{
	   struct spi_transfer t = {
			  .rx_buf     = spidev->buffer,
			  .len        = len,
							  };
	   struct spi_message  m;
	   spi_message_init(&m);
	   spi_message_add_tail(&t, &m);
	   return spidev_sync(spidev, &m);
}
#endif

#if 1
unsigned char INC_SPI_DRV_REG_WRITE(unsigned short uiAddr, unsigned short uiData)
{
	int res;
	INC_UINT16 uiCMD = INC_REGISTER_CTRL(SPI_REGWRITE_CMD) | 1;
	struct spi_message 	msg;
	struct spi_transfer transfer; 
	memset(&transfer, 0, sizeof(struct spi_transfer));
	
	g_pTxBuff[0] = uiAddr>>8 ;
	g_pTxBuff[1] = uiAddr&0xff;
	g_pTxBuff[2] = uiCMD>>8;
	g_pTxBuff[3] = uiCMD&0xff;
	g_pTxBuff[4] = uiData>>8;
	g_pTxBuff[5] = uiData&0xff;

	spi_message_init(&msg);
	transfer.tx_buf = g_pTxBuff;
	transfer.rx_buf = NULL;
	transfer.len 	= 6;

	spi_message_add_tail(&transfer, &msg);
	INC_MSG_PRINTF(0, "[%s] Enter addr:0x%X, data:0x%X\r\n", __func__, uiAddr, uiData);

	res = spi_sync(&gInc_spi, &msg);
	if(res != 0){
		INC_MSG_PRINTF(1, "[%s] can't send spi message [0x%X]\r\n", __func__, res);
		return INC_ERROR;
	}
	return INC_SUCCESS;
}
#else
unsigned char INC_SPI_DRV_REG_WRITE(unsigned short uiAddr, unsigned short uiData)
{
	int res;
	INC_UINT16 uiCMD = INC_REGISTER_CTRL(SPI_REGWRITE_CMD) | 1;
	//struct spi_message 	msg;
	//struct spi_transfer transfer; 
	struct spidev_data spidev;

	//memset(&transfer, 0, sizeof(struct spi_transfer));
	
	//INC_MSG_PRINTF(1, "[%s] +++\r\n", __func__);

	
	
	g_pTxBuff[0] = uiAddr>>8 ;
	g_pTxBuff[1] = uiAddr&0xff;
	g_pTxBuff[2] = uiCMD>>8;
	g_pTxBuff[3] = uiCMD&0xff;
	g_pTxBuff[4] = uiData>>8;
	g_pTxBuff[5] = uiData&0xff;
	
	spidev.buffer = g_pTxBuff;
	spidev.spi=&gInc_spi;

	spidev_sync_write(&spidev, 6);
	
	//INC_MSG_PRINTF(1, "[%s] test1\r\n", __func__);
/*
	spi_message_init(&msg);
	transfer.tx_buf = g_pTxBuff;
	transfer.rx_buf = NULL;
	transfer.len 	= 6;
    
    INC_MSG_PRINTF(1, "[%s] test2\r\n", __func__);

	spi_message_add_tail(&transfer, &msg);
	INC_MSG_PRINTF(1, "[%s] Enter addr:0x%X, data:0x%X\r\n", __func__, uiAddr, uiData);

	res = spi_sync(&gInc_spi, &msg);

	if(res != 0){
		INC_MSG_PRINTF(1, "[%s] can't send spi message [0x%X]\r\n", __func__, res);
		return INC_ERROR;
	}
	*/
	return INC_SUCCESS;
}
#endif

#if 1
unsigned short INC_SPI_DRV_REG_READ(unsigned short uiAddr)
{
	int			res;
	INC_UINT16	uiCMD = INC_REGISTER_CTRL(SPI_REGREAD_CMD) | 1;
	struct spi_message 	msg;
	struct spi_transfer transfer[2]; 

	memset(&transfer, 0, sizeof(struct spi_transfer));
	memset(g_pTxBuff, 0, SPI_TX_BUF_SIZE);

	g_pTxBuff[0] = uiAddr>>8;
	g_pTxBuff[1] = uiAddr&0xff;
	g_pTxBuff[2] = uiCMD>>8;
	g_pTxBuff[3] = uiCMD&0xff;

	spi_message_init(&msg);
#if defined(TARGET_CONFIG_GSENSE_MTK6577)
	transfer[0].tx_buf 	= g_pTxBuff;
	transfer[0].rx_buf 	= NULL;
	transfer[0].len 	= CMD_SIZE;
	spi_message_add_tail(&transfer[0], &msg);

	transfer[1].tx_buf 	= NULL;
	transfer[1].rx_buf 	= g_pRxBuff;
	transfer[1].len 	= 2;
	spi_message_add_tail(&transfer[1], &msg);
#else
	transfer[0].tx_buf 	= g_pTxBuff;
	transfer[0].rx_buf 	= g_pRxBuff;
	transfer[0].len 	= CMD_SIZE+2;
	spi_message_add_tail(&transfer[0], &msg);
#endif	
	INC_MSG_PRINTF(0, "[%s] Enter addr:0x%X\r\n", __func__, uiAddr);

	res = spi_sync(&gInc_spi, &msg);
	if(res != 0){
		INC_MSG_PRINTF(1, "[%s] can't send spi message [0x%X]\r\n", __func__, res);
		return INC_ERROR;
	}
	//INC_MSG_PRINTF(1, "\r\n[%s] TX: %02X, %02X, %02X, %02X, %02X, %02X \n", 
	//	__func__,g_pTxBuff[0],g_pTxBuff[1],g_pTxBuff[2],g_pTxBuff[3],g_pTxBuff[4],g_pTxBuff[5]);
	//INC_MSG_PRINTF(1, "[%s] RX: %02X, %02X, %02X, %02X, %02X, %02X \n\r\n", 
	//	__func__,g_pRxBuff[0],g_pRxBuff[1],g_pRxBuff[2],g_pRxBuff[3],g_pRxBuff[4],g_pRxBuff[5]);
		
	return g_pRxBuff[4]<<8 | g_pRxBuff[5];
}
#else
unsigned short INC_SPI_DRV_REG_READ(unsigned short uiAddr)
{
	int			res;
	INC_UINT16	uiCMD = INC_REGISTER_CTRL(SPI_REGREAD_CMD) | 1;
	struct spi_message 	msg;
	//struct spi_transfer transfer[2]; 
	struct spidev_data spidev;
	
	//memset(&transfer, 0, sizeof(struct spi_transfer));
	memset(g_pTxBuff, 0, SPI_TX_BUF_SIZE);

	g_pTxBuff[0] = uiAddr>>8;
	g_pTxBuff[1] = uiAddr&0xff;
	g_pTxBuff[2] = uiCMD>>8;
	g_pTxBuff[3] = uiCMD&0xff;
    
	spidev.buffer = g_pTxBuff;
	spidev.spi=&gInc_spi;
	spidev_sync_write(&spidev, 4);
	
	INC_MSG_PRINTF(0, "[%s] Enter addr:0x%X\r\n", __func__, uiAddr);
	
	spidev.buffer = g_pRxBuff;
	spidev_sync_read(&spidev, 2);
		
	return g_pRxBuff[0]<<8 | g_pRxBuff[1];
}
#endif

#if 1
unsigned char INC_SPI_DRV_READ_BURST(unsigned char* tx_data, unsigned short tx_length, unsigned char* rx_data, unsigned short rx_length)
{
	int res;
	int nLoop, pkt_cnt, remain;

	struct spi_message	msg;
	struct spi_transfer transfer[5]; 
	memset(&transfer, 0, sizeof(struct spi_transfer)*5);

	nLoop = 0;
	pkt_cnt= rx_length / SPI_DMA_MAX_BYTE;
	remain = rx_length %  SPI_DMA_MAX_BYTE;

	spi_message_init(&msg);
	transfer[0].tx_buf	= tx_data; //g_pTxBuff;
	transfer[0].rx_buf	= rx_data;
	transfer[0].len 	= tx_length + rx_length;
	
	spi_message_add_tail(&transfer[0], &msg);

	res = spi_sync(&gInc_spi, &msg);
	if(res != 0){
		INC_MSG_PRINTF(1, "[%s] can't send spi message [0x%X]\r\n", __func__,res);
		return INC_ERROR;
	}
	return INC_SUCCESS;
}
#else
unsigned char INC_SPI_DRV_READ_BURST(unsigned char* tx_data, unsigned short tx_length, 
									unsigned char* rx_data, unsigned short rx_length)
{
	int res;

	struct spi_message 	msg;
	struct spidev_data spidev;
	
	memset(g_pTxBuff, 0, SPI_TX_BUF_SIZE);

    
	spidev.buffer = tx_data;
	spidev.spi=&gInc_spi;
	spidev_sync_write(&spidev, tx_length);
	
	
	spidev.buffer = rx_data;
	
	spidev_sync_read(&spidev, rx_length);
    INC_MSG_PRINTF(0, "[%s] ---\n", __func__);
	return INC_SUCCESS;
}
#endif

unsigned char INC_SPI_DRV_MEM_READ(unsigned short uiAddr, unsigned char* pBuff, unsigned short wSize)
{
	INC_UINT16 uiLoop, nIndex = 0, uiCMD = 0;
	INC_UINT16 anLength[2] = {0,};

	memset(g_pTxBuff, 0, SPI_TX_BUF_SIZE);
	if(pBuff == NULL || wSize >= INC_MPI_MAX_BUFF)
		return INC_ERROR;
	
	if(wSize > INC_MPI_MAX_LEN) {
		anLength[nIndex++] = INC_MPI_MAX_LEN;
		anLength[nIndex++] = wSize - INC_MPI_MAX_LEN;
	}
	else anLength[nIndex++] = wSize;

	for(uiLoop=0; uiLoop<nIndex; uiLoop++)
	{
		g_pTxBuff[0] = uiAddr >> 8;
		g_pTxBuff[1] = uiAddr & 0xff;
		uiCMD = INC_REGISTER_CTRL(SPI_MEMREAD_CMD) | (anLength[uiLoop] & INC_MPI_MAX_LEN);
		g_pTxBuff[2] = uiCMD >> 8;
		g_pTxBuff[3] = uiCMD & 0xff;

		/////////////////////////////////////////////////
		//
		/////////////////////////////////////////////////
		INC_SPI_DRV_READ_BURST(g_pTxBuff, 4, g_pRxBuff, anLength[uiLoop]);

		memcpy((INC_UINT8*)pBuff, (INC_UINT8*)&g_pRxBuff[4], anLength[uiLoop]);
		pBuff += anLength[uiLoop];
	}
	return INC_SUCCESS;
}

