#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

#include <mach/platform.h>
#include <mach/nxp-v4l2-platformdata.h>
#include <mach/soc.h>

#include "nxp-v4l2.h"
#include "nxp-tvout.h"

/**
 * util functions
 */
static inline struct nxp_tvout *ctrl_to_me(struct v4l2_ctrl *ctrl)
{
    return container_of(ctrl->handler, struct nxp_tvout, handler);
}

/**
 * controls
 */
#define V4L2_CID_YBW           (V4L2_CTRL_CLASS_USER | 0x1001)
#define V4L2_CID_CBW           (V4L2_CTRL_CLASS_USER | 0x1002)
#define V4L2_CID_SCH           (V4L2_CTRL_CLASS_USER | 0x1003)

static int nxp_tvout_set_brightness(struct v4l2_ctrl *ctrl)
{
    //printk("%s: ctrl->val = %d\n", __func__, ctrl->val);

    struct nxp_tvout *me = ctrl_to_me(ctrl);
    if (me->brightness == ctrl->val)
        return 0;


    me->brightness = ctrl->val;
    NX_DPC_SetVideoEncoderSCHLockControl(me->module, CFALSE);
    NX_DPC_SetVideoEncoderColorControl(me->module,
            me->sch,
            me->hue,
            me->saturation,
            me->contrast,
            me->brightness);
    return 0;
}

static int nxp_tvout_set_contrast(struct v4l2_ctrl *ctrl)
{
    //printk("%s: ctrl->val = %d\n", __func__, ctrl->val);

    struct nxp_tvout *me = ctrl_to_me(ctrl);
    if (me->contrast == ctrl->val)
        return 0;


    me->contrast = ctrl->val;
    NX_DPC_SetVideoEncoderSCHLockControl(me->module, CFALSE);
    NX_DPC_SetVideoEncoderColorControl(me->module,
            me->sch,
            me->hue,
            me->saturation,
            me->contrast,
            me->brightness);

    return 0;
}

static int nxp_tvout_set_hue(struct v4l2_ctrl *ctrl)
{
    struct nxp_tvout *me = ctrl_to_me(ctrl);
    if (me->hue == ctrl->val)
        return 0;

    me->hue = ctrl->val;
    NX_DPC_SetVideoEncoderColorControl(me->module,
            me->sch,
            me->hue,
            me->saturation,
            me->contrast,
            me->brightness);

    return 0;
}

static int nxp_tvout_set_saturation(struct v4l2_ctrl *ctrl)
{
    struct nxp_tvout *me = ctrl_to_me(ctrl);
    if (me->saturation == ctrl->val)
        return 0;

    me->saturation = ctrl->val;
    NX_DPC_SetVideoEncoderColorControl(me->module,
            me->sch,
            me->hue,
            me->saturation,
            me->contrast,
            me->brightness);

    return 0;
}

static int nxp_tvout_set_sch(struct v4l2_ctrl *ctrl)
{
    struct nxp_tvout *me = ctrl_to_me(ctrl);
    if (me->sch == ctrl->val)
        return 0;

    me->sch = ctrl->val;
    NX_DPC_SetVideoEncoderColorControl(me->module,
            me->sch,
            me->hue,
            me->saturation,
            me->contrast,
            me->brightness);

    return 0;
}

static int nxp_tvout_set_ybw(struct v4l2_ctrl *ctrl)
{
    struct nxp_tvout *me = ctrl_to_me(ctrl);
    if (me->ybw == ctrl->val)
        return 0;

    me->ybw = ctrl->val;
    NX_DPC_SetVideoEncoderBandwidth(me->module, me->ybw, me->cbw);

    return 0;
}

static int nxp_tvout_set_cbw(struct v4l2_ctrl *ctrl)
{
    struct nxp_tvout *me = ctrl_to_me(ctrl);
    if (me->cbw == ctrl->val)
        return 0;

    me->cbw = ctrl->val;
    NX_DPC_SetVideoEncoderBandwidth(me->module, me->ybw, me->cbw);

    return 0;
}

static int nxp_tvout_s_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_BRIGHTNESS:
        return nxp_tvout_set_brightness(ctrl);
    case V4L2_CID_CONTRAST:
        return nxp_tvout_set_contrast(ctrl);
    case V4L2_CID_HUE:
        return nxp_tvout_set_hue(ctrl);
    case V4L2_CID_SATURATION:
        return nxp_tvout_set_saturation(ctrl);
    case V4L2_CID_YBW:
        return nxp_tvout_set_ybw(ctrl);
    case V4L2_CID_CBW:
        return nxp_tvout_set_cbw(ctrl);
    case V4L2_CID_SCH:
        return nxp_tvout_set_sch(ctrl);
    default:
        printk(KERN_ERR "%s: invalid control id 0x%x\n", __func__, ctrl->id);
        return -EINVAL;
    }
}

static const struct v4l2_ctrl_ops nxp_tvout_ctrl_ops = {
    .s_ctrl = nxp_tvout_s_ctrl,
};

static const struct v4l2_ctrl_config nxp_tvout_custom_ctrls[] = {
    {
        .ops  = &nxp_tvout_ctrl_ops,
        .id   = V4L2_CID_YBW,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .name = "LumaBandwidthControl",
        .min  = 0,
        .max  = 2,
        .def  = 0,
        .step = 1,
    }, {
        .ops  = &nxp_tvout_ctrl_ops,
        .id   = V4L2_CID_CBW,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .name = "ChromaBandwidthControl",
        .min  = 0,
        .max  = 2,
        .def  = 0,
        .step = 1,
    }, {
        .ops  = &nxp_tvout_ctrl_ops,
        .id   = V4L2_CID_SCH,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .name = "ColorBurstPhase",
        .min  = 0,
        .max  = 255,
        .def  = 0,
        .step = 1,
    }
};

// brightness : 0 ~ 127
// contrast : -128 ~ 0
// hue : 0 ~ 255
// saturation : 0 ~ 255
// ybw : 0 ~ 2
// cbw : 0 ~ 2
// sch : 0 ~ 255
#define NUM_CTLRS       7
static int nxp_tvout_initialize_ctrls(struct nxp_tvout *me)
{
    v4l2_ctrl_handler_init(&me->handler, NUM_CTLRS);

    /* standard controls */
    me->ctrl_brightness = v4l2_ctrl_new_std(&me->handler, &nxp_tvout_ctrl_ops,
            V4L2_CID_BRIGHTNESS, 0, 127, 1, 0);
    if (!me->ctrl_brightness) {
        printk(KERN_ERR "%s: failed to v4l2_ctrl_new_std for brightness\n", __func__);
        return -ENOENT;
    }

    me->ctrl_contrast = v4l2_ctrl_new_std(&me->handler, &nxp_tvout_ctrl_ops,
            V4L2_CID_CONTRAST, -128, 0, 1, 0);
    if (!me->ctrl_contrast) {
        printk(KERN_ERR "%s: failed to v4l2_ctrl_new_std for contrast\n", __func__);
        return -ENOENT;
    }

    me->ctrl_hue = v4l2_ctrl_new_std(&me->handler, &nxp_tvout_ctrl_ops,
            V4L2_CID_HUE, 0, 255, 1, 0);
    if (!me->ctrl_hue) {
        printk(KERN_ERR "%s: failed to v4l2_ctrl_new_std for hue\n", __func__);
        return -ENOENT;
    }

    me->ctrl_saturation = v4l2_ctrl_new_std(&me->handler, &nxp_tvout_ctrl_ops,
            V4L2_CID_SATURATION, 0, 255, 1, 0);
    if (!me->ctrl_saturation) {
        printk(KERN_ERR "%s: failed to v4l2_ctrl_new_std for saturation\n", __func__);
        return -ENOENT;
    }

    /* custom controls */
    me->ctrl_ybw = v4l2_ctrl_new_custom(&me->handler, &nxp_tvout_custom_ctrls[0], NULL);
    if (!me->ctrl_ybw) {
        printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for ybw\n", __func__);
        return -ENOENT;
    }

    me->ctrl_cbw = v4l2_ctrl_new_custom(&me->handler, &nxp_tvout_custom_ctrls[1], NULL);
    if (!me->ctrl_cbw) {
        printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for cbw\n", __func__);
        return -ENOENT;
    }

    me->ctrl_sch = v4l2_ctrl_new_custom(&me->handler, &nxp_tvout_custom_ctrls[2], NULL);
    if (!me->ctrl_sch) {
        printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for sch\n", __func__);
        return -ENOENT;
    }

    me->sd.ctrl_handler = &me->handler;
    if (me->handler.error) {
        printk(KERN_ERR "%s: ctrl handler error(%d)\n", __func__, me->handler.error);
        v4l2_ctrl_handler_free(&me->handler);
        return -EINVAL;
    }

    return 0;
}
/**
 * video ops
 */
