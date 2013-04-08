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


#include "../libco/libco.h"
#include "libretro.h"

#include "dosbox.h"
#include "mapper.h"
#include "control.h"
#include "pic.h"

#if 0
# define LOG(msg) fprintf(stderr, "%s\n", msg)
#else
# define LOG(msg)
#endif

//

static retro_video_refresh_t video_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
static retro_environment_t environ_cb = NULL;

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_environment(retro_environment_t cb) { environ_cb = cb; }

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
    LOG("Program restart not supported.");
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
    if(FRONTENDwantsExit)
    {
        throw 1;
    }
    
    // Schedule the next frontend interrupt 
    PIC_AddEvent(retro_leave_thread, 1000.0f / 60.0f, 0);
}

static void retro_start_emulator()
{
    const char* const argv[2] = {"dosbox", loadPath.c_str()};
	CommandLine com_line(loadPath.empty() ? 1 : 2, argv);
	Config myconf(&com_line);
	control=&myconf;

	/* Init the configuration system and add default values */
	DOSBOX_Init();

    /* Load config */
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
        LOG("Frontend said to quit.");
        return;
    }
    
    LOG("DOSBox said to quit.");
    DOSBOXwantsExit = true;
}

static void retro_wrap_emulator()
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
        LOG("Running a dead emulator.");
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
   info->library_name = "dosbox";
   info->library_version = "svn";
   info->valid_extensions = "exe|com|bat|conf|EXE|COM|BAT|CONF";
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
    // Get color mode: 32 first as VGA has 6 bits per pixel
    RDOSGFXcolorMode = RETRO_PIXEL_FORMAT_XRGB8888;
    if(!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &RDOSGFXcolorMode))
    {
        RDOSGFXcolorMode = RETRO_PIXEL_FORMAT_RGB565;
        if(!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &RDOSGFXcolorMode))
        {
            RDOSGFXcolorMode = RETRO_PIXEL_FORMAT_0RGB1555;
        }
    }


    if(!emuThread && !mainThread)
    {
        mainThread = co_active();
        emuThread = co_create(65536*sizeof(void*), retro_wrap_emulator);
    }
    else
    {
        LOG("retro_init called more than once.");
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
        LOG("retro_deinit called when there is no emulator thread.");
    }
}

bool retro_load_game(const struct retro_game_info *game)
{
    if(emuThread)
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
                const bool gotSysDir = environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &systemDir);
            
                configPath = normalizePath(std::string(gotSysDir ? systemDir : ".") + "/dosbox.conf");
            }
        }
        
        co_switch(emuThread);
        samplesPerFrame = MIXER_RETRO_GetFrequency() / 60;
        
        return true;
    }
    else
    {
        LOG("retro_load_game called when there is no emulator thread.");
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

void retro_run (void)
{
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
        LOG("retro_run called when there is no emulator thread.");
    }
}

// Stubs
void retro_set_controller_port_device(unsigned in_port, unsigned device) { }
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
