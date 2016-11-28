/*
 * tdmb_ebi.c
 *
 * RAONTECH MTV EBI driver.
 *
 * Copyright (C) (2013, RAONTECH)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/types.h>

#include "mtv319.h"
#include "mtv319_internal.h"

#include "tdmb.h"
#include "tdmb_gpio.h"

#ifdef RTV_IF_EBI2

#define EBI_DEV_NAME	"tdmbebi"

#if defined(RTV_EBI2_BUS_WITDH_16)
	#define EBI_IO_WRITE	__raw_writew
	#define EBI_IO_READ	__raw_readw
#elif defined(RTV_EBI2_BUS_WITDH_32)
	#define EBI_IO_WRITE	__raw_writel
	#define EBI_IO_READ	__raw_readl
#endif

/*
SPI conntection: MSB first!
value[1]: When write
value[0]: When read
NOTE: MSB first!
*/

static __always_inline void write_bits(const volatile void __iomem *ioaddr,
					U8 value)
{
	EBI_IO_WRITE(value>>(7-1), ioaddr);
	EBI_IO_WRITE(value>>(6-1), ioaddr);
	EBI_IO_WRITE(value>>(5-1), ioaddr);
	EBI_IO_WRITE(value>>(4-1), ioaddr);
	EBI_IO_WRITE(value>>(3-1), ioaddr);
	EBI_IO_WRITE(value>>(2-1), ioaddr);
	EBI_IO_WRITE(value>>(1-1), ioaddr);
	EBI_IO_WRITE(value<<1, ioaddr);
}

static __always_inline U8 read_bits(const volatile void __iomem *ioaddr)
{
	unsigned long bits;
	U8 value = 0;

	bits = EBI_IO_READ(ioaddr);
	value |= ((bits & 0x01) << 7);

	bits = EBI_IO_READ(ioaddr);
	value |= ((bits & 0x01) << 6);

	bits = EBI_IO_READ(ioaddr);
	value |= ((bits & 0x01) << 5);

	bits = EBI_IO_READ(ioaddr);
	value |= ((bits & 0x01) << 4);

	bits = EBI_IO_READ(ioaddr);
	value |= ((bits & 0x01) << 3);

	bits = EBI_IO_READ(ioaddr);
	value |= ((bits & 0x01) << 2);

	bits = EBI_IO_READ(ioaddr);
	value |= ((bits & 0x01) << 1);

	bits = EBI_IO_READ(ioaddr);
	value |= ((bits & 0x01) << 0);

	return value;
}

unsigned char tdmb_ebi2_read(unsigned char page, unsigned char reg)
{	
	U8 data;
	/* To speed up, we use local variable to access io memory. */
	const volatile void __iomem *ioaddr = tdmb_cb_ptr->ioaddr;

	write_bits(ioaddr, 0x90 | page);
	write_bits(ioaddr, reg);
	write_bits(ioaddr, 1); /* Read size */

	data = read_bits(ioaddr);

#if 0
	DMBMSG("0x%02X\n", data);
#endif
	return data;
}

void tdmb_ebi2_read_burst(unsigned char page, unsigned char reg,
			unsigned char *buf, int size)
{
	UINT loop_cnt;
	U8 out_buf[MTV319_SPI_CMD_SIZE];
	const volatile void __iomem *ioaddr = tdmb_cb_ptr->ioaddr;

#define NUM_LOOP_UNROLLING	4
	loop_cnt = size / NUM_LOOP_UNROLLING;
	size -= (loop_cnt * NUM_LOOP_UNROLLING);

	out_buf[0] = 0xA0; /* Memory read */
	out_buf[1] = 0x00;
	out_buf[2] = 188; /* Fix */

	write_bits(ioaddr, out_buf[0]);
	write_bits(ioaddr, out_buf[1]);
	write_bits(ioaddr, out_buf[2]);

	/* Use the loop unrolling */
	do {
		*buf++ = read_bits(ioaddr);
		*buf++ = read_bits(ioaddr);
		*buf++ = read_bits(ioaddr);
		*buf++ = read_bits(ioaddr);
	} while (--loop_cnt);

	while (size--)
		*buf++ = read_bits(ioaddr);
}

void tdmb_ebi2_write(unsigned char page, unsigned char reg, unsigned char val)
{
	const volatile void __iomem *ioaddr = tdmb_cb_ptr->ioaddr;

	write_bits(ioaddr, 0x80 | page);
	write_bits(ioaddr, reg);
	write_bits(ioaddr, 1);
	write_bits(ioaddr, val);
}

void tdmb_spi_recover(unsigned char *buf, unsigned int size)
{
	int i;
	const volatile void __iomem *ioaddr = tdmb_cb_ptr->ioaddr;

	for (i = 0; i < size; i++)
		write_bits(ioaddr, 0xFF);
}

int tdmb_deinit_ebi_bus(void)
{
	if (tdmb_cb_ptr->ioaddr) {
		iounmap(tdmb_cb_ptr->ioaddr);
		tdmb_cb_ptr->ioaddr = NULL;
	}

	if (tdmb_cb_ptr->io_mem) {
		release_mem_region(S5PV210_PA_SROM_BANK0, PAGE_SIZE);
		tdmb_cb_ptr->io_mem = NULL;
	}

	return 0;
}

int tdmb_init_ebi_bus(void)
{
	tdmb_cb_ptr->io_mem = request_mem_region(S5PV210_PA_SROM_BANK0,
						PAGE_SIZE, EBI_DEV_NAME);
	if (tdmb_cb_ptr->io_mem  == NULL) {
		DMBERR("%s failed to get memory region\n", EBI_DEV_NAME);
		return -EBUSY;
	}

	tdmb_cb_ptr->ioaddr = ioremap_nocache(S5PV210_PA_SROM_BANK0, PAGE_SIZE);
	if (tdmb_cb_ptr->ioaddr == 0)	{
		DMBERR("%s failed to ioremap_nocache() region\n", EBI_DEV_NAME);
		return -ENOMEM;
	}

	DMBMSG("EBI2 bus OK\n");

	return 0;
}
#endif /* #ifdef RTV_IF_EBI2 */

