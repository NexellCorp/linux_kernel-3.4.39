/*
 * f_audio_sink.c -- USB Audio class function driver
  *
 * Copyright (C) 2008 Bryan Wu <cooloney@kernel.org>
 * Copyright (C) 2008 Analog Devices, Inc
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/atomic.h>

#include "u_audio_sink.h"

static int req_buf_size = UAC1_OUT_EP_MAX_PACKET_SIZE;
module_param(req_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(req_buf_size, "ISO OUT endpoint request buffer size");

static int req_count = UAC1_REQ_COUNT;
module_param(req_count, int, S_IRUGO);
MODULE_PARM_DESC(req_count, "ISO OUT endpoint request count");

static int audio_buf_size = UAC1_AUDIO_BUF_SIZE;
module_param(audio_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(audio_buf_size, "Audio buffer size");

static int generic_set_cmd(struct usb_audio_control *con, u8 cmd, int value);
static int generic_get_cmd(struct usb_audio_control *con, u8 cmd);

/*
 * DESCRIPTORS ... most are static, but strings and full
 * configuration descriptors are built on demand.
 */

/*
 * We have two interfaces- AudioControl and AudioStreaming
 * TODO: only supcard playback currently
 */
#define F_AUDIO_AC_INTERFACE	0
#define F_AUDIO_AS_INTERFACE	1
#define F_AUDIO_NUM_INTERFACES	2

/* B.3.1  Standard AC Interface Descriptor */
static struct usb_interface_descriptor ac_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOCONTROL,
};

/*
 * The number of AudioStreaming and MIDIStreaming interfaces
 * in the Audio Interface Collection
 */
DECLARE_UAC_AC_HEADER_DESCRIPTOR(2);

#define UAC_DT_AC_HEADER_LENGTH	UAC_DT_AC_HEADER_SIZE(F_AUDIO_AS_INTERFACE)
/* 1 input terminal, 1 output terminal and 1 feature unit */
#define UAC_DT_TOTAL_LENGTH \
	(UAC_DT_AC_HEADER_LENGTH + UAC_DT_INPUT_TERMINAL_SIZE \
	+ UAC_DT_OUTPUT_TERMINAL_SIZE + UAC_DT_FEATURE_UNIT_SIZE(0))
/* B.3.2  Class-Specific AC Interface Descriptor */
static struct uac1_ac_header_descriptor_2 ac_header_desc = {
	.bLength =		UAC_DT_AC_HEADER_LENGTH,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_HEADER,
	.bcdADC =		__constant_cpu_to_le16(0x0100),
	.wTotalLength =		__constant_cpu_to_le16(UAC_DT_TOTAL_LENGTH),
	.bInCollection =	F_AUDIO_AS_INTERFACE,
	.baInterfaceNr = {
	/* Interface number of the first AudioStream interface */
		[0] =		1,
	}
};

#define INPUT_TERMINAL_ID	1
static struct uac_input_terminal_descriptor input_terminal_desc = {
	.bLength =		UAC_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_INPUT_TERMINAL,
	.bTerminalID =		INPUT_TERMINAL_ID,
	.wTerminalType =	UAC_TERMINAL_STREAMING,
	.bAssocTerminal =	0,
	.wChannelConfig =	0x3,
};

DECLARE_UAC_FEATURE_UNIT_DESCRIPTOR(2);

#define FEATURE_UNIT_ID		2
static struct uac_feature_unit_descriptor_2 feature_unit_desc = {
	.bLength		= UAC_DT_FEATURE_UNIT_SIZE(2),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_FEATURE_UNIT,
	.bUnitID		= FEATURE_UNIT_ID,
	.bSourceID		= INPUT_TERMINAL_ID,
	.bControlSize		= 2,
	.bmaControls[0]		= (UAC_FU_MUTE | UAC_FU_VOLUME),
	.bmaControls[1]		= 0,
	.bmaControls[2]		= 0,
	.iFeature		= 0,
};

static struct usb_audio_control mute_control = {
	.list = LIST_HEAD_INIT(mute_control.list),
	.name = "Mute Control",
	.type = UAC_FU_MUTE,
	/* Todo: add real Mute control code */
	.set = generic_set_cmd,
	.get = generic_get_cmd,
};

