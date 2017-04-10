/*
 * (C) Copyright 2010
 * Youngbok Park, Nexell Co, <ybpark@nexell.co.kr>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/time.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "./nxp_iso.h"
/*
#define	pr_debug	printk
*/

#define RESET_UART_ID 50
#define DRVNAME	"bcasif"

#define UART_PL01x_FR_TXFE	0x80
#define UART_PL01x_FR_RXFF	0x40
#define UART_PL01x_FR_TXFF	0x20
#define UART_PL01x_FR_RXFE	0x10
#define UART_PL01x_FR_BUSY	0x08
#define UART_PL01x_FR_TMSK	(UART_PL01x_FR_TXFF + UART_PL01x_FR_BUSY)

#define UART_PL011_LCRH_SPS	(1 << 7)
#define UART_PL011_LCRH_WLEN_8	(3 << 5)
#define UART_PL011_LCRH_WLEN_7	(2 << 5)
#define UART_PL011_LCRH_WLEN_6	(1 << 5)
#define UART_PL011_LCRH_WLEN_5	(0 << 5)
#define UART_PL011_LCRH_FEN	(1 << 4)
#define UART_PL011_LCRH_STP2	(1 << 3)
#define UART_PL011_LCRH_EPS	(1 << 2)
#define UART_PL011_LCRH_PEN	(1 << 1)
#define UART_PL011_LCRH_BRK	(1 << 0)

#define UART_PL011_CR_CTSEN	(1 << 15)
#define UART_PL011_CR_RTSEN	(1 << 14)
#define UART_PL011_CR_OUT2	(1 << 13)
#define UART_PL011_CR_OUT1	(1 << 12)
#define UART_PL011_CR_RTS	(1 << 11)
#define UART_PL011_CR_DTR	(1 << 10)
#define UART_PL011_CR_RXE	(1 << 9)
#define UART_PL011_CR_TXE	(1 << 8)
#define UART_PL011_CR_LPE	(1 << 7)
#define UART_PL011_CR_IIRLP	(1 << 2)
#define UART_PL011_CR_SIREN	(1 << 1)
#define UART_PL011_CR_UARTEN	(1 << 0)

#define UART_DR		0x0
#define UART_ECR	0x04
#define UART_FR		0x18
#define UART_IBRD	0x24
#define UART_IFRD	0x28
#define UART_LCRH	0x2c
#define UART_CR		0x30

struct nxp_iso7816 {
	struct clk *clk;
	struct pwm_device	*pwm;
	unsigned int	period;
	unsigned int	use_pwm;
	int rst_gpio;
	int uart_ch;
	int uart_reg;
	unsigned int divider;
	unsigned int fraction;
	unsigned int lcr;
	unsigned int cr;

};

struct scr_cmd {
	unsigned int rlen;
	unsigned int wlen;
	unsigned char buf[256];
};

static struct nxp_iso7816 *pdata;
static char atr[13];

static char uart_putc(char ch)
{
	unsigned int status;

	/* Wait until there is space in the FIFO */
	while ((readl(pdata->uart_reg + UART_FR) & UART_PL01x_FR_TXFF))
		;

	/* Send the character */
	writel(ch, pdata->uart_reg + UART_DR);

	/*
	* Finally, wait for transmitter to become empty
	*/
	do {
		status = readw(pdata->uart_reg + UART_FR);
	} while (status & UART_PL01x_FR_BUSY);

	return 0;
}

static int write_uart(char *buf, int len)
{
	int i = 0;
	while (len--)
		uart_putc(buf[i++]);
	return i;
}

static int read_uart(char *buf, int len, unsigned long timeout)
{
	int i = 0;
	unsigned long b_start = jiffies;
	unsigned long start = jiffies;

	while (len--) {
		b_start = jiffies;
		while ((readl(pdata->uart_reg + UART_FR)
					& UART_PL01x_FR_RXFE)) {
			if (time_after(jiffies, b_start + 50))
				return -1;
		}

		buf[i] = readl(pdata->uart_reg + UART_DR);
		if (buf[i] & 0xFFFFFF00) {
			/* Clear the error */
			writel(0xFFFFFFFF, pdata->uart_reg + UART_ECR);
			return -1;
		}
		i++;

		if (time_after(jiffies, start + timeout)) {
			pr_err("%s: fail uart recive timeout\n", __func__);
			return -1;
		}
	}
	return i;
}

static void read_atr(void)
{
	int len = 13;

	read_uart(atr, len, 500);
}

