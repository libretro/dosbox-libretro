ifndef MAKEFILE_MUNT
MAKEFILE_MUNT = 1

MUNT_BUILD_DIR = $(DEPS_BIN_DIR)/munt_build
MUNT = $(DEPS_BIN_DIR)/lib/pkgconfig/mt32emu.pc
EXTRA_PACKAGES := mt32emu $(EXTRA_PACKAGES)

$(MUNT):
	mkdir -p $(MUNT_BUILD_DIR)
	cd $(MUNT_BUILD_DIR) \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& $(CMAKE) \
	    -DCMAKE_FIND_ROOT_PATH="$(DEPS_BIN_DIR)" \
	    -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	    -DCMAKE_INSTALL_PREFIX="$(DEPS_BIN_DIR)" \
	    -DLIB_INSTALL_DIR:PATH=lib \
	    -Dlibmt32emu_PKGCONFIG_INSTALL_PREFIX:PATH=lib \
	    -Dlibmt32emu_C_INTERFACE=ON \
	    -Dlibmt32emu_SHARED=OFF \
	    -Dlibmt32emu_WITH_INTERNAL_RESAMPLER=ON \
	    $(EXTRA_CMAKE_FLAGS) \
	    "$(CURDIR)/deps/munt/mt32emu" \
	&& VERBOSE=1 $(CMAKE) --build . --config $(CMAKE_BUILD_TYPE) --target install -j $(NUMPROC)
	touch "$@"

.PHONY: munt
munt: $(MUNT)

.PHONY: deps
deps: munt

endif
