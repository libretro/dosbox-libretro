/*
 *  Copyright (C) 2002-2018 - The DOSBox Team
 *  Copyright (C) 2015-2018 - Andrés Suárez
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <algorithm>
#include <string>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <libco.h>
#include "libretro.h"
#include "retrodos.h"

#include "setup.h"
#include "dosbox.h"
#include "mapper.h"
#include "mixer.h"
#include "control.h"
#include "pic.h"
#include "joystick.h"
#include "ints/int10.h"

#define RETRO_DEVICE_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)

#ifndef PATH_MAX_LENGTH
#define PATH_MAX_LENGTH 4096
#endif

#define CORE_VERSION "0.74"

#ifndef PATH_SEPARATOR
#if defined(WINDOWS_PATH_STYLE) || defined(_WIN32)
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif
#endif

cothread_t mainThread;
cothread_t emuThread;

Bit32u MIXER_RETRO_GetFrequency();
void MIXER_CallBack(void * userdata, uint8_t *stream, int len);

unsigned cycles;
unsigned cycles_multiplier;

extern Config * control;
MachineType machine = MCH_VGA;
SVGACards svgaCard = SVGA_None;

/* input variables */
int current_port;
bool autofire;
bool gamepad[16]; /* true means gamepad, false means joystick */
bool connected[16];
bool emulated_mouse;

/* directories */
std::string retro_save_directory;
std::string retro_system_directory;
std::string retro_content_directory;

/* libretro variables */
retro_video_refresh_t video_cb;
retro_audio_sample_batch_t audio_batch_cb;
retro_input_poll_t poll_cb;
retro_input_state_t input_cb;
retro_environment_t environ_cb;
retro_log_printf_t log_cb;
extern struct retro_midi_interface *retro_midi_interface;

/* DOSBox state */
static std::string loadPath;
static std::string configPath;
static bool dosbox_exit;
static bool frontend_exit;
static bool is_restarting = false;

/* video variables */
extern Bit8u RDOSGFXbuffer[1024*768*4];
extern Bitu RDOSGFXwidth, RDOSGFXheight, RDOSGFXpitch;
extern unsigned RDOSGFXcolorMode;
extern void* RDOSGFXhaveFrame;
unsigned currentWidth, currentHeight;

/* audio variables */
static uint8_t audioData[829 * 4]; // 49716hz max
static uint32_t samplesPerFrame = 735;

/* callbacks */
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }

/* helper functions */
bool update_dosbox_variable(std::string section_string, std::string var_string, std::string val_string)
{
    bool ret = false;

    Section* section = control->GetSection(section_string);
    Section_prop *secprop = static_cast <Section_prop*>(section);
    if (secprop)
    {
        section->ExecuteDestroy(false);
        std::string inputline = var_string + "=" + val_string;
        ret = section->HandleInputline(inputline.c_str());
        section->ExecuteInit(false);
    }
    return ret;
}

void check_variables()
{
    struct retro_variable var = {0};

    bool update_cycles = false;

    var.key = "dosbox_machine_type";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (strcmp(var.value, "hercules") == 0)
            machine = MCH_HERC;
        else if (strcmp(var.value, "cga") == 0)
            machine = MCH_CGA;
        else if (strcmp(var.value, "pcjr") == 0)
            machine = MCH_PCJR;
        else if (strcmp(var.value, "tandy") == 0)
            machine = MCH_TANDY;
        else if (strcmp(var.value, "ega") == 0)
            machine = MCH_EGA;
        else if (strcmp(var.value, "svga_s3") == 0)
        {
            machine = MCH_VGA;
            svgaCard = SVGA_S3Trio;
        }
        else if (strcmp(var.value, "svga_et4000") == 0)
        {
            machine = MCH_VGA;
            svgaCard = SVGA_TsengET4K;
        }
        else if (strcmp(var.value, "svga_et3000") == 0)
        {
            machine = MCH_VGA;
            svgaCard = SVGA_TsengET3K;
        }
        else if (strcmp(var.value, "svga_paradise") == 0)
        {
            machine = MCH_VGA;
            svgaCard = SVGA_ParadisePVGA1A;
        }
        else if (strcmp(var.value, "vesa_nolfb") == 0)
        {
            machine = MCH_VGA;
            svgaCard = SVGA_S3Trio;
            int10.vesa_nolfb = true;
        }
        else if (strcmp(var.value, "vesa_nolfb") == 0)
        {
            machine = MCH_VGA;
            svgaCard = SVGA_S3Trio;
            int10.vesa_oldvbe = true;
        }
        else
        {
            machine = MCH_VGA;
            svgaCard = SVGA_None;
        }
        update_dosbox_variable("dosbox", "machine", var.value);
    }

    var.key = "dosbox_emulated_mouse";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (strcmp(var.value, "enable") == 0)
            emulated_mouse = true;
        else
            emulated_mouse = false;
    }

