#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
#include <linux/timer.h>
#endif

#include <mach/nxp-v4l2-platformdata.h>

#include "nxp-v4l2-common.h"

#include <nx_hdmi.h>

#include <mach/hdmi/regs-hdmi.h>
#include <mach/hdmi/hdmi-priv.h>
#include <mach/hdmi/nxp-hdcp.h>

/**
 * defines
 */
#define AN_SIZE                     8
#define AKSV_SIZE                   5
#define BKSV_SIZE                   5
#define MAX_KEY_SIZE                16

#define BKSV_RETRY_CNT              14
#define BKSV_DELAY                  100

#define DDC_RETRY_CNT               400000
#define DDC_DELAY                   25

#define KEY_LOAD_RETRY_CNT          1000
#define ENCRYPT_CHECK_CNT           10

#define KSV_FIFO_RETRY_CNT          50
#define KSV_FIFO_CHK_DELAY          100 /* ms */
#define KSV_LIST_RETRY_CNT          10000

#define BCAPS_SIZE                  1
#define BSTATUS_SIZE                2
#define SHA_1_HASH_SIZE             20
#define HDCP_MAX_DEVS               128
#define HDCP_KSV_SIZE               5

/* offset of HDCP port */
#define HDCP_BKSV                   0x00
#define HDCP_RI                     0x08
#define HDCP_AKSV                   0x10
#define HDCP_AN                     0x18
#define HDCP_SHA1                   0x20
#define HDCP_BCAPS                  0x40
#define HDCP_BSTATUS                0x41
#define HDCP_KSVFIFO                0x43

#define KSV_FIFO_READY              (0x1 << 5)

#define MAX_CASCADE_EXCEEDED_ERROR  (-2)
#define MAX_DEVS_EXCEEDED_ERROR     (-3)
#define REPEATER_ILLEGAL_DEVICE_ERROR   (-4)
#define REPEATER_TIMEOUT_ERROR      (-5)

#define MAX_CASCADE_EXCEEDED        (0x1 << 3)
#define MAX_DEVS_EXCEEDED           (0x1 << 7)

#define nxp_hdcp_to_parent(me)       \
           container_of(me, struct nxp_hdmi, hdcp)

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
#define IS_HDMI_RUNNING(me) hdmi_hpd_status()
#else
#define IS_HDMI_RUNNING(me) true
#endif

/* #define DEBUG_KEY */
/* #define DEBUG_I2C */
#define DEBUG_BKSV
#define DEBUG_AKSV
/* #define DEBUG_RI */
/* #define DEBUG_REPEATER */
/* #define DEBUG_EVENT */
#define DEBUG_ALL

#if defined(DEBUG_KEY) || defined(DEBUG_ALL)
#define dbg_key(a...) printk(a)
#else
#define dbg_key(a...)
#endif

#if defined(DEBUG_I2C) || defined(DEBUG_ALL)
#define dbg_i2c(a...) printk(a)
#else
#define dbg_i2c(a...)
#endif

#if defined(DEBUG_BKSV) || defined(DEBUG_ALL)
#define dbg_bksv(a...) printk(a)
#else
#define dbg_bksv(a...)
#endif

#if defined(DEBUG_AKSV) || defined(DEBUG_ALL)
#define dbg_aksv(a...) printk(a)
#else
#define dbg_aksv(a...)
#endif

#if defined(DEBUG_RI) || defined(DEBUG_ALL)
#define dbg_ri(a...) printk(a)
#else
#define dbg_ri(a...)
#endif

#if defined(DEBUG_REPEATER) || defined(DEBUG_ALL)
#define dbg_repeater(a...) printk(a)
#else
#define dbg_repeater(a...)
#endif

#if defined(DEBUG_EVENT) || defined(DEBUG_ALL)
#define dbg_event(a...) printk(a)
#else
#define dbg_event(a...)
#endif

/**
 * internal functions
 */
static int _hdcp_i2c_read(struct nxp_hdcp *me, u8 offset, int bytes, u8 *buf)
{
    struct i2c_client *client = me->client;
    int ret, cnt = 0;

    struct i2c_msg msg[] = {
        [0] = {
            .addr  = client->addr,
            .flags = 0,
            .len   = 1,
            .buf   = &offset
        },
        [1] = {
            .addr  = client->addr,
            .flags = I2C_M_RD,
            .len   = bytes,
            .buf   = buf
        }
    };

    do {
        if (!IS_HDMI_RUNNING(me))
            return -ENODEV;

        ret = i2c_transfer(client->adapter, msg, 2);

        if (ret < 0 || ret != 2)
            pr_err("%s: can't read data, retry %d\n", __func__, cnt);
        else
            break;

        if (me->auth_state == FIRST_AUTHENTICATION_DONE ||
            me->auth_state == SECOND_AUTHENTICATION_DONE)
            goto ddc_read_err;

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
		// patch for HDMI Compliance Test, 1B-02
        if (!IS_HDMI_RUNNING(me))
			return 0;
#endif

        msleep(DDC_DELAY);
        cnt++;
    } while (cnt < DDC_RETRY_CNT);

    if (cnt == DDC_RETRY_CNT)
        goto ddc_read_err;

    dbg_i2c("%s: read data ok\n", __func__);

    return 0;

ddc_read_err:
    pr_err("%s: can't read data, timeout\n", __func__);
    return -ETIMEDOUT;
}

static int _hdcp_i2c_write(struct nxp_hdcp *me, u8 offset, int bytes, u8 *buf)
{
    struct i2c_client *client = me->client;
    u8 msg[bytes + 1];
    int ret, cnt = 0;

    msg[0] = offset;
    memcpy(&msg[1], buf, bytes);

    do {
        if (!IS_HDMI_RUNNING(me))
            return -ENODEV;

        ret = i2c_master_send(client, msg, bytes + 1);

        if (ret < 0 || ret < bytes + 1)
            pr_err("%s: can't write data, retry %d\n", __func__, cnt);
        else
            break;

        msleep(DDC_DELAY);
        cnt++;
    } while (cnt < DDC_RETRY_CNT);

    if (cnt == DDC_RETRY_CNT)
        goto ddc_write_err;

    dbg_i2c("%s: write data ok\n", __func__);
    return 0;

ddc_write_err:
    pr_err("%s: can't write data, timeout\n", __func__);
    return -ETIMEDOUT;
}

