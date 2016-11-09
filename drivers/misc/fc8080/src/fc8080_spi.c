/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080_spi.c

	Description : spi interface source file

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	History :
	----------------------------------------------------------------------
*******************************************************************************/
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/module.h>

#include "fci_types.h"
#include "fc8080_regs.h"
#include "fci_oal.h"
#include "fc8080_spi.h"

#define SPI_BMODE       0x00
#define SPI_WMODE       0x04
#define SPI_LMODE       0x08
#define SPI_RD_THRESH   0x30
#define SPI_RD_REG      0x20
#define SPI_READ        0x40
#define SPI_WRITE       0x00
#define SPI_AINC        0x80

#define CHIPID          0
//#define DRIVER_NAME "fc8080_spi"
#define DRIVER_NAME "mtvspi"

//extern struct spi_device *spi_master_tref;
struct spi_device g_spi;
static struct spi_device *fc8080_spi;
extern u8 fc8080_tx_data[32];



#define SPI_RX_BUF_SIZE (4096)
#define SPI_TX_BUF_SIZE (4096)
u8* g_pRxBuff;// = kmalloc(SPI_RX_BUF_SIZE, GFP_KERNEL);
u8* g_pTxBuff;// = kmalloc(SPI_TX_BUF_SIZE, GFP_KERNEL);




static DEFINE_MUTEX(lock);

int fc8080_spi_write_then_read(u8 *txbuf, u16 tx_length, u8 *rxbuf, u16 rx_length)
{
	s32 res;

	struct spi_message message;
	struct spi_transfer	transfer;

//
//        if (fc8080_spi != spi_master_tref) {
//          print_log(NULL, "memory corruption happens (w/r) \n");
//          fc8080_spi = spi_master_tref;
//        }
//
	spi_message_init(&message);
	memset(&transfer, 0, sizeof(struct spi_transfer));
    //memset(g_pTxBuff, 0, 4096);



	//spi_message_add_tail(&transfer, &message);

    //g_pTxBuff[0] = txbuf[0];
    //g_pTxBuff[1] = txbuf[1];

	transfer.tx_buf = txbuf;  //g_pTxBuff;//txbuf;
    transfer.rx_buf = txbuf;  //g_pRxBuff; //txbuf;
	transfer.len = tx_length + rx_length;

	spi_message_add_tail(&transfer, &message);

    res = spi_sync(&g_spi, &message);

    //print_log(0, "read end\n");


	memcpy(rxbuf, transfer.rx_buf + tx_length, rx_length);

	return res;
}


int fc8080_spi_write_then_burstread(u8 *txbuf, u16 tx_length, u8 *rxbuf, u16 rx_length)
{
	s32 res;

	struct spi_message	message;
	struct spi_transfer	transfer;
//
//        if (fc8080_spi != spi_master_tref) {
//          print_log(NULL, "memory corruption happens (w/r burst) \n");
//          fc8080_spi = spi_master_tref;
//        }

    memset(&transfer, 0, sizeof(struct spi_transfer));
    //memset(&transfer, 0, SPI_TX_BUF_SIZE);



	spi_message_init(&message);
	memset(&transfer, 0, sizeof transfer);

//	spi_message_add_tail(&transfer, &message);

	transfer.tx_buf = txbuf;
	transfer.rx_buf = rxbuf;
	transfer.len = tx_length + rx_length;

	spi_message_add_tail(&transfer, &message);
    
    res = spi_sync(&g_spi, &message);


	return res;
}

static s32 spi_bulkread(HANDLE handle, u16 addr, u8 command, u8 *data,
			u16 length)
{
	s32 res = BBM_OK;

	fc8080_tx_data[0] = (u8) (addr & 0xff);
	fc8080_tx_data[1] = (u8) ((addr >> 8) & 0xff);
	fc8080_tx_data[2] = (u8) ((command & 0xfc) | CHIPID);
	fc8080_tx_data[3] = (u8) (length & 0xff);

    

	res = fc8080_spi_write_then_read(&fc8080_tx_data[0], 4, &data[0], length);

	if (res) {
		print_log(0, "fc8080_spi_bulkread fail : %d\n", res);
		return BBM_NOK;
	}

	return BBM_OK;
}





static s32 spi_bulkwrite(HANDLE handle, u16 addr, u8 command, u8 *data,
			u16 length)
{
	s32 res = BBM_OK;
	s32 i = 0;

	fc8080_tx_data[0] = (u8) (addr & 0xff);
	fc8080_tx_data[1] = (u8) ((addr >> 8) & 0xff);
	fc8080_tx_data[2] = (u8) ((command & 0xfc) | CHIPID);
	fc8080_tx_data[3] = (u8) (length & 0xff);

 //   print_log(0, "spi_bulkwrite: %d\n", length);


	for (i = 0; i < length; i++)
		fc8080_tx_data[4+i] = data[i];

	res = fc8080_spi_write_then_read(&fc8080_tx_data[0], length+4, NULL, 0);

	if (res) {
		print_log(0, "fc8080_spi_bulkwrite fail : %d\n", res);
		return BBM_NOK;
	}

	return BBM_OK;
}

static s32 spi_dataread(HANDLE handle, u8 addr, u8 command, u8 *data,
			u16 length)
{
	s32 res = BBM_OK;

	fc8080_tx_data[0] = (u8) (addr & 0xff);
	fc8080_tx_data[1] = (u8) ((addr >> 8) & 0xff);
	fc8080_tx_data[2] = (u8) ((command & 0xfc) | CHIPID);
	fc8080_tx_data[3] = (u8) (length & 0xff);

	res = fc8080_spi_write_then_burstread(&fc8080_tx_data[0], 4, &data[0], length);

	//res = fc8080_spi_write_then_read(&fc8080_tx_data[0], 4, &data[0], length);
	
    
    if (res) {
		print_log(0, "fc8080_spi_dataread fail : %d\n", res);
		return BBM_NOK;
	}

	return BBM_OK;
}

