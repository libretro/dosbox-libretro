ifndef MAKEFILE_FLUIDSYNTH
MAKEFILE_FLUIDSYNTH = 1

FLUIDSYNTH_BUILD_DIR = $(DEPS_BIN_DIR)/fluidsynth_build
FLUIDSYNTH = $(DEPS_BIN_DIR)/lib/pkgconfig/fluidsynth.pc
EXTRA_PACKAGES := fluidsynth $(EXTRA_PACKAGES)
COMMONFLAGS += -DWITH_FLUIDSYNTH

# Always a release build because debug build creates broken .pc file with mingw.
#
# fluidsynth generates some of its tables by compiling make_tables and running
# it, as a sub-project with no toolchain file of its own - so it picks its
# compiler out of the environment, which in a cross build is the cross compiler,
# and the build then tries to run an ARM binary on the machine doing the
# building. $(CC_FOR_BUILD) is the autoconf name for "the compiler for this
# machine" and the libretro webOS template already exports it; when it is set,
# hand it to the build step, which is where that sub-project configures itself.
# CMAKE_TOOLCHAIN_FILE has to go with it: an SDK that exports one in the
# environment would otherwise still cross-compile the generator, since cmake
# 3.21 and later read that variable as the default for the cache entry. The
# outer build takes its toolchain file from the cache it was configured with,
# so dropping it here costs it nothing.
$(FLUIDSYNTH): $(LIBSNDFILE)
	mkdir -p "$(FLUIDSYNTH_BUILD_DIR)"
	cd "$(FLUIDSYNTH_BUILD_DIR)" \
	&& unset CFLAGS \
	&& unset CXXFLAGS \
	&& unset LDFLAGS \
	&& LDFLAGS="-L$(DEPS_BIN_DIR)/lib"$(if $(filter $(platform),osx)," -framework Foundation") \
	    $(CMAKE) \
	    -DCMAKE_FIND_ROOT_PATH="$(DEPS_BIN_DIR)" \
	    -DCMAKE_BUILD_TYPE=Release \
	    -DBUILD_SHARED_LIBS=OFF \
	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	    -DCMAKE_INSTALL_PREFIX="$(DEPS_BIN_DIR)" \
	    -DCMAKE_INSTALL_LIBDIR=lib \
	    -DLIB_INSTALL_DIR=lib \
	    -DDEFAULT_SOUNDFONT= \
	    -Denable-alsa=OFF \
	    -Denable-aufile=OFF \
	    -Denable-dbus=OFF \
	    -Denable-dsound=OFF \
	    -Denable-wasapi=OFF \
	    -Denable-ipv6=OFF \
	    -Denable-jack=OFF \
	    -Denable-ladspa=OFF \
	    -Denable-lash=OFF \
	    -Denable-libinstpatch=OFF \
	    -Denable-libsndfile=ON \
	    -Denable-midishare=OFF \
	    -Denable-network=OFF \
	    -Denable-oboe=OFF \
	    -Denable-opensles=OFF \
	    -Denable-oss=OFF \
	    -Denable-pkgconfig=ON \
	    -Denable-portaudio=OFF \
	    -Denable-pulseaudio=OFF \
	    -Denable-readline=OFF \
	    -Denable-sdl2=OFF \
	    -Denable-systemd=OFF \
	    -Denable-threads=ON \
	    -Denable-openmp=OFF \
	    -Denable-waveout=OFF \
	    -Denable-winmidi=OFF \
	    -Denable-coreaudio=OFF \
	    -Denable-coremidi=OFF \
	    -Denable-framework=OFF \
	    $(EXTRA_CMAKE_FLAGS) \
	    "$(CURDIR)/deps/fluidsynth-sans-glib" \
	&& $(if $(CC_FOR_BUILD),env -u CMAKE_TOOLCHAIN_FILE CC="$(CC_FOR_BUILD)" CXX="$(CXX_FOR_BUILD)") \
	    VERBOSE=1 $(CMAKE) --build . --config Release --target install -j $(NUMPROC)
	touch "$@"

.PHONY: fluidsynth
fluidsynth: $(FLUIDSYNTH)

.PHONY: deps
deps: fluidsynth

endif
