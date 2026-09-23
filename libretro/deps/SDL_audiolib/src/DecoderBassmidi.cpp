// This is copyrighted software. More information is at the end of this file.
#include "Aulib/DecoderBassmidi.h"

#include "Buffer.h"
#include "aulib.h"
#include "aulib_debug.h"
#include "missing.h"
#include <SDL_audio.h>
#include <SDL_rwops.h>
#include <bass.h>
#include <bassmidi.h>

namespace chrono = std::chrono;

static bool bassIsInitialized = false;

class HstreamWrapper final
{
public:
    HstreamWrapper() noexcept = default;

    explicit HstreamWrapper(BOOL mem, const void* file, QWORD offset, QWORD len, DWORD flags,
                            DWORD freq)
    {
        reset(mem, file, offset, len, flags, freq);
    }

    HstreamWrapper(const HstreamWrapper&) = delete;
    HstreamWrapper(HstreamWrapper&&) = delete;
    auto operator=(const HstreamWrapper&) -> HstreamWrapper& = delete;
    auto operator=(HstreamWrapper&&) -> HstreamWrapper& = delete;

    ~HstreamWrapper()
    {
        freeStream();
    }

    explicit operator bool() const noexcept
    {
        return hstream != 0;
    }

    auto get() const noexcept -> HSTREAM
    {
        return hstream;
    }

    void reset(BOOL mem, const void* file, QWORD offset, QWORD len, DWORD flags, DWORD freq)
    {
        freeStream();
        hstream = BASS_MIDI_StreamCreateFile(mem, file, offset, len, flags, freq);
        if (hstream == 0) {
            AM_debugPrintLn("DecoderBassmidi: got BASS error " << BASS_ErrorGetCode()
                                                               << " while creating HSTREAM.");
        }
    }

    void swap(HstreamWrapper& other) noexcept
    {
        std::swap(hstream, other.hstream);
    }

private:
    HSTREAM hstream = 0;

    void freeStream() noexcept
    {
        if (hstream == 0) {
            return;
        }
        if (BASS_StreamFree(hstream) == 0) {
            AM_debugPrintLn("DecoderBassmidi: got BASS error " << BASS_ErrorGetCode()
                                                               << " while freeing HSTREAM.");
        }
    }
};

namespace Aulib {

struct DecoderBassmidi_priv final
{
    DecoderBassmidi_priv();

    HstreamWrapper hstream;
    Buffer<Uint8> midiData{0};
    bool eof = false;
};

} // namespace Aulib

Aulib::DecoderBassmidi_priv::DecoderBassmidi_priv()
{
    if (bassIsInitialized) {
        return;
    }
    if (BASS_Init(0, Aulib::sampleRate(), 0, nullptr, nullptr) != 0) {
        bassIsInitialized = true;
        return;
    }
    AM_debugPrintLn("DecoderBassmidi: got BASS error " << BASS_ErrorGetCode()
                                                       << " while initializing.");
}

Aulib::DecoderBassmidi::DecoderBassmidi()
    : d(std::make_unique<DecoderBassmidi_priv>())
{}

Aulib::DecoderBassmidi::~DecoderBassmidi() = default;

auto Aulib::DecoderBassmidi::setDefaultSoundfont(const std::string& filename) -> bool
{
    if (BASS_SetConfigPtr(BASS_CONFIG_MIDI_DEFFONT, filename.c_str()) != 0) {
        return true;
    }
    AM_debugPrintLn("DecoderBassmidi: got BASS error " << BASS_ErrorGetCode()
                                                       << " while setting default soundfont.");
    return false;
}

auto Aulib::DecoderBassmidi::open(SDL_RWops* rwops) -> bool
{
    if (isOpen()) {
        return true;
    }

    // FIXME: error reporting
    Sint64 midiDataLen = SDL_RWsize(rwops);
    if (midiDataLen <= 0) {
        return false;
    }
    Buffer<Uint8> newMidiData(midiDataLen);
    DWORD bassFlags =
        BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_MIDI_DECAYEND | BASS_MIDI_SINCINTER;

    if (SDL_RWread(rwops, newMidiData.get(), newMidiData.size(), 1) != 1) {
        return false;
    }
    d->hstream.reset(TRUE, newMidiData.get(), 0, newMidiData.size(), bassFlags, 1);
    if (not d->hstream) {
        return false;
    }
    d->midiData.swap(newMidiData);
    setIsOpen(true);
    return true;
}

auto Aulib::DecoderBassmidi::getChannels() const -> int
{
    return 2;
}

auto Aulib::DecoderBassmidi::getRate() const -> int
{
    if (not isOpen()) {
        return 0;
    }

    BASS_CHANNELINFO inf;
    if (BASS_ChannelGetInfo(d->hstream.get(), &inf) != 0) {
        return inf.freq;
    }
    AM_debugPrintLn("DecoderBassmidi: got BASS error " << BASS_ErrorGetCode()
                                                       << " while getting BASS_CHANNELINFO");
    return 0;
}

auto Aulib::DecoderBassmidi::doDecoding(float buf[], int len, bool& /*callAgain*/) -> int
{
    if (d->eof or not isOpen()) {
        return 0;
    }

    DWORD byteLen = len * static_cast<int>(sizeof(*buf));
    DWORD ret = BASS_ChannelGetData(d->hstream.get(), buf, byteLen | BASS_DATA_FLOAT);
    if (ret == static_cast<DWORD>(-1)) {
        SDL_SetError("DecoderBassmidi: got BASS error %d during decoding.\n", BASS_ErrorGetCode());
        return 0;
    }
    if (ret < byteLen) {
        d->eof = true;
    }
    return ret / static_cast<int>(sizeof(*buf));
}

auto Aulib::DecoderBassmidi::rewind() -> bool
{
    return seekToTime(chrono::microseconds::zero());
}

auto Aulib::DecoderBassmidi::duration() const -> chrono::microseconds
{
    if (not isOpen()) {
        return {};
    }

    QWORD pos = BASS_ChannelGetLength(d->hstream.get(), BASS_POS_BYTE);
    if (pos == static_cast<QWORD>(-1)) {
        AM_debugPrintLn("DecoderBassmidi: got BASS error " << BASS_ErrorGetCode()
                                                           << " while getting channel length.");
        return {};
    }
    double sec = BASS_ChannelBytes2Seconds(d->hstream.get(), pos);
    if (sec < 0) {
        AM_debugPrintLn("DecoderBassmidi: got BASS error "
                        << BASS_ErrorGetCode()
                        << " while translating duration from bytes to seconds.");
        return {};
    }
    return chrono::duration_cast<chrono::microseconds>(chrono::duration<double>(sec));
}

auto Aulib::DecoderBassmidi::seekToTime(chrono::microseconds pos) -> bool
{
    if (not isOpen()) {
        return false;
    }

    QWORD bytePos =
        BASS_ChannelSeconds2Bytes(d->hstream.get(), chrono::duration<double>(pos).count());
    if (bytePos == static_cast<QWORD>(-1)) {
        SDL_SetError("DecoderBassmidi: got BASS error %d while translating seek time to bytes.",
                     BASS_ErrorGetCode());
        return false;
    }
    if (BASS_ChannelSetPosition(d->hstream.get(), bytePos, BASS_POS_BYTE) == 0) {
        SDL_SetError("DecoderBassmidi: got BASS error %d during rewinding.\n", BASS_ErrorGetCode());
        return false;
    }
    d->eof = false;
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
