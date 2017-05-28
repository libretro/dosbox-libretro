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
#include "control.h"
#include "pic.h"
#include "joystick.h"

#define RETRO_DEVICE_GAMEPAD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)
#define RETRO_DEVICE_MAPPER RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 2)

#ifndef PATH_MAX_LENGTH
#define PATH_MAX_LENGTH 4096
#endif

#define CORE_VERSION "0.74"


int cycles_0 = 0;
int cycles_1 = 1;
int cycles_2 = 0;
int cycles_3 = 0;

extern Config * control;
extern Bit32s CPU_CycleMax;
extern Bit32s CPU_CycleLimit;
extern bool CPU_CycleAutoAdjust;
extern bool CPU_SkipCycleAutoAdjust;

MachineType machine = MCH_VGA;
SVGACards svgaCard = SVGA_None;

int current_port;

bool gamepad[16]; /* true means gamepad, false means joystick */
bool connected[16];
bool mapper[16];
bool emulated_mouse;

unsigned mapper_keys[12];


const char* keyDesc[] = {
   "null", /*disables input for that button, needs an offset in the input code */
   "1","2","3","4",
   "5","6","7","8",
   "9","0","a","b",
   "c","d","e","f",
   "g","h","i","j",
   "k","l","m","n",
   "o","p","q","r",
   "s","t","u","v",
   "w","x","y","z",
   "F1","F2", "F3", "F4",
   "F5","F6", "F7", "F8",
   "F9","F10","F11","F12",
   "Escape","Tab","Bkspace",
   "Enter","Space","Left Alt",
   "Right Alt","Left Ctrl","Right Ctrl",
   "Left Shift","Right Shift","Caps Lock",
   "Scroll Lock","Num Lock"
   "-","=","\\",
   "[","]",";",
   "\"",".",",",
   "/","SysRq","PrtScn",
   "Pause","Insert","Home",
   "Page Up","Page Down","Delete",
   "End","Left","Up",
   "Down","Right",
   "KeyPad 1","KeyPad 2","KeyPad 3",
   "KeyPad 4","KeyPad 5","KeyPad 6",
   "KeyPad 7","KeyPad 8","KeyPad 9",
   "KeyPad 0","KeyPad /","KeyPad *",
   "KeyPad -","KeyPad +","KeyPad Enter",
   "KeyPad .", NULL
};

int keyId(const char *val)
{
   int i=0;
   while (keyDesc[i]!=NULL)
   {
      if (!strcmp(keyDesc[i],val))
         return i;
      i++;
   }
   return 0;
}

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

char buf[12][PATH_MAX_LENGTH];

