#ifndef __LINUX_AXP_CFG_H_
#define __LINUX_AXP_CFG_H_

#include <mach/platform.h>
#include "axp-mfd.h"

/* i2c slave address */
#define	AXP_DEVICES_ADDR		(0x68 >> 1)

/*i2c channel */
#define	AXP_I2CBUS				3

/* interrupt */
#define AXP_IRQNO				CFG_GPIO_PMIC_INTR // 164

#define AXP_ALDO1_VALUE			3300		/* VCC3P3_ALIVE		ALDO1	： 	AXP22:  700~3300, 100/step*/
#define AXP_ALDO2_VALUE			1800		/* VCC1P8_ALIVE		ALDO2	： 	AXP22:  700~3300, 100/step*/
#define AXP_ALDO3_VALUE			1000		/* VCC1P0_ALIVE		ALDO3	： 	AXP22:  700~3300, 100/step*/
#define AXP_DLDO1_VALUE			3300		/* VCC_WIDE			DLDO1	： 	AXP22:  700~3300, 100/step*/
#define AXP_DLDO2_VALUE			1800		/* VCC1P8_CAM			DLDO2	： 	AXP22 : 700~3300, 100/step*/
#define AXP_DLDO3_VALUE			700		/* NC					DLDO3	： 	AXP22:  700~3300, 100/step*/
#define AXP_DLDO4_VALUE			700		/* NC					DLDO4	： 	AXP22:  700~3300, 100/step*/
#define AXP_ELDO1_VALUE			1800		/* VCC1P8_SYS			ELDO1	： 	AXP22:  700~3300, 100/step*/
#define AXP_ELDO2_VALUE			3300		/* VCC3P3_WIFI			ELDO2	： 	AXP22:  700~3300, 100/step*/
#define AXP_ELDO3_VALUE			700		/* NC					ELDO3	： 	AXP22:  700~3300, 100/step*/
#define AXP_DC5LDO_VALUE		1200		/* VCC1P2_CVBS		DC5LDO	： 	AXP22:  700~1400, 100/step*/
#define AXP_DCDC1_VALUE			3300		/* VCC3P3_SYS			DCDC1	 : 	AXP22:1600~3400, 100/setp*/
#define AXP_DCDC2_VALUE			1100		/* VCC1P1_ARM			DCDC2	： 	AXP22:  600~1540,   20/step*/
#define AXP_DCDC3_VALUE			1100		/* VCC1P1_ARM(CORE)	DCDC3	： 	AXP22:  600~1860,   20/step*/
#define AXP_DCDC4_VALUE			1500		/* VCC1P5_SYS			DCDC4	： 	AXP22:  600~1540,   20/step*/
#define AXP_DCDC5_VALUE			1500		/* VCC1P5_DDR			DCDC5	： 	AXP22:1000~2550,   50/step*/


#define AXP_ALDO1_ENABLE		1		/* VCC3P3_ALIVE		ALDO1	： 	AXP22:  700~3300, 100/step*/
#define AXP_ALDO2_ENABLE		1		/* VCC1P8_ALIVE		ALDO2	： 	AXP22:  700~3300, 100/step*/
#define AXP_ALDO3_ENABLE		1		/* VCC1P0_ALIVE		ALDO3	： 	AXP22:  700~3300, 100/step*/
#define AXP_DLDO1_ENABLE		1		/* VCC_WIDE			DLDO1	： 	AXP22:  700~3300, 100/step*/
#define AXP_DLDO2_ENABLE		1		/* VCC1P8_CAM			DLDO2	： 	AXP22 : 700~3300, 100/step*/
#define AXP_DLDO3_ENABLE		0		/* NC					DLDO3	： 	AXP22:  700~3300, 100/step*/
#define AXP_DLDO4_ENABLE		0		/* NC					DLDO4	： 	AXP22:  700~3300, 100/step*/
#define AXP_ELDO1_ENABLE		1		/* VCC1P8_SYS			ELDO1	： 	AXP22:  700~3300, 100/step*/
#define AXP_ELDO2_ENABLE		1		/* VCC3P3_WIFI			ELDO2	： 	AXP22:  700~3300, 100/step*/
#define AXP_ELDO3_ENABLE		0		/* NC					ELDO3	： 	AXP22:  700~3300, 100/step*/
#define AXP_DC5LDO_ENABLE		0		/* VCC1P2_CVBS		DC5LDO	： 	AXP22:  700~1400, 100/step*/
#define AXP_DCDC1_ENABLE		1		/* VCC3P3_SYS			DCDC1	 : 	AXP22:1600~3400, 100/setp*/
#define AXP_DCDC2_ENABLE		1		/* VCC1P1_ARM			DCDC2	： 	AXP22:  600~1540,   20/step*/
#define AXP_DCDC3_ENABLE		1		/* VCC1P1_ARM(CORE)	DCDC3	： 	AXP22:  600~1860,   20/step*/
#define AXP_DCDC4_ENABLE		1		/* VCC1P5_SYS			DCDC4	： 	AXP22:  600~1540,   20/step*/
#define AXP_DCDC5_ENABLE		1		/* VCC1P5_DDR			DCDC5	： 	AXP22:1000~2550,   50/step*/


