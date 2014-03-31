#
# Copyright 2009 Nexell Co.
#
# Makefile for the Nexell platform board

ifeq ($(CONFIG_PLAT_NXP4330_LYNX),y)
PLATBOARD :=lynx
endif

ifeq ($(CONFIG_PLAT_NXP4330_VTK),y)
PLATBOARD :=vtk
endif

ifeq ($(CONFIG_PLAT_NXP4330_SMARTBOX),y)
PLATBOARD :=smartbox
endif

ifeq ($(CONFIG_PLAT_NXP4330_PYXIS),y)
PLATBOARD :=pyxis
endif

ifeq ($(CONFIG_PLAT_NXP4330_MORPHEUS),y)
PLATBOARD :=morpheus
endif

ifeq ($(CONFIG_PLAT_NXP4330_SECRET),y)
PLATBOARD :=secret
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



