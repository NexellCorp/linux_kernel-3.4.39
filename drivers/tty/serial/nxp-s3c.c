/*
 * Driver core for Samsung SoC onboard UARTs.
 *
 * Ben Dooks, Copyright (c) 2003-2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/* Hote on 2410 error handling
 *
 * The s3c2410 manual has a love/hate affair with the contents of the
 * UERSTAT register in the UART blocks, and keeps marking some of the
 * error bits as reserved. Having checked with the s3c2410x01,
 * it copes with BREAKs properly, so I am happy to ignore the RESERVED
 * feature from the latter versions of the manual.
 *
 * If it becomes aparrent that latter versions of the 2410 remove these
 * bits, then action will have to be taken to differentiate the versions
 * and change the policy on BREAK
 *
 * BJD, 04-Nov-2004
*/

#if defined(CONFIG_SERIAL_NXP_S3C_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/of.h>
#include <linux/wakelock.h>

#include <asm/irq.h>
#include <mach/serial.h>

#include "nxp-s3c.h"

#if 0
#define dbg(x...)	printk(x)
#else
#define dbg(x...) 	do { } while (0)
#endif

/* UART name and device definitions */

#define S3C24XX_SERIAL_NAME		"ttySAC"
#define S3C24XX_SERIAL_MAJOR	204
#define S3C24XX_SERIAL_MINOR	64

static struct s3c24xx_serial_drv_data exynos4210_serial_drv_data = {
	.info = &(struct s3c24xx_uart_info) {
		.name			= "Nexell Exynos4 UART",
		.type			= PORT_S3C6400,
		.has_divslot	= 1,
		.rx_fifomask	= S5PV210_UFSTAT_RXMASK,
		.rx_fifoshift	= S5PV210_UFSTAT_RXSHIFT,
		.rx_fifofull	= S5PV210_UFSTAT_RXFULL,
		.tx_fifofull	= S5PV210_UFSTAT_TXFULL,
		.tx_fifomask	= S5PV210_UFSTAT_TXMASK,
		.tx_fifoshift	= S5PV210_UFSTAT_TXSHIFT,
		.def_clk_sel	= S3C2410_UCON_CLKSEL0,
		.num_clks		= 1,
		.clksel_mask	= 0,
		.clksel_shift	= 0,
	},
	.def_data = &(struct s3c24xx_uart_platdata) {
		.ucon		= S5PV210_UCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
		.has_fracval	= 1,
	},
	.fifosize = { 256, 64, 16, 16 },
};
#define EXYNOS4210_SERIAL_DRV_DATA (&exynos4210_serial_drv_data)

/* macros to change one thing to another */

#define tx_enabled(port) ((port)->unused[0])
#define rx_enabled(port) ((port)->unused[1])

/* flag to ignore all characters coming in */
#define RXSTAT_DUMMY_READ (0x10000000)

static inline struct s3c24xx_uart_port *to_uport(struct uart_port *port)
{
	return container_of(port, struct s3c24xx_uart_port, port);
}

/* translate a port to the device name */

static inline const char *s3c24xx_serial_portname(struct uart_port *port)
{
	return to_platform_device(port->dev)->name;
}

static int s3c24xx_serial_txempty_nofifo(struct uart_port *port)
{
	return (rd_regl(port, S3C2410_UTRSTAT) & S3C2410_UTRSTAT_TXE);
}

/*
 * s3c64xx and later SoC's include the interrupt mask and status registers in
 * the controller itself, unlike the s3c24xx SoC's which have these registers
 * in the interrupt controller. Check if the port type is s3c64xx or higher.
 */
static int s3c24xx_serial_has_interrupt_mask(struct uart_port *port)
{
	return to_uport(port)->info->type == PORT_S3C6400;
}

static void s3c24xx_serial_rx_enable(struct uart_port *port)
{
	unsigned long flags;
	unsigned int ucon, ufcon;
	int count = 10000;

	spin_lock_irqsave(&port->lock, flags);

	while (--count && !s3c24xx_serial_txempty_nofifo(port))
		udelay(100);

	ufcon = rd_regl(port, S3C2410_UFCON);
	ufcon |= S3C2410_UFCON_RESETRX;
	wr_regl(port, S3C2410_UFCON, ufcon);

	ucon = rd_regl(port, S3C2410_UCON);
	ucon |= S3C2410_UCON_RXIRQMODE;
	wr_regl(port, S3C2410_UCON, ucon);

	rx_enabled(port) = 1;
	spin_unlock_irqrestore(&port->lock, flags);
}

static void s3c24xx_serial_rx_disable(struct uart_port *port)
{
	unsigned long flags;
	unsigned int ucon;

	spin_lock_irqsave(&port->lock, flags);

	ucon = rd_regl(port, S3C2410_UCON);
	ucon &= ~S3C2410_UCON_RXIRQMODE;
	wr_regl(port, S3C2410_UCON, ucon);

	rx_enabled(port) = 0;
	spin_unlock_irqrestore(&port->lock, flags);
}

static void s3c24xx_serial_stop_tx(struct uart_port *port)
{
	struct s3c24xx_uart_port *uport = to_uport(port);

	if (tx_enabled(port)) {
		if (s3c24xx_serial_has_interrupt_mask(port))
			__set_bit(S3C64XX_UINTM_TXD,
				portaddrl(port, S3C64XX_UINTM));
		else
			disable_irq_nosync(uport->tx_irq);
		tx_enabled(port) = 0;
		if (port->flags & UPF_CONS_FLOW)
			s3c24xx_serial_rx_enable(port);
	}
}

