ifndef MAKEFILE_FLAC
MAKEFILE_FLAC = 1

FLAC_BUILD_DIR = $(DEPS_BIN_DIR)/flac_build
FLAC = $(DEPS_BIN_DIR)/lib/pkgconfig/flac.pc

$(FLAC): $(OGG)
	cd "$(CURDIR)/deps/flac/" && ./autogen.sh
	mkdir -p $(FLAC_BUILD_DIR)
	cd $(FLAC_BUILD_DIR) \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& "$(CURDIR)/deps/flac/configure" \
	    --host=$(TARGET_TRIPLET) \
	    --prefix="$(DEPS_BIN_DIR)" \
	    --disable-shared \
	    --enable-static \
	    --enable-debug=$(AUTOCONF_DEBUG_FLAG) \
	    --disable-doxygen-docs \
	    --disable-xmms-plugin \
	    --disable-cpplibs \
	    --disable-oggtest \
	    --disable-examples \
	    --disable-rpath \
	    --with-pic \
	&& $(MAKE) -j$(NUMPROC) install
	touch "$@"

# $(FLAC): $(OGG)
# 	mkdir -p $(FLAC_BUILD_DIR)
# 	cd $(FLAC_BUILD_DIR) \
#	&& unset CFLAGS \
#	&& unset CXXFLAGS \
#	&& unset LDFLAGS \
# 	&& $(CMAKE) \
#	    -DCMAKE_FIND_ROOT_PATH="$(DEPS_BIN_DIR)" \
# 	    -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
# 	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
# 	    -DCMAKE_INSTALL_PREFIX="$(DEPS_BIN_DIR)" \
# 	    -DCMAKE_INSTALL_LIBDIR=lib \
# 	    -DBUILD_CXXLIBS=OFF \
# 	    -DBUILD_PROGRAMS=OFF \
# 	    -DBUILD_DOCS=OFF \
# 	    -DBUILD_DOXYGEN=OFF \
# 	    -DBUILD_EXAMPLES=OFF \
# 	    -DBUILD_TESTING=OFF \
# 	    -DENABLE_WERROR=OFF \
# 	    -DWITH_STACK_PROTECTOR=OFF \
# 	    $(EXTRA_CMAKE_FLAGS) \
# 	    "$(CURDIR)/deps/flac" \
# 	&& VERBOSE=1 $(CMAKE) --build . --config $(CMAKE_BUILD_TYPE) --target install -j $(NUMPROC)

.PHONY: flac
flac: $(FLAC)

.PHONY: deps
deps: flac

endif
