/*
 * include/linux/mfd/nxe1500.h
 *
 *  Copyright (C) 2016 Nexell
 *  Jongsin Park <pjsin865@nexell.co.kr>
 *
 * Based on code
 *	Copyright (C) 2011 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef __LINUX_MFD_NXE1500_H
#define __LINUX_MFD_NXE1500_H

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/i2c.h>

/* Maximum number of main interrupts */
#define MAX_INTERRUPT_MASKS			4
#define MAX_MAIN_INTERRUPT			7
#define MAX_GPEDGE_REG				2

/* Power control register */
#define NXE1500_PWR_WD				0x0B
#define NXE1500_PWR_WD_COUNT		0x0C
#define NXE1500_PWR_FUNC			0x0D
#define NXE1500_PWR_SLP_CNT			0x0E
#define NXE1500_PWR_REP_CNT			0x0F
#define NXE1500_PWR_ON_TIMSET		0x10
#define NXE1500_PWR_NOE_TIMSET		0x11
#define NXE1500_PWR_IRSEL			0x15
#define NXE1500_PWR_DC4_SLOT		0x19

/* Interrupt enable register */
#define NXE1500_INT_EN_SYS			0x12
#define NXE1500_INT_EN_DCDC			0x40
#define NXE1500_INT_EN_RTC			0xAE
#define NXE1500_INT_EN_ADC1			0x88
#define NXE1500_INT_EN_ADC2			0x89
#define NXE1500_INT_EN_ADC3			0x8A
#define NXE1500_INT_EN_GPIO			0x94
#define NXE1500_INT_EN_GPIO2		0x94 /* dummy */
#define NXE1500_INT_MSK_CHGCTR		0xBE
#define NXE1500_INT_MSK_CHGSTS1		0xBF
#define NXE1500_INT_MSK_CHGSTS2		0xC0
#define NXE1500_INT_MSK_CHGERR		0xC1
#define NXE1500_INT_MSK_CHGEXTIF	0xD1

/* Interrupt select register */
#define NXE1500_PWR_IRSEL			0x15
#define NXE1500_CHG_CTRL_DETMOD1	0xCA
#define NXE1500_CHG_CTRL_DETMOD2	0xCB
#define NXE1500_CHG_STAT_DETMOD1	0xCC
#define NXE1500_CHG_STAT_DETMOD2	0xCD
#define NXE1500_CHG_STAT_DETMOD3	0xCE


/* interrupt status registers (monitor regs)*/
#define NXE1500_INTC_INTPOL			0x9C
#define NXE1500_INTC_INTEN			0x9D
#define NXE1500_INTC_INTMON			0x9E

#define NXE1500_INT_MON_SYS			0x14
#define NXE1500_INT_MON_DCDC		0x42
#define NXE1500_INT_MON_RTC			0xAF

#define NXE1500_INT_MON_CHGCTR		0xC6
#define NXE1500_INT_MON_CHGSTS1		0xC7
#define NXE1500_INT_MON_CHGSTS2		0xC8
#define NXE1500_INT_MON_CHGERR		0xC9
#define NXE1500_INT_MON_CHGEXTIF	0xD3

/* interrupt clearing registers */
#define NXE1500_INT_IR_SYS			0x13
#define NXE1500_INT_IR_DCDC			0x41
#define NXE1500_INT_IR_RTC			0xAF
#define NXE1500_INT_IR_ADCL			0x8C
#define NXE1500_INT_IR_ADCH			0x8D
#define NXE1500_INT_IR_ADCEND		0x8E
#define NXE1500_INT_IR_GPIOR		0x95
#define NXE1500_INT_IR_GPIOF		0x96
#define NXE1500_INT_IR_CHGCTR		0xC2
#define NXE1500_INT_IR_CHGSTS1		0xC3
#define NXE1500_INT_IR_CHGSTS2		0xC4
#define NXE1500_INT_IR_CHGERR		0xC5
#define NXE1500_INT_IR_CHGEXTIF		0xD2

/* GPIO register base address */
#define NXE1500_GPIO_IOSEL			0x90
#define NXE1500_GPIO_IOOUT			0x91
#define NXE1500_GPIO_GPEDGE1		0x92
#define NXE1500_GPIO_GPEDGE2		0x93
#define NXE1500_GPIO_EN_GPIR		0x94
#define NXE1500_GPIO_IR_GPR			0x95
#define NXE1500_GPIO_IR_GPF			0x96
#define NXE1500_GPIO_MON_IOIN		0x97
#define NXE1500_GPIO_LED_FUNC		0x98

#define NXE1500_REG_BANKSEL			0xFF

#define	NXE1500_PSWR				0x07

/* NXE1500 IRQ definitions */
enum {
	NXE1500_IRQ_POWER_ON,
	NXE1500_IRQ_EXTIN,
	NXE1500_IRQ_PRE_VINDT,
	NXE1500_IRQ_PREOT,
	NXE1500_IRQ_POWER_OFF,
	NXE1500_IRQ_NOE_OFF,
	NXE1500_IRQ_WD,
	NXE1500_IRQ_CLK_STP,

