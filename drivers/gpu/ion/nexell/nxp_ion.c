/*
 * (C) Copyright 2009
 * sung woo park, Nexell Co, <swpark@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/err.h>
#include <linux/ion.h>
#include <linux/export.h>
#include <linux/nxp_ion.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/cma.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/bitops.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/dma-buf.h>

#include <asm/pgtable.h>

#include "../ion_priv.h"

/**
 * variables
 * start "s_" : static
 * start "g_" : global
 */
struct ion_device *g_ion_nxp = NULL;

static int s_num_heaps = 0;
static struct ion_heap **s_heaps = NULL;
static struct device *s_nxp_ion_dev = NULL;

/*
 * external support functions
 */
struct ion_device *get_global_ion_device(void)
{
    return g_ion_nxp;
}
EXPORT_SYMBOL(get_global_ion_device);

#ifdef CONFIG_FALINUX_ZEROBOOT
#include <linux/mutex.h>

LIST_HEAD(zb_dma_used_node);
LIST_HEAD(zb_dma_free_node);

struct zb_dma_used_mem {
	unsigned long phys;
	unsigned long len;

	struct list_head dma_list;
};

// 1G 를 PAGE_SIZE 로 나누면
// 0x40000 = 256K
// bit operation 을 하게 되면 256Kbit
// 32KiB 면 4G 를 PAGE 로 정의할수 있다.

#define	ZB_DMA_MEM_SIZE	SZ_32K
#define	ZB_ION_COUNT	(ZB_DMA_MEM_SIZE / (sizeof(struct zb_dma_used_mem)))

#define	ZB_DMA_MEM_NODE_SIZE	SZ_1M
#define	ZB_DMA_MEM_NODE_COUNT	(ZB_DMA_MEM_NODE_SIZE / (sizeof(struct zb_dma_used_mem)))

extern u32 zero_trace;
struct zb_dma_used_mem *zb_dma_mem = NULL;
unsigned char *dma_used_mem;
EXPORT_SYMBOL(dma_used_mem);
int zb_dma_count = 0;
EXPORT_SYMBOL(zb_dma_count);

struct mutex zb_dma_lock;

void zb_dma_alloc(void)
{
//	dma_used_mem = (struct zb_dma_used_mem *)kzalloc(ZB_DMA_MEM_SIZE, GFP_KERNEL);
//	dma_used_mem[0].phys = 0xffffffff;	// to avoid 0 search bug

	dma_used_mem = (unsigned char *)kzalloc(ZB_DMA_MEM_SIZE, GFP_KERNEL);
}

struct zb_dma_used_mem* zb_dma_node_alloc(void)
{
	return (struct zb_dma_used_mem *)kzalloc(ZB_DMA_MEM_NODE_SIZE, GFP_KERNEL);
}

static void zb_dma_priv_set_bit(unsigned long phys)
{
	unsigned long dividend, remainder, order;
	unsigned char *imem = dma_used_mem;
	unsigned char mask;

	if (!imem) {
		printk("Fail!!! dma used mem is NULL\n");
		return;
	} 

	phys &= ~(PAGE_SIZE -1);
	order = phys / PAGE_SIZE;

	dividend = order >> 8;	
	remainder = order & 0x7;
	mask = 1 << remainder;

	imem[dividend] |= mask;
}

static void zb_dma_priv_clr_bit(unsigned long phys)
{
	unsigned long dividend, remainder, order;
	unsigned char *imem = dma_used_mem;
	unsigned char mask;

	if (!imem) {
		printk("Fail!!! dma used mem is NULL\n");
		return;
	} 

	phys &= ~(PAGE_SIZE -1);
	order = phys / PAGE_SIZE;

	dividend = order >> 8;	
	remainder = order & 0x7;
	mask = ~(1 << remainder);

	imem[dividend] &= mask;
}