static struct retro_variable vars[] = {
   { "dosbox_machine_type", "Machine type; vgaonly|svga_s3|svga_et3000|svga_et4000|svga_paradise|hercules|cga|tandy|pcjr|ega" },
   { "dosbox_emulated_mouse", "Gamepad emulated mouse; enable|disable" },
   { "dosbox_cpu_cycles_0", "CPU cycles x 100000; 0|1|2|3|4|5|6|7|8|9" },
   { "dosbox_cpu_cycles_1", "CPU cycles x 10000; 0|1|2|3|4|5|6|7|8|9" },
   { "dosbox_cpu_cycles_2", "CPU cycles x 1000; 1|2|3|4|5|6|7|8|9|0" },
   { "dosbox_cpu_cycles_3", "CPU cycles x 100; 0|1|2|3|4|5|6|7|8|9" },
   { "dosbox_mapper_y", buf[0] },
   { "dosbox_mapper_x", buf[1] },
   { "dosbox_mapper_b", buf[2] },
   { "dosbox_mapper_a", buf[3] },
   { "dosbox_mapper_l", buf[4] },
   { "dosbox_mapper_r", buf[5] },
   { "dosbox_mapper_up", buf[6] },
   { "dosbox_mapper_down", buf[7] },
   { "dosbox_mapper_left", buf[8] },
   { "dosbox_mapper_right", buf[9] },
   { "dosbox_mapper_select", buf[10] },
   { "dosbox_mapper_start", buf[11] },
   { NULL, NULL },
};

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool allow_no_game = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &allow_no_game);

   char keys[PATH_MAX_LENGTH];
   int i=0;
   while (keyDesc[i]!=NULL)
   {
      if (i == 0)
         strcpy (keys, keyDesc[i]);
      else
         strcat (keys, keyDesc[i]);
      if (keyDesc[i+1]!=NULL)
         strcat (keys, "|");
      i++;
   }

   snprintf(buf[0],sizeof(buf[0]), "RetroPad Y; %s",     keys);
   snprintf(buf[1],sizeof(buf[1]), "RetroPad X; %s",     keys);
   snprintf(buf[2],sizeof(buf[2]), "RetroPad B; %s",     keys);
   snprintf(buf[3],sizeof(buf[3]), "RetroPad A; %s",     keys);
   snprintf(buf[4],sizeof(buf[4]), "RetroPad L; %s",     keys);
   snprintf(buf[5],sizeof(buf[5]), "RetroPad R; %s",     keys);
   snprintf(buf[6],sizeof(buf[6]), "RetroPad Up; %s",    keys);
   snprintf(buf[7],sizeof(buf[7]), "RetroPad Down; %s",   keys);
   snprintf(buf[8],sizeof(buf[8]), "RetroPad Left; %s",   keys);
   snprintf(buf[9],sizeof(buf[9]), "RetroPad Right; %s",  keys);
   snprintf(buf[10],sizeof(buf[10]),"RetroPad Select; %s", keys);
   snprintf(buf[11],sizeof(buf[11]),"RetroPad Start; %s",  keys);



   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

   static const struct retro_controller_description pads[] =
   {
      { "Gamepad",  RETRO_DEVICE_GAMEPAD },
      { "Joystick", RETRO_DEVICE_JOYSTICK },
      { "Mapper",   RETRO_DEVICE_MAPPER },
      { 0 },
   };

   static const struct retro_controller_info ports[] = {
      { pads, 3 },
      { pads, 3 },
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
   connected[port] = false;
   gamepad[port]   = false;
   mapper[port]    = false;
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_GAMEPAD:
         connected[port] = true;
         gamepad[port] = true;
         mapper[port] = false;
         break;
      case RETRO_DEVICE_ANALOG:
      case RETRO_DEVICE_JOYSTICK:
         connected[port] = true;
         gamepad[port] = false;
         mapper[port] = false;
         break;
      case RETRO_DEVICE_MAPPER:
         connected[port] = false;
         gamepad[port] = false;
         mapper[port] = true;
         break;
      default:
         connected[port] = false;
         gamepad[port] = false;
         mapper[port] = false;
         break;
   }
   MAPPER_Init();
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
      else
      {
         machine = MCH_VGA;
         svgaCard = SVGA_None;
       }
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

   var.key = "dosbox_cpu_cycles_0";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      cycles_0 = atoi(var.value) * 100000;
      update_cycles = true;

   }

   var.key = "dosbox_cpu_cycles_1";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      cycles_1 = atoi(var.value) * 10000;
      update_cycles = true;

   }

   var.key = "dosbox_cpu_cycles_2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      cycles_2 = atoi(var.value) * 1000;
      update_cycles = true;

   }

   var.key = "dosbox_cpu_cycles_3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      cycles_3 = atoi(var.value) * 100;
      update_cycles = true;

   }
   if(update_cycles)
   {
      int cycles = cycles_0 + cycles_1 + cycles_2 + cycles_3;
      char cycles_val[16];

      CPU_CycleMax=cycles;
   }
   update_cycles = false;

   var.key = "dosbox_mapper_y";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[0] = keyId(var.value);
   }
   var.key = "dosbox_mapper_x";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
     mapper_keys[1] = keyId(var.value);
   }
   var.key = "dosbox_mapper_b";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[2] = keyId(var.value);
   }
   var.key = "dosbox_mapper_a";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[3] = keyId(var.value);
   }
   var.key = "dosbox_mapper_l";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[4] = keyId(var.value);
   }
   var.key = "dosbox_mapper_r";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[5] = keyId(var.value);
   }
   var.key = "dosbox_mapper_up";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[6] = keyId(var.value);
   }
   var.key = "dosbox_mapper_down";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[7] = keyId(var.value);
   }
   var.key = "dosbox_mapper_left";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[8] = keyId(var.value);
   }
   var.key = "dosbox_mapper_right";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[9] = keyId(var.value);
   }
   var.key = "dosbox_mapper_select";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[10] = keyId(var.value);
   }
   var.key = "dosbox_mapper_start";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[11] = keyId(var.value);
   }
   /* Init the keyMapper */
   MAPPER_Init();
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

unsigned currentWidth, currentHeight;

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
   bool ret;

   check_variables();
   /* Init the configuration system and add default values */
   DOSBOX_Init();

   /* Load config */
   if(!configPath.empty())
      ret = control->ParseConfigFile(configPath.c_str());

   /* Init all the sections */
   control->Init();

   /* Init the keyMapper */
   //MAPPER_Init();

   check_variables();

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
   info->library_version = CORE_VERSION GIT_VERSION;
#else
   info->library_version = CORE_VERSION;
#endif
   info->valid_extensions = "exe|com|bat|conf";
   info->need_fullpath = true;
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width = 320;//vga default width
   info->geometry.base_height = 200;//vga default height
   info->geometry.max_width = 1024;
   info->geometry.max_height = 768;
   info->geometry.aspect_ratio = (float)4/3;
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
      RETROLOG("retro_load_game called when there is no emulator thread.");
      return false;
   }
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   if(num_info == 2)
   {
      configPath = normalizePath(info[1].path);
      return retro_load_game(&info[0]);
   }
   return false;
}



void retro_run (void)
{

   if (RDOSGFXwidth != currentWidth || RDOSGFXheight != currentHeight)
   {
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
   {
      check_variables();
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
