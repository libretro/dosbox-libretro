#ifndef _LIBRETRO_DOSBOX_H
#define _LIBRETRO_DOSBOX_H

#include <stdint.h>
#include "libretro.h"

# define RETROLOG(msg) printf("%s\n", msg)

extern retro_video_refresh_t video_cb;
extern retro_audio_sample_batch_t audio_batch_cb;
extern retro_input_poll_t poll_cb;
extern retro_input_state_t input_cb;
extern retro_environment_t environ_cb;

#endif
