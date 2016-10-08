LOCAL_PATH := $(call my-dir)
GIT_VERSION := "Git ($(shell git describe --abbrev=4 --dirty --always --tags))"

include $(CLEAR_VARS)

LOCAL_MODULE    := retro

#is not reset by clear_vars and will overflow to the next arch if not reset
WITH_DYNAREC :=

ifeq ($(TARGET_ARCH_ABI), armeabi)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
WITH_DYNAREC := oldarm
endif

ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
WITH_DYNAREC := arm
endif

ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
#the armv7 dynarec wont run on armv8 unless running in armv7 emulation mode
endif

ifeq ($(TARGET_ARCH_ABI), x86)
LOCAL_CFLAGS +=  -DANDROID_X86
WITH_DYNAREC := x86
endif

ifeq ($(TARGET_ARCH_ABI), x86_64)
LOCAL_CFLAGS +=  -DANDROID_X86
WITH_DYNAREC := x86_64
endif

ifeq ($(TARGET_ARCH_ABI), mips)
LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
WITH_DYNAREC := mips
endif

ifeq ($(TARGET_ARCH_ABI), mips64)
LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

CORE_DIR := ..

SOURCES_C   :=
SOURCES_ASM :=
INCFLAGS :=
COMMONFLAGS :=

ifeq ($(DEBUG), 1)
APP_OPTIM := -O0 -g
else
APP_OPTIM := -O3 -DNDEBUG
endif

include $(CORE_DIR)/Makefile.common

LOCAL_SRC_FILES := $(SOURCES_C) $(SOURCES_CXX) $(SOURCES_ASM)
LOCAL_CFLAGS += $(APP_OPTIM) $(COMMONFLAGS) -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 $(INCFLAGS) -DGIT_VERSION=\"$(GIT_VERSION)\"
LOCAL_CPP_FEATURES += rtti exceptions

include $(BUILD_SHARED_LIBRARY)
