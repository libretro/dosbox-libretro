ifndef MAKEFILE_SDLNET
MAKEFILE_SDLNET = 1

SDLNET_BUILD_DIR = $(DEPS_BIN_DIR)/sdl_net_build
SDLNET = $(DEPS_BIN_DIR)/lib/pkgconfig/SDL_net.pc

$(SDLNET): $(SDL)
	mkdir -p "$(SDLNET_BUILD_DIR)"
	cd "$(SDLNET_BUILD_DIR)" \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& $(if $(filter $(platform),osx),CC=clang CXX=clang++) \
	    "$(CURDIR)/deps/sdl_net/configure" \
	    --host=$(TARGET_TRIPLET) \
	    --prefix="$(DEPS_BIN_DIR)" \
	    --disable-shared \
	    --enable-static \
	    --disable-sdltest \
	    --disable-gui \
	    --with-pic \
	&& sed -i'.original' 's/^PROGRAMS =.*/PROGRAMS = /g' Makefile \
	&& $(MAKE) -j$(NUMPROC) install
	touch "$@"

.PHONY: sdl_net
sdl_net: $(SDLNET)

.PHONY: deps
deps: sdl_net

endif