static struct usb_audio_control volume_control = {
	.list = LIST_HEAD_INIT(volume_control.list),
	.name = "Volume Control",
	.type = UAC_FU_VOLUME,
	/* Todo: add real Volume control code */
	.set = generic_set_cmd,
	.get = generic_get_cmd,
};

static struct usb_audio_control_selector feature_unit = {
	.list = LIST_HEAD_INIT(feature_unit.list),
	.id = FEATURE_UNIT_ID,
	.name = "Mute & Volume Control",
	.type = UAC_FEATURE_UNIT,
	.desc = (struct usb_descriptor_header *)&feature_unit_desc,
};

#define OUTPUT_TERMINAL_ID	3
static struct uac1_output_terminal_descriptor output_terminal_desc = {
	.bLength		= UAC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_OUTPUT_TERMINAL,
	.bTerminalID		= OUTPUT_TERMINAL_ID,
	.wTerminalType		= UAC_OUTPUT_TERMINAL_SPEAKER,
	.bAssocTerminal		= FEATURE_UNIT_ID,
	.bSourceID		= FEATURE_UNIT_ID,
};

/* B.4.1  Standard AS Interface Descriptor */
static struct usb_interface_descriptor as_interface_alt_0_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor as_interface_alt_1_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	1,
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

/* B.4.2  Class-Specific AS Interface Descriptor */
static struct uac1_as_header_descriptor as_header_desc = {
	.bLength =		UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_AS_GENERAL,
	.bTerminalLink =	INPUT_TERMINAL_ID,
	.bDelay =		1,
	.wFormatTag =		UAC_FORMAT_TYPE_I_PCM,
};

DECLARE_UAC_FORMAT_TYPE_I_DISCRETE_DESC(1);

static struct uac_format_type_i_discrete_descriptor_1 as_type_i_desc = {
	.bLength =		UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_FORMAT_TYPE,
	.bFormatType =		UAC_FORMAT_TYPE_I,
	.bSubframeSize =	2,
	.bBitResolution =	16,
	.bSamFreqType =		1,
};

/* Standard ISO OUT Endpoint Descriptor */
static struct usb_endpoint_descriptor as_out_ep_desc  = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_SYNC_ADAPTIVE
				| USB_ENDPOINT_XFER_ISOC,
	.wMaxPacketSize	=	cpu_to_le16(UAC1_OUT_EP_MAX_PACKET_SIZE),
	.bInterval =		4,
	.bRefresh  =		0,
	.bSynchAddress =	0,
};

/* Class-specific AS ISO OUT Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor as_iso_out_desc = {
	.bLength =		UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType =	USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype =	UAC_EP_GENERAL,
	.bmAttributes =		1,
	.bLockDelayUnits =	1,
	.wLockDelay =		__constant_cpu_to_le16(1),
};

static struct usb_descriptor_header *f_audio_desc[] = {
	(struct usb_descriptor_header *)&ac_interface_desc,
	(struct usb_descriptor_header *)&ac_header_desc,

	(struct usb_descriptor_header *)&input_terminal_desc,
	(struct usb_descriptor_header *)&output_terminal_desc,
	(struct usb_descriptor_header *)&feature_unit_desc,

	(struct usb_descriptor_header *)&as_interface_alt_0_desc,
	(struct usb_descriptor_header *)&as_interface_alt_1_desc,
	(struct usb_descriptor_header *)&as_header_desc,

	(struct usb_descriptor_header *)&as_type_i_desc,

	(struct usb_descriptor_header *)&as_out_ep_desc,
	(struct usb_descriptor_header *)&as_iso_out_desc,
	NULL,
};

enum {
	STR_AC_IF,
	STR_INPUT_TERMINAL,
	STR_INPUT_TERMINAL_CH_NAMES,
	STR_FEAT_DESC_0,
	STR_OUTPUT_TERMINAL,
	STR_AS_IF_ALT0,
	STR_AS_IF_ALT1,
	STR_AS_IF_ALT2,
};

static struct usb_string strings_uac1[] = {
	[STR_AC_IF].s = "AC Interface",
	[STR_INPUT_TERMINAL].s = "Input terminal",
	[STR_INPUT_TERMINAL_CH_NAMES].s = "Channels",
	[STR_FEAT_DESC_0].s = "Volume control & mute",
	[STR_OUTPUT_TERMINAL].s = "Output terminal",
	[STR_AS_IF_ALT0].s = "AS Interface",
	[STR_AS_IF_ALT1].s = "AS Interface",
	[STR_AS_IF_ALT2].s = "AS Interface",
	{ },
};

static struct usb_gadget_strings str_uac1 = {
	.language = 0x0409,	/* en-us */
	.strings = strings_uac1,
};

