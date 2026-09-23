// This is copyrighted software. More information is at the end of this file.
#pragma once
#include <string>

namespace retro {
class CoreOptions;
extern CoreOptions core_options;
} // namespace retro

class Value;

void init_libretro_conf_properties();
void sync_core_opts_to_conf(const std::string& conf_prop, const Value& new_val);

inline constexpr const char* CORE_OPTCAT_CORE_OPTION_BEHAVIOR = "core_opt_behavior";
inline constexpr const char* CORE_OPT_OPTION_HANDLING = "option_handling";
inline constexpr const char* CORE_OPT_LOAD_DEFAULT_CONF = "load_default_conf";
inline constexpr const char* CORE_OPT_ADV_OPTIONS = "adv_options";
inline constexpr const char* CORE_OPT_SHOW_KB_MAP_OPTIONS = "show_kb_map_options";

inline constexpr const char* CORE_OPTCAT_TIMING = "timing";
inline constexpr const char* CORE_OPT_CORE_TIMING = "core_timing";
inline constexpr const char* CORE_OPT_CORE_VGA_REFRESH = "vga_hz";
inline constexpr const char* CORE_OPT_FRAME_DUPING = "frame_duping";
inline constexpr const char* CORE_OPT_THREAD_SYNC = "thread_sync";

inline constexpr const char* CORE_OPTCAT_FILE_AND_DISK = "file_and_disk";
inline constexpr const char* CORE_OPT_MOUNT_C_AS = "mount_c_as";
inline constexpr const char* CORE_OPT_DEFAULT_MOUNT_FREESIZE = "default_mount_freesize";
inline constexpr const char* CORE_OPT_SAVE_OVERLAY = "save_overlay";

inline constexpr const char* CORE_OPTCAT_VIDEO_EMULATION = "video_emulation";
inline constexpr const char* CORE_OPT_MACHINE_TYPE = "machine";
inline constexpr const char* CORE_OPT_MACHINE_HERCULES_PALETTE = "machine_hercules_palette";
inline constexpr const char* CORE_OPT_MACHINE_CGA_COMPOSITE_MODE = "machine_cga_composite_mode";
inline constexpr const char* CORE_OPT_MACHINE_CGA_MODEL = "machine_cga_model";
inline constexpr const char* CORE_OPT_VOODOO = "voodoo";
inline constexpr const char* CORE_OPT_VOODOO_MEMORY_SIZE = "voodoomem";

inline constexpr const char* CORE_OPTCAT_SPECS = "specs";
inline constexpr const char* CORE_OPT_MEMORY_SIZE = "memsize";
inline constexpr const char* CORE_OPT_XMS = "xms";
inline constexpr const char* CORE_OPT_EMS = "ems";
inline constexpr const char* CORE_OPT_UMB = "umb";
inline constexpr const char* CORE_OPT_CPU_CORE = "core";
inline constexpr const char* CORE_OPT_CPU_TYPE = "cputype";
inline constexpr const char* CORE_OPT_CPU_CYCLES_MODE = "cpu_cycles_mode";
inline constexpr const char* CORE_OPT_CPU_CYCLES_MULTIPLIER_REALMODE =
    "cpu_cycles_multiplier_realmode";
inline constexpr const char* CORE_OPT_CPU_CYCLES_REALMODE = "cpu_cycles_realmode";
inline constexpr const char* CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE_REALMODE =
    "cpu_cycles_multiplier_fine_realmode";
inline constexpr const char* CORE_OPT_CPU_CYCLES_FINE_REALMODE = "cpu_cycles_fine_realmode";
inline constexpr const char* CORE_OPT_CPU_CYCLES_LIMIT = "cpu_cycles_limit";
inline constexpr const char* CORE_OPT_CPU_CYCLES_MULTIPLIER = "cpu_cycles_multiplier";
inline constexpr const char* CORE_OPT_CPU_CYCLES = "cpu_cycles";
inline constexpr const char* CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE = "cpu_cycles_multiplier_fine";
inline constexpr const char* CORE_OPT_CPU_CYCLES_FINE = "cpu_cycles_fine";
inline constexpr const char* CORE_OPT_CPU_CYCLES_UP = "cycleup";
inline constexpr const char* CORE_OPT_CPU_CYCLES_DOWN = "cycledown";

