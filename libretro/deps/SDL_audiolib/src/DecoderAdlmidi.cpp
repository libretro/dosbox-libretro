// This is copyrighted software. More information is at the end of this file.
#include "Aulib/DecoderAdlmidi.h"

#include "Buffer.h"
#include "aulib.h"
#include "missing.h"
#include <SDL_rwops.h>
#include <adlmidi.h>

namespace chrono = std::chrono;
using BankData = std::unique_ptr<void, decltype(&SDL_free)>;

static constexpr int SAMPLE_RATE = 49716;

static auto embeddedBanks() -> const std::vector<std::string>&
{
    static std::vector<std::string> banks;
    static int bank_count = -1;

    if (bank_count >= 0) {
        return banks;
    }

    bank_count = adl_getBanksCount();
    auto bank_ptr = adl_getBankNames();
    for (int i = 0; i < bank_count; ++i) {
        banks.emplace_back(bank_ptr[i]);
    }
    return banks;
}

namespace Aulib {

struct DecoderAdlmidi_priv final
{
    std::unique_ptr<ADL_MIDIPlayer, decltype(&adl_close)> adl_player{nullptr, adl_close};
    bool eof = false;
    chrono::microseconds duration{};
    DecoderAdlmidi::Emulator emulator;
    bool change_emulator = false;
    int chip_amount = 6;
    BankData bank_data{nullptr, SDL_free};
    size_t bank_data_size = 0;
    int embedded_bank = -1;

    auto setEmulator() -> bool
    {
        using Emulator = DecoderAdlmidi::Emulator;
        ADL_Emulator adl_emu;

        switch (emulator) {
        case Emulator::Nuked:
            adl_emu = ADLMIDI_EMU_NUKED;
            break;
        case Emulator::Nuked_174:
            adl_emu = ADLMIDI_EMU_NUKED_174;
            break;
        case Emulator::Dosbox:
            adl_emu = ADLMIDI_EMU_DOSBOX;
            break;
        case Emulator::Opal:
            adl_emu = ADLMIDI_EMU_OPAL;
            break;
        case Emulator::Java:
            adl_emu = ADLMIDI_EMU_JAVA;
            break;
        }

        if (adl_switchEmulator(adl_player.get(), adl_emu) < 0) {
            SDL_SetError("libADLMIDI failed to set emulator. %s", adl_errorInfo(adl_player.get()));
            return false;
        }
        return true;
    }

    auto setChipAmount() -> bool
    {
        if (adl_setNumChips(adl_player.get(), chip_amount) < 0) {
            SDL_SetError("libADLMIDI failed to change chip amount. %s",
                         adl_errorInfo(adl_player.get()));
            return false;
        }
        return true;
    }

    auto setAndFreeBank() -> bool
    {
        if (adl_openBankData(adl_player.get(), bank_data.get(), bank_data_size) < 0) {
            SDL_SetError("libADLMIDI failed to load bank data. %s",
                         adl_errorInfo(adl_player.get()));
            return false;
        }
        bank_data.reset();
        return true;
    }

    auto setEmbeddedBank() -> bool
    {
        if (adl_setBank(adl_player.get(), embedded_bank) < 0) {
            SDL_SetError("libADLMIDI failed to set embedded bank. %s",
                         adl_errorInfo(adl_player.get()));
            return false;
        }
        return true;
    }
};

} // namespace Aulib

Aulib::DecoderAdlmidi::DecoderAdlmidi()
    : d(std::make_unique<DecoderAdlmidi_priv>())
{}

Aulib::DecoderAdlmidi::~DecoderAdlmidi() = default;

auto Aulib::DecoderAdlmidi::setEmulator(Emulator emulator) -> bool
{
    d->emulator = emulator;
    d->change_emulator = true;
    if (not isOpen()) {
        return true;
    }
    if (not d->setEmulator()) {
        return false;
    }
    adl_reset(d->adl_player.get());
    return true;
}

auto Aulib::DecoderAdlmidi::setChipAmount(int chip_amount) -> bool
{
    d->chip_amount = chip_amount;
    if (not isOpen()) {
        return true;
    }
    if (not d->setChipAmount()) {
        return false;
    }
    adl_reset(d->adl_player.get());
    return true;
}

