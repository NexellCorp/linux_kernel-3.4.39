#ifndef _NXP_TVOUT_H
#define _NXP_TVOUT_H

#include <media/v4l2-ctrls.h>

struct media_pad;
struct v4l2_subdev;

struct nxp_tvout {
    int module;

    struct v4l2_subdev sd;
    struct media_pad pad;
    struct v4l2_ctrl_handler handler;

    /* standard control */
    struct v4l2_ctrl *ctrl_brightness;
    struct v4l2_ctrl *ctrl_contrast;
    struct v4l2_ctrl *ctrl_hue;
    struct v4l2_ctrl *ctrl_saturation;
    /* custom control */
    struct v4l2_ctrl *ctrl_ybw;
    struct v4l2_ctrl *ctrl_cbw;
    struct v4l2_ctrl *ctrl_sch;

    /* control values */
    int brightness;
    int contrast;
    int hue;
    int saturation;
    int ybw;
    int cbw;
    int sch;
};

/**
 * publi api
 */
struct nxp_tvout *create_nxp_tvout(void);
void release_nxp_tvout(struct nxp_tvout *);
int register_nxp_tvout(struct nxp_tvout *);
void unregister_nxp_tvout(struct nxp_tvout *);
int suspend_nxp_tvout(struct nxp_tvout *);
int resume_nxp_tvout(struct nxp_tvout *);

#endif