static void s3c24xx_serial_start_tx(struct uart_port *port)
{
	struct s3c24xx_uart_port *uport = to_uport(port);

	if (!tx_enabled(port)) {
		if (port->flags & UPF_CONS_FLOW)
			s3c24xx_serial_rx_disable(port);

		if (s3c24xx_serial_has_interrupt_mask(port))
			__clear_bit(S3C64XX_UINTM_TXD,
				portaddrl(port, S3C64XX_UINTM));
		else
			enable_irq(uport->tx_irq);
		tx_enabled(port) = 1;
	}
}

static void s3c24xx_serial_stop_rx(struct uart_port *port)
{
	struct s3c24xx_uart_port *uport = to_uport(port);

	if (rx_enabled(port)) {
		dbg("s3c24xx_serial_stop_rx: port=%p\n", port);
		if (s3c24xx_serial_has_interrupt_mask(port))
			__set_bit(S3C64XX_UINTM_RXD,
				portaddrl(port, S3C64XX_UINTM));
		else
			disable_irq_nosync(uport->rx_irq);
		rx_enabled(port) = 0;
	}
}

static void s3c24xx_serial_enable_ms(struct uart_port *port)
{
}

static inline struct s3c24xx_uart_info *s3c24xx_port_to_info(struct uart_port *port)
{
	return to_uport(port)->info;
}

static inline struct s3c24xx_uart_platdata *s3c24xx_port_to_data(struct uart_port *port)
{
	struct s3c24xx_uart_port *uport;

	if (port->dev == NULL)
		return NULL;

	uport = container_of(port, struct s3c24xx_uart_port, port);
	return uport->data;
}

static int s3c24xx_serial_rx_fifocnt(struct s3c24xx_uart_port *uport,
				     unsigned long ufstat)
{
	struct s3c24xx_uart_info *info = uport->info;

	if (ufstat & info->rx_fifofull)
		return uport->port.fifosize;

	return (ufstat & info->rx_fifomask) >> info->rx_fifoshift;
}


/* ? - where has parity gone?? */
#define S3C2410_UERSTAT_PARITY (0x1000)

static irqreturn_t
s3c24xx_serial_rx_chars(int irq, void *dev_id)
{
	struct s3c24xx_uart_port *uport = dev_id;
	struct uart_port *port = &uport->port;
	struct tty_struct *tty = port->state->port.tty;
	unsigned int ufcon, ch, flag, ufstat, uerstat;
	int max_count = 64;

	while (max_count-- > 0) {
		ufcon = rd_regl(port, S3C2410_UFCON);
		ufstat = rd_regl(port, S3C2410_UFSTAT);

		if (s3c24xx_serial_rx_fifocnt(uport, ufstat) == 0)
			break;

		uerstat = rd_regl(port, S3C2410_UERSTAT);
		ch = rd_regb(port, S3C2410_URXH);

		if (port->flags & UPF_CONS_FLOW) {
			int txe = s3c24xx_serial_txempty_nofifo(port);

			if (rx_enabled(port)) {
				if (!txe) {
					rx_enabled(port) = 0;
					continue;
				}
			} else {
				if (txe) {
					ufcon |= S3C2410_UFCON_RESETRX;
					wr_regl(port, S3C2410_UFCON, ufcon);
					rx_enabled(port) = 1;
					goto out;
				}
				continue;
			}
		}

		/* insert the character into the buffer */

		flag = TTY_NORMAL;
		port->icount.rx++;

		if (unlikely(uerstat & S3C2410_UERSTAT_ANY)) {
			dbg("rxerr: port ch=0x%02x, rxs=0x%08x\n",
			    ch, uerstat);

			/* check for break */
			if (uerstat & S3C2410_UERSTAT_BREAK) {
				dbg("break!\n");
				port->icount.brk++;
				if (uart_handle_break(port))
				    goto ignore_char;
			}

			if (uerstat & S3C2410_UERSTAT_FRAME)
				port->icount.frame++;
			if (uerstat & S3C2410_UERSTAT_OVERRUN)
				port->icount.overrun++;

			uerstat &= port->read_status_mask;

			if (uerstat & S3C2410_UERSTAT_BREAK)
				flag = TTY_BREAK;
			else if (uerstat & S3C2410_UERSTAT_PARITY)
				flag = TTY_PARITY;
			else if (uerstat & (S3C2410_UERSTAT_FRAME |
					    S3C2410_UERSTAT_OVERRUN))
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(port, ch))
			goto ignore_char;

		uart_insert_char(port, uerstat, S3C2410_UERSTAT_OVERRUN,
				 ch, flag);

 ignore_char:
		continue;
	}
	tty_flip_buffer_push(tty);

 out:
	return IRQ_HANDLED;
}

