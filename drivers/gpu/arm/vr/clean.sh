#!/bin/bash

#defalut value
#BUILD=debug
BUILD=release

#===================================
# mali environment config for user
#===================================
KDIR=../../../../../../kernel/
CROSS_COMPILE=arm-eabi-

USING_UMP=0
USING_PROFILING=0
VR_SHARED_INTERRUPTS=1
VR_NXP4330=1
VR_NXP5430=0

#===================================
# mali device driver build
#===================================
make clean KDIR=$KDIR BUILD=$BUILD \
	USING_UMP=$USING_UMP USING_PROFILING=$USING_PROFILING VR_SHARED_INTERRUPTS=$VR_SHARED_INTERRUPTS VR_NXP4330=$VR_NXP4330 VR_NXP5430=$VR_NXP5430