/*
    var.key = "dosbox_cpu_cycles_mode";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (strcmp(var.value, "auto") == 0)
            update_dosbox_variable("cpu", "cycles", "auto");
        else if (strcmp(var.value, "max") == 0)
            update_dosbox_variable("cpu", "cycles", "max");
        else
        {
            char s[8];
            snprintf(s, sizeof(s), "%d", cycles * cycles_multiplier);
            update_dosbox_variable("cpu", "cycles", s);
        }
    }
*/
    var.key = "dosbox_cpu_cycles";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        cycles = atoi(var.value);

        char s[8];
        snprintf(s, sizeof(s), "%d", cycles * cycles_multiplier);
        update_dosbox_variable("cpu", "cycles", s);
    }


    var.key = "dosbox_cpu_cycles_multiplier";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        cycles_multiplier = atoi(var.value);

       char s[8];
        snprintf(s, sizeof(s), "%d", cycles * cycles_multiplier);
        update_dosbox_variable("cpu", "cycles", s);
    }


    var.key = "dosbox_scaler";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        update_dosbox_variable("render", "scaler", var.value);
    /* TO-DO: Only Reinit Mapper on Mouse Core option */
    MAPPER_Init();
}

static void leave_thread(Bitu)
{
    MIXER_CallBack(0, audioData, samplesPerFrame * 4);
    co_switch(mainThread);

    /* Schedule the next frontend interrupt */
    PIC_AddEvent(leave_thread, 1000.0f / 60.0f, 0);
}

static void start_dosbox(void)
{

    const char* const argv[2] = {"dosbox", loadPath.c_str()};
    CommandLine com_line(loadPath.empty() ? 1 : 2, argv);
    Config myconf(&com_line);
    control = &myconf;
    check_variables();

    /* Init the configuration system and add default values */
    DOSBOX_Init();

    /* Load config */
    if(!configPath.empty())
        control->ParseConfigFile(configPath.c_str());

    if (!is_restarting)
        control->Init();
    check_variables();

    /* Init done, go back to the main thread */
    co_switch(mainThread);

    /* Schedule the next frontend interrupt */
    PIC_AddEvent(leave_thread, 1000.0f / 60.0f, 0);

    try
    {
        control->StartUp();
    }
    catch(int)
    {
        if (log_cb)
            log_cb(RETRO_LOG_WARN, "Frontend asked to exit\n");
        return;
    }

    if (log_cb)
        log_cb(RETRO_LOG_WARN, "DOSBox asked to exit\n");

    dosbox_exit = true;
}

static void wrap_dosbox()
{
    start_dosbox();

    co_switch(mainThread);

    /* Dead emulator */
    while(true)
    {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Running a dead DOSBox instance\n");
        co_switch(mainThread);
    }
}

void init_threads(void)
{
    if(!emuThread && !mainThread)
    {
        mainThread = co_active();
#ifdef __GENODE__
        emuThread = co_create((1<<18)*sizeof(void*), wrap_dosbox);
#else
        emuThread = co_create(65536*sizeof(void*)*16, wrap_dosbox);
#endif
    }
    else
    {
        if (log_cb)
            log_cb(RETRO_LOG_WARN, "Init called more than once \n");
    }
}

void restart_program(std::vector<std::string> & parameters)
{

    if (log_cb)
        log_cb(RETRO_LOG_WARN, "Program restart not supported\n");

    return;

    /* TO-DO: this kinda works but it's still not working 100% hence the early return*/
    if(emuThread)
    {
        /* If the frontend wants to exit we need to let the emulator
           run to finish its job. */
        if(frontend_exit)
            co_switch(emuThread);

        co_delete(emuThread);
        emuThread = NULL;
    }

    co_delete(mainThread);
    mainThread = NULL;

    is_restarting = true;
    init_threads();
}