static irqreturn_t s3c24xx_serial_tx_chars(int irq, void *id)
{
	struct s3c24xx_uart_port *uport = id;
	struct uart_port *port = &uport->port;
	struct circ_buf *xmit = &port->state->xmit;
	int count = 256;

	if (port->x_char) {
		wr_regb(port, S3C2410_UTXH, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		goto out;
	}

	/* if there isn't anything more to transmit, or the uart is now
	 * stopped, disable the uart and exit
	*/

	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		s3c24xx_serial_stop_tx(port);
		goto out;
	}

	/* try and drain the buffer... */

	while (!uart_circ_empty(xmit) && count-- > 0) {
		if (rd_regl(port, S3C2410_UFSTAT) & uport->info->tx_fifofull)
			break;

		wr_regb(port, S3C2410_UTXH, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		s3c24xx_serial_stop_tx(port);

 out:
	return IRQ_HANDLED;
}

static irqreturn_t s3c24xx_serial_handle_irq(int irq, void *id)
{
	struct s3c24xx_uart_port *uport = id;
	struct uart_port *port = &uport->port;
	unsigned int pend = rd_regl(port, S3C64XX_UINTP);
	unsigned long flags;
	irqreturn_t ret = IRQ_HANDLED;

	spin_lock_irqsave(&port->lock, flags);
	if (pend & S3C64XX_UINTM_RXD_MSK) {
		ret = s3c24xx_serial_rx_chars(irq, id);
		wr_regl(port, S3C64XX_UINTP, S3C64XX_UINTM_RXD_MSK);
	}
	if (pend & S3C64XX_UINTM_TXD_MSK) {
		ret = s3c24xx_serial_tx_chars(irq, id);
		wr_regl(port, S3C64XX_UINTP, S3C64XX_UINTM_TXD_MSK);
	}
	spin_unlock_irqrestore(&port->lock, flags);
	return ret;
}

static unsigned int s3c24xx_serial_tx_empty(struct uart_port *port)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned long ufstat = rd_regl(port, S3C2410_UFSTAT);
	unsigned long ufcon = rd_regl(port, S3C2410_UFCON);

	if (ufcon & S3C2410_UFCON_FIFOMODE) {
		if ((ufstat & info->tx_fifomask) != 0 ||
		    (ufstat & info->tx_fifofull))
			return 0;

		return 1;
	}

	return s3c24xx_serial_txempty_nofifo(port);
}

/* no modem control lines */
static unsigned int s3c24xx_serial_get_mctrl(struct uart_port *port)
{
	unsigned int umstat = rd_regb(port, S3C2410_UMSTAT);

	if (umstat & S3C2410_UMSTAT_CTS)
		return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
	else
		return TIOCM_CAR | TIOCM_DSR;
}

static void s3c24xx_serial_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* todo - possibly remove AFC and do manual CTS */
}

static void s3c24xx_serial_break_ctl(struct uart_port *port, int break_state)
{
	unsigned long flags;
	unsigned int ucon;

	spin_lock_irqsave(&port->lock, flags);

	ucon = rd_regl(port, S3C2410_UCON);

	if (break_state)
		ucon |= S3C2410_UCON_SBREAK;
	else
		ucon &= ~S3C2410_UCON_SBREAK;

	wr_regl(port, S3C2410_UCON, ucon);

	spin_unlock_irqrestore(&port->lock, flags);
}

static void s3c24xx_serial_shutdown(struct uart_port *port)
{
	struct s3c24xx_uart_port *uport = to_uport(port);

	if (uport->tx_claimed) {
		if (!s3c24xx_serial_has_interrupt_mask(port))
			free_irq(uport->tx_irq, uport);
		else
			free_irq(port->irq, uport);
		tx_enabled(port) = 0;
		uport->tx_claimed = 0;
	}

	if (uport->rx_claimed) {
		if (!s3c24xx_serial_has_interrupt_mask(port))
			free_irq(uport->rx_irq, uport);
		/* else already freed above as the s3c24xx_serial_startup()
		 * will have set both tx_claimed and rx_claimed */
		uport->rx_claimed = 0;
		rx_enabled(port) = 0;
	}

	/* Clear pending interrupts and mask all interrupts */
	if (s3c24xx_serial_has_interrupt_mask(port)) {
		wr_regl(port, S3C64XX_UINTP, 0xf);
		wr_regl(port, S3C64XX_UINTM, 0xf);
	}

	if (uport->data) {
		struct s3c24xx_uart_platdata *udata = uport->data;
		if (udata->exit)
			udata->exit(udata->hwport);
	}
}

static int s3c24xx_serial_startup(struct uart_port *port)
{
	struct s3c24xx_uart_port *uport = to_uport(port);
	int ret;

	dbg("s3c24xx_serial_startup: port=%p (%p)\n",
		(void*)port->mapbase, (void*)port->membase);

	ret = request_irq(port->irq, s3c24xx_serial_handle_irq, IRQF_SHARED,
			  s3c24xx_serial_portname(port), uport);
	if (ret) {
		printk(KERN_ERR "cannot get irq %d\n", port->irq);
		return ret;
	}

	if (uport->data) {
		struct s3c24xx_uart_platdata *udata = uport->data;
		if (udata->init)
			udata->init(udata->hwport);
	}

	/* For compatibility with s3c24xx Soc's */
	rx_enabled(port) = 1;
	uport->rx_claimed = 1;
	tx_enabled(port) = 0;
	uport->tx_claimed = 1;

	/* Enable Rx Interrupt */
	__clear_bit(S3C64XX_UINTM_RXD, portaddrl(port, S3C64XX_UINTM));
	dbg("s3c24xx_serial_startup ok\n");
	return ret;
}

/* power power management control */

static void s3c24xx_serial_pm(struct uart_port *port, unsigned int level,
			      unsigned int old)
{
	struct s3c24xx_uart_port *uport = to_uport(port);

	uport->pm_level = level;

	switch (level) {
	case 3:
		if (!IS_ERR(uport->baudclk))
			clk_disable(uport->baudclk);

		clk_disable(uport->clk);
		break;

	case 0:
		clk_enable(uport->clk);

		if (!IS_ERR(uport->baudclk))
			clk_enable(uport->baudclk);

		break;
	default:
		printk(KERN_ERR "s3c24xx_serial: unknown pm %d\n", level);
	}
}

