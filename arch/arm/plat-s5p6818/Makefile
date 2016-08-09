#
# Copyright 2009 Nexell Co.
#
# Makefile for the Nexell platform board

ifeq ($(CONFIG_PLAT_S5P6818_SVT),y)
PLATBOARD :=svt
endif

ifeq ($(CONFIG_PLAT_S5P6818_AVN_REF),y)
PLATBOARD :=avn_ref
endif

ifeq ($(CONFIG_PLAT_S5P6818_AVN_REF_BT),y)
PLATBOARD :=avn_ref_bt
endif

#
# Build options
#

obj-y	:= $(PLATBOARD)/

KBUILD_AFLAGS 	+= -I$(srctree)/$(platdirs)common 	\
				   -I$(srctree)/$(platdirs)$(PLATBOARD)/include
KBUILD_CFLAGS 	+= -I$(srctree)/$(platdirs)common	\
				   -I$(srctree)/$(platdirs)$(PLATBOARD)/include
KBUILD_CPPFLAGS += -I$(srctree)/$(platdirs)common	\
				   -I$(srctree)/$(platdirs)$(PLATBOARD)/include



