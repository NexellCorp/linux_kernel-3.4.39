#
# Kernel makefile for TDMB(MTV818)
#

ifdef CONFIG_TDMB_MTV818_AV
EXTRA_CFLAGS += -I. -I./src 
endif

obj-$(CONFIG_TDMB_MTV818_AV) += mtv.o \
							mtv_i2c.o \
							mtv_spi.o \
							mtv_isr.o \
							./src/raontv.o \
							./src/raontv_port.o \
							./src/raontv_rf.o \
							./src/raontv_tdmb.o

