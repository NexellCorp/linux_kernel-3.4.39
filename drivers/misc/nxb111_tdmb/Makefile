##########################################
# Makefile for TDMB Device Drivers.
##########################################

#ifdef CONFIG_MTV_NXB111_TDMB
EXTRA_CFLAGS += -I. -I./src
ccflags-y += -Idrivers/misc/nxb111_tdmb
ccflags-y += -Idrivers/misc/nxb111_tdmb/mtv319
#endif

obj-y += tdmb.o \
		tdmb_isr.o \
		tdmb_spi.o \
		tdmb_tsp_buf.o \
		./mtv319/mtv319.o \
		./mtv319/mtv319_tdmb.o \
		./mtv319/mtv319_rf.o \
		./mtv319/mtv319_cifdec.o \
		./mtv319/mtv319_port.o


