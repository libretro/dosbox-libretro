ifndef MAKEFILE_VORBIS
MAKEFILE_VORBIS = 1

VORBIS_BUILD_DIR = $(DEPS_BIN_DIR)/vorbis_build
VORBIS = $(DEPS_BIN_DIR)/lib/pkgconfig/vorbis.pc

$(VORBIS): $(OGG)
	mkdir -p $(VORBIS_BUILD_DIR)
	cd $(VORBIS_BUILD_DIR) \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& $(CMAKE) \
	    -DCMAKE_FIND_ROOT_PATH="$(DEPS_BIN_DIR)" \
	    -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	    -DCMAKE_INSTALL_PREFIX="$(DEPS_BIN_DIR)" \
	    -DCMAKE_INSTALL_LIBDIR=lib \
	    -DBUILD_FRAMEWORK=OFF \
	    $(EXTRA_CMAKE_FLAGS) \
	    "$(CURDIR)/deps/vorbis" \
	&& VERBOSE=1 $(CMAKE) --build . --config $(CMAKE_BUILD_TYPE) --target install -j $(NUMPROC) \
	&& sed -i'.original' '/^Requires:/d' $(VORBIS) \
	&& sed -i'.original' 's/^Requires.private:/Requires:/g' $(VORBIS)
	touch "$@"

.PHONY: vorbis
vorbis: $(VORBIS)

.PHONY: deps
deps: vorbis

endif
