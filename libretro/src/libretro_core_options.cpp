// This is copyrighted software. More information is at the end of this file.
#include "libretro_core_options.h"
#include "CoreOptions.h"
#include "control.h"
#include "deps/textflowcpp/TextFlow.hpp"
#include "libretro_dosbox.h"
#include "libretro_gfx.h"
#include "log.h"
#include "setup.h"
#include "util.h"
#include <fmt/format.h>

inline constexpr const char* LOCKED_OPTION_INFO =
    "This option has been locked due to a .conf file setting or a DOSBox command.";
inline constexpr const char* LOCKED_OPTION_DESC = "[Locked]";

bool disable_core_opt_sync = false;
static std::set<std::string> locked_core_options;

void init_libretro_conf_properties()
{
    auto* secprop = control->AddSection_prop(
        "dosbox-core",
        [](Section* const conf) {
            const auto* const section = static_cast<Section_prop*>(conf);
            const auto mult = section->Get_int(CORE_OPT_MOUSE_SPEED_MULT);
            mouse_speed_factor_x = section->Get_int(CORE_OPT_MOUSE_SPEED_X) * mult / 100.0f;
            mouse_speed_factor_y = section->Get_int(CORE_OPT_MOUSE_SPEED_Y) * mult / 100.0f;
            update_mouse_speed_fix(gfx::height);
            set_mouse_speed_clamp(section->Get_bool(CORE_OPT_MOUSE_SPEED_CLAMP));
        },
        true);

    auto* int_prop = secprop->Add_int(CORE_OPT_MOUSE_SPEED_X, Property::Changeable::Always, 100);
    int_prop->SetMinMax(1, 127);
    int_prop->Set_help(
        TextFlow::Column(fmt::format(
                             "{} (min {}, max {})",
                             retro::core_options.option(CORE_OPT_MOUSE_SPEED_X)->descAndInfo(),
                             int_prop->getMin(), int_prop->getMax()))
            .width(70)
            .toString());

    int_prop = secprop->Add_int(CORE_OPT_MOUSE_SPEED_Y, Property::Changeable::Always, 100);
    int_prop->SetMinMax(1, 127);
    int_prop->Set_help(
        TextFlow::Column(fmt::format(
                             "{} (min {}, max {})",
                             retro::core_options.option(CORE_OPT_MOUSE_SPEED_Y)->descAndInfo(),
                             int_prop->getMin(), int_prop->getMax()))
            .width(70)
            .toString());

    int_prop = secprop->Add_int(CORE_OPT_MOUSE_SPEED_MULT, Property::Changeable::Always, 1);
    int_prop->SetMinMax(1, 5);
    int_prop->Set_help(
        TextFlow::Column(fmt::format(
                             "{} (min {}, max {})",
                             retro::core_options.option(CORE_OPT_MOUSE_SPEED_MULT)->descAndInfo(),
                             int_prop->getMin(), int_prop->getMax()))
            .width(70)
            .toString());

    auto* bool_prop =
        secprop->Add_bool(CORE_OPT_MOUSE_SPEED_HACK, Property::Changeable::Always, false);
    bool_prop->Set_help(
        TextFlow::Column(retro::core_options.option(CORE_OPT_MOUSE_SPEED_HACK)->descAndInfo())
            .width(70)
            .toString());

    bool_prop = secprop->Add_bool(CORE_OPT_MOUSE_SPEED_CLAMP, Property::Changeable::Always, false);
    bool_prop->Set_help(
        TextFlow::Column(retro::core_options.option(CORE_OPT_MOUSE_SPEED_CLAMP)->descAndInfo())
            .width(70)
            .toString());

    int_prop = secprop->Add_int(CORE_OPT_CORE_VGA_REFRESH, Property::Changeable::Always, 0);
    int_prop->SetMinMax(0, 70);
    int_prop->Set_help(
        TextFlow::Column(fmt::format(
                             "{} (min 50, max 70, 0 to disable)",
                             retro::core_options.option(CORE_OPT_CORE_VGA_REFRESH)->descAndInfo()))
            .width(70)
            .toString());
}

static bool sync_special_option(const std::string_view prop, const Value& new_val)
{
    if (is_equal_to_one_of(
            prop, "language", "captures", "frameskip", "nosound", "rate", "oplrate", "ultradir",
            "pcrate", "tandyrate", "joysticktype", "autofire", "swap34", "buttonwrap",
            "circularinput", "deadzone", "serial1", "serial2", "serial3", "serial4",
            "keyboardlayout", "mt32.romdir", "pinhackexpandwidth"))
    {
        // We simply ignore all of these. They have no effect on any of the core options.
        return true;
    }

    const bool disable_option =
        retro::core_options[CORE_OPT_OPTION_HANDLING].toString() == "disable";

    if (prop == "cycles") {
        for (const auto& opt_name :
             {CORE_OPT_CPU_CYCLES_MODE, CORE_OPT_CPU_CYCLES_MULTIPLIER_REALMODE,
              CORE_OPT_CPU_CYCLES_REALMODE, CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE_REALMODE,
              CORE_OPT_CPU_CYCLES_FINE_REALMODE, CORE_OPT_CPU_CYCLES_LIMIT,
              CORE_OPT_CPU_CYCLES_MULTIPLIER, CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE,
              CORE_OPT_CPU_CYCLES_FINE})
        {
            disabled_core_options.insert(opt_name);
            retro::core_options.setVisible(opt_name, false);
        }

        if (disable_option) {
            disabled_core_options.insert(CORE_OPT_CPU_CYCLES);
            retro::core_options.setVisible(CORE_OPT_CPU_CYCLES, false);
        } else {
            auto* opt = retro::core_options.option(CORE_OPT_CPU_CYCLES);
            if (locked_core_options.count(CORE_OPT_CPU_CYCLES) == 0) {
                opt->setDesc(fmt::format("{} CPU cycles", LOCKED_OPTION_DESC));
                opt->setInfo(fmt::format("Emulated CPU cycles.\n\n{}", LOCKED_OPTION_INFO));
                locked_core_options.insert(CORE_OPT_CPU_CYCLES);
            }
            retro::CoreOptionValue val(new_val.ToString(), new_val.ToString());
            retro::core_options.option(CORE_OPT_CPU_CYCLES)->setValues({val}, val);
        }
    } else if (prop == "pinhackexpandheight") {
        retro::core_options.setCurrentValue(CORE_OPT_PINHACKEXPANDHEIGHT_FINE, 0);
        disabled_core_options.insert(CORE_OPT_PINHACKEXPANDHEIGHT_FINE);
        retro::core_options.setVisible(CORE_OPT_PINHACKEXPANDHEIGHT_FINE, false);
        if (disable_option) {
            disabled_core_options.insert(CORE_OPT_PINHACKEXPANDHEIGHT_COARSE);
            retro::core_options.setVisible(CORE_OPT_PINHACKEXPANDHEIGHT_COARSE, false);
        } else {
            auto* opt = retro::core_options.option(CORE_OPT_PINHACKEXPANDHEIGHT_COARSE);
            opt->clearValues();
            opt->addValue({static_cast<int>(new_val), new_val.ToString()});

            if (locked_core_options.count(CORE_OPT_PINHACKEXPANDHEIGHT_COARSE) == 0) {
                opt->setDesc(fmt::format("{} Expand height", LOCKED_OPTION_DESC, opt->desc()));
                if (opt->info().empty()) {
                    opt->setInfo(LOCKED_OPTION_INFO);
                } else if (opt->info().find("\n\n") != std::string::npos) {
                    opt->setInfo(fmt::format("{}\n\n{}", opt->info(), LOCKED_OPTION_INFO));
                } else {
                    opt->setInfo(fmt::format("{}\n{}", opt->info(), LOCKED_OPTION_INFO));
                }
                locked_core_options.insert(CORE_OPT_PINHACKEXPANDHEIGHT_COARSE);
            }
        }
    } else {
        return false;
    }

    retro::core_options.updateFrontend();
    return true;
}

static auto find_existing_option_value(
    const retro::CoreOptionDefinition& core_option, const Value& new_val)
    -> const retro::CoreOptionValue*
{
    for (const auto& val : core_option) {
        if (new_val.type == Value::V_BOOL && static_cast<bool>(new_val) == val.toBool()) {
            return &val;
        }
        if (new_val.type == Value::V_INT && static_cast<int>(new_val) == val.toInt()) {
            return &val;
        }
        if (new_val.type == Value::V_STRING && static_cast<const char*>(new_val) == val.toString())
        {
            return &val;
        }
        if (new_val.ToString() == val.toString()) {
            return &val;
        }
    }
    return nullptr;
}

