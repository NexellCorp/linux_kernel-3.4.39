/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_utgard.h
 * Defines types and interface exposed by the VR Utgard device driver
 */

#ifndef __VR_UTGARD_H__
#define __VR_UTGARD_H__

#define VR_GPU_NAME_UTGARD "vr-utgard"

/* VR-200 */

#define VR_GPU_RESOURCES_VR200(base_addr, gp_irq, pp_irq, mmu_irq) \
	VR_GPU_RESOURCE_PP(base_addr + 0x0000, pp_irq) \
	VR_GPU_RESOURCE_GP(base_addr + 0x2000, gp_irq) \
	VR_GPU_RESOURCE_MMU(base_addr + 0x3000, mmu_irq)

/* VR-300 */

#define VR_GPU_RESOURCES_VR300(base_addr, gp_irq, gp_mmu_irq, pp_irq, pp_mmu_irq) \
	VR_GPU_RESOURCES_VR_MP1(base_addr, gp_irq, gp_mmu_irq, pp_irq, pp_mmu_irq)

#define VR_GPU_RESOURCES_VR300_PMU(base_addr, gp_irq, gp_mmu_irq, pp_irq, pp_mmu_irq) \
	VR_GPU_RESOURCES_VR_MP1_PMU(base_addr, gp_irq, gp_mmu_irq, pp_irq, pp_mmu_irq)

/* VR-400 */

