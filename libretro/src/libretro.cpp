// This is copyrighted software. More information is at the end of this file.
#include "libretro.h"
#include <streams/file_stream.h>
#include "CoreOptions.h"
#include "control.h"
#include "deps/char8_t-remediation/char8_t-remediation.h"
#include "disk_control.h"
#include "dos/drives.h"
#include "dos_inc.h"
#include "callback.h"
#include "automap.h"
#include "cross.h"
#include <stdexcept>
#include "dosbox.h"
#include "emu_thread.h"
#include "fake_timing.h"
#include "ints/int10.h"
#include "libretro-vkbd.h"
#include "libretro_audio.h"
#include "libretro_core_options.h"
#include "libretro_dosbox.h"
#include "libretro_gfx.h"
#include "libretro_input.h"
#include "libretro_message.h"
#include "log.h"
#include "mixer.h"
#include "pic.h"
#include "programs.h"
#include "render.h"
#include "setup.h"
#include "util.h"
#ifdef ANDROID
    #include "nonlibc.h"
#endif
#if HAVE_ALSA
    #include "midi_alsa.h"
#endif
#ifdef _WIN32
    #include "midi_win32.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <thread>
#ifdef _WIN32
    #include <direct.h>
#else
    #include <unistd.h>
#endif

#define RETRO_DEVICE_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)

#define CORE_VERSION SVN_VERSION " " GIT_VERSION

#ifdef WITH_FAKE_SDL
bool startup_state_capslock;
bool startup_state_numlock;
#endif

static constexpr float internal_sync_fps = 60.0f;

bool autofire;
static bool dosbox_initialiazed = false;

std::set<std::string> disabled_dosbox_variables;
std::set<std::string> disabled_core_options;

extern Bit8u herc_pal;
MachineType machine = MCH_VGA;
SVGACards svgaCard = SVGA_None;

/* input variables */
std::array<bool, RETRO_INPUT_PORTS_MAX> gamepad{}; // True means gamepad, false means joystick.
std::array<bool, RETRO_INPUT_PORTS_MAX> connected;
static bool force_2axis_joystick = false;
int mouse_emu_deadzone = 0;
float mouse_speed_factor_x = 1.0;
float mouse_speed_factor_y = 1.0;
float mouse_speed_hack_factor = 1.0;
static bool enable_mouse_speed_clamp = false;

/* core option variables */
bool run_synced = true;
static bool use_frame_duping = true;
static bool use_spinlock = false;

/* directories */
std::filesystem::path retro_save_directory;
std::filesystem::path retro_system_directory;
std::filesystem::path load_game_directory;
static std::filesystem::path retro_content_directory;
static const std::string retro_library_name = "DOSBox";

/* libretro variables */
static retro_video_refresh_t video_cb;
retro_input_poll_t poll_cb;
retro_input_state_t input_cb;
retro_environment_t environ_cb;
retro_perf_callback perf_cb;

/* DOSBox state */
static std::filesystem::path game_path;
static std::filesystem::path config_path;
bool dosbox_exit;
bool frontend_exit;
static bool fast_forward_status = false;

/* audio variables */
struct retro_midi_interface retro_midi_interface;
bool use_retro_midi = false;
bool have_retro_midi = false;
#ifdef HAVE_ALSA
static auto alsa_midi_ports = getAlsaMidiPorts();
#endif
bool disney_init;

/* callbacks */
void retro_set_video_refresh(retro_video_refresh_t cb)
{
    video_cb = cb;
}
void retro_set_input_poll(retro_input_poll_t cb)
{
    poll_cb = cb;
}
void retro_set_input_state(retro_input_state_t cb)
{
    input_cb = cb;
}

// Pending overlay mount.
static bool mount_overlay = true;

// Whether directory content has been looked up for automatic gamepad mapping.
static bool automap_checked = false;

// ZIP content, mounted as C: by MOUNTZIP.COM once DOS is up. Nothing is
// extracted: the archive is a read-only zipDrive, and a unionDrive over it
// keeps everything the game writes in a save file of its own.
static std::filesystem::path zip_content_path;

// Thread we run dosbox in.
static std::thread emu_thread;

/* helper functions */

auto retro_ticks() -> long
{
    return perf_cb.get_time_usec ? perf_cb.get_time_usec() : 0;
}

static void mount_overlay_filesystem(const char drive, std::filesystem::path path)
{
    // Make sure the path ends with a dir separator, otherwise dosbox will glitch out when writing
    // to the overlay.
    path.make_preferred();
    auto path_str = from_u8string(path.u8string());
    if (path_str.back() != std::filesystem::path::preferred_separator) {
        path_str += std::filesystem::path::preferred_separator;
    }

    retro::logDebug("Mounting {} in {} as overlay.", path_str, drive);

    if (!Drives[drive - 'A']) {
        retro::showOsdError(
            fmt::format("Failed to mount overlay because drive {} is not mounted.", drive),
            RETRO_MESSAGE_TYPE_NOTIFICATION);
        return;
    }

    auto* base_drive = dynamic_cast<localDrive*>(Drives[drive - 'A']);
    if (!base_drive || dynamic_cast<cdromDrive*>(base_drive)
        || dynamic_cast<Overlay_Drive*>(base_drive))
    {
        retro::showOsdError(
            fmt::format("Failed to mount overlay because base drive {} is not compatible.", drive),
            RETRO_MESSAGE_TYPE_NOTIFICATION);
        return;
    }

    retro::logDebug("Creating save directory {}.", path_str);
    try {
        if (host_is_vfs_path(path_str.c_str())) {
            // std::filesystem cannot see a frontend-only path - it would create a local directory
            // named after the scheme - so make each level through the VFS instead.
            const auto after_scheme = path_str.find("://") + 3;
            for (auto sep = path_str.find('/', after_scheme); sep != std::string::npos;
                 sep = path_str.find('/', sep + 1)) {
                host_mkdir(path_str.substr(0, sep).c_str());
            }
            struct stat st;
            if (host_stat(path_str.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                throw std::runtime_error("cannot create it through the frontend VFS");
            }
        } else {
            std::filesystem::create_directories(path);
        }
    }
    catch (const std::exception& e) {
        retro::showOsdError(
            fmt::format("Error creating overlay directory {}: {}.", path_str, e.what()),
            RETRO_MESSAGE_TYPE_NOTIFICATION);
        return;
    }

    // Give the overlay the same size as the base drive.
    Bit16u bytes_per_sector;
    Bit8u sectors_per_cluster;
    Bit16u total_clusters;
    Bit16u free_clusters;
    base_drive->AllocationInfo(
        &bytes_per_sector, &sectors_per_cluster, &total_clusters, &free_clusters);
    Bit8u o_error = 0;
    auto overlay = std::make_unique<Overlay_Drive>(
        base_drive->getBasedir(), path_str.c_str(), bytes_per_sector, sectors_per_cluster,
        total_clusters, free_clusters, 0xF8, o_error);
    if (o_error != 0) {
        if (o_error == 1) {
            retro::showOsdError(
                "Failed to mount overlay: can't mix absolute and relative paths.",
                RETRO_MESSAGE_TYPE_NOTIFICATION);

        } else if (o_error == 2) {
            retro::showOsdError(
                "Failed to mount overlay: overlay can't be in the same underlying file system.",
                RETRO_MESSAGE_TYPE_NOTIFICATION);
        } else {
            retro::showOsdError(
                fmt::format("Something went wrong while mounting overlay. Error code: {}", o_error),
                RETRO_MESSAGE_TYPE_NOTIFICATION);
        }
        return;
    }

    mem_writeb(Real2Phys(dos.tables.mediaid) + (drive - 'A') * 9, overlay->GetMediaByte());
    overlay->dirCache.SetLabel((drive + std::string("_OVERLAY")).c_str(), false, false);
    // Preserve current working directory if not marked as deleted.
    if (overlay->TestDir(base_drive->curdir)) {
        std::strncpy(overlay->curdir, base_drive->curdir, DOS_PATHLENGTH);
        overlay->curdir[DOS_PATHLENGTH - 1] = '\0';
    }
    Drives[drive - 'A'] = overlay.release();
    delete base_drive;
}

static auto compare_dosbox_variable(
    const std::string& section_string, const std::string& var_string, const std::string& val_string)
    -> bool
{
    bool ret = false;
    Section* section = control->GetSection(section_string);
    Section_prop* secprop = static_cast<Section_prop*>(section);
    if (secprop) {
        ret = section->GetPropValue(var_string) == val_string;
    }
    return ret;
}

auto update_dosbox_variable(
    const bool autoexec, const std::string& section_string, const std::string& var_string,
    const std::string& val_string) -> bool
{
    bool ret = false;
    if (dosbox_initialiazed && compare_dosbox_variable(section_string, var_string, val_string)) {
        return false;
    }

    if (disabled_dosbox_variables.count(var_string) != 0
        && retro::core_options[CORE_OPT_OPTION_HANDLING].toString() == "disable")
    {
        return false;
    }

    disable_core_opt_sync = true;
    Section* section = control->GetSection(section_string);
    Section_prop* secprop = static_cast<Section_prop*>(section);
    if (secprop) {
        if (!autoexec) {
            section->ExecuteDestroy(false);
        }
        std::string inputline = var_string + "=" + val_string;
        ret = section->HandleInputline(inputline.c_str());
        if (!autoexec) {
            section->ExecuteInit(false);
        }
    }
    disable_core_opt_sync = false;
    retro::logDebug("Variable {}::{} updated to {}.", section_string, var_string, val_string);
    return ret;
}

static void leave_thread(const Bitu /*val*/)
{
    switchThread();

    if (!run_synced) {
        /* Schedule the next frontend interrupt */
        PIC_AddEvent(leave_thread, 1000.0f / internal_sync_fps);
    }
}

void update_mouse_speed_fix(const int gfx_height)
{
    if (!retro::core_options[CORE_OPT_MOUSE_SPEED_HACK].toBool()) {
        mouse_speed_hack_factor = 1.0f;
        return;
    }

    switch (gfx_height) {
    case 240:
    case 480:
    case 720:
    case 960:
    case 1200:
        mouse_speed_hack_factor = 2.0f;
        break;
    default:
        mouse_speed_hack_factor = 0.85f;
    }
}

void set_mouse_speed_clamp(const bool enable)
{
    enable_mouse_speed_clamp = enable;
}

static void update_gfx_mode(const bool change_fps)
{
    retro_system_av_info new_av_info;
    bool cb_error = false;

    retro_get_system_av_info(&new_av_info);

    if (change_fps) {
        cb_error = !environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &new_av_info);
        if (cb_error) {
            retro::logError("SET_SYSTEM_AV_INFO failed.");
        }
    } else {
        cb_error = !environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &new_av_info);
        if (cb_error) {
            retro::logError("SET_GEOMETRY failed.");
        }
    }

    if (!cb_error) {
        retro::logInfo(
            "Resolution changed to {}x{} @ {:.9}Hz AR: {:.5}.", gfx::width, gfx::height,
            run_synced ? render.src.fps : internal_sync_fps, gfx::aspect_ratio);
    }

    update_mouse_speed_fix(new_av_info.geometry.base_height);
}