static void _hdcp_encryption(struct nxp_hdcp *me, bool on)
{
    if (on)
        hdmi_write_mask(HDMI_ENC_EN, ~0, HDMI_HDCP_ENC_ENABLE);
    else
        hdmi_write_mask(HDMI_ENC_EN, 0, HDMI_HDCP_ENC_ENABLE);

    hdmi_mute(!on);
}

__attribute__((__unused__)) static int _hdcp_dump_key(struct nxp_hdcp *me, int size, int reg, int offset)
{
    u8 buf[MAX_KEY_SIZE];
    int i;

    memset(buf, 0, sizeof(buf));
    hdmi_read_bytes(reg, buf, size);

    for (i = 0; i < size + 1; i++) 
        printk("%s[%d] : 0x%02x\n", offset == HDCP_AN ? "An" : "Aksv", i, buf[i]);

    return 0;
}

static int _hdcp_write_key(struct nxp_hdcp *me, int size, int reg, int offset)
{
    u8 buf[MAX_KEY_SIZE];
    int cnt, zero = 0;

    memset(buf, 0, sizeof(buf));
    hdmi_read_bytes(reg, buf, size);

    for (cnt = 0; cnt < size; cnt++)
        if (buf[cnt] == 0)
            zero++;

    if (zero == size) {
        pr_err("%s: %s is null\n", __func__, offset == HDCP_AN ? "An" : "Aksv");
#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
		// patch for HDMI Compliance Test, 1A-07
        if (me->is_err) {
            printk("%s: patch for 1A-07, if AN is null reset err\n", __func__);

            me->event      = HDCP_EVENT_STOP;
            me->auth_state = NOT_AUTHENTICATED;

            me->event |= HDCP_EVENT_READ_BKSV_START;
            me->is_err = true;
            dbg_event("restart first authentication --> READ BKSV START\n");
            queue_work(me->wq, &me->work);
            return 0;
        }
#endif
        goto write_key_err;
    }

    if (_hdcp_i2c_write(me, offset, size, buf) < 0) {
        pr_err("%s: failed to i2c write\n", __func__);
        goto write_key_err;
    }

#ifdef DEBUG_AKSV
    {
        int i;
        for (i = 0; i < size; i++) {
            printk("%s: %s[%d] : 0x%02x\n", __func__,
                    offset == HDCP_AN ? "An" : "Aksv", i, buf[i]);
        }
    }
#endif

    return 0;

write_key_err:
    return -1;
}

static int _hdcp_read_bcaps(struct nxp_hdcp *me)
{
    u8 bcaps = 0;

    if (_hdcp_i2c_read(me, HDCP_BCAPS, BCAPS_SIZE, &bcaps) < 0) {
        pr_err("%s: failed to i2c read\n", __func__);
        return -ETIMEDOUT;
    }

    if (!IS_HDMI_RUNNING(me)) {
        pr_err("%s: hdmi is not streaming!!!\n", __func__);
        return -ENODEV;
    }

    dbg_bksv("%s: bcaps 0x%x\n", __func__, bcaps);
    hdmi_writeb(HDMI_HDCP_BCAPS, bcaps);

    if (bcaps & HDMI_HDCP_BCAPS_REPEATER)
        me->is_repeater = true;
    else
        me->is_repeater = false;

    dbg_bksv("%s: device is %s\n", __func__,
            me->is_repeater ? "REPEAT" : "SINK");
    dbg_bksv("%s: [i2c] bcaps : 0x%02x\n", __func__, bcaps);

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
	// patch for HDMI Compliance Test, 1B-02
    if (!IS_HDMI_RUNNING(me)) {
        pr_err("%s: hdmi is not streaming!!!\n", __func__);
        return -ENODEV;
    }
#endif

    return 0;
}

static int _hdcp_read_bksv(struct nxp_hdcp *me)
{
    u8 bksv[BKSV_SIZE];
    int i, j;
    u32 one = 0, zero = 0, result = 0;
    u32 cnt = 0;

    memset(bksv, 0, sizeof(bksv));

    do {
        if (_hdcp_i2c_read(me, HDCP_BKSV, BKSV_SIZE, bksv) < 0) {
            pr_err("%s: failed to i2c read\n", __func__);
            return -ETIMEDOUT;
        }

#ifdef DEBUG_BKSV
        for (i = 0; i < BKSV_SIZE; i++) {
            printk("%s: i2c read: bksv[%d]: 0x%x\n",
                    __func__, i, bksv[i]);
        }
#endif

        for (i = 0; i < BKSV_SIZE; i++) {
            for (j = 0; j < 8; j++) {
                result = bksv[i] & (0x1 << j);
                if (result)
                    one++;
                else
                    zero++;
            }
        }

        if (!IS_HDMI_RUNNING(me)) {
            pr_err("%s: hdmi is not running\n", __func__);
            return -ENODEV;
        }

        if ((zero == 20) && (one == 20)) {
            hdmi_write_bytes(HDMI_HDCP_BKSV_(0), bksv, BKSV_SIZE);
            break;
        }

        pr_err("%s: invalid bksv, retry : %d\n", __func__, cnt);

        msleep(BKSV_DELAY);
        cnt++;
    } while (cnt < BKSV_RETRY_CNT);

    if (cnt == BKSV_RETRY_CNT) {
        pr_err("%s: read timeout\n", __func__);
        return -ETIMEDOUT;
    }

    dbg_bksv("%s: bksv read OK, retry : %d\n", __func__, cnt);

    return 0;
}