static int __devinit fc8080_spi_probe(struct spi_device *spi)
{
//	
//    s32 ret;
//
//	print_log(0, "fc8080_spi_probe\n");
//
//	spi->max_speed_hz =  12500000;
//
//	ret = spi_setup(spi);
//	if (ret < 0)
//		return ret;
//
//	spi_master_tref = fc8080_spi = spi;
//
//	return ret;
    
    if(spi == NULL) {
        print_log (1, "[%s]   SPI Device is NULL !!! SPI Open Error...", __func__);
        return -1;
    }

    print_log(1, "######## [%s] %s, bus[%d], cs[%d], mod[%d], %dkhz, %dbit \n",
            __func__, spi->modalias, spi->master->bus_num, spi->chip_select, spi->mode, spi->max_speed_hz/1000, spi->bits_per_word);

    ///////////////////////////////////////
    // SPI setup to I&C
    ///////////////////////////////////////
    memcpy(&g_spi, spi, sizeof(struct spi_device));



g_pRxBuff =  kmalloc(SPI_RX_BUF_SIZE, GFP_KERNEL);
g_pTxBuff = kmalloc(SPI_TX_BUF_SIZE, GFP_KERNEL);



    g_spi.mode = 0;
    g_spi.bits_per_word = 8;
    g_spi.max_speed_hz = 14*1000*1000;

    return 0;




}

static int fc8080_spi_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver fc8080_spi_driver = {
	.driver = {
		.name		= DRIVER_NAME,
        .bus        = &spi_bus_type,
		.owner		= THIS_MODULE,
	},
	.probe		= fc8080_spi_probe,
	.remove		= __devexit_p(fc8080_spi_remove),
    .suspend = NULL,
    .resume = NULL,
};

int fc8080_spi_init(HANDLE hDevice, u16 param1, u16 param2)
{
  int res;

  res = spi_register_driver(&fc8080_spi_driver);

  if (res) {
    print_log(0, "fc8080_spi register fail : %d\n", res);
    return BBM_NOK;
  }

  return BBM_OK;
}

/*
s32 fc8080_spi_init(HANDLE handle, u16 param1, u16 param2)
{
	return BBM_OK;
}
*/
s32 fc8080_spi_byteread(HANDLE handle, u16 addr, u8 *data)
{
	s32 res;
	u8 command = SPI_READ;

	mutex_lock(&lock);
	res = spi_bulkread(handle, addr, command, data, 1);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_spi_wordread(HANDLE handle, u16 addr, u16 *data)
{
	s32 res;
	u8 command = SPI_READ | SPI_AINC;
    //print_log(1, "addr:0x%x\n", addr);

	mutex_lock(&lock);
    //print_log(1, "s\n");
	res = spi_bulkread(handle, addr, command, (u8 *) data, 2);
	mutex_unlock(&lock);
    //print_log(1, "data(0x%x)\n", *data);
	return res;
}

s32 fc8080_spi_longread(HANDLE handle, u16 addr, u32 *data)
{
	s32 res;
	u8 command = SPI_READ | SPI_AINC;

	mutex_lock(&lock);
	res = spi_bulkread(handle, addr, command, (u8 *) data, 4);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_spi_bulkread(HANDLE handle, u16 addr, u8 *data, u16 length)
{
	s32 i;
	u16 x, y;
	s32 res = BBM_OK;
	u8 command = SPI_READ | SPI_AINC;

	x = length / 255;
	y = length % 255;

	mutex_lock(&lock);
	for (i = 0; i < x; i++, addr += 255)
		res |= spi_bulkread(handle, addr, command, &data[i * 255], 255);
	if (y)
		res |= spi_bulkread(handle, addr, command, &data[x * 255], y);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_spi_bytewrite(HANDLE handle, u16 addr, u8 data)
{
	s32 res;
	u8 command = SPI_WRITE;

	mutex_lock(&lock);
	res = spi_bulkwrite(handle, addr, command, (u8 *) &data, 1);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_spi_wordwrite(HANDLE handle, u16 addr, u16 data)
{
	s32 res;
	u8 command = SPI_WRITE;

	if ((addr & 0xff00) != 0x0f00)
		command |= SPI_AINC;

	mutex_lock(&lock);
	res = spi_bulkwrite(handle, addr, command, (u8 *) &data, 2);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_spi_longwrite(HANDLE handle, u16 addr, u32 data)
{
	s32 res;
	u8 command = SPI_WRITE | SPI_AINC;

	mutex_lock(&lock);
	res = spi_bulkwrite(handle, addr, command, (u8 *) &data, 4);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_spi_bulkwrite(HANDLE handle, u16 addr, u8 *data, u16 length)
{
	s32 i;
	u16 x, y;
	s32 res = BBM_OK;
	u8 command = SPI_WRITE | SPI_AINC;

	x = length / 255;
	y = length % 255;

	mutex_lock(&lock);
	for (i = 0; i < x; i++, addr += 255)
		res |= spi_bulkwrite(handle, addr, command, &data[i * 255],
					255);
	if (y)
		res |= spi_bulkwrite(handle, addr, command, &data[x * 255], y);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_spi_dataread(HANDLE handle, u16 addr, u8 *data, u16 length)
{
	s32 res;
	u8 command = SPI_READ | SPI_RD_THRESH;

	mutex_lock(&lock);
	res = spi_dataread(handle, addr, command, data, length);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_spi_deinit(HANDLE handle)
{
	return BBM_OK;
}
