#ifndef __INC_SPI_H__
#define __INC_SPI_H__


typedef struct{
	int irq;
	struct completion comp;
}ST_INC_Interrupt;


void INC_GPIO_DMBEnable(void);
void INC_GPIO_DMBDisable(void);
void INC_GPIO_PMU_Enable(int bStatus);
void INC_GPIO_Reset(int bStart);
irqreturn_t INC_isr(int irq, void *handle);
int INC_GPIO_set_Interrupt(void);
int INC_GPIO_free_Interrupt(void);

unsigned char INC_SPI_SETUP(void); // by kdk
unsigned char INC_SPI_DRV_OPEN(void);
unsigned char INC_SPI_DRV_CLOSE(void);
unsigned char INC_SPI_DRV_RESET(void);
unsigned char INC_SPI_DRV_REG_WRITE(unsigned short uiAddr, unsigned short uiData);
unsigned char INC_SPI_DRV_MEM_READ(unsigned short uiAddr, unsigned char* pBuff, unsigned short wSize);
//unsigned char INC_SPI_DRV_READ_BURST(unsigned short uiAddr,unsigned char* buff,unsigned short length );
unsigned char INC_SPI_DRV_READ_BURST(unsigned char* tx_data, unsigned short tx_length, unsigned char* rx_data, unsigned short rx_length);

unsigned short INC_SPI_DRV_REG_READ(unsigned short uiAddr);






#endif // __INC_SPI_H__
