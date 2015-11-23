obj-y = hdmi-priv.o hdmi-preset.o nxp-edid.o nxp-hdcp.o nxp-hdmiphy.o nxp-hdmi-context.o

EXTRA_CFLAGS += -Idrivers/media/video/nexell

MACH = 
ifeq ($(CONFIG_ARCH_S5P4418),"y")
	MACH = mach-s5p4418
else
	MACH = mach-s5p6818
endif

EXTRA_CFLAGS += -I$(srctree)/arch/arm/$(MACH)/prototype/base -I$(srctree)/arch/arm/$(MACH)/prototype/module