static RETRO_CALLCONV auto update_core_option_visibility() -> bool
{
    using namespace retro;

    bool updated = false;
    const bool show_all = core_options[CORE_OPT_ADV_OPTIONS].toBool();

    const auto& mode = core_options[CORE_OPT_CPU_CYCLES_MODE].toString();
    updated |= core_options.setVisible(CORE_OPT_CPU_CYCLES_LIMIT, mode == "max" || mode == "auto");
    updated |= core_options.setVisible(
        {CORE_OPT_CPU_CYCLES_MULTIPLIER_REALMODE, CORE_OPT_CPU_CYCLES_REALMODE,
         CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE_REALMODE, CORE_OPT_CPU_CYCLES_FINE_REALMODE},
        mode == "auto");

    const bool mpu_enabled = core_options[CORE_OPT_MPU_TYPE].toString() != "none";
    updated |= core_options.setVisible(CORE_OPT_MIDI_DRIVER, mpu_enabled);

    const auto& midi_driver = core_options[CORE_OPT_MIDI_DRIVER].toString();
#ifdef WITH_BASSMIDI
    const auto bassmidi_enabled = mpu_enabled && midi_driver == "bassmidi";
    updated |= core_options.setVisible(
        {CORE_OPT_BASSMIDI_SOUNDFONT, CORE_OPT_BASSMIDI_SFVOLUME}, bassmidi_enabled);
    updated |= core_options.setVisible(CORE_OPT_BASSMIDI_VOICES, show_all && bassmidi_enabled);
#endif

#ifdef WITH_FLUIDSYNTH
    const auto fsynth_enabled = mpu_enabled && midi_driver == "fluidsynth";
    for (const auto* name :
         {CORE_OPT_FLUID_SOUNDFONT, CORE_OPT_FLUID_GAIN, CORE_OPT_FLUID_POLYPHONY,
          CORE_OPT_FLUID_CORES})
    {
        updated |= core_options.setVisible(name, fsynth_enabled);
    }
    for (const auto* name :
         {CORE_OPT_FLUID_SAMPLERATE, CORE_OPT_FLUID_REVERB, CORE_OPT_FLUID_REVERB_ROOMSIZE,
          CORE_OPT_FLUID_REVERB_DAMPING, CORE_OPT_FLUID_REVERB_WIDTH, CORE_OPT_FLUID_REVERB_LEVEL,
          CORE_OPT_FLUID_CHORUS, CORE_OPT_FLUID_CHORUS_NUMBER, CORE_OPT_FLUID_CHORUS_LEVEL,
          CORE_OPT_FLUID_CHORUS_SPEED, CORE_OPT_FLUID_CHORUS_DEPTH})
    {
        updated |= core_options.setVisible(name, show_all && fsynth_enabled);
    }
#endif

    const auto mt32_enabled = mpu_enabled && midi_driver == "mt32";
    for (const auto* name :
         {CORE_OPT_MT32_TYPE, CORE_OPT_MT32_THREAD, CORE_OPT_MT32_PARTIALS, CORE_OPT_MT32_ANALOG})
    {
        updated |= core_options.setVisible(name, mt32_enabled);
    }
    for (const auto* name :
         {CORE_OPT_MT32_REVERSE_STEREO, CORE_OPT_MT32_DAC, CORE_OPT_MT32_REVERB_MODE,
          CORE_OPT_MT32_REVERB_TIME, CORE_OPT_MT32_REVERB_LEVEL, CORE_OPT_MT32_RATE,
          CORE_OPT_MT32_SRC_QUALITY, CORE_OPT_MT32_NICEAMPRAMP})
    {
        updated |= core_options.setVisible(name, show_all && mt32_enabled);
    }
    const bool mt32_is_threaded = core_options[CORE_OPT_MT32_THREAD].toBool();
    for (const auto* name : {CORE_OPT_MT32_CHUNK, CORE_OPT_MT32_PREBUFFER}) {
        updated |= core_options.setVisible(name, show_all && mt32_enabled && mt32_is_threaded);
    }

#ifdef HAVE_ALSA
    updated |= core_options.setVisible(CORE_OPT_MIDI_PORT, midi_driver == "alsa" && mpu_enabled);
#endif
#ifdef __WIN32__
    updated |= core_options.setVisible(CORE_OPT_MIDI_PORT, midi_driver == "win32" && mpu_enabled);
#endif

#ifdef WITH_VOODOO
    updated |= core_options.setVisible(
        CORE_OPT_VOODOO_MEMORY_SIZE, core_options[CORE_OPT_VOODOO].toString() != "false");
#endif

    const auto& machine_type = core_options[CORE_OPT_MACHINE_TYPE].toString();

    updated |= core_options.setVisible(
        {CORE_OPT_MACHINE_CGA_COMPOSITE_MODE, CORE_OPT_MACHINE_CGA_MODEL},
        show_all && machine_type == "cga");

    updated |= core_options.setVisible(
        CORE_OPT_MACHINE_HERCULES_PALETTE, show_all && machine_type == "hercules");

    const auto& sb_type = core_options[CORE_OPT_SBLASTER_TYPE].toString();
    const bool sb_enabled = sb_type != "none";
    const bool sb_is_gameblaster = sb_type == "gb";
    const bool sb_is_sb16 = sb_type == "sb16";
    updated |= core_options.setVisible(
        {CORE_OPT_SBLASTER_BASE, CORE_OPT_SBMIXER, CORE_OPT_SBLASTER_OPL_MODE,
         CORE_OPT_SBLASTER_OPL_EMU},
        show_all && sb_enabled);
    updated |= core_options.setVisible(
        {CORE_OPT_SBLASTER_IRQ, CORE_OPT_SBLASTER_DMA},
        show_all && sb_enabled && !sb_is_gameblaster);
    updated |= core_options.setVisible(CORE_OPT_SBLASTER_HDMA, show_all && sb_is_sb16);

    auto gus_enabled = core_options[CORE_OPT_GUS].toBool();
    updated |= core_options.setVisible(
        {CORE_OPT_GUSBASE, CORE_OPT_GUSIRQ, CORE_OPT_GUSDMA}, show_all && gus_enabled);

    updated |= core_options.setVisible(
        {CORE_OPT_DEFAULT_MOUNT_FREESIZE, CORE_OPT_THREAD_SYNC, CORE_OPT_CPU_TYPE, CORE_OPT_SCALER,
         CORE_OPT_MPU_TYPE, CORE_OPT_TANDY, CORE_OPT_DISNEY, CORE_OPT_LOG_METHOD,
         CORE_OPT_LOG_LEVEL},
        show_all);

#ifdef WITH_PINHACK
    const auto pinhack_enabled = core_options[CORE_OPT_PINHACK].toBool();
    const bool pinhack_expand_height_enabled =
        core_options[CORE_OPT_PINHACKEXPANDHEIGHT_COARSE].toInt() != 0;
    updated |= core_options.setVisible(
        {CORE_OPT_PINHACKACTIVE, CORE_OPT_PINHACKTRIGGERWIDTH, CORE_OPT_PINHACKTRIGGERHEIGHT,
         CORE_OPT_PINHACKEXPANDHEIGHT_COARSE},
        pinhack_enabled);
    updated |= core_options.setVisible(
        CORE_OPT_PINHACKEXPANDHEIGHT_FINE, pinhack_enabled && pinhack_expand_height_enabled);
#endif

    static const std::regex pad_map_regex{"^pad(0|1)_map_"};
    updated |=
        core_options.setVisible(pad_map_regex, core_options[CORE_OPT_SHOW_KB_MAP_OPTIONS].toBool());

    updated |= core_options.setVisible(
        CORE_OPT_LOG_LEVEL, core_options[CORE_OPT_LOG_METHOD].toString() == "stdout/stderr");

    for (const auto& option_name : disabled_core_options) {
        if (core_options.option(option_name)) {
            updated |= core_options.setVisible(option_name, false);
        }
    }
    return updated;
}

