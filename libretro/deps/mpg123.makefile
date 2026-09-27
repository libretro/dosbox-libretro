ifndef MAKEFILE_LIBMPG123
MAKEFILE_LIBMPG123 = 1

LIBMPG123_BUILD_DIR = $(DEPS_BIN_DIR)/mpg123_build
LIBMPG123 = $(DEPS_BIN_DIR)/lib/pkgconfig/libmpg123.pc

# Always build in release mode since debug mode spams stdout with infinite debug messages.
$(LIBMPG123):
	mkdir -p $(LIBMPG123_BUILD_DIR)
	cd $(LIBMPG123_BUILD_DIR) \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& CFLAGS="-g -O2 -Wno-error=incompatible-pointer-types" \
	   "$(CURDIR)/deps/mpg123/configure" \
	    --host=$(TARGET_TRIPLET) \
	    --prefix="$(DEPS_BIN_DIR)" \
	    --disable-shared \
	    --enable-static \
	    --disable-debug \
	    --with-pic \
	    --disable-modules \
	    --disable-network \
	    --disable-ipv6 \
	    --disable-id3v2 \
	    --disable-equalizer \
	    --with-audio=dummy \
	    --with-default-audio=dummy \
	&& $(MAKE) -j$(NUMPROC) install
	touch "$@"

.PHONY: mpg123
mpg123: $(LIBMPG123)

.PHONY: deps
deps: mpg123

endif