std::string normalize_path(const std::string& aPath)
{
    std::string result = aPath;
    for(size_t found = result.find_first_of("\\/"); std::string::npos != found; found = result.find_first_of("\\/", found + 1))
    {
        result[found] = PATH_SEPARATOR;
    }

    return result;
}

/* libretro core implementation */
static struct retro_variable vars[] = {
    { "dosbox_machine_type",          "Emulated machine; svga_s3|svga_et3000|svga_et4000|svga_paradise|vesa_nolfb|vesa_oldvbe|hercules|cga|tandy|pcjr|ega|vgaonly" },
    { "dosbox_scaler",                "Scaler; none|normal2x|normal3x|advmame2x|advmame3x|advinterp2x|advinterp3x|hq2x|hq3x|2xsai|super2xsai|supereagle|tv2x|tv3x|rgb2x|rgb3x|scan2x|scan3x" },
    { "dosbox_emulated_mouse",        "Gamepad emulated mouse; enable|disable" },
    { "dosbox_cpu_core",              "CPU core; auto|dynamic|normal|simple" },
    { "dosbox_cpu_type",              "CPU type; auto|386|386_slow|486|486_slow|pentium_slow|pentium|pentium_mmx|386_prefetch" },
    //{ "dosbox_cpu_cycles_mode",       "CPU cycle mode; fixed" },//
    { "dosbox_cpu_cycles_multiplier", "CPU cycle multiplier; 1000|10000|100000|100" },
    { "dosbox_cpu_cycles",            "CPU cycles; 1|2|3|4|5|6|7|8|9" },
    { NULL, NULL },
};

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;

    bool allow_no_game = true;
    environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &allow_no_game);

    cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

    static const struct retro_controller_description ports_default[] =
    {
        { "Keyboard + Mouse",    RETRO_DEVICE_KEYBOARD },
        { "Gamepad",             RETRO_DEVICE_JOYPAD },
        { "Joystick",            RETRO_DEVICE_JOYSTICK },
        { "Disconnected",        RETRO_DEVICE_NONE },
        { 0 },
    };
    static const struct retro_controller_description ports_keyboard[] =
    {
        { "Keyboard + Mouse", RETRO_DEVICE_KEYBOARD },
        { "Disconnected",     RETRO_DEVICE_NONE },
        { 0 },
    };

    static const struct retro_controller_info ports[] = {
        { ports_default,  4 },
        { ports_default,  4 },
        { ports_keyboard, 2 },
        { ports_keyboard, 2 },
        { ports_keyboard, 2 },
        { ports_keyboard, 2 },
        { 0 },
    };
    environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

    const char *system_dir = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
        retro_system_directory=system_dir;
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "SYSTEM_DIRECTORY: %s\n", retro_system_directory.c_str());

    const char *save_dir = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
        retro_save_directory = save_dir;
    else
        retro_save_directory=retro_system_directory;
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "SAVE_DIRECTORY: %s\n", retro_save_directory.c_str());

    const char *content_dir = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
        retro_content_directory=content_dir;
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "CONTENT_DIRECTORY: %s\n", retro_content_directory.c_str());
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{

    if (port > 0)
        return;
    connected[port] = false;
    gamepad[port]	= false;
    switch (device)
    {
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
    MAPPER_Init();
}

void retro_get_system_info(struct retro_system_info *info)
{
    info->library_name = "DOSBox";
#if defined(GIT_VERSION) && defined(SVN_VERSION)
    info->library_version = CORE_VERSION SVN_VERSION GIT_VERSION;
elif defined(GIT_VERSION)
    info->library_version = CORE_VERSION SVN_VERSION
#else
    info->library_version = CORE_VERSION;
#endif
    info->valid_extensions = "exe|com|bat|conf";
    info->need_fullpath = true;
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->geometry.base_width = 320;
    info->geometry.base_height = 200;
    info->geometry.max_width = 1024;
    info->geometry.max_height = 768;
    info->geometry.aspect_ratio = (float)4/3;
    info->timing.fps = 60.0;
    info->timing.sample_rate = (double)MIXER_RETRO_GetFrequency();
}

void retro_init (void)
{
    /* Initialize logger interface */
    struct retro_log_callback log;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        log_cb = log.log;
    else
        log_cb = NULL;

    if (log_cb)
        log_cb(RETRO_LOG_INFO, "Logger interface initialized\n");

    static struct retro_midi_interface midi_interface;
    if(environ_cb(RETRO_ENVIRONMENT_GET_MIDI_INTERFACE, &midi_interface))
        retro_midi_interface = &midi_interface;
    else
        retro_midi_interface = NULL;

    if (log_cb)
        log_cb(RETRO_LOG_INFO, "MIDI interface %s.\n",
            retro_midi_interface ? "initialized" : "unavailable\n");

    RDOSGFXcolorMode = RETRO_PIXEL_FORMAT_XRGB8888;
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &RDOSGFXcolorMode);

    init_threads();
}