static void check_blaster_variables(const bool autoexec)
{
    using namespace retro;

    update_dosbox_variable(
        autoexec, "sblaster", "sbtype", core_options[CORE_OPT_SBLASTER_TYPE].toString());
    update_dosbox_variable(
        autoexec, "sblaster", "sbbase", core_options[CORE_OPT_SBLASTER_BASE].toString());
    update_dosbox_variable(
        autoexec, "sblaster", "irq", core_options[CORE_OPT_SBLASTER_IRQ].toString());
    update_dosbox_variable(
        autoexec, "sblaster", "dma", core_options[CORE_OPT_SBLASTER_DMA].toString());
    update_dosbox_variable(
        autoexec, "sblaster", "hdma", core_options[CORE_OPT_SBLASTER_HDMA].toString());
    update_dosbox_variable(
        autoexec, "sblaster", "sbmixer", core_options[CORE_OPT_SBMIXER].toString());
    update_dosbox_variable(
        autoexec, "sblaster", "oplmode", core_options[CORE_OPT_SBLASTER_OPL_MODE].toString());
    update_dosbox_variable(
        autoexec, "sblaster", "oplemu", core_options[CORE_OPT_SBLASTER_OPL_EMU].toString());
}

static void check_gus_variables(const bool autoexec)
{
    using namespace retro;

    update_dosbox_variable(autoexec, "gus", "gus", core_options[CORE_OPT_GUS].toString());
    update_dosbox_variable(autoexec, "gus", "gusbase", core_options[CORE_OPT_GUSBASE].toString());
    update_dosbox_variable(autoexec, "gus", "gusirq", core_options[CORE_OPT_GUSIRQ].toString());
    update_dosbox_variable(autoexec, "gus", "gusdma", core_options[CORE_OPT_GUSDMA].toString());
}

static void check_vkbd_variables()
{
    using namespace retro;

    const auto& theme = core_options[CORE_OPT_VKBD_THEME].toString();
    if (theme.find("light") != std::string::npos) {
        opt_vkbd_theme = 1;
    } else if (theme.find("dark") != std::string::npos) {
        opt_vkbd_theme = 2;
    }
    if (theme.find("outline") != std::string::npos) {
        opt_vkbd_theme |= 0x80;
    }

    const auto& transparency = core_options[CORE_OPT_VKBD_TRANSPARENCY].toString();
    if (transparency == "0%") {
        opt_vkbd_alpha = GRAPH_ALPHA_100;
    } else if (transparency == "25%") {
        opt_vkbd_alpha = GRAPH_ALPHA_75;
    } else if (transparency == "50%") {
        opt_vkbd_alpha = GRAPH_ALPHA_50;
    } else if (transparency == "75%") {
        opt_vkbd_alpha = GRAPH_ALPHA_25;
    } else if (transparency == "100%") {
        opt_vkbd_alpha = GRAPH_ALPHA_0;
    }
}

static void check_pinhack_variables()
{
#ifdef WITH_PINHACK
    using namespace retro;
    bool updated = false;

    for (const auto* name :
         {CORE_OPT_PINHACK, CORE_OPT_PINHACKACTIVE, CORE_OPT_PINHACKTRIGGERWIDTH,
          CORE_OPT_PINHACKTRIGGERHEIGHT})
    {
        updated |= update_dosbox_variable(false, "pinhack", name, core_options[name].toString());
    }

    const int expand_height_coarse = core_options[CORE_OPT_PINHACKEXPANDHEIGHT_COARSE].toInt();
    if (expand_height_coarse > 0) {
        const int expand_height_fine = core_options[CORE_OPT_PINHACKEXPANDHEIGHT_FINE].toInt();
        updated |= update_dosbox_variable(
            false, "pinhack", "pinhackexpandheight",
            std::to_string(expand_height_coarse + expand_height_fine));
    }

    if (updated) {
        gfx::request_VGA_SetupDrawing = true;
    }
#endif
}

void core_autoexec()
{
    check_blaster_variables(true);
    check_gus_variables(true);
}

// The year of the game, when its name says it ("Game (1993).zip", or a
// /1993/ directory) or the automatic mapping database knows it; 0 otherwise.
static int content_year = 0;

static auto year_from_path(const std::string& path) -> int
{
    // As DOSBox Pure reads it: a year in parentheses or as a directory name,
    // looking from the end, with 199x or 19xx meaning the decade or century.
    const char* const start = path.c_str();
    for (const char *p = start + path.size(), *p_min = start + 5; p >= p_min; p--) {
        while (p >= p_min && *p != ')' && *p != '/' && *p != '\\') {
            p--;
        }
        if (p < p_min || (p[-5] != '(' && p[-5] != '/' && p[-5] != '\\')) {
            continue;
        }
        const int year = atoi(p - 4) * ((p[-2] | 0x20) == 'x' ? 100 : ((p[-1] | 0x20) == 'x' ? 10 : 1));
        if (year > 1970 && year < 2100) {
            return year;
        }
    }
    return 0;
}

// Cycles for a CPU of that year, DOSBox Pure's table.
static auto cycles_for_year(const int year) -> int
{
    static const int cycles_1982_to_1996[] = {900, 1500, 2100, 2750, 3800, 4800, 6300, 7800,
        14000, 23800, 27000, 44000, 55000, 66800, 93000};
    if (year < 1982) {
        return 315;
    }
    return cycles_1982_to_1996[std::min(year, 1996) - 1982];
}

static auto make_cpu_cycles_string() -> std::string
{
    using namespace retro;

    if (content_year > 1970 && core_options[CORE_OPT_CPU_CYCLES_BY_YEAR].toBool()) {
        // Real mode at the speed of the game's time, protected mode at full.
        return "auto " + std::to_string(cycles_for_year(content_year));
    }

    const auto& mode = core_options[CORE_OPT_CPU_CYCLES_MODE].toString();
    const int realmode_cycles = core_options[CORE_OPT_CPU_CYCLES_REALMODE].toInt()
            * core_options[CORE_OPT_CPU_CYCLES_MULTIPLIER_REALMODE].toInt()
        + core_options[CORE_OPT_CPU_CYCLES_FINE_REALMODE].toInt()
            * core_options[CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE_REALMODE].toInt();
    const int cycles = core_options[CORE_OPT_CPU_CYCLES].toInt()
            * core_options[CORE_OPT_CPU_CYCLES_MULTIPLIER].toInt()
        + core_options[CORE_OPT_CPU_CYCLES_FINE].toInt()
            * core_options[CORE_OPT_CPU_CYCLES_MULTIPLIER_FINE].toInt();
    const auto cycles_str = std::to_string(cycles);
    const std::string max_limits_str = [cycles, &cycles_str] {
        const auto& limit = core_options[CORE_OPT_CPU_CYCLES_LIMIT].toString();
        std::string retval;
        if (limit != "none") {
            retval += ' ' + limit;
        }
        if (cycles > 0) {
            retval += " limit " + cycles_str;
        }
        return retval;
    }();

    std::string cycles_string = mode;
    if (mode == "fixed" && cycles > 0) {
        cycles_string += ' ' + cycles_str;
    } else if (mode == "max") {
        cycles_string += max_limits_str;
    } else {
        if (realmode_cycles > 0) {
            cycles_string += ' ' + std::to_string(realmode_cycles);
        }
        cycles_string += max_limits_str;
    }
    return cycles_string;
}

static void check_cpu_cycle_variables()
{
    update_dosbox_variable(
        false, "cpu", "cycleup", retro::core_options[CORE_OPT_CPU_CYCLES_UP].toString());
    update_dosbox_variable(
        false, "cpu", "cycledown", retro::core_options[CORE_OPT_CPU_CYCLES_DOWN].toString());

    if (disabled_dosbox_variables.count("cycles") != 0) {
        return;
    }

    static std::string prev_cycles_string;
    auto new_cycles_string = make_cpu_cycles_string();

    if (prev_cycles_string != new_cycles_string) {
        update_dosbox_variable(false, "cpu", "cycles", new_cycles_string);
        prev_cycles_string = std::move(new_cycles_string);
    }
}

static void update_bassmidi_variables()
{
#ifdef WITH_BASSMIDI
    const auto soundfont = retro_system_directory / "soundfonts"
        / retro::core_options[CORE_OPT_BASSMIDI_SOUNDFONT].toString();
    update_dosbox_variable(false, "bassmidi", "bassmidi.soundfont", soundfont.u8string());

    for (const auto* name : {CORE_OPT_BASSMIDI_SFVOLUME, CORE_OPT_BASSMIDI_VOICES}) {
        update_dosbox_variable(false, "bassmidi", name, retro::core_options[name].toString());
    }
#endif
}

static void update_fsynth_variables()
{
#ifdef WITH_FLUIDSYNTH
    const auto soundfont = retro_system_directory / "soundfonts"
        / retro::core_options[CORE_OPT_FLUID_SOUNDFONT].toString();
    update_dosbox_variable(false, "midi", "fluid.soundfont", soundfont.u8string());

    for (const auto* name :
         {CORE_OPT_FLUID_GAIN, CORE_OPT_FLUID_POLYPHONY, CORE_OPT_FLUID_CORES,
          CORE_OPT_FLUID_SAMPLERATE, CORE_OPT_FLUID_REVERB, CORE_OPT_FLUID_REVERB_ROOMSIZE,
          CORE_OPT_FLUID_REVERB_DAMPING, CORE_OPT_FLUID_REVERB_WIDTH, CORE_OPT_FLUID_REVERB_LEVEL,
          CORE_OPT_FLUID_CHORUS, CORE_OPT_FLUID_CHORUS_NUMBER, CORE_OPT_FLUID_CHORUS_LEVEL,
          CORE_OPT_FLUID_CHORUS_SPEED, CORE_OPT_FLUID_CHORUS_DEPTH})
    {
        update_dosbox_variable(false, "midi", name, retro::core_options[name].toString());
    }
#endif
}

