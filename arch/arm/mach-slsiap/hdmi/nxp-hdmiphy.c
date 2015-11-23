#define DEBUG 1

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/delay.h>

// for setting TX_AMP_LVL[0:4] : 5bit, 0 ~ 31
#include <linux/platform_device.h>

/* for prototype */
#include <nx_hdmi.h>

#include <mach/hdmi/regs-hdmi.h>
#include <mach/hdmi/hdmi-priv.h>
#include <mach/hdmi/nxp-hdmiphy.h>

#include <nx_rstcon.h>

#define NXP_HDMIPHY_PRESET_TABLE_SIZE   30

/**
 * preset tables : TODO
 */
static const u8 hdmiphy_preset25_2[32] = {
	0x52, 0x3f, 0x55, 0x40, 0x01, 0x00, 0xc8, 0x82,
    0xc8, 0xbd, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x06,
    0x80, 0x01, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xf4, 0x24, 0x00, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset25_175[32] = {
    0xd1, 0x1f, 0x50, 0x40, 0x20, 0x1e, 0xc8, 0x81,
    0xe8, 0xbd, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x06,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xf4, 0x24, 0x00, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset27[32] = {
    0xd1, 0x22, 0x51, 0x40, 0x08, 0xfc, 0xe0, 0x98,
    0xe8, 0xcb, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x06,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xe4, 0x24, 0x00, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset27_027[32] = {
    0xd1, 0x2d, 0x72, 0x40, 0x64, 0x12, 0xc8, 0x43,
    0xe8, 0x0e, 0xd9, 0x45, 0xa0, 0xac, 0x80, 0x06,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xe3, 0x24, 0x00, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset54[32] = {
    0x54, 0x2d, 0x35, 0x40, 0x01, 0x00, 0xc8, 0x82,
    0xc8, 0x0e, 0xd9, 0x45, 0xa0, 0xac, 0x80, 0x06,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xe4, 0x24, 0x01, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset54_054[32] = {
    0xd1, 0x2d, 0x32, 0x40, 0x64, 0x12, 0xc8, 0x43,
    0xe8, 0x0e, 0xd9, 0x45, 0xa0, 0xac, 0x80, 0x06,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xe3, 0x24, 0x01, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset74_175[32] = {
    0xd1, 0x1f, 0x10, 0x40, 0x5b, 0xef, 0xc8, 0x81,
    0xe8, 0xb9, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x56,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xa6, 0x24, 0x01, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset74_25[32] = {
    0xd1, 0x1f, 0x10, 0x40, 0x40, 0xf8, 0xc8, 0x81,
    0xe8, 0xba, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x56,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xa5, 0x24, 0x01, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset148_352[32] = {
    0xd1, 0x1f, 0x00, 0x40, 0x5b, 0xef, 0xc8, 0x81,
    0xe8, 0xb9, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x66,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0x4b, 0x25, 0x03, 0x00, 0x00, 0x01, 0x80, 0x10,
};

static const u8 hdmiphy_preset148_5[32] = {
    0xd1, 0x1f, 0x00, 0x40, 0x40, 0xf8, 0xc8, 0x81,
    0xe8, 0xba, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x6b,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0x4b, 0x25, 0x03, 0x00, 0x00, 0x01, 0x80, 0x10,
};

const struct hdmiphy_preset hdmiphy_preset[] = {
	{ V4L2_DV_480P59_94, hdmiphy_preset27 },
	{ V4L2_DV_480P60, hdmiphy_preset27_027 },
	{ V4L2_DV_576P50, hdmiphy_preset27 },
	{ V4L2_DV_720P50, hdmiphy_preset74_25 },
	{ V4L2_DV_720P59_94, hdmiphy_preset74_175 },
	{ V4L2_DV_720P60, hdmiphy_preset74_25 },
    { V4L2_DV_1080P24, hdmiphy_preset74_25 },
	{ V4L2_DV_1080P50, hdmiphy_preset148_5 },
	{ V4L2_DV_1080P59_94, hdmiphy_preset148_352 },
	{ V4L2_DV_1080P60, hdmiphy_preset148_5 },
};

const int hdmiphy_preset_cnt = ARRAY_SIZE(hdmiphy_preset);

static const u8 *_hdmiphy_preset2conf(u32 preset)
{
    int i;
    for (i = 0; i < hdmiphy_preset_cnt; i++) {
        if (hdmiphy_preset[i].preset == preset)
            return hdmiphy_preset[i].data;
    }
    return NULL;
}

/* TODO */
static int _hdmiphy_enable_pad(struct nxp_hdmiphy *me)
{
    pr_debug("%s\n", __func__);
    return 0;
}

/* TODO */
static int _hdmiphy_reset(struct nxp_hdmiphy *me)
{
    pr_debug("%s\n", __func__);
#if defined (CONFIG_ARCH_S5P4418)
    NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_PHY), RSTCON_nDISABLE);
#elif defined (CONFIG_ARCH_S5P6818)
	NX_RSTCON_SetRST(NX_HDMI_GetResetNumber(0, i_nRST_PHY), RSTCON_ASSERT);
	mdelay(1);
	NX_RSTCON_SetRST(NX_HDMI_GetResetNumber(0, i_nRST_PHY), RSTCON_NEGATE);
#endif
    return 0;
}

/* TODO */
static int _hdmiphy_clk_enable(struct nxp_hdmiphy *me, int enable)
{
    pr_debug("%s\n", __func__);
    return 0;
}

static int _write_tx_level(int level);
static int _hdmiphy_reg_set(struct nxp_hdmiphy *me,
        const u8 *data, size_t size)
{
    int i;
    u32 reg_addr;
    pr_debug("%s\n", __func__);

    NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (0<<7));
#if defined (CONFIG_ARCH_S5P4418)
    NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (0<<7));
#endif
    NX_HDMI_SetReg(0, HDMI_PHY_Reg04, (0<<4));
#if defined (CONFIG_ARCH_S5P4418)
    NX_HDMI_SetReg(0, HDMI_PHY_Reg04, (0<<4));
#endif
    NX_HDMI_SetReg(0, HDMI_PHY_Reg24, (1<<7));
#if defined (CONFIG_ARCH_S5P4418)
    NX_HDMI_SetReg(0, HDMI_PHY_Reg24, (1<<7));
#endif

    for (i = 0, reg_addr = HDMI_PHY_Reg04; i < size; i++, reg_addr += 4) {
        NX_HDMI_SetReg(0, reg_addr, data[i]);
#if defined (CONFIG_ARCH_S5P4418)
        NX_HDMI_SetReg(0, reg_addr, data[i]);
#endif
    }

#ifdef CFG_HDMIPHY_TX_LEVEL
    _write_tx_level(CFG_HDMIPHY_TX_LEVEL);
#endif

    NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, 0x80);
#if defined (CONFIG_ARCH_S5P4418)
    NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, 0x80);
#endif
    NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (1<<7));
#if defined (CONFIG_ARCH_S5P4418)
    NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (1<<7));
#endif
    return 0;
}