/* baud rate calculation
 *
 * The UARTs on the S3C2410/S3C2440 can take their clocks from a number
 * of different sources, including the peripheral clock ("pclk") and an
 * external clock ("uclk"). The S3C2440 also adds the core clock ("fclk")
 * with a programmable extra divisor.
 *
 * The following code goes through the clock sources, and calculates the
 * baud clocks (and the resultant actual baud rates) and then tries to
 * pick the closest one and select that.
 *
*/

#define MAX_CLK_NAME_LENGTH 15

static inline int s3c24xx_serial_getsource(struct uart_port *port)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned int ucon;

	if (info->num_clks == 1)
		return 0;

	ucon = rd_regl(port, S3C2410_UCON);
	ucon &= info->clksel_mask;
	return ucon >> info->clksel_shift;
}

static void s3c24xx_serial_setsource(struct uart_port *port,
			unsigned int clk_sel)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned int ucon;

	if (info->num_clks == 1)
		return;

	ucon = rd_regl(port, S3C2410_UCON);
	if ((ucon & info->clksel_mask) >> info->clksel_shift == clk_sel)
		return;

	ucon &= ~info->clksel_mask;
	ucon |= clk_sel << info->clksel_shift;
	wr_regl(port, S3C2410_UCON, ucon);
}

static unsigned int s3c24xx_serial_getclk(struct s3c24xx_uart_port *uport,
			unsigned int req_baud, struct clk **best_clk,
			unsigned int *clk_num)
{
	struct s3c24xx_uart_info *info = uport->info;
	struct clk *clk;
	unsigned long rate;
	unsigned int cnt, baud, quot, clk_sel, best_quot = 0;
	int calc_deviation, deviation = (1 << 30) - 1;

	dbg("s3c24xx_serial_getclk: %s\n", dev_name(uport->port.dev));
	clk_sel = (uport->data->clk_sel) ? uport->data->clk_sel :
			uport->info->def_clk_sel;
	for (cnt = 0; cnt < info->num_clks; cnt++) {
		if (!(clk_sel & (1 << cnt)))
			continue;

		clk = clk_get(uport->port.dev, NULL);
		if (IS_ERR(clk))
			continue;

		rate = clk_get_rate(clk);
		if (!rate)
			continue;

		if (uport->info->has_divslot) {
			unsigned long div = rate / req_baud;

			/* The UDIVSLOT register on the newer UARTs allows us to
			 * get a divisor adjustment of 1/16th on the baud clock.
			 *
			 * We don't keep the UDIVSLOT value (the 16ths we
			 * calculated by not multiplying the baud by 16) as it
			 * is easy enough to recalculate.
			 */

			quot = div / 16;
			baud = rate / div;
		} else {
			quot = (rate + (8 * req_baud)) / (16 * req_baud);
			baud = rate / (quot * 16);
		}
		quot--;

		calc_deviation = req_baud - baud;
		if (calc_deviation < 0)
			calc_deviation = -calc_deviation;

		if (calc_deviation < deviation) {
			*best_clk = clk;
			best_quot = quot;
			*clk_num = cnt;
			deviation = calc_deviation;
		}
	}

	return best_quot;
}

/* udivslot_table[]
 *
 * This table takes the fractional value of the baud divisor and gives
 * the recommended setting for the UDIVSLOT register.
 */
static u16 udivslot_table[16] = {
	[0] = 0x0000,
	[1] = 0x0080,
	[2] = 0x0808,
	[3] = 0x0888,
	[4] = 0x2222,
	[5] = 0x4924,
	[6] = 0x4A52,
	[7] = 0x54AA,
	[8] = 0x5555,
	[9] = 0xD555,
	[10] = 0xD5D5,
	[11] = 0xDDD5,
	[12] = 0xDDDD,
	[13] = 0xDFDD,
	[14] = 0xDFDF,
	[15] = 0xFFDF,
};

static void s3c24xx_serial_set_termios(struct uart_port *port,
				       struct ktermios *termios,
				       struct ktermios *old)
{
	struct s3c24xx_uart_platdata *data = s3c24xx_port_to_data(port);
	struct s3c24xx_uart_port *uport = to_uport(port);
	struct clk *clk = ERR_PTR(-EINVAL);
	unsigned long flags;
	unsigned int baud, quot, clk_sel = 0;
	unsigned int ulcon;
	unsigned int umcon;
	unsigned int udivslot = 0;

	/*
	 * We don't support modem control lines.
	 */
	termios->c_cflag &= ~(HUPCL | CMSPAR);
	termios->c_cflag |= CLOCAL;

	/*
	 * Ask the core to calculate the divisor for us.
	 */

	baud = uart_get_baud_rate(port, termios, old, 0, 115200*8);
	quot = s3c24xx_serial_getclk(uport, baud, &clk, &clk_sel);
	if (baud == 38400 && (port->flags & UPF_SPD_MASK) == UPF_SPD_CUST)
		quot = port->custom_divisor;
	if (IS_ERR(clk))
		return;

	/* check to see if we need  to change clock source */

	if (uport->baudclk != clk) {
		s3c24xx_serial_setsource(port, clk_sel);

		if (!IS_ERR(uport->baudclk)) {
			clk_disable(uport->baudclk);
			uport->baudclk = ERR_PTR(-EINVAL);
		}

		clk_enable(clk);

		uport->baudclk = clk;
		uport->baudclk_rate = clk ? clk_get_rate(clk) : 0;
	}
	dbg("s3c24xx_serial_set_termios: rate %ld, clk_sel %d\n",
		uport->baudclk_rate, clk_sel);