static int _hdcp_read_ri(struct nxp_hdcp *me)
{
    u8 ri[2] = {0, 0};
    u8 rj[2] = {0, 0};

    ri[0] = hdmi_readb(HDMI_HDCP_RI_0);
    ri[1] = hdmi_readb(HDMI_HDCP_RI_1);

    if (_hdcp_i2c_read(me, HDCP_RI, 2, rj) < 0) {
        pr_err("%s: failed to i2c read\n", __func__);
        return -ETIMEDOUT;
    }

    dbg_ri("%s: Rx -> rj[0]: 0x%02x, rj[1]: 0x%02x\n", __func__,
            rj[0], rj[1]);
    dbg_ri("%s: Tx -> ri[0]: 0x%02x, ri[1]: 0x%02x\n", __func__,
            ri[0], ri[1]);

    if ((ri[0] == rj[0]) && (ri[1] == rj[1]) && (ri[0] | ri[1]))
        hdmi_writeb(HDMI_HDCP_CHECK_RESULT, HDMI_HDCP_RI_MATCH_RESULT_Y);
    else {
        hdmi_writeb(HDMI_HDCP_CHECK_RESULT, HDMI_HDCP_RI_MATCH_RESULT_N);
#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
		// patch for HDMI Compliance Test, 1B-02
		pr_err("ENCRYPTION OFF\n");
		_hdcp_encryption(me, false);
#endif
        pr_err("%s: compare error\n", __func__);
        goto compare_err;
    }

    dbg_ri("%s: ri and rj are matched\n", __func__);
    return 0;

compare_err:
    me->event = HDCP_EVENT_STOP;
    me->auth_state = NOT_AUTHENTICATED;

    msleep(10);

    return -EINVAL;
}

static void _hdcp_sw_reset(struct nxp_hdcp *me)
{
    u32 val = hdmi_get_int_mask();

    hdmi_set_int_mask(HDMI_INTC_EN_HPD_PLUG, false);
    hdmi_set_int_mask(HDMI_INTC_EN_HPD_UNPLUG, false);

    hdmi_sw_hpd_enable(true);
    hdmi_sw_hpd_plug(false);
    hdmi_sw_hpd_plug(true);
    hdmi_sw_hpd_enable(false);

    if (val & HDMI_INTC_EN_HPD_PLUG)
        hdmi_set_int_mask(HDMI_INTC_EN_HPD_PLUG, true);
    if (val & HDMI_INTC_EN_HPD_UNPLUG)
        hdmi_set_int_mask(HDMI_INTC_EN_HPD_UNPLUG, true);
}

static int _hdcp_reset_auth(struct nxp_hdcp *me)
{
    u32 val;

    if (!IS_HDMI_RUNNING(me))
        return -ENODEV;

    me->event      = HDCP_EVENT_STOP;
    me->auth_state = NOT_AUTHENTICATED;

    hdmi_write(HDMI_HDCP_CTRL1, 0x0);
    hdmi_write(HDMI_HDCP_CTRL2, 0x0);
    hdmi_mute(true);

    _hdcp_encryption(me, false);

    printk("%s: reset authentication\n", __func__);

    val = HDMI_UPDATE_RI_INT_EN | HDMI_WRITE_INT_EN |
        HDMI_WATCHDOG_INT_EN | HDMI_WTFORACTIVERX_INT_EN;
    hdmi_write_mask(HDMI_STATUS_EN, 0, val);

    hdmi_writeb(HDMI_HDCP_CHECK_RESULT, HDMI_HDCP_CLR_ALL_RESULTS);

    /* delat 1 frame */
    msleep(16);

    _hdcp_sw_reset(me);

    val = HDMI_UPDATE_RI_INT_EN | HDMI_WRITE_INT_EN |
        HDMI_WATCHDOG_INT_EN | HDMI_WTFORACTIVERX_INT_EN;
    hdmi_write_mask(HDMI_STATUS_EN, ~0, val);
    hdmi_write_mask(HDMI_HDCP_CTRL1, ~0, HDMI_HDCP_CP_DESIRED_EN);

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
	// patch for HDMI Compliance Test, 1A-06, 1A-07, 1A-07a
	if (hdmi_hpd_status()) {
		if (me->err_num == -EINVAL) {
			me->event |= HDCP_EVENT_READ_BKSV_START;
			me->is_err = true;
			dbg_event("restart first authentication --> READ BKSV START\n");
			queue_work(me->wq, &me->work);
		} else if (me->err_num == -ETIMEDOUT) {
			// for 1A-07a 
			printk("%s: set timer after 2.5 seconds\n", __func__);
			mod_timer(&me->timer, jiffies + msecs_to_jiffies(2500));
		}
	}
#endif

    printk("%s exit\n", __func__);
    return 0;
}

#define HDCP_KEY_SIZE 288

