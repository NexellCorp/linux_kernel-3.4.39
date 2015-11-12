#
# Copyright 2009 Nexell Co.
#
# Makefile for the Nexell platform board

ifeq ($(CONFIG_PLAT_S5P4418_SVT),y)
PLATBOARD :=svt
endif

ifeq ($(CONFIG_PLAT_S5P4418_MSD8000B),y)
PLATBOARD :=msd8000b
endif

ifeq ($(CONFIG_PLAT_S5P4418_AVN_REF),y)
PLATBOARD :=avn_ref
endif

ifeq ($(CONFIG_PLAT_S5P4418_TK_AVN),y)
PLATBOARD :=tk_avn
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