static inline struct zb_dma_used_mem* zb_get_free_node(void)
{
	struct zb_dma_used_mem *z;
	unsigned long flags;

	//mutex_lock(&zb_dma_lock);
	local_irq_save(flags);	
	
	list_for_each_entry(z, &zb_dma_free_node, dma_list) {
		list_del(&z->dma_list);
		zb_dma_count++;
		mutex_unlock(&zb_dma_lock);
		return z;
	}

	//mutex_unlock(&zb_dma_lock);
	local_irq_restore(flags);
	return NULL;
}
static inline void zb_put_free_node(struct zb_dma_used_mem *z)
{
	unsigned long flags;

	//mutex_lock(&zb_dma_lock);
	local_irq_save(flags);	
	
	list_add(&z->dma_list, &zb_dma_free_node);

	if (zb_dma_count > 0) zb_dma_count--;
	//mutex_unlock(&zb_dma_lock);
	local_irq_restore(flags);
}
static inline struct zb_dma_used_mem* zb_get_used_node(void)
{
	struct zb_dma_used_mem *z;
	unsigned long flags;

	//mutex_lock(&zb_dma_lock);
	local_irq_save(flags);	
	
	list_for_each_entry(z, &zb_dma_used_node, dma_list) {
		list_del(&z->dma_list);
		//mutex_unlock(&zb_dma_lock);
		local_irq_restore(flags);
		return z;
	}

	//mutex_unlock(&zb_dma_lock);
	local_irq_restore(flags);
	return NULL;
}
static inline struct zb_dma_used_mem* zb_search_used_node(unsigned long phys)
{
	struct zb_dma_used_mem *z;
	unsigned long flags;

	//mutex_lock(&zb_dma_lock);
	local_irq_save(flags);	
	
	list_for_each_entry(z, &zb_dma_used_node, dma_list) {
		if (z->phys == phys) {
			list_del(&z->dma_list);
			//mutex_unlock(&zb_dma_lock);
			local_irq_restore(flags);
			return z;
		}
	}

	//mutex_unlock(&zb_dma_lock);
	local_irq_restore(flags);
	return NULL;
}
static inline void zb_put_used_node(struct zb_dma_used_mem *z)
{
	unsigned long flags;

	//mutex_lock(&zb_dma_lock);
	local_irq_save(flags);	
	
	list_add(&z->dma_list, &zb_dma_used_node);

	//mutex_unlock(&zb_dma_lock);
	local_irq_restore(flags);
}

