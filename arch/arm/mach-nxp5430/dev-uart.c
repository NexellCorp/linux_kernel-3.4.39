/*
 * (C) Copyright 2009
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <mach/serial.h>

#define UART_RESOURCE(device, base, irqnr)  \
struct resource device##_resource[] = {	\
		[0] = { .start	= base, .end = base + 0x40, .flags = IORESOURCE_MEM,	},	\
		[1] = {	.start	= irqnr , .end = irqnr, .flags = IORESOURCE_IRQ, },	\
	};

#define UART_PLDEVICE(device, dev_name, ch, res, pdata)  \
struct platform_device device##_device = {	\
	    .name = dev_name,          				\
	    .id = ch,								\
		.num_resources = ARRAY_SIZE(res),		\
		.resource = res,						\
	    .dev = { .platform_data = pdata,	},	\
	};

#define	UART_CHARNNEL_INIT(ch)	{ \
		char name[16] = { 'n','x','p','-','u','a','r','t','.', (48 + ch) };	\
		struct clk *clk = clk_get(NULL, name);	\
		if (!nxp_soc_peri_reset_stat(RESET_ID_UART## ch)) {		\
			NX_TIEOFF_Set(TIEOFF_UART## ch ##_USERSMC , 0);	\
			NX_TIEOFF_Set(TIEOFF_UART## ch ##_SMCTXENB, 0);	\
			NX_TIEOFF_Set(TIEOFF_UART## ch ##_SMCRXENB, 0);	\
			nxp_soc_peri_reset_set(RESET_ID_UART## ch);			\
		}													\
		clk_set_rate(clk, CFG_UART_CLKGEN_CLOCK_HZ);		\
		clk_enable(clk);								\
	};

#define	uart_device_register(ch) 		{	\
		UART_CHARNNEL_INIT(ch);	\
		platform_device_register(&uart## ch ##_device);	\
	}

static void uart_device_init(int hwport)
{
	switch (hwport) {
	case 0 : UART_CHARNNEL_INIT(0);	break;
	case 1 : UART_CHARNNEL_INIT(1);	break;
	case 2 : UART_CHARNNEL_INIT(2);	break;
	case 3 : UART_CHARNNEL_INIT(3);	break;
	case 4 : UART_CHARNNEL_INIT(4);	break;
	case 5 : UART_CHARNNEL_INIT(5);	break;
	}
	NX_GPIO_SetPadFunction (PAD_GET_GROUP(PAD_GPIO_D), 14, NX_GPIO_PADFUNC_1);	// RX
	NX_GPIO_SetPadFunction (PAD_GET_GROUP(PAD_GPIO_D), 18, NX_GPIO_PADFUNC_1);	// TX
	NX_GPIO_SetOutputEnable(PAD_GET_GROUP(PAD_GPIO_D), 14, CFALSE);
	NX_GPIO_SetOutputEnable(PAD_GET_GROUP(PAD_GPIO_D), 18, CTRUE);
}

static void uart_device_exit(int hwport) { }
static void uart_device_wake_peer(struct uart_port *uport) { }

/*------------------------------------------------------------------------------
 * Serial platform device
 */
#if defined(CONFIG_SERIAL_NXP_S3C_UART0)
void uport0_weak_alias_init(int hwport)	__attribute__((weak, alias("uart_device_init")));
void uport0_weak_alias_exit(int hwport)	__attribute__((weak, alias("uart_device_exit")));
void uport0_weak_alias_wake_peer(struct uart_port *uport)
		__attribute__((weak, alias("uart_device_wake_peer")));

static struct s3c24xx_uart_platdata  uart0_data = {
	.hwport = 0,
	.init = uport0_weak_alias_init,
	.exit = uport0_weak_alias_exit,
	.wake_peer = uport0_weak_alias_wake_peer,
	.ucon = S5PV210_UCON_DEFAULT,
	.ufcon = S5PV210_UFCON_DEFAULT,
	.has_fracval = 1,
	#if defined(CONFIG_SERIAL_NXP_S3C_UART0_DMA)
 	.dma_filter = pl08x_filter_id,
 	.dma_rx_param = (void *) DMA_PERIPHERAL_NAME_UART0_RX,
 	.dma_tx_param = (void *) DMA_PERIPHERAL_NAME_UART0_TX,
 	#endif
};

static UART_RESOURCE(uart0, PHY_BASEADDR_UART0, IRQ_PHY_UART0);
static UART_PLDEVICE(uart0, "nxp-uart", 0, uart0_resource, &uart0_data);
#endif