static void update_mt32_variables()
{
    update_dosbox_variable(false, "midi", "mt32.romdir", retro_system_directory.u8string());

    for (const auto* name :
         {CORE_OPT_MT32_TYPE, CORE_OPT_MT32_THREAD, CORE_OPT_MT32_PARTIALS, CORE_OPT_MT32_ANALOG,
          CORE_OPT_MT32_REVERSE_STEREO, CORE_OPT_MT32_DAC, CORE_OPT_MT32_REVERB_MODE,
          CORE_OPT_MT32_REVERB_TIME, CORE_OPT_MT32_REVERB_LEVEL, CORE_OPT_MT32_RATE,
          CORE_OPT_MT32_SRC_QUALITY, CORE_OPT_MT32_NICEAMPRAMP, CORE_OPT_MT32_CHUNK,
          CORE_OPT_MT32_PREBUFFER})
    {
        update_dosbox_variable(false, "midi", name, retro::core_options[name].toString());
    }
}

static void use_libretro_log_cb()
{
    retro_log_callback log_cb{nullptr};
    if (!environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_cb)) {
        retro::logError("RETRO_ENVIRONMENT_GET_LOG_INTERFACE failed.");
        return;
    }
    retro::setRetroLogCb(log_cb.log);
}

static void update_libretro_log_interface()
{
    if (retro::core_options[CORE_OPT_LOG_METHOD].toString() == "frontend") {
        use_libretro_log_cb();
    } else {
        retro::setRetroLogCb(nullptr);
    }
}

static void update_log_verbosity()
{
    const auto& level = retro::core_options[CORE_OPT_LOG_LEVEL].toString();
    if (level == "errors") {
        retro::setLoggingLevel(RETRO_LOG_ERROR);
    } else if (level == "warnings") {
        retro::setLoggingLevel(RETRO_LOG_WARN);
    } else if (level == "info") {
        retro::setLoggingLevel(RETRO_LOG_INFO);
    } else {
        retro::setLoggingLevel(RETRO_LOG_DEBUG);
    }
}

static void check_variables()
{
    using namespace retro;

    update_log_verbosity();
    update_libretro_log_interface();

    {
        const bool old_timing = run_synced;
        run_synced = core_options[CORE_OPT_CORE_TIMING].toString() == "external";

        if (dosbox_initialiazed && run_synced != old_timing) {
            if (!run_synced) {
                PIC_AddEvent(leave_thread, 1000.0f / internal_sync_fps);
            }
            update_gfx_mode(true);
        }
    }

    use_frame_duping = core_options[CORE_OPT_FRAME_DUPING].toBool();
    use_spinlock = core_options[CORE_OPT_THREAD_SYNC].toString() == "spin";
    useSpinlockThreadSync(use_spinlock);

    if (!dosbox_initialiazed) {
        update_dosbox_variable(
            false, "dosbox", "memsize", core_options[CORE_OPT_MEMORY_SIZE].toString());

        svgaCard = SVGA_None;
        machine = MCH_VGA;
        int10.vesa_nolfb = false;
        int10.vesa_oldvbe = false;
        const std::string& machine_type = core_options[CORE_OPT_MACHINE_TYPE].toString();
        if (machine_type == "hercules") {
            machine = MCH_HERC;
        } else if (machine_type == "cga") {
            machine = MCH_CGA;
        } else if (machine_type == "pcjr") {
            machine = MCH_PCJR;
        } else if (machine_type == "tandy") {
            machine = MCH_TANDY;
        } else if (machine_type == "ega") {
            machine = MCH_EGA;
        } else if (machine_type == "svga_s3") {
            machine = MCH_VGA;
            svgaCard = SVGA_S3Trio;
        } else if (machine_type == "svga_et4000") {
            machine = MCH_VGA;
            svgaCard = SVGA_TsengET4K;
        } else if (machine_type == "svga_et3000") {
            machine = MCH_VGA;
            svgaCard = SVGA_TsengET3K;
        } else if (machine_type == "svga_paradise") {
            machine = MCH_VGA;
            svgaCard = SVGA_ParadisePVGA1A;
        } else if (machine_type == "vesa_nolfb") {
            machine = MCH_VGA;
            svgaCard = SVGA_S3Trio;
            int10.vesa_nolfb = true;
        } else if (machine_type == "vesa_nolfb") {
            machine = MCH_VGA;
            svgaCard = SVGA_S3Trio;
            int10.vesa_oldvbe = true;
        } else {
            machine = MCH_VGA;
            svgaCard = SVGA_None;
        }
        update_dosbox_variable(false, "dosbox", "machine", machine_type);

#ifdef WITH_VOODOO
        update_dosbox_variable(false, "pci", "voodoo", core_options[CORE_OPT_VOODOO].toString());
        update_dosbox_variable(
            false, "pci", "voodoomem", core_options[CORE_OPT_VOODOO_MEMORY_SIZE].toString());
#endif

        mount_overlay = core_options[CORE_OPT_SAVE_OVERLAY].toBool();
    } else {
        update_dosbox_variable(false, "dos", "xms", core_options[CORE_OPT_XMS].toString());
        update_dosbox_variable(false, "dos", "ems", core_options[CORE_OPT_EMS].toString());
        update_dosbox_variable(false, "dos", "umb", core_options[CORE_OPT_UMB].toString());

        if (machine == MCH_HERC) {
            herc_pal = core_options[CORE_OPT_MACHINE_HERCULES_PALETTE].toInt();
            Herc_Palette();
            VGA_DAC_CombineColor(1, 7);
        } else if (machine == MCH_CGA) {
            CGA_Composite_Mode(core_options[CORE_OPT_MACHINE_CGA_COMPOSITE_MODE].toInt());
            CGA_Model(core_options[CORE_OPT_MACHINE_CGA_MODEL].toInt());
        }

        check_blaster_variables(false);
        check_gus_variables(false);

        {
            const bool prev_force_2axis_joystick = force_2axis_joystick;
            const int prev_mouse_emu_deadzone = mouse_emu_deadzone;

            force_2axis_joystick = core_options[CORE_OPT_JOYSTICK_FORCE_2AXIS].toBool();
            mouse_emu_deadzone = core_options[CORE_OPT_EMULATED_MOUSE_DEADZONE].toInt();
            if (prev_force_2axis_joystick != force_2axis_joystick
                || prev_mouse_emu_deadzone != mouse_emu_deadzone)
            {
                libretro_input_init();
            }
        }

        for (const auto& option :
             {CORE_OPT_MOUSE_SPEED_X, CORE_OPT_MOUSE_SPEED_Y, CORE_OPT_MOUSE_SPEED_MULT,
              CORE_OPT_MOUSE_SPEED_HACK, CORE_OPT_MOUSE_SPEED_CLAMP})
        {
            update_dosbox_variable(false, "dosbox-core", option, core_options[option].toString());
        }

        update_dosbox_variable(false, "cpu", "cputype", core_options[CORE_OPT_CPU_TYPE].toString());
        update_dosbox_variable(false, "cpu", "core", core_options[CORE_OPT_CPU_CORE].toString());
        update_dosbox_variable(
            false, "render", "aspect", core_options[CORE_OPT_ASPECT_CORRECTION].toString());
        update_dosbox_variable(false, "render", "scaler", core_options[CORE_OPT_SCALER].toString());
        update_dosbox_variable(
            false, "joystick", "timed", core_options[CORE_OPT_JOYSTICK_TIMED].toString());

        check_cpu_cycle_variables();

        update_dosbox_variable(
            false, "speaker", "pcspeaker", core_options[CORE_OPT_PCSPEAKER].toString());

        {
            const auto& mpu_type = core_options[CORE_OPT_MPU_TYPE].toString();
            update_dosbox_variable(false, "midi", "mpu401", mpu_type);

            const auto& midi_driver = core_options[CORE_OPT_MIDI_DRIVER].toString();
            use_retro_midi = midi_driver == "libretro";
            update_dosbox_variable(
                false, "midi", "mididevice", use_retro_midi ? "none" : midi_driver);

            update_bassmidi_variables();
            update_fsynth_variables();
            update_mt32_variables();
            if (midi_driver == "sc55") {
                // Where the SC-55 ROMs are: an SC-55.zip, or else the system
                // directory, which the handler looks through for them.
                const auto zip = retro_system_directory / "SC-55.zip";
                struct stat st;
                const auto zip_str = from_u8string(zip.u8string());
                update_dosbox_variable(false, "midi", "midiconfig",
                    host_stat(zip_str.c_str(), &st) == 0
                        ? zip_str
                        : from_u8string(retro_system_directory.u8string()));
            }

            if (use_retro_midi && !have_retro_midi) {
                have_retro_midi =
                    environ_cb(RETRO_ENVIRONMENT_GET_MIDI_INTERFACE, &retro_midi_interface);
                retro::logDebug(
                    "Libretro MIDI interface {}.", have_retro_midi ? "initialized" : "unavailable");
            }
#if defined(HAVE_ALSA)
            // Dosbox only accepts the numerical MIDI port, not client/port names.
            const auto& current_value = core_options[CORE_OPT_MIDI_PORT].toString();
            std::string first_gm_port;
            bool port_found = false;
            for (const auto& [midi_std, port, client, port_name] : alsa_midi_ports) {
                if ((current_value == "auto gm" && midi_std == MidiStandard::GM)
                    || (current_value == "auto gs gm" && midi_std == MidiStandard::GS)
                    || (current_value == "auto xg" && midi_std == MidiStandard::XG)
                    || (current_value == "auto mt32" && midi_std == MidiStandard::MT32)
                    || (current_value == "auto gm2" && midi_std == MidiStandard::GM2)
                    || (client + ':' + port_name == current_value))
                {
                    update_dosbox_variable(false, "midi", "midiconfig", port);
                    port_found = true;
                    break;
                }
                if (midi_std == MidiStandard::GM && first_gm_port.empty()) {
                    first_gm_port = std::move(port);
                }
            }
            // Fall back to the first GM port if we originally wanted GS but didn't find one.
            if (!port_found && current_value == "auto gs gm" && !first_gm_port.empty()) {
                update_dosbox_variable(false, "midi", "midiconfig", first_gm_port);
            }
#endif
#ifdef __WIN32__
            update_dosbox_variable(
                false, "midi", "midiconfig", core_options[CORE_OPT_MIDI_PORT].toString());
#endif
        }

#if defined(C_IPX)
        update_dosbox_variable(false, "ipx", "ipx", core_options[CORE_OPT_IPX].toString());
#endif

        update_dosbox_variable(false, "speaker", "tandy", core_options[CORE_OPT_TANDY].toString());

        disney_init = core_options[CORE_OPT_DISNEY].toBool();
        update_dosbox_variable(false, "speaker", "disney", disney_init ? "true" : "false");

        check_vkbd_variables();
        check_pinhack_variables();
    }
}