static int nxp_tvout_s_stream(struct v4l2_subdev *sd, int enable)
{
    printk("%s: %d\n", __func__, enable);
    /* nxp_soc_disp_device_enable_all(DISP_DEVICE_SYNCGEN1, enable); */
    nxp_soc_disp_device_enable_all(1, enable);
    return 0;
}

static const struct v4l2_subdev_video_ops nxp_tvout_video_ops = {
    .s_stream = nxp_tvout_s_stream,
};

/**
 * subdev ops
 */
static const struct v4l2_subdev_ops nxp_tvout_subdev_ops = {
    .video = &nxp_tvout_video_ops,
};

static int _init_entities(struct nxp_tvout *me)
{
    int ret;
    struct v4l2_subdev *sd = &me->sd;
    struct media_pad *pad  = &me->pad;
    struct media_entity *entity = &sd->entity;

    v4l2_subdev_init(sd, &nxp_tvout_subdev_ops);

    strlcpy(sd->name, "NXP TVOUT", sizeof(sd->name));
    v4l2_set_subdevdata(sd, me);
    sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

    pad->flags = MEDIA_PAD_FL_SINK;
    ret = media_entity_init(entity, 1, pad, 0);
    if (ret < 0) {
        pr_err("%s: failed to media_entity_init()\n", __func__);
        return ret;
    }

    return 0;
}

struct nxp_tvout *create_nxp_tvout(void)
{
    int ret;
    struct nxp_tvout *me;

    me = kzalloc(sizeof(*me), GFP_KERNEL);
    if (!me) {
        pr_err("%s: failed to alloc me!!!\n", __func__);
        return NULL;
    }

    ret = _init_entities(me);
    if (ret < 0) {
        pr_err("%s: failed to _init_entities()\n", __func__);
        kfree(me);
        return NULL;
    }

    return me;
}

void release_nxp_tvout(struct nxp_tvout *me)
{
    kfree(me);
}

int register_nxp_tvout(struct nxp_tvout *me)
{
    int ret = v4l2_device_register_subdev(nxp_v4l2_get_v4l2_device(), &me->sd);
    if (ret < 0) {
        pr_err("%s: failed to v4l2_device_register_subdev()\n", __func__);
        return ret;
    }

    ret = nxp_tvout_initialize_ctrls(me);
    if (ret < 0) {
        printk(KERN_ERR "%s: failed to initiailze controls\n", __func__);
        kfree(me);
        return ret;
    }

    return 0;
}

void unregister_nxp_tvout(struct nxp_tvout *me)
{
    v4l2_device_unregister_subdev(&me->sd);
}

int suspend_nxp_tvout(struct nxp_tvout *me)
{
    // TODO
    return 0;
}

int resume_nxp_tvout(struct nxp_tvout *me)
{
    // TODO
    return 0;
}
