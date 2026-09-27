# DOSBox-core

A libretro core of [DOSBox](https://www.dosbox.com) for use in
[RetroArch](https://www.retroarch.com) and other
[libretro frontends](https://www.libretro.com/index.php/powered-by-libretro).
DOSBox-core is kept up to date with the latest sources from DOSBox SVN trunk.

The core provides some improvements over the DOSBox-SVN core:

* Native MIDI support on Linux and Windows.  
  You can select the MIDI output port in the core settings. The core will try
  to select the correct MIDI device even if the MIDI port changes in the
  future, as it remembers the MIDI port by name, not by number.

  Native MIDI support allows you to use any stand-alone MIDI synth, like the
  Munt MT-32 emulator or Roland's Sound Canvas emulator VSTi plugin "Sound
  Canvas VA," which you can run using a VSTi host like
  [Falcosoft Midi Player](https://www.vogons.org/viewtopic.php?f=5&t=48207),
  which is shown running on Linux through Wine in this screenshot:

  <img src="/docs/images/screenshot_ultima8_scva.jpg?raw=true" alt="screenshot" style="width: 40%;"/>

* Cycle-accurate OPL3 (YMF262) emulation option.  
  Using [Nuked OPL3](https://nukeykt.retrohost.net).

* Built-in MT-32, CM32-L and LAPC-I emulation.  
  Using [Munt](https://github.com/munt/munt).

* Built-in soundfont-based MIDI synthesizer.  
  - Using [FluidSynth](http://www.fluidsynth.org), a MIDI software synthesizer
    that supports SF2/SF3 soundfonts.
  - Using [BASSMIDI](https://www.un4seen.com), a MIDI software synthesizer that
    supports SF2/SFZ soundfonts.

* Support for CUE CD images with split audio tracks.  
  Audio tracks can be WAV, FLAC, Opus, Ogg Vorbis or MP3 files.

* Experimental 3dfx Voodoo support.  
  This is based on a [patch by kekko](https://www.vogons.org/viewtopic.php?t=41853),
  which in turn is based on MAME code. Currently, only software-based emulation
  is provided and as a result it is very slow. Most games will probably not be
  able to run at full speed yet.

* On-screen virtual keyboard.  
  Ported over by [sonninnos](https://github.com/sonninnos) from the
  [UAE](https://github.com/libretro/libretro-uae) and
  [VICE](https://github.com/libretro/vice-libretro) cores.

* Pinhack support.  
  A [patch by Felipe Sanches](https://github.com/DeXteRrBDN/dosbox-pinhack)
  that allows some pinball games (like Pinball Dreams, Pinball Fantasies and
  others) to show the whole table at once without scrolling. There is a
  [discussion thread over at Vogons](https://www.vogons.org/viewtopic.php?f=41&t=12424)
  about this patch.

* Better support for .conf files.  
  When a .conf file is loaded as content, the dosbox settings specified in the
  .conf file will not conflict with core options. The core options will be
  synced with the .conf file settings and marked as locked.

  It's also possible to load the default DOSBox-core.conf file in the libretro
  saves directory, if it exists. This is disabled by default but can be enabled
  in the core options. This is equivalent to using the "-userconf" flag in
  stand-alone dosbox.

* Other general, under-the-hood improvements and bugfixes.

DOSBox-core was originally based on the https://github.com/libretro/dosbox-svn
libretro core, developed mainly by [Radius](https://github.com/fr500) as well
as other contributors (the full commit history has been preserved.)

## Supported Platforms

* Linux (x86, x86-64, ARMv7 (armhf), ARM64)
* Windows 7 or later
* macOS 10.9 (Mavericks) or later
* Android

It may work on other platforms as well if you build from source.

## Usage

You can load .exe, .bat, .iso, .cue, and .conf files directly. Note that when
loading content, the current directory is set to the content's directory. This
means it is possible to use relative paths in your `mount` and `imgmount`
commands.

The recommended way to run DOS games is to have a .conf file for each game and
then use the manual scanner in RetroArch to scan for `conf` files. For example,
here is:

```text
Ultima VII - The Black Gate.conf
```

for running Ultima 7 when the game's DOS installation is inside the
`drive_c/ultima71` folder and the game expects to be available in `C:\ultima71`
inside DOS:

```ini
[dos]
xms = true
ems = false
umb = false

[autoexec]
@echo off
mixer sb 50:50 /noshow
mount c drive_c
c:
cd ultima71
ultima7.com
exit
```

This game is known to not run correctly with EMS and UMB, and runs best with
XMS, so you can set those dosbox settings as shown above. The respective core
options for XMS, EMS and UMB will be locked.

Specifying dosbox settings in the .conf file is optional. In this example, you
could just as well configure XMS, EMS and UMB using the core options instead.
But if you prefer to configure settings in .conf files, you can do it without
worrying that those settings might conflict with core options.

### Cycling between mounted CD images

If you mount multiple CD images to the same CD-ROM drive:

```text
imgmount d cd1.cue cd2.cue cd3.cue -t cdrom
```

You can cycle between them with CTRL+F4.

### Temporary adjustment of CPU cycles

You can decrease/increase current CPU cycles using CTRL+F11 and CTRL+F12. The
cycle decrement/increment amount can be configured in the core options. The
change in cycles is only temporary and will not be saved permanently.

### MT-32

For MT-32 emulation, make sure you have the correct MT-32 ROMs in your
frontend's system directory. See the `dosbox_core_libretro.info` file for the
ROM filenames and MD5 checksums.

### BASSMIDI

To use BASSMIDI for MIDI output, you need to download the BASS and BASSMIDI
dynamic libraries for your OS from https://www.un4seen.com and place them in
the system folder of your frontend. These files are not included with
DOSBox-core due to licensing issues.

On Linux you need `libbass.so` and `libbassmidi.so`, on Windows `bass.dll` and
`bassmidi.dll`, and on macOS `libbass.dylib` and `libbassmidi.dylib`. Make sure
to use the 32-bit or 64-bit versions of these files depending on whether your
frontend is 32-bit or 64-bit.

## Compilation

Building the core requires a C++17 compiler. GCC 9 and Clang 9 are known to
work. When building on macOS with XCode Clang, macOS 10.15 and XCode 11.1 are
required. Using GCC instead will allow building for older macOS versions.

CMake and Ninja are also assumed to be installed in order to build the bundled
dependencies.

Most of the dependencies are bundled. They are built and linked statically.
See `Makefile.libretro` for which make variables to set to enable/disable the
bundled libraries.

The only dependencies that are not bundled are alsa-lib, which is only needed
on Linux, and dlfnc which is needed on Windows. dlfcn is available as a package
in both MXE as well as MSYS2.

To build on Linux x86-64 with the bundled audio, libsndfile, SDL and SDL_net
libraries, you would do:

    cd libretro
    make BUNDLED_AUDIO_CODECS=1 BUNDLED_LIBSNDFILE=1 BUNDLED_SDL=1 WITH_DYNAREC=x86_64 deps
    make BUNDLED_AUDIO_CODECS=1 BUNDLED_LIBSNDFILE=1 BUNDLED_SDL=1 WITH_DYNAREC=x86_64 -j`nproc`

The first `make` invokation will build the bundled dependencies, the second
will built the core itself. You can omit the first invokation and the deps
will be built regardless, but doing it this way results in a faster build.

Note that the `clean` target will clean both dependency
and core object files. To clean just core objects, use the `targetclean`
target. To clean deps only, use `depsclean`.

To specify which compiler to use, set the `CC` and `CXX` environment
variables. For example:

    CC=gcc-9 CXX=g++-9 make ...

If you want to cross-compile, you should set `TARGET_TRIPLET` to the prefix
string of your cross compilation toolchain. If your toolchain uses wrappers for
cmake and pkg-config, pass their full names when calling make. For example:

    make TARGET_TRIPLET="i686-w64-mingw32.static" \
      PKGCONFIG="i686-w64-mingw32.static-pkg-config" \
      CMAKE="i686-w64-mingw32.static-cmake" \
      ...

If you want to use a different CMake generator when building dependencies that
use CMake, set the `CMAKE_GENERATOR` variable. By default, it's set to `Ninja`.
