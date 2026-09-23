// This is copyrighted software. More information is at the end of this file.
#include "Aulib/DecoderFluidsynth.h"

#include "Buffer.h"
#include "aulib.h"
#include "aulib_debug.h"
#include "missing.h"
#include <SDL_audio.h>
#include <SDL_rwops.h>
#include <array>
#include <cstdio>
#include <fluidsynth.h>

#if FLUIDSYNTH_VERSION_MAJOR == 2 && FLUIDSYNTH_VERSION_MINOR >= 2
using read_cb_count_type = fluid_long_long_t;
using seek_cb_offset_type = fluid_long_long_t;
#else
using read_cb_count_type = int;
using seek_cb_offset_type = long;
#endif

namespace chrono = std::chrono;

static fluid_settings_t* settings = nullptr;

/* Huge kludge. Fluidsynth doesn't have a nice API for custom soundfont loading, so we hijack the
 * filename string to store a pointer to the rwops. We prefix such filenames with a "&".
 */
static auto sfontOpenCb(const char* filename) -> void*
{
    if (not filename) {
        return nullptr;
    }
    if (filename[0] != '&') {
        return SDL_RWFromFile(filename, "rb");
    }
    void* rwops;
    if (sscanf(filename, "&%p", &rwops) != 1) {
        AM_warnLn(
            "failed to extract rwops pointer from string - rwops might have leaked (please "
            "file a bug)");
        return nullptr;
    }
    return rwops;
}

static auto sfontReadCb(void* dst, read_cb_count_type count, void* rwops) -> int
{
    Buffer<char> buf(count);
    if (SDL_RWread(static_cast<SDL_RWops*>(rwops), buf.get(), 1, count) <= 0) {
        return FLUID_FAILED;
    }
    memcpy(dst, buf.get(), count);
    return FLUID_OK;
}

static auto sfontSeekCb(void* rwops, seek_cb_offset_type offset, int whence) -> int
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
    if (SDL_RWseek(static_cast<SDL_RWops*>(rwops), offset, whence) == -1) {
        return FLUID_FAILED;
    }
    return FLUID_OK;
}

static auto sfontCloseCb(void* rwops) -> int
{
    if (SDL_RWclose(static_cast<SDL_RWops*>(rwops)) != 0) {
        return FLUID_FAILED;
    }
    return FLUID_OK;
}

static auto sfontTellCb(void* rwops) -> seek_cb_offset_type
{
    auto pos = SDL_RWtell(static_cast<SDL_RWops*>(rwops));
    if (pos == -1) {
        return FLUID_FAILED;
    }
    return pos;
}

static auto initFluidSynth() -> int
{
    if (settings) {
        return 0;
    }
    if (not(settings = new_fluid_settings())) {
        return -1;
    }
    fluid_settings_setnum(settings, "synth.sample-rate", Aulib::sampleRate());
    for (const auto i : {FLUID_PANIC, FLUID_ERR, FLUID_WARN, FLUID_INFO, FLUID_DBG}) {
        fluid_set_log_function(i, nullptr, nullptr);
    }
    return 0;
}

namespace Aulib {

struct DecoderFluidsynth_priv final
{
    DecoderFluidsynth_priv();

    std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)> fSynth{nullptr,
                                                                         &delete_fluid_synth};
    std::unique_ptr<fluid_player_t, decltype(&delete_fluid_player)> fPlayer{nullptr,
                                                                            &delete_fluid_player};
    fluid_sfloader_t* sfloader = nullptr;
    Buffer<Uint8> fMidiData{0};
    bool fEOF = false;
};

} // namespace Aulib

Aulib::DecoderFluidsynth_priv::DecoderFluidsynth_priv()
{
    if (not settings) {
        initFluidSynth();
    }
    fSynth.reset(new_fluid_synth(settings));
    if (not fSynth) {
        return;
    }
    fluid_synth_set_interp_method(fSynth.get(), -1, FLUID_INTERP_7THORDER);
    fluid_synth_set_reverb(fSynth.get(),
                           0.6, // Room size
                           0.5, // Damping
                           0.5, // Width
                           0.3); // Level
    sfloader = new_fluid_defsfloader(settings);
    fluid_sfloader_set_callbacks(sfloader, sfontOpenCb, sfontReadCb, sfontSeekCb, sfontTellCb,
                                 sfontCloseCb);
    fluid_synth_add_sfloader(fSynth.get(), sfloader);
}

Aulib::DecoderFluidsynth::DecoderFluidsynth()
    : d(std::make_unique<DecoderFluidsynth_priv>())
{}

Aulib::DecoderFluidsynth::~DecoderFluidsynth() = default;

