ifndef MAKEFILE_OGG
MAKEFILE_OGG = 1

OGG_BUILD_DIR = $(DEPS_BIN_DIR)/ogg_build
OGG = $(DEPS_BIN_DIR)/lib/pkgconfig/ogg.pc

$(OGG):
	mkdir -p $(OGG_BUILD_DIR)
	cd $(OGG_BUILD_DIR) \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& $(CMAKE) \
	    -DCMAKE_FIND_ROOT_PATH="$(DEPS_BIN_DIR)" \
	    -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	    -DCMAKE_INSTALL_PREFIX="$(DEPS_BIN_DIR)" \
	    -DCMAKE_INSTALL_LIBDIR=lib \
	    -DBUILD_TESTING=OFF \
	    -DINSTALL_DOCS=OFF \
	    -DINSTALL_PKG_CONFIG_MODULE=ON \
	    -DBUILD_FRAMEWORK=OFF \
	    $(EXTRA_CMAKE_FLAGS) \
	    "$(CURDIR)/deps/ogg" \
	&& VERBOSE=1 $(CMAKE) --build . --config $(CMAKE_BUILD_TYPE) --target install -j $(NUMPROC)
	touch "$@"

.PHONY: ogg
ogg: $(OGG)

.PHONY: deps
deps: ogg

endif
