// This is copyrighted software. More information is at the end of this file.
#include "SDL_sound.h"

#include "log.h"
#include <Aulib/DecoderDrflac.h>
#include <Aulib/DecoderDrwav.h>
#include <Aulib/DecoderMpg123.h>
#include <Aulib/DecoderOpus.h>
#include <Aulib/DecoderVorbis.h>
#include <Aulib/ResamplerSpeex.h>
#include <SDL_rwops.h>
#include <algorithm>
#include <aulib.h>
#include <chrono>
#include <vector>

auto Sound_Init() -> int
{
    return Aulib::initWithoutOutput(44100, 2);
}

auto Sound_Quit() -> int
{
    Aulib::quit();
    return true;
}

auto Sound_NewSampleFromFile(
    const char* const fname, Sound_AudioInfo* const desired, const uint32_t bufferSize)
    -> Sound_Sample*
{
    using namespace Aulib;

    auto* const rwops = SDL_RWFromFile(fname, "rb");
    if (!rwops) {
        retro::logWarn("Failed to create rwops for file \"{}\". {}", fname, SDL_GetError());
        return nullptr;
    }

    std::shared_ptr<Decoder> decoder =
        Decoder::decoderFor<DecoderOpus, DecoderVorbis, DecoderDrflac, DecoderDrwav, DecoderMpg123>(
            rwops);
    if (!decoder) {
        retro::logWarn("No suitable audio decoder found for file {}.", fname);
        SDL_RWclose(rwops);
        return nullptr;
    }

    Sound_Sample* sample = new Sound_Sample;
    sample->rwops = rwops;
    sample->byte_buffer.resize(bufferSize);
    sample->float_buffer.resize(bufferSize / 2);
    sample->buffer = sample->byte_buffer.data();
    sample->buffer_size = bufferSize;
    decoder->open(sample->rwops);
    sample->decoder = std::move(decoder);
    sample->resampler.setQuality(3);
    sample->resampler.setDecoder(sample->decoder);
    sample->resampler.setSpec(desired->rate, desired->channels, sample->float_buffer.size() / 2);
    sample->flags = SOUND_SAMPLEFLAG_CANSEEK;
    return sample;
}

void Sound_FreeSample(Sound_Sample* const sample)
{
    if (!sample) {
        return;
    }
    SDL_RWclose(sample->rwops);
    delete sample;
}

auto Sound_SetBufferSize(Sound_Sample* const sample, const uint32_t new_size) -> int
{
    sample->byte_buffer.resize(new_size);
    sample->buffer = sample->byte_buffer.data();
    sample->float_buffer.resize(new_size / 2);
    sample->buffer_size = new_size;
    return true;
}

static constexpr auto floatSampleToInt16(const float src) noexcept -> int16_t
{
    return static_cast<int16_t>(std::clamp(src * 32768.f, -32768.f, 32767.f));
}

auto Sound_Decode(Sound_Sample* const sample) -> uint32_t
{
    const int float_sample_count =
        sample->resampler.resample(sample->float_buffer.data(), sample->float_buffer.size());
    auto* dst = sample->byte_buffer.data();

    for (int i = 0; i < float_sample_count; ++i) {
        const auto int_sample = floatSampleToInt16(sample->float_buffer[i]);
        memcpy(dst, &int_sample, sizeof(int_sample));
        dst += sizeof(int_sample);
    }

    static_assert(sizeof(sample->buffer[0]) == 1, "");
    return dst - sample->byte_buffer.data();
}

auto Sound_Seek(Sound_Sample* const sample, const uint32_t ms) -> int
{
    sample->resampler.discardPendingSamples();
    if (ms == 0) {
        return sample->decoder->rewind();
    }
    return sample->decoder->seekToTime(std::chrono::milliseconds{ms});
}

auto Sound_Duration(Sound_Sample* const sample) -> int
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(sample->decoder->duration()).count();
}

/*

Copyright (C) 2021 Nikos Chantziaras <realnc@gmail.com>

This file is part of DOSBox-core.

DOSBox-core is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or (at your option) any later
version.

DOSBox-core is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
DOSBox-core. If not, see <https://www.gnu.org/licenses/>.

*/