// What the autoexec runs after mounting the ZIP: the game, when there is
// nothing to choose (see find_program_to_run), and nothing otherwise, which
// leaves the C:\> prompt. Kept here because the virtual file points into it.
static std::string ziprun_bat;
void VFILE_Remove(const char* name); // drive_virtual.cpp

// Lists a directory of the drive: the programs in it (.EXE, .COM, .BAT) and
// its subdirectories.
static void list_directory(DOS_Drive* const drive, const std::string& dir,
    std::vector<std::string>& programs, std::vector<std::string>& dirs)
{
    const RealPt save_dta = dos.dta();
    dos.dta(dos.tables.tempdta);
    DOS_DTA dta(dos.dta());
    char pattern[] = "*.*";
    dta.SetupSearch('C' - 'A', 0xff & ~DOS_ATTR_VOLUME, pattern);
    std::string dir_buf = dir;
    for (bool more = drive->FindFirst(&dir_buf[0], dta); more; more = drive->FindNext(dta)) {
        char name[DOS_NAMELENGTH_ASCII];
        Bit32u size;
        Bit16u date;
        Bit16u time;
        Bit8u attr;
        dta.GetResult(name, size, date, time, attr);
        if (attr & DOS_ATTR_DIRECTORY) {
            if (strcmp(name, ".") && strcmp(name, "..")) {
                dirs.emplace_back(name);
            }
            continue;
        }
        const char* const dot = strrchr(name, '.');
        if (dot && (!strcasecmp(dot, ".EXE") || !strcasecmp(dot, ".COM") || !strcasecmp(dot, ".BAT"))) {
            programs.emplace_back(name);
        }
    }
    dos.dta(save_dta);
}

// The game to start from a ZIP, the way DOSBox Pure picks one when there is
// nothing to choose: look in the root, or in the one directory that is all
// the root has, and take the program there if there is a single one once
// setup and install programs are left aside. Empty when there is a choice.
static auto find_program_to_run(DOS_Drive* const drive, std::string& dir) -> std::string
{
    std::vector<std::string> programs;
    std::vector<std::string> dirs;
    dir.clear();
    list_directory(drive, dir, programs, dirs);
    if (programs.empty() && dirs.size() == 1) {
        dir = dirs[0];
        dirs.clear();
        list_directory(drive, dir, programs, dirs);
    }
    std::vector<std::string> games;
    for (const auto& program : programs) {
        std::string upper = program;
        for (auto& c : upper) {
            c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }
        if (upper.find("SETUP") != std::string::npos || upper.find("INSTALL") != std::string::npos
            || upper.find("CONFIG") != std::string::npos || upper.find("UNINST") != std::string::npos
            || upper.find("SETSOUND") != std::string::npos) {
            continue;
        }
        games.push_back(program);
    }
    return games.size() == 1 ? games[0] : std::string();
}

// The disk images inside a ZIP: CD images (cue sheets, or ISOs when there is
// no cue sheet) and floppy images. They are read straight out of the ZIP
// through "$C:\\..." paths (drive_dbp.cpp). The first CD is mounted as D:
// and the first floppy as A:, and all of them go to the frontend's disc
// control, so RetroArch can swap between them. Hard disk images (images
// larger than a floppy) come back as IMGMOUNT lines for the autoexec, as E:
// and on; IMGMOUNT works out the geometry of the usual 16-head, 63-sector
// layout by itself.
static auto mount_images(DOS_Drive* const drive) -> std::string
{
    struct Found {
        std::vector<std::string> cue, iso, floppy, hdd;
    } found;
    DriveFileIterator(drive,
        [](const char* path, bool is_dir, Bit32u size, Bit16u, Bit16u, Bit8u, Bitu data) {
            if (is_dir) {
                return;
            }
            const char* const dot = strrchr(path, '.');
            if (!dot) {
                return;
            }
            auto& f = *reinterpret_cast<Found*>(data);
            if (!strcasecmp(dot, ".CUE")) {
                f.cue.emplace_back(path);
            } else if (!strcasecmp(dot, ".ISO")) {
                f.iso.emplace_back(path);
            } else if (!strcasecmp(dot, ".IMG") || !strcasecmp(dot, ".IMA")) {
                (size <= 2949120 ? f.floppy : f.hdd).emplace_back(path);
            }
        },
        reinterpret_cast<Bitu>(&found));

    std::string hdd_mounts;
    std::sort(found.hdd.begin(), found.hdd.end());
    char letter = 'E';
    for (const auto& hdd : found.hdd) {
        if (letter > 'Y') {
            break;
        }
        hdd_mounts += std::string("IMGMOUNT ") + letter++ + " $C:\\" + hdd + " -t hdd\r\n";
    }

    auto& cds = found.cue.empty() ? found.iso : found.cue;
    std::sort(cds.begin(), cds.end());
    std::sort(found.floppy.begin(), found.floppy.end());
    std::vector<std::filesystem::path> images;
    for (const auto& cd : cds) {
        images.emplace_back("$C:\\" + cd);
    }
    for (const auto& floppy : found.floppy) {
        // disk_control tells floppies from CDs by the .img extension.
        auto path = "$C:\\" + floppy;
        images.emplace_back(path);
    }
    if (images.empty()) {
        return hdd_mounts;
    }
    if (!found.floppy.empty()) {
        disk_control::mount("$C:\\" + found.floppy[0]);
    }
    if (!cds.empty()) {
        disk_control::mount("$C:\\" + cds[0]);
    }
    // The CD is the one to swap, when there is one.
    disk_control::set_images(std::move(images), 0);
    retro::logInfo("Mounted the disk images in the ZIP: {} CD, {} floppy, {} hard disk.",
        cds.size(), found.floppy.size(), found.hdd.size());
    return hdd_mounts;
}

// The start menu, after DOSBox Pure's: when a ZIP leaves a choice of
// programs, this lists every program on C: and on the images mounted from
// it, and starts the one picked. It runs inside DOS, drawn with the console's
// ANSI support, so it needs nothing from the frontend. The pick can be made
// the one that starts on its own next time; it is kept in C:\DOSBOX.RUN,
// which lands in the save file like any other change, and a key pressed
// while it is about to start brings the menu back instead. After the program
// exits, the menu comes back; leaving it goes to the command line.
//
// ZIPGO.BAT runs the loop: ZIPMENU.COM writes what to run to ZIPSEL.BAT, or
// registers ZIPQUIT to leave, as an internal program cannot set ERRORLEVEL.
static std::string zipsel_bat;
static std::string zipquit_file;
static const char zipgo_bat[] =
    "@ECHO OFF\r\n"
    ":MENU\r\n"
    "Z:\\ZIPMENU.COM\r\n"
    "IF EXIST Z:\\ZIPQUIT GOTO END\r\n"
    "CALL Z:\\ZIPSEL.BAT\r\n"
    "GOTO MENU\r\n"
    ":END\r\n";
static constexpr const char* auto_run_file = "C:\\DOSBOX.RUN";
extern bool zipmenu_active;          // libretro_input.cpp
unsigned libretro_joypad_state();    // libretro_input.cpp