#define AXP_DLDO1_SLEEP_OFF		(1)		/* VCC_WIDE			0 : sleep on, 1 : sleep off */
#define AXP_DLDO2_SLEEP_OFF		(1)		/* VCC1P8_CAM			0 : sleep on, 1 : sleep off */
#define AXP_DLDO3_SLEEP_OFF		(1)		/* NC					0 : sleep on, 1 : sleep off */
#define AXP_DLDO4_SLEEP_OFF		(1)		/* NC					0 : sleep on, 1 : sleep off */
#define AXP_ELDO1_SLEEP_OFF		(1)		/* VCC1P8_SYS			0 : sleep on, 1 : sleep off */
#define AXP_ELDO2_SLEEP_OFF		(1)		/* VCC3P3_WIFI			0 : sleep on, 1 : sleep off */
#define AXP_ELDO3_SLEEP_OFF		(1)		/* NC					0 : sleep on, 1 : sleep off */
#define AXP_DC5LDO_SLEEP_OFF	(1)		/* VCC1P2_CVBS		0 : sleep on, 1 : sleep off */
#define AXP_DCDC1_SLEEP_OFF		(1)		/* VCC3P3_SYS			0 : sleep on, 1 : sleep off */
#define AXP_DCDC2_SLEEP_OFF		(1)		/* VCC1P1_ARM			0 : sleep on, 1 : sleep off */
#define AXP_DCDC3_SLEEP_OFF		(1)		/* VCC1P1_ARM(CORE)	0 : sleep on, 1 : sleep off */
#define AXP_DCDC4_SLEEP_OFF		(1)		/* VCC1P5_SYS			0 : sleep on, 1 : sleep off */
#define AXP_DCDC5_SLEEP_OFF		(0)		/* VCC1P5_DDR			0 : sleep on, 1 : sleep off */
#define AXP_ALDO1_SLEEP_OFF		(0)		/* VCC3P3_ALIVE		0 : sleep on, 1 : sleep off */
#define AXP_ALDO2_SLEEP_OFF		(0)		/* VCC1P8_ALIVE		0 : sleep on, 1 : sleep off */
#define AXP_ALDO3_SLEEP_OFF		(0)		/* VCC1P0_ALIVE		0 : sleep on, 1 : sleep off */

#define BATCAP				CFG_BATTERY_CAP

#define BATRDC				137 //100 

#define AXP_VOL_MAX			1

#define CHGEN				1

#define STACHGCUR			1500*1000		/* AXP22:300~2550,100/step */
#define EARCHGCUR			1500*1000		/* AXP22:300~2550,100/step */
#define SUSCHGCUR			1500*1000		/* AXP22:300~2550,100/step */
#define CLSCHGCUR			1500*1000		/* AXP22:300~2550,100/step */

/* AC current charge */
#define AC_CHARGE_CURRENT	1500*1000

/* AC current limit */
#define AC_LIMIT_CURRENT	1500*1000

#define CHGVOL				4200*1000		/* AXP22:4100/4220/4200/4240 */

#define ENDCHGRATE			10		/* AXP22:10\15 */ 

#define SHUTDOWNVOL			3300

#define ADCFREQ				100		/* AXP22:100\200\400\800 */

#define CHGPRETIME			50		/* AXP22:40\50\60\70 */

#define CHGCSTTIME			480		/* AXP22:360\480\600\720 */

