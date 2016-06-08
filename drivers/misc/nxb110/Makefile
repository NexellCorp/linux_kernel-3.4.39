#
# Kernel makefile for TDMB&1SEG(NXB110)
#

ifdef CONFIG_MTV_NXB110
EXTRA_CFLAGS += -I. -I./src 
endif

obj-$(CONFIG_MTV_NXB110) += mtv.o \
			mtv_i2c.o \
			mtv_spi.o \
			mtv_isr.o \
			./src/nxb110tv.o \
			./src/nxb110tv_port.o \
			./src/nxb110tv_rf.o \
			./src/nxb110tv_tdmb.o

