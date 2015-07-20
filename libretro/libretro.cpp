/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <algorithm>
#include <string>
#include <stdlib.h>

#include "../libco/libco.h"
#include "libretro.h"
#include "retrodos.h"

#include "setup.h"
#include "dosbox.h"
#include "mapper.h"
#include "control.h"
#include "pic.h"
#include "joystick.h"

#define RETRO_DEVICE_2BUTTON_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 0)                     //2 buttons, 2 axes
#define RETRO_DEVICE_2BUTTON_JOYSTICK_DPAD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)       //2 buttons, 2 axes, dpad (emulated joystick)
#define RETRO_DEVICE_2BUTTON_JOYSTICK_DPAD_ARROWS RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 2)         //2 buttons, 2 axes, dpad (emulated keystrokes)
#define RETRO_DEVICE_4BUTTON_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 3)                     //4 buttons, 4 axes
#define RETRO_DEVICE_4BUTTON_JOYSTICK_DPAD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 4)       //4 buttons, 4 axes, dpad (emulated joystick)
#define RETRO_DEVICE_4BUTTON_JOYSTICK_DPAD_ARROWS RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 5)         //4 buttons, 4 axes, dpad (emulated keystrokes)*/


int cycles_0 = 0;
int cycles_1 = 0;
int cycles_2 = 1000;
int cycles_3 = 0;
bool cycles_flag = false;

bool options_boot = true;

extern Config * control;
extern Bit32s CPU_CycleMax;
extern Bit32s CPU_CycleLimit;
extern bool CPU_CycleAutoAdjust;
extern bool CPU_SkipCycleAutoAdjust;

int current_port;

extern JoystickType joystick_type[16];
bool dpad[16];
bool connected[16];
bool emulated_kbd[16];

std::string retro_save_directory;
std::string retro_system_directory;
std::string retro_content_directory;

retro_video_refresh_t video_cb;
retro_audio_sample_batch_t audio_batch_cb;
retro_input_poll_t poll_cb;
retro_input_state_t input_cb;
retro_environment_t environ_cb;

retro_log_printf_t log_cb;

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }

void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;

    bool allow_no_game = false;
    environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &allow_no_game);

     static const struct retro_variable vars[] = {
        { "dosbox_options_on_boot", "Enable core options on boot; enabled|disabled" },
        /*{ "dosbox_cpu_cycles_auto", "CPU Cycles auto; enabled|disabled" },*/
        { "dosbox_cpu_cycles_0", "CPU Cycles coarser; 0|100000|200000|300000|400000|500000|600000|700000|800000|900000" },
        { "dosbox_cpu_cycles_1", "CPU Cycles coarse; 0|10000|20000|30000|40000|50000|60000|70000|80000|90000" },
        { "dosbox_cpu_cycles_2", "CPU Cycles fine; 1000|2000|3000|4000|5000|6000|7000|8000|9000|0" },
        { "dosbox_cpu_cycles_3", "CPU Cycles finer; 0|100|200|300|400|500|600|700|800|900" },
        { NULL, NULL },
    };

    cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

    static const struct retro_controller_description pads_p1[] = {
        { "Gamepad (2buttons)", RETRO_DEVICE_2BUTTON_JOYSTICK_DPAD },
        { "Gamepad (4buttons)", RETRO_DEVICE_4BUTTON_JOYSTICK_DPAD },
        { "Joystick (2axis/2buttons)", RETRO_DEVICE_2BUTTON_JOYSTICK },
        { "Joystick (4axis/4buttons)", RETRO_DEVICE_4BUTTON_JOYSTICK },
        { "Joystick (2axis/2buttons + kbd arrows)", RETRO_DEVICE_2BUTTON_JOYSTICK_DPAD_ARROWS },
        { "Joystick (4axis/4buttons + kbd arrows)", RETRO_DEVICE_4BUTTON_JOYSTICK_DPAD_ARROWS }
    };

    static const struct retro_controller_description pads_p2[] = {
        { "Gamepad (2buttons)", RETRO_DEVICE_2BUTTON_JOYSTICK_DPAD },
        { "Gamepad (4buttons)", RETRO_DEVICE_4BUTTON_JOYSTICK_DPAD },
        { "Joystick (2axis/2buttons)", RETRO_DEVICE_2BUTTON_JOYSTICK },
        { "Joystick (4axis/4buttons)", RETRO_DEVICE_4BUTTON_JOYSTICK }
        
    };

    static const struct retro_controller_info ports[] = {
        { pads_p1, 6 },
        { pads_p2, 4 },
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
               joystick_type[port] = JOY_NONE;
            connected[port] = false;
            dpad[port] = false;
            emulated_kbd[port] = false;
    switch (device)
    {
        case RETRO_DEVICE_2BUTTON_JOYSTICK:
        case RETRO_DEVICE_ANALOG:
        case RETRO_DEVICE_JOYPAD:
            joystick_type[port] = JOY_2AXIS;
            connected[port] = true;
            dpad[port] = false;
            emulated_kbd[port] = false;
            break;
        case RETRO_DEVICE_2BUTTON_JOYSTICK_DPAD_ARROWS:
            joystick_type[port] = JOY_2AXIS;
            connected[port] = true;
            dpad[port] = false;
            emulated_kbd[port] = true;
            break;
        case RETRO_DEVICE_2BUTTON_JOYSTICK_DPAD:
            joystick_type[port] = JOY_2AXIS;
            connected[port] = true;
            dpad[port] = true;
            emulated_kbd[port] = false;
            break;
        case RETRO_DEVICE_4BUTTON_JOYSTICK:
            joystick_type[port] = JOY_4AXIS;
            connected[port] = true;
            dpad[port] = false;
            emulated_kbd[port] = false;
            break;
        case RETRO_DEVICE_4BUTTON_JOYSTICK_DPAD_ARROWS:
            joystick_type[port] = JOY_4AXIS;
            connected[port] = true;
            dpad[port] = false;
            emulated_kbd[port] = true;
            break;
        case RETRO_DEVICE_4BUTTON_JOYSTICK_DPAD:
            joystick_type[port] = JOY_4AXIS;
            connected[port] = true;
            dpad[port] = true;
            emulated_kbd[port] = false;
            break;
        default:
            joystick_type[port] = JOY_NONE;
            connected[port] = false;
            dpad[port] = false;
            emulated_kbd[port] = false;
            break;
    }
    log_cb(RETRO_LOG_INFO,"%d\n",joystick_type[port]);
    MAPPER_Init();

}

void check_variables(void)
{

    struct retro_variable var = {0};
    var.key = "dosbox_cpu_cycles_0";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        cycles_0 = atoi(var.value);
        cycles_flag = true;

    }

    var.key = "dosbox_cpu_cycles_1";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        cycles_1 = atoi(var.value);
        cycles_flag = true;

    }

    var.key = "dosbox_cpu_cycles_2";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        cycles_2 = atoi(var.value);
        cycles_flag = true;

    }

    var.key = "dosbox_cpu_cycles_3";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        cycles_3 = atoi(var.value);
        cycles_flag = true;

    }

    var.key = "dosbox_options_on_boot";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (strcmp(var.value, "enabled") == 0)
            options_boot = true;
        if (strcmp(var.value, "disabled") == 0)
            options_boot = false;

    }

    //not working will keep this disabled for the time being
    /*var.key = "dosbox_cpu_cycles_auto";
    var.value = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (strcmp(var.value, "enabled") == 0)
        {
            CPU_CycleAutoAdjust = true;
            CPU_SkipCycleAutoAdjust = false;
            CPU_CycleLimit = 90;
        }
        if (strcmp(var.value, "disabled") == 0)
        {
            CPU_CycleAutoAdjust = false;
            CPU_SkipCycleAutoAdjust = true;
            CPU_CycleLimit = -1;
        }

    }*/

}