class ZIPMENU final : public Program {
public:
    void Run() override
    {
        struct Active {
            Active() { zipmenu_active = true; }
            ~Active() { zipmenu_active = false; }
        } active;
        std::vector<std::string> programs;
        for (const char letter : {'C', 'D', 'E', 'A'}) {
            if (!Drives[letter - 'A']) {
                continue;
            }
            std::vector<std::string> found;
            DriveFileIterator(Drives[letter - 'A'],
                [](const char* path, bool is_dir, Bit32u, Bit16u, Bit16u, Bit8u, Bitu data) {
                    const char* const dot = strrchr(path, '.');
                    if (!is_dir && dot
                        && (!strcasecmp(dot, ".EXE") || !strcasecmp(dot, ".COM")
                            || !strcasecmp(dot, ".BAT"))) {
                        reinterpret_cast<std::vector<std::string>*>(data)->emplace_back(path);
                    }
                },
                reinterpret_cast<Bitu>(&found));
            std::sort(found.begin(), found.end());
            for (auto& path : found) {
                programs.push_back(std::string(1, letter) + ":\\" + path);
            }
        }
        set_quit(false);
        if (programs.empty()) {
            set_quit(true);
            return;
        }

        std::string auto_run = read_auto_run();
        int selected = 0;
        for (size_t i = 0; i < programs.size(); ++i) {
            if (!strcasecmp(programs[i].c_str(), auto_run.c_str())) {
                selected = static_cast<int>(i);
            }
        }
        if (!auto_run.empty() && strcasecmp(programs[selected].c_str(), auto_run.c_str())) {
            auto_run.clear(); // the program it named is gone
        }
        if (!auto_run.empty() && !first_run_done) {
            first_run_done = true;
            WriteOut("\033[2J\033[1;1HStarting %s - press any key for the menu\n", auto_run.c_str());
            const double until = PIC_FullIndex() + 1500.0;
            bool key = false;
            const unsigned pad_before = libretro_joypad_state();
            while (PIC_FullIndex() < until) {
                if (DOS_GetSTDINStatus()) {
                    key = true;
                    read_key();
                    break;
                }
                if (libretro_joypad_state() & ~pad_before) {
                    key = true;
                    break;
                }
                CALLBACK_Idle();
            }
            if (!key) {
                select(auto_run);
                return;
            }
        }
        first_run_done = true;

        const int rows = 18;
        int top = 0;
        for (;;) {
            if (selected < top) {
                top = selected;
            } else if (selected >= top + rows) {
                top = selected - rows + 1;
            }
            WriteOut("\033[0m\033[2J\033[1;1H\033[1;36m Start menu\033[0m - choose the program to run\n\n");
            const int count = static_cast<int>(programs.size());
            for (int i = top; i < count && i < top + rows; ++i) {
                const bool is_auto = !strcasecmp(programs[i].c_str(), auto_run.c_str());
                WriteOut("%s  %-60s%s\033[0m\n", i == selected ? "\033[7m" : "", programs[i].c_str(),
                    is_auto ? " [auto]" : "       ");
            }
            WriteOut("\033[23;1H\033[1;33m Enter\033[0m start  \033[1;33mA\033[0m always start this "
                     "(again: stop)  \033[1;33mEsc\033[0m command line\n"
                     "\033[1;33m Gamepad\033[0m: A/Start start, Y always start this, Select command line");
            const int key = read_key();
            if (key == 13) {
                select(programs[selected]);
                return;
            }
            if (key == 27) {
                WriteOut("\033[0m\033[2J\033[1;1H");
                set_quit(true);
                return;
            }
            if (key == 'a' || key == 'A') {
                if (!strcasecmp(programs[selected].c_str(), auto_run.c_str())) {
                    auto_run.clear();
                    DOS_UnlinkFile(auto_run_file);
                    continue;
                }
                auto_run = programs[selected];
                write_auto_run(auto_run);
                select(auto_run);
                return;
            }
            if (key == 0x148) {
                selected = std::max(0, selected - 1);
            } else if (key == 0x150) {
                selected = std::min(count - 1, selected + 1);
            } else if (key == 0x149) {
                selected = std::max(0, selected - rows);
            } else if (key == 0x151) {
                selected = std::min(count - 1, selected + rows);
            } else if (key == 0x147) {
                selected = 0;
            } else if (key == 0x14F) {
                selected = count - 1;
            }
        }
    }

    static bool first_run_done;

private:
    // A key from the keyboard: its character, or 0x100 + its scan code for
    // the keys that have none (arrows, page keys). A gamepad button pressed
    // meanwhile answers as the key it stands for here.
    static int read_key()
    {
        unsigned pad_prev = libretro_joypad_state();
        for (;;) {
            if (DOS_GetSTDINStatus()) {
                Bit8u c = 0;
                Bit16u n = 1;
                DOS_ReadFile(STDIN, &c, &n);
                if (c != 0) {
                    return c;
                }
                n = 1;
                DOS_ReadFile(STDIN, &c, &n);
                return 0x100 + c;
            }
            const unsigned pad = libretro_joypad_state();
            const unsigned pressed = pad & ~pad_prev;
            pad_prev = pad;
            auto is = [pressed](unsigned id) { return (pressed & (1u << id)) != 0; };
            if (is(RETRO_DEVICE_ID_JOYPAD_UP)) return 0x148;
            if (is(RETRO_DEVICE_ID_JOYPAD_DOWN)) return 0x150;
            if (is(RETRO_DEVICE_ID_JOYPAD_LEFT) || is(RETRO_DEVICE_ID_JOYPAD_L)) return 0x149;
            if (is(RETRO_DEVICE_ID_JOYPAD_RIGHT) || is(RETRO_DEVICE_ID_JOYPAD_R)) return 0x151;
            if (is(RETRO_DEVICE_ID_JOYPAD_A) || is(RETRO_DEVICE_ID_JOYPAD_START)) return 13;
            if (is(RETRO_DEVICE_ID_JOYPAD_Y)) return 'a';
            if (is(RETRO_DEVICE_ID_JOYPAD_SELECT)) return 27;
            CALLBACK_Idle();
        }
    }

    // What ZIPSEL.BAT runs for a path like D:\GAME\GAME.EXE: change to its
    // drive and directory, run it, and come back to C:\.
    static void select(const std::string& path)
    {
        const auto sep = path.rfind('\\');
        const std::string dir = path.substr(2, sep - 2);
        zipsel_bat = "@ECHO OFF\r\n" + path.substr(0, 2) + "\r\nCD " + (dir.empty() ? "\\" : dir)
            + "\r\n" + path.substr(sep + 1) + "\r\nC:\r\nCD \\\r\n";
        VFILE_Remove("ZIPSEL.BAT");
        VFILE_Register("ZIPSEL.BAT", reinterpret_cast<Bit8u*>(&zipsel_bat[0]),
            static_cast<Bit32u>(zipsel_bat.size()));
    }

    static void set_quit(const bool quit)
    {
        VFILE_Remove("ZIPQUIT");
        if (quit) {
            zipquit_file = "\r\n";
            VFILE_Register("ZIPQUIT", reinterpret_cast<Bit8u*>(&zipquit_file[0]), 2);
        }
    }

    static std::string read_auto_run()
    {
        Bit16u handle;
        if (!DOS_OpenFile(auto_run_file, OPEN_READ, &handle)) {
            return std::string();
        }
        char buf[128];
        Bit16u n = sizeof(buf) - 1;
        DOS_ReadFile(handle, reinterpret_cast<Bit8u*>(buf), &n);
        DOS_CloseFile(handle);
        buf[n] = '\0';
        std::string path(buf);
        while (!path.empty() && (path.back() == '\r' || path.back() == '\n' || path.back() == ' ')) {
            path.pop_back();
        }
        return path;
    }

    static void write_auto_run(const std::string& path)
    {
        Bit16u handle;
        if (!DOS_CreateFile(auto_run_file, DOS_ATTR_ARCHIVE, &handle)) {
            return;
        }
        std::string line = path + "\r\n";
        Bit16u n = static_cast<Bit16u>(line.size());
        DOS_WriteFile(handle, reinterpret_cast<Bit8u*>(&line[0]), &n);
        DOS_CloseFile(handle);
    }
};

bool ZIPMENU::first_run_done = false;

// Looks the content up in the automatic gamepad mapping database, once the
// ZIP and the images in it are mounted; retro_run applies what it finds.
class AUTOMAP final : public Program {
public:
    void Run() override
    {
        if (retro::core_options[CORE_OPT_AUTO_MAPPING].toBool()) {
            automap_detect(from_u8string(zip_content_path.stem().u8string()));
        }
    }
};

static void AUTOMAP_ProgramStart(Program** make)
{
    *make = new AUTOMAP;
}

static void ZIPMENU_ProgramStart(Program** make)
{
    *make = new ZIPMENU;
}