/* this is not real key !!! */
static unsigned char _hdcp_encrypted_table[] ={ 
    0x93,0xB4,0x09,0x96,0x94,0x5D,0x15,0x2B,0x4D,0xA5,0xC6,0x58,0xC5,0xBE,0x41,0x40,//10
    0xF3,0xCD,0xCF,0xF9,0xD2,0x06,0x5B,0xDF,0xC2,0x41,0x32,0xFE,0x03,0xF2,0xA8,0x48,//20
    0x87,0x7B,0xD2,0x43,0xFB,0x74,0x2F,0xC8,0x07,0x1C,0x9D,0x46,0xA3,0x93,0x9D,0x28,//30
    0x8F,0xB2,0xA0,0xDB,0x87,0xCC,0x56,0x28,0x50,0xD8,0x57,0x29,0xB6,0x3C,0x9E,0xC3,//40
    0x69,0x6C,0xB3,0xC7,0xC0,0x97,0xE2,0xA6,0xB7,0x6E,0x74,0xC8,0x74,0x19,0xEB,0x62,//50
    0xAB,0x55,0x36,0x20,0xCF,0xF7,0xC6,0xD9,0x2C,0x77,0xA6,0x9A,0x76,0x07,0x3C,0x5A,//60
    0x26,0x62,0xBB,0xA4,0x7A,0x01,0xFC,0x51,0x68,0x46,0x21,0x83,0xFF,0x4D,0xC7,0x78,//70
    0x28,0xEA,0xAC,0xFD,0x44,0xB6,0xD7,0xA7,0x04,0xC4,0xC8,0xE1,0xAB,0x8D,0xE7,0x7E,//80
    0xFF,0x8D,0x2D,0xC3,0x6C,0xD1,0x67,0x6E,0xFC,0xD8,0xD9,0xC1,0x00,0xB4,0x12,0x06,//90
    0x9D,0x8F,0x00,0x93,0xF4,0xAE,0x69,0x43,0xA0,0x27,0xA9,0x9C,0x03,0x4A,0x1D,0x94,//a0
    0x3A,0xB2,0x2E,0xD2,0x57,0x5C,0x38,0x07,0x6A,0x93,0xAF,0xD2,0x12,0x4C,0x76,0x02,//b0
    0x72,0x78,0x4D,0x18,0xD6,0x3A,0x0B,0x25,0x94,0x55,0xB5,0xDA,0x8F,0xD1,0x2B,0xA4,//c0
    0x26,0xB2,0x42,0x8F,0xC9,0xCB,0xDA,0xEE,0x8D,0xF3,0x1F,0xF5,0xBD,0x7B,0xA7,0x4C,//d0
    0x24,0x34,0x8F,0x25,0xB5,0xF8,0xDB,0xAB,0xCA,0x8B,0xB6,0x34,0x91,0x38,0xA2,0xDC,//e0
    0xA9,0x91,0x39,0x0E,0x16,0x96,0x0E,0x2D,0xF1,0xB2,0x2F,0xB6,0xD5,0xAC,0xC6,0xD1,//f0
    0x81,0xED,0x36,0x07,0xBA,0x49,0x5B,0x3A,0x53,0x47,0x69,0x39,0x3E,0xBA,0xC6,0x14,//100
    0x3C,0xAC,0x84,0x63,0xD7,0xFB,0x84,0xB5,0x18,0xB3,0xE2,0x96,0x61,0xBD,0xF1,0x23,//110
    0x2D,0x58,0xF5,0xFA,0x98,0xA8,0xA2,0x7E,0xBA,0x69,0xAA,0x5B,0x0D,0x7E,0x11,0xEE,//120
    0x53,0x8D,0xF4,0x8E,0x2C,0xA5,0x47,0xB9,0xE2,0xB7,0x8C,0xC1,0x70,0xC9,0xCA,0x59,//130
    0x35,0xD5,0x7E,0x1D,0xF9,0x93,0x0F,0x6D,0x0C,0x7C,0xA8,0x79,0x06,0x29,0xD2,0x7F,//140
};

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/syscalls.h>

#define ENCRYPTED_FILE "/data/ENCRYPTED_HDCP_KEY_TABLE.bin"

static int _hdcp_read_keytable(struct nxp_hdcp *me, unsigned char *buf)
{
    char *file = ENCRYPTED_FILE;
    struct file *fp = NULL;
    int ret = 0;
    fp = filp_open(file, O_RDONLY, 0);
    if (IS_ERR(fp)) {
        pr_err("%s: failed to filp_open for %s\n", __func__, file);
        return -EINVAL;
    }
    dbg_key("%s: succeed to file_open for %s\n", __func__, file);

    ret = kernel_read(fp, 0, buf, HDCP_KEY_SIZE);
    if (ret != HDCP_KEY_SIZE) {
        pr_err(KERN_ERR "%s: read error(%d/%d)\n", __func__, ret, HDCP_KEY_SIZE);
        ret = -EINVAL;
    }
    ret = 0;

    if (fp)
        filp_close(fp, NULL);

#ifdef DEBUG_KEY
    {
        int i;
        printk("HDCP ENCRYPTED TABLE ====> \n");
        for (i = 0; i < HDCP_KEY_SIZE; i++) {
            printk("0x%2x,", buf[i]);
            if ((i % 16) == 15) {
                printk("\n");
            }
        }
        printk("\n<====\n");
    }
#endif

    return ret;
}

static int _hdcp_loadkey(struct nxp_hdcp *me)
{
    int i;

    u32 val = hdmi_readb(HDMI_HDCP_KEY_LOAD);
    if (!(val & 1)) {
        pr_err("%s: HDMI_EFUSE_ECC_FAIL\n", __func__);
        return -1;
    }

    if (_hdcp_read_keytable(me, _hdcp_encrypted_table)) {
        pr_err("%s: failed to _hdcp_read_keytable\n", __func__);
        return -1;
    }

    dbg_key("AES_DATA_SIZE_L --> 0x%02x\n", hdmi_readb(HDMI_LINK_AES_DATA_SIZE_L));
    dbg_key("AES_DATA_SIZE_H --> 0x%02x\n", hdmi_readb(HDMI_LINK_AES_DATA_SIZE_H));
    for (i = 0; i < HDCP_KEY_SIZE; i++)
        hdmi_writeb(HDMI_LINK_AES_DATA, _hdcp_encrypted_table[i]);

#ifdef DEBUG_KEY
    {
        u8 read_buf[HDCP_KEY_SIZE] = {0, };
        printk("DUMP HDCP ENCRYPTED TABLE from HDMI_LINK_AES_DATA ====> \n");
        for (i = 0; i < HDCP_KEY_SIZE; i++) {
            read_buf[i] = hdmi_readb(HDMI_LINK_AES_DATA);
            printk("0x%02x,", read_buf[i]);
            if ((i % 16) == 15)
                printk("\n");
        }
        printk("\n<====\n");
    }
#endif

    hdmi_writeb(HDMI_LINK_AES_START, 0x01);

    do {
        val = hdmi_readb(HDMI_LINK_AES_START);
        dbg_key("%s: AES_START --> 0x%02x\n", __func__, val);
    } while (val != 0x00);

    return 0;
}

