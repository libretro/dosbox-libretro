// This is copyrighted software. More information is at the end of this file.
#include "Aulib/DecoderDrmp3.h"

#define DR_MP3_NO_STDIO

#include "aulib_debug.h"
#include "dr_mp3.h"
#include "missing.h"
#include <SDL_rwops.h>

namespace chrono = std::chrono;

extern "C" {

static auto drmp3ReadCb(void* const rwops, void* const dst, const size_t len) -> size_t
{
    return SDL_RWread(static_cast<SDL_RWops*>(rwops), dst, 1, len);
}

static auto drmp3SeekCb(void* const rwops_void, const int offset, const drmp3_seek_origin origin)
    -> drmp3_bool32
{
    SDL_ClearError();

    auto* const rwops = static_cast<SDL_RWops*>(rwops_void);
    const auto rwops_size = SDL_RWsize(rwops);
    const auto cur_pos = SDL_RWtell(rwops);

    auto seekIsPastEof = [=] {
        const int abs_offset = offset + (origin == drmp3_seek_origin_current ? cur_pos : 0);
        return abs_offset >= rwops_size;
    };

    if (rwops_size < 0) {
        AM_warnLn("dr_mp3: Cannot determine rwops size: " << (rwops_size == -1 ? "unknown error"
                                                                               : SDL_GetError()));
        return false;
    }
    if (cur_pos < 0) {
        AM_warnLn("dr_mp3: Cannot tell current rwops position.");
        return false;
    }

    int whence;
    switch (origin) {
    case drmp3_seek_origin_start:
        whence = RW_SEEK_SET;
        break;
    case drmp3_seek_origin_current:
        whence = RW_SEEK_CUR;
        break;
    default:
        AM_warnLn("dr_mp3: Unrecognized origin in seek callback.");
        return false;
    }
    return not seekIsPastEof() and SDL_RWseek(rwops, offset, whence) >= 0;
}

} // extern "C"

namespace Aulib {

struct DecoderDrmp3_priv final
{
    drmp3 handle_{};
    std::chrono::microseconds duration_{};
    bool fEOF = false;
};

} // namespace Aulib

Aulib::DecoderDrmp3::DecoderDrmp3()
    : d(std::make_unique<DecoderDrmp3_priv>())
{}

Aulib::DecoderDrmp3::~DecoderDrmp3()
{
    if (not isOpen()) {
        return;
    }
    drmp3_uninit(&d->handle_);
}

auto Aulib::DecoderDrmp3::open(SDL_RWops* const rwops) -> bool
{
    if (isOpen()) {
        return true;
    }

    if (not drmp3_init(&d->handle_, drmp3ReadCb, drmp3SeekCb, rwops, nullptr)) {
        SDL_SetError("drmp3_init failed.");
        return false;
    }
    // Calculating the duration on an MP3 stream involves iterating over every frame in it, which is
    // only possible when the total size of the stream is known.
    if (SDL_RWsize(rwops) > 0) {
        d->duration_ = chrono::duration_cast<chrono::microseconds>(chrono::duration<double>(
            static_cast<double>(drmp3_get_pcm_frame_count(&d->handle_)) / getRate()));
    }
    setIsOpen(true);
    return true;
}

auto Aulib::DecoderDrmp3::doDecoding(float* const buf, const int len, bool& /*callAgain*/) -> int
{
    if (d->fEOF or not isOpen()) {
        return 0;
    }

    const auto ret =
        drmp3_read_pcm_frames_f32(&d->handle_, len / getChannels(), buf) * getChannels();
    if (ret < static_cast<drmp3_uint64>(len)) {
        d->fEOF = true;
    }
    return ret;
}

auto Aulib::DecoderDrmp3::getChannels() const -> int
{
    return d->handle_.channels;
}

auto Aulib::DecoderDrmp3::getRate() const -> int
{
    return d->handle_.sampleRate;
}

auto Aulib::DecoderDrmp3::rewind() -> bool
{
    return seekToTime({});
}

auto Aulib::DecoderDrmp3::duration() const -> chrono::microseconds
{
    return d->duration_;
}

auto Aulib::DecoderDrmp3::seekToTime(const chrono::microseconds pos) -> bool
{
    if (not isOpen()) {
        return false;
    }

    const auto target_frame = chrono::duration<double>(pos).count() * getRate();
    if (not drmp3_seek_to_pcm_frame(&d->handle_, target_frame)) {
        return false;
    }
    d->fEOF = false;
    return true;
}

/*

Copyright (C) 2021 Nikos Chantziaras.

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
