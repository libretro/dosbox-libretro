// This is copyrighted software. More information is at the end of this file.
#include "Aulib/Decoder.h"

#include "Aulib/DecoderAdlmidi.h"
#include "Aulib/DecoderBassmidi.h"
#include "Aulib/DecoderDrflac.h"
#include "Aulib/DecoderDrmp3.h"
#include "Aulib/DecoderDrwav.h"
#include "Aulib/DecoderFluidsynth.h"
#include "Aulib/DecoderModplug.h"
#include "Aulib/DecoderMpg123.h"
#include "Aulib/DecoderMusepack.h"
#include "Aulib/DecoderOpenmpt.h"
#include "Aulib/DecoderOpus.h"
#include "Aulib/DecoderSndfile.h"
#include "Aulib/DecoderVorbis.h"
#include "Aulib/DecoderWildmidi.h"
#include "Aulib/DecoderXmp.h"
#include "Buffer.h"
#include "aulib.h"
#include "aulib_config.h"
#include <SDL_audio.h>
#include <SDL_rwops.h>
#include <array>

namespace Aulib {

struct Decoder_priv final
{
    Buffer<float> stereoBuf{0};
    bool isOpen = false;
};

} // namespace Aulib

Aulib::Decoder::Decoder()
    : d(std::make_unique<Aulib::Decoder_priv>())
{}

Aulib::Decoder::~Decoder() = default;

auto Aulib::Decoder::decoderFor(const std::string& filename) -> std::unique_ptr<Aulib::Decoder>
{
    auto rwopsClose = [](SDL_RWops* rwops) { SDL_RWclose(rwops); };
    std::unique_ptr<SDL_RWops, decltype(rwopsClose)> rwops(SDL_RWFromFile(filename.c_str(), "rb"),
                                                           rwopsClose);
    return Decoder::decoderFor(rwops.get());
}

auto Aulib::Decoder::decoderFor(SDL_RWops* rwops) -> std::unique_ptr<Aulib::Decoder>
{
    const auto rwPos = SDL_RWtell(rwops);

    auto rewindRwops = [rwops, rwPos] { SDL_RWseek(rwops, rwPos, RW_SEEK_SET); };

    [[maybe_unused]] auto tryDecoder = [rwops, &rewindRwops](auto dec) {
        rewindRwops();
        bool ret = dec->open(rwops);
        rewindRwops();
        return ret;
    };

#if USE_DEC_DRFLAC
    if (tryDecoder(std::make_unique<DecoderDrflac>())) {
        return std::make_unique<Aulib::DecoderDrflac>();
    }
#endif
#if USE_DEC_LIBVORBIS
    if (tryDecoder(std::make_unique<DecoderVorbis>())) {
        return std::make_unique<Aulib::DecoderVorbis>();
    }
#endif
#if USE_DEC_LIBOPUSFILE
    if (tryDecoder(std::make_unique<DecoderOpus>())) {
        return std::make_unique<Aulib::DecoderOpus>();
    }
#endif
#if USE_DEC_MUSEPACK
    if (tryDecoder(std::make_unique<DecoderMusepack>())) {
        return std::make_unique<Aulib::DecoderMusepack>();
    }
#endif
#if USE_DEC_FLUIDSYNTH or USE_DEC_BASSMIDI or USE_DEC_WILDMIDI or USE_DEC_ADLMIDI
    {
        std::array<char, 5> head{};
        if (SDL_RWread(rwops, head.data(), 1, 4) == 4 and head == decltype(head){"MThd"}) {
            using midi_dec_type =
#    if USE_DEC_FLUIDSYNTH
                DecoderFluidsynth;
#    elif USE_DEC_BASSMIDI
                DecoderBassmidi;
#    elif USE_DEC_WILDMIDI
                DecoderWildmidi;
#    elif USE_DEC_ADLMIDI
                DecoderAdlmidi;
#    endif
            if (tryDecoder(std::make_unique<midi_dec_type>())) {
                return std::make_unique<midi_dec_type>();
            }
        }
    }
#endif
#if USE_DEC_SNDFILE
    if (tryDecoder(std::make_unique<DecoderSndfile>())) {
        return std::make_unique<Aulib::DecoderSndfile>();
    }
#endif
#if USE_DEC_DRWAV
    if (tryDecoder(std::make_unique<DecoderDrwav>())) {
        return std::make_unique<Aulib::DecoderDrwav>();
    }
#endif
#if USE_DEC_OPENMPT
    if (tryDecoder(std::make_unique<DecoderOpenmpt>())) {
        return std::make_unique<Aulib::DecoderOpenmpt>();
    }
#endif

#if USE_DEC_XMP
    if (tryDecoder(std::make_unique<DecoderXmp>())) {
        return std::make_unique<Aulib::DecoderXmp>();
    }
#endif
#if USE_DEC_MODPLUG
    // We don't try ModPlug, since it thinks just about anything is a module
    // file, which would result in virtually everything we feed it giving a
    // false positive.
#endif

// The MP3 decoders have too many false positives. So try them last.
#if USE_DEC_MPG123
    if (tryDecoder(std::make_unique<DecoderMpg123>())) {
        return std::make_unique<Aulib::DecoderMpg123>();
    }
#endif
#if USE_DEC_DRMP3
    if (tryDecoder(std::make_unique<DecoderDrmp3>())) {
        return std::make_unique<Aulib::DecoderDrmp3>();
    }
#endif
    return nullptr;
}

auto Aulib::Decoder::isOpen() const -> bool
{
    return d->isOpen;
}

// Conversion happens in-place.
static constexpr void monoToStereo(float buf[], int len)
{
    if (len < 1 or not buf) {
        return;
    }
    for (int i = len / 2 - 1, j = len - 1; i >= 0; --i) {
        buf[j--] = buf[i];
        buf[j--] = buf[i];
    }
}

static constexpr void stereoToMono(float dst[], const float src[], int srcLen)
{
    if (srcLen < 1 or not dst or not src) {
        return;
    }
    for (int i = 0, j = 0; i < srcLen; i += 2, ++j) {
        dst[j] = src[i] * 0.5f;
        dst[j] += src[i + 1] * 0.5f;
    }
}

auto Aulib::Decoder::decode(float buf[], int len, bool& callAgain) -> int
{
    if (this->getChannels() == 1 and Aulib::channelCount() == 2) {
        int srcLen = this->doDecoding(buf, len / 2, callAgain);
        monoToStereo(buf, srcLen * 2);
        return srcLen * 2;
    }

    if (this->getChannels() == 2 and Aulib::channelCount() == 1) {
        if (d->stereoBuf.size() != len * 2) {
            d->stereoBuf.reset(len * 2);
        }
        int srcLen = this->doDecoding(d->stereoBuf.get(), d->stereoBuf.size(), callAgain);
        stereoToMono(buf, d->stereoBuf.get(), srcLen);
        return srcLen / 2;
    }
    return this->doDecoding(buf, len, callAgain);
}

void Aulib::Decoder::setIsOpen(bool f)
{
    d->isOpen = f;
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
