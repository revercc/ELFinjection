LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ELFinjection
LOCAL_SRC_FILES	:= $(LOCAL_PATH)/src/elf_injection.c $(LOCAL_PATH)/src/global_data.c $(LOCAL_PATH)/src/PtraceInjection/ptreace_injection.c $(LOCAL_PATH)/src/RelyInjection/rely_injection.c 
LOCAL_MODULE_PATH := $(LOCAL_PATH)/bin
include $(BUILD_EXECUTABLE)