	if (uport->info->has_divslot) {
		unsigned int div = uport->baudclk_rate / baud;

		if (data->has_fracval) {
			udivslot = (div & 15);
			dbg("fracval = %04x\n", udivslot);
		} else {
			udivslot = udivslot_table[div & 15];
			dbg("udivslot = %04x (div %d)\n", udivslot, div & 15);
		}
	}

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		dbg("config: 5bits/char\n");
		ulcon = S3C2410_LCON_CS5;
		break;
	case CS6:
		dbg("config: 6bits/char\n");
		ulcon = S3C2410_LCON_CS6;
		break;
	case CS7:
		dbg("config: 7bits/char\n");
		ulcon = S3C2410_LCON_CS7;
		break;
	case CS8:
	default:
		dbg("config: 8bits/char\n");
		ulcon = S3C2410_LCON_CS8;
		break;
	}

	/* preserve original lcon IR settings */
	ulcon |= (data->ulcon & S3C2410_LCON_IRM);

	if (termios->c_cflag & CSTOPB)
		ulcon |= S3C2410_LCON_STOPB;

	umcon = (termios->c_cflag & CRTSCTS) ? S3C2410_UMCOM_AFC : 0;

	if (termios->c_cflag & PARENB) {
		if (termios->c_cflag & PARODD)
			ulcon |= S3C2410_LCON_PODD;
		else
			ulcon |= S3C2410_LCON_PEVEN;
	} else {
		ulcon |= S3C2410_LCON_PNONE;
	}

	spin_lock_irqsave(&port->lock, flags);

	dbg("setting ulcon to %08x, brddiv to %d, udivslot %08x\n",
	    ulcon, quot, udivslot);

	wr_regl(port, S3C2410_ULCON, ulcon);
	wr_regl(port, S3C2410_UBRDIV, quot);
	wr_regl(port, S3C2410_UMCON, umcon);

	if (uport->info->has_divslot)
		wr_regl(port, S3C2443_DIVSLOT, udivslot);

	dbg("uart: ulcon = 0x%08x, ucon = 0x%08x, ufcon = 0x%08x\n",
	    rd_regl(port, S3C2410_ULCON),
	    rd_regl(port, S3C2410_UCON),
	    rd_regl(port, S3C2410_UFCON));

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	/*
	 * Which character status flags are we interested in?
	 */
	port->read_status_mask = S3C2410_UERSTAT_OVERRUN;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= S3C2410_UERSTAT_FRAME | S3C2410_UERSTAT_PARITY;

	/*
	 * Which character status flags should we ignore?
	 */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= S3C2410_UERSTAT_OVERRUN;
	if (termios->c_iflag & IGNBRK && termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= S3C2410_UERSTAT_FRAME;

	/*
	 * Ignore all characters if CREAD is not set.
	 */
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= RXSTAT_DUMMY_READ;

	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *s3c24xx_serial_type(struct uart_port *port)
{
	dbg("s3c24xx_serial_type: type=0x%x\n", port->type);
	switch (port->type) {
	case PORT_S3C2410:
		return "S3C2410";
	case PORT_S3C2440:
		return "S3C2440";
	case PORT_S3C2412:
		return "S3C2412";
	case PORT_S3C6400:
	default:
		return "S3C6400/10";
	}
}

#define MAP_SIZE SZ_4K

static void s3c24xx_serial_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, MAP_SIZE);
}

static int s3c24xx_serial_request_port(struct uart_port *port)
{
	const char *name = s3c24xx_serial_portname(port);
	return request_mem_region(port->mapbase, MAP_SIZE, name)
			!= NULL ? 0 : -EBUSY;
}

static void s3c24xx_serial_config_port(struct uart_port *port, int flags)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	dbg("s3c24xx_serial_config_port: flags=0x%x, %s, type=0x%x\n",
		flags, info->name, info->type);

	if (flags & UART_CONFIG_TYPE) {
		port->type = info->type;
	    s3c24xx_serial_request_port(port);
	}
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int
s3c24xx_serial_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);

	if (ser->type != PORT_UNKNOWN && ser->type != info->type)
		return -EINVAL;

	return 0;
}

static void s3c24xx_serial_wake_peer(struct uart_port *port)
{
	struct s3c24xx_uart_platdata *data = s3c24xx_port_to_data(port);

	if (data->wake_peer)
		data->wake_peer(port);
}

#ifdef CONFIG_SERIAL_NXP_S3C_CONSOLE
static struct console s3c24xx_serial_console;
#define S3C24XX_SERIAL_CONSOLE 	&s3c24xx_serial_console
#else
#define S3C24XX_SERIAL_CONSOLE	NULL
#endif

static struct uart_ops s3c24xx_serial_ops = {
	.pm				= s3c24xx_serial_pm,
	.tx_empty		= s3c24xx_serial_tx_empty,
	.get_mctrl		= s3c24xx_serial_get_mctrl,
	.set_mctrl		= s3c24xx_serial_set_mctrl,
	.stop_tx		= s3c24xx_serial_stop_tx,
	.start_tx		= s3c24xx_serial_start_tx,
	.stop_rx		= s3c24xx_serial_stop_rx,
	.enable_ms		= s3c24xx_serial_enable_ms,
	.break_ctl		= s3c24xx_serial_break_ctl,
	.startup		= s3c24xx_serial_startup,
	.shutdown		= s3c24xx_serial_shutdown,
	.set_termios	= s3c24xx_serial_set_termios,
	.type			= s3c24xx_serial_type,
	.release_port	= s3c24xx_serial_release_port,
	.request_port	= s3c24xx_serial_request_port,
	.config_port	= s3c24xx_serial_config_port,
	.verify_port	= s3c24xx_serial_verify_port,
	.wake_peer		= s3c24xx_serial_wake_peer,
};