static auto make_locked_option_value(
    const retro::CoreOptionDefinition& core_option, const Value& new_val) -> retro::CoreOptionValue
{
    if (const auto* existing_value = find_existing_option_value(core_option, new_val)) {
        return *existing_value;
    }

    switch (new_val.type) {
    case Value::V_BOOL:
        return {static_cast<bool>(new_val), new_val.ToString()};
    case Value::V_INT:
        return {static_cast<int>(new_val), new_val.ToString()};
    case Value::V_STRING: {
        std::string val_string = static_cast<const char*>(new_val);
        if (val_string.empty()) {
            val_string = "none";
        }
        return {val_string, val_string};
    }
    default:
        return {new_val.ToString(), new_val.ToString()};
    }
}

void sync_core_opts_to_conf(const std::string& conf_prop, const Value& new_val)
{
    if (disable_core_opt_sync) {
        return;
    }

    disabled_dosbox_variables.insert(conf_prop);

    if (sync_special_option(conf_prop, new_val)) {
        return;
    }

    auto* const core_option = retro::core_options.option(conf_prop);
    if (!core_option) {
        return;
    }

    const auto& option_handling = retro::core_options[CORE_OPT_OPTION_HANDLING].toString();
    if (option_handling == "disable") {
        disabled_core_options.insert(conf_prop);
        retro::core_options.setVisible(conf_prop, false);
        return;
    } else if (option_handling != "lock") {
        retro::logError(
            "Internal error in {} line {}: invalid option handling setting.", __FILE__, __LINE__);
        return;
    }

    auto locked_value = make_locked_option_value(*core_option, new_val);
    core_option->setValues({locked_value}, locked_value);

    if (locked_core_options.count(core_option->key()) == 0) {
        core_option->setDesc(fmt::format("{} {}", LOCKED_OPTION_DESC, core_option->desc()));
        if (core_option->info().empty()) {
            core_option->setInfo(LOCKED_OPTION_INFO);
        } else if (core_option->info().find("\n\n") != std::string::npos) {
            core_option->setInfo(fmt::format("{}\n\n{}", core_option->info(), LOCKED_OPTION_INFO));
        } else {
            core_option->setInfo(fmt::format("{}\n{}", core_option->info(), LOCKED_OPTION_INFO));
        }
        locked_core_options.insert(core_option->key());
    }
    retro::core_options.updateFrontend();
}

// clang-format off

#define CYCLES_COARSE_MULTIPLIERS \
    100, \
    1000, \
    10000, \
    100000,

#define CYCLES_FINE_MULTIPLIERS \
    1, \
    10, \
    100, \
    1000, \
    10000,

#define CYCLES_VALUES 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

#define MOUSE_SPEED_FACTORS \
    { \
          1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18, \
         19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36, \
         37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54, \
         55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72, \
         73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90, \
         91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, \
        109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, \
        127 \
    }

#define PINHACK_EXPAND_FINE_VALUES \
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, \
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, \
    46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, \
    69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, \
    92, 93, 94, 95, 96, 97, 98, 99

#define CYCLES_UP_DOWN_VALUES \
    {  1,  "1%" }, {  2,  "2%" }, {  3,  "3%" }, {  4,  "4%" }, {  5,  "5%" }, {  6,  "6%" }, \
    {  7,  "7%" }, {  8,  "8%" }, {  9,  "9%" }, { 10, "10%" }, { 15, "15%" }, { 20, "20%" }, \
    { 25, "25%" }, { 30, "30%" }, { 35, "35%" }, { 40, "40%" }, { 45, "45%" }, { 50, "50%" }, \
      100,   150,   200,   250,   300,   250,   400,   450,   500,   600,   700,   800,   900, \
     1000,  1500,  2000,  2500,  3000,  3500,  4000,  4500,  5000,  6000,  7000,  8000,  9000, \
    10000, 15000, 20000, 25000, 30000,

// This isn't a macro because GCC on mingw x86 crashes when compiling it.
static const std::initializer_list<retro::CoreOptionValue> retro_keyboard_ids {
    { RETROK_UNKNOWN, "---" },
    { RETROK_1, "1" },
    { RETROK_2, "2" },
    { RETROK_3, "3" },
    { RETROK_4, "4" },
    { RETROK_5, "5" },
    { RETROK_6, "6" },
    { RETROK_7, "7" },
    { RETROK_8, "8" },
    { RETROK_9, "9" },
    { RETROK_0, "0" },
    { RETROK_a, "A" },
    { RETROK_b, "B" },
    { RETROK_c, "C" },
    { RETROK_d, "D" },
    { RETROK_e, "E" },
    { RETROK_f, "F" },
    { RETROK_g, "G" },
    { RETROK_h, "H" },
    { RETROK_i, "I" },
    { RETROK_j, "J" },
    { RETROK_k, "K" },
    { RETROK_l, "L" },
    { RETROK_m, "M" },
    { RETROK_n, "N" },
    { RETROK_o, "O" },
    { RETROK_p, "P" },
    { RETROK_q, "Q" },
    { RETROK_r, "R" },
    { RETROK_s, "S" },
    { RETROK_t, "T" },
    { RETROK_u, "U" },
    { RETROK_v, "V" },
    { RETROK_w, "W" },
    { RETROK_x, "X" },
    { RETROK_y, "Y" },
    { RETROK_z, "Z" },
    { RETROK_F1, "F1" },
    { RETROK_F2, "F2" },
    { RETROK_F3, "F3" },
    { RETROK_F4, "F4" },
    { RETROK_F5, "F5" },
    { RETROK_F6, "F6" },
    { RETROK_F7, "F7" },
    { RETROK_F8, "F8" },
    { RETROK_F9, "F9" },
    { RETROK_F10, "F10" },
    { RETROK_F11, "F11" },
    { RETROK_F12, "F12" },
    { RETROK_ESCAPE, "Esc" },
    { RETROK_TAB, "Tab" },
    { RETROK_BACKSPACE, "Backspace" },
    { RETROK_RETURN, "Return/Enter" },
    { RETROK_SPACE, "Space" },
    { RETROK_LALT, "Left Alt" },
    { RETROK_RALT, "Right Alt" },
    { RETROK_LCTRL, "Left Ctrl" },
    { RETROK_RCTRL, "Right Ctrl" },
    { RETROK_LSHIFT, "Left Shift" },
    { RETROK_RSHIFT, "Right Shift" },
    { RETROK_CAPSLOCK, "Caps Lock" },
    { RETROK_SCROLLOCK, "Scroll Lock" },
    { RETROK_NUMLOCK, "Num Lock" },
    { RETROK_MINUS, "-" },
    { RETROK_EQUALS, "=" },
    { RETROK_BACKSLASH, "\\" },
    { RETROK_LEFTBRACKET, "[" },
    { RETROK_RIGHTBRACKET, "]" },
    { RETROK_SEMICOLON, ";" },
    { RETROK_QUOTE, "'" },
    { RETROK_PERIOD, "." },
    { RETROK_COMMA, "," },
    { RETROK_SLASH, "/" },
    { RETROK_SYSREQ, "SysReq/PrintScr" },
    { RETROK_PAUSE, "Pause" },
    { RETROK_INSERT, "Insert" },
    { RETROK_HOME, "Home" },
    { RETROK_PAGEUP, "Page Up" },
    { RETROK_PAGEDOWN, "Page Down" },
    { RETROK_DELETE, "Delete" },
    { RETROK_END, "End" },
    { RETROK_LEFT, "Left" },
    { RETROK_UP, "Up" },
    { RETROK_DOWN, "Down" },
    { RETROK_RIGHT, "Right" },
    { RETROK_KP1, "Keypad 1" },
    { RETROK_KP2, "Keypad 2" },
    { RETROK_KP3, "Keypad 3" },
    { RETROK_KP4, "Keypad 4" },
    { RETROK_KP5, "Keypad 5" },
    { RETROK_KP6, "Keypad 6" },
    { RETROK_KP7, "Keypad 7" },
    { RETROK_KP8, "Keypad 8" },
    { RETROK_KP9, "Keypad 9" },
    { RETROK_KP0, "Keypad 0" },
    { RETROK_KP_DIVIDE, "Keypad /" },
    { RETROK_KP_MULTIPLY, "Keypad *" },
    { RETROK_KP_MINUS, "Keypad -" },
    { RETROK_KP_PLUS, "Keypad +" },
    { RETROK_KP_ENTER, "Keypad Enter" },
    { RETROK_KP_PERIOD, "Keypad ." },
    { RETROK_BACKQUOTE, "`" },
};

