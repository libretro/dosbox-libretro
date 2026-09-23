#pragma once
#include "aulib_export.h"
#include <SDL_stdinc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_RWops SDL_RWops;
#define SDL_RWsize SDL_RWsize_missing

extern AULIB_NO_EXPORT Sint64 SDL_RWsize_missing(SDL_RWops *context);

#ifdef __cplusplus
}
#endif
