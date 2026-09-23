// This is copyrighted software. More information is at the end of this file.
#include "Aulib/DecoderFluidsynth.h"
#include "Aulib/DecoderModplug.h"
#include "Aulib/DecoderMpg123.h"
#include "Aulib/DecoderSndfile.h"
#include "Aulib/DecoderVorbis.h"
#include "Aulib/ResamplerSpeex.h"
#include "Aulib/Stream.h"
#include "aulib_config.h"
#include "aulib_debug.h"
#include "aulib_global.h"
#include "sdl_mixer_emu.h"

// Currently active global music stream (SDL_mixer only supports one.)
static Aulib::Stream* gMusic = nullptr;
static float gMusicVolume = 1.0f;

extern "C" {
static void (*gMusicFinishHook)() = nullptr;
}

static void musicFinishHookWrapper(Aulib::Stream& /*unused*/)
{
    if (gMusicFinishHook) {
        gMusicFinishHook();
    }
}

auto Mix_LoadMUS(const char* file) -> Mix_Music*
{
    AM_debugPrintLn(__func__);

    auto strm = new Aulib::Stream(file, Aulib::Decoder::decoderFor(file),
                                  std::make_unique<Aulib::ResamplerSpeex>());
    strm->open();
    return (Mix_Music*)strm;
}

auto Mix_LoadMUS_RW(SDL_RWops* rw) -> Mix_Music*
{
    AM_debugPrintLn(__func__);

    auto strm = new Aulib::Stream(rw, Aulib::Decoder::decoderFor(rw),
                                  std::make_unique<Aulib::ResamplerSpeex>(), false);
    strm->open();
    return (Mix_Music*)strm;
}

auto Mix_LoadMUSType_RW(SDL_RWops* rw, Mix_MusicType type, int freesrc) -> Mix_Music*
{
    AM_debugPrintLn(__func__ << " type: " << type);

    auto decoder = std::unique_ptr<Aulib::Decoder>(nullptr);
    switch (type) {
    case MUS_WAV:
    case MUS_FLAC:
        decoder = std::make_unique<Aulib::DecoderSndfile>();
        break;

    case MUS_MOD:
    case MUS_MODPLUG:
        decoder = std::make_unique<Aulib::DecoderModplug>();
        break;

    case MUS_MID:
        decoder = std::make_unique<Aulib::DecoderFluidsynth>();
        break;

    case MUS_OGG:
        decoder = std::make_unique<Aulib::DecoderVorbis>();
        break;

    case MUS_MP3:
    case MUS_MP3_MAD:
        decoder = std::make_unique<Aulib::DecoderMpg123>();
        break;

    default:
        AM_debugPrintLn("NO DECODER FOUND");
        return nullptr;
    }

    auto strm = new Aulib::Stream(rw, std::move(decoder), std::make_unique<Aulib::ResamplerSpeex>(),
                                  freesrc != 0);
    strm->open();
    return (Mix_Music*)strm;
}

void Mix_FreeMusic(Mix_Music* music)
{
    AM_debugPrintLn(__func__);

    auto* strm = (Aulib::Stream*)music;
    if (gMusic == strm) {
        gMusic = nullptr;
    }
    delete strm;
}

auto Mix_GetNumMusicDecoders() -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GetMusicDecoder(int /*index*/) -> const char*
{
    AM_debugPrintLn(__func__);

    return nullptr;
}

auto Mix_GetMusicType(const Mix_Music* /*music*/) -> Mix_MusicType
{
    AM_debugPrintLn(__func__);

    return MUS_NONE;
}

void Mix_HookMusic(void (*/*mix_func*/)(void*, Uint8*, int), void* /*arg*/)
{
    AM_debugPrintLn(__func__);
}

void Mix_HookMusicFinished(void (*music_finished)())
{
    AM_debugPrintLn(__func__);

    if (not music_finished) {
        if (gMusic) {
            gMusic->unsetFinishCallback();
        }
        gMusicFinishHook = nullptr;
    } else {
        gMusicFinishHook = music_finished;
        if (gMusic) {
            gMusic->setFinishCallback(musicFinishHookWrapper);
        }
    }
}

auto Mix_GetMusicHookData() -> void*
{
    AM_debugPrintLn(__func__);

    return nullptr;
}

auto Mix_PlayMusic(Mix_Music* music, int loops) -> int
{
    AM_debugPrintLn(__func__);

    if (gMusic) {
        gMusic->stop();
        gMusic = nullptr;
    }
    if (loops == 0) {
        return 0;
    }
    gMusic = (Aulib::Stream*)music;
    gMusic->setVolume(gMusicVolume);
    return static_cast<int>(gMusic->play(loops == -1 ? 0 : loops));
}

auto Mix_FadeInMusic(Mix_Music* music, int loops, int /*ms*/) -> int
{
    AM_debugPrintLn(__func__);

    return Mix_PlayMusic(music, loops);
}

auto Mix_FadeInMusicPos(Mix_Music* /*music*/, int /*loops*/, int /*ms*/, double /*position*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_VolumeMusic(int volume) -> int
{
    AM_debugPrintLn(__func__);

    int prevVol = gMusicVolume * MIX_MAX_VOLUME;
    volume = std::min(std::max(-1, volume), MIX_MAX_VOLUME);

    if (volume >= 0) {
        gMusicVolume = (float)volume / (float)MIX_MAX_VOLUME;
        if (gMusic) {
            gMusic->setVolume(gMusicVolume);
        }
    }
    return prevVol;
}

auto Mix_HaltMusic() -> int
{
    AM_debugPrintLn(__func__);

    if (gMusic) {
        gMusic->stop();
        gMusic = nullptr;
    }
    return 0;
}

auto Mix_FadeOutMusic(int /*ms*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_FadingMusic() -> Mix_Fading
{
    AM_debugPrintLn(__func__);

    return MIX_NO_FADING;
}

void Mix_PauseMusic()
{
    AM_debugPrintLn(__func__);

    if (gMusic) {
        gMusic->pause();
    }
}

void Mix_ResumeMusic()
{
    AM_debugPrintLn(__func__);

    if (gMusic and gMusic->isPaused()) {
        gMusic->resume();
    }
}

void Mix_RewindMusic()
{
    AM_debugPrintLn(__func__);

    if (gMusic) {
        gMusic->rewind();
    }
}

auto Mix_PausedMusic() -> int
{
    AM_debugPrintLn(__func__);

    if (gMusic) {
        return static_cast<int>(gMusic->isPaused());
    }
    return 0;
}

auto Mix_SetMusicPosition(double /*position*/) -> int
{
    AM_debugPrintLn(__func__);

    return -1;
}

auto Mix_PlayingMusic() -> int
{
    AM_debugPrintLn(__func__);

    if (gMusic) {
        return static_cast<int>(gMusic->isPlaying());
    }
    return 0;
}

auto Mix_SetMusicCMD(const char* /*command*/) -> int
{
    AM_debugPrintLn(__func__);

    return -1;
}

auto Mix_SetSynchroValue(int /*value*/) -> int
{
    AM_debugPrintLn(__func__);

    return -1;
}

auto Mix_GetSynchroValue() -> int
{
    AM_debugPrintLn(__func__);

    return -1;
}

auto Mix_SetSoundFonts(const char* /*paths*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
}

auto Mix_GetSoundFonts() -> const char*
{
    AM_debugPrintLn(__func__);

    return nullptr;
}

auto Mix_EachSoundFont(int (*/*function*/)(const char*, void*), void* /*data*/) -> int
{
    AM_debugPrintLn(__func__);

    return 0;
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