inline constexpr const char* CORE_OPTCAT_SCALING = "scaling";
inline constexpr const char* CORE_OPT_ASPECT_CORRECTION = "aspect";
inline constexpr const char* CORE_OPT_SCALER = "scaler";

inline constexpr const char* CORE_OPTCAT_INPUT = "input";
inline constexpr const char* CORE_OPT_JOYSTICK_FORCE_2AXIS = "joystick_force_2axis";
inline constexpr const char* CORE_OPT_JOYSTICK_TIMED = "timed";
inline constexpr const char* CORE_OPT_EMULATED_MOUSE_DEADZONE = "emulated_mouse_deadzone";
inline constexpr const char* CORE_OPT_MOUSE_SPEED_X = "mouse_speed_x";
inline constexpr const char* CORE_OPT_MOUSE_SPEED_Y = "mouse_speed_y";
inline constexpr const char* CORE_OPT_MOUSE_SPEED_MULT = "mouse_speed_mult";
inline constexpr const char* CORE_OPT_MOUSE_SPEED_HACK = "mouse_speed_hack";
inline constexpr const char* CORE_OPT_MOUSE_SPEED_CLAMP = "mouse_speed_clamp";

inline constexpr const char* CORE_OPTCAT_VKBD = "vkbd";
inline constexpr const char* CORE_OPT_VKBD_ENABLED = "vkbd_enabled";
inline constexpr const char* CORE_OPT_VKBD_THEME = "vkbd_theme";
inline constexpr const char* CORE_OPT_VKBD_TRANSPARENCY = "vkbd_transparency";

inline constexpr const char* CORE_OPTCAT_PAD0_KB_MAPPINGS = "pad0_kb_mappings";
inline constexpr const char* CORE_OPT_PAD0_MAP_UP = "pad0_map_up";
inline constexpr const char* CORE_OPT_PAD0_MAP_DOWN = "pad0_map_down";
inline constexpr const char* CORE_OPT_PAD0_MAP_LEFT = "pad0_map_left";
inline constexpr const char* CORE_OPT_PAD0_MAP_RIGHT = "pad0_map_right";
inline constexpr const char* CORE_OPT_PAD0_MAP_B = "pad0_map_b";
inline constexpr const char* CORE_OPT_PAD0_MAP_A = "pad0_map_a";
inline constexpr const char* CORE_OPT_PAD0_MAP_Y = "pad0_map_y";
inline constexpr const char* CORE_OPT_PAD0_MAP_X = "pad0_map_x";
inline constexpr const char* CORE_OPT_PAD0_MAP_SELECT = "pad0_map_select";
inline constexpr const char* CORE_OPT_PAD0_MAP_START = "pad0_map_start";
inline constexpr const char* CORE_OPT_PAD0_MAP_LBUMP = "pad0_map_lbump";
inline constexpr const char* CORE_OPT_PAD0_MAP_RBUMP = "pad0_map_rbump";
inline constexpr const char* CORE_OPT_PAD0_MAP_LTRIG = "pad0_map_ltrig";
inline constexpr const char* CORE_OPT_PAD0_MAP_RTRIG = "pad0_map_rtrig";
inline constexpr const char* CORE_OPT_PAD0_MAP_LTHUMB = "pad0_map_lthumb";
inline constexpr const char* CORE_OPT_PAD0_MAP_RTHUMB = "pad0_map_rthumb";
inline constexpr const char* CORE_OPT_PAD0_MAP_LAUP = "pad0_map_laup";
inline constexpr const char* CORE_OPT_PAD0_MAP_LADOWN = "pad0_map_ladown";
inline constexpr const char* CORE_OPT_PAD0_MAP_LALEFT = "pad0_map_laleft";
inline constexpr const char* CORE_OPT_PAD0_MAP_LARIGHT = "pad0_map_laright";
inline constexpr const char* CORE_OPT_PAD0_MAP_RAUP = "pad0_map_raup";
inline constexpr const char* CORE_OPT_PAD0_MAP_RADOWN = "pad0_map_radown";
inline constexpr const char* CORE_OPT_PAD0_MAP_RALEFT = "pad0_map_raleft";
inline constexpr const char* CORE_OPT_PAD0_MAP_RARIGHT = "pad0_map_raright";