#define VR_GPU_RESOURCES_VR_MP1(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x1000) \
	VR_GPU_RESOURCE_GP_WITH_MMU(base_addr + 0x0000, gp_irq, base_addr + 0x3000, gp_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(0, base_addr + 0x8000, pp0_irq, base_addr + 0x4000, pp0_mmu_irq)

#define VR_GPU_RESOURCES_VR_MP1_PMU(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq) \
	VR_GPU_RESOURCES_VR_MP1(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq) \
	VR_GPU_RESOURCE_PMU(base_addr + 0x2000)

#define VR_GPU_RESOURCES_VR_MP2(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x1000) \
	VR_GPU_RESOURCE_GP_WITH_MMU(base_addr + 0x0000, gp_irq, base_addr + 0x3000, gp_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(0, base_addr + 0x8000, pp0_irq, base_addr + 0x4000, pp0_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(1, base_addr + 0xA000, pp1_irq, base_addr + 0x5000, pp1_mmu_irq)

#define VR_GPU_RESOURCES_VR_MP2_PMU(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq) \
	VR_GPU_RESOURCES_VR_MP2(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq) \
	VR_GPU_RESOURCE_PMU(base_addr + 0x2000)

#define VR_GPU_RESOURCES_VR_MP3(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x1000) \
	VR_GPU_RESOURCE_GP_WITH_MMU(base_addr + 0x0000, gp_irq, base_addr + 0x3000, gp_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(0, base_addr + 0x8000, pp0_irq, base_addr + 0x4000, pp0_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(1, base_addr + 0xA000, pp1_irq, base_addr + 0x5000, pp1_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(2, base_addr + 0xC000, pp2_irq, base_addr + 0x6000, pp2_mmu_irq)

#define VR_GPU_RESOURCES_VR_MP3_PMU(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq) \
	VR_GPU_RESOURCES_VR_MP3(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq) \
	VR_GPU_RESOURCE_PMU(base_addr + 0x2000)

#define VR_GPU_RESOURCES_VR_MP4(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x1000) \
	VR_GPU_RESOURCE_GP_WITH_MMU(base_addr + 0x0000, gp_irq, base_addr + 0x3000, gp_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(0, base_addr + 0x8000, pp0_irq, base_addr + 0x4000, pp0_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(1, base_addr + 0xA000, pp1_irq, base_addr + 0x5000, pp1_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(2, base_addr + 0xC000, pp2_irq, base_addr + 0x6000, pp2_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(3, base_addr + 0xE000, pp3_irq, base_addr + 0x7000, pp3_mmu_irq)

#define VR_GPU_RESOURCES_VR_MP4_PMU(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq) \
	VR_GPU_RESOURCES_VR_MP4(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq) \
	VR_GPU_RESOURCE_PMU(base_addr + 0x2000)

/* VR-450 */
#define VR_GPU_RESOURCES_VR450_MP2(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x10000) \
	VR_GPU_RESOURCE_GP_WITH_MMU(base_addr + 0x00000, gp_irq, base_addr + 0x03000, gp_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x01000) \
	VR_GPU_RESOURCE_PP_WITH_MMU(0, base_addr + 0x08000, pp0_irq, base_addr + 0x04000, pp0_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(1, base_addr + 0x0A000, pp1_irq, base_addr + 0x05000, pp1_mmu_irq) \
	VR_GPU_RESOURCE_BCAST(base_addr + 0x13000) \
	VR_GPU_RESOURCE_DLBU(base_addr + 0x14000) \
	VR_GPU_RESOURCE_PP_BCAST(base_addr + 0x16000, pp_bcast_irq) \
	VR_GPU_RESOURCE_PP_MMU_BCAST(base_addr + 0x15000)

#define VR_GPU_RESOURCES_VR450_MP2_PMU(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCES_VR450_MP2(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCE_PMU(base_addr + 0x2000) \

#define VR_GPU_RESOURCES_VR450_MP4(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x10000) \
	VR_GPU_RESOURCE_GP_WITH_MMU(base_addr + 0x00000, gp_irq, base_addr + 0x03000, gp_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x01000) \
	VR_GPU_RESOURCE_PP_WITH_MMU(0, base_addr + 0x08000, pp0_irq, base_addr + 0x04000, pp0_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(1, base_addr + 0x0A000, pp1_irq, base_addr + 0x05000, pp1_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(2, base_addr + 0x0C000, pp2_irq, base_addr + 0x06000, pp2_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(3, base_addr + 0x0E000, pp3_irq, base_addr + 0x07000, pp3_mmu_irq) \
	VR_GPU_RESOURCE_BCAST(base_addr + 0x13000) \
	VR_GPU_RESOURCE_DLBU(base_addr + 0x14000) \
	VR_GPU_RESOURCE_PP_BCAST(base_addr + 0x16000, pp_bcast_irq) \
	VR_GPU_RESOURCE_PP_MMU_BCAST(base_addr + 0x15000)

#define VR_GPU_RESOURCES_VR450_MP4_PMU(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCES_VR450_MP4(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCE_PMU(base_addr + 0x2000) \

#define VR_GPU_RESOURCES_VR450_MP6(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp4_irq, pp4_mmu_irq, pp5_irq, pp5_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x10000) \
	VR_GPU_RESOURCE_GP_WITH_MMU(base_addr + 0x00000, gp_irq, base_addr + 0x03000, gp_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x01000) \
	VR_GPU_RESOURCE_PP_WITH_MMU(0, base_addr + 0x08000, pp0_irq, base_addr + 0x04000, pp0_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(1, base_addr + 0x0A000, pp1_irq, base_addr + 0x05000, pp1_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(2, base_addr + 0x0C000, pp2_irq, base_addr + 0x06000, pp2_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x11000) \
	VR_GPU_RESOURCE_PP_WITH_MMU(3, base_addr + 0x28000, pp3_irq, base_addr + 0x1C000, pp3_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(4, base_addr + 0x2A000, pp4_irq, base_addr + 0x1D000, pp4_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(5, base_addr + 0x2C000, pp5_irq, base_addr + 0x1E000, pp5_mmu_irq) \
	VR_GPU_RESOURCE_BCAST(base_addr + 0x13000) \
	VR_GPU_RESOURCE_DLBU(base_addr + 0x14000) \
	VR_GPU_RESOURCE_PP_BCAST(base_addr + 0x16000, pp_bcast_irq) \
	VR_GPU_RESOURCE_PP_MMU_BCAST(base_addr + 0x15000)

#define VR_GPU_RESOURCES_VR450_MP6_PMU(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp4_irq, pp4_mmu_irq, pp5_irq, pp5_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCES_VR450_MP6(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp4_irq, pp4_mmu_irq, pp5_irq, pp5_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCE_PMU(base_addr + 0x2000) \

#define VR_GPU_RESOURCES_VR450_MP8(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp4_irq, pp4_mmu_irq, pp5_irq, pp5_mmu_irq, pp6_irq, pp6_mmu_irq, pp7_irq, pp7_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x10000) \
	VR_GPU_RESOURCE_GP_WITH_MMU(base_addr + 0x00000, gp_irq, base_addr + 0x03000, gp_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x01000) \
	VR_GPU_RESOURCE_PP_WITH_MMU(0, base_addr + 0x08000, pp0_irq, base_addr + 0x04000, pp0_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(1, base_addr + 0x0A000, pp1_irq, base_addr + 0x05000, pp1_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(2, base_addr + 0x0C000, pp2_irq, base_addr + 0x06000, pp2_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(3, base_addr + 0x0E000, pp3_irq, base_addr + 0x07000, pp3_mmu_irq) \
	VR_GPU_RESOURCE_L2(base_addr + 0x11000) \
	VR_GPU_RESOURCE_PP_WITH_MMU(4, base_addr + 0x28000, pp4_irq, base_addr + 0x1C000, pp4_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(5, base_addr + 0x2A000, pp5_irq, base_addr + 0x1D000, pp5_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(6, base_addr + 0x2C000, pp6_irq, base_addr + 0x1E000, pp6_mmu_irq) \
	VR_GPU_RESOURCE_PP_WITH_MMU(7, base_addr + 0x2E000, pp7_irq, base_addr + 0x1F000, pp7_mmu_irq) \
	VR_GPU_RESOURCE_BCAST(base_addr + 0x13000) \
	VR_GPU_RESOURCE_DLBU(base_addr + 0x14000) \
	VR_GPU_RESOURCE_PP_BCAST(base_addr + 0x16000, pp_bcast_irq) \
	VR_GPU_RESOURCE_PP_MMU_BCAST(base_addr + 0x15000)

#define VR_GPU_RESOURCES_VR450_MP8_PMU(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp4_irq, pp4_mmu_irq, pp5_irq, pp5_mmu_irq, pp6_irq, pp6_mmu_irq, pp7_irq, pp7_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCES_VR450_MP8(base_addr, gp_irq, gp_mmu_irq, pp0_irq, pp0_mmu_irq, pp1_irq, pp1_mmu_irq, pp2_irq, pp2_mmu_irq, pp3_irq, pp3_mmu_irq, pp4_irq, pp4_mmu_irq, pp5_irq, pp5_mmu_irq, pp6_irq, pp6_mmu_irq, pp7_irq, pp7_mmu_irq, pp_bcast_irq) \
	VR_GPU_RESOURCE_PMU(base_addr + 0x2000) \

#define VR_GPU_RESOURCE_L2(addr) \
	{ \
		.name = "VR_L2", \
		.flags = IORESOURCE_MEM, \
		.start = addr, \
		.end   = addr + 0x200, \
	},

#define VR_GPU_RESOURCE_GP(gp_addr, gp_irq) \
	{ \
		.name = "VR_GP", \
		.flags = IORESOURCE_MEM, \
		.start = gp_addr, \
		.end =   gp_addr + 0x100, \
	}, \
	{ \
		.name = "VR_GP_IRQ", \
		.flags = IORESOURCE_IRQ, \
		.start = gp_irq, \
		.end   = gp_irq, \
	}, \

#define VR_GPU_RESOURCE_GP_WITH_MMU(gp_addr, gp_irq, gp_mmu_addr, gp_mmu_irq) \
	{ \
		.name = "VR_GP", \
		.flags = IORESOURCE_MEM, \
		.start = gp_addr, \
		.end =   gp_addr + 0x100, \
	}, \
	{ \
		.name = "VR_GP_IRQ", \
		.flags = IORESOURCE_IRQ, \
		.start = gp_irq, \
		.end   = gp_irq, \
	}, \
	{ \
		.name = "VR_GP_MMU", \
		.flags = IORESOURCE_MEM, \
		.start = gp_mmu_addr, \
		.end =   gp_mmu_addr + 0x100, \
	}, \
	{ \
		.name = "VR_GP_MMU_IRQ", \
		.flags = IORESOURCE_IRQ, \
		.start = gp_mmu_irq, \
		.end =   gp_mmu_irq, \
	},

#define VR_GPU_RESOURCE_PP(pp_addr, pp_irq) \
	{ \
		.name = "VR_PP", \
		.flags = IORESOURCE_MEM, \
		.start = pp_addr, \
		.end =   pp_addr + 0x1100, \
	}, \
	{ \
		.name = "VR_PP_IRQ", \
		.flags = IORESOURCE_IRQ, \
		.start = pp_irq, \
		.end =   pp_irq, \
	}, \

#define VR_GPU_RESOURCE_PP_WITH_MMU(id, pp_addr, pp_irq, pp_mmu_addr, pp_mmu_irq) \
	{ \
		.name = "VR_PP" #id, \
		.flags = IORESOURCE_MEM, \
		.start = pp_addr, \
		.end =   pp_addr + 0x1100, \
	}, \
	{ \
		.name = "VR_PP" #id "_IRQ", \
		.flags = IORESOURCE_IRQ, \
		.start = pp_irq, \
		.end =   pp_irq, \
	}, \
	{ \
		.name = "VR_PP" #id "_MMU", \
		.flags = IORESOURCE_MEM, \
		.start = pp_mmu_addr, \
		.end =   pp_mmu_addr + 0x100, \
	}, \
	{ \
		.name = "VR_PP" #id "_MMU_IRQ", \
		.flags = IORESOURCE_IRQ, \
		.start = pp_mmu_irq, \
		.end =   pp_mmu_irq, \
	},

#define VR_GPU_RESOURCE_MMU(mmu_addr, mmu_irq) \
	{ \
		.name = "VR_MMU", \
		.flags = IORESOURCE_MEM, \
		.start = mmu_addr, \
		.end =   mmu_addr + 0x100, \
	}, \
	{ \
		.name = "VR_MMU_IRQ", \
		.flags = IORESOURCE_IRQ, \
		.start = mmu_irq, \
		.end =   mmu_irq, \
	},

#define VR_GPU_RESOURCE_PMU(pmu_addr) \
	{ \
		.name = "VR_PMU", \
		.flags = IORESOURCE_MEM, \
		.start = pmu_addr, \
		.end =   pmu_addr + 0x100, \
	},

#define VR_GPU_RESOURCE_DLBU(dlbu_addr) \
	{ \
		.name = "VR_DLBU", \
		.flags = IORESOURCE_MEM, \
		.start = dlbu_addr, \
		.end = dlbu_addr + 0x100, \
	},

#define VR_GPU_RESOURCE_BCAST(bcast_addr) \
	{ \
		.name = "VR_Broadcast", \
		.flags = IORESOURCE_MEM, \
		.start = bcast_addr, \
		.end = bcast_addr + 0x100, \
	},

#define VR_GPU_RESOURCE_PP_BCAST(pp_addr, pp_irq) \
	{ \
		.name = "VR_PP_Broadcast", \
		.flags = IORESOURCE_MEM, \
		.start = pp_addr, \
		.end =   pp_addr + 0x1100, \
	}, \
	{ \
		.name = "VR_PP_Broadcast_IRQ", \
		.flags = IORESOURCE_IRQ, \
		.start = pp_irq, \
		.end =   pp_irq, \
	}, \

#define VR_GPU_RESOURCE_PP_MMU_BCAST(pp_mmu_bcast_addr) \
	{ \
		.name = "VR_PP_MMU_Broadcast", \
		.flags = IORESOURCE_MEM, \
		.start = pp_mmu_bcast_addr, \
		.end = pp_mmu_bcast_addr + 0x100, \
	},

struct vr_gpu_device_data
{
	/* Dedicated GPU memory range (physical). */
	unsigned long dedicated_mem_start;
	unsigned long dedicated_mem_size;

	/* Shared GPU memory */
	unsigned long shared_mem_size;

	/* Frame buffer memory to be accessible by VR GPU (physical) */
    /* nexell block */
#if 0
	unsigned long fb_start;
	unsigned long fb_size;
#endif

	/* Report GPU utilization in this interval (specified in ms) */
	unsigned long utilization_interval;

	/* Function that will receive periodic GPU utilization numbers */
	void (*utilization_handler)(unsigned int);
};

/** @brief VR GPU power down using VR in-built PMU
 *
 * called to power down all cores
 */
int vr_pmu_powerdown(void);


/** @brief VR GPU power up using VR in-built PMU
 *
 * called to power up all cores
 */
int vr_pmu_powerup(void);


#endif
