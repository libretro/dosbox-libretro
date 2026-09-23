ifndef MAKEFILE_LIBSNDFILE
MAKEFILE_LIBSNDFILE = 1

LIBSNDFILE_BUILD_DIR = $(DEPS_BIN_DIR)/libsndfile_build
LIBSNDFILE = $(DEPS_BIN_DIR)/lib/pkgconfig/sndfile.pc
EXTRA_PACKAGES := sndfile $(EXTRA_PACKAGES)

$(LIBSNDFILE): $(OGG) $(VORBIS) $(OPUS) $(FLAC)
	mkdir -p "$(LIBSNDFILE_BUILD_DIR)"
	cd "$(LIBSNDFILE_BUILD_DIR)" \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& CFLAGS="-DFLAC__NO_DLL" $(CMAKE) \
	    -DCMAKE_FIND_ROOT_PATH="$(DEPS_BIN_DIR)" \
	    -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
	    -DBUILD_SHARED_LIBS=OFF \
	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	    -DCMAKE_INSTALL_PREFIX="$(DEPS_BIN_DIR)" \
	    -DCMAKE_INSTALL_LIBDIR=lib \
	    -DBUILD_EXAMPLES=OFF \
	    -DBUILD_PROGRAMS=OFF \
	    -DBUILD_REGTEST=OFF \
	    -DBUILD_TESTING=OFF \
	    -DENABLE_EXTERNAL_LIBS=ON \
	    -DENABLE_PACKAGE_CONFIG=ON \
	    $(EXTRA_CMAKE_FLAGS) \
	    "$(CURDIR)/deps/libsndfile" \
	&& VERBOSE=1 $(CMAKE) --build . --config $(CMAKE_BUILD_TYPE) --target install -j $(NUMPROC) \
	&& sed -i'.original' '/^Requires:/d' $(LIBSNDFILE) \
	&& sed -i'.original' 's/^Requires.private:/Requires:/g' $(LIBSNDFILE)
	touch "$@"

.PHONY: libsndfile
libsndfile: $(LIBSNDFILE)

.PHONY: deps
deps: libsndfile

endif