static struct usb_gadget_strings *uac1_strings[] = {
	&str_uac1,
	NULL,
};

struct audio_sink_config {
	int	card;
	int	device;
};

/*
 * This function is an ALSA sound card following USB Audio Class Spec 1.0.
 */

/*-------------------------------------------------------------------------*/
struct f_audio_buf {
	u8 *buf;
	int actual;
	struct list_head list;
};

static struct f_audio_buf *f_audio_buffer_alloc(int buf_size)
{
	struct f_audio_buf *copy_buf;

	copy_buf = kzalloc(sizeof *copy_buf, GFP_ATOMIC);
	if (!copy_buf)
		return ERR_PTR(-ENOMEM);

	copy_buf->buf = kzalloc(buf_size, GFP_ATOMIC);
	if (!copy_buf->buf) {
		kfree(copy_buf);
		return ERR_PTR(-ENOMEM);
	}

	return copy_buf;
}

static void f_audio_buffer_free(struct f_audio_buf *audio_buf)
{
	kfree(audio_buf->buf);
	kfree(audio_buf);
}
/*-------------------------------------------------------------------------*/
struct uac1_req {
	int index;
	struct usb_request *req;
};

struct f_audio {
	struct gaudio			card;

	u8 alt_num;

	/* endpoints handle full and/or high speeds */
	struct usb_ep			*out_ep;
	bool				ep_enabled;

	int				req_buf_size;
	int				req_count;
	int				audio_buf_size;
	struct uac1_req ureq[UAC1_REQ_COUNT];

	spinlock_t			lock;
	struct f_audio_buf *copy_buf;
	struct work_struct playback_work;
	struct list_head play_queue;

	/* Control Set command */
	struct list_head cs;
	u8 set_cmd;
	struct usb_audio_control *set_con;
	struct mutex mutex;
	unsigned bound;
};

static inline struct f_audio *func_to_audio(struct usb_function *f)
{
	return container_of(f, struct f_audio, card.func);
}

/*-------------------------------------------------------------------------*/

static void f_audio_playback_work(struct work_struct *data)
{
	struct f_audio *audio = container_of(data, struct f_audio,
					playback_work);
	struct f_audio_buf *play_buf;

	mutex_lock(&audio->mutex);
	spin_lock_irq(&audio->lock);
	if (list_empty(&audio->play_queue)) {
		spin_unlock_irq(&audio->lock);
		return;
	}
	play_buf = list_first_entry(&audio->play_queue,
			struct f_audio_buf, list);
	list_del(&play_buf->list);
	spin_unlock_irq(&audio->lock);

	u_audio_playback(&audio->card, play_buf->buf, play_buf->actual);
	f_audio_buffer_free(play_buf);
	mutex_unlock(&audio->mutex);
}

static int f_audio_out_ep_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;
	struct usb_composite_dev *cdev = audio->card.func.config->cdev;
	struct f_audio_buf *copy_buf = audio->copy_buf;
	int err;

	if (!copy_buf)
		return -EINVAL;
	if (!req)
		return -EINVAL;
	if (!req->buf)
		return -EINVAL;

	/* Copy buffer is full, add it to the play_queue */
	if (audio_buf_size - copy_buf->actual < req->actual) {
		list_add_tail(&copy_buf->list, &audio->play_queue);
		schedule_work(&audio->playback_work);
		copy_buf = f_audio_buffer_alloc(audio_buf_size);
		if (IS_ERR(copy_buf))
			return -ENOMEM;
	}

	memcpy(copy_buf->buf + copy_buf->actual, req->buf, req->actual);
	copy_buf->actual += req->actual;
	audio->copy_buf = copy_buf;

	err = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (err)
		ERROR(cdev, "%s queue req: %d\n", ep->name, err);

	return 0;

}

