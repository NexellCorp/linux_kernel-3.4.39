#
# Copyright 2009 Nexell Co.
#
# Makefile for the Nexell platform board

ifeq ($(CONFIG_PLAT_NXP5430_VTK),y)
PLATBOARD :=vtk
endif

ifeq ($(CONFIG_PLAT_NXP5430_DRONE),y)
PLATBOARD :=drone
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



