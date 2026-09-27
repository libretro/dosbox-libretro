// This is copyrighted software. More information is at the end of this file.
/*
 * Minimal and slightly altered implementation of the SDL_sound API.
 */
#pragma once
#include <Aulib/ResamplerSpeex.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace Aulib {
class Decoder;
} // namespace Aulib

struct SDL_RWops;

enum Sound_SampleFlags
{
    SOUND_SAMPLEFLAG_CANSEEK = 1,
    SOUND_SAMPLEFLAG_ERROR = 1 << 30,
};

struct Sound_AudioInfo
{
    uint16_t format;
    uint8_t channels;
    uint32_t rate;
};

struct Sound_Sample
{
    char* buffer;
    std::vector<char> byte_buffer;
    std::vector<float> float_buffer;
    uint32_t buffer_size;
    Sound_SampleFlags flags;
    SDL_RWops* rwops;
    std::shared_ptr<Aulib::Decoder> decoder;
    Aulib::ResamplerSpeex resampler;
};

auto Sound_Init() -> int;
auto Sound_Quit() -> int;
auto Sound_NewSampleFromFile(const char* fname, Sound_AudioInfo* desired, uint32_t bufferSize)
    -> Sound_Sample*;
void Sound_FreeSample(Sound_Sample* sample);
auto Sound_SetBufferSize(Sound_Sample* sample, uint32_t new_size) -> int;
auto Sound_Decode(Sound_Sample* sample) -> uint32_t;
auto Sound_Seek(Sound_Sample* sample, uint32_t ms) -> int;
auto Sound_Duration(Sound_Sample* sample) -> int;

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
