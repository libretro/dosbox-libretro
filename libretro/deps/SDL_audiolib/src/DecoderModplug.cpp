// This is copyrighted software. More information is at the end of this file.
#include "Aulib/DecoderModplug.h"

#include "Buffer.h"
#include "aulib.h"
#include "missing.h"
#include <SDL_audio.h>
#include <libmodplug/modplug.h>
#include <limits>

namespace chrono = std::chrono;

static ModPlug_Settings modplugSettings;
static bool initialized = false;

static void initModPlug()
{
    ModPlug_GetSettings(&modplugSettings);
    modplugSettings.mFlags = MODPLUG_ENABLE_OVERSAMPLING | MODPLUG_ENABLE_NOISE_REDUCTION;
    // TODO: can modplug handle more than 2 channels?
    modplugSettings.mChannels = Aulib::channelCount() == 1 ? 1 : 2;
    // It seems MogPlug does resample to any samplerate. 32, 44.1, up to
    // 192K all seem to work correctly.
    modplugSettings.mFrequency = Aulib::sampleRate();
    modplugSettings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
    modplugSettings.mBits = 32;
    ModPlug_SetSettings(&modplugSettings);
    initialized = true;
}

namespace Aulib {

struct DecoderModplug_priv final
{
    DecoderModplug_priv();

    std::unique_ptr<ModPlugFile, decltype(&ModPlug_Unload)> mpHandle{nullptr, &ModPlug_Unload};
    bool atEOF = false;
    chrono::microseconds fDuration{};
};

} // namespace Aulib

Aulib::DecoderModplug_priv::DecoderModplug_priv()
{
    if (not initialized) {
        initModPlug();
    }
}

Aulib::DecoderModplug::DecoderModplug()
    : d(std::make_unique<DecoderModplug_priv>())
{}

Aulib::DecoderModplug::~DecoderModplug() = default;

auto Aulib::DecoderModplug::open(SDL_RWops* rwops) -> bool
{
    if (isOpen()) {
        return true;
    }
    // FIXME: error reporting
    Sint64 dataSize = SDL_RWsize(rwops);
    if (dataSize <= 0 or dataSize > std::numeric_limits<int>::max()) {
        return false;
    }
    Buffer<Uint8> data(dataSize);
    if (SDL_RWread(rwops, data.get(), data.size(), 1) != 1) {
        return false;
    }
    d->mpHandle.reset(ModPlug_Load(data.get(), data.size()));
    if (not d->mpHandle) {
        return false;
    }
    ModPlug_SetMasterVolume(d->mpHandle.get(), 192);
    d->fDuration = chrono::milliseconds(ModPlug_GetLength(d->mpHandle.get()));
    setIsOpen(true);
    return true;
}

auto Aulib::DecoderModplug::getChannels() const -> int
{
    return modplugSettings.mChannels;
}

auto Aulib::DecoderModplug::getRate() const -> int
{
    return modplugSettings.mFrequency;
}

auto Aulib::DecoderModplug::doDecoding(float buf[], int len, bool& /*callAgain*/) -> int
{
    if (d->atEOF or not isOpen()) {
        return 0;
    }
    Buffer<Sint32> tmpBuf(len);
    int ret = ModPlug_Read(d->mpHandle.get(), tmpBuf.get(), len * 4);
    // Convert from 32-bit to float.
    for (int i = 0; i < len; ++i) {
        buf[i] = tmpBuf[i] / 2147483648.f;
    }
    if (ret == 0) {
        d->atEOF = true;
    }
    return ret / static_cast<int>(sizeof(*buf));
}

auto Aulib::DecoderModplug::rewind() -> bool
{
    return seekToTime(chrono::microseconds::zero());
}

auto Aulib::DecoderModplug::duration() const -> chrono::microseconds
{
    return d->fDuration;
}

auto Aulib::DecoderModplug::seekToTime(chrono::microseconds pos) -> bool
{
    if (not isOpen()) {
        return false;
    }
    ModPlug_Seek(d->mpHandle.get(), chrono::duration_cast<chrono::milliseconds>(pos).count());
    d->atEOF = false;
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