static int _hdcp_start_encryption(struct nxp_hdcp *me)
{
    u32 val;
    u32 cnt = 0;

    do {
        val = hdmi_readb(HDMI_STATUS);

        if (val & HDMI_AUTHEN_ACK_AUTH) {
            _hdcp_encryption(me, true);
            break;
        }

        mdelay(1);
        cnt++;
    } while (cnt < ENCRYPT_CHECK_CNT);

    if (cnt == ENCRYPT_CHECK_CNT) {
        pr_err("%s: error timeout\n", __func__);
        _hdcp_encryption(me, false);
        return -ETIMEDOUT;
    }

    dbg_ri("HDCP encryption is started\n");
    return 0;
}

static int _hdcp_check_repeater(struct nxp_hdcp *me)
{
    int val;
    int cnt = 0, cnt2 = 0;

    u8 bcaps = 0;
    u8 status[BSTATUS_SIZE];
    u8 rx_v[SHA_1_HASH_SIZE];
    u8 ksv_list[HDCP_MAX_DEVS * HDCP_KSV_SIZE];

    u32 dev_cnt;
    int ret = 0;

    memset(status, 0, sizeof(status));
    memset(rx_v, 0, sizeof(rx_v));
    memset(ksv_list, 0, sizeof(ksv_list));

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
	// patch for HDMI Compliance Test, 1B-02
    if (!IS_HDMI_RUNNING(me)) {
        pr_err("%s: hdmi is not running\n", __func__);
        return -EINVAL;
    }
#endif

    do {
        if (_hdcp_read_bcaps(me) < 0)
            goto check_repeater_err;

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
        // patch for HDMI Compliance Test, 1B-02
        if (!IS_HDMI_RUNNING(me)) {
            pr_err("%s: hdmi is not running\n", __func__);
            return -EINVAL;
        }
#endif
        bcaps = hdmi_readb(HDMI_HDCP_BCAPS);

        if (bcaps & KSV_FIFO_READY)
            break;

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
        // patch for HDMI Compliance Test, 1B-02
        if (!IS_HDMI_RUNNING(me)) {
            pr_err("%s: hdmi is not running\n", __func__);
            return -EINVAL;
        }
#endif

        msleep(KSV_FIFO_CHK_DELAY);
        cnt++;
    } while (cnt < KSV_FIFO_RETRY_CNT);

    if (cnt == KSV_FIFO_RETRY_CNT) {
        ret = REPEATER_TIMEOUT_ERROR;
        goto check_repeater_err;
    }

    dbg_repeater("%s: repeater : ksv fifo ready\n", __func__);

    if (_hdcp_i2c_read(me, HDCP_BSTATUS, BSTATUS_SIZE, status) < 0) {
        ret = -ETIMEDOUT;
        goto check_repeater_err;
    }

    if (status[1] & MAX_CASCADE_EXCEEDED) {
        ret = MAX_CASCADE_EXCEEDED_ERROR;
        goto check_repeater_err;
    } else if (status[0] & MAX_DEVS_EXCEEDED) {
        ret = MAX_DEVS_EXCEEDED_ERROR;
        goto check_repeater_err;
    }

    hdmi_writeb(HDMI_HDCP_BSTATUS_0, status[0]);
    hdmi_writeb(HDMI_HDCP_BSTATUS_1, status[1]);

    dbg_repeater("%s: status[0] :0x%02x\n", __func__, status[0]);
    dbg_repeater("%s: status[1] :0x%02x\n", __func__, status[1]);

    dev_cnt = status[0] & 0x7f;
    dbg_repeater("%s: repeater : dev cnt = %d\n", __func__, dev_cnt);

    if (dev_cnt) {
        if (_hdcp_i2c_read(me, HDCP_KSVFIFO, dev_cnt * HDCP_KSV_SIZE,
                    ksv_list) < 0) {
            pr_err("%s: failed to i2c read HDCP_KSVFIFO\n", __func__);
            ret = -ETIMEDOUT;
            goto check_repeater_err;
        }

        cnt = 0;

        do {
            hdmi_write_bytes(HDMI_HDCP_KSV_LIST_(0),
                    &ksv_list[cnt * 5], HDCP_KSV_SIZE);

            val = HDMI_HDCP_KSV_WRITE_DONE;

            if (cnt == dev_cnt - 1)
                val |= HDMI_HDCP_KSV_END;

            hdmi_write(HDMI_HDCP_KSV_LIST_CON, val);

            if (cnt < dev_cnt - 1) {
                cnt2 = 0;
                do {
                    val = hdmi_readb(HDMI_HDCP_KSV_LIST_CON);
                    if (val & HDMI_HDCP_KSV_READ)
                        break;
                    cnt2++;
                } while (cnt2 < KSV_LIST_RETRY_CNT);

                if (cnt2 == KSV_LIST_RETRY_CNT)
                    pr_err("%s: error, ksv list not readed\n", __func__);
            }
            cnt++;
        } while (cnt < dev_cnt);
    } else {
        hdmi_writeb(HDMI_HDCP_KSV_LIST_CON, HDMI_HDCP_KSV_LIST_EMPTY);
    }

    if (_hdcp_i2c_read(me, HDCP_SHA1, SHA_1_HASH_SIZE, rx_v) < 0) {
        pr_err("%s: failed to i2c read HDCP_SHA1\n", __func__);
        ret = -ETIMEDOUT;
        goto check_repeater_err;
    }

#ifdef DEBUG_REPEATER
    {
        int i;
        for (i = 0; i < SHA_1_HASH_SIZE; i++)
            printk("%s: [i2c] SHA-1 rx :: %02x\n", __func__, rx_v[i]);
    }
#endif

    hdmi_write_bytes(HDMI_HDCP_SHA1_(0), rx_v, SHA_1_HASH_SIZE);

    val = hdmi_readb(HDMI_HDCP_SHA_RESULT);
    if (val & HDMI_HDCP_SHA_VALID_RD) {
        if (val & HDMI_HDCP_SHA_VALID) {
            dbg_repeater("%s: SHA-1 result is ok\n", __func__);
            hdmi_writeb(HDMI_HDCP_SHA_RESULT, 0x0);
        } else {
            pr_err("%s: SHA-1 result is not valid\n", __func__);
            hdmi_writeb(HDMI_HDCP_SHA_RESULT, 0x0);
            ret = -EINVAL;
            goto check_repeater_err;
        }
    } else {
        pr_err("%s: SHA-1 result is not ready\n", __func__);
        hdmi_writeb(HDMI_HDCP_SHA_RESULT, 0x0);
        ret = -ETIMEDOUT;
        goto check_repeater_err;
    }

    dbg_repeater("%s: check repeater is ok\n", __func__);
    return 0;

check_repeater_err:
    pr_err("%s: failed(err: %d)\n", __func__, ret);
    return ret;
}

