#ifndef __MTV_GPIO_H__
#define __MTV_GPIO_H__


#ifdef __cplusplus 
extern "C"{ 
#endif  

#include <mach/gpio.h>
#include <mach/platform.h>
#include <mach/soc.h>
	
		//#error "code not present"
	#define MTV_PWR_EN                  CFG_MTV_ENABLE // CFG_DMB0_ENABLE
	//#define MTV_PWR_EN_1                // CFG_DMB1_ENABLE
		
	#define TDMB_IRQ_INT               CFG_MTV_IRQ//CFG_DMB0_IRQ
//	#define TDMB_IRQ_INT_1             //CFG_DMB1_IRQ
		// tnn
	#define RAONTV_CS_SPI1				CFG_SPI2_CS
//	#define RAONTV_CS_SPI2			//	CFG_SPI2_CS
		
	static inline int mtv_configure_gpio(void)
	{
		//printk("TNN _______ MTV_PWR_EN _______%d \n ",MTV_PWR_EN);
		//printk("TNN _______ MTV_PWR_EN_1_______%d \n ",MTV_PWR_EN_1);
		//printk("TNN __ RAONTV_IRQ_INT __%d \n ",RAONTV_IRQ_INT);
		//printk("TNN __ RAONTV_IRQ_INT_1____________%d \n ",RAONTV_IRQ_INT_1);

		return 0;
	}

#ifdef __cplusplus 
} 
#endif 

#endif /* __MTV_GPIO_H__*/