auto Aulib::DecoderAdlmidi::loadBank(SDL_RWops* rwops) -> bool
{
    if (not rwops) {
        SDL_SetError("rwops is null.");
        return false;
    }
    BankData tmp_data{SDL_LoadFile_RW(rwops, &d->bank_data_size, true), SDL_free};
    if (not tmp_data) {
        SDL_SetError("SDL failed to read bank data. %s", SDL_GetError());
        return false;
    }
    d->embedded_bank = -1;
    if (not isOpen()) {
        d->bank_data = std::move(tmp_data);
        return true;
    }
    if (not d->setAndFreeBank()) {
        return false;
    }
    adl_reset(d->adl_player.get());
    return true;
}

auto Aulib::DecoderAdlmidi::loadBank(const std::string& filename) -> bool
{
    auto* rwops = SDL_RWFromFile(filename.c_str(), "rb");
    if (not rwops) {
        SDL_SetError("SDL failed to create rwops from filename: %s", SDL_GetError());
        return false;
    }
    return loadBank(rwops);
}

auto Aulib::DecoderAdlmidi::loadEmbeddedBank(int bank_number) -> bool
{
    if (bank_number < 0 or bank_number >= adl_getBanksCount()) {
        SDL_SetError("Invalid bank number.");
        return false;
    }
    d->bank_data.reset();
    d->embedded_bank = bank_number;
    if (not isOpen()) {
        return true;
    }
    if (not d->setEmbeddedBank()) {
        return false;
    }
    return true;
}

auto Aulib::DecoderAdlmidi::getEmbeddedBanks() -> const std::vector<std::string>&
{
    return embeddedBanks();
}

auto Aulib::DecoderAdlmidi::open(SDL_RWops* rwops) -> bool
{
    if (isOpen()) {
        return true;
    }
    Sint64 midiDataLen = SDL_RWsize(rwops);
    if (midiDataLen <= 0) {
        SDL_SetError("Tried to open zero-length MIDI data source.");
        return false;
    }
    Buffer<Uint8> new_midi_data(midiDataLen);
    if (SDL_RWread(rwops, new_midi_data.get(), new_midi_data.size(), 1) != 1) {
        SDL_SetError("Failed to read MIDI data.");
        return false;
    }
    d->adl_player.reset(adl_init(SAMPLE_RATE));
    if (not d->adl_player) {
        SDL_SetError("Failed to initialize libADLMIDI: %s", adl_errorString());
        return false;
    }
    if (not d->setChipAmount()) {
        return false;
    }
    if (d->change_emulator and not d->setEmulator()) {
        return false;
    }
    if (not d->bank_data and d->embedded_bank < 0) {
        SDL_SetError("No FM patch bank loaded.");
        return false;
    }
    if ((d->bank_data and not d->setAndFreeBank())
        or (d->embedded_bank >= 0 and not d->setEmbeddedBank())) {
        return false;
    }
    if (adl_openData(d->adl_player.get(), new_midi_data.get(), new_midi_data.size()) != 0) {
        SDL_SetError("libADLMIDI failed to open MIDI data: %s", adl_errorInfo(d->adl_player.get()));
        return false;
    }
    d->duration = chrono::duration_cast<chrono::microseconds>(
        chrono::duration<double>(adl_totalTimeLength(d->adl_player.get())));
    setIsOpen(true);
    return true;
}

auto Aulib::DecoderAdlmidi::getChannels() const -> int
{
    return 2;
}

auto Aulib::DecoderAdlmidi::getRate() const -> int
{
    return SAMPLE_RATE;
}

auto Aulib::DecoderAdlmidi::rewind() -> bool
{
    if (not isOpen()) {
        return false;
    }
    adl_positionRewind(d->adl_player.get());
    d->eof = false;
    return true;
}

auto Aulib::DecoderAdlmidi::duration() const -> chrono::microseconds
{
    return d->duration;
}

auto Aulib::DecoderAdlmidi::seekToTime(chrono::microseconds pos) -> bool
{
    if (not isOpen()) {
        return false;
    }
    adl_positionSeek(d->adl_player.get(), chrono::duration<double>(pos).count());
    d->eof = false;
    return true;
}

auto Aulib::DecoderAdlmidi::doDecoding(float buf[], int len, bool& /*callAgain*/) -> int
{
    if (d->eof or not isOpen()) {
        return 0;
    }
    constexpr ADLMIDI_AudioFormat adl_format{ADLMIDI_SampleType_F32, sizeof(float),
                                             sizeof(float) * 2};
    int sample_count = adl_playFormat(d->adl_player.get(), len, (ADL_UInt8*)buf,
                                      (ADL_UInt8*)(buf + 1), &adl_format);
    if (sample_count < len) {
        d->eof = true;
        return 0;
    }
    return sample_count;
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
