#obj-$(CONFIG_SLSIAP_CODA960) += nx_vpu.o
obj-$(CONFIG_SLSIAP_CODA960) += nx_vpu.o

nx_vpu-objs := linux/linux_vpu_drv.o
nx_vpu-objs += osapi/drv_osapi_linux.o
nx_vpu-objs += src/vpu_hw_interface.o
nx_vpu-objs += src/nx_jpu_api.o
nx_vpu-objs += src/nx_vpu_api.o
nx_vpu-objs += src/nx_vpu_gdi.o

EXTRA_CFLAGS += -Idrivers/misc/coda960/include \
				-Idrivers/misc/coda960/firmware