inline constexpr const char* CORE_OPTCAT_PAD1_KB_MAPPINGS = "pad1_kb_mappings";
inline constexpr const char* CORE_OPT_PAD1_MAP_UP = "pad1_map_up";
inline constexpr const char* CORE_OPT_PAD1_MAP_DOWN = "pad1_map_down";
inline constexpr const char* CORE_OPT_PAD1_MAP_LEFT = "pad1_map_left";
inline constexpr const char* CORE_OPT_PAD1_MAP_RIGHT = "pad1_map_right";
inline constexpr const char* CORE_OPT_PAD1_MAP_B = "pad1_map_b";
inline constexpr const char* CORE_OPT_PAD1_MAP_A = "pad1_map_a";
inline constexpr const char* CORE_OPT_PAD1_MAP_Y = "pad1_map_y";
inline constexpr const char* CORE_OPT_PAD1_MAP_X = "pad1_map_x";
inline constexpr const char* CORE_OPT_PAD1_MAP_SELECT = "pad1_map_select";
inline constexpr const char* CORE_OPT_PAD1_MAP_START = "pad1_map_start";
inline constexpr const char* CORE_OPT_PAD1_MAP_LBUMP = "pad1_map_lbump";
inline constexpr const char* CORE_OPT_PAD1_MAP_RBUMP = "pad1_map_rbump";
inline constexpr const char* CORE_OPT_PAD1_MAP_LTRIG = "pad1_map_ltrig";
inline constexpr const char* CORE_OPT_PAD1_MAP_RTRIG = "pad1_map_rtrig";
inline constexpr const char* CORE_OPT_PAD1_MAP_LTHUMB = "pad1_map_lthumb";
inline constexpr const char* CORE_OPT_PAD1_MAP_RTHUMB = "pad1_map_rthumb";
inline constexpr const char* CORE_OPT_PAD1_MAP_LAUP = "pad1_map_laup";
inline constexpr const char* CORE_OPT_PAD1_MAP_LADOWN = "pad1_map_ladown";
inline constexpr const char* CORE_OPT_PAD1_MAP_LALEFT = "pad1_map_laleft";
inline constexpr const char* CORE_OPT_PAD1_MAP_LARIGHT = "pad1_map_laright";
inline constexpr const char* CORE_OPT_PAD1_MAP_RAUP = "pad1_map_raup";
inline constexpr const char* CORE_OPT_PAD1_MAP_RADOWN = "pad1_map_radown";
inline constexpr const char* CORE_OPT_PAD1_MAP_RALEFT = "pad1_map_raleft";
inline constexpr const char* CORE_OPT_PAD1_MAP_RARIGHT = "pad1_map_raright";

inline constexpr const char* CORE_OPTCAT_SOUND_CARD = "sound_card";
inline constexpr const char* CORE_OPT_SBLASTER_TYPE = "sbtype";
inline constexpr const char* CORE_OPT_SBLASTER_BASE = "sbbase";
inline constexpr const char* CORE_OPT_SBLASTER_IRQ = "irq";
inline constexpr const char* CORE_OPT_SBLASTER_DMA = "dma";
inline constexpr const char* CORE_OPT_SBLASTER_HDMA = "hdma";
inline constexpr const char* CORE_OPT_SBMIXER = "sbmixer";
inline constexpr const char* CORE_OPT_SBLASTER_OPL_MODE = "oplmode";
inline constexpr const char* CORE_OPT_SBLASTER_OPL_EMU = "oplemu";
inline constexpr const char* CORE_OPT_GUS = "gus";
inline constexpr const char* CORE_OPT_GUSBASE = "gusbase";
inline constexpr const char* CORE_OPT_GUSIRQ = "gusirq";
inline constexpr const char* CORE_OPT_GUSDMA = "gusdma";
inline constexpr const char* CORE_OPT_PCSPEAKER = "pcspeaker";
inline constexpr const char* CORE_OPT_TANDY = "tandy";
inline constexpr const char* CORE_OPT_DISNEY = "disney";