static struct uart_driver s3c24xx_uart_drv = {
	.owner			= THIS_MODULE,
	.driver_name	= S3C24XX_SERIAL_NAME,
	.dev_name		= S3C24XX_SERIAL_NAME,
	.nr				= UART_NR,
	.cons			= S3C24XX_SERIAL_CONSOLE,
	.major			= S3C24XX_SERIAL_MAJOR,
	.minor			= S3C24XX_SERIAL_MINOR,
};

static struct s3c24xx_uart_port *s3c24xx_serial_ports[UART_NR];

/* s3c24xx_serial_resetport
 *
 * reset the fifos and other the settings.
*/
static void s3c24xx_serial_resetport(struct uart_port *port,
				   struct s3c24xx_uart_platdata *data)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned long ucon = rd_regl(port, S3C2410_UCON);
	unsigned int ucon_mask;

	ucon_mask = info->clksel_mask;
	if (info->type == PORT_S3C2440)
		ucon_mask |= S3C2440_UCON0_DIVMASK;

	ucon &= ucon_mask;

	wr_regl(port, S3C2410_UCON,  ucon | data->ucon);
	wr_regl(port, S3C2410_ULCON, data->ulcon);

	/* reset both fifos */
	wr_regl(port, S3C2410_UFCON, data->ufcon | S3C2410_UFCON_RESETBOTH);
	wr_regl(port, S3C2410_UFCON, data->ufcon);

	/* some delay is required after fifo reset */
	udelay(1);
}

/* s3c24xx_serial_init_port
 *
 * initialise a single serial port from the platform device given
 */
static int s3c24xx_serial_init_port(struct s3c24xx_uart_port *uport,
				    struct platform_device *platdev)
{
	struct uart_port *port = &uport->port;
	struct s3c24xx_uart_platdata *data = uport->data;
	void __iomem *base;
	struct resource *res;
	int ret;

	dbg("s3c24xx_serial_init_port: port=%p, platdev=%p\n", port, platdev);

	if (platdev == NULL)
		return -ENODEV;

	if (port->mapbase != 0)
		return 0;

	/* setup info for port */
	port->dev = &platdev->dev;

	/* Startup sequence is different for s3c64xx and higher SoC's */
	if (s3c24xx_serial_has_interrupt_mask(port))
		s3c24xx_serial_ops.startup = s3c24xx_serial_startup;

	port->uartclk = 1;

	if (data->uart_flags & UPF_CONS_FLOW) {
		dbg("s3c24xx_serial_init_port: enabling flow control\n");
		port->flags |= UPF_CONS_FLOW;
	}

	/* sort our the physical and virtual addresses for each UART */

	res = platform_get_resource(platdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		printk(KERN_ERR "failed to find memory resource for uart\n");
		return -EINVAL;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		printk(KERN_ERR "failed to ioremap memory resource for uart\n");
		return -ENOMEM;
	}
	dbg("resource %p (%lx..%lx)\n",
		(void*)res, (long)res->start, (long)res->end);

	port->mapbase = res->start;
	port->membase = base;
	ret = platform_get_irq(platdev, 0);
	if (ret < 0)
		port->irq = 0;
	else {
		port->irq = ret;
		uport->rx_irq = ret;
		uport->tx_irq = ret + 1;
	}

	ret = platform_get_irq(platdev, 1);
	if (ret > 0)
		uport->tx_irq = ret;

	uport->clk = clk_get(&platdev->dev, NULL);

	/* Keep all interrupts masked and cleared */
	if (s3c24xx_serial_has_interrupt_mask(port)) {
		wr_regl(port, S3C64XX_UINTM, 0xf);
		wr_regl(port, S3C64XX_UINTP, 0xf);
		wr_regl(port, S3C64XX_UINTSP, 0xf);
	}

	dbg("port: map=%p, mem=%p, irq=%d (%d,%d), clock=%d\n",
	    (void*)port->mapbase, (void*)port->membase, port->irq,
	    uport->rx_irq, uport->tx_irq, port->uartclk);

	/* reset the fifos (and setup the uart) */
	s3c24xx_serial_resetport(port, data);

	return 0;
}


/* Device driver serial port probe */
static const struct of_device_id s3c24xx_uart_dt_match[];

static inline struct s3c24xx_serial_drv_data *s3c24xx_get_driver_data(
			struct platform_device *pdev)
{
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(s3c24xx_uart_dt_match, pdev->dev.of_node);
		return (struct s3c24xx_serial_drv_data *)match->data;
	}
#endif
	return EXYNOS4210_SERIAL_DRV_DATA;
}

#if defined (CONFIG_PM) && defined (CONFIG_SERIAL_NXP_RESUME_WORK)
static void s3c24xx_resume_work(struct work_struct *work);
#endif