// input_poll and input_state are in libretro/mapper.cpp

//

// Normalize Path separators based on platform
#ifndef PATH_SEPARATOR
# if defined(WINDOWS_PATH_STYLE) || defined(_WIN32)
#  define PATH_SEPARATOR '\\'
# else
#  define PATH_SEPARATOR '/'
# endif
#endif
std::string normalizePath(const std::string& aPath)
{
    std::string result = aPath;
    for(size_t found = result.find_first_of("\\/"); std::string::npos != found; found = result.find_first_of("\\/", found + 1))
    {
        result[found] = PATH_SEPARATOR;
    }

    return result;
}

//

cothread_t mainThread;
cothread_t emuThread;

Bit32u MIXER_RETRO_GetFrequency();

bool autofire;
bool startup_state_capslock;
bool startup_state_numlock;

void Mouse_AutoLock(bool enable)
{
    // Nothing: The frontend should lock the mouse there's no way to tell it to do that though.
}

void restart_program(std::vector<std::string> & parameters)
{
    // Not supported; this is used by the CONFIG -r command.
    RETROLOG("Program restart not supported.");
}

// Events
void MIXER_CallBack(void * userdata, uint8_t *stream, int len);

static std::string loadPath;
static std::string configPath;
static uint8_t audioData[829 * 4]; // 49716hz max
static uint32_t samplesPerFrame = 735;
static bool DOSBOXwantsExit;
static bool FRONTENDwantsExit;

extern Bit8u RDOSGFXbuffer[1024*768*4];
extern Bitu RDOSGFXwidth, RDOSGFXheight, RDOSGFXpitch;
extern unsigned RDOSGFXcolorMode;
extern void* RDOSGFXhaveFrame;

static void retro_leave_thread(Bitu)
{
    MIXER_CallBack(0, audioData, samplesPerFrame * 4);

    co_switch(mainThread);

    // If the frontend said to exit, throw an int to be caught in retro_start_emulator.
    /*if(FRONTENDwantsExit)
    {
        throw 1;
    }*/

    // Schedule the next frontend interrupt
    PIC_AddEvent(retro_leave_thread, 1000.0f / 60.0f, 0);
}

static void retro_start_emulator(void)
{

    const char* const argv[2] = {"dosbox", loadPath.c_str()};
    CommandLine com_line(loadPath.empty() ? 1 : 2, argv);
    Config myconf(&com_line);
    control=&myconf;

    /* Init the configuration system and add default values */
    DOSBOX_Init();

    /* Load config */
    log_cb(RETRO_LOG_INFO,"Config path: %s\n",configPath.c_str());
    if(!configPath.empty())
    {
        control->ParseConfigFile(configPath.c_str());
    }
    /* Init all the sections */
    control->Init();

    /* Init the keyMapper */
    MAPPER_Init();

    /* Init done, go back to the main thread */
    co_switch(mainThread);

    // Schedule an interrupt time to return to the frontend
    PIC_AddEvent(retro_leave_thread, 1000.0f / 60.0f, 0);

    try
    {
        control->StartUp();
    }
    catch(int)
    {
        RETROLOG("Frontend said to quit.");
        return;
    }

    RETROLOG("DOSBox said to quit.");
    DOSBOXwantsExit = true;
}

static void retro_wrap_emulator(void)
{
    retro_start_emulator();

    // Exit comes from DOSBox, tell the frontend
    if(DOSBOXwantsExit)
    {
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0);
    }

    // Were done here
    co_switch(mainThread);

    // Dead emulator, but libco says not to return
    while(true)
    {
        RETROLOG("Running a dead emulator.");
        co_switch(mainThread);
    }
}

//

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "DOSBox";
#ifdef GIT_VERSION
    info->library_version = GIT_VERSION;
#else   
   info->library_version = "SVN";
