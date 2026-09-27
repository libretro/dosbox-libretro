ifndef MAKEFILE_SDL
MAKEFILE_SDL = 1

SDL_BUILD_DIR = $(DEPS_BIN_DIR)/sdl_build
SDL = $(DEPS_BIN_DIR)/lib/pkgconfig/sdl.pc

$(SDL):
	mkdir -p "$(SDL_BUILD_DIR)"
	cd "$(SDL_BUILD_DIR)" \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& $(if $(filter $(platform),osx),CC=clang CXX=clang++) \
	    "$(CURDIR)/deps/sdl/configure" \
	    --host=$(TARGET_TRIPLET) \
	    --prefix="$(DEPS_BIN_DIR)" \
	    --disable-shared \
	    --enable-static \
	    --disable-audio \
	    --disable-video \
	    --disable-events \
	    --disable-joystick \
	    --enable-cdrom \
	    --enable-threads \
	    --enable-timers \
	    --enable-file \
	    --disable-loadso \
	    --enable-cpuinfo \
	    --enable-assembly \
	    --disable-nasm \
	    --disable-altivec \
	    --disable-stdio-redirect \
	    --disable-rpath \
	    --with-pic \
	    --without-x \
	&& $(MAKE) -j$(NUMPROC) install
	touch "$@"

.PHONY: sdl
sdl: $(SDL)

.PHONY: deps
deps: sdl

endif
