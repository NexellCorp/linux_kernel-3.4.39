#
# Copyright 2009 Nexell Co.
#
# Makefile for the Nexell platform board

ifeq ($(CONFIG_PLAT_S5P4418_LEPUS_QUICK),y)
PLATBOARD :=lepus_quick
endif

ifeq ($(CONFIG_PLAT_S5P4418_X1DASH),y)
PLATBOARD :=x1dash
endif

#
# Build options
#

obj-y   := $(PLATBOARD)/

KBUILD_AFLAGS   += -I$(srctree)/$(platdirs)common   \
                   -I$(srctree)/$(platdirs)$(PLATBOARD)/include
KBUILD_CFLAGS   += -I$(srctree)/$(platdirs)common   \
                   -I$(srctree)/$(platdirs)$(PLATBOARD)/include
KBUILD_CPPFLAGS += -I$(srctree)/$(platdirs)common   \
                   -I$(srctree)/$(platdirs)$(PLATBOARD)/include


