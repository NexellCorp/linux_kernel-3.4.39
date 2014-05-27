#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/platform_device.h>

#include <mach/platform.h>
#include <mach/soc.h>
#include <mach/nxp-dfs-bclk.h>

#define BCLK_MIN    100000000
#define BCLK_MEDIUM 200000000
#define BCLK_MAX    400000000

/**
 * sysfs attributes
 */
static uint32_t bclk_min = BCLK_MIN;
/*static uint32_t bclk_medium = BCLK_MEDIUM;*/
static uint32_t bclk_max = BCLK_MAX;
static uint32_t enable = true;

#define ATOMIC_SET_MASK(PTR, MASK)  \
    do { \
        unsigned long oldval = atomic_read(PTR); \
        unsigned long newval = oldval | MASK; \
        atomic_cmpxchg(PTR, oldval, newval); \
    } while (0)

#define ATOMIC_CLEAR_MASK(PTR, MASK) \
        atomic_clear_mask(MASK, (unsigned long *)&((PTR)->counter))

struct vsync_callback_ctx {
    struct completion completion;
    uint32_t pll_num;
    uint32_t bclk;
};

static void _vsync_irq_callback(void *data)
{
    struct vsync_callback_ctx *ctx = data;
    nxp_cpu_pll_change_frequency(ctx->pll_num, ctx->bclk);
    complete(&ctx->completion);
}

static inline void _set_and_wait(uint32_t pll_num, uint32_t bclk)
{
    struct vsync_callback_ctx ctx;
    printk("%s: pll %d, bclk %d\n", __func__, pll_num, bclk);
    init_completion(&ctx.completion);
    ctx.pll_num = pll_num;
    ctx.bclk = bclk;
    nxp_soc_disp_register_irq_callback(0, _vsync_irq_callback, &ctx);
    wait_for_completion(&ctx.completion);
    nxp_soc_disp_unregister_irq_callback(0);
}

static int default_dfs_bclk_func(uint32_t pll_num, uint32_t counter, uint32_t user_bitmap, uint32_t current_bclk)
{
    if (counter > 0) {
        if (user_bitmap & ((1 << BCLK_USER_MPEG) | (1 << BCLK_USER_OGL))) {
            if (current_bclk != bclk_max) {
                uint32_t bclk = bclk_max;
                _set_and_wait(pll_num, bclk);
                return bclk;
            }
        }
    } else {
        if (current_bclk != bclk_min) {
            uint32_t bclk = bclk_min;
            _set_and_wait(pll_num, bclk);
            return bclk;
        }
    }
    return current_bclk;
}

struct dfs_bclk_manager {
    uint32_t bclk_pll_num;
    atomic_t counter;
    atomic_t user_bitmap;
    uint32_t current_bclk;

    dfs_bclk_func func;
} dfs_bclk_manager = {
#ifdef CONFIG_NXP4330_DFS_BCLK_PLL_0
    .bclk_pll_num = 0,
#else
    .bclk_pll_num = 1,
#endif
    .counter = ATOMIC_INIT(0),
    .user_bitmap = ATOMIC_INIT(0),
    .current_bclk = BCLK_MAX,
    .func = default_dfs_bclk_func
};

int bclk_get(uint32_t user)
{
    if (enable) {
        printk("%s: %d, %d\n", __func__, user, atomic_read(&dfs_bclk_manager.counter));
        atomic_inc(&dfs_bclk_manager.counter);
        ATOMIC_SET_MASK(&dfs_bclk_manager.user_bitmap, 1<<user);
        dfs_bclk_manager.current_bclk =
            dfs_bclk_manager.func(
                    dfs_bclk_manager.bclk_pll_num,
                    atomic_read(&dfs_bclk_manager.counter),
                    atomic_read(&dfs_bclk_manager.user_bitmap),
                    dfs_bclk_manager.current_bclk
                    );
    }
    return 0;
}

int bclk_put(uint32_t user)
{
    if (enable) {
        printk("%s: %d, %d\n", __func__, user, atomic_read(&dfs_bclk_manager.counter));
        atomic_dec(&dfs_bclk_manager.counter);
        ATOMIC_CLEAR_MASK(&dfs_bclk_manager.user_bitmap, 1<<user);
        dfs_bclk_manager.current_bclk =
            dfs_bclk_manager.func(
                    dfs_bclk_manager.bclk_pll_num,
                    atomic_read(&dfs_bclk_manager.counter),
                    atomic_read(&dfs_bclk_manager.user_bitmap),
                    dfs_bclk_manager.current_bclk
                    );
    }
    return 0;
}

int register_dfs_bclk_func(dfs_bclk_func func)
{
    dfs_bclk_manager.func = func;
    return 0;
}

EXPORT_SYMBOL(bclk_get);
EXPORT_SYMBOL(bclk_put);
EXPORT_SYMBOL(register_dfs_bclk_func);

static ssize_t max_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", bclk_max);
}

static ssize_t max_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    uint32_t val;
    sscanf(buf, "%d", &val);
    if (val >= bclk_min && val <= BCLK_MAX) {
        printk("%s: bclk_max set to %d\n", __func__, val);
        bclk_max = val;
    } else {
        printk("%s: invalid value %d(%d-%d)\n", __func__, val, bclk_min, BCLK_MAX);
    }
    return n;
}

static ssize_t min_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", bclk_min);
}

static ssize_t min_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    uint32_t val;
    sscanf(buf, "%d", &val);
    if (val >= BCLK_MIN && val <= bclk_max) {
        printk("%s: bclk_max set to %d\n", __func__, val);
        bclk_max = val;
    } else {
        printk("%s: invalid value %d(%d-%d)\n", __func__, val, BCLK_MIN, bclk_max);
    }
    return n;
}

static ssize_t enable_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", enable);
}

static ssize_t enable_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    uint32_t val;
    sscanf(buf, "%d", &val);
    if (val > 0)
        enable = 1;
    else
        enable = 0;
    return n;
}

static ssize_t force_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    uint32_t val;
    sscanf(buf, "%d", &val);
    _set_and_wait(dfs_bclk_manager.bclk_pll_num, val);
    return n;
}

static struct device_attribute max_attr =
    __ATTR(max, S_IRUGO | S_IWUSR, max_show, max_store);
static struct device_attribute min_attr =
    __ATTR(min, S_IRUGO | S_IWUSR, min_show, min_store);
static struct device_attribute enable_attr =
    __ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);
static struct device_attribute force_attr =
    __ATTR(force, S_IRUGO | S_IWUSR, NULL, force_store);

static struct attribute *attrs[] = {
    &max_attr.attr,
    &min_attr.attr,
    &enable_attr.attr,
    &force_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
     .attrs = (struct attribute **)attrs,
};

static int __init dfs_bclk_init(void)
{
     struct kobject *kobj = NULL;
     int ret = 0;

     kobj = kobject_create_and_add("dfs-bclk", &platform_bus.kobj);
     if (!kobj) {
         printk(KERN_ERR "%s: Failed to create kobject for dfs-bclk\n", __func__);
         return -EINVAL;
     }

     ret = sysfs_create_group(kobj, &attr_group);
     if (ret) {
         printk(KERN_ERR "%s: Failed to sysfs_create_group for dfs-bclk\n", __func__);
         kobject_del(kobj);
         return -ret;
     }

     return 0;
}
module_init(dfs_bclk_init);

MODULE_AUTHOR("swpark <swpark@nexell.co.kr>");
MODULE_DESCRIPTION("DFS BCLK Manger for NXP4330");
MODULE_LICENSE("GPL");
