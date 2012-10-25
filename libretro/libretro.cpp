/*
 *  Copyright (C) 2002-2011  The DOSBox Team
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
#include "mouse.h"
#include "mapper.h"
#include "keyboard.h"
#include "control.h"
#include "pic.h"

#if 0
# define LOG(msg) fprintf(stderr, "%s\n", msg)
#else
# define LOG(msg)
#endif

//

static retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_environment_t environ_cb = NULL;

//

cothread_t mainThread;
cothread_t emuThread;

// RETROK->KBD Map
static struct 
{
    unsigned retroID;
    KBD_KEYS dosboxID;
    bool down;
} keyMap[] = 
{
    {RETROK_1, KBD_1}, {RETROK_2, KBD_2}, {RETROK_3, KBD_3}, {RETROK_4, KBD_4},
    {RETROK_5, KBD_5}, {RETROK_6, KBD_6}, {RETROK_7, KBD_7}, {RETROK_8, KBD_8},
    {RETROK_9, KBD_9}, {RETROK_0, KBD_0}, {RETROK_a, KBD_a}, {RETROK_b, KBD_b},
    {RETROK_c, KBD_c}, {RETROK_d, KBD_d}, {RETROK_e, KBD_e}, {RETROK_f, KBD_f},
    {RETROK_g, KBD_g}, {RETROK_h, KBD_h}, {RETROK_i, KBD_i}, {RETROK_j, KBD_j},
    {RETROK_k, KBD_k}, {RETROK_l, KBD_l}, {RETROK_m, KBD_m}, {RETROK_n, KBD_n},
    {RETROK_o, KBD_o}, {RETROK_p, KBD_p}, {RETROK_q, KBD_q}, {RETROK_r, KBD_r},
    {RETROK_s, KBD_s}, {RETROK_t, KBD_t}, {RETROK_u, KBD_u}, {RETROK_v, KBD_v},
    {RETROK_w, KBD_w}, {RETROK_x, KBD_x}, {RETROK_y, KBD_y}, {RETROK_z, KBD_z},
    {RETROK_F1, KBD_f1}, {RETROK_F2, KBD_f2}, {RETROK_F3, KBD_f3}, 
    {RETROK_F4, KBD_f4}, {RETROK_F5, KBD_f5}, {RETROK_F6, KBD_f6}, 
    {RETROK_F7, KBD_f7}, {RETROK_F8, KBD_f8}, {RETROK_F9, KBD_f9}, 
    {RETROK_F10, KBD_f10}, {RETROK_F11, KBD_f11}, {RETROK_F12, KBD_f12}, 
    {RETROK_ESCAPE, KBD_esc}, {RETROK_TAB, KBD_tab}, {RETROK_BACKSPACE, KBD_backspace},
    {RETROK_RETURN, KBD_enter}, {RETROK_SPACE, KBD_space}, {RETROK_LALT, KBD_leftalt},
    {RETROK_RALT, KBD_rightalt}, {RETROK_LCTRL, KBD_leftctrl}, {RETROK_RCTRL, KBD_rightctrl},
    {RETROK_LSHIFT, KBD_leftshift}, {RETROK_RSHIFT, KBD_rightshift},
    {RETROK_CAPSLOCK, KBD_capslock}, {RETROK_SCROLLOCK, KBD_scrolllock},
    {RETROK_NUMLOCK, KBD_numlock}, {RETROK_MINUS, KBD_minus}, {RETROK_EQUALS, KBD_equals},
    {RETROK_BACKSLASH, KBD_backslash}, {RETROK_LEFTBRACKET, KBD_leftbracket},
    {RETROK_RIGHTBRACKET, KBD_rightbracket}, {RETROK_SEMICOLON, KBD_semicolon},
    {RETROK_QUOTE, KBD_quote}, {RETROK_PERIOD, KBD_period}, {RETROK_COMMA, KBD_comma},
    {RETROK_SLASH, KBD_slash}, {RETROK_SYSREQ, KBD_printscreen}, {RETROK_PAUSE, KBD_pause},
    {RETROK_INSERT, KBD_insert}, {RETROK_HOME, KBD_home}, {RETROK_PAGEUP, KBD_pageup},
    {RETROK_PAGEDOWN, KBD_pagedown}, {RETROK_DELETE, KBD_delete}, {RETROK_END, KBD_end},
    {RETROK_LEFT, KBD_left}, {RETROK_UP, KBD_up}, {RETROK_DOWN, KBD_down}, {RETROK_RIGHT, KBD_right},
    {RETROK_KP1, KBD_kp1}, {RETROK_KP2, KBD_kp2}, {RETROK_KP3, KBD_kp3}, {RETROK_KP4, KBD_kp4}, 
    {RETROK_KP5, KBD_kp5}, {RETROK_KP6, KBD_kp6}, {RETROK_KP7, KBD_kp7}, {RETROK_KP8, KBD_kp8}, 
    {RETROK_KP9, KBD_kp9}, {RETROK_KP0, KBD_kp0}, {RETROK_KP_DIVIDE, KBD_kpdivide},
    {RETROK_KP_MULTIPLY, KBD_kpmultiply}, {RETROK_KP_MINUS, KBD_kpminus},
    {RETROK_KP_PLUS, KBD_kpplus}, {RETROK_KP_ENTER, KBD_kpenter}, {RETROK_KP_PERIOD, KBD_kpperiod}
};

// Mouse Button Handler
template<int I>
struct ButtonHandler
{
    bool down;
    
    ButtonHandler() : down(false) {}
    
    void process(bool aDown)
    {
        if(aDown && !down)
        {
            Mouse_ButtonPressed(I);
        }
        else if(!aDown && down)
        {
            Mouse_ButtonReleased(I);
        }
        
        down = aDown;
    }
};

Bit32u MIXER_RETRO_GetFrequency();

bool autofire;
bool startup_state_capslock;
bool startup_state_numlock;

void Mouse_AutoLock(bool enable)
{
    // Nothing
}

void restart_program(std::vector<std::string> & parameters)
{
    // Nothing
}

// MAPPER
void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key,Bitu mods,char const * const eventname,char const * const buttonname)
{

}

void MAPPER_Init(void)
{

}

// MIDI
bool MIDI_Available()
{
    return false;
}

void MIDI_RawOutByte(unsigned char)
{
    // Nothing
}

void MIDI_Init(Section*)
{
    // Nothing
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
extern Bitu RDOSGFXwidth, RDOSGFXheight;
extern unsigned RDOSGFXcolorMode;
extern bool RDOSGFXhaveFrame;

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

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_cb = cb;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "dosbox";
   info->library_version = "svn";
   info->valid_extensions = "exe";
   info->need_fullpath = true;
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    // TODO
    info->geometry.base_width = 640;
    info->geometry.base_height = 480;
    info->geometry.max_width = 640;
    info->geometry.max_height = 480;
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
        loadPath = game->path;
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
            else
            {
                const char* systemDir = 0;
                const bool gotSysDir = environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &systemDir);
            
                configPath = std::string(gotSysDir ? systemDir : ".") + "/dosbox.conf";
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

void retro_run (void)
{
    if(emuThread)
    {
        poll_cb();
    
        // Mouse movement
        const int16_t mouseX = input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
        const int16_t mouseY = input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
    
        if(mouseX || mouseY)
        {
            Mouse_CursorMoved(mouseX, mouseY, 0, 0, true);
        }
    
        // Mouse Buttons
        static ButtonHandler<0> left;
        left.process(input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT));
            
        static ButtonHandler<1> right;
        right.process(input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT));
        
        // Keys
        for(int i = 0; i != sizeof(keyMap) / sizeof(keyMap[0]); i ++)
        {
            const bool downNow = input_cb(0, RETRO_DEVICE_KEYBOARD, 0, keyMap[i].retroID);
        
            if(downNow && !keyMap[i].down)
            {
                KEYBOARD_AddKey(keyMap[i].dosboxID, true);
            }
            else if(!downNow && keyMap[i].down)
            {
                KEYBOARD_AddKey(keyMap[i].dosboxID, false);
            }
            
            keyMap[i].down = downNow;
        }
        
        // Run emu
        co_switch(emuThread);
    
        // Upload video: TODO: Check the CANDUPE env value
        if(RDOSGFXhaveFrame && RDOSGFXwidth && RDOSGFXheight)
        {
            video_cb(RDOSGFXbuffer, RDOSGFXwidth, RDOSGFXheight, RDOSGFXwidth * ((RETRO_PIXEL_FORMAT_XRGB8888 == RDOSGFXcolorMode) ? 4 : 2));
            RDOSGFXhaveFrame = false;
        }
    
        // Upload audio: TODO: Support sample rate control
        audio_batch_cb((int16_t*)audioData, samplesPerFrame);
    }
    else
    {
        LOG("retro_run called when there is no emulator thread.");
    }
}

// Stubs
void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
    //TODO
}

void *retro_get_memory_data(unsigned type)
{
   return 0;
}

size_t retro_get_memory_size(unsigned type)
{
   return 0;
}

void retro_reset (void)
{
}

size_t retro_serialize_size (void)
{
    return 0;
}

bool retro_serialize(void *data, size_t size)
{
    return false;
}

bool retro_unserialize(const void * data, size_t size)
{
    return false;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned unused, bool unused1, const char* unused2)
{}

bool retro_load_game_special(
  unsigned game_type,
  const struct retro_game_info *info, size_t num_info
)
{ return false; }

void retro_unload_game (void)
{
}

unsigned retro_get_region (void)
{ 
   return RETRO_REGION_NTSC; 
}