#endif
   info->valid_extensions = "exe|com|bat|conf";
   info->need_fullpath = true;
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    // TODO
    info->geometry.base_width = 640;
    info->geometry.base_height = 400;
    info->geometry.max_width = 1024;
    info->geometry.max_height = 768;
    info->geometry.aspect_ratio = 1.333333f;
    info->timing.fps = 60.0;
    info->timing.sample_rate = (double)MIXER_RETRO_GetFrequency();
}

void retro_init (void)
{

    // initialize logger interface
    struct retro_log_callback log;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        log_cb = log.log;
    else
        log_cb = NULL;

    if (log_cb)
        log_cb(RETRO_LOG_INFO, "Logger interface initialized... \n");

    RDOSGFXcolorMode = RETRO_PIXEL_FORMAT_XRGB8888;
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &RDOSGFXcolorMode);

    if(!emuThread && !mainThread)
    {
        mainThread = co_active();
        emuThread = co_create(65536*sizeof(void*)*16, retro_wrap_emulator);
    }
    else
    {
        RETROLOG("retro_init called more than once.");
    }
}

void retro_deinit(void)
{
    FRONTENDwantsExit = !DOSBOXwantsExit;

    if(emuThread)
    {
        // If the frontend says to exit we need to let the emulator run to finish its job.
        if(FRONTENDwantsExit)
        {
            co_switch(emuThread);
        }

        co_delete(emuThread);
        emuThread = 0;
    }
    else
    {
        RETROLOG("retro_deinit called when there is no emulator thread.");
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
            // Copy the game path
            loadPath = normalizePath(game->path);
            const size_t lastDot = loadPath.find_last_of('.');

            // Find any config file to load
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
                    const char* systemDir = 0;
                    configPath = normalizePath(retro_system_directory + slash + "DOSbox" + slash + "dosbox-libretro.conf");
                }
            }
        }

        co_switch(emuThread);
        samplesPerFrame = MIXER_RETRO_GetFrequency() / 60;

        return true;
    }
    else
    {
        RETROLOG("retro_load_game called when there is no emulator thread.");
        return false;
    }
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    if(2 == num_info)
    {
        configPath = normalizePath(info[1].path);
        return retro_load_game(&info[0]);
    }
    return false;
}

void update_cpu_cycles()
{
    if(cycles_flag)
    {
        int cycles = cycles_0 + cycles_1 + cycles_2 + cycles_3;
        CPU_CycleMax=cycles;

        //update dosbox config value in case the user wants to export a config file
        char cycles_val[16];
        sprintf(cycles_val,"%d",cycles);
        Section* cpu = control->GetSection("cpu");
        Prop_multival_remain* prop = ((Section_prop*)cpu)->Get_multivalremain("cycles");
        prop->SetValue(cycles_val);
    }

    cycles_flag = false;

}

int frame = 0;

void retro_run (void)
{

    if(frame==0)
    {
        check_variables();
        if(options_boot)
        {
            cycles_flag = true;
            update_cpu_cycles();
        }
    }

    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
    {
        check_variables();
        update_cpu_cycles();
    }


    if(emuThread)
    {
        // Keys
        MAPPER_Run(false);

        // Run emu
        co_switch(emuThread);

        // Upload video: TODO: Check the CANDUPE env value
        video_cb(RDOSGFXhaveFrame, RDOSGFXwidth, RDOSGFXheight, RDOSGFXpitch);
        RDOSGFXhaveFrame = 0;

        // Upload audio: TODO: Support sample rate control
        audio_batch_cb((int16_t*)audioData, samplesPerFrame);
    }
    else
    {
        RETROLOG("retro_run called when there is no emulator thread.");
    }

    frame++;
}

// Stubs
void *retro_get_memory_data(unsigned type) { return 0; }
size_t retro_get_memory_size(unsigned type) { return 0; }
void retro_reset (void) { }
size_t retro_serialize_size (void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void * data, size_t size) { return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2) { }
void retro_unload_game (void) { }
unsigned retro_get_region (void) { return RETRO_REGION_NTSC; }
