LOCAL_PATH := $(call my-dir)
CORE_DIR   := $(LOCAL_PATH)/../..

INCFLAGS    := -I$(CORE_DIR)/libretro/deps/embedded_sdl/include/ -I$(CORE_DIR)/libretro/deps/embedded_sdl/include/SDL/ -I$(CORE_DIR)/libretro/deps/embedded_sdl/SDL_net/ -I$(CORE_DIR)/libretro/deps/fmt/include
COMMONFLAGS :=

WITH_DYNAREC :=
ifeq ($(TARGET_ARCH_ABI), armeabi)
    WITH_DYNAREC := oldarm
else ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    WITH_DYNAREC := arm
else ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
    WITH_DYNAREC := arm64
else ifeq ($(TARGET_ARCH_ABI), x86)
    WITH_DYNAREC := x86_generic
else ifeq ($(TARGET_ARCH_ABI), x86_64)
    WITH_DYNAREC := x86_64
else ifeq ($(TARGET_ARCH_ABI), mips)
    WITH_DYNAREC := mips
else ifeq ($(TARGET_ARCH_ABI), mips64)
    WITH_DYNAREC := mips64
endif

WITH_IPX := 1
WITH_VOODOO := 0

include $(CORE_DIR)/libretro/Makefile.common

SOURCES_C += \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/thread/pthread/SDL_syscond.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/thread/pthread/SDL_sysmutex.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/thread/pthread/SDL_syssem.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/thread/pthread/SDL_systhread.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/thread/SDL_thread.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/timer/unix/SDL_systimer.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/timer/SDL_timer.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/cdrom/dummy/SDL_syscdrom.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/cdrom/SDL_cdrom.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL/src/SDL_error.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL_net/SDLnet.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL_net/SDLnetTCP.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL_net/SDLnetUDP.c \
    $(CORE_DIR)/libretro/deps/embedded_sdl/SDL_net/SDLnetselect.c

SOURCES_CXX +=\
    $(CORE_DIR)/libretro/src/nonlibc/snprintf.cpp

COMMONFLAGS += -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 $(INCFLAGS) -DC_HAVE_MPROTECT -DC_IPX

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
SVN_VERSION := " $(shell cat ../svn)"

ifneq ($(GIT_VERSION)," unknown")
    COMMONFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

ifneq ($(SVN_VERSION)," unknown")
    COMMONFLAGS += -DSVN_VERSION=\"$(SVN_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE := munt
LOCAL_SRC_FILES := $(CORE_DIR)/libretro/deps_bin/$(TARGET_ARCH_ABI)/munt_build/libmt32emu.a
LOCAL_EXPORT_C_INCLUDES := $(CORE_DIR)/libretro/deps_bin/$(TARGET_ARCH_ABI)/munt_build/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := retro
LOCAL_SRC_FILES := $(SOURCES_C) $(SOURCES_CXX)
LOCAL_STATIC_LIBRARIES := munt
LOCAL_CFLAGS := $(COMMONFLAGS)
LOCAL_CPPFLAGS := $(COMMONFLAGS) -std=gnu++17 -Wno-register -DFMT_HEADER_ONLY
LOCAL_LDFLAGS := -Wl,-version-script=$(CORE_DIR)/libretro/link.T
LOCAL_LDLIBS := -llog
LOCAL_CPP_FEATURES := rtti exceptions
LOCAL_ARM_MODE := arm
include $(BUILD_SHARED_LIBRARY)
