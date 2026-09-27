ifndef MAKEFILE_OPUS
MAKEFILE_OPUS = 1

OPUS_BUILD_DIR = $(DEPS_BIN_DIR)/opus_build
OPUS = $(DEPS_BIN_DIR)/lib/pkgconfig/opus.pc

$(OPUS):
	mkdir -p $(OPUS_BUILD_DIR)
	cd $(OPUS_BUILD_DIR) \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& $(CMAKE) \
	    -DCMAKE_FIND_ROOT_PATH="$(DEPS_BIN_DIR)" \
	    -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	    -DCMAKE_INSTALL_PREFIX="$(DEPS_BIN_DIR)" \
	    -DCMAKE_INSTALL_LIBDIR=lib \
	    -DOPUS_BUILD_PROGRAMS=OFF \
	    -DOPUS_BUILD_SHARED_LIBRARY=OFF \
	    -DOPUS_INSTALL_PKG_CONFIG_MODULE=ON \
	    -DOPUS_STACK_PROTECTOR=OFF \
	    -DOPUS_DISABLE_INTRINSICS=ON \
	    $(EXTRA_CMAKE_FLAGS) \
	    "$(CURDIR)/deps/opus" \
	&& VERBOSE=1 $(CMAKE) --build . --config $(CMAKE_BUILD_TYPE) --target install -j $(NUMPROC) \
	&& sed -i'.original' 's/^Version: 0/Version: 1.3.1/g' $(OPUS)
	touch "$@"

.PHONY: opus
opus: $(OPUS)

.PHONY: deps
deps: opus

endif