static unsigned int iso7816_get_base(int ch)
{
	int reg = 0;
	switch (ch) {
	case 0:
		reg = IO_ADDRESS(PHY_BASEADDR_UART0);
		break;
	case 1:
		reg = IO_ADDRESS(PHY_BASEADDR_UART1);
		break;
	case 2:
		reg = IO_ADDRESS(PHY_BASEADDR_UART2);
		break;
	case 3:
		reg = IO_ADDRESS(PHY_BASEADDR_UART3);
		break;
	case 4:
		reg = IO_ADDRESS(PHY_BASEADDR_UART4);
		break;
	case 5:
		reg = IO_ADDRESS(PHY_BASEADDR_UART5);
		break;
	}
	return reg;
}

static int tieoff_init(int ch)
{
	switch (ch) {
	case 0:
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART0_USESMC, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART0_SMCTXENB, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART0_SMCRXENB, 1);
			break;
	case 1:
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_MODEM0_USESMC, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_MODEM0_SMCTXENB, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_MODEM0_SMCRXENB, 1);
			break;
	case 2:
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART1_USESMC, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART1_SMCTXENB, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART1_SMCRXENB, 1);
			break;
	case 3:
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA0_USESMC, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA0_SMCTXENB, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA0_SMCRXENB, 1);
			break;
	case 4:
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA1_USESMC, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA1_SMCTXENB, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA1_SMCRXENB, 1);
		break;
	case 5:
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA2_USESMC, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA2_SMCTXENB, 1);
		NX_TIEOFF_Set(TIEOFFINDEX_OF_UART_NODMA2_SMCRXENB, 1);
		break;
	}
	return 0;
}

static int iso7816_set_baud(int ch, int baud)
{
	unsigned int temp = 0, remainder;
	unsigned long rate;

	rate = clk_get_rate(pdata->clk);

	temp = 16 * baud;
	pdata->divider = rate / temp;
	remainder = rate % temp;
	temp = (8 * remainder) / baud;
	pdata->fraction = (temp >> 1) + (temp & 1);

	writel(0, pdata->uart_reg + UART_CR);
	writel(pdata->divider, pdata->uart_reg + UART_IBRD);
	writel(pdata->fraction, pdata->uart_reg + UART_IFRD);
	writel(pdata->lcr, pdata->uart_reg + UART_LCRH);
	writel(pdata->cr, pdata->uart_reg + UART_CR);

	return 0;
}

static int iso7816_uart_init(int ch)
{
	static struct clk *uart_clk;

	char buf[16] = {0, };

	tieoff_init(ch);
	nxp_soc_peri_reset_set(RESET_UART_ID + ch);

	sprintf(buf, "%s.%d", DEV_NAME_UART, ch);
	uart_clk = clk_get(NULL, buf);

	clk_set_rate(uart_clk, 100000000);
	clk_enable(uart_clk);

	pdata->clk = uart_clk;

	iso7816_set_baud(pdata->uart_ch, 9600);
	pdata->lcr = UART_PL011_LCRH_WLEN_8 | UART_PL011_LCRH_FEN |
		UART_PL011_LCRH_EPS | UART_PL011_LCRH_PEN ;
	pdata->cr = UART_PL011_CR_UARTEN | UART_PL011_CR_TXE |
		UART_PL011_CR_RXE | UART_PL011_CR_RTS | UART_PL011_CR_DTR;

	writel(pdata->lcr, pdata->uart_reg + UART_LCRH);
	writel(pdata->cr, pdata->uart_reg + UART_CR);
	return 0;
}

#ifdef CONFIG_PM
static int nxp_iso_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int nxp_iso_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define nxp_iso_suspend NULL
#define nxp_iso_resume NULL
#endif
static int nxp_iso7816_reset(void)
{
	iso7816_set_baud(pdata->uart_ch, 9600);

	gpio_set_value(pdata->rst_gpio, 0);
	mdelay(100);
	gpio_set_value(pdata->rst_gpio, 1);

	read_atr();
	iso7816_set_baud(pdata->uart_ch, 19200);

	return 0;
}

static void nxp_pwm_init(void)
{
	if (pdata->use_pwm) {
		pwm_config(pdata->pwm, pdata->period/2 , pdata->period);
		pwm_enable(pdata->pwm);
	}
}


static void nxp_pwm_disable(void)
{
	if (pdata->use_pwm) {
		pwm_disable(pdata->pwm);
		pwm_free(pdata->pwm);
	}
}

static int nxp_iso_open(struct inode *inode, struct file *flip)
{
	nxp_pwm_init();
	iso7816_uart_init(pdata->uart_ch);
	nxp_iso7816_reset();

	return 0;
}

