// This is copyrighted software. More information is at the end of this file.
#include "Aulib/DecoderVorbis.h"

#include "aulib_debug.h"
#include <SDL_rwops.h>
#include <vorbis/vorbisfile.h>

namespace chrono = std::chrono;

extern "C" {

static auto vorbisReadCb(void* ptr, size_t size, size_t nmemb, void* rwops) -> size_t
{
    return SDL_RWread(static_cast<SDL_RWops*>(rwops), ptr, size, nmemb);
}

static auto vorbisSeekCb(void* rwops, ogg_int64_t offset, int whence) -> int
{
    if (SDL_RWseek(static_cast<SDL_RWops*>(rwops), offset, whence) < 0) {
        return -1;
    }
    return 0;
}

static auto vorbisTellCb(void* rwops) -> long
{
    return SDL_RWtell(static_cast<SDL_RWops*>(rwops));
}

} // extern "C"

namespace Aulib {

struct DecoderVorbis_priv final
{
    std::unique_ptr<OggVorbis_File, decltype(&ov_clear)> fVFHandle{nullptr, ov_clear};
    int fCurrentSection = 0;
    vorbis_info* fCurrentInfo = nullptr;
    bool fEOF = false;
    chrono::microseconds fDuration{};
};

} // namespace Aulib

Aulib::DecoderVorbis::DecoderVorbis()
    : d(std::make_unique<DecoderVorbis_priv>())
{}

Aulib::DecoderVorbis::~DecoderVorbis() = default;

auto Aulib::DecoderVorbis::open(SDL_RWops* rwops) -> bool
{
    if (isOpen()) {
        return true;
    }
    ov_callbacks cbs{};
    cbs.read_func = vorbisReadCb;
    cbs.seek_func = vorbisSeekCb;
    cbs.tell_func = vorbisTellCb;
    cbs.close_func = nullptr;
    auto newHandle = decltype(d->fVFHandle){new OggVorbis_File, ov_clear};
    if (ov_open_callbacks(rwops, newHandle.get(), nullptr, 0, cbs) != 0) {
        return false;
    }
    d->fCurrentInfo = ov_info(newHandle.get(), -1);
    auto len = ov_time_total(newHandle.get(), -1);
    if (len == OV_EINVAL) {
        d->fDuration = chrono::microseconds::zero();
    } else {
        d->fDuration = chrono::duration_cast<chrono::microseconds>(chrono::duration<double>(len));
    }
    d->fVFHandle.swap(newHandle);
    setIsOpen(true);
    return true;
}

auto Aulib::DecoderVorbis::getChannels() const -> int
{
    return d->fCurrentInfo ? d->fCurrentInfo->channels : 0;
}

auto Aulib::DecoderVorbis::getRate() const -> int
{
    return d->fCurrentInfo ? d->fCurrentInfo->rate : 0;
}

auto Aulib::DecoderVorbis::doDecoding(float buf[], int len, bool& callAgain) -> int
{
    if (d->fEOF or not isOpen()) {
        return 0;
    }

    float** out;
    int decSamples = 0;

    while (decSamples < len and not callAgain) {
        int lastSection = d->fCurrentSection;
        // TODO: We only support up to 2 channels for now.
        auto channels = std::min(d->fCurrentInfo->channels, 2);
        auto ret = ov_read_float(d->fVFHandle.get(), &out, (len - decSamples) / channels,
                                 &d->fCurrentSection);
        if (ret == 0) {
            d->fEOF = true;
            break;
        }
        if (ret < 0) {
            AM_debugPrint("libvorbis stream error: ");
            switch (ret) {
            case OV_HOLE:
                AM_debugPrintLn("OV_HOLE");
                break;
            case OV_EBADLINK:
                AM_debugPrintLn("OV_EBADLINK");
                break;
            case OV_EINVAL:
                AM_debugPrintLn("OV_EINVAL");
                break;
            default:
                AM_debugPrintLn("unknown error: " << ret);
            }
            break;
        }
        if (d->fCurrentSection != lastSection) {
            d->fCurrentInfo = ov_info(d->fVFHandle.get(), -1);
            callAgain = true;
        }
        // Copy samples to output buffer in interleaved format.
        for (long samp = 0; samp < ret; ++samp) {
            for (int chan = 0; chan < channels; ++chan) {
                *buf++ = out[chan][samp];
            }
        }
        decSamples += ret * channels;
    }
    return decSamples;
}

auto Aulib::DecoderVorbis::rewind() -> bool
{
    if (not isOpen()) {
        return false;
    }

    d->fEOF = false;
    return ov_raw_seek(d->fVFHandle.get(), 0) == 0;
}

auto Aulib::DecoderVorbis::duration() const -> chrono::microseconds
{
    return d->fDuration;
}

auto Aulib::DecoderVorbis::seekToTime(chrono::microseconds pos) -> bool
{
    if (not isOpen()
        or ov_time_seek(d->fVFHandle.get(), chrono::duration<double>(pos).count()) != 0) {
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
