// This is copyrighted software. More information is at the end of this file.
#include "aulib.h"

#include "Aulib/Stream.h"
#include "aulib_debug.h"
#include "missing.h"
#include "sampleconv.h"
#include "stream_p.h"
#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_version.h>

enum class InitType
{
    None,
    NoOutput,
    Full,
};

static InitType gInitType = InitType::None;

extern "C" {
static void sdlCallback(void* /*unused*/, Uint8 out[], int outLen)
{
    Aulib::Stream_priv::fSdlCallbackImpl(nullptr, out, outLen);
}
}

auto Aulib::init(int freq, AudioFormat format, int channels, int frameSize,
                 const std::string& device) -> bool
{
    if (gInitType != InitType::None) {
        SDL_SetError("SDL_audiolib already initialized, cannot initialize again.");
        return false;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        return false;
    }

    // We only support mono and stereo at this point.
    channels = std::min(std::max(1, channels), 2);

    SDL_AudioSpec requestedSpec{};
    requestedSpec.freq = freq;
    requestedSpec.format = format;
    requestedSpec.channels = channels;
    requestedSpec.samples = frameSize;
    requestedSpec.callback = ::sdlCallback;
    Stream_priv::fAudioSpec = requestedSpec;
#if SDL_VERSION_ATLEAST(2, 0, 0)
    auto flags = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE;
#    if SDL_VERSION_ATLEAST(2, 0, 9)
    flags |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
#    endif
    Stream_priv::fDeviceId = SDL_OpenAudioDevice(device.empty() ? nullptr : device.c_str(), false,
                                                 &requestedSpec, &Stream_priv::fAudioSpec, flags);
    if (Stream_priv::fDeviceId == 0) {
        Aulib::quit();
        return false;
    }
#else
    if (SDL_OpenAudio(&requestedSpec, &Stream_priv::fAudioSpec) == -1) {
        Aulib::quit();
        return false;
    }
#endif

    AM_debugPrint("SDL initialized with sample format: ");
    switch (Stream_priv::fAudioSpec.format) {
    case AUDIO_S8:
        AM_debugPrintLn("S8");
        Stream_priv::fSampleConverter = Aulib::floatToS8;
        break;
    case AUDIO_U8:
        AM_debugPrintLn("U8");
        Stream_priv::fSampleConverter = Aulib::floatToU8;
        break;
    case AUDIO_S16LSB:
        AM_debugPrintLn("S16LSB");
        Stream_priv::fSampleConverter = Aulib::floatToS16LSB;
        break;
    case AUDIO_U16LSB:
        AM_debugPrintLn("U16LSB");
        Stream_priv::fSampleConverter = Aulib::floatToU16LSB;
        break;
    case AUDIO_S16MSB:
        AM_debugPrintLn("S16MSB");
        Stream_priv::fSampleConverter = Aulib::floatToS16MSB;
        break;
    case AUDIO_U16MSB:
        AM_debugPrintLn("U16MSB");
        Stream_priv::fSampleConverter = Aulib::floatToU16MSB;
        break;
#if SDL_VERSION_ATLEAST(2, 0, 0)
    case AUDIO_S32LSB:
        AM_debugPrintLn("S32LSB");
        Stream_priv::fSampleConverter = Aulib::floatToS32LSB;
        break;
    case AUDIO_S32MSB:
        AM_debugPrintLn("S32MSB");
        Stream_priv::fSampleConverter = Aulib::floatToS32MSB;
        break;
    case AUDIO_F32LSB:
        AM_debugPrintLn("F32LSB");
        Stream_priv::fSampleConverter = Aulib::floatToFloatLSB;
        break;
    case AUDIO_F32MSB:
        AM_debugPrintLn("F32MSB");
        Stream_priv::fSampleConverter = Aulib::floatToFloatMSB;
        break;
#endif
    default:
        AM_warnLn("Unknown audio format spec: " << Stream_priv::fAudioSpec.format);
        Aulib::quit();
        return false;
    }

#if SDL_VERSION_ATLEAST(2, 0, 0)
    SDL_PauseAudioDevice(Stream_priv::fDeviceId, false);
#else
    SDL_PauseAudio(/*pause_on=*/0);
#endif
    gInitType = InitType::Full;
    std::atexit(Aulib::quit);
    return true;
}

auto Aulib::initWithoutOutput(const int freq, const int channels) -> bool
{
    if (gInitType != InitType::None) {
        SDL_SetError("SDL_audiolib already initialized, cannot initialize again.");
        return false;
    }

    Stream_priv::fAudioSpec.freq = freq;
    Stream_priv::fAudioSpec.channels = channels;
    gInitType = InitType::NoOutput;
    std::atexit(Aulib::quit);
    return true;
}

void Aulib::quit()
{
    if (gInitType == InitType::None) {
        return;
    }
#if SDL_VERSION_ATLEAST(2, 0, 0)
    SDL_CloseAudioDevice(Stream_priv::fDeviceId);
#else
    SDL_CloseAudio();
#endif
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    Stream_priv::fSampleConverter = nullptr;
    gInitType = InitType::None;
}

auto Aulib::sampleFormat() noexcept -> AudioFormat
{
    return Stream_priv::fAudioSpec.format;
}

auto Aulib::sampleRate() noexcept -> int
{
    return Stream_priv::fAudioSpec.freq;
}

auto Aulib::channelCount() noexcept -> int
{
    return Stream_priv::fAudioSpec.channels;
}

auto Aulib::frameSize() noexcept -> int
{
    return Stream_priv::fAudioSpec.samples;
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
