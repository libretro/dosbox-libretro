/* This is copyrighted software. More information is at the end of this file. */
#pragma once
#include <SDL_endian.h>
#include <SDL_stdinc.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !SDL_VERSION_ATLEAST(2, 0, 0)
static inline float SDL_SwapFloat_missing(float x)
{
    union
    {
        float f;
        Uint32 ui32;
    } swapper;
    swapper.f = x;
    swapper.ui32 = SDL_Swap32(swapper.ui32);
    return swapper.f;
}
#endif

#define SDL_SwapFloat SDL_SwapFloat_missing

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#    define SDL_SwapFloatLE(X) (X)
#    define SDL_SwapFloatBE(X) SDL_SwapFloat(X)
#else
#    define SDL_SwapFloatLE(X) SDL_SwapFloat(X)
#    define SDL_SwapFloatBE(X) (X)
#endif

#ifdef __cplusplus
}
#endif

/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