static void f_audio_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;
	struct usb_composite_dev *cdev = audio->card.func.config->cdev;
	int status = req->status;
	u32 data = 0;
	struct usb_ep *out_ep = audio->out_ep;

	if (!req->context)
		ERROR(cdev, "%s NULL error %d\n", __func__, __LINE__);
	if (!req->buf)
		ERROR(cdev, "%s NULL error %d\n", __func__, __LINE__);
	if (!ep)
		ERROR(cdev, "%s NULL error %d\n", __func__, __LINE__);

	/* i/f shutting down */
	if (!audio->ep_enabled || status == -ESHUTDOWN)
		return;

	switch (status) {

	case 0:				/* normal completion? */
		if (ep == out_ep)
			f_audio_out_ep_complete(ep, req);
		else if (audio->set_con) {
			memcpy(&data, req->buf, req->length);
			audio->set_con->set(audio->set_con, audio->set_cmd,
					le16_to_cpu(data));
			audio->set_con = NULL;
		}
		break;

	/* this endpoint is normally active while we're configured */
	case -ECONNABORTED:	/* hardware forced ep reset */
	case -ECONNRESET:	/* request dequeued */
	case -ESHUTDOWN:	/* disconnect from host */
		return;

	default:
		break;
	}
}

static int audio_set_intf_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	u8			id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			con_sel = (w_value >> 8) & 0xFF;
	u8			cmd = (ctrl->bRequest & 0x0F);
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
			ctrl->bRequest, w_value, len, id);

	list_for_each_entry(cs, &audio->cs, list) {
		if (cs->id == id) {
			list_for_each_entry(con, &cs->control, list) {
				if (con->type == con_sel) {
					audio->set_con = con;
					break;
				}
			}
			break;
		}
	}

	audio->set_cmd = cmd;
	req->context = audio;
	req->complete = f_audio_complete;

	return len;
}

static int audio_get_intf_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u8			id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			con_sel = (w_value >> 8) & 0xFF;
	u8			cmd = (ctrl->bRequest & 0x0F);
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
			ctrl->bRequest, w_value, len, id);
	list_for_each_entry(cs, &audio->cs, list) {
		if (cs->id == id) {
			list_for_each_entry(con, &cs->control, list) {
				if (con->type == con_sel && con->get) {
					value = con->get(con, cmd);
					break;
				}
			}
			break;
		}
	}

	req->context = audio;
	req->complete = f_audio_complete;
	len = min_t(size_t, sizeof(value), len);
	memcpy(req->buf, &value, len);

	return len;
}

static int audio_set_endpoint_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	int			value = -EOPNOTSUPP;
	u16			ep = le16_to_cpu(ctrl->wIndex);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);

	DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, endpoint %d\n",
			ctrl->bRequest, w_value, len, ep);

	switch (ctrl->bRequest) {
	case UAC_SET_CUR:
		value = len;
		break;

	case UAC_SET_MIN:
		break;

	case UAC_SET_MAX:
		break;

	case UAC_SET_RES:
		break;

	case UAC_SET_MEM:
		break;

	default:
		break;
	}

	return value;
}

static int audio_get_endpoint_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	int value = -EOPNOTSUPP;
	u8 ep = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16 len = le16_to_cpu(ctrl->wLength);
	u16 w_value = le16_to_cpu(ctrl->wValue);

	DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, endpoint %d\n",
			ctrl->bRequest, w_value, len, ep);

	switch (ctrl->bRequest) {
	case UAC_GET_CUR:
	case UAC_GET_MIN:
	case UAC_GET_MAX:
	case UAC_GET_RES:
		value = len;
		break;
	case UAC_GET_MEM:
		break;
	default:
		break;
	}

	return value;
}

