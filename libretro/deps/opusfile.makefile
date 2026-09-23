ifndef MAKEFILE_OPUSFILE
MAKEFILE_OPUSFILE = 1

OPUSFILE_BUILD_DIR = $(DEPS_BIN_DIR)/opusfile_build
OPUSFILE = $(DEPS_BIN_DIR)/lib/pkgconfig/opusfile.pc

$(OPUSFILE): $(OGG) $(OPUS)
	cd "$(CURDIR)/deps/opusfile/" && ./autogen.sh
	mkdir -p $(OPUSFILE_BUILD_DIR)
	cd $(OPUSFILE_BUILD_DIR) \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& "$(CURDIR)/deps/opusfile/configure" \
	    --host=$(TARGET_TRIPLET) \
	    --prefix="$(DEPS_BIN_DIR)" \
	    --disable-shared \
	    --enable-static \
	    --disable-debug \
	    --with-pic \
	    --disable-http \
	    --disable-examples \
	    --disable-doc \
	&& $(MAKE) -j$(NUMPROC) install
	touch "$@"

.PHONY: opusfile
opusfile: $(OPUSFILE)

.PHONY: deps
deps: opusfile

endif