// Mounts the ZIP the core was started with as C:. Run from the autoexec, so
// it happens after DOS is set up and before anything on C: is looked at.
class MOUNTZIP final : public Program {
public:
    void Run() override
    {
        if (zip_content_path.empty()) {
            return;
        }
        const auto zip_path = from_u8string(zip_content_path.u8string());
        FILE* const zip_file = fopen_wrap(zip_path.c_str(), "rb");
        if (!zip_file) {
            WriteOut("Cannot open %s\n", zip_path.c_str());
            return;
        }
        auto* const zip_drive = new zipDrive(new rawFile(zip_file, false));

        // The game's changes go to <name>.save.zip in the saves directory.
        const auto& save_dir = retro_save_directory;
        const auto save_dir_str = from_u8string(save_dir.u8string());
        if (host_is_vfs_path(save_dir_str.c_str())) {
            host_mkdir(save_dir_str.c_str());
        } else {
            std::error_code ec;
            std::filesystem::create_directories(save_dir, ec);
        }
        auto save_file = from_u8string(
            (save_dir / zip_content_path.stem()).u8string()) + ".save.zip";

        auto* const drive = new unionDrive(*zip_drive, &save_file[0], true);
        if (Drives['C' - 'A']) {
            delete Drives['C' - 'A'];
        }
        Drives['C' - 'A'] = drive;
        mem_writeb(Real2Phys(dos.tables.mediaid) + ('C' - 'A') * 9, drive->GetMediaByte());
        retro::logInfo("Mounted {} as C:, saving changes to {}.", zip_path, save_file);

        std::string dir;
        const auto program = find_program_to_run(drive, dir);
        ziprun_bat = "@ECHO OFF\r\n";
        ziprun_bat += mount_images(drive);
        ziprun_bat += "Z:\\AUTOMAP.COM\r\n";
        if (!dir.empty()) {
            ziprun_bat += "CD " + dir + "\r\n";
        }
        if (program.empty()) {
            // A choice to make, or programs only on the images: the menu,
            // unless it is switched off.
            if (retro::core_options[CORE_OPT_ZIP_START_MENU].toBool()) {
                ziprun_bat += "Z:\\ZIPGO.BAT\r\n";
            }
        } else {
            ziprun_bat += program + "\r\n";
            retro::logInfo("Running {}{}{}, the only game program in the ZIP.", dir,
                dir.empty() ? "" : "\\", program);
        }
        VFILE_Remove("ZIPRUN.BAT");
        VFILE_Register("ZIPRUN.BAT", reinterpret_cast<Bit8u*>(&ziprun_bat[0]),
            static_cast<Bit32u>(ziprun_bat.size()));
    }
};

static void MOUNTZIP_ProgramStart(Program** make)
{
    *make = new MOUNTZIP;
}

static void start_dosbox(const std::string cmd_line)
{
    // ZIP content is not a path the shell can run: mount it and change to it.
    const char* const zip_argv[] = {"dosbox", "-c", "MOUNTZIP", "-c", "C:", "-c", "Z:\\ZIPRUN.BAT"};
    const char* const argv[2] = {"dosbox", cmd_line.c_str()};
    CommandLine com_line = zip_content_path.empty()
        ? CommandLine(cmd_line.empty() ? 1 : 2, argv)
        : CommandLine(7, zip_argv);
    Config myconf(&com_line);
    control = &myconf;
    dosbox_initialiazed = false;

    /* Init the configuration system and add default values */
    DOSBOX_Init();

    // Forcibly load default config if user says so.
    if (const auto default_conf = retro_save_directory / (retro_library_name + ".conf");
        retro::core_options[CORE_OPT_LOAD_DEFAULT_CONF].toBool() && config_path != default_conf)
    {
        control->ParseConfigFile(from_u8string(default_conf.u8string()).c_str());
    }

    /* Load config */
    if (!config_path.empty()) {
        control->ParseConfigFile(from_u8string(config_path.u8string()).c_str());
    }

    check_variables();
    control->Init();
    PROGRAMS_MakeFile("MOUNTZIP.COM", MOUNTZIP_ProgramStart);
    PROGRAMS_MakeFile("ZIPMENU.COM", ZIPMENU_ProgramStart);
    PROGRAMS_MakeFile("AUTOMAP.COM", AUTOMAP_ProgramStart);
    automap_reset();
    automap_checked = false;
    VFILE_Register("ZIPGO.BAT", reinterpret_cast<Bit8u*>(const_cast<char*>(zipgo_bat)),
        static_cast<Bit32u>(sizeof(zipgo_bat) - 1));
    ZIPMENU::first_run_done = false;

    /* Init done, go back to the main thread */
    switchThread();

    dosbox_initialiazed = true;
    check_variables();

    if (!run_synced) {
        /* When not synced, schedule the first frontend interrupt */
        PIC_AddEvent(leave_thread, 1000.0f / internal_sync_fps);
    }

    try {
        control->StartUp();
    }
    catch (const EmuThreadCanceled&) {
        retro::logDebug("Frontend asked to exit.");
        return;
    }

    retro::logDebug("Core asked to exit.");
    dosbox_exit = true;
    switchThread();
}

void restart_program(std::vector<std::string>& /*parameters*/)
{
    retro::logWarn("Program restart not supported.");
}

auto retro_api_version() -> unsigned
{
    return RETRO_API_VERSION;
}

/* cross.cpp: the frontend VFS that host paths with a scheme go through */
extern struct retro_vfs_interface* host_vfs;
extern unsigned host_vfs_version;

void retro_set_environment(const retro_environment_t cb)
{
    /* Take the frontend's VFS when it offers one: CD images are opened through
       it (see BinaryFile), and from version 3 on - which adds stat, mkdir and
       directory listing - so are the files and directories of mounted drives
       (see host_is_vfs_path in cross.cpp), so content only the frontend can
       open, like Android SAF paths, becomes loadable. */
    {
        retro_vfs_interface_info vfs_iface_info;
        vfs_iface_info.required_interface_version = 3;
        vfs_iface_info.iface = nullptr;
        if (cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info) && vfs_iface_info.iface) {
            host_vfs = vfs_iface_info.iface;
            host_vfs_version = vfs_iface_info.required_interface_version;
            filestream_vfs_init(&vfs_iface_info);
        } else {
            vfs_iface_info.required_interface_version = 1;
            vfs_iface_info.iface = nullptr;
            if (cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info)) {
                filestream_vfs_init(&vfs_iface_info);
            }
        }
    }

    environ_cb = cb;

    bool allow_no_game = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &allow_no_game);

    static constexpr std::array<retro_controller_description, 4> ports_default{{
        {"Keyboard + Mouse", RETRO_DEVICE_KEYBOARD},
        {"Gamepad", RETRO_DEVICE_JOYPAD},
        {"Joystick", RETRO_DEVICE_JOYSTICK},
        {"Disconnected", RETRO_DEVICE_NONE},
    }};

    static std::array<retro_controller_info, RETRO_INPUT_PORTS_MAX + 1> ports = [] {
        decltype(ports) tmp;
        tmp.fill({ports_default.data(), ports_default.size()});
        tmp.back() = {};
        return tmp;
    }();
    environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, ports.data());

    retro_core_options_update_display_callback display_cb{update_core_option_visibility};
    environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK, &display_cb);
}

void retro_set_controller_port_device(const unsigned port, const unsigned device)
{
    if (static_cast<int>(port) >= RETRO_INPUT_PORTS_MAX) {
        retro::logWarn(
            "Ignoring controller port {} since we only support {} ports.", port + 1,
            RETRO_INPUT_PORTS_MAX);
        return;
    }

    connected[port] = false;
    gamepad[port] = false;
    switch (device) {
    case RETRO_DEVICE_JOYPAD:
        connected[port] = true;
        gamepad[port] = true;
        break;
    case RETRO_DEVICE_JOYSTICK:
        connected[port] = true;
        gamepad[port] = false;
        break;
    case RETRO_DEVICE_KEYBOARD:
    default:
        connected[port] = false;
        gamepad[port] = false;
        break;
    }
    libretro_input_init();
}

void retro_get_system_info(retro_system_info* const info)
{
    info->library_name = retro_library_name.c_str();
    info->library_version = CORE_VERSION;
    info->valid_extensions = "exe|com|bat|conf|cue|iso|img|zip";
    info->need_fullpath = true;
    info->block_extract = true; // ZIP content is mounted as a drive, not extracted
}

void retro_get_system_av_info(retro_system_av_info* const info)
{
    info->geometry.base_width = gfx::width;
    info->geometry.base_height = gfx::height;
    info->geometry.max_width = gfx::max_width;
    info->geometry.max_height = gfx::max_height;
    info->geometry.aspect_ratio = gfx::aspect_ratio;
    info->timing.fps = run_synced ? render.src.fps : internal_sync_fps;
    info->timing.sample_rate = (double)MIXER_RETRO_GetFrequency();
}

void retro_init()
{
    use_libretro_log_cb();
    retro::setMessageEnvCb(environ_cb);

    init_audio();

    if (const char* system_dir = nullptr;
        !environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir))
    {
        retro::logError("RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY failed.");
    } else if (system_dir) {
        retro_system_directory = std::filesystem::path(system_dir).make_preferred();
        retro::logDebug("System directory: {}", retro_system_directory);
    }

    if (const char* save_dir = nullptr;
        !environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir)) {
        retro::logError("RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY failed.");
    } else if (save_dir) {
        retro_save_directory = std::filesystem::path(save_dir).make_preferred();
        retro::logDebug("Save directory: {}", retro_save_directory);
    }

    if (const char* content_dir = nullptr;
        !environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir))
    {
        retro::logError("RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY failed.");
    } else if (content_dir) {
        retro_content_directory = std::filesystem::path(content_dir).make_preferred();
        retro::logDebug("Core assets directory: {}", retro_content_directory);
    }

#ifdef HAVE_ALSA
    // Add detected MIDI ports as additional values to the midi port option.
    {
        auto* const option = retro::core_options.option(CORE_OPT_MIDI_PORT);
        // We don't use numerical ports since these can change. We instead use the MIDI client and
        // port name and resolve them back to numerical ports later on.
        for (const auto& [midi_std, port, client, port_name] : alsa_midi_ports) {
            option->addValue(
                {client + ':' + port_name, "[" + client + "] " + port_name + " - " + port});
        }
    }
#endif
#ifdef __WIN32__
    {
        std::vector<retro::CoreOptionValue> values;
        for (const auto& port : getWin32MidiPorts()) {
            values.emplace_back(port);
        }
        if (values.empty()) {
            values.emplace_back("none", "(no MIDI ports found)");
        }
        retro::core_options.option(CORE_OPT_MIDI_PORT)->setValues(values, values.front());
    }