auto Aulib::DecoderFluidsynth::loadSoundfont(SDL_RWops* rwops) -> bool
{
    if (not d->fSynth) {
        return false;
    }
    if (not rwops) {
        SDL_SetError("rwops is null.");
        return false;
    }

    auto closeRwops = [rwops] {
        if (SDL_RWclose(rwops) != 0) {
            AM_warnLn("failed to close rwops: " << SDL_GetError());
        }
    };

    std::array<char, 64> bogus_fname;
    auto ret = snprintf(bogus_fname.data(), bogus_fname.size(), "&%p", static_cast<void*>(rwops));
    if (ret < 0 or ret >= static_cast<int>(bogus_fname.size())) {
        SDL_SetError("internal string representation of pointer is too long (please file a bug)");
        closeRwops();
        return false;
    }
    if (fluid_synth_sfload(d->fSynth.get(), bogus_fname.data(), 1) == FLUID_FAILED) {
        SDL_SetError("failed to load soundfont from rwops");
        closeRwops();
        return false;
    }
    return true;
}

auto Aulib::DecoderFluidsynth::loadSoundfont(const std::string& filename) -> bool
{
    if (not d->fSynth) {
        return false;
    }
    if (fluid_synth_sfload(d->fSynth.get(), filename.c_str(), 1) == FLUID_FAILED) {
        SDL_SetError("FluidSynth failed to load soundfont.");
        return false;
    }
    return true;
}

auto Aulib::DecoderFluidsynth::gain() const -> float
{
    if (not d->fSynth) {
        return 0.f;
    }
    return fluid_synth_get_gain(d->fSynth.get());
}

void Aulib::DecoderFluidsynth::setGain(float gain)
{
    if (not d->fSynth) {
        return;
    }
    fluid_synth_set_gain(d->fSynth.get(), gain);
}

auto Aulib::DecoderFluidsynth::open(SDL_RWops* rwops) -> bool
{
    if (isOpen()) {
        return true;
    }
    if (not d->fSynth) {
        SDL_SetError("FluidSynth failed to initialize.");
        return false;
    }
    if (not rwops) {
        SDL_SetError("rwops is null.");
        return false;
    }

    Sint64 midiDataLen = SDL_RWsize(rwops);
    if (midiDataLen <= 0) {
        SDL_SetError("Invalid MIDI data.");
        return false;
    }
    Buffer<Uint8> newMidiData(midiDataLen);
    if (SDL_RWread(rwops, newMidiData.get(), newMidiData.size(), 1) != 1) {
        SDL_SetError("Unable to read MIDI data. %s", SDL_GetError());
        return false;
    }
    d->fPlayer.reset(new_fluid_player(d->fSynth.get()));
    if (not d->fPlayer) {
        SDL_SetError("Failed to create FluidSynth player.");
        return false;
    }
    if (fluid_player_add_mem(d->fPlayer.get(), newMidiData.get(), newMidiData.usize())
        != FLUID_OK) {
        SDL_SetError("FluidSynth failed to load MIDI data.");
        return false;
    }
    if (fluid_player_play(d->fPlayer.get()) != FLUID_OK) {
        SDL_SetError("FluidSynth failed to start MIDI player.");
        return false;
    }
    d->fMidiData.swap(newMidiData);
    setIsOpen(true);
    return true;
}

auto Aulib::DecoderFluidsynth::getChannels() const -> int
{
    if (not settings) {
        return 0;
    }

    int channels;
    fluid_settings_getint(settings, "synth.audio-channels", &channels);
    // This is the amount of stereo channel *pairs*, so each pair has *two* audio channels.
    return channels * 2;
}

auto Aulib::DecoderFluidsynth::getRate() const -> int
{
    if (not settings) {
        return 0;
    }

    double rate;
    fluid_settings_getnum(settings, "synth.sample-rate", &rate);
    return rate;
}

auto Aulib::DecoderFluidsynth::doDecoding(float buf[], int len, bool& /*callAgain*/) -> int
{
    if (d->fEOF or not isOpen()) {
        return 0;
    }

    len /= Aulib::channelCount();
    int res = fluid_synth_write_float(d->fSynth.get(), len, buf, 0, 2, buf, 1, 2);
    if (fluid_player_get_status(d->fPlayer.get()) == FLUID_PLAYER_DONE) {
        d->fEOF = true;
    }
    if (res == FLUID_OK) {
        return len * Aulib::channelCount();
    }
    return 0;
}

auto Aulib::DecoderFluidsynth::rewind() -> bool
{
    if (not isOpen()) {
        return false;
    }

    fluid_player_stop(d->fPlayer.get());
    d->fPlayer.reset(new_fluid_player(d->fSynth.get()));
    if (not d->fPlayer) {
        SDL_SetError("FluidSynth failed to create new player.");
        return false;
    }
    fluid_player_add_mem(d->fPlayer.get(), d->fMidiData.get(), d->fMidiData.usize());
    fluid_player_play(d->fPlayer.get());
    d->fEOF = false;
    return true;
}

auto Aulib::DecoderFluidsynth::duration() const -> chrono::microseconds
{
    SDL_SetError("Duration cannot be determined with this decoder.");
    return {};
}

auto Aulib::DecoderFluidsynth::seekToTime(chrono::microseconds /*pos*/) -> bool
{
    SDL_SetError("Seeking is not supported with this decoder.");
    return false;
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
