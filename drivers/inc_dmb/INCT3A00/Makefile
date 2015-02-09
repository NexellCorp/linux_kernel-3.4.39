#
# Makefile for kernel SPI drivers.
#
BOARD_CONFIG_S5P6818=y

ifeq ($(CONFIG_SPI_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif

# small core, mostly translating board-specific
# config declarations into driver model code
obj-y		+= tdmb.o
obj-y		+= INC_INTERFACE.o INC_PROCESS.o INC_FICDEC.o INC_RF_CTRL500.o INC_QFIFO.o INC_BUFF_CTRL.o INC_EWSDEC.o

ifeq ($(BOARD_CONFIG_S5P6818),y)
obj-y		+= INC_SPI_S5P6818.o
endif

clean:
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -rf *.bak
	rm -rf modules.order
	rm -rf Module.symvers
	rm -rf .tmp_version*