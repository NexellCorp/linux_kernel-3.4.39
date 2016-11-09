LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE	:= testmtv
LOCAL_C_INCLUDES	:= $(LOCAL_PATH)/../src
LOCAL_C_INCLUDES	+= $(LOCAL_PATH)/..

LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE -pie

LOCAL_SRC_FILES	:= test.c test_freq_tbl.c test_tdmb_dab.c nxb110tv_ficdec.c
LOCAL_LDLIBS	:= -llog

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_EXECUTABLE)