#if 0
void zb_add_dma_priv_mem(unsigned long phys, unsigned long len)
{
	struct zb_dma_used_mem *imem;
	unsigned int index, hit, i;
	unsigned long flags;

	if (zero_trace)
		return;

	if (phys == 0 || len == 0) {
		printk("invalid arg phys 0x%lx len 0x%lx\n", phys, len);
		return;
	}

	if (len < PAGE_SIZE) {
//		printk("resize len 0x%lx\n", len);
		len = PAGE_SIZE;
		return;
	}

	if (phys & ~PAGE_MASK) {
//		printk("align phys 0x%lx->", phys);
		phys &= PAGE_MASK;
//		printk("0x%lx\n", phys);
		return;
	}

//	mutex_lock(&zb_dma_lock);
	local_irq_save(flags);	

	imem = &dma_used_mem[0];
	for(i = 0, hit = 0, index = 0; i < ZB_ION_COUNT; i++, imem++) {
		// 처음 찾은 경우
		if (imem->phys == 0 && hit == 0) {
			hit = 1;
			index = i;
		}

		// 이미 있는 경우
		if (imem->phys == phys) {
			if (index)
				printk("zb_add_dma_priv_mem duplicated request phys 0x%lx modify index 0x%x -> 0x%x\n", phys, index, i);

			if (imem->len != len) {
				printk("zb_add_dma_priv_mem duplicated and different len request len 0x%lx, exist len 0x%lx\n", len, imem->len);
				//panic(" zb_add_dma_priv_mem has a BUG");
				if (len < imem->len)
					len = imem->len;
			}

			if (index == 0)
				hit = 1;
			else {
				hit++;
			}

			index = i;
		}
	}

	if (hit > 1) {
		printk("Too many hit count %d, may cause BUG!!!\n", hit);
		panic("search BUG");
	}
	if (hit == 0) {
		printk("zb_add_dma_priv_mem fail to search phys 0x%lx\n", phys);
		panic("you need more memory");
		//mutex_unlock(&zb_dma_lock);
		local_irq_restore(flags);
		return;
	}
	
	imem = &dma_used_mem[index];
	imem->phys = phys;
	imem->len = len;
	zb_dma_count++;

	//mutex_unlock(&zb_dma_lock);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(zb_add_dma_priv_mem);

void zb_remove_dma_priv_mem(unsigned long phys)
{
	struct zb_dma_used_mem *imem;
	unsigned int hit, i;
	unsigned long flags;

	if (zero_trace)
		return;

	if (phys == 0) {
		printk("invalid arg phys 0x%lx\n", phys);
		return;
	}

	if (phys & ~PAGE_MASK) {
//		printk("align phys 0x%lx->", phys);
		phys &= PAGE_MASK;
//		printk("0x%lx\n", phys);
		return;
	}

//	mutex_lock(&zb_dma_lock);
	local_irq_save(flags);	

	imem = &dma_used_mem[0];
	for(i = 0, hit = 0; i < ZB_ION_COUNT; i++, imem++) {
		if (imem->phys == phys) {
			if (!hit) {
				printk("duplicate phys 0x%lx len 0x%lx found at index 0x%x\n", imem->phys, imem->len, i);
			}
			hit++;
			imem->phys = 0;
			imem->len = 0;
			zb_dma_count--;
		}
	}

//	mutex_unlock(&zb_dma_lock);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(zb_remove_dma_priv_mem);

#else
void zb_add_dma_priv_mem(unsigned long phys, unsigned long len)
{
	unsigned long base, end;
	unsigned long flags;

	if (zero_trace)
		return;

	if (phys == 0 || len == 0) {
		printk("invalid arg phys 0x%lx len 0x%lx\n", phys, len);
		return;
	}

	if (len < PAGE_SIZE)
		len = PAGE_SIZE;

	if (phys & ~PAGE_MASK)
		phys &= PAGE_MASK;

	local_irq_save(flags);	

	base = phys; 
	end = phys + len;
	for (; base < end; base += PAGE_SIZE) {
		zb_dma_priv_set_bit(base);
	}

{
	struct zb_dma_used_mem *z;

	z = zb_get_free_node();
	if (z) {
		z->phys = phys;
		z->len = len;
//		if (len != 0x1000) {
	//		printk("%x+%x\n", phys, len);
//			dump_stack();
//		}
//		else
//			printk("%x+\n", phys);
		zb_put_free_node(z);
	} else {
		panic("bug no more free node");
	}
}

	local_irq_restore(flags);
}
EXPORT_SYMBOL(zb_add_dma_priv_mem);

void zb_remove_dma_priv_mem(unsigned long phys)
{
	unsigned long base, end, len;
	unsigned long flags;

	if (zero_trace)
		return;

	// FIXME needs len
	// to avoid error
	len = PAGE_SIZE;
	if (phys == 0 || len == 0) {
		printk("invalid arg phys 0x%lx len 0x%lx\n", phys, len);
		return;
	}

	if (len < PAGE_SIZE)
		len = PAGE_SIZE;

	if (phys & ~PAGE_MASK)
		phys &= PAGE_MASK;

	local_irq_save(flags);	

	base = phys;
	end = phys + len;
	for (; base < end; base += PAGE_SIZE) {
		zb_dma_priv_clr_bit(base);
	}

{
	struct zb_dma_used_mem *z;

	z = zb_search_used_node(phys);
	if (z) {
		z->phys = phys;
		z->len = len;
		if (len != 0x1000)
			printk("%x-%x\n", phys, len);
		else
			printk("%x-\n", phys);
		zb_put_free_node(z);
	} else {
		printk("bug list search phys 0x%lx\n", phys);
		dump_stack();
	}
}

	local_irq_restore(flags);
}
EXPORT_SYMBOL(zb_remove_dma_priv_mem);
#endif

unsigned long zb_get_dma_mem(void)
{
	return (unsigned long)dma_used_mem;
}
EXPORT_SYMBOL(zb_get_dma_mem);
#else
void zb_add_dma_priv_mem()	{};
void zb_remove_dma_priv_mem()	{};
#endif

/**
 * nxp ion contig heap ops
 */
static int ion_nxp_contig_heap_allocate(struct ion_heap *heap,
                    struct ion_buffer *buffer,
                    unsigned long len,
                    unsigned long align,
                    unsigned long flags)
{
    char *type = "ion-nxp"; /* CMA type */

    buffer->priv_phys = cma_alloc(s_nxp_ion_dev, type, len, align);
    if (IS_ERR_VALUE(buffer->priv_phys)) {
        pr_err("%s error: %d\n", __func__, (int)buffer->priv_phys);
        return (int)buffer->priv_phys;
    }
	zb_add_dma_priv_mem(buffer->priv_phys, len);
    buffer->flags = flags;

    return 0;
}

static int ion_nxp_reserve_heap_allocate(struct ion_heap *heap,
                    struct ion_buffer *buffer,
                    unsigned long len,
                    unsigned long align,
                    unsigned long flags)
{
    char *type = "ion-reserve"; /* CMA type */

    buffer->priv_phys = cma_alloc(s_nxp_ion_dev, type, len, align);
    if (IS_ERR_VALUE(buffer->priv_phys)) {
        pr_err("%s error: %d\n", __func__, (int)buffer->priv_phys);
        return (int)buffer->priv_phys;
    }
    buffer->flags = flags;

    return 0;
}


static void ion_nxp_heap_free(struct ion_buffer *buffer)
{
	zb_remove_dma_priv_mem(buffer->priv_phys);
    cma_free(buffer->priv_phys);
}

static int ion_nxp_heap_phys(struct ion_heap *heap,
                    struct ion_buffer *buffer,
                    ion_phys_addr_t *addr, size_t *len)
{
    *addr = buffer->priv_phys;
    *len  = buffer->size;
    return 0;
}

static struct sg_table *ion_nxp_heap_map_dma(struct ion_heap *heap,
                    struct ion_buffer *buffer)
{
    struct sg_table *table;
    int ret;

    table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
    if (!table) {
        pr_err("%s error: fail to kzalloc size(%d)\n", __func__, sizeof(struct sg_table));
        return ERR_PTR(-ENOMEM);
    }
    ret = sg_alloc_table(table, 1, GFP_KERNEL);
    if (ret) {
        pr_err("%s error: fail to sg_alloc_table\n", __func__);
        return ERR_PTR(ret);
    }
    sg_init_one(table->sgl, phys_to_virt(buffer->priv_phys), buffer->size);
    return table;
}

static void ion_nxp_heap_unmap_dma(struct ion_heap *heap,
                    struct ion_buffer *buffer)
{
    if (buffer->sg_table) {
        sg_free_table(buffer->sg_table);
    }
}

static void *ion_nxp_heap_map_kernel(struct ion_heap *heap,
                    struct ion_buffer *buffer)
{
    return phys_to_virt(buffer->priv_phys);
}

static void ion_nxp_heap_unmap_kernel(struct ion_heap *heap,
                    struct ion_buffer *buffer)
{
}

static int ion_nxp_heap_map_user(struct ion_heap *heap,
                    struct ion_buffer *buffer,
                    struct vm_area_struct *vma)
{
    unsigned long pfn = __phys_to_pfn(buffer->priv_phys);
    return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
                    vma->vm_end - vma->vm_start,
                    vma->vm_page_prot);
}

static struct ion_heap_ops contig_heap_ops = {
    .allocate   = ion_nxp_contig_heap_allocate,
    .free       = ion_nxp_heap_free,
    .phys       = ion_nxp_heap_phys,
    .map_dma    = ion_nxp_heap_map_dma,
    .unmap_dma  = ion_nxp_heap_unmap_dma,
    .map_kernel = ion_nxp_heap_map_kernel,
    .unmap_kernel = ion_nxp_heap_unmap_kernel,
    .map_user   = ion_nxp_heap_map_user,
};

static struct ion_heap_ops reserve_heap_ops = {
    .allocate   = ion_nxp_reserve_heap_allocate,
    .free       = ion_nxp_heap_free,
    .phys       = ion_nxp_heap_phys,
    .map_dma    = ion_nxp_heap_map_dma,
    .unmap_dma  = ion_nxp_heap_unmap_dma,
    .map_kernel = ion_nxp_heap_map_kernel,
    .unmap_kernel = ion_nxp_heap_unmap_kernel,
    .map_user   = ion_nxp_heap_map_user,
};

static struct ion_heap *ion_nxp_heap_create(int type)
{
    struct ion_heap *heap;
    heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
    if (!heap) {
        pr_err("%s: fail to kzalloc size(%d)\n", __func__, sizeof(struct ion_heap));
        return ERR_PTR(-ENOMEM);
    }

    switch (type) {
    case ION_HEAP_TYPE_NXP_CONTIG:
        heap->ops  = &contig_heap_ops;
        heap->type = ION_HEAP_TYPE_NXP_CONTIG;
        break;
    case ION_HEAP_TYPE_NXP_RESERVE:
        heap->ops  = &reserve_heap_ops;
        heap->type = ION_HEAP_TYPE_NXP_RESERVE;
        break;
    default:
        printk("%s: invalid type 0x%x\n", __func__, type);
        kfree(heap);
        return NULL;
    }

    return heap;
}

static void ion_nxp_heap_destroy(struct ion_heap *heap)
{
    kfree(heap);
}

/**
 * local helper functions
 * start _
 */
static struct ion_heap *_ion_heap_create(struct ion_platform_heap *heap_data)
{
    struct ion_heap *heap = NULL;
    int heap_type = heap_data->type;

    /*
     * current supported custom heap type
     *   - ION_HEAP_TYPE_NXP_CONTIG
     */
    /*switch (heap_data->type) {*/
    switch (heap_type) {
    case ION_HEAP_TYPE_NXP_CONTIG:
    case ION_HEAP_TYPE_NXP_RESERVE:
        heap = ion_nxp_heap_create(heap_data->type);
        break;
    default:
        return ion_heap_create(heap_data);
    }

    if (IS_ERR_OR_NULL(heap)) {
        pr_err("%s: error creating heap %s type %d base %lu size %u\n",
                __func__, heap_data->name, heap_data->type,
                heap_data->base, heap_data->size);
        return ERR_PTR(-EINVAL);
    }

    heap->name = heap_data->name;
    heap->id   = heap_data->id;

    return heap;
}

static void _ion_heap_destroy(struct ion_heap *heap)
{
    int heap_type;

    if (!heap)
        return;

    heap_type = heap->type;

    switch (heap_type) {
    case ION_HEAP_TYPE_NXP_CONTIG:
    case ION_HEAP_TYPE_NXP_RESERVE:
        ion_nxp_heap_destroy(heap);
        break;
    default:
        ion_heap_destroy(heap);
    }
}

static int ion_sync_from_device(struct ion_client *client, int fd)
{
    struct dma_buf *dmabuf;
    struct ion_buffer *buffer;

    /*printk("%s: fd %d\n", __func__, fd);*/
    dmabuf = dma_buf_get(fd);
    if (IS_ERR(dmabuf))
        return PTR_ERR(dmabuf);

    buffer = dmabuf->priv;

    dma_sync_sg_for_cpu(NULL, buffer->sg_table->sgl, buffer->sg_table->nents, DMA_FROM_DEVICE);
    dma_buf_put(dmabuf);

    return 0;
}
/* custom ioctl */
static long nxp_ion_custom_ioctl(struct ion_client *client,
        unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    switch (cmd) {
    case NXP_ION_GET_PHY_ADDR:
        {
            struct nxp_ion_physical data;
            struct dma_buf *dmabuf;
            struct ion_buffer *buffer;
            if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
                pr_err("%s error: failed to copy_from_user()\n", __func__);
                return -EFAULT;
            }
            pr_debug("%s: request buf fd(%d)\n", __func__, data.ion_buffer_fd);
            dmabuf = dma_buf_get(data.ion_buffer_fd);
            if (IS_ERR_OR_NULL(dmabuf)) {
                pr_err("%s: can't get dmabuf\n", __func__);
                return -EINVAL;
            }
            buffer = dmabuf->priv;
            data.phys = (unsigned long)buffer->priv_phys;
            if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
                pr_err("%s error: failed to copy_to_user()\n", __func__);
                return -EFAULT;
            }
            dma_buf_put(dmabuf);
        }
        break;
    case NXP_ION_SYNC_FROM_DEVICE:
        {
            struct ion_fd_data data;
            if (copy_from_user(&data, (void __user *)arg, sizeof(struct ion_fd_data)))
                return -EFAULT;
            ion_sync_from_device(client, data.fd);
            break;
        }
    default:
        return -EINVAL;
    }

    return ret;
}

/**
 * platform driver
 */
static int nxp_ion_probe(struct platform_device *pdev)
{
    struct ion_platform_data *pdata = pdev->dev.platform_data;
    int error;
    int i;
    struct ion_device *ion_dev;
    struct ion_heap **heaps;

    if (pdata->nr <= 0) {
        pr_err("%s error: invalid platform_data, nr(%d)\n", __func__, pdata->nr);
        return -EINVAL;
    }

    ion_dev = ion_device_create(nxp_ion_custom_ioctl);
    if (IS_ERR_OR_NULL(ion_dev)) {
        pr_err("%s error: fail to ion_device_create\n", __func__);
        return -EINVAL;
    }

    heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);
    if (!heaps) {
        pr_err("%s error: fail to kzalloc struct ion_heap(size: %d)\n",
                __func__, sizeof(struct ion_heap *) * pdata->nr);
        ion_device_destroy(ion_dev);
        return -ENOMEM;
    }

    for (i = 0; i < pdata->nr; i++) {
        struct ion_platform_heap *heap_data = &pdata->heaps[i];

        heaps[i] = _ion_heap_create(heap_data);
        if (IS_ERR_OR_NULL(heaps[i])) {
            error = PTR_ERR(heaps[i]);
            pr_err("%s error: fail to _ion_heap_create(error: %d)\n", __func__, error);
            goto err;
        }
        ion_device_add_heap(ion_dev, heaps[i]);
    }

    s_num_heaps     = pdata->nr;
    s_heaps         = heaps;
    g_ion_nxp       = ion_dev;
    s_nxp_ion_dev   = &pdev->dev;

    platform_set_drvdata(pdev, g_ion_nxp);

