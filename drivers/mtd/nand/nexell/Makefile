#
# linux/drivers/nand/nexell/Makefile
#
obj-$(CONFIG_MTD_NAND_NEXELL)	+= nand.o

ifeq ($(CONFIG_MTD_NAND_MANUAL_ELP_PATH),y)
obj-$(CONFIG_MTD_NAND_ECC_HW)	+= nand_ecc_elp.o
else
obj-$(CONFIG_MTD_NAND_ECC_HW)	+= nand_ecc.o
endif
