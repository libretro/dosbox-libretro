// This is copyrighted software. More information is at the end of this file.
#include "libretro_audio.h"
#include "SDL_stdinc.h"
#include "libretro.h"
#include "log.h"
#include "mixer.h"
#include <vector>

namespace audio {

static retro_audio_sample_batch_t batch_cb;
static std::vector<int16_t> buffer;

} // namespace audio

void init_audio()
{
    audio::buffer.reserve(4096);
}

auto queue_audio() -> Bitu
{
    const auto available_audio_frames = MIXER_RETRO_GetAvailableFrames();

    if (available_audio_frames > 0) {
        const auto samples = available_audio_frames * 2;
        const auto bytes = available_audio_frames * 4;

        if (samples > audio::buffer.capacity()) {
            audio::buffer.reserve(samples);
            retro::logDebug("Output audio buffer resized to {} samples.\n", samples);
        }
        MIXER_CallBack(nullptr, reinterpret_cast<Uint8*>(audio::buffer.data()), bytes);
    }
    return available_audio_frames;
}

void upload_audio(int len_frames) noexcept
{
    auto* src = audio::buffer.data();

    while (len_frames > 0) {
        const auto uploaded_frames = audio::batch_cb(src, len_frames);
        src += uploaded_frames * 2;
        len_frames -= uploaded_frames;
    }
}

void retro_set_audio_sample(retro_audio_sample_t)
{ }

void retro_set_audio_sample_batch(const retro_audio_sample_batch_t cb)
{
    audio::batch_cb = cb;
}

/*

Copyright (C) 2022 Nikos Chantziaras <realnc@gmail.com>

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
