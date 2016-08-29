#ifndef SDL_FAKE_
#define SDL_FAKE_

#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#include <retro_miscellaneous.h>

#ifdef VITA
#include <psp2/kernel/threadmgr.h>
#endif

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;

// This is used in src/ints/bios_keyboard.cpp as a workaround for caps/num lock on old SDL versions.
#define SDL_VERSION_ATLEAST(...) 0

// These are 'used' in src/dos/cdrom_image.cpp and src/dos/cdrom_ioctl_win32.cpp.
// Really they are meant to synchronize with an audio thread, but
// that isn't used in libreto so there is no need for these.
typedef int SDL_mutex;
#define SDL_CreateMutex() 0
#define SDL_DestroyMutex(...)
#define SDL_mutexV(...)
#define SDL_mutexP(...)

// TIMER
inline void SDL_Delay(unsigned ms)
{
   retro_sleep(ms);
}

inline unsigned SDL_GetTicks()
{
    struct timeval t;
    gettimeofday(&t, 0);
    
    return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

// CD
#define SDL_CDNumDrives() 0
#define SDL_CDName(x) "NONE"
struct SDL_CD {int dummy;};

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

#endif