/**
 * for sysfs control of TX_AMP_LVL[0:4] : 5bit, 0 ~ 31
 */
struct nxp_hdmiphy *_me = NULL;
static int _read_tx_level(void)
{
    if (!_me->enabled)
        return -1;
    else {
        int level = 0;
        u32 val = NX_HDMI_GetReg(0, HDMI_PHY_Reg3C);
        level = (val & 0x80) >> 7;
        val = NX_HDMI_GetReg(0, HDMI_PHY_Reg40);
        val &= 0x0f;
        val <<= 1;
        level |= val;
        return level;
    }
}

static int _write_tx_level(int level)
{
    if (level > 31) {
        printk(KERN_ERR "Invalid level %d\n", level);
        printk(KERN_ERR "Valid HDMIPHY TX LEVEL range : 0 ~ 31\n");
        return -1;
    }

    if (!_me->enabled)
        return -1;
    else {
        u32 val;
        u32 regval;
        int cur_val = _read_tx_level();
        if (cur_val == level) {
            printk("%s: same to current tx level %d\n", __func__, cur_val);
            return 0;
        }

        NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (0<<7));
#if defined (CONFIG_ARCH_S5P4418)
        NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (0<<7));
#endif

        regval = NX_HDMI_GetReg(0, HDMI_PHY_Reg3C);
#if defined (CONFIG_ARCH_S5P4418)
        regval = NX_HDMI_GetReg(0, HDMI_PHY_Reg3C);
#endif
        val = level & 0x1;
        val <<= 7;
        regval &= ~0x80;
        regval |= val;
        pr_debug("[HDMIPHY] write reg3c: 0x%x\n", regval);
        NX_HDMI_SetReg(0, HDMI_PHY_Reg3C, regval);
