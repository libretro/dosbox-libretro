#ifndef SDL_FAKE_
#define SDL_FAKE_

#include <string.h>

#define SDL_VERSION_ATLEAST(...) 0

#define SDL_mutexV(...)
#define SDL_mutexP(...)
#define SDL_CreateMutex() 0
#define SDL_DestroyMutex(...)
typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned long Uint32;
typedef int SDL_mutex;

#define SDL_GetError() "FAKE"

// TIMER
#include <sys/time.h>
#include <unistd.h>

inline void SDL_Delay(unsigned x)
{
    usleep(x * 1000);
}

inline unsigned SDL_GetTicks()
{
    struct timeval t;
    gettimeofday(&t, 0);
    
    return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

// CD
#define SDL_CDOpen(...) 0
#define SDL_CDClose(...)
#define SDL_CDPlay(...) false
#define SDL_CDResume(...) false
#define SDL_CDPause(...) false
#define SDL_CDStop(...) false
#define SDL_CDEject(...) false

typedef enum {
  CD_TRAYEMPTY,
  CD_STOPPED,
  CD_PLAYING,
  CD_PAUSED,
  CD_ERROR = -1
} CDstatus;

typedef struct{
  Uint8 id;
  Uint8 type;
  Uint32 length;
  Uint32 offset;
} SDL_CDtrack;

static CDstatus cdstatH;
#define SDL_CDStatus(...) cdstatH

#define CD_INDRIVE(...) false

#define SDL_MAX_TRACKS 99

typedef struct{
  int id;
  CDstatus status;
  int numtracks;
  int cur_track;
  int cur_frame;
  SDL_CDtrack track[SDL_MAX_TRACKS+1];
} SDL_CD;

#define SDL_CDNumDrives() 0
#define SDL_CDName(x) "NONE"
#define MSF_TO_FRAMES(...) 0
#define FRAMES_TO_MSF(...) 0


#endif