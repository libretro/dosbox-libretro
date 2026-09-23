#pragma once
#include "SDL_endian.h"
#include "SDL_stdinc.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include <cstdint>

// This is used in src/ints/bios_keyboard.cpp as a workaround for caps/num lock on old SDL versions.
#define SDL_VERSION_ATLEAST(...) 0

inline constexpr auto SDL_CDNumDrives() noexcept -> int
{
    return 0;
}

inline constexpr auto SDL_CDName(const int /*drive*/) noexcept -> const char*
{
    return "NONE";
}

struct SDL_CD
{ };

/** @name Frames / MSF Conversion Functions
 *  Conversion functions from frames to Minute/Second/Frames and vice versa
 */
/*@{*/
#define CD_FPS	75
#define FRAMES_TO_MSF(f, M,S,F)	{					\
	int value = f;							\
	*(F) = value%CD_FPS;						\
	value /= CD_FPS;						\
	*(S) = value%60;						\
	value /= 60;							\
	*(M) = value;							\
}
#define MSF_TO_FRAMES(M, S, F)	((M)*60*CD_FPS+(S)*CD_FPS+(F))
/*@}*/