#if defined (CONFIG_ARCH_S5P4418)
        NX_HDMI_SetReg(0, HDMI_PHY_Reg3C, regval);
#endif
        
        regval = NX_HDMI_GetReg(0, HDMI_PHY_Reg40);
#if defined (CONFIG_ARCH_S5P4418)
        regval = NX_HDMI_GetReg(0, HDMI_PHY_Reg40);
#endif
        val = (level & 0x1f) >> 1;
        regval &= ~0x0f;
        regval |= val;
        pr_debug("[HDMIPHY] write reg40: 0x%x\n", regval);
        NX_HDMI_SetReg(0, HDMI_PHY_Reg40, regval);
#if defined (CONFIG_ARCH_S5P4418)
        NX_HDMI_SetReg(0, HDMI_PHY_Reg40, regval);
#endif

        NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (1<<7));
#if defined (CONFIG_ARCH_S5P4418)
        NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (1<<7));
#endif
        return 0;
    }
}

static ssize_t tx_level_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
    int tx_level = _read_tx_level();
    return scnprintf(buf, PAGE_SIZE, "%d\n", tx_level);
}

static ssize_t tx_level_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    int tx_level;
    sscanf(buf, "%d", &tx_level);
    _write_tx_level(tx_level);
    return n;
}

static struct device_attribute tx_level_attr = __ATTR(tx_level, 0666, tx_level_show, tx_level_store);

static struct attribute *attrs[] = {
    &tx_level_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = (struct attribute **)attrs,
};

static int _init_sysfs(void)
{
    int ret = 0;
    struct kobject *kobj = NULL;

    kobj = kobject_create_and_add("hdmiphy", &platform_bus.kobj);
    if (!kobj) {
        printk(KERN_ERR "%s: failed to kobject_create_and_add for hdmiphy\n", __func__);
        return -EINVAL;
    }

    ret = sysfs_create_group(kobj, &attr_group);
    if (ret) {
        printk(KERN_ERR "%s: failed to sysfs_create_group for hdmiphy\n", __func__);
        kobject_del(kobj);
        return -ret;
    }

    return 0;
}

/**
 * member function
 */
static int nxp_hdmiphy_s_power(struct nxp_hdmiphy *me, int on)
{
    pr_debug("%s: %d\n", __func__, on);
    if (on) {
        _hdmiphy_enable_pad(me);
        _hdmiphy_reset(me);
    }
    _hdmiphy_clk_enable(me, on);

    return 0;
}

static int nxp_hdmiphy_s_dv_preset(struct nxp_hdmiphy *me,
        struct v4l2_dv_preset *preset)
{
    const u8 *data;
    pr_debug("%s: preset(%d)\n", __func__, preset->preset);

    data = _hdmiphy_preset2conf(preset->preset);;
    if (!data) {
        pr_err("%s: can't find preset\n", __func__);
        return -EINVAL;
    }

    me->preset = (u8 *)data;
    return 0;
}

static int nxp_hdmiphy_s_stream(struct nxp_hdmiphy *me, int enable)
{
    int ret;
    pr_debug("%s: %d\n", __func__, enable);

    if (enable) {
        ret = _hdmiphy_reg_set(me, me->preset, NXP_HDMIPHY_PRESET_TABLE_SIZE);
        if (ret < 0) {
            pr_err("%s: failed to _hdmiphy_reg_set()\n", __func__);
            return ret;
        }
        me->enabled = true;
    } else {
        me->enabled = false;
        _hdmiphy_reset(me);
    }

    return 0;
}

static int nxp_hdmiphy_suspend(struct nxp_hdmiphy *me)
{
    pr_debug("%s\n", __func__);
    return 0;
}

static int nxp_hdmiphy_resume(struct nxp_hdmiphy *me)
{
    pr_debug("%s\n", __func__);
    return 0;
}

/**
 * public api
 */
int nxp_hdmiphy_init(struct nxp_hdmiphy *me)
{
    int ret = 0;

    pr_debug("%s\n", __func__);

    me->s_power = nxp_hdmiphy_s_power;
    me->s_dv_preset = nxp_hdmiphy_s_dv_preset;
    me->s_stream = nxp_hdmiphy_s_stream;
    me->suspend = nxp_hdmiphy_suspend;
    me->resume = nxp_hdmiphy_resume;

    _init_sysfs();
    _me = me;

    return ret;
}

void nxp_hdmiphy_cleanup(struct nxp_hdmiphy *me)
{
    pr_debug("%s\n", __func__);
}