#ifdef CONFIG_FALINUX_ZEROBOOT
	mutex_init(&zb_dma_lock);
	zb_dma_alloc();	


{
	struct zb_dma_used_mem *cur, *n;
	int i;

	INIT_LIST_HEAD(&zb_dma_used_node);
	INIT_LIST_HEAD(&zb_dma_free_node);

	zb_dma_mem = zb_dma_node_alloc();
	if (zb_dma_mem) {
		for (i = 0; i < ZB_DMA_MEM_NODE_COUNT; i++) { 
			list_add_tail(&zb_dma_mem[i].dma_list, &zb_dma_free_node);
		}
		printk("======== zb dma node %d added\n", ZB_DMA_MEM_NODE_COUNT);
		printk("======== zb dma node %d added\n", ZB_DMA_MEM_NODE_COUNT);
		printk("======== zb dma node %d added\n", ZB_DMA_MEM_NODE_COUNT);
		printk("======== zb dma node %d added\n", ZB_DMA_MEM_NODE_COUNT);
		printk("======== zb dma node %d added\n", ZB_DMA_MEM_NODE_COUNT);

//		list_for_each_entry_safe(cur, n, &zb_dma_free_node, dma_list) {
//			list_del(&cur->dma_list);
//		}
	} else {
		printk(" memory alloc fail ===========\n");
		printk(" memory alloc fail ===========\n");
		printk(" memory alloc fail ===========\n");
		printk(" memory alloc fail ===========\n");
		printk(" memory alloc fail ===========\n");
		BUG();
	}
}
#endif

    printk("%s success!!!\n", __func__);
    return 0;

err:
    for (i = 0; i < pdata->nr; i++) {
        if (heaps[i]) {
            ion_heap_destroy(heaps[i]);
        }
    }
    kfree(heaps);
    ion_device_destroy(ion_dev);
    return error;
}

static int nxp_ion_remove(struct platform_device *pdev)
{
    struct ion_device *idev = platform_get_drvdata(pdev);
    int i;

    ion_device_destroy(idev);
    for (i = 0; i < s_num_heaps; i++)
        _ion_heap_destroy(s_heaps[i]);
    kfree(s_heaps);
    return 0;
}

static struct platform_driver ion_driver = {
    .probe  = nxp_ion_probe,
    .remove = nxp_ion_remove,
    .driver = {
        .name = "ion-nxp"
    }
};

static int __init ion_init(void)
{
    return platform_driver_register(&ion_driver);
}

subsys_initcall(ion_init);

MODULE_AUTHOR("swpark <swpark@nexell.co.kr>");
MODULE_DESCRIPTION("ION Platform Driver for Nexell");
MODULE_LICENSE("GPL");
