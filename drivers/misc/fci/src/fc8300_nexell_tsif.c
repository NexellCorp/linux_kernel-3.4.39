/*
 * Copyright (C) 2016  Nexell Co., Ltd.
 * Author: Jongkeun, Choi <jkchoi@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <mach/platform.h>
#include <mach/nxp_mp2ts.h>

#include "../inc/fc8300_nexell_tsif.h"

int tsif_get_channel_num(void)
{
	return 1;
}

int tsif_init(u8 ch_num)
{
	struct ts_config_descr	config_descr;
	struct ts_param_descr	param_descr;
	struct ts_param_descr   param_desc;
	struct ts_buf_init_info	buf_info;

	void *clear_buf = kmalloc(64, GFP_KERNEL);

	/*      configuration   */
	config_descr.ch_num                 = ch_num;
	config_descr.un.bits.clock_pol      = 1;
	config_descr.un.bits.valid_pol      = 1;
	config_descr.un.bits.sync_pol       = 1;
	config_descr.un.bits.err_pol        = 1;
	config_descr.un.bits.data_width1    = 1; /* Serial Setting */
	config_descr.un.bits.bypass_enb     = 1;
	config_descr.un.bits.xfer_mode      = 0;
	config_descr.un.bits.encry_on       = 0;

	/*	set param - bypass	*/
	if (clear_buf) {
		memset(clear_buf, 0x00, sizeof(clear_buf));

		param_desc.info.un.bits.ch_num	= ch_num;
		param_desc.info.un.bits.type	= NXP_MP2TS_PARAM_TYPE_PID;
		param_desc.buf			= (void *)clear_buf;
		param_descr.buf_size            = sizeof(clear_buf);

		kfree(clear_buf);
	}

	/*	set buffer information	*/
	buf_info.ch_num         = ch_num;
	buf_info.packet_size    = TS_PACKET_SIZE;
	buf_info.packet_num     = TS_PACKET_NUM;
	buf_info.page_size      = (TS_PACKET_SIZE * TS_PACKET_NUM);
	buf_info.page_num       = TS_PAGE_NUM;

	if (ts_initialize(&config_descr, &param_descr, &buf_info) < 0) {
		pr_err("%s: failed ts initialization!!\n", __func__);
		return -1;
	}

	return 0;
}

int tsif_deinit(u8 ch_num)
{
	int res = 0;

	/*	deinitialzation		*/
	if (ts_deinitialize(ch_num) < 0) {
		pr_err("%s: failed deinitialization!!\n", __func__);
		res = -1;
	}

	return res;
}

int tsif_start(u8 ch_num)
{
	int res = 0;
	struct ts_op_mode       ts_op;

	ts_op.ch_num		= ch_num;
	ts_op.tx_mode		= false;

	if (ts_start(&ts_op) < 0) {
		pr_err("%s: failed ts ts_start!!\n", __func__);
		res = -1;
	}

	return res;
}

void tsif_stop(u8 ch_num)
{
	/* stop	*/
	ts_stop(ch_num);
}

int tsif_read(u8 ch_num, void *buf, int count)
{
	struct ts_param_descr   param_desc;
	int alloc_size, read_size, read_count;
	int read_len;

	/*	read buffer	*/
	read_size	= count;
	alloc_size	= (TS_PAGE_SIZE * 1);
	read_count	= (read_size / alloc_size);

	/*	read buffer	*/
	param_desc.info.un.bits.ch_num  = ch_num;
	param_desc.info.un.bits.type    = NXP_MP2TS_PARAM_TYPE_BUF;
	param_desc.buf_size             = alloc_size;
	param_desc.read_count           = read_count;
	param_desc.wait_time            = 0;
	param_desc.buf                  = (void *)buf;

	if (ts_read(&param_desc) < 0) {
		read_len = 0;
		/* pr_err("%s: data is not receiving!!\n", __func__); */
	} else
		read_len = read_size;

	return read_len;
}

int tsif_alloc_buf(u8 ch_num)
{
	struct ts_buf_init_info	buf_info;

	/*	set buffer information	*/
	buf_info.ch_num         = ch_num;
	buf_info.packet_size    = TS_PACKET_SIZE;
	buf_info.packet_num     = TS_PACKET_NUM;
	buf_info.page_size      = (TS_PACKET_SIZE * TS_PACKET_NUM);
	buf_info.page_num       = TS_PAGE_NUM;

	if (ts_init_buf(&buf_info) < 0) {
		pr_err("%s: failed ts init buffer!!\n", __func__);
		return -1;
	}

	return 0;
}
