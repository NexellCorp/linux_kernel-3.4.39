#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/irq.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>

/*
#define CFG_IO_WIFI_PWR_ON              ((PAD_GPIO_B + 11) | PAD_FUNC_ALT0)   
#define CFG_IO_WIFI_RES_ON              ((PAD_GPIO_B + 27) | PAD_FUNC_ALT0)
*/

#define WLAN_STATIC_SCAN_BUF0           5
#define WLAN_STATIC_SCAN_BUF1           6
#define PREALLOC_WLAN_SEC_NUM           4
#define PREALLOC_WLAN_BUF_NUM           160
#define PREALLOC_WLAN_SECTION_HEADER    24

#define WLAN_SECTION_SIZE_0     (PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1     (PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2     (PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3     (PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE                 336
#define DHD_SKB_1PAGE_BUFSIZE   ((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE   ((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE   ((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM        17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
    void *mem_ptr;
    unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
    {NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
    {NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
    {NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
    {NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static void *wlan_static_scan_buf0 = NULL;
static void *wlan_static_scan_buf1 = NULL;

static void *_wifi_mem_prealloc(int section, unsigned long size)
{
    if (section == PREALLOC_WLAN_SEC_NUM)
        return wlan_static_skb;
    if (section == WLAN_STATIC_SCAN_BUF0)
        return wlan_static_scan_buf0;
    if (section == WLAN_STATIC_SCAN_BUF1)
        return wlan_static_scan_buf1;

    if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
        return NULL;

    if (wlan_mem_array[section].size < size)
        return NULL;

    return wlan_mem_array[section].mem_ptr;
}

static int bcm_init_wlan_mem(void)
{
    int i;

    for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
        wlan_static_skb[i] = NULL;
    }

    for (i = 0; i < 8; i++) {
        wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
        if (!wlan_static_skb[i])
            goto err_skb_alloc;
    }

    for (; i < 16; i++) {
        wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
        if (!wlan_static_skb[i])
            goto err_skb_alloc;
    }

    wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
    if (!wlan_static_skb[i])
        goto err_skb_alloc;

    for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
        wlan_mem_array[i].mem_ptr =
            kmalloc(wlan_mem_array[i].size, GFP_KERNEL);
        if (!wlan_mem_array[i].mem_ptr)
            goto err_mem_alloc;
    }

    wlan_static_scan_buf0 = kmalloc(65536, GFP_KERNEL);
    if (!wlan_static_scan_buf0)
        goto err_mem_alloc;

    wlan_static_scan_buf1 = kmalloc(65536, GFP_KERNEL);
    if (!wlan_static_scan_buf1)
        goto err_static_scan_buf;

    pr_info("wifi: %s: WIFI MEM Allocated\n", __func__);
    return 0;

err_static_scan_buf:
    pr_err("%s: failed to allocate scan_buf0\n", __func__);
    kfree(wlan_static_scan_buf0);
    wlan_static_scan_buf0 = NULL;

err_mem_alloc:
    pr_err("%s: failed to allocate mem_alloc\n", __func__);
    for (; i >= 0 ; i--) {
        kfree(wlan_mem_array[i].mem_ptr);
        wlan_mem_array[i].mem_ptr = NULL;
    }

    i = WLAN_SKB_BUF_NUM;
err_skb_alloc:
    pr_err("%s: failed to allocate skb_alloc\n", __func__);
    for (; i >= 0 ; i--) {
        dev_kfree_skb(wlan_static_skb[i]);
        wlan_static_skb[i] = NULL;
    }

    return -ENOMEM;
}



static int _wifi_power(int on)
{
    printk("%s %d\n", __func__, on);
    nxp_soc_gpio_set_out_value(CFG_IO_WIFI_REG_ON, on);
    return 0;
}

static int _wifi_reset(int on)
{
    printk("%s %d\n", __func__, on);
    return 0;
}

static unsigned char _wifi_mac_addr[IFHWADDRLEN] = { 0, 0x90, 0x4c, 0, 0, 0 };

static int _wifi_get_mac_addr(unsigned char *buf)
{
    memcpy(buf, _wifi_mac_addr, IFHWADDRLEN);
    return 0;
}

extern struct platform_device dwmci_dev_ch2;
static void (*wifi_status_cb)(struct platform_device *, int state);

int bcm_wlan_ext_cd_init(
            void (*notify_func)(struct platform_device *, int))
{
    wifi_status_cb = notify_func;
    return 0;
}

int bcm_wlan_ext_cd_cleanup(
            void (*notify_func)(struct platform_device *, int))
{
    wifi_status_cb = NULL;
    return 0;
}


static int _wifi_set_carddetect(int val)
{
    printk("%s %d\n", __func__, val);

     if (wifi_status_cb)
        wifi_status_cb(&dwmci_dev_ch2, val);
    else
        pr_warning("%s: Nobody to notify\n", __func__);

    return 0;
}


static struct wifi_platform_data _wifi_control = {
    .set_power          = _wifi_power,
    .set_reset          = _wifi_reset,
    .set_carddetect     = _wifi_set_carddetect,
    .mem_prealloc       = _wifi_mem_prealloc,
    //.get_mac_addr       = _wifi_get_mac_addr,
    .get_mac_addr       = NULL,
    /*.get_country_code   = _wifi_get_country_code,*/
    .get_country_code   = NULL,
};


/* for OOB interrupt */
#define CONFIG_BROADCOM_WIFI_USE_OOB
#ifdef CONFIG_BROADCOM_WIFI_USE_OOB
static struct resource brcm_wlan_resouces[] = {
    [0] = {
        .name = "bcmdhd_wlan_irq",
#if defined(CONFIG_OOB_INTR_ALIVE_0)
        .start = IRQ_ALIVE_0,
        .end   = IRQ_ALIVE_0,
#elif defined(CONFIG_OOB_INTR_ALIVE_1)
        .start = IRQ_ALIVE_1,
        .end   = IRQ_ALIVE_1,
#elif defined(CONFIG_OOB_INTR_ALIVE_2)
        .start = IRQ_ALIVE_2,
        .end   = IRQ_ALIVE_2,
#elif defined(CONFIG_OOB_INTR_ALIVE_3)
        .start = IRQ_ALIVE_3,
        .end   = IRQ_ALIVE_3,
#elif defined(CONFIG_OOB_INTR_ALIVE_4)
        .start = IRQ_ALIVE_4,
        .end   = IRQ_ALIVE_4,
#elif defined(CONFIG_OOB_INTR_ALIVE_5)
        .start = IRQ_ALIVE_5,
        .end   = IRQ_ALIVE_5,
#elif defined(CONFIG_OOB_INTR_ALIVE_6)
        .start = IRQ_ALIVE_6,
        .end   = IRQ_ALIVE_6,
#elif defined(CONFIG_OOB_INTR_ALIVE_7)
        .start = IRQ_ALIVE_7,
        .end   = IRQ_ALIVE_7,
#endif
        .start = CFG_IO_WIFI_HOST_WAKE_UP_IRQ_NUM,
        .end   = CFG_IO_WIFI_HOST_WAKE_UP_IRQ_NUM,
        .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
    }
};
#endif

static struct platform_device bcm_wifi_device = {
    .name  = "bcmdhd_wlan",
    .id    = 0,
#ifdef CONFIG_BROADCOM_WIFI_USE_OOB
    .num_resources = ARRAY_SIZE(brcm_wlan_resouces),
    .resource = brcm_wlan_resouces,
#endif
    .dev   = {
        .platform_data = &_wifi_control,
    },
};

static int __init bcm_wifi_init_gpio_mem(struct platform_device *pdev)
{
	bcm_init_wlan_mem();
	return 0;
}

void __init init_bcm_wifi(void)
{
    bcm_wifi_init_gpio_mem(&bcm_wifi_device);
    platform_device_register(&bcm_wifi_device);
}