void retro_deinit(void)
{
    frontend_exit = !dosbox_exit;

    if(emuThread)
    {
        /* If the frontend wants to exit we need to let the emulator
           run to finish its job. */
        if(frontend_exit)
            co_switch(emuThread);

        co_delete(emuThread);
        emuThread = 0;
    }
}

bool retro_load_game(const struct retro_game_info *game)
{
char slash;
#ifdef _WIN32
    slash = '\\';
#else
    slash = '/';
#endif

    if(emuThread)
    {
        if(game)
        {
            /* Copy the game path */
            loadPath = normalize_path(game->path);
            const size_t lastDot = loadPath.find_last_of('.');

            /* Find any config file to load */
            if(std::string::npos != lastDot)
            {
                std::string extension = loadPath.substr(lastDot + 1);
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                if((extension == "conf"))
                {
                    configPath = loadPath;
                    loadPath.clear();
                }
                else if(configPath.empty())
                {
                    configPath = normalize_path(retro_system_directory + slash + "DOSbox" + slash + "dosbox-libretro.conf");
                    if(log_cb)
                        log_cb(RETRO_LOG_INFO, "Loading default configuration %s\n", configPath.c_str());
                }
            }
        }

        co_switch(emuThread);
        samplesPerFrame = MIXER_RETRO_GetFrequency() / 60;
        return true;
    }
    else
    {
        if(log_cb)
            log_cb(RETRO_LOG_WARN, "Load game called without emulator thread\n");
        return false;
    }
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    return false;
}

void retro_run (void)
{
    /* TO-DO: Add a core option for this */
    if (dosbox_exit && emuThread) {
        co_delete(emuThread);
        emuThread = NULL;
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0);
        return;
    }

    /* Dynamic resolution switching */
    if (RDOSGFXwidth != currentWidth || RDOSGFXheight != currentHeight)
    {
        if (log_cb)
            log_cb(RETRO_LOG_INFO,"Resolution changed %dx%d => %dx%d\n",
                currentWidth, currentHeight, RDOSGFXwidth, RDOSGFXheight);
        struct retro_system_av_info new_av_info;
        retro_get_system_av_info(&new_av_info);
        new_av_info.geometry.base_width = RDOSGFXwidth;
        new_av_info.geometry.base_height = RDOSGFXheight;
        environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &new_av_info);
        currentWidth = RDOSGFXwidth;
        currentHeight = RDOSGFXheight;
    }

    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        check_variables();

    if(emuThread)
    {
        /* Read input */
        MAPPER_Run(false);

        /* Run emulator */
        co_switch(emuThread);

        /* Upload video */
        video_cb(RDOSGFXhaveFrame, RDOSGFXwidth, RDOSGFXheight, RDOSGFXpitch);
        RDOSGFXhaveFrame = 0;

        /* Upload audio */
        audio_batch_cb((int16_t*)audioData, samplesPerFrame);
    }
    else
    {
        if (log_cb)
            log_cb(RETRO_LOG_WARN, "Run called without emulator thread\n");
    }
    if (retro_midi_interface && retro_midi_interface->output_enabled())
        retro_midi_interface->flush();
}

void retro_reset (void)
{
    restart_program(control->startup_params);
}

/* Stubs */
void *retro_get_memory_data(unsigned type) { return 0; }
size_t retro_get_memory_size(unsigned type) { return 0; }
size_t retro_serialize_size (void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void * data, size_t size) { return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2) { }
void retro_unload_game (void) { }
unsigned retro_get_region (void) { return RETRO_REGION_NTSC; }

bool startup_state_capslock;
bool startup_state_numlock;