/*
 *   Apple "IUI" driver
 *
 *   Copyright (c) 2015 Hyunseok Jung <hsjung@nexell.co.kr>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include "hid-ids.h"

/*
#define pr_debug	printk
*/
//#define PACKET_DEBUG

#define IUIHID_DEV_NAME        "iuihid"
#define IUIHID_DEV_MAJOR       206
#define IUIHID_DEV_MINOR       1

#define RECV_PCK_SIZE		  1024+64

static struct class *iuihid_class;
static struct hid_device *iui_dev = NULL;
static struct device *iui_udev;

//function proto types
long iuihid_ioctl(struct file * file,  unsigned int cmd, unsigned long arg);
int iuihid_open(struct inode *inode, struct file *file);
int iuihid_release(struct inode *inode, struct file *file);

struct file_operations iuihid_fops=
{
    .owner      		= THIS_MODULE,
    .unlocked_ioctl     = iuihid_ioctl,
    .open       		= iuihid_open,
    .release       		= iuihid_release,
};

/**
 * struct iuihid_sc - IUI HID specific data.
 * @input: Input device through which we report events.
 * @quirks: Currently unused.
 */
struct iuihid_sc {
	struct input_dev *input;
	unsigned long quirks;
};

static bool send_packet(char *buf, int len)
{
	int ret = 0;
	char rid, lcb = 0;
	char *repbuf = kmalloc(len+3, GFP_KERNEL);
	int replen = 0, pktlen = 0;

	pr_debug("%s %d %x %x %x %x\n", __func__, len, buf[0], buf[1], buf[2], buf[3]);
	
	while (len > 0) {
		rid = 0xff;
//		int pktlen = len < pktmax ? len : pktmax;
//		for (int i = 0; i < 
		pktlen = len;
		//len += 3;
		rid = 0x0f;
		if (rid == 0xff) {
			printk(" rid < 0\n");
			return false;
		}
		if (len > pktlen ) lcb |= 2; // more
		else lcb &= ~2; // complete
		repbuf[/*ID0*/0] = 0;
		repbuf[/*ID1*/1] = rid;
		repbuf[/*LCB*/2] = lcb;
		memcpy(repbuf+3, buf, pktlen);
		memset(repbuf+3+pktlen, 0, 3);
		pr_debug("TX(%d:%d)=%d(%d)\n", rid, lcb, replen, pktlen);
		ret = iui_dev->hid_output_raw_report(iui_dev, repbuf, pktlen+6, HID_OUTPUT_REPORT);

		if (ret < 0) {
			printk("%s SetReport error\n", __func__);
			return false;
		}
		buf += pktlen;
		len -= pktlen;
		lcb |= 1; //continue
	}
	kfree(repbuf);
	return true;
}

long iuihid_ioctl(struct file * file,  unsigned int cmd, unsigned long arg)
{
	if (iui_dev == NULL) {
		pr_debug("iui_dev is not connected\n");
		return -ENODEV;
	}

	struct usb_interface *intf = to_usb_interface(iui_dev->dev.parent);
    struct usb_device *dev = interface_to_usbdev(intf);

	int ret = 0;
	int actual_length;
#ifdef PACKET_DEBUG
	int i = 0;
#endif
	void __user *argp = (void __user *)arg;

    struct iui_send_packet {
		int len;
		char *buf;
	} packet;

	struct iui_recv_packet {
		int len;
		char buf[RECV_PCK_SIZE];
	} recv_packet;

	pr_debug("++++%s(%d)\r\n",__func__,cmd);

	switch(cmd)
	{
		case 0: /*IIF_IOCTL_SEND_IAPPACKET*/
			if (copy_from_user(&packet, argp, sizeof(packet))) {
				printk("%s ERROR!!!!!!!!!!!\n", __func__);
				return -EFAULT;
			}
#ifdef PACKET_DEBUG
			for(i = 0; i < packet.len; i++)
				printk("[%x]", packet.buf[i]);
			printk("\nsize %d", packet.len);
#endif
    		char *test = kmalloc(packet.len, GFP_KERNEL);
			memcpy(test, packet.buf, packet.len);
			send_packet(test, packet.len);
			pr_debug("custom vendor request send complete.\n");
			kfree(test);
			break;
		case 1: /*IIF_IOCTL_GET_MESSAGE*/
			pr_debug("IIF_IOCTL_GET_MESSAGE\n");
			break;
		case 2: /*IOCTL_GET_VERSION*/
			pr_debug("IOCTL_GET_VERSION\n");
			break;
		case 3: /*IIF_IOCTL_GET_MESSAGE*/
			memset(recv_packet.buf, 0, RECV_PCK_SIZE);
			ret = usb_interrupt_msg(dev, usb_rcvintpipe(dev, 3),
					recv_packet.buf, RECV_PCK_SIZE, &recv_packet.len, USB_CTRL_GET_TIMEOUT);
			if (ret < 0) {
				if (ret == -110)
					pr_debug("ioctl receive timeout error %s %d\n", __func__, ret);
				else
					pr_debug("============ERROR============%s %d\n", __func__, ret);
			}
			else {
				pr_debug("===========SUCCESS============ %s %d\n", __func__, ret);
				if (recv_packet.len > 1024) {
					memcpy(&recv_packet.buf[768], &recv_packet.buf[770], recv_packet.len - 768);
				}
				copy_to_user((void __user *)arg, &recv_packet, recv_packet.len+sizeof(recv_packet.len));
#ifdef PACKET_DEBUG
				printk("actual_length %d\n", actual_length);
				for (i = 0; i < actual_length; i++) {
					printk("[%x]", *recv_packet.buf++);					
				}
#endif
			}

			break;
		default:
			break;
	}

    pr_debug("%s--\r\n",__func__);
 
	return ret;
}

int iuihid_open(struct inode *inode, struct file *file)
{
	pr_debug("%s\r\n",__func__);

	return 0;
}

int iuihid_release(struct inode *inode, struct file *file)
{
	pr_debug("%s\r\n",__func__);

	return 0;
}


static int iuihid_probe(struct hid_device *hdev,
	const struct hid_device_id *id)
{
	struct iuihid_sc *isc;
	int ret;
	char *envp[] = {"change@/devices/virtual/iuihid/iuihid", NULL};

	isc = kzalloc(sizeof(*isc), GFP_KERNEL);
	if (isc == NULL) {
		hid_err(hdev, "can't alloc iuihid descriptor\n");
		return -ENOMEM;
	}

	isc->quirks = id->driver_data;

	hid_set_drvdata(hdev, isc);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "iui hid parse failed\n");
		goto err_free;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "iuihid hw start failed\n");
		goto err_free;
	}

	iui_dev = hdev;

	ret = register_chrdev(IUIHID_DEV_MAJOR, IUIHID_DEV_NAME, &iuihid_fops); // register char device
	if (ret < 0) {
		printk("[%s] iuihid device register error(%d)\r\n", __func__, ret);
		goto err_stop_hw;
	}

	iuihid_class = class_create(THIS_MODULE, IUIHID_DEV_NAME);
	iui_udev = device_create(iuihid_class, NULL, MKDEV(IUIHID_DEV_MAJOR, IUIHID_DEV_MINOR), NULL, IUIHID_DEV_NAME);

	kobject_uevent_env(&iui_udev->kobj, KOBJ_CHANGE, envp);

	return 0;

err_stop_hw:
	hid_hw_stop(hdev);
err_free:
	kfree(isc);
	return ret;
}

static void iuihid_remove(struct hid_device *hdev)
{
	struct iuihid_sc *isc = hid_get_drvdata(hdev);
	char *envp[] = {"change@/devices/virtual/iuihid/iuihid", NULL};

	iui_dev = NULL;

	kobject_uevent_env(&iui_udev->kobj, KOBJ_CHANGE, envp);

	device_destroy(iuihid_class, MKDEV(IUIHID_DEV_MAJOR, IUIHID_DEV_MINOR));
	class_destroy(iuihid_class);
	unregister_chrdev(IUIHID_DEV_MAJOR, IUIHID_DEV_NAME);

	hid_hw_stop(hdev);
	kfree(isc);
}

static const struct hid_device_id iuihid_devices[] = {
   { 
		HID_USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IUI_DEVICE), 
		.driver_data = 0
	},
{ }
};
MODULE_DEVICE_TABLE(hid, iuihid_devices);

static struct hid_driver iuihid_driver = {
	.name = "iuihid",
	.id_table = iuihid_devices,
	.probe = iuihid_probe,
	.remove = iuihid_remove,
};

static int __init iuihid_init(void)
{
	int ret;

	ret = hid_register_driver(&iuihid_driver);
	if (ret)
		pr_err("can't register iuihid driver\n");

	return ret;
}

static void __exit iuihid_exit(void)
{
	hid_unregister_driver(&iuihid_driver);
}

module_init(iuihid_init);
module_exit(iuihid_exit);
MODULE_LICENSE("GPL");
