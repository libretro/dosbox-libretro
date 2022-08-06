# libretro wrapper for DOSBox

This core is obsolete. Please use [DOSBox Pure](https://github.com/schellingb/dosbox-pure/) or [DOSBox-SVN](https://github.com/libretro/dosbox-svn) instead. No pull requests will be granted for this project.

<hr />

* To use you can either load an exe/com/bat file or a \*.conf file.
* If loading exe/com/bat the system directory will be searched for a *dosbox.conf* file to load. If one isn't available default values will be used. This mode is equivalent to running a DOSBox binary with the specified file as the command line argument.
* If loading a conf file DOSBox will be loaded with the options in the config file. This mode is useful if you just want to be dumped at a command prompt, but can also be used to load a game by putting commands in the autoexec section.
* To be useful the frontend will need to have keyboard+mouse support, and all keyboard shortcuts need to be remapped.

## Unsupported features:

* Physical CD-ROMs, CD images are supported.
* The key mapper, key remapping does not work.

## TODO:

* Joysticks need more testing. Flightsticks types are not implemented yet.

## Building:

* To build for your current platform:

```
cd */libretro-dosbox
make
```

## Notes:

* There seems to be no trivial way to have the DOSBox core return periodically, so libco is used to enter/exit the emulator loop. This actually works better than one would expect.
* There is no serialization support, it's not supported by DOSBox.
* DOSBox uses 'wall time' for timing, frontend fast forward and slow motion features will have no effect.
* To use MIDI you need MT32_CONTROL.ROM and MT32_PCM.ROM in the system directory of RetroArch. Then set:

<pre>[midi]
mpu401=intelligent
mididevice=mt32</pre>