	NXE1500_IRQ_DC1LIM,
	NXE1500_IRQ_DC2LIM,
	NXE1500_IRQ_DC3LIM,
	NXE1500_IRQ_DC4LIM,

	NXE1500_IRQ_GPIO0,
	NXE1500_IRQ_GPIO1,
	NXE1500_IRQ_GPIO2,
	NXE1500_IRQ_GPIO3,
	NXE1500_IRQ_GPIO4,

	/* Should be last entry */
	NXE1500_NR_IRQS,
};

/* NXE1500 gpio definitions */
enum {
	NXE1500_GPIO0,
	NXE1500_GPIO1,
	NXE1500_GPIO2,
	NXE1500_GPIO3,
	NXE1500_GPIO4,

	NXE1500_NR_GPIO,
};

enum nxe1500_sleep_control_id {
	NXE1500_DS_DC1,
	NXE1500_DS_DC2,
	NXE1500_DS_DC3,
	NXE1500_DS_DC4,
	NXE1500_DS_DC5,
	NXE1500_DS_LDO1,
	NXE1500_DS_LDO2,
	NXE1500_DS_LDO3,
	NXE1500_DS_LDO4,
	NXE1500_DS_LDO5,
	NXE1500_DS_LDO6,
	NXE1500_DS_LDO7,
	NXE1500_DS_LDO8,
	NXE1500_DS_LDO9,
	NXE1500_DS_LDO10,
	NXE1500_DS_LDORTC1,
	NXE1500_DS_LDORTC2,
	NXE1500_DS_PSO0,
	NXE1500_DS_PSO1,
	NXE1500_DS_PSO2,
	NXE1500_DS_PSO3,
	NXE1500_DS_PSO4,
};

enum int_type {
	SYS_INT		= 0x01,
	DCDC_INT	= 0x02,
	RTC_INT		= 0x04,
	ADC_INT		= 0x08,
	GPIO_INT	= 0x10,
	WDG_INT		= 0x20,
	CHG_INT		= 0x40,
	FG_INT		= 0x80,
};

struct nxe1500_subdev_info {
	int			id;
	const char	*name;
	void		*platform_data;
};

struct nxe1500_gpio_init_data {
	unsigned output_mode_en:1;	/* Enable output mode during init */
	unsigned output_val:1;	/* Output value if it is in output mode */
	unsigned init_apply:1;	/* Apply init data on configuring gpios*/
	unsigned led_mode:1;	/* Select LED mode during init */
	unsigned led_func:1;	/* Set LED function if LED mode is 1 */
};

struct nxe1500 {
	struct device		*dev;
	struct i2c_client	*client;
	struct mutex		io_lock;
	struct workqueue_struct *workqueue;
	struct delayed_work	dcdc_int_work;

	int			gpio_base;
	struct gpio_chip	gpio_chip;
	int			irq_base;
/*	struct irq_chip		irq_chip; */
	int			chip_irq;
	int			chip_irq_type;
	struct mutex		irq_lock;
	unsigned long		group_irq_en[MAX_MAIN_INTERRUPT];

	/* For main interrupt bits in INTC */
	u8			intc_inten_cache;
	u8			intc_inten_reg;

	/* For group interrupt bits and address */
	u8			irq_en_cache[MAX_INTERRUPT_MASKS];
	u8			irq_en_reg[MAX_INTERRUPT_MASKS];

	/* For gpio edge */
	u8			gpedge_cache[MAX_GPEDGE_REG];
	u8			gpedge_reg[MAX_GPEDGE_REG];

	int			bank_num;
};

struct nxe1500_platform_data {
	int		num_subdevs;
	struct	nxe1500_subdev_info *subdevs;
	int (*init_port)(int irq_num); /* Init GPIO for IRQ pin */
	int		gpio_base;
	int		irq_base;
	int		irq_type;
	struct nxe1500_gpio_init_data *gpio_init_data;
	int num_gpioinit_data;
	bool enable_shutdown_pin;
};

/* ==================================== */
/* NXE1500 Power_Key device data	*/
/* ==================================== */
struct nxe1500_pwrkey_platform_data {
	int irq;
	unsigned long delay_ms;
};
extern int pwrkey_wakeup;

extern int nxe1500_pm_read(uint8_t reg, uint8_t *val);
extern int nxe1500_read(struct device *dev, uint8_t reg, uint8_t *val);
extern int nxe1500_bulk_reads(struct device *dev, u8 reg, u8 count,
								uint8_t *val);
extern int nxe1500_pm_write(u8 reg, uint8_t val);
extern int nxe1500_write(struct device *dev, u8 reg, uint8_t val);
extern int nxe1500_bulk_writes(struct device *dev, u8 reg, u8 count,
								uint8_t *val);
extern int nxe1500_set_bits(struct device *dev, u8 reg, uint8_t bit_mask);
extern int nxe1500_clr_bits(struct device *dev, u8 reg, uint8_t bit_mask);
extern int nxe1500_update(struct device *dev, u8 reg, uint8_t val,
								uint8_t mask);
extern void nxe1500_power_off(void);
extern int nxe1500_irq_init(struct nxe1500 *nxe1500);
extern int nxe1500_irq_exit(struct nxe1500 *nxe1500);

#endif