static int _hdcp_bksv(struct nxp_hdcp *me)
{
    me->auth_state = RECEIVER_READ_READY;

    if (_hdcp_read_bcaps(me) < 0)
        goto bksv_start_err;

    me->auth_state = BCAPS_READ_DONE;

    if (_hdcp_read_bksv(me) < 0)
        goto bksv_start_err;

    hdmi_writeb(HDMI_HDCP_CHECK_RESULT, HDMI_HDCP_CLR_ALL_RESULTS);

    me->auth_state = BKSV_READ_DONE;
    dbg_bksv("%s: bksv start is ok\n", __func__);

    return 0;

bksv_start_err:
#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
    // patch for HDMI Compliance Test, 1B-02
    if (me->is_err) {
        me->is_err = 0;
        me->event      = HDCP_EVENT_STOP;
        me->auth_state = NOT_AUTHENTICATED;
        return 0;
    }
#endif
    pr_err("%s: failed to start bskv\n", __func__);
    msleep(100);
    return -1;
}

static int _hdcp_second_auth(struct nxp_hdcp *me)
{
    int ret = 0;

    dbg_repeater("%s\n", __func__);

    if (!me->is_start) {
        pr_err("%s: hdcp is not started\n", __func__);
        return -EINVAL;
    }

    if (!IS_HDMI_RUNNING(me)) {
        pr_err("%s: hdmi is not running\n", __func__);
        return -EINVAL;
    }

    ret = _hdcp_check_repeater(me);

    if (!ret) {
        me->auth_state = SECOND_AUTHENTICATION_DONE;
        _hdcp_start_encryption(me);
    } else {
        switch (ret) {
        case REPEATER_ILLEGAL_DEVICE_ERROR:
            hdmi_writeb(HDMI_HDCP_CTRL2, 0x1);
            mdelay(1);
            hdmi_writeb(HDMI_HDCP_CTRL2, 0x0);
            pr_err("%s: repeater : illegal device\n", __func__);
            break;

        case REPEATER_TIMEOUT_ERROR:
            hdmi_write_mask(HDMI_HDCP_CTRL1, ~0,
                    HDMI_HDCP_SET_REPEATER_TIMEOUT);
            hdmi_write_mask(HDMI_HDCP_CTRL1, 0,
                    HDMI_HDCP_SET_REPEATER_TIMEOUT);
            pr_err("%s: repeater : timeout\n", __func__);
            break;

        case MAX_CASCADE_EXCEEDED_ERROR:
            pr_err("%s: repeater : exceeded MAX_CASCADE\n", __func__);
            break;

        case MAX_DEVS_EXCEEDED_ERROR:
            pr_err("%s: repeater : exceeded MAX_DEVS\n", __func__);
            break;

        default:
            break;
        }

        me->auth_state = NOT_AUTHENTICATED;
        return -EINVAL;
    }

    dbg_repeater("%s: second authentication is OK\n", __func__);
    return 0;
}

static int _hdcp_write_aksv(struct nxp_hdcp *me)
{
    dbg_aksv("%s\n", __func__);

    if (me->auth_state != BKSV_READ_DONE) {
        pr_err("%s: bksv is not ready\n", __func__);
        return -EINVAL;
    }

    if (!IS_HDMI_RUNNING(me)) {
        pr_err("%s: hdmi is not running\n", __func__);
        return -EINVAL;
    }

    if (_hdcp_write_key(me, AN_SIZE, HDMI_HDCP_AN_(0), HDCP_AN) < 0) {
        pr_err("%s: failed to _hdcp_write_key() HDCP_AN\n", __func__);
        return -EINVAL;
    }

    me->auth_state = AN_WRITE_DONE;
    dbg_aksv("%s: write AN is done\n", __func__);

    if (_hdcp_write_key(me, AKSV_SIZE, HDMI_HDCP_AKSV_(0), HDCP_AKSV) < 0) {
        pr_err("%s: failed to _hdcp_write_key() HDCP_AKSV\n", __func__);
        return -EINVAL;
    }

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
	// patch for HDMI Compliance Test, 1B-02
	msleep(200);
#else
    msleep(100);
#endif

    me->auth_state = AKSV_WRITE_DONE;

    dbg_aksv("%s success!!!\n", __func__);
    return 0;
}

static int _hdcp_check_ri(struct nxp_hdcp *me)
{
#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
	// patch for HDMI Compliance Test, 1A-07
	int ret = 0;
#endif
    dbg_ri("%s\n", __func__);

    if (me->auth_state < AKSV_WRITE_DONE) {
        int retry = 300;
        pr_err("%s: ri check is not ready\n", __func__);
        printk("%s: wait for state AKSV_WRITE_DONE for 3second\n", __func__);
        while (retry--) {
            msleep(10);
            if (me->auth_state >= AKSV_WRITE_DONE) {
                printk("state changed to 0x%x\n", me->auth_state);
                break;
            }
        }
        if (retry <= 0) {
            pr_err("%s: timeout wait for state AKSV_WRITE_DONE\n", __func__);
            return -EINVAL;
        }
    }

    if (!IS_HDMI_RUNNING(me)) {
        pr_err("%s: hdmi is not running\n", __func__);
        return -ENODEV;
    }

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
	// patch for HDMI Compliance Test, 1A-07
	ret = _hdcp_read_ri(me);
	if (ret != 0) {
        pr_err("%s: failed to _hdcp_read_ri(), ret %d\n", __func__, ret);
		return ret;
	}
#else
    if (_hdcp_read_ri(me) < 0) {
        pr_err("%s: failed to _hdcp_read_ri()\n", __func__);
        return -EINVAL;
    }
#endif

    if (me->is_repeater)
        me->auth_state = SECOND_AUTHENTICATION_RDY;
    else {
        me->auth_state = FIRST_AUTHENTICATION_DONE;
        _hdcp_start_encryption(me);
    }

    dbg_ri("%s: ri check is OK\n", __func__);
    return 0;
}

