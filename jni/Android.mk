LOCAL_PATH := $(call my-dir)
CORE_DIR := $(LOCAL_PATH)/..

INCFLAGS    :=
COMMONFLAGS :=

WITH_DYNAREC :=
ifeq ($(TARGET_ARCH_ABI), armeabi)
    WITH_DYNAREC := oldarm
else ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    WITH_DYNAREC := arm
else ifeq ($(TARGET_ARCH_ABI), x86)
    WITH_DYNAREC := x86
else ifeq ($(TARGET_ARCH_ABI), x86_64)
    WITH_DYNAREC := x86_64
endif

include $(CORE_DIR)/Makefile.common

COMMONFLAGS += -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 $(INCFLAGS) -DC_HAVE_MPROTECT="1"

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
    COMMONFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE       := retro
LOCAL_SRC_FILES    := $(SOURCES_C) $(SOURCES_CXX)
LOCAL_CFLAGS       := $(COMMONFLAGS)
LOCAL_CPPFLAGS     := $(COMMONFLAGS)
LOCAL_LDFLAGS      := -Wl,-version-script=$(CORE_DIR)/libretro/link.T
LOCAL_CPP_FEATURES := rtti exceptions
LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
include $(BUILD_SHARED_LIBRARY)