inline constexpr const char* CORE_OPTCAT_MIDI = "midi";
inline constexpr const char* CORE_OPT_MPU_TYPE = "mpu401";
inline constexpr const char* CORE_OPT_MIDI_DRIVER = "mididevice";
inline constexpr const char* CORE_OPT_MIDI_PORT = "midiconfig";
inline constexpr const char* CORE_OPT_BASSMIDI_SOUNDFONT = "bassmidi.soundfont";
inline constexpr const char* CORE_OPT_BASSMIDI_SFVOLUME = "bassmidi.sfvolume";
inline constexpr const char* CORE_OPT_BASSMIDI_VOICES = "bassmidi.voices";
inline constexpr const char* CORE_OPT_FLUID_SOUNDFONT = "fluid.soundfont";
inline constexpr const char* CORE_OPT_FLUID_SAMPLERATE = "fluid.samplerate";
inline constexpr const char* CORE_OPT_FLUID_GAIN = "fluid.gain";
inline constexpr const char* CORE_OPT_FLUID_POLYPHONY = "fluid.polyphony";
inline constexpr const char* CORE_OPT_FLUID_CORES = "fluid.cores";
inline constexpr const char* CORE_OPT_FLUID_REVERB = "fluid.reverb";
inline constexpr const char* CORE_OPT_FLUID_REVERB_ROOMSIZE = "fluid.reverb.roomsize";
inline constexpr const char* CORE_OPT_FLUID_REVERB_DAMPING = "fluid.reverb.damping";
inline constexpr const char* CORE_OPT_FLUID_REVERB_WIDTH = "fluid.reverb.width";
inline constexpr const char* CORE_OPT_FLUID_REVERB_LEVEL = "fluid.reverb.level";
inline constexpr const char* CORE_OPT_FLUID_CHORUS = "fluid.chorus";
inline constexpr const char* CORE_OPT_FLUID_CHORUS_NUMBER = "fluid.chorus.number";
inline constexpr const char* CORE_OPT_FLUID_CHORUS_LEVEL = "fluid.chorus.level";
inline constexpr const char* CORE_OPT_FLUID_CHORUS_SPEED = "fluid.chorus.speed";
inline constexpr const char* CORE_OPT_FLUID_CHORUS_DEPTH = "fluid.chorus.depth";
inline constexpr const char* CORE_OPT_MT32_TYPE = "mt32.type";
inline constexpr const char* CORE_OPT_MT32_REVERSE_STEREO = "mt32.reverse.stereo";
inline constexpr const char* CORE_OPT_MT32_THREAD = "mt32.thread";
inline constexpr const char* CORE_OPT_MT32_CHUNK = "mt32.chunk";
inline constexpr const char* CORE_OPT_MT32_PREBUFFER = "mt32.prebuffer";
inline constexpr const char* CORE_OPT_MT32_PARTIALS = "mt32.partials";
inline constexpr const char* CORE_OPT_MT32_DAC = "mt32.dac";
inline constexpr const char* CORE_OPT_MT32_ANALOG = "mt32.analog";
inline constexpr const char* CORE_OPT_MT32_REVERB_MODE = "mt32.reverb.mode";
inline constexpr const char* CORE_OPT_MT32_REVERB_TIME = "mt32.reverb.time";
inline constexpr const char* CORE_OPT_MT32_REVERB_LEVEL = "mt32.reverb.level";
inline constexpr const char* CORE_OPT_MT32_RATE = "mt32.rate";
inline constexpr const char* CORE_OPT_MT32_SRC_QUALITY = "mt32.src.quality";
inline constexpr const char* CORE_OPT_MT32_NICEAMPRAMP = "mt32.niceampramp";

inline constexpr const char* CORE_OPT_IPX = "ipx";

inline constexpr const char* CORE_OPTCAT_PINHACK = "pinhack";
inline constexpr const char* CORE_OPT_PINHACK = "pinhack";
inline constexpr const char* CORE_OPT_PINHACKACTIVE = "pinhackactive";
inline constexpr const char* CORE_OPT_PINHACKTRIGGERWIDTH = "pinhacktriggerwidth";
inline constexpr const char* CORE_OPT_PINHACKTRIGGERHEIGHT = "pinhacktriggerheight";
inline constexpr const char* CORE_OPT_PINHACKEXPANDHEIGHT_COARSE = "pinhackexpandheight_coarse";
inline constexpr const char* CORE_OPT_PINHACKEXPANDHEIGHT_FINE = "pinhackexpandheight_fine";

inline constexpr const char* CORE_OPTCAT_LOGGING = "logging";
inline constexpr const char* CORE_OPT_LOG_METHOD = "log_method";
inline constexpr const char* CORE_OPT_LOG_LEVEL = "log_level";

/*

Copyright (C) 2022 Nikos Chantziaras.

This file is part of DOSBox-core.

DOSBox-core is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or (at your option) any later
version.

DOSBox-core is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
DOSBox-core. If not, see <https://www.gnu.org/licenses/>.

*/
