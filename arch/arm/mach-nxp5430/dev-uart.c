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

static struct platform_device_id uart_ids = {
	.name = "nxp-uart",
};

#define UART_RES(_dev, _base, _irq)  \
struct resource _dev##_resource[] = {	\
	[0] = {							\
		.start	= _base, .end = _base + 0x40,	\
		.flags	= IORESOURCE_MEM,	\
	}, [1] = {				\
		.start	= _irq, .end = _irq,	\
		.flags	= IORESOURCE_IRQ,	\
	},	\
};

#define UART_DEV(_dev, _name, _id, _res, _data)  \
struct platform_device _dev##_device = {	\
    .name = _name,          				\
    .id = _id,								\
	.num_resources = ARRAY_SIZE(_res),		\
	.resource = _res,						\
    .id_entry = &uart_ids,					\
    .dev = { .platform_data = _data,	},	\
};

#define	UART_PORT_INIT(ch) do { \
	struct clk *clk;			\
	char name[16];				\
	sprintf(name, "nxp-uart.%d", ch);					\
	clk = clk_get(NULL, name);							\
	if (!nxp_soc_peri_reset_stat(RESET_ID_UART## ch)) {		\
		NX_TIEOFF_Set(TIEOFF_UART## ch ##_USERSMC , 0);	\
		NX_TIEOFF_Set(TIEOFF_UART## ch ##_SMCTXENB, 0);	\
		NX_TIEOFF_Set(TIEOFF_UART## ch ##_SMCRXENB, 0);	\
		nxp_soc_peri_reset_set(RESET_ID_UART## ch);			\
	}													\
	clk_set_rate(clk, CFG_UART_CLKGEN_CLOCK_HZ);		\
	clk_enable(clk);								\
	} while (0);


/*------------------------------------------------------------------------------
 * Serial platform device
 */
#if defined(CONFIG_SERIAL_NXP_S3C_UART0)
static void __nxp_uart0_init(void)
{
	UART_PORT_INIT(0);
	NX_GPIO_SetPadFunction (PAD_GET_GROUP(PAD_GPIO_D), 14, NX_GPIO_PADFUNC_1);	// RX
	NX_GPIO_SetPadFunction (PAD_GET_GROUP(PAD_GPIO_D), 18, NX_GPIO_PADFUNC_1);	// TX
	NX_GPIO_SetOutputEnable(PAD_GET_GROUP(PAD_GPIO_D), 14, CFALSE);
	NX_GPIO_SetOutputEnable(PAD_GET_GROUP(PAD_GPIO_D), 18, CTRUE);
}

static void __nxp_uart0_wake_peer(struct uart_port *uport) { }
static void __nxp_uart0_exit(void) { }

void nxp_uart0_init(void)				__attribute__((weak, alias("__nxp_uart0_init")));
void nxp_uart0_wake_peer(struct uart_port *uport)	__attribute__((weak, alias("__nxp_uart0_wake_peer")));
void nxp_uart0_exit(void)				__attribute__((weak, alias("__nxp_uart0_exit")));

static struct s3c24xx_uart_platdata  uart0_data = {
	#if defined(CONFIG_SERIAL_NXP_S3C_UART0_DMA)
 	.dma_filter = pl08x_filter_id,
 	.dma_rx_param = (void *) DMA_PERIPHERAL_NAME_UART0_RX,
 	.dma_tx_param = (void *) DMA_PERIPHERAL_NAME_UART0_TX,
 	#endif
	.init = nxp_uart0_init,
	.exit = nxp_uart0_exit,
	.wake_peer = nxp_uart0_wake_peer,
	.ucon = S5PV210_UCON_DEFAULT,
	.ufcon = S5PV210_UFCON_DEFAULT,
	.has_fracval = 1,
};

static UART_RES(uart0, PHY_BASEADDR_UART0, IRQ_PHY_UART0);
static UART_DEV(uart0, "nxp-uart", 0, uart0_resource, &uart0_data);
#endif