static int s3c24xx_serial_probe(struct platform_device *pdev)
{
	struct s3c24xx_uart_port *uport;
	int ret;

	dbg("s3c24xx_serial_probe(%p) %d\n", pdev, pdev->id);

	if (pdev->id >= UART_NR)
		return -EINVAL;

	uport = kzalloc(sizeof(struct s3c24xx_uart_port), GFP_KERNEL);
	if (uport == NULL) {
		dev_err(&pdev->dev, "could not alloc uart driver data\n");
		return -ENOMEM;
	}

	uport->drv_data = s3c24xx_get_driver_data(pdev);
	if (!uport->drv_data) {
		dev_err(&pdev->dev, "could not find uart driver data\n");
		return -ENODEV;
	}

	uport->baudclk = ERR_PTR(-EINVAL);
	uport->info = uport->drv_data->info;
	uport->data = (pdev->dev.platform_data) ?
			(struct s3c24xx_uart_platdata *)pdev->dev.platform_data :
			uport->drv_data->def_data;
	uport->port.line = pdev->id;
	uport->port.iotype = UPIO_MEM;
	uport->port.uartclk	= 0;
	uport->port.fifosize = 16;
	uport->port.ops = &s3c24xx_serial_ops;
	uport->port.flags = UPF_BOOT_AUTOCONF;
	uport->port.fifosize = (uport->info->fifosize) ? uport->info->fifosize :
						uport->drv_data->fifosize[pdev->id];

#if defined (CONFIG_PM) && defined (CONFIG_SERIAL_NXP_RESUME_WORK)
	INIT_DELAYED_WORK(&uport->resume_work, s3c24xx_resume_work);
 	wake_lock_init(&uport->resume_lock, WAKE_LOCK_SUSPEND,
	             kasprintf(GFP_KERNEL, "UART%u", pdev->id));
	device_enable_async_suspend(&pdev->dev);
#endif
	dbg("%s: initialising port %p...\n", __func__, uport);

	ret = s3c24xx_serial_init_port(uport, pdev);
	if (ret < 0)
		goto probe_err;

	s3c24xx_serial_ports[pdev->id] = uport;

	uart_add_one_port(&s3c24xx_uart_drv, &uport->port);
	platform_set_drvdata(pdev, &uport->port);
	dbg("%s: adding port.%d\n", __func__, pdev->id);

	return 0;

 probe_err:
	return ret;
}

static int __devexit s3c24xx_serial_remove(struct platform_device *dev)
{
	struct uart_port *port = s3c24xx_dev_to_port(&dev->dev);
	struct s3c24xx_uart_port *uport = container_of(port, struct s3c24xx_uart_port, port);

	if (port)
		uart_remove_one_port(&s3c24xx_uart_drv, port);

	s3c24xx_serial_ports[dev->id] = NULL;
	kfree(uport);
	return 0;
}

/* UART power management code */
#ifdef CONFIG_PM_SLEEP
unsigned int s3c24xx_serial_mask_save[UART_NR];

#if defined (CONFIG_PM) && defined (CONFIG_SERIAL_NXP_RESUME_WORK)
#define uart_console(port)	((port)->cons && (port)->cons->index == (port)->line)
#define	UART_RESUME_WORK_DELAY	(400)		/* wait for end resume_console */

static void s3c24xx_resume_work(struct work_struct *work)
{
	struct s3c24xx_uart_port *uport = container_of(work,
					struct s3c24xx_uart_port, resume_work.work);
	struct uart_port *port = &uport->port;

	if (uart_console(port)) {
		clk_enable(uport->clk);
		wr_regl(port, S3C64XX_UINTM, s3c24xx_serial_mask_save[port->line]);
		s3c24xx_serial_resetport(port, s3c24xx_port_to_data(port));
		clk_disable(uport->clk);

		uart_resume_port(&s3c24xx_uart_drv, port);
		wake_unlock(&uport->resume_lock);
	}
}
#endif

static int s3c24xx_serial_suspend(struct device *dev)
{
	struct uart_port *port = s3c24xx_dev_to_port(dev);

	s3c24xx_serial_mask_save[port->line] = rd_regl(port, S3C64XX_UINTM);
	if (port)
		uart_suspend_port(&s3c24xx_uart_drv, port);

	return 0;
}

static int s3c24xx_serial_resume(struct device *dev)
{
	struct uart_port *port = s3c24xx_dev_to_port(dev);
	struct s3c24xx_uart_port *uport = to_uport(port);
	struct s3c24xx_uart_platdata *udata = uport->data;

	if (udata->init)
		udata->init(udata->hwport);

	if (port) {
#if defined (CONFIG_PM) && defined (CONFIG_SERIAL_NXP_RESUME_WORK)
		/*
		 * disable console duration delay time
	 	 * to save wakeup time
	 	 */
		wake_lock(&uport->resume_lock);
		schedule_delayed_work(&uport->resume_work, msecs_to_jiffies(UART_RESUME_WORK_DELAY));
		return 0;
#endif
		clk_enable(uport->clk);
		wr_regl(port, S3C64XX_UINTM, s3c24xx_serial_mask_save[port->line]);
		s3c24xx_serial_resetport(port, s3c24xx_port_to_data(port));
		clk_disable(uport->clk);

		uart_resume_port(&s3c24xx_uart_drv, port);
	}

	return 0;
}

static const struct dev_pm_ops s3c24xx_serial_pm_ops = {
	.suspend = s3c24xx_serial_suspend,
	.resume = s3c24xx_serial_resume,
};
#define SERIAL_SAMSUNG_PM_OPS	(&s3c24xx_serial_pm_ops)

#else /* !CONFIG_PM_SLEEP */

#define SERIAL_SAMSUNG_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

/* Console code */

#ifdef CONFIG_SERIAL_NXP_S3C_CONSOLE

static struct uart_port *cons_uart;

static int
s3c24xx_serial_console_txrdy(struct uart_port *port, unsigned int ufcon)
{
	struct s3c24xx_uart_info *info = s3c24xx_port_to_info(port);
	unsigned long ufstat, utrstat;

	if (ufcon & S3C2410_UFCON_FIFOMODE) {
		/* fifo mode - check amount of data in fifo registers... */

		ufstat = rd_regl(port, S3C2410_UFSTAT);
		return (ufstat & info->tx_fifofull) ? 0 : 1;
	}

	/* in non-fifo mode, we go and use the tx buffer empty */

	utrstat = rd_regl(port, S3C2410_UTRSTAT);
	return (utrstat & S3C2410_UTRSTAT_TXE) ? 1 : 0;
}