static unsigned int _hdcp_clear_event(unsigned int event)
{
    unsigned int my_event = event;
    if (my_event & HDCP_EVENT_READ_BKSV_START)
        my_event &= ~HDCP_EVENT_READ_BKSV_START;
    if (my_event & HDCP_EVENT_WRITE_AKSV_START)
        my_event &= ~HDCP_EVENT_WRITE_AKSV_START;
    if (my_event & HDCP_EVENT_SECOND_AUTH_START)
        my_event &= ~HDCP_EVENT_SECOND_AUTH_START;
    if (my_event & HDCP_EVENT_CHECK_RI_START)
        my_event &= ~HDCP_EVENT_CHECK_RI_START;
    return my_event;
}

/**
 * work
 */
static void _hdcp_work(struct work_struct *work)
{
    struct nxp_hdcp *me = container_of(work, struct nxp_hdcp, work);
    unsigned long flags;
    unsigned int event;

    if (!me->is_start)
        return;

    if (!IS_HDMI_RUNNING(me))
        return;

    dbg_event("%s entered\n", __func__);

    spin_lock_irqsave(&me->lock, flags);
    event = me->event;
    me->event = _hdcp_clear_event(event);
    spin_unlock_irqrestore(&me->lock, flags);

    dbg_event("event --> 0x%x\n", event);

    mutex_lock(&me->mutex);

    if (event & HDCP_EVENT_READ_BKSV_START) {
        if (_hdcp_bksv(me) < 0)
            goto work_err;
        else
            event &= ~HDCP_EVENT_READ_BKSV_START;
    }

    if (event & HDCP_EVENT_SECOND_AUTH_START) {
        if (_hdcp_second_auth(me) < 0)
            goto work_err;
    }

    if (event & HDCP_EVENT_WRITE_AKSV_START) {
        if (_hdcp_write_aksv(me) < 0)
            goto work_err;
    }

    if (event & HDCP_EVENT_CHECK_RI_START) {
#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
        // patch for HDMI Compliance Test, 1A-07
        int ret = _hdcp_check_ri(me);
        if (ret < 0) {
            me->err_num = ret;
            goto work_err;
        }
#else
        if (_hdcp_check_ri(me) < 0)
            goto work_err;
#endif
    }

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
    // patch for HDMI Compliance Test, 1A-07
    if (me->is_err) {
        me->is_err = false;
        if (IS_HDMI_RUNNING(me)) {
            me->event |= HDCP_EVENT_WRITE_AKSV_START;
            dbg_event("FORCE START WRITE_AKSV_START\n");
            queue_work(me->wq, &me->work);
        }
    }
#endif

    goto MUTEX_UNLOCK_STAGE;

work_err:
    if (!me->is_start)
        return;
    if (!IS_HDMI_RUNNING(me))
        return;

    _hdcp_reset_auth(me);

MUTEX_UNLOCK_STAGE:
    mutex_unlock(&me->mutex);
    dbg_event("%s exit\n", __func__);
    return;
}

/**
 * member functions
 */

static irqreturn_t nxp_hdcp_irq_handler(struct nxp_hdcp *me)
{
    u32 event = 0;
    u8 flag;

    if (!IS_HDMI_RUNNING(me)) {
        spin_lock(&me->lock);
        me->event       = HDCP_EVENT_STOP;
        spin_unlock(&me->lock);
        me->auth_state  = NOT_AUTHENTICATED;
        pr_err("%s: hdmi is not running\n", __func__);
        return IRQ_HANDLED;
    }

    flag = hdmi_readb(HDMI_STATUS);

    dbg_event("=====> %s: flag 0x%x\n", __func__, flag);

    if (flag & HDMI_WTFORACTIVERX_INT_OCC) {
        event |= HDCP_EVENT_READ_BKSV_START;
        hdmi_write_mask(HDMI_STATUS, ~0, HDMI_WTFORACTIVERX_INT_OCC);
        hdmi_write(HDMI_HDCP_I2C_INT, 0x0);
    }

    if (flag & HDMI_WRITE_INT_OCC) {
        event |= HDCP_EVENT_WRITE_AKSV_START;
        hdmi_write_mask(HDMI_STATUS, ~0, HDMI_WRITE_INT_OCC);
        hdmi_write(HDMI_HDCP_AN_INT, 0x0);
    }

    if (flag & HDMI_UPDATE_RI_INT_OCC) {
        event |= HDCP_EVENT_CHECK_RI_START;
        hdmi_write_mask(HDMI_STATUS, ~0, HDMI_UPDATE_RI_INT_OCC);
        hdmi_write(HDMI_HDCP_RI_INT, 0x0);
    }

    if (flag & HDMI_WATCHDOG_INT_OCC) {
        event |= HDCP_EVENT_SECOND_AUTH_START;
        hdmi_write_mask(HDMI_STATUS, ~0, HDMI_WATCHDOG_INT_OCC);
        hdmi_write(HDMI_HDCP_WDT_INT, 0x0);
    }

    if (!event) {
        pr_err("%s: unknown irq\n", __func__);
        return IRQ_HANDLED;
    }

    if (IS_HDMI_RUNNING(me)) {
        spin_lock(&me->lock);
        me->event |= event;
        spin_unlock(&me->lock);
        dbg_event("<==== event: 0x%x\n", event);
        queue_work(me->wq, &me->work);
    } else {
        me->event = HDCP_EVENT_STOP;
        me->auth_state = NOT_AUTHENTICATED;
    }

    return IRQ_HANDLED;
}