#endif

    {
        std::vector<retro::CoreOptionValue> fsynth_values;
        std::vector<retro::CoreOptionValue> bass_values;
        try {
            namespace fs = std::filesystem;
            for (const auto& file : fs::directory_iterator(retro_system_directory / "soundfonts")) {
                const auto extension = lower_case(file.path().extension().string());
                if (extension == ".sfz") {
                    bass_values.emplace_back(from_u8string(file.path().filename().u8string()));
                    continue;
                }
                if (extension == ".sf2") {
                    bass_values.emplace_back(from_u8string(file.path().filename().u8string()));
                    fsynth_values.emplace_back(from_u8string(file.path().filename().u8string()));
                    continue;
                }
                for (const char* ext : {".sf3", ".dls", ".gig"}) {
                    if (extension == ext) {
                        fsynth_values.emplace_back(
                            from_u8string(file.path().filename().u8string()));
                    }
                }
            }
        }
        catch (const std::exception& e) {
            retro::logError("Error reading soundfont directory: {}", e.what());
        }
        if (bass_values.empty()) {
            bass_values.push_back({"none", "(no soundfonts found)"});
        }
        if (fsynth_values.empty()) {
            fsynth_values.push_back({"none", "(no soundfonts found)"});
        }
#ifdef WITH_BASSMIDI
        retro::core_options.option(CORE_OPT_BASSMIDI_SOUNDFONT)
            ->setValues(bass_values, bass_values.front());
#endif
#ifdef WITH_FLUIDSYNTH
        retro::core_options.option(CORE_OPT_FLUID_SOUNDFONT)
            ->setValues(fsynth_values, fsynth_values.front());
#endif
    }

    {
        bool flag = true;
        environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &flag);
    }

    retro::core_options.setEnvironmentCallback(environ_cb);
    retro::core_options.updateFrontend();
    update_core_option_visibility();
    disk_control::init(environ_cb);

    if (!environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb)) {
        perf_cb.get_time_usec = nullptr;
    }

    libretro_supports_bitmasks = environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, nullptr);
    retro::logDebug("Frontend input bitmask support: {}", libretro_supports_bitmasks);
}

void retro_deinit()
{
    frontend_exit = true;
    if (emu_thread.joinable()) {
        if (!dosbox_exit) {
            switchThread();
        }
        try {
            emu_thread.join();
        }
        catch (...) {
        }
    }

    libretro_graph_free();
}

auto retro_load_game(const retro_game_info* const game) -> bool
{
    gfx::pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
    retro::logDebug("Setting pixel format to RETRO_PIXEL_FORMAT_XRGB8888.");
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &gfx::pixel_format)) {
        retro::logError("RETRO_ENVIRONMENT_SET_PIXEL_FORMAT failed.");
    }

    std::filesystem::path load_path;
    std::filesystem::path disk_load_image;

    if (game && game->path) {
        try {
            load_path =
                std::filesystem::canonical(std::filesystem::path(game->path)).make_preferred();
            game_path = load_path;
        }
        catch (const std::filesystem::filesystem_error& e) {
            /* A frontend VFS path is not something the host filesystem can
               resolve - Android SAF hands out content:// URIs, which only the
               VFS understands - so pass those through untouched instead of
               refusing them. Anything else is a genuine failure. */
            if (std::string(game->path).find("://") == std::string::npos) {
                retro::logError("Failed to load \"{}\": {}", game->path, e.what());
                return false;
            }
            load_path = std::filesystem::path(game->path);
            game_path = load_path;
        }
    }

    if (const auto extension = lower_case(load_path.extension().string()); extension == ".conf") {
        config_path = load_path;
        load_path.clear();
    } else {
        retro::logInfo("Loading default configuration: {}", config_path);
        config_path = retro_save_directory / (retro_library_name + ".conf");
        if (extension == ".iso" || extension == ".cue") {
            disk_load_image = std::move(load_path);
            load_path.clear();
        } else if (extension == ".zip") {
            zip_content_path = std::move(load_path);
            load_path.clear();
        }
    }

    if (game_path.has_parent_path()) {
        load_game_directory = game_path.parent_path();
    }
    content_year = year_from_path(from_u8string(game_path.u8string()));

    emu_thread = std::thread(start_dosbox, from_u8string(load_path.u8string()));
    // Run dosbox until it sets its initial video mode.
    while (switchThread() != ThreadSwitchReason::VideoModeChange && !dosbox_exit)
        ;
    update_mouse_speed_fix(gfx::height);

    if (!disk_load_image.empty()) {
        disk_control::mount(std::move(disk_load_image));
    }

    update_core_option_visibility();
    return true;
}

auto retro_load_game_special(
    const unsigned /*game_type*/, const retro_game_info* const /*info*/, const size_t /*num_info*/)
    -> bool
{
    return false;
}

void retro_run()
{
    if (dosbox_exit) {
        if (emu_thread.joinable()) {
            switchThread();
            try {
                emu_thread.join();
            }
            catch (...) {
            }
        }
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, nullptr);
        return;
    }

    {
        bool new_fast_forward_status = false;
        environ_cb(RETRO_ENVIRONMENT_GET_FASTFORWARDING, &new_fast_forward_status);
        if (new_fast_forward_status != fast_forward_status) {
            DOSBOX_UnlockSpeed(new_fast_forward_status);
            fast_forward_status = new_fast_forward_status;
        }
    }

    if (retro::core_options.changed()) {
        const auto current_aspect_ratio = gfx::aspect_ratio;
        check_variables();
        if (current_aspect_ratio != gfx::aspect_ratio) {
            update_gfx_mode(false);
        }
        update_core_option_visibility();
        libretro_input_init();
    }

    /* Once C is mounted, mount the overlay */
    if (Drives['C' - 'A'] && mount_overlay && zip_content_path.empty()) {
        auto overlay_directory =
            retro_save_directory / retro_library_name / game_path.parent_path().filename();
        mount_overlay_filesystem('C', std::move(overlay_directory));
        mount_overlay = false;
    }

    // Content from a directory - a program, or a .conf that mounts C: - gets
    // the automatic gamepad mapping too, once C: is there. A directory on the
    // host can be large, so the look stops after 500 directories, as in
    // DOSBox Pure. ZIP content is looked up by AUTOMAP.COM instead, once its
    // images are mounted.
    if (!automap_checked && Drives['C' - 'A'] && zip_content_path.empty() && !mount_overlay) {
        automap_checked = true;
        if (retro::core_options[CORE_OPT_AUTO_MAPPING].toBool()) {
            automap_detect(from_u8string(game_path.stem().u8string()), 500);
        }
    }

    if (automap_take_pending()) {
        if (!content_year && automap_year() > 1970) {
            content_year = automap_year();
            check_cpu_cycle_variables();
        }
        libretro_input_init();
        retro::showOsdInfo(fmt::format("Gamepad mapped for {}", automap_title()), RETRO_MESSAGE_TYPE_NOTIFICATION);
    }

    handle_libretro_input(enable_mouse_speed_clamp);

    /* Run emulator */
    auto current_gfx_fps = render.src.fps;
    fakeTimingReset();
    while (switchThread() == ThreadSwitchReason::VideoModeChange) {
        update_gfx_mode(run_synced && render.src.fps != current_gfx_fps);
        current_gfx_fps = render.src.fps;
    }

    /* Virtual keyboard */
    if (retro_vkbd)
        print_vkbd();

    // If we have a new frame, submit it.
    if (gfx::frontbuffer_uploaded && use_frame_duping) {
        video_cb(nullptr, gfx::width, gfx::height, gfx::pitch);
    } else if (run_synced) {
        video_cb(gfx::framebuffers[0].data(), gfx::width, gfx::height, gfx::pitch);
    } else {
        video_cb(gfx::frontbuffer->data(), gfx::width, gfx::height, gfx::pitch);
    }
    gfx::frontbuffer_uploaded = true;

    upload_audio(queue_audio());

    if (use_retro_midi && have_retro_midi && retro_midi_interface.output_enabled()) {
        retro_midi_interface.flush();
    }
}

void retro_reset()
{
    restart_program(control->startup_params);
}

auto retro_get_memory_data(const unsigned type) -> void*
{
    if (type == RETRO_MEMORY_SYSTEM_RAM) {
        return GetMemBase();
    }
    return nullptr;
}

auto retro_get_memory_size(const unsigned type) -> size_t
{
    if (type == RETRO_MEMORY_SYSTEM_RAM) {
        return MEM_TotalPages() * 4096;
    }
    return 0;
}

/* Stubs */
auto retro_serialize_size() -> size_t
{
    return 0;
}

auto retro_serialize(void* const /*data*/, const size_t /*size*/) -> bool
{
    return false;
}

auto retro_unserialize(const void* const /*data*/, const size_t /*size*/) -> bool
{
    return false;
}

void retro_cheat_reset()
{ }

void retro_cheat_set(unsigned /*unused*/, bool /*unused1*/, const char* /*unused2*/)
{ }

void retro_unload_game()
{ }

auto retro_get_region() -> unsigned
{
    return RETRO_REGION_NTSC;
}

/*

Copyright (C) 2002-2018 The DOSBox Team
Copyright (C) 2015-2018 Andrés Suárez
Copyright (C) 2019 Nikos Chantziaras <realnc@gmail.com>

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