static void
s3c24xx_serial_console_putchar(struct uart_port *port, int ch)
{
	unsigned int ufcon = rd_regl(cons_uart, S3C2410_UFCON);
	while (!s3c24xx_serial_console_txrdy(port, ufcon))
		barrier();
	wr_regb(cons_uart, S3C2410_UTXH, ch);
}

static void
s3c24xx_serial_console_write(struct console *co, const char *s,
			     unsigned int count)
{
	uart_console_write(cons_uart, s, count, s3c24xx_serial_console_putchar);
}

static void __init
s3c24xx_serial_get_options(struct uart_port *port, int *baud,
			   int *parity, int *bits)
{
	struct clk *clk;
	unsigned int ulcon;
	unsigned int ucon;
	unsigned int ubrdiv;
	unsigned long rate;
	unsigned int clk_sel;

	ulcon  = rd_regl(port, S3C2410_ULCON);
	ucon   = rd_regl(port, S3C2410_UCON);
	ubrdiv = rd_regl(port, S3C2410_UBRDIV);

	dbg("s3c24xx_serial_get_options: port=%p\n"
	    "registers: ulcon=%08x, ucon=%08x, ubdriv=%08x\n",
	    port, ulcon, ucon, ubrdiv);

	if ((ucon & 0xf) != 0) {
		/* consider the serial port configured if the tx/rx mode set */

		switch (ulcon & S3C2410_LCON_CSMASK) {
		case S3C2410_LCON_CS5:
			*bits = 5;
			break;
		case S3C2410_LCON_CS6:
			*bits = 6;
			break;
		case S3C2410_LCON_CS7:
			*bits = 7;
			break;
		default:
		case S3C2410_LCON_CS8:
			*bits = 8;
			break;
		}

		switch (ulcon & S3C2410_LCON_PMASK) {
		case S3C2410_LCON_PEVEN:
			*parity = 'e';
			break;

		case S3C2410_LCON_PODD:
			*parity = 'o';
			break;

		case S3C2410_LCON_PNONE:
		default:
			*parity = 'n';
		}

		/* now calculate the baud rate */

		clk_sel = s3c24xx_serial_getsource(port);
		clk = clk_get(port->dev, NULL);
		if (!IS_ERR(clk))
			rate = clk_get_rate(clk);
		else
			rate = 1;

		*baud = rate / (16 * (ubrdiv + 1));
		dbg("calculated baud %d (%s:%lu)\n", *baud, dev_name(port->dev), rate);
	}

}

static int __init
s3c24xx_serial_console_setup(struct console *co, char *options)
{
	struct s3c24xx_uart_port *uport;
	struct uart_port *port;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	dbg("s3c24xx_serial_console_setup: co=%p (%d), %s\n",
	    co, co->index, options);

	/* is this a valid port */

	if (co->index == -1 || co->index >= UART_NR)
		co->index = 0;

	uport = s3c24xx_serial_ports[co->index];
	port  = &uport->port;

	/* is the port configured? */

	if (port->mapbase == 0x0)
		return -ENODEV;

	cons_uart = port;

	if (uport->data) {
		struct s3c24xx_uart_platdata *udata = uport->data;
		if (udata->init)
			udata->init(udata->hwport);
	}
	dbg("s3c24xx_serial_console_setup: port=%p (%d)\n", port, co->index);

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		s3c24xx_serial_get_options(port, &baud, &parity, &bits);

	dbg("s3c24xx_serial_console_setup: baud %d\n", baud);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console s3c24xx_serial_console = {
	.name		= S3C24XX_SERIAL_NAME,
	.device		= uart_console_device,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.write		= s3c24xx_serial_console_write,
	.setup		= s3c24xx_serial_console_setup,
	.data		= &s3c24xx_uart_drv,
};

#endif /* CONFIG_SERIAL_NXP_S3C_CONSOLE */

#ifdef CONFIG_OF
static const struct of_device_id s3c24xx_uart_dt_match[] = {
	{ .compatible = "Nexell,s3c-uart",
		.data = (void *)EXYNOS4210_SERIAL_DRV_DATA },
	{},
};
MODULE_DEVICE_TABLE(of, s3c24xx_uart_dt_match);
#else
#define s3c24xx_uart_dt_match NULL
#endif

static struct platform_driver samsung_serial_driver = {
	.probe		= s3c24xx_serial_probe,
	.remove		= __devexit_p(s3c24xx_serial_remove),
	.driver		= {
		.name	= "nxp-uart",
		.owner	= THIS_MODULE,
		.pm	= SERIAL_SAMSUNG_PM_OPS,
		.of_match_table	= s3c24xx_uart_dt_match,
	},
};

/* module initialisation code */

static int __init s3c24xx_serial_modinit(void)
{
	int ret = uart_register_driver(&s3c24xx_uart_drv);
	if (ret < 0) {
		printk(KERN_ERR "failed to register UART driver\n");
		return -1;
	}

	return platform_driver_register(&samsung_serial_driver);
}

static void __exit s3c24xx_serial_modexit(void)
{
	uart_unregister_driver(&s3c24xx_uart_drv);
}

module_init(s3c24xx_serial_modinit);
module_exit(s3c24xx_serial_modexit);

MODULE_ALIAS("platform:samsung-uart");
MODULE_DESCRIPTION("Samsung SoC Serial port driver");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_LICENSE("GPL v2");