/* pok open time set */
#define PEKOPEN				1000		/* AXP22:128/1000/2000/3000 */

/* pok long time set*/
#define PEKLONG				1500		/* AXP22:1000/1500/2000/2500 */

/* pek offlevel poweroff en set*/
#define PEKOFFEN			1

/*Init offlevel restart or not */
#define PEKOFFRESTART		0

/* pek delay set */
#define PEKDELAY			32		/* AXP20:8/16/32/64 */

/*  pek offlevel time set */
#define PEKOFF				6000		/* AXP22:4000/6000/8000/10000 */

/* Init PMU Over Temperature protection*/
#define OTPOFFEN			0

#define USBVOLLIMEN			1

#define USBVOLLIM			4700		/* AXP22:4000~4700，100/step */

#define USBVOLLIMPC			4700		/* AXP22:4000~4700，100/step */

#define USBCURLIMEN			0

#define USBCURLIM			500		/* AXP22:500/900 */

#define USBCURLIMPC			0		/* AXP22:500/900 */

/* Init IRQ wakeup en*/
#define IRQWAKEUP			0

/* Init N_VBUSEN status*/
#define VBUSEN				1

/* Init InShort status*/
#define VBUSACINSHORT		0

/* Init CHGLED function*/
#define CHGLEDFUN			1

/* set CHGLED Indication Type*/
#define CHGLEDTYPE			0

/* Init battery capacity correct function*/
#define BATCAPCORRENT		1

/* Init battery regulator enable or not when charge finish*/
#define BATREGUEN			0

#define BATDET				1

/* Init 16's Reset PMU en */
#define PMURESET			0

/* set lowe power warning level */
#define BATLOWLV1			15		/* AXP22:5%~20% */

/* set lowe power shutdown level */
#define BATLOWLV2			0		/* AXP22:0%~15% */

#define ABS(x)				((x) >0 ? (x) : -(x) )

#ifdef	CONFIG_KP_AXP22

/*AXP GPIO start NUM, */
#define AXP22_NR_BASE 		100

/*AXP GPIO NUM, LCD power VBUS driver pin*/
#define AXP22_NR 			5

/* OCV Table */
#if 1
#define OCVREG0				0		//3.13V
#define OCVREG1				0		//3.27V
#define OCVREG2				0		//3.34V
#define OCVREG3				0		//3.41V
#define OCVREG4				0		//3.48V
#define OCVREG5				0		//3.52V
#define OCVREG6				0		//3.55V
#define OCVREG7				0		//3.57V
#define OCVREG8				0		//3.59V
#define OCVREG9				0		//3.61V
#define OCVREGA				1		//3.63V
#define OCVREGB				2		//3.64V
#define OCVREGC				3		//3.66V
#define OCVREGD				8		//3.70V
#define OCVREGE				15		//3.73V 
#define OCVREGF				23		//3.77V
#define OCVREG10			29		//3.78V
#define OCVREG11			37		//3.80V
#define OCVREG12			42		//3.82V 
#define OCVREG13			48		//3.84V
#define OCVREG14			51		//3.85V
#define OCVREG15			55		//3.87V
#define OCVREG16			62		//3.91V
#define OCVREG17			68		//3.94V
#define OCVREG18			73		//3.98V
#define OCVREG19			78		//4.01V
#define OCVREG1A			82		//4.05V
#define OCVREG1B			86		//4.08V
#define OCVREG1C			89		//4.10V 
#define OCVREG1D			93		//4.12V
#define OCVREG1E			96		//4.14V
#define OCVREG1F			100		//4.15V
#else
#define OCVREG0				0		 //3.13V
#define OCVREG1				0		 //3.27V
#define OCVREG2				0		 //3.34V
#define OCVREG3				0		 //3.41V
#define OCVREG4				0		 //3.48V
#define OCVREG5				0		 //3.52V
#define OCVREG6				0		 //3.55V
#define OCVREG7				0		 //3.57V
#define OCVREG8				5		 //3.59V
#define OCVREG9				8		 //3.61V
#define OCVREGA				9		 //3.63V
#define OCVREGB				10		 //3.64V
#define OCVREGC				13		 //3.66V
#define OCVREGD				16		 //3.7V
#define OCVREGE				20		 //3.73V 
#define OCVREGF				33		 //3.77V
#define OCVREG10		 	41                //3.78V
#define OCVREG11		 	46                //3.8V
#define OCVREG12		 	50                //3.82V 
#define OCVREG13		 	53                //3.84V
#define OCVREG14		 	57                //3.85V
#define OCVREG15		 	61                //3.87V
#define OCVREG16		 	67                //3.91V
#define OCVREG17		 	73                //3.94V
#define OCVREG18		 	78                //3.98V
#define OCVREG19		 	84                //4.01V
#define OCVREG1A		 	88                //4.05V
#define OCVREG1B		 	92                //4.08V
#define OCVREG1C		 	93                //4.1V 
#define OCVREG1D		 	94                //4.12V
#define OCVREG1E		 	95                //4.14V
#define OCVREG1F		 	100                //4.15V
#endif

/*  AXP IRQ */
#define AXP_IRQ_USBLO		AXP22_IRQ_USBLO	//usb 低电
#define AXP_IRQ_USBRE		AXP22_IRQ_USBRE	//usb 拔出
#define AXP_IRQ_USBIN		AXP22_IRQ_USBIN	//usb 插入
#define AXP_IRQ_USBOV		AXP22_IRQ_USBOV	//usb 过压
#define AXP_IRQ_ACRE		AXP22_IRQ_ACRE	//ac  拔出
#define AXP_IRQ_ACIN		AXP22_IRQ_ACIN	//ac  插入
#define AXP_IRQ_ACOV		AXP22_IRQ_ACOV //ac  过压
#define AXP_IRQ_TEMLO		AXP22_IRQ_TEMLO //电池低温
#define AXP_IRQ_TEMOV		AXP22_IRQ_TEMOV //电池过温
#define AXP_IRQ_CHAOV		AXP22_IRQ_CHAOV //电池充电结束
#define AXP_IRQ_CHAST		AXP22_IRQ_CHAST //电池充电开始
#define AXP_IRQ_BATATOU		AXP22_IRQ_BATATOU //电池退出激活模式
#define AXP_IRQ_BATATIN		AXP22_IRQ_BATATIN //电池进入激活模式
#define AXP_IRQ_BATRE		AXP22_IRQ_BATRE //电池拔出
#define AXP_IRQ_BATIN		AXP22_IRQ_BATIN //电池插入
#define AXP_IRQ_PEKLO		AXP22_IRQ_POKLO //power键长按
#define AXP_IRQ_PEKSH		AXP22_IRQ_POKSH //power键短按

#define AXP_IRQ_CHACURLO	AXP22_IRQ_CHACURLO //充电电流小于设置值
#define AXP_IRQ_ICTEMOV		AXP22_IRQ_ICTEMOV //AXP芯片内部过温
#define AXP_IRQ_EXTLOWARN2	AXP22_IRQ_EXTLOWARN2 //APS低压警告电压2
#define AXP_IRQ_EXTLOWARN1	AXP22_IRQ_EXTLOWARN1 //APS低压警告电压1

#define AXP_IRQ_GPIO0TG		AXP22_IRQ_GPIO0TG //GPIO0输入边沿触发
#define AXP_IRQ_GPIO1TG		AXP22_IRQ_GPIO1TG //GPIO1输入边沿触发

#define AXP_IRQ_PEKFE		AXP22_IRQ_PEKFE //power键下降沿触发
#define AXP_IRQ_PEKRE		AXP22_IRQ_PEKRE //power键上升沿触发
#define AXP_IRQ_TIMER		AXP22_IRQ_TIMER //计时器超时

#endif

static const uint64_t AXP22_NOTIFIER_ON = (AXP_IRQ_USBIN |AXP_IRQ_USBRE |
											AXP_IRQ_ACIN |AXP_IRQ_ACRE |
											AXP_IRQ_BATIN |AXP_IRQ_BATRE |
											AXP_IRQ_CHAST |AXP_IRQ_CHAOV |
											AXP_IRQ_PEKLO |AXP_IRQ_PEKSH |
											(uint64_t)AXP_IRQ_PEKFE |
											(uint64_t)AXP_IRQ_PEKRE |
											(uint64_t)AXP_IRQ_EXTLOWARN2 |
											(uint64_t)AXP_IRQ_EXTLOWARN1);


#define POWER_START 		0


/* debug log */
//#define ENABLE_DEBUG
//#define DBG_AXP_PSY

#endif
