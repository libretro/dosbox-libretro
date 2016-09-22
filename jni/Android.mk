LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := retro

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
ifneq (,$(filter $(TARGET_ARCH_ABI),arm64-v8a armeabi-v7a))
WITH_DYNAREC := arm
endif
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS +=  -DANDROID_X86

ifeq ($(TARGET_ARCH_ABI), x86_64)
WITH_DYNAREC := x86_64
else
WITH_DYNAREC := x86
endif

endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

CORE_DIR := ..

SOURCES_C   :=
SOURCES_ASM :=

ifeq ($(DEBUG),1)
APP_OPTIM := -O0 -g
else
APP_OPTIM := -O3 -DNDEBUG
endif

include $(CORE_DIR)/Makefile.common

LOCAL_SRC_FILES := $(SOURCES_C) $(SOURCES_CXX) $(SOURCES_ASM)
LOCAL_CFLAGS += $(APP_OPTIM) -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 $(INCFLAGS)
LOCAL_CPP_FEATURES += rtti exceptions

include $(BUILD_SHARED_LIBRARY)