static int
audio_sink_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* composite driver infrastructure handles everything; interface
	 * activation uses set_alt().
	 */
	switch (ctrl->bRequestType) {
	case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
		value = audio_set_intf_req(f, ctrl);
		break;

	case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
		value = audio_get_intf_req(f, ctrl);
		break;

	case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
		value = audio_set_endpoint_req(f, ctrl);
		break;

	case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
		value = audio_get_endpoint_req(f, ctrl);
		break;

	default:
		ERROR(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		DBG(cdev, "audio req%02x.%02x v%04x i%04x l%d >> %d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length, value);
		req->zero = value < w_length;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "audio response on err %d\n", value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static inline void free_ep(struct f_audio *audio, struct usb_ep *ep)
{
	struct usb_composite_dev *cdev = audio->card.func.config->cdev;
	int i = 0;

	if (!audio->ep_enabled)
		return;

	audio->ep_enabled = false;

	req_count = audio->req_count;
	for (i = 0; i < req_count; i++) {
		if (audio->ureq[i].req) {
			usb_ep_dequeue(ep, audio->ureq[i].req);
			usb_ep_free_request(ep, audio->ureq[i].req);
			audio->ureq[i].req = NULL;
		}
	}

	if (usb_ep_disable(ep))
		ERROR(cdev, "%s:%s Error!\n", __func__, ep->name);
}


static int audio_sink_set_alt(struct usb_function *f,
			      unsigned intf, unsigned alt)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_ep *out_ep = audio->out_ep;
	struct usb_request *req;
	int i = 0, err = 0;

	DBG(cdev, "intf %d, alt %d\n", intf, alt);

	audio->req_count = req_count;

	if (intf == 1) {
		if (alt >= 1) {
			err = config_ep_by_speed(cdev->gadget, f, out_ep);
			if (err)
				return err;

			if (usb_ep_enable(out_ep))
				ERROR(cdev, "%s:%s Error!\n",
				      __func__, out_ep->name);
			else
				audio->ep_enabled = true;
			audio->copy_buf = f_audio_buffer_alloc(audio_buf_size);
			if (IS_ERR(audio->copy_buf))
				return -ENOMEM;

			/*
			 * allocate a bunch of read buffers
			 * and queue them all at once.
			 */
			for (i = 0; i < req_count && err == 0; i++) {
				req = usb_ep_alloc_request(out_ep, GFP_ATOMIC);
				if (req) {
					audio->ureq[i].req = req;
					req->buf = kzalloc(req_buf_size,
							GFP_ATOMIC);
					if (req->buf) {
						req->zero = 0;
						req->length = req_buf_size;
						req->context = audio;
						req->complete =
							f_audio_complete;
						err = usb_ep_queue(out_ep,
							audio->ureq[i].req,
							GFP_ATOMIC);
					} else
						err = -ENOMEM;
				} else
					err = -ENOMEM;
			}

		} else {
			if (work_pending(&audio->playback_work))
				cancel_work_sync(&audio->playback_work);
			free_ep(audio, out_ep);
		}
		audio->alt_num = alt;
	}

	return err;
}

static int audio_sink_get_alt(struct usb_function *f, unsigned intf)
{
	struct f_audio		*audio = func_to_audio(f);

	if (intf == 1)
		return audio->alt_num;

	return -EINVAL;
}

static void audio_sink_disable(struct usb_function *f)
{
	struct f_audio		*audio = func_to_audio(f);

	if (work_pending(&audio->playback_work))
		cancel_work_sync(&audio->playback_work);
	free_ep(audio, audio->out_ep);
	audio->alt_num = 0;

	return;
}

/*-------------------------------------------------------------------------*/

static void audio_sink_build_desc(struct f_audio *audio)
{
	struct gaudio *card = &audio->card;
	u8 *sam_freq;
	int rate;

	/* Set channel numbers */
	input_terminal_desc.bNrChannels = u_audio_get_playback_channels(card);
	as_type_i_desc.bNrChannels = u_audio_get_playback_channels(card);

	/* Set sample rates */
	rate = u_audio_get_playback_rate(card);
	sam_freq = as_type_i_desc.tSamFreq[0];
	memcpy(sam_freq, &rate, 3);

	/* Todo: Set Sample bits and other parameters */

	return;
}

/* audio function driver setup/binding */
static int
audio_sink_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_audio		*audio = func_to_audio(f);
	int			status;
	struct usb_ep		*ep = NULL;


	audio->card.gadget = c->cdev->gadget;
	/* set up ASLA audio devices */
	if (!audio->bound) {
		status = gaudio_setup(&audio->card);
		if (status < 0) {
			ERROR(cdev, "%s %d: ERROR status %d\n",
			       __func__, __LINE__, status);
			return status;
		}
		audio->bound = true;
	}

	audio_sink_build_desc(audio);

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	ac_interface_desc.bInterfaceNumber = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	as_interface_alt_0_desc.bInterfaceNumber = status;
	as_interface_alt_1_desc.bInterfaceNumber = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &as_out_ep_desc);
	if (!ep)
		goto fail;
	audio->out_ep = ep;
	audio->out_ep->desc = &as_out_ep_desc;

	status = -ENOMEM;

	/* copy descriptors, and track endpoint copies */
	f->descriptors = usb_copy_descriptors(f_audio_desc);

	/*
	 * support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		c->highspeed = true;
		f->hs_descriptors = usb_copy_descriptors(f_audio_desc);
	}

	return 0;

fail:
	gaudio_cleanup(&audio->card);
	audio->bound = false;
	return status;
}

/*-------------------------------------------------------------------------*/

static int generic_set_cmd(struct usb_audio_control *con, u8 cmd, int value)
{
	con->data[cmd] = value;

	return 0;
}

static int generic_get_cmd(struct usb_audio_control *con, u8 cmd)
{
	return con->data[cmd];
}

/* Todo: add more control selecotor dynamically */
int control_selector_init(struct f_audio *audio)
{
	INIT_LIST_HEAD(&audio->cs);
	list_add(&feature_unit.list, &audio->cs);

	INIT_LIST_HEAD(&feature_unit.control);
	list_add(&mute_control.list, &feature_unit.control);
	list_add(&volume_control.list, &feature_unit.control);

	volume_control.data[UAC__CUR] = 0x0000;
	volume_control.data[UAC__MIN] = 0x8100;
	volume_control.data[UAC__MAX] = 0x0000;
	volume_control.data[UAC__RES] = 0x0100;

	return 0;
}

static void audio_sink_unbind(struct usb_configuration *c,
			      struct usb_function *f)
{
	struct f_audio *audio = func_to_audio(f);
	struct f_audio_buf *copy_buf = audio->copy_buf;

	cancel_work_sync(&audio->playback_work);
	if (copy_buf)
		f_audio_buffer_free(copy_buf);
	usb_free_descriptors(f->descriptors);
	usb_free_descriptors(f->hs_descriptors);
	gaudio_cleanup(&audio->card);
	audio->bound = false;
	kfree(audio);
}

/**
 * audio_bind_config - add USB audio function to a configuration
 * @c: the configuration to supcard the USB audio function
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 */
int audio_sink_bind_config(struct usb_configuration *c,
		struct audio_sink_config *config)
{
	struct f_audio *audio;
	int status;

	/* allocate and initialize one new instance */
	audio = kzalloc(sizeof(*audio), GFP_KERNEL);
	if (!audio)
		return -ENOMEM;

	audio->card.func.name = "audio_sink";
	audio->card.gadget = c->cdev->gadget;

	INIT_LIST_HEAD(&audio->play_queue);
	spin_lock_init(&audio->lock);
	mutex_init(&audio->mutex);

	audio->card.func.bind = audio_sink_bind;
	audio->card.func.unbind = audio_sink_unbind;
	audio->card.func.set_alt = audio_sink_set_alt;
	audio->card.func.get_alt = audio_sink_get_alt;
	audio->card.func.setup = audio_sink_setup;
	audio->card.func.disable = audio_sink_disable;

	control_selector_init(audio);

	INIT_WORK(&audio->playback_work, f_audio_playback_work);

	status = usb_add_function(c, &audio->card.func);
	if (status)
		goto add_fail;

	INFO(c->cdev, "audio_buf_size %d, req_buf_size %d, req_count %d\n",
		audio_buf_size, req_buf_size, req_count);

	return status;

add_fail:
	kfree(audio);
	return status;
}