static int nxp_iso_close(struct inode *inode, struct file *flip)
{
	nxp_pwm_disable();
	gpio_free(pdata->rst_gpio);
	return 0;
}

static long nxp_iso_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct scr_cmd scr_cmd;
	int ret = -1;

	switch (cmd) {
	case SCR_IO_REINIT:
		nxp_iso7816_reset();
		ret = 0;
	break;

	case SCR_IO_GET_CARD_STATUS:
		ret = 1;
	break;

	case SCR_IO_GET_ATR:
		if (0 != copy_from_user(&scr_cmd, (void *)arg,
					sizeof(struct scr_cmd))) {
			pr_err("%s : no scr cmd\n", __func__);
			ret = -1;
			break;

		}
		scr_cmd.rlen = 13;
		memcpy(scr_cmd.buf, atr, 13);
		if (0 != copy_to_user((void *)arg, &scr_cmd,
					sizeof(struct scr_cmd))) {
			pr_err("%s : atr data copy\n", __func__);
			ret = -1;
			break;
		}
	ret = 0;
	break;

	case SCR_IO_SET_ETU:
	break;

	case SCR_IO_READ:
	break;

	case SCR_IO_WRITE:
		if (0 != copy_from_user(&scr_cmd, (void *)arg,
					sizeof(struct scr_cmd))) {
			pr_err("%s : no scr cmd\n", __func__);
			ret = -1;
			break;
		}

		write_uart(scr_cmd.buf, scr_cmd.wlen);
		ret = read_uart(scr_cmd.buf, scr_cmd.wlen, 500);
		ret = read_uart(scr_cmd.buf, scr_cmd.rlen, 500);
		if (ret < 0) {
			pr_err("%s : read Bcas data fail!\n", __func__);
			return -1;
		}

		if (0 != copy_to_user((void *)arg, &scr_cmd,
					sizeof(struct scr_cmd))) {
			pr_err("%s :fail data copy\n", __func__);
			ret = -1;
			break;
		}
		ret = 0;

		break;
	}

	return ret;
}

const struct file_operations nxp_iso_ops = {
	.owner		= THIS_MODULE,
	.open		= nxp_iso_open,
	.release	= nxp_iso_close,
	.unlocked_ioctl	= nxp_iso_ioctl,
};

static struct class *nxpiso_class;
static int __devinit nxp_iso_probe(struct platform_device *pdev)
{
	struct nxp_iso7816_platdata *plat = pdev->dev.platform_data;
	int ret, iso_major;

	pdata = kzalloc(sizeof(struct nxp_iso7816), GFP_KERNEL);

	if (!plat) {
		pr_err("%s, No platform data....\n", __func__);
		return -EINVAL;
	}

	pdata->uart_ch = plat->uart_ch;
	pdata->uart_reg = iso7816_get_base(pdata->uart_ch);

	if (plat->use_pwm) {
		pdata->pwm = pwm_request(plat->pwm_ch, "iso7816");
		if (IS_ERR(pdata->pwm)) {
			pr_err("%s fail request pwm %d\n",
					__func__, plat->pwm_ch);
			return -EINVAL;
		}
		pdata->period = plat->pwm_period;
		pdata->use_pwm = 1;
	}

	pdata->rst_gpio = plat->rst_gpio;
	ret = gpio_request(pdata->rst_gpio, "bcas_rst");
	if (ret < 0) {
		pr_err("%s, fail Requeset reset gpio\n", __func__);
		return -EINVAL;
	}
	gpio_direction_output(pdata->rst_gpio, 1);
	gpio_set_value(pdata->rst_gpio, 1);

	iso_major = register_chrdev(0, DRVNAME, &nxp_iso_ops);
	nxpiso_class = class_create(THIS_MODULE, DRVNAME);

	device_create(nxpiso_class, NULL, MKDEV(iso_major, 0), NULL, DRVNAME);
	platform_set_drvdata(pdev, pdata);

	return 0;
}

static int __devexit nxp_iso_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver nxp_iso_driver = {
	.driver = {
		.name	= DRVNAME,
		.owner	= THIS_MODULE,
	},
	.probe = nxp_iso_probe,
	.remove	= __devexit_p(nxp_iso_remove),
	.suspend = nxp_iso_suspend,
	.resume = nxp_iso_resume,
};

static int __init nxp_iso_init(void)
{
	return platform_driver_register(&nxp_iso_driver);
}
late_initcall(nxp_iso_init);

MODULE_AUTHOR("Youngbok Park <ybpark@nexell.co.kr>");
MODULE_DESCRIPTION("SLsiAP iso7816 monitor");
MODULE_LICENSE("GPL");
