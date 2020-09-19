
/*
 * Copyright (C) 2017 - 2018 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include "mapper.h"
#include "cpu.h"
#include "ppu.h"
#include "mem.h"
#include "input.h"
#include "fm2play.h"
#include "apu.h"
#include "audio.h"
#include "audio_fds.h"
#include "audio_vrc7.h"
#include "mapper_h/nsf.h"
#include "libretro.h"

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

static bool libretro_supports_bitmasks = false;

int nesEmuLoadGame(const char* filename);
void nesEmuMainLoop(void);
void nesEmuDeinit(void);
#ifdef COL_32BIT
extern uint32_t textureImage[0xF000];
#else //case COL_16BIT
extern uint16_t textureImage[0xF000];
#endif //end COL_32BIT
extern uint8_t inValReads[8];
static bool inDiskSwitch = false;
extern const char *VERSION_STRING;

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "fixNES";
   info->library_version = VERSION_STRING + 7;
   info->need_fullpath = true;
   info->block_extract = false;
   info->valid_extensions = "nes|fds|qd|nsf";
}

extern bool nesPAL;

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width    = VISIBLE_DOTS;
   info->geometry.base_height   = VISIBLE_LINES;
   info->geometry.max_width     = VISIBLE_DOTS;
   info->geometry.max_height    = VISIBLE_LINES;
   info->geometry.aspect_ratio  = 0.0f;
   info->timing.fps             = nesPAL ? 50.0070 : 60.0988;
   info->timing.sample_rate     = apuGetFrequency();
}

void retro_init(void)
{
   struct retro_log_callback log;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;
}

void retro_deinit()
{
   libretro_supports_bitmasks = false;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}
void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}
void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {}

extern bool nesEmuNSFPlayback;
void retro_reset()
{
   //will be used at the end of a frame
   if(!nesEmuNSFPlayback)
      ppuSoftReset();
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   return false;
}
void retro_cheat_reset()
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
}


bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_input_descriptor desc[] =
   {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "FDS Switch Side" },
      { 0 },
   };

   if (nesEmuLoadGame(info->path) != EXIT_SUCCESS)
      return false;

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
#ifdef COL_32BIT
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "XRGB8888 is not supported.\n");
      return false;
   }
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "RGB565 is not supported.\n");
      return false;
   }
#endif

   inDiskSwitch = false;

   return true;
}


bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info,
                             size_t num_info)

{
   return false;
}

void retro_unload_game()
{
   nesEmuDeinit();
}

unsigned retro_get_region()
{
   return nesPAL ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

extern bool emuSaveEnabled;
extern uint8_t *emuNesROM;
extern uint32_t emuNesROMsize;
extern uint8_t *emuPrgRAM;
extern uint32_t emuPrgRAMsize;
extern uint8_t audioExpansion;
void *retro_get_memory_data(unsigned id)
{
   switch(id & RETRO_MEMORY_MASK)
   {
   case RETRO_MEMORY_SAVE_RAM:
      if(emuNesROM != NULL)
      {
         if(!nesEmuNSFPlayback && (audioExpansion&EXP_FDS))
            return emuNesROM;
      }
      if(emuPrgRAM != NULL)
      {
         if(emuSaveEnabled)
			return emuPrgRAM;
      }
	  break;
   default:
      break;
   }
   return NULL;
}
extern bool emuFdsHasSideB;
size_t retro_get_memory_size(unsigned id)
{
   switch(id & RETRO_MEMORY_MASK)
   {
   case RETRO_MEMORY_SAVE_RAM:
      if(emuNesROM != NULL)
      {
         if(!nesEmuNSFPlayback && (audioExpansion&EXP_FDS))
         {
            if(emuFdsHasSideB)
               return 0x20000;
            else
               return 0x10000;
         }
      }
      if(emuPrgRAM != NULL)
      {
         if(emuSaveEnabled)
            return emuPrgRAMsize;
      }
   }
   return 0;
}

int audioUpdate()
{
   uint16_t* buffer_in = (uint16_t*)apuGetBuf();
   int samples = apuGetBufSize() / (2 * sizeof(uint16_t));
   while (samples > 512)
   {
     audio_batch_cb(buffer_in, 512);
     buffer_in += 1024;
     samples -= 512;
   }
   if(samples > 0)
      audio_batch_cb(buffer_in, samples);

   return 1;
}

static char disksysPath[4096];
FILE *doOpenFDSBIOS()
{
   const char *dir = NULL;
   if(!environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) || !dir)
      return NULL;
   snprintf(disksysPath, sizeof(disksysPath), "%s/disksys.rom", dir);
   FILE *f = fopen(disksysPath, "rb");
   return f;
}

extern bool fdsSwitch;
void retro_run()
{
   unsigned i;
   int16_t joypad_bits;

   input_poll_cb();

   if (libretro_supports_bitmasks)
      joypad_bits = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
   else
   {
      joypad_bits = 0;
      for (i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R3+1); i++)
         joypad_bits |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
   }

   inValReads[BUTTON_A]      = joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;
   inValReads[BUTTON_B]      = joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
   inValReads[BUTTON_SELECT] = joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) ? 1 : 0;
   inValReads[BUTTON_START]  = joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_START) ? 1 : 0;
   inValReads[BUTTON_RIGHT]  = joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) ? 1 : 0;
   inValReads[BUTTON_LEFT]   = joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) ? 1 : 0;
   inValReads[BUTTON_UP]     = joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_UP) ? 1 : 0;
   inValReads[BUTTON_DOWN]   = joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) ? 1 : 0;

   if(joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_L))
   {
      if(!inDiskSwitch)
      {
         fdsSwitch = true;
         inDiskSwitch = true;
      }
   }
   else
      inDiskSwitch = false;

   nesEmuMainLoop();

#ifdef COL_32BIT
   video_cb(textureImage, VISIBLE_DOTS, VISIBLE_LINES, VISIBLE_DOTS * 4);
#else //case COL_16BIT
   video_cb(textureImage, VISIBLE_DOTS, VISIBLE_LINES, VISIBLE_DOTS * 2);
#endif //end COL_32BIT
   apuUpdate();
}

unsigned retro_api_version()
{
   return RETRO_API_VERSION;
}
