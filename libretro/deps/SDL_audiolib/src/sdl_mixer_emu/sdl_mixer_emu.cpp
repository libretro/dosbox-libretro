// This is copyrighted software. More information is at the end of this file.
#include "sdl_mixer_emu.h"
#include "aulib.h"
#include "aulib_config.h"
#include "aulib_debug.h"
#include "aulib_global.h"
//#include "stream.h"

auto Mix_Linked_Version() -> const SDL_version*
{
    AM_debugPrintLn(__func__);

    static SDL_version v = {SDL_MIXER_MAJOR_VERSION, SDL_MIXER_MINOR_VERSION, SDL_MIXER_PATCHLEVEL};
    return &v;
}

auto Mix_Init(int flags) -> int
{
    AM_debugPrintLn(__func__);

    return flags;
}

void Mix_Quit()
{
    AM_debugPrintLn(__func__);

    Aulib::quit();
}

auto Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize) -> int
{
    AM_debugPrintLn(__func__);

    return Aulib::init(frequency, format, channels, chunksize);
}

auto Mix_AllocateChannels(int numchans) -> int
{
    AM_debugPrintLn(__func__);

    return numchans;
}

auto Mix_QuerySpec(int* frequency, Uint16* format, int* channels) -> int
{
    AM_debugPrintLn(__func__);

    if (frequency) {
        *frequency = Aulib::sampleRate();
    }
    if (format) {
        *format = Aulib::sampleFormat();
    }
    if (channels) {
        *channels = Aulib::channelCount();
    }
    return 1; // TODO
}

auto Mix_LoadWAV_RW(SDL_RWops* /*src*/, int /*freesrc*/) -> Mix_Chunk*
{
    AM_debugPrintLn(__func__);

    return nullptr;
}

auto Mix_QuickLoad_WAV(Uint8* /*mem*/) -> Mix_Chunk*
{
    AM_debugPrintLn(__func__);

    return nullptr;
}

auto Mix_QuickLoad_RAW(Uint8* /*mem*/, Uint32 /*len*/) -> Mix_Chunk*
{
    AM_debugPrintLn(__func__);

    return nullptr;
}

void Mix_FreeChunk(Mix_Chunk* /*chunk*/)
{
    AM_debugPrintLn(__func__);
}

auto Mix_GetNumChunkDecoders() -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GetChunkDecoder(int /*index*/) -> const char*
{
    AM_debugPrintLn(__func__);

    return nullptr;
}

void Mix_SetPostMix(void (*/*mix_func*/)(void*, Uint8*, int), void* /*arg*/)
{
    AM_debugPrintLn(__func__);
}

void Mix_ChannelFinished(void (*/*channel_finished*/)(int))
{
    AM_debugPrintLn(__func__);
}

auto Mix_RegisterEffect(int /*chan*/, Mix_EffectFunc_t /*f*/, Mix_EffectDone_t /*d*/, void* /*arg*/)
    -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_UnregisterEffect(int /*channel*/, Mix_EffectFunc_t /*f*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_UnregisterAllEffects(int /*channel*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_SetPanning(int /*channel*/, Uint8 /*left*/, Uint8 /*right*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_SetPosition(int /*channel*/, Sint16 /*angle*/, Uint8 /*distance*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_SetDistance(int /*channel*/, Uint8 /*distance*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_SetReverseStereo(int /*channel*/, int /*flip*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_ReserveChannels(int /*num*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GroupChannel(int /*which*/, int /*tag*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GroupChannels(int /*from*/, int /*to*/, int /*tag*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GroupAvailable(int /*tag*/) -> int
{
    AM_debugPrintLn(__func__);

    return -1;
}

auto Mix_GroupCount(int /*tag*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GroupOldest(int /*tag*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GroupNewer(int /*tag*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_PlayChannelTimed(int /*channel*/, Mix_Chunk* /*chunk*/, int /*loops*/, int /*ticks*/)
    -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_FadeInChannelTimed(int /*channel*/, Mix_Chunk* /*chunk*/, int /*loops*/, int /*ms*/,
                            int /*ticks*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_Volume(int /*channel*/, int /*volume*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_VolumeChunk(Mix_Chunk* /*chunk*/, int /*volume*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_HaltChannel(int /*channel*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_HaltGroup(int /*tag*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_ExpireChannel(int /*channel*/, int /*ticks*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_FadeOutChannel(int /*which*/, int /*ms*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_FadeOutGroup(int /*tag*/, int /*ms*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_FadingChannel(int /*which*/) -> Mix_Fading
{
    AM_debugPrintLn(__func__);

    return MIX_NO_FADING;
}

void Mix_Pause(int /*channel*/)
{
    AM_debugPrintLn(__func__);
}

void Mix_Resume(int /*channel*/)
{
    AM_debugPrintLn(__func__);
}

auto Mix_Paused(int /*channel*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_Playing(int /*channel*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GetChunk(int /*channel*/) -> Mix_Chunk*
{
    AM_debugPrintLn(__func__);

    return nullptr;
}

void Mix_CloseAudio()
{
    AM_debugPrintLn(__func__);
}

/*

Copyright (C) 2014, 2015, 2016, 2017, 2018, 2019 Nikos Chantziaras.

This file is part of SDL_audiolib.

SDL_audiolib is free software: you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

SDL_audiolib is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License
along with SDL_audiolib. If not, see <http://www.gnu.org/licenses/>.

*/