static int nxp_hdcp_prepare(struct nxp_hdcp *me)
{
    me->wq = create_singlethread_workqueue("khdcpd");
    if (!me->wq)
        return -ENOMEM;

    INIT_WORK(&me->work, _hdcp_work);
    return 0;
}

static int nxp_hdcp_start(struct nxp_hdcp *me)
{
    if (me->is_start) {
        printk(KERN_ERR "%s: called duplicate\n", __func__);
        return 0;
    }

    printk("%s entered\n", __func__);

    me->event = HDCP_EVENT_STOP;
    me->auth_state = NOT_AUTHENTICATED;

    _hdcp_sw_reset(me);
    _hdcp_encryption(me, false);

    msleep(120);

    if (_hdcp_loadkey(me) < 0) {
        pr_err("%s: failed to _hdcp_loadkey()\n", __func__);
        return -1;
    }

    hdmi_write(HDMI_GCP_CON, HDMI_GCP_CON_NO_TRAN);
    hdmi_write(HDMI_STATUS_EN, HDMI_INT_EN_ALL);
    hdmi_write(HDMI_HDCP_CTRL1, HDMI_HDCP_CP_DESIRED_EN);

    me->is_start = true;

    hdmi_set_int_mask(HDMI_INTC_EN_HDCP, true);

    printk("%s exit\n", __func__);

    return 0;
}


static int nxp_hdcp_stop(struct nxp_hdcp *me)
{
    u32 val;

    if (!me->is_start)
        return 0;

    printk("%s enterd\n", __func__);

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
    // patch for HDMI Compliance Test, 1A-07a
    del_timer(&me->timer);
#endif

    /* first stop workqueue */
    cancel_work_sync(&me->work);
    flush_work(&me->work);

    hdmi_set_int_mask(HDMI_INTC_EN_HDCP, false);

    me->event       = HDCP_EVENT_STOP;
    me->auth_state  = NOT_AUTHENTICATED;
    me->is_start    = false;

    hdmi_writeb(HDMI_HDCP_CTRL1, 0x0);

    hdmi_sw_hpd_enable(false);

    val = HDMI_UPDATE_RI_INT_EN | HDMI_WRITE_INT_EN |
        HDMI_WATCHDOG_INT_EN | HDMI_WTFORACTIVERX_INT_EN;
    hdmi_write_mask(HDMI_STATUS_EN, 0, val);
    hdmi_write_mask(HDMI_STATUS_EN, ~0, val);

    hdmi_write_mask(HDMI_STATUS, ~0, HDMI_INT_EN_ALL);

    _hdcp_encryption(me, false);

    hdmi_writeb(HDMI_HDCP_CHECK_RESULT, HDMI_HDCP_CLR_ALL_RESULTS);

    // restore inten to default
    NX_HDMI_SetReg(0, HDMI_LINK_INTC_CON_0, (1<<6)|(1<<3)|(1<<2));

    printk("%s exit\n", __func__);

    return 0;
}

static int nxp_hdcp_suspend(struct nxp_hdcp *me)
{
    printk("%s\n", __func__);
    return 0;
}

static int nxp_hdcp_resume(struct nxp_hdcp *me)
{
    printk("%s\n", __func__);
    return 0;
}

/**
 * i2c driver
 */
static int __devinit _hdcp_i2c_probe(struct i2c_client *client,
        const struct i2c_device_id *dev_id)
{
    return 0;
}

static int _hdcp_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static struct i2c_device_id _hdcp_idtable[] = {
    {"nxp_hdcp", 0},
};
MODULE_DEVICE_TABLE(i2c, _hdcp_idtable);

static struct i2c_driver _hdcp_driver = {
    .driver = {
        .name  = "nxp_hdcp",
        .owner = THIS_MODULE,
    },
    .id_table  = _hdcp_idtable,
    .probe     = _hdcp_i2c_probe,
    .remove    = _hdcp_i2c_remove,
};

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
// patch for HDMI Compliance Test, 1A-07a
static void timer_handler(unsigned long priv)
{
    struct nxp_hdcp *me = (struct nxp_hdcp *)priv;

	if (!me)
		BUG();

	if (IS_HDMI_RUNNING(me) &&
		me->event == HDCP_EVENT_STOP &&
		me->auth_state == NOT_AUTHENTICATED) {
			me->event |= HDCP_EVENT_READ_BKSV_START;
			me->is_err = true;
			dbg_event("timer ---> READ BKSV START\n");
			queue_work(me->wq, &me->work);
	}
}
#endif

/**
 * public api
 */
int nxp_hdcp_init(struct nxp_hdcp *me, struct nxp_v4l2_i2c_board_info *i2c_info)
{
    int ret = 0;

    memset(me, 0, sizeof(struct nxp_hdcp));

    ret = i2c_add_driver(&_hdcp_driver);
    if (ret < 0) {
        pr_err("%s: failed to i2c_add_driver()\n", __func__);
        return -EINVAL;
    }

    me->client = nxp_v4l2_get_i2c_client(i2c_info);
    if (!me->client) {
        pr_err("%s: can't find hdcp i2c device\n", __func__);
        return -EINVAL;
    }

    me->irq_handler = nxp_hdcp_irq_handler;
    me->prepare = nxp_hdcp_prepare;
    me->start   = nxp_hdcp_start;
    me->stop    = nxp_hdcp_stop;
    me->suspend = nxp_hdcp_suspend;
    me->resume  = nxp_hdcp_resume;

    mutex_init(&me->mutex);
    spin_lock_init(&me->lock);

#ifdef CONFIG_PATCH_HDMI_COMPLIANCE_TEST
    // patch for HDMI Compliance Test, 1A-07a
    setup_timer(&me->timer, timer_handler, (long)me);
#endif

    return 0;
}

void nxp_hdcp_cleanup(struct nxp_hdcp *me)
{
    i2c_del_driver(&_hdcp_driver);

    if (me->client) {
        i2c_unregister_device(me->client);
        me->client = NULL;
    }
}
