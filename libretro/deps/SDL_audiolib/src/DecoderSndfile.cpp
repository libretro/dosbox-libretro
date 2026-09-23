// This is copyrighted software. More information is at the end of this file.
#include <Aulib/DecoderSndfile.h>

#include "aulib_debug.h"
#include "missing.h"
#include <SDL_rwops.h>
#include <SDL_version.h>
#include <sndfile.h>

namespace chrono = std::chrono;

extern "C" {

static auto sfLenCb(void* rwops) -> sf_count_t
{
    Sint64 size = SDL_RWsize(static_cast<SDL_RWops*>(rwops));
    if (size < 0) {
        return 0;
    }
    return size;
}

static auto sfSeekCb(sf_count_t offset, int whence, void* rwops) -> sf_count_t
{
    switch (whence) {
    case SEEK_SET:
        whence = RW_SEEK_SET;
        break;
    case SEEK_CUR:
        whence = RW_SEEK_CUR;
        break;
    default:
        whence = RW_SEEK_END;
    }
    int pos = SDL_RWseek(static_cast<SDL_RWops*>(rwops), offset, whence);
    return pos;
}

static auto sfReadCb(void* dst, sf_count_t count, void* rwops) -> sf_count_t
{
    int ret = SDL_RWread(static_cast<SDL_RWops*>(rwops), dst, 1, count);
    return ret;
}

static auto sfTellCb(void* rwops) -> sf_count_t
{
    return SDL_RWtell(static_cast<SDL_RWops*>(rwops));
}

} // extern "C"

namespace Aulib {

struct DecoderSndfile_priv final
{
    std::unique_ptr<SNDFILE, decltype(&sf_close)> fSndfile{nullptr, &sf_close};
    SF_INFO fInfo{};
    bool fEOF = false;
    chrono::microseconds fDuration{};
};

} // namespace Aulib

Aulib::DecoderSndfile::DecoderSndfile()
    : d(std::make_unique<DecoderSndfile_priv>())
{}

Aulib::DecoderSndfile::~DecoderSndfile() = default;

auto Aulib::DecoderSndfile::open(SDL_RWops* rwops) -> bool
{
    if (isOpen()) {
        return true;
    }
    d->fInfo.format = 0;
    SF_VIRTUAL_IO cbs;
    cbs.get_filelen = sfLenCb;
    cbs.seek = sfSeekCb;
    cbs.read = sfReadCb;
    cbs.write = nullptr;
    cbs.tell = sfTellCb;
    d->fSndfile.reset(sf_open_virtual(&cbs, SFM_READ, &d->fInfo, rwops));
    if (not d->fSndfile) {
        return false;
    }
    d->fDuration = chrono::duration_cast<chrono::microseconds>(
        chrono::duration<double>(static_cast<double>(d->fInfo.frames) / d->fInfo.samplerate));
    setIsOpen(true);
    return true;
}

auto Aulib::DecoderSndfile::getChannels() const -> int
{
    return d->fInfo.channels;
}

auto Aulib::DecoderSndfile::getRate() const -> int
{
    return d->fInfo.samplerate;
}

auto Aulib::DecoderSndfile::doDecoding(float buf[], int len, bool& /*callAgain*/) -> int
{
    if (d->fEOF or not isOpen()) {
        return 0;
    }
    sf_count_t ret = sf_read_float(d->fSndfile.get(), buf, len);
    if (ret == 0) {
        d->fEOF = true;
    }
    return ret;
}

auto Aulib::DecoderSndfile::rewind() -> bool
{
    return seekToTime(chrono::microseconds::zero());
}

auto Aulib::DecoderSndfile::duration() const -> std::chrono::microseconds
{
    return d->fDuration;
}

auto Aulib::DecoderSndfile::seekToTime(std::chrono::microseconds pos) -> bool
{
    using chrono::duration;
    if (not isOpen()
        or sf_seek(d->fSndfile.get(), duration<double>(pos).count() * getRate(), SEEK_SET) == -1) {
        return false;
    }
    d->fEOF = false;
    return true;
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