namespace retro {

CoreOptions core_options {
    "dosbox_core_",

    CoreOptionCategory {
        CORE_OPTCAT_CORE_OPTION_BEHAVIOR,
        "Core option behavior",
        "Options relating to the behavior of the core options themselves.",

        CoreOptionDefinition {
            CORE_OPT_OPTION_HANDLING,
            "Core option handling",
            "When configuring emulation settings using the loaded .conf file's INI properties or "
                "with DOSBox commands (like \"config -set\",) there will be conflicts with the "
                "core options. This setting specifies how those conflicts should be handled.\n"
                "\n"
                "Lock changed options: Lock the core option to a single value. No changes will be "
                "allowed through the core options UI. The core option becomes purely "
                "informational, simply displaying the current value that was set in the .conf file "
                "or through DOSBox commands.\n"
                "\n"
                "Disable changed options: Disable and hide the core option. Its current value will "
                "not be changed. If the frontend doesn't support option hiding, changing the "
                "option will have no effect.",
            {
                { "lock", "lock changed options" },
                { "disable", "disable changed options" },
            },
            "lock"
        },
        CoreOptionDefinition {
            CORE_OPT_LOAD_DEFAULT_CONF,
            "Always load DOSBox-core.conf",
            "Always load the DOSBox-core.conf file if it exists in the libretro saves directory. "
                "Normally, this is not done when loading a custom conf file as content. This is the "
                "equivalent to using the \"-userconf\" option with stand-alone dosbox.\n"
                "\n"
                "A DOSBox-core.conf file based on the current dosbox settings can be generated with "
                "the \"config -wcd\" command.",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_ADV_OPTIONS,
            "Show all options",
            "Show all options, including those that usually do not require changing.",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_SHOW_KB_MAP_OPTIONS,
            "Show keyboard mapping options",
            {
                true,
                false,
            },
            true
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_TIMING,
        "Timing",
        "Timing and synchronization.",

        CoreOptionDefinition {
            CORE_OPT_CORE_TIMING,
            "Frame timing mode",
            "External mode is the recommended setting. It enables the frontend to drive frame "
                "pacing. It has no input lag and allows frontend features like DRC and Frame Delay "
                "to work correctly. Internal mode runs out of sync with the frontend. This allows "
                "for 60FPS output when running 70FPS games, but input lag and stutter/judder are "
                "increased. If you have a high refresh rate display (70Hz or better,) then use "
                "external mode. It also works great with VRR (g-sync/freesync.) Internal mode is "
                "mostly only useful for 60Hz displays in order to get rid of tearing in 70FPS "
                "games.",
            {
                "external",
                { "internal", "internal (fixed 60FPS)" },
            },
            "external"
        },
        CoreOptionDefinition {
            CORE_OPT_CORE_VGA_REFRESH,
            "Override emulated video refresh rate",
            "Forces the emulated refresh rate to a specific value. This can be useful as a method "
                "of slowing down game speed without affecting audio speed. Only works in games that "
                "use vsync to limit their speed. Keep in mind that this is a hack and might break "
                "some games. Using external timing mode is recommended when enabling this."
                "\n\n"
                "Example use cases for this option are forcing Turrican 2 to run at 50Hz in order "
                "to match the speed of the original Amiga version, or forcing a 70Hz game to run at "
                "a slower 60Hz if you don't have a high refresh rate display and you don't mind the "
                "slower game speed.",
            {
                { 0, "OFF" },
                { 50, "50Hz" },
                { 51, "51Hz" },
                { 52, "52Hz" },
                { 53, "53Hz" },
                { 54, "54Hz" },
                { 55, "55Hz" },
                { 56, "56Hz" },
                { 57, "57Hz" },
                { 58, "58Hz" },
                { 59, "59Hz" },
                { 60, "60Hz" },
                { 61, "61Hz" },
                { 62, "62Hz" },
                { 63, "63Hz" },
                { 64, "64Hz" },
                { 65, "65Hz" },
                { 66, "66Hz" },
                { 67, "67Hz" },
                { 68, "68Hz" },
                { 69, "69Hz" },
                { 70, "70Hz" },
            },
            0
        },
        CoreOptionDefinition {
            CORE_OPT_FRAME_DUPING,
            "Frame duping (minor speedup)",
            "Can provide a (very) minor performance benefit by instructing the frontend to present "
                "the previous frame again without re-uploading it if there is no new frame. Some "
                "drivers might not correctly support this however. If you run into issues where "
                "you get a black screen that lasts until your next input, you can disable frame "
                "duping as a workaround.",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_THREAD_SYNC,
            "Thread synchronization method",
            "\"Wait\" is the recommended method and should work well on most systems. If for some "
                "reason it doesn't and you're seeing stutter, setting this to \"spin\" might help "
                "(or it might make it worse.) However, \"spin\" will also result in 100% usage on "
                "one of your CPU cores. This is \"idle load\" and doesn't increase CPU "
                "temperatures by much, but it will prevent the CPU from clocking down which on "
                "laptops will affect battery life. ",
            {
                "wait",
                "spin",
            },
            "wait"
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_FILE_AND_DISK,
        "Storage",
        "File and disk options.",

        CoreOptionDefinition {
            CORE_OPT_MOUNT_C_AS,
            "Mount drive C as",
            "When directly loading a DOS executable rather than a .conf file, the C drive can be "
                "mounted to be either the executable's directory, or its parent directory. For "
                "example, when loading DUKE3D.EXE that is located in a directory called DUKE3D, "
                "setting this option to \"content\" will result in DOS finding the file in "
                "C:\\DUKE3D.EXE. If this option is set to \"parent\", the file will be found in "
                "C:\\DUKE3D\\DUKE3D.EXE instead.",
            {
                { "content", "content directory" },
                { "parent", "parent directory of content" },
            },
            "content"
        },
        CoreOptionDefinition {
            CORE_OPT_DEFAULT_MOUNT_FREESIZE,
            "Free space for default-mounted drive C",
            "This is the \"-freesize\" value to use for drive C when loading a DOS executable "
                "instead of a .conf file that contains its own MOUNT command.",
            {
                { 256, "256MB" },
                { 384, "384MB" },
                { 512, "512MB" },
                { 768, "768MB" },
                { 1024, "1GB" },
                { 1280, "1.25GB" },
                { 1536, "1.5GB" },
                { 1792, "1.75GB" },
            },
            1024
        },
        CoreOptionDefinition {
            CORE_OPT_SAVE_OVERLAY,
            "Enable overlay file system (restart)",
            "Enable overlay file system to redirect filesystem changes to the save directory. "
                "Disable if you have problems starting some games.",
            {
                true,
                false,
            },
            false
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_VIDEO_EMULATION,
        "Video card",
        "Configuration of the emulated video card.",

        CoreOptionDefinition {
            CORE_OPT_MACHINE_TYPE,
            "Emulated machine (restart)",
            "The type of video hardware DOSBox will emulate.",
            {
                { "hercules", "Hercules" },
                { "cga", "CGA" },
                { "tandy", "Tandy" },
                { "pcjr", "PCjr" },
                { "ega", "EGA" },
                { "vgaonly", "VGA" },
                { "svga_s3", "SVGA (S3 Trio64)" },
                { "vesa_nolfb", "SVGA (S3 Trio64 no-line buffer hack)" },
                { "vesa_oldvbe", "SVGA (S3 Trio64 VESA 1.3)" },
                { "svga_et3000", "SVGA (Tseng Labs ET3000)" },
                { "svga_et4000", "SVGA (Tseng Labs ET4000)" },
                { "svga_paradise", "SVGA (Paradise PVGA1A)" },
            },
            "svga_s3"
        },
        CoreOptionDefinition {
            CORE_OPT_MACHINE_HERCULES_PALETTE,
            "Hercules color mode",
            "The color scheme for hercules emulation.",
            {
                { 0, "black & white" },
                { 1, "black & amber" },
                { 2, "black & green" },
            },
            0
        },
        CoreOptionDefinition {
            CORE_OPT_MACHINE_CGA_COMPOSITE_MODE,
            "CGA composite mode toggle",
            "Enable or disable CGA composite mode.",
            {
                { 0, "auto" },
                { 1, "true" },
                { 2, "false" },
            },
            0
        },
        CoreOptionDefinition {
            CORE_OPT_MACHINE_CGA_MODEL,
            "CGA model",
            "The type of CGA model in the emulated system.",
            {
                { 0, "late" },
                { 1, "early" },
            },
            0
        },
        #ifdef WITH_VOODOO
        CoreOptionDefinition {
            CORE_OPT_VOODOO,
            "3dfx Voodoo emulation (restart)",
            "This emulates the actual 3dfx hardware. This is not a glide emulator and as a result "
                "a glide wrapper is neither needed nor supported.\n"
                "Only slow (VERY slow), software-based emulation is supported at the moment. It is "
                "probably not possible to get playable speeds in most games.",
            {
            // Not implemented yet.
            #if 0
                "opengl",
            #endif
                "software",
                { false, "none" },
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_VOODOO_MEMORY_SIZE,
            "Voodoo memory size (restart)",
            "The amount of memory that the emulated Voodoo card has. 4MB is the standard memory "
                "configuration for the original Voodoo. 12MB is a non-standard configuration.",
            {
                { "standard", "4MB" },
                { "max", "12MB" },
            },
            "standard"
        },
        #endif
    },
    CoreOptionCategory {
        CORE_OPTCAT_SPECS,
        "Specs",
        "CPU and RAM specifications of the emulated DOS PC.",

        CoreOptionDefinition {
            CORE_OPT_MEMORY_SIZE,
            "Memory size (restart)",
            "The amount of memory that the emulated machine has. This value is best left at its "
                "default to avoid problems with some games, though few games might require a higher "
                "value.",
            {
                { 1, "1MB" },   { 2, "2MB" },   { 3, "3MB" },   { 4, "4MB" },   { 5, "5MB" },
                { 6, "6MB" },   { 7, "7MB" },   { 8, "8MB" },   { 9, "9MB" },   { 10, "10MB" },
                { 11, "11MB" }, { 12, "12MB" }, { 13, "13MB" }, { 14, "14MB" }, { 15, "15MB" },
                { 16, "16MB" }, { 17, "17MB" }, { 18, "18MB" }, { 19, "19MB" }, { 20, "20MB" },
                { 21, "21MB" }, { 22, "22MB" }, { 23, "23MB" }, { 24, "24MB" }, { 25, "25MB" },
                { 26, "26MB" }, { 27, "27MB" }, { 28, "28MB" }, { 29, "29MB" }, { 30, "30MB" },
                { 31, "31MB" }, { 32, "32MB" }, { 33, "33MB" }, { 34, "34MB" }, { 35, "35MB" },
                { 36, "36MB" }, { 37, "37MB" }, { 38, "38MB" }, { 39, "39MB" }, { 40, "40MB" },
                { 41, "41MB" }, { 42, "42MB" }, { 43, "43MB" }, { 44, "44MB" }, { 45, "45MB" },
                { 46, "46MB" }, { 47, "47MB" }, { 48, "48MB" }, { 49, "49MB" }, { 50, "50MB" },
                { 51, "51MB" }, { 52, "52MB" }, { 53, "53MB" }, { 54, "54MB" }, { 55, "55MB" },
                { 56, "56MB" }, { 57, "57MB" }, { 58, "58MB" }, { 59, "59MB" }, { 60, "60MB" },
                { 61, "61MB" }, { 62, "62MB" }, { 63, "63MB" },
            },
            16
        },
        CoreOptionDefinition {
            CORE_OPT_XMS,
            "XMS support",
            "Extended memory (XMS) is usually required by protected mode games.",
            {
                true,
                false,
            },
            true
        },
        CoreOptionDefinition {
            CORE_OPT_EMS,
            "EMS support",
            "Expanded memory (EMS) is needed or recommended by some older games. However, some "
                "games will not run at all with it enabled (Ultima 7, for example,) or will run "
                "better with it disabled.\n"
                "\n"
                "Mixed mode is the most compatible setting for most games that use EMS memory.",
            {
                { true, "mixed mode" },
                { "emm386", "map EMS to XMS (EMM386)" },
                { "emsboard", "emulate physical EMS memory board" },
                false,
            },
            true
        },
        CoreOptionDefinition {
            CORE_OPT_UMB,
            "UMB support",
            "The upper memory block (UMB) is usually not needed by games, but can be used to load "
                "TSR programs into it without using any of the 640KB base memory.",
            {
                true,
                false,
            },
            true
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CORE,
            "CPU core",
            "CPU core used for emulation. "
        #if defined(C_DYNREC) || defined(C_DYNAMIC_X86)
                "When set to \"auto\", the \"normal\" interpreter core will be used for real mode "
                "games, while the faster \"dynamic\" recompiler core will be used for protected "
                "mode games. The \"simple\" interpreter core is optimized for old real mode games.",
            {
                "auto",
                {
                    "dynamic",
                    "dynamic recompiler"
                #if defined(C_DYNAMIC_X86)
                    #if C_TARGETCPU == X86_64
                        " (x86-64 optimized)"
                    #elif C_TARGETCPU == X86
                        " (x86 optimized)"
                    #endif
                #elif C_TARGETCPU == X86_64 || C_TARGETCPU == X86
                    " (generic)"
                #endif
                },
                "normal",
                "simple",
            },
            "auto"
        #else
            "\"Simple\" is optimized for old real-mode games. (There are no dynamic recompiler "
                "cores available on this platform.)",
            {
                "normal",
                "simple",
            },
            "normal"
        #endif
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_TYPE,
            "CPU type",
            "Emulated CPU type. \"Auto\" is the fastest choice.",
            {
                "auto",
                "386",
                { "386_slow", "386 (slow)" },
                { "386_prefetch", "386 (prefetch queue emulation)" },
                "486",
                { "486_slow", "486 (slow)" },
                { "pentium_slow", "pentium (slow)" },
            },
            "auto"
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_MODE,
            "CPU cycles mode",
            "Method to determine the amount of emulated CPU cycles per millisecond. \"Fixed\" mode "
                "emulates the amount of cycles you have set. \"Max\" mode will emulate as many "
                "cycles as possible, depending on the limits you have set. You can configure a "
                "maximum CPU load percentage as well as a cycle amount as limits. \"Auto\" will "
                "emulate the fixed cycle amount set in the \"real mode\" cycles options when "
                "running real mode games, while for protected mode games it will switch to \"max\" "
                "mode. \"Fixed\" mode in combination with an appropriate cycle amount is the most "
                "compatible setting, as \"auto\" and \"max\" have issues on many systems.",
            {
                "auto",
                "fixed",
                "max",
            },
            "fixed"
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_MULTIPLIER_REALMODE,
            "Real mode coarse CPU cycles multiplier",
            "Multiplier for coarse CPU cycles tuning when running real mode games in \"auto\" "
                "cycles mode.",
            { CYCLES_COARSE_MULTIPLIERS },
            1000
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_REALMODE,
            "Real mode coarse CPU cycles value",
            "Value for coarse CPU cycles tuning when running real mode games in \"auto\" cycles "
                "mode.",
            { CYCLES_VALUES },
            3
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE_REALMODE,
            "Real mode fine CPU cycles multiplier",
            "Multiplier for fine CPU cycles tuning when running real mode games in \"auto\" cycles "
                "mode.",
            { CYCLES_FINE_MULTIPLIERS },
            100
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_FINE_REALMODE,
            "Real mode fine CPU cycles value",
            "Value for fine CPU cycles tuning when running real mode games in \"auto\" cycles "
                "mode.",
            { CYCLES_VALUES },
            0
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_LIMIT,
            "Max CPU cycles limit",
            "Limit the maximum amount of CPU cycles used when using \"max\" mode.",
            {
                "none",
                "10%",
                "20%",
                "30%",
                "40%",
                "50%",
                "60%",
                "70%",
                "80%",
                "90%",
                "100%",
                "105%",
            },
            "100%"
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_MULTIPLIER,
            "Coarse CPU cycles multiplier",
            "Multiplier for coarse CPU cycles tuning.",
            { CYCLES_COARSE_MULTIPLIERS },
            10000
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES,
            "Coarse CPU cycles value",
            "Value for coarse CPU cycles tuning.",
            { CYCLES_VALUES },
            1
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE,
            "Fine CPU cycles multiplier",
            "Multiplier for fine CPU cycles tuning.",
            { CYCLES_FINE_MULTIPLIERS },
            1000
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_FINE,
            "Fine CPU cycles value",
            "Value for fine CPU cycles tuning.",
            { CYCLES_VALUES },
            0
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_UP,
            "Cycle increment for Ctrl-F12",
            "Values from 100 and up are cycles. Values below 100 are percentages.",
            { CYCLES_UP_DOWN_VALUES },
            10,
        },
        CoreOptionDefinition {
            CORE_OPT_CPU_CYCLES_DOWN,
            "Cycle decrement for Ctrl-F11",
            "Values from 100 and up are cycles. Values below 100 are percentages.",
            { CYCLES_UP_DOWN_VALUES },
            20,
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_SCALING,
        "Scaling",
        "Image scaling options.",

        CoreOptionDefinition {
            CORE_OPT_ASPECT_CORRECTION,
            "Aspect ratio correction",
            "When enabled, the aspect ratio will match that of a CRT monitor. This is required for "
                "non-square pixel resolutions to look as intended. Disable this if you want "
                "unscaled square pixel aspect ratios (at the cost of a squashed or stretched "
                "image), or if the result looks clearly wrong (games that use 640x350 for example "
                "will look stretched with this enabled.)",
            {
                true,
                false,
            },
            true
        },
        CoreOptionDefinition {
            CORE_OPT_SCALER,
            "DOSBox scaler",
            "Built-in, CPU-based DOSBox scalers. These are provided here only as a last resort. "
                "You should generally set this to \"none\" and instead use the scaling options and "
                "shaders that are provided by your frontend.",
            {
                "normal2x",
                "normal3x",
                "advmame2x",
                "advmame3x",
                "advinterp2x",
                "advinterp3x",
                "hq2x",
                "hq3x",
                "2xsai",
                "super2xsai",
                "supereagle",
                "tv2x",
                "tv3x",
                "rgb2x",
                "rgb3x",
                "scan2x",
                "scan3x",
                "none",
            },
            "none"
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_INPUT,
        "Input",
        "Emulated joystick and mouse.",

        CoreOptionDefinition {
            CORE_OPT_JOYSTICK_FORCE_2AXIS,
            "Force 2-axis/2-button",
            "Normally, when only one port is assigned a joystick or gamepad, 4 axes and 4 buttons "
                "are emulated on that port. Some (usually older) games however do not work "
                "correctly without a classic 2-axis/2-button joystick.",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_JOYSTICK_TIMED,
            "Enable joystick timed intervals",
            "Enable timed intervals for joystick axes. Experiment with this option if your "
                "joystick drifts.",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_EMULATED_MOUSE_DEADZONE,
            "Gamepad emulated mouse deadzone",
            "Deadzone of the gamepad emulated mouse. Experiment with this value if the mouse "
                "cursor drifts.",
            {
                { 0, "0%" },
                { 5, "5%" },
                { 10, "10%" },
                { 15, "15%" },
                { 20, "20%" },
                { 25, "25%" },
                { 30, "30%" },
            },
            30
        },
        CoreOptionDefinition {
            CORE_OPT_MOUSE_SPEED_X,
            "Horizontal mouse speed",
            "Experiment with this value if the mouse is too fast when moving left/right.",
            MOUSE_SPEED_FACTORS,
            100
        },
        CoreOptionDefinition {
            CORE_OPT_MOUSE_SPEED_Y,
            "Vertical mouse speed",
            "Experiment with this value if the mouse is too fast when moving up/down.",
            MOUSE_SPEED_FACTORS,
            100
        },
        CoreOptionDefinition {
            CORE_OPT_MOUSE_SPEED_MULT,
            "Mouse speed multiplier",
            "Since the possible mouse speed range is 1 to 127 due to a libretro limitation, this "
                "option can be used to increase mouse speed further.",
            {
                { 1, "1x" },
                { 2, "2x" },
                { 3, "3x" },
                { 4, "4x" },
                { 5, "5x" },
            },
            1
        },
        CoreOptionDefinition {
            CORE_OPT_MOUSE_SPEED_HACK,
            "Vertical mouse sensitivity correction",
            "A hack that modifies vertical sensitivity depending on the current video mode. Try "
                "enabling this for games that switch between different video modes and result in "
                "inconsistent vertical mouse speed.",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_MOUSE_SPEED_CLAMP,
            "Clamp minimum mouse speed",
            "Forces very slow mouse movements to be registered as slightly faster. This can be "
                "useful in games that don't register mouse movements that are slower than a "
                "certain threshold. Flight of the Amazon Queen is an example of such a game. Do "
                "not enable this option in games that don't have this problem. It makes mouse "
                "movement worse.",
            {
                true,
                false,
            },
            false
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_VKBD,
        "Virtual keyboard",
        "On-screen virtual keyboard.",

        CoreOptionDefinition {
            CORE_OPT_VKBD_ENABLED,
            "Virtual Keyboard Support",
            {
                true,
                false,
            },
            true,
        },
        CoreOptionDefinition {
            CORE_OPT_VKBD_THEME,
            "Color theme",
            {
                { "light", "Light (shadow)" },
                { "light_outline", "Light (outline)" },
                { "dark", "Dark (shadow)" },
                { "dark_outline", "Dark (outline)" },
            },
            "light",
        },
        CoreOptionDefinition {
            CORE_OPT_VKBD_TRANSPARENCY,
            "Transparency",
            {
                "0%",
                "25%",
                "50%",
                "75%",
                "100%",
            },
            "25%",
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_PAD0_KB_MAPPINGS,
        "Gamepad/Joystick 1 keyboard mappings",
        "It's impossible to map \"Gamepad\" or \"Joystick\" port inputs to keyboard keys using the "
            "frontend's UI. You need to use these core options instead.",

        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_UP,
            "(J1) D-Pad Up",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_DOWN,
            "(J1) D-Pad Down",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_LEFT,
            "(J1) D-Pad Left",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_RIGHT,
            "(J1) D-Pad Right",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_B,
            "(J1) B",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_A,
            "(J1) A",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_Y,
            "(J1) Y",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_X,
            "(J1) X",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_SELECT,
            "(J1) Select",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_START,
            "(J1) Start",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_LBUMP,
            "(J1) Left Bumper",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_RBUMP,
            "(J1) Right Bumper",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_LTRIG,
            "(J1) Left Trigger",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_RTRIG,
            "(J1) Right Trigger",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_LTHUMB,
            "(J1) Left Thumb",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_RTHUMB,
            "(J1) Right Thumb",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_LAUP,
            "(J1) Left Analog Up",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_LADOWN,
            "(J1) Left Analog Down",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_LALEFT,
            "(J1) Left Analog Left",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_LARIGHT,
            "(J1) Left Analog Right",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_RAUP,
            "(J1) Right Analog Up",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_RADOWN,
            "(J1) Right Analog Down",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_RALEFT,
            "(J1) Right Analog Left",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD0_MAP_RARIGHT,
            "(J1) Right Analog Right",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_PAD1_KB_MAPPINGS,
        "Gamepad/Joystick 2 keyboard mappings",
        "It's impossible to map \"Gamepad\" or \"Joystick\" port inputs to keyboard keys using the "
            "frontend's UI. You need to use these core options instead.",

        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_UP,
            "(J2) D-Pad Up",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_DOWN,
            "(J2) D-Pad Down",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_LEFT,
            "(J2) D-Pad Left",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_RIGHT,
            "(J2) D-Pad Right",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_B,
            "(J2) B",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_A,
            "(J2) A",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_Y,
            "(J2) Y",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_X,
            "(J2) X",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_SELECT,
            "(J2) Select",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_START,
            "(J2) Start",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_LBUMP,
            "(J2) Left Bumper",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_RBUMP,
            "(J2) Right Bumper",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_LTRIG,
            "(J2) Left Trigger",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_RTRIG,
            "(J2) Right Trigger",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_LTHUMB,
            "(J2) Left Thumb",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_RTHUMB,
            "(J2) Right Thumb",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_LAUP,
            "(J2) Left Analog Up",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_LADOWN,
            "(J2) Left Analog Down",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_LALEFT,
            "(J2) Left Analog Left",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_LARIGHT,
            "(J2) Left Analog Right",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_RAUP,
            "(J2) Right Analog Up",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_RADOWN,
            "(J2) Right Analog Down",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_RALEFT,
            "(J2) Right Analog Left",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
        CoreOptionDefinition {
            CORE_OPT_PAD1_MAP_RARIGHT,
            "(J2) Right Analog Right",
            retro_keyboard_ids,
            RETROK_UNKNOWN,
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_SOUND_CARD,
        "Sound",
        "Emulated audio device parameters.",

        CoreOptionDefinition {
            CORE_OPT_SBLASTER_TYPE,
            "SoundBlaster type",
            "Type of emulated SoundBlaster card.",
            {
                { "sb1", "SoundBlaster 1.0" },
                { "sb2", "SoundBlaster 2.0" },
                { "sbpro1", "SoundBlaster Pro" },
                { "sbpro2", "SoundBlaster Pro 2" },
                { "sb16", "SoundBlaster 16" },
                { "gb", "GameBlaster" },
                { "none", "none" },
            },
            "sb16"
        },
        CoreOptionDefinition {
            CORE_OPT_SBLASTER_BASE,
            "SoundBlaster Base Address",
            "The I/O address for the emulated SoundBlaster card.",
            {
                "220",
                "240",
                "260",
                "280",
                "2a0",
                "2c0",
                "2e0",
                "300",
            },
            "220"
        },
        CoreOptionDefinition {
            CORE_OPT_SBLASTER_IRQ,
            "SoundBlaster IRQ Number",
            "The IRQ number for the emulated SoundBlaster card.",
            {
                3,
                5,
                7,
                9,
                10,
                11,
                12,
            },
            7
        },
        CoreOptionDefinition {
            CORE_OPT_SBLASTER_DMA,
            "SoundBlaster DMA Number",
            "The DMA number for the emulated SoundBlaster card.",
            {
                0,
                1,
                3,
                5,
                6,
                7,
            },
            1
        },
        CoreOptionDefinition {
            CORE_OPT_SBLASTER_HDMA,
            "SoundBlaster High DMA Number",
            "The High DMA number for the emulated SoundBlaster card.",
            {
                0,
                1,
                3,
                5,
                6,
                7,
            },
            5
        },
        CoreOptionDefinition {
            CORE_OPT_SBMIXER,
            "SoundBlaster mixer",
            "This exposes the DOSBox mixer to games as a SoundBlaster mixer. Disable this if you "
                "don't want games to be able to override your custom mixer volume levels.",
            {
                true,
                false,
            },
            true
        },
        CoreOptionDefinition {
            CORE_OPT_SBLASTER_OPL_MODE,
            "SoundBlaster OPL mode",
            "The SoundBlaster emulated OPL mode. All modes are Adlib compatible except cms.",
            {
                { "auto", "auto (select based on the SoundBlaster type)" },
                { "cms", "CMS (Creative Music System / GameBlaster)" },
                { "opl2", "OPL-2 (AdLib / OPL-2 / Yamaha 3812)" },
                { "dualopl2", "Dual OPL-2 (used by SoundBlaster Pro 1.0 for stereo sound)" },
                { "opl3", "OPL-3 (AdLib / OPL-3 / Yamaha YMF262)" },
                { "opl3gold", "OPL-3 Gold (AdLib Gold / OPL-3 / Yamaha YMF262)" },
                "none",
            },
            "auto"
        },
        CoreOptionDefinition {
            CORE_OPT_SBLASTER_OPL_EMU,
            "SoundBlaster OPL provider",
            "\"Nuked OPL3\" is a cycle-accurate OPL3 (YMF262) emulator. It offers the best "
                "quality, but is quite demanding on the CPU. \"Compat\" is the next best option. "
                "It is less accurate, but also less demanding.",
            {
                { "nuked", "Nuked OPL3" },
                "compat",
                "mame",
                "fast",
            },
            "compat"
        },
        CoreOptionDefinition {
            CORE_OPT_GUS,
            "Gravis Ultrasound support",
            "Enables Gravis Ultrasound emulation. The ULTRADIR directory is not configurable. It "
                "is always set to C:\\ULTRASND.",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_GUSBASE,
            "Ultrasound IO address",
            "The IO base address for the emulated Gravis Ultrasound card.",
            {
                "220",
                "240",
                "260",
                "280",
                "2a0",
                "2c0",
                "2e0",
                "300",
            },
            "240"
        },
        CoreOptionDefinition {
            CORE_OPT_GUSIRQ,
            "Ultrasound IRQ",
            "The IRQ number for the emulated Gravis Ultrasound card.",
            {
                3,
                5,
                7,
                9,
                10,
                11,
                12,
            },
            5
        },
        CoreOptionDefinition {
            CORE_OPT_GUSDMA,
            "Ultrasound DMA",
            "The DMA channel for the emulated Gravis Ultrasound card.",
            {
                0,
                1,
                3,
                5,
                6,
                7,
            },
            3
        },
        CoreOptionDefinition {
            CORE_OPT_PCSPEAKER,
            "Enable PC speaker",
            "Enable PC speaker emulation.",
            {
                true,
                false,
            },
            true
        },
        CoreOptionDefinition {
            CORE_OPT_TANDY,
            "Enable Tandy Sound System",
            "Enable Tandy Sound System Emulation. Auto only works if machine is set to tandy.",
            {
                "auto",
                { "on", "true" },
                { "off", "false" },
            },
            "off"
        },
        CoreOptionDefinition {
            CORE_OPT_DISNEY,
            "Enable Disney Sound Source",
            "Enable Disney Sound Source Emulation.",
            {
                true,
                false,
            },
            false
        },
    },
    CoreOptionCategory {
        CORE_OPTCAT_MIDI,
        "MIDI",
        "MIDI emulation and output.",

        CoreOptionDefinition {
            CORE_OPT_MPU_TYPE,
            "MPU-401 type",
            "Type of MPU-401 MIDI interface to emulate. \"Intelligent\" mode is the best choice.",
            {
                "intelligent",
                { "uart", "UART" },
                "none",
            },
            "intelligent"
        },
        CoreOptionDefinition {
            CORE_OPT_MIDI_DRIVER,
            "MIDI driver",
            "The MT-32 emulation driver uses Munt and needs the correct ROMs in the frontend's "
                "system directory.\n"
            #ifdef WITH_BASSMIDI
                "For BASSMIDI, you need to download the BASS and BASSMIDI libraries for your OS "
                "from https://www.un4seen.com and place them in the frontend's system directory.\n"
            #endif
                "The libretro driver forwards MIDI to the frontend, in which case you need to "
                "configure MIDI output there.",
            {
            #ifdef HAVE_ALSA
                { "alsa", "ALSA" },
            #endif
            #ifdef __WIN32__
                { "win32", "Windows MIDI" },
            #endif
            #ifdef WITH_BASSMIDI
                { "bassmidi", "BASSMIDI" },
            #endif
            #ifdef WITH_FLUIDSYNTH
                { "fluidsynth", "FluidSynth" },
            #endif
                { "mt32", "MT-32 emulator" },
                "libretro",
                "none",
            },
            "none"
        },
        #ifdef HAVE_ALSA
        CoreOptionDefinition {
            CORE_OPT_MIDI_PORT,
            "ALSA MIDI port",
            "ALSA port to send MIDI to.",
            {
                { "auto gs gm", "Autodetect GS port (use GM if GS is not found)" },
                { "auto gm", "Autodetect GM port" },
                { "auto mt32", "Autodetect MT-32 port" },
                { "auto xg", "Autodetect XG port" },
                { "auto gm2", "Autodetect GM2 port" },
            },
            "auto gs gm"
        },
        #endif
        #ifdef __WIN32__
        CoreOptionDefinition {
            CORE_OPT_MIDI_PORT,
            "Windows MIDI port",
            "Windows port to send MIDI to."
            // No values. We detect and set MIDI ports at runtime.
        },
        #endif
        #ifdef WITH_BASSMIDI
        CoreOptionDefinition {
            CORE_OPT_BASSMIDI_SOUNDFONT,
            "BASSMIDI soundfont",
            "Soundfonts are looked for in the \"soundfonts\" directory inside the frontend's "
                "system directory. Supported formats are SF2 and SFZ.",
            // No values. We scan for soundfonts at runtime.
        },
        CoreOptionDefinition {
            CORE_OPT_BASSMIDI_SFVOLUME,
            "BASSMIDI soundfont volume",
            {
                "0.0",
                "0.1",
                "0.2",
                "0.3",
                "0.4",
                "0.5",
                "0.6",
                "0.7",
                "0.8",
                "0.9",
                "1.0",
                "1.1",
                "1.2",
                "1.3",
                "1.4",
                "1.5",
                "1.6",
                "1.7",
                "1.8",
                "1.9",
                "2.0",
                "2.5",
                "3.0",
                "3.5",
                "4.0",
                "4.5",
                "5.0",
                "6.0",
                "7.0",
                "8.0",
                "9.0",
                "10.0",
            },
            "0.6",
        },
        CoreOptionDefinition {
            CORE_OPT_BASSMIDI_VOICES,
            "BASSMIDI voice count",
            "Maximum number of samples that can play together. This is not the same thing as the "
                "maximum number of notes; multiple samples may be played for a single note. ",
            {
                20,
                30,
                40,
                50,
                60,
                70,
                80,
                90,
                100,
                120,
                140,
                160,
                180,
                200,
                250,
                300,
                350,
                400,
                450,
                500,
                600,
                700,
                800,
                900,
                1000,
            },
            100
        },
        #endif
        #ifdef WITH_FLUIDSYNTH
        CoreOptionDefinition {
            CORE_OPT_FLUID_SOUNDFONT,
            "FluidSynth soundfont",
            "Soundfonts are looked for in the \"soundfonts\" directory inside the frontend's "
                "system directory. Supported formats are SF2, SF3, DLS and GIG. SF2 and SF3 are "
                "the recommended formats.",
            // No values. We scan for soundfonts at runtime.
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_SAMPLERATE,
            "FluidSynth sample rate",
            "The sample rate of the audio generated by the synthesizer.",
            {
                { 8000, "8kHz" },
                { 11025, "11.025kHz" },
                { 16000, "16kHz" },
                { 22050, "22.05kHz" },
                { 32000, "32kHz" },
                { 44100, "44.1kHz" },
                { 48000, "48kHz" },
                { 96000, "96kHz" },
            },
            44100
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_GAIN,
            "FluidSynth volume gain",
            "The volume gain is applied to the final output of the synthesizer. Usually this needs "
                "to be rather low (0.2-0.5) for most soundfonts to avoid audio clipping and "
                "distortion. ",
            {
                "0.0",
                "0.1",
                "0.2",
                "0.3",
                "0.4",
                "0.5",
                "0.6",
                "0.7",
                "0.8",
                "0.9",
                "1.0",
                "1.1",
                "1.2",
                "1.3",
                "1.4",
                "1.5",
                "1.6",
                "1.7",
                "1.8",
                "1.9",
                "2.0",
                "2.5",
                "3.0",
                "3.5",
                "4.0",
                "4.5",
                "5.0",
                "6.0",
                "7.0",
                "8.0",
                "9.0",
                "10.0",
            },
            "0.4",
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_POLYPHONY,
            "FluidSynth polyphony",
            "The polyphony defines how many voices can be played in parallel. Higher values are "
                "more CPU intensive.",
            {
                1,
                2,
                4,
                8,
                16,
                32,
                64,
                128,
                256,
                384,
                512,
                768,
                1024,
                1536,
                2048,
                3072,
                4096,
            },
            256
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_CORES,
            "FluidSynth CPU cores",
            "Sets the number of synthesis CPU cores. If set to a value greater than 1, then "
                "additional synthesis threads will be created to take advantage of a multi CPU or "
                "CPU core system.",
            {
                1,
                2,
                3,
                4,
                5,
                6,
                7,
                8,
                9,
                10,
                11,
                12,
                13,
                14,
                15,
                16,
                20,
                24,
                32
            },
            1
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_REVERB,
            "FluidSynth enable reverb",
            {
                true,
                false,
            },
            true
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_REVERB_ROOMSIZE,
            "FluidSynth reverb room size",
            {
                "0.0",
                "0.1",
                "0.2",
                "0.3",
                "0.4",
                "0.5",
                "0.6",
                "0.7",
                "0.8",
                "0.9",
                "1.0",
            },
            "0.2"
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_REVERB_DAMPING,
            "FluidSynth reverb damping",
            {
                "0.0",
                "0.1",
                "0.2",
                "0.3",
                "0.4",
                "0.5",
                "0.6",
                "0.7",
                "0.8",
                "0.9",
                "1.0",
            },
            "0.0"
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_REVERB_WIDTH,
            "FluidSynth reverb width",
            {
                "0.0",
                "0.1",
                "0.2",
                "0.3",
                "0.4",
                "0.5",
                "0.6",
                "0.7",
                "0.8",
                "0.9",
                "1.1",
                "1.2",
                "1.3",
                "1.4",
                "1.5",
                "1.6",
                "1.7",
                "1.8",
                "1.9",
                "2.0",
                "2.5",
                "3.0",
                "3.5",
                "4.0",
                "4.5",
                "5.0",
                "6.0",
                "7.0",
                "8.0",
                "9.0",
                "10.0",
                "12.0",
                "14.0",
                "16.0",
                "18.0",
                "20.0",
                "25.0",
                "30.0",
                "35.0",
                "40.0",
                "45.0",
                "50.0",
                "60.0",
                "70.0",
                "80.0",
                "90.0",
                "100.0",
            },
            "0.5"
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_REVERB_LEVEL,
            "FluidSynth reverb level",
            {
                "0.0",
                "0.1",
                "0.2",
                "0.3",
                "0.4",
                "0.5",
                "0.6",
                "0.7",
                "0.8",
                "0.9",
                "1.0",
            },
            "0.9"
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_CHORUS,
            "FluidSynth enable chorus",
            {
                true,
                false,
            },
            true
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_CHORUS_NUMBER,
            "FluidSynth chorus voices",
            {
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                7,
                8,
                9,
                10,
                11,
                12,
                14,
                16,
                18,
                20,
                24,
                28,
                32,
                56,
                64,
                96,
            },
            3
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_CHORUS_LEVEL,
            "FluidSynth chorus level",
            {
                "0.0",
                "0.2",
                "0.4",
                "0.6",
                "0.8",
                "1.0",
                "1.2",
                "1.4",
                "1.6",
                "1.8",
                "2.0",
                "2.2",
                "2.4",
                "2.6",
                "2.8",
                "3.0",
                "3.5",
                "4.0",
                "4.5",
                "5.0",
                "5.5",
                "6.0",
                "6.5",
                "7.0",
                "7.5",
                "8.0",
                "8.5",
                "9.0",
                "9.5",
                "10.0",
            },
            "2.0"
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_CHORUS_SPEED,
            "FluidSynth chorus speed",
            {
                "0.1",
                "0.2",
                "0.3",
                "0.4",
                "0.5",
                "0.6",
                "0.7",
                "0.8",
                "0.9",
                "1.0",
                "1.1",
                "1.2",
                "1.3",
                "1.4",
                "1.5",
                "1.6",
                "1.7",
                "1.8",
                "1.9",
                "2.0",
                "2.1",
                "2.2",
                "2.3",
                "2.4",
                "2.5",
                "2.6",
                "2.7",
                "2.8",
                "2.9",
                "3.0",
                "3.1",
                "3.2",
                "3.3",
                "3.4",
                "3.5",
                "3.6",
                "3.7",
                "3.8",
                "3.9",
                "4.0",
                "4.1",
                "4.2",
                "4.3",
                "4.4",
                "4.5",
                "4.6",
                "4.7",
                "4.8",
                "4.9",
                "5.0",
            },
            "0.3"
        },
        CoreOptionDefinition {
            CORE_OPT_FLUID_CHORUS_DEPTH,
            "FluidSynth chorus depth",
            {
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                7,
                8,
                9,
                10,
                11,
                12,
                13,
                14,
                15,
                16,
                18,
                20,
                22,
                24,
                28,
                32,
                48,
                64,
                80,
                96,
                112,
                128,
                160,
                192,
                224,
                256,
            },
            8
        },
        #endif
        CoreOptionDefinition {
            CORE_OPT_MT32_TYPE,
            "MT-32 hardware type",
            "Type of MT-32 module to emulate. MT-32 is the older, original model. The CM-32L "
                "and LAPC-I are later models that provide some extra instruments not found on the "
                "original MT-32. Some games make use of these extra sounds and won't sound correct "
                "on the original MT-32.",
            {
                { "mt32", "MT-32" },
                { "cm32l", "CM-32L/LAPC-I" },
            },
            "cm32l"
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_REVERSE_STEREO,
            "MT-32 reverse stereo channels",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_THREAD,
            "MT-32 threaded emulation",
            "Run MT-32 emulation in its own thread. Improves performance on multi-core CPUs.",
            {
                true,
                false,
            },
            false
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_CHUNK,
            "MT-32 threaded chunk size",
            "Minimum milliseconds of data to render at once. Increasing this value reduces "
                "rendering overhead which may improve performance but also increases audio lag.",
            {
                { 2, "2ms" },
                { 3, "3ms" },
                { 4, "4ms" },
                { 5, "5ms" },
                { 7, "7ms" },
                { 10, "10ms" },
                { 13, "13ms" },
                { 16, "16ms" },
                { 20, "20ms" },
                { 24, "24ms" },
                { 28, "28ms" },
                { 32, "32ms" },
                { 40, "40ms" },
                { 48, "48ms" },
                { 56, "56ms" },
                { 64, "64ms" },
                { 80, "80ms" },
                { 96, "96ms" },
            },
            16
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_PREBUFFER,
            "MT-32 threaded prebuffer size",
            "How many milliseconds of data to render ahead. Increasing this value may help to "
                "avoid underruns but also increases audio lag. Cannot be set less than or equal to "
                "the chunk size value.",
            {
                { 3, "3ms" },
                { 4, "4ms" },
                { 5, "5ms" },
                { 7, "7ms" },
                { 10, "10ms" },
                { 13, "13ms" },
                { 16, "16ms" },
                { 20, "20ms" },
                { 24, "24ms" },
                { 28, "28ms" },
                { 32, "32ms" },
                { 40, "40ms" },
                { 48, "48ms" },
                { 56, "56ms" },
                { 64, "64ms" },
                { 80, "80ms" },
                { 96, "96ms" },
                { 112, "112ms" },
                { 128, "128ms" },
                { 144, "144ms" },
                { 160, "160ms" },
                { 176, "176ms" },
                { 192, "192ms" },
            },
            32
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_PARTIALS,
            "MT-32 max partials",
            "The maximum number of partials playing simultaneously. A value of 32 matches real "
                "MT-32 hardware. Lowering this value increases performance at the cost of notes "
                "getting cut off sooner. Increasing it allows more notes to stay audible compared "
                "to real hardware at the cost of performance.",
            {
                8,
                9,
                10,
                11,
                12,
                14,
                16,
                20,
                24,
                28,
                32,
                40,
                48,
                56,
                64,
                72,
                80,
                96,
                112,
                128,
                144,
                160,
                176,
                192,
                224,
                256,
            },
            32
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_DAC,
            "MT-32 DAC input emulation mode",
            "High quality: Produces samples at double the volume, without tricks. Higher quality "
                "than the real devices.\n"
                "Pure: Produces samples that exactly match the bits output from the emulated LA32. "
                "Nicer overdrive characteristics than the DAC hacks (it simply clips samples "
                "within range.) Much less likely to overdrive than any other mode. Half the volume "
                "of any of the other modes.\n"
                "Gen 1: Re-orders the LA32 output bits as in the early generation MT-32.\n"
                "Gen 2: Re-orders the LA32 output bits as in the later generations MT-32 and "
                "CM-32Ls.",
            {
                { 0, "high quality" },
                { 1, "pure" },
                { 2, "gen 1" },
                { 3, "gen 2" },
            },
            0
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_ANALOG,
            "MT-32 analog output emulation mode",
            "Digital: Only digital path is emulated. The output samples correspond to the digital "
                "output signal appeared at the DAC entrance. Fastest mode.\n"
                "Coarse: Coarse emulation of LPF circuit. High frequencies are boosted, sample "
                "rate remains unchanged. A bit better sounding but also a bit slower.\n"
                "Accurate: Finer emulation of LPF circuit. Output signal is upsampled to 48 kHz to "
                "allow emulation of audible mirror spectra above 16 kHz, which is passed through "
                "the LPF circuit without significant attenuation. Sounding is closer to the analog "
                "output from real hardware but also slower than \"digital\" and \"coarse\".\n"
                "Oversampled: Same as \"accurate\" but the output signal is 2x oversampled, i.e. "
                "the output sample rate is 96 kHz. Even slower than all the other modes but better "
                "retains highest frequencies while further resampled in DOSBox mixer.",
            {
                { 0, "digital" },
                { 1, "coarse" },
                { 2, "accurate" },
                { 3, "oversampled" },
            },
            2
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_REVERB_MODE,
            "MT-32 reverb mode",
            "Reverb emulation mode. \"Auto\" will automatically adjust reverb parameters to match "
                "the loaded control ROM version.",
            {
                { "auto" },
                { 0, "room" },
                { 1, "hall" },
                { 2, "plate" },
                { 3, "tap delay" },
            },
            "auto"
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_REVERB_TIME,
            "MT-32 reverb decay time",
            {
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                7,
            },
            5
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_REVERB_LEVEL,
            "MT-32 reverb level",
            {
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                7,
            },
            3
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_RATE,
            "MT-32 sample rate",
            {
                { 8000, "8kHz" },
                { 11025, "11.025kHz" },
                { 16000, "16kHz" },
                { 22050, "22.05kHz" },
                { 32000, "32kHz" },
                { 44100, "44.1kHz" },
                { 48000, "48kHz" },
                { 49716, "49.716kHz" },
            },
            44100
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_SRC_QUALITY,
            "MT-32 resampling quality",
            {
                { 0, "fastest" },
                { 1, "fast" },
                { 2, "good" },
                { 3, "best" },
            },
            2
        },
        CoreOptionDefinition {
            CORE_OPT_MT32_NICEAMPRAMP,
            "MT-32 nice amp ramp",
            "Improves amplitude ramp for sustaining instruments. Quick changes of volume or "
                "expression on a MIDI channel may result in amp jumps on real hardware. Enabling "
                "this option prevents this from happening. Disabling this options preserves "
                "emulation accuracy.",
            {
                true,
                false,
            },
            true
        },
    },
    CoreOptionDefinition {
        CORE_OPT_IPX,
        "Enable IPX networking",
        "Enable IPX over UDP tunneling.",
        {
            true,
            false,
        },
        false
    },
    #ifdef WITH_PINHACK
    CoreOptionCategory {
        CORE_OPTCAT_PINHACK,
        "Pinhack",
        "Pinhack configuration options. Pinhack is a no-scroll hack for some pinball games.",

        CoreOptionDefinition {
            CORE_OPT_PINHACK,
            "Pinhack",
            "A hack that allows some pinball games to display the whole table without scrolling. "
                "Do not enable this unless you're playing a game that works with it. It will cause "
                "severe issues with other games. See https://github.com/DeXteRrBDN/dosbox-pinhack "
                "for more information.",
            {
                { true, "on" },
                { false, "off" },
            },
            false,
        },
        CoreOptionDefinition {
            CORE_OPT_PINHACKACTIVE,
            "Initial mode",
            "Whether or not to start with pinhack toggled on or off. It can be toggled on and off "
                "at any point with the Insert key.",
            {
                { true, "activated" },
                { false, "deactivated" },
            },
            false,
        },
        CoreOptionDefinition {
            CORE_OPT_PINHACKTRIGGERWIDTH,
            "Horizontal trigger range",
            "The horizontal resolution range the pinball hack should trigger at. Usually not needed.",
            {
                { 0, "disabled" },
                "300-310",
                "311-320",
                "321-330",
                "331-340",
                "341-350",
                "351-360",
                "361-370",
                "371-380",
                "381-390",
                "391-400",
                "401-410",
                "411-420",
                "421-430",
                "431-440",
                "441-450",
                "451-460",
                "461-470",
                "471-480",
                "481-490",
                "491-500",
                "501-510",
                "511-520",
                "521-530",
                "531-540",
                "541-550",
                "551-560",
                "561-570",
                "571-580",
                "581-590",
                "591-600",
                "601-610",
                "611-620",
                "621-630",
                "631-640",
            },
            0
        },
        CoreOptionDefinition {
            CORE_OPT_PINHACKTRIGGERHEIGHT,
            "Vertical trigger range",
            "The vertical resolution range the pinball hack should trigger at.",
            {
                0,
                "200-210",
                "211-220",
                "221-230",
                "231-240",
                "241-250",
                "251-260",
                "261-270",
                "271-280",
                "281-290",
                "291-300",
                "301-310",
                "311-320",
                "321-330",
                "331-340",
                "341-350",
                "351-360",
                "361-370",
                "371-380",
                "381-390",
                "391-400",
                "401-410",
                "411-420",
                "421-430",
                "431-440",
                "441-450",
                "451-460",
                "461-470",
                "471-480",
            },
            "231-240"
        },
        CoreOptionDefinition {
            CORE_OPT_PINHACKEXPANDHEIGHT_COARSE,
            "Coarse expand height",
            "The coarse vertical resolution to expand the game to. You need the correct value for "
                "each individual game.",
            {
                { 0, "disabled" },
                300,
                400,
                500,
                600,
                700,
                800,
                900,
            },
            600
        },
        CoreOptionDefinition {
            CORE_OPT_PINHACKEXPANDHEIGHT_FINE,
            "Fine expand height",
            "Combine this with the coarse expand height to get a final value. For example setting "
                "coarse to 600 and fine to 9 will result in an expand height of 609 (Pinball Fantasies).",
            {
                PINHACK_EXPAND_FINE_VALUES
            },
            9
        },
    },
    #endif
    CoreOptionCategory {
        CORE_OPTCAT_LOGGING,
        "Logging",
        "Event logging.",

        CoreOptionDefinition {
            CORE_OPT_LOG_METHOD,
            "Output method",
            "Where to send log output. \"Frontend\" will send it to the frontend, while "
                "\"stdout/stderr\" will print it to standard output or standard error (warnings "
                "and errors go to stderr, debug and informational messages to stdout.)",
            {
                "frontend",
                "stdout/stderr",
            },
            "frontend",
        },
        CoreOptionDefinition {
            CORE_OPT_LOG_LEVEL,
            "Verbosity level",
            {
                "debug",
                "info",
                "warnings",
                "errors",
            },
            "warnings",
        },
    },
};

// clang-format on

} // namespace retro

/*

Copyright (C) 2015-2018 Andrés Suárez
Copyright (C) 2019 Nikos Chantziaras.

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
