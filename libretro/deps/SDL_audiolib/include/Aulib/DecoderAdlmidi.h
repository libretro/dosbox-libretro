// This is copyrighted software. More information is at the end of this file.
#pragma once
#include <Aulib/Decoder.h>

#include <string>
#include <vector>

namespace Aulib {

/*!
 * \brief libADLMIDI decoder.
 *
 * This decoder always generates samples at 49716Hz, the native rate of the OPL synths provided by
 * libADLMIDI.
 */
class AULIB_EXPORT DecoderAdlmidi: public Decoder
{
public:
    enum class Emulator
    {
        Nuked,
        Nuked_174,
        Dosbox,
        Opal,
        Java,
    };

    DecoderAdlmidi();
    ~DecoderAdlmidi() override;

    /*!
     * \brief Set OPL emulator.
     *
     * If you don't call this function, libADLMIDI will use whatever emulator it uses by default.
     * Which one that is, is not specified. Note that libADLMIDI might have been built without all
     * of the emulators. Trying to set an emulator that is missing will fail.
     *
     * The emulator will be set immediately if the decoder is open. Otherwise, it will be set when
     * \ref open() is called.
     *
     * \return \c true on success, \c false otherwise.
     */
    auto setEmulator(Emulator emulator) -> bool;

    /*!
     * \brief Set amount of emulated OPL chips.
     *
     * By default, 6 chips will be emulated, which should provide a high enough polyphony without
     * note dropouts with most FM banks. However, the needed chip amount might be higher with some
     * FM banks, in which case you can use a higher amount of chips.
     *
     * The chip amount will be changed immediately if the decoder is open. Otherwise, it will be
     * changed when \ref open() is called.
     *
     * \return \c true on success, \c false otherwise.
     */
    auto setChipAmount(int chip_amount) -> bool;

    /*!
     * \brief Load and set an FM patch bank.
     *
     * The data must be in a format that libADLMIDI understands. At the time of this writing, this
     * means WOPL3. If there's an already loaded bank, it will be replaced by the new one. Ownership
     * of the \p rwops is transfered to the decoder.
     *
     * The bank will be set immediately if the decoder is open. Otherwise, it will be set when
     * \ref open() is called.
     *
     * \return \c true on success, \c false otherwise.
     */
    auto loadBank(SDL_RWops* rwops) -> bool;

    //! \overload
    auto loadBank(const std::string& filename) -> bool;

    /*!
     * \brief Load and set one of the FM patch banks embedded in libADLMIDI.
     *
     * By default, libADLMIDI comes with a set of embedded FM patch banks. These can be disabled
     * when building libADLMIDI, in which case this function will always fail. Use
     * \ref getEmbeddedBanks() to get a list of all embedded banks. If there's an already loaded
     * bank, it will be replaced by the new one.
     *
     * The bank will be set immediately if the decoder is open. Otherwise, it will be set when
     * \ref open() is called.
     *
     * \return \c true on success, \c false otherwise.
     */
    auto loadEmbeddedBank(int bank_number) -> bool;

    /*!
     * \brief Get a list of all libADLMIDI embedded FM patch banks.
     *
     * If libADLMIDI was built without embedded banks, the list will be empty. To use one of these
     * banks, pass its index to \ref loadEmbeddedBank().
     *
     * \return List of embedded banks.
     */
    static auto getEmbeddedBanks() -> const std::vector<std::string>&;

    auto open(SDL_RWops* rwops) -> bool override;
    auto getChannels() const -> int override;
    auto getRate() const -> int override;
    auto rewind() -> bool override;
    auto duration() const -> std::chrono::microseconds override;
    auto seekToTime(std::chrono::microseconds pos) -> bool override;

protected:
    auto doDecoding(float buf[], int len, bool& callAgain) -> int override;

private:
    std::unique_ptr<struct DecoderAdlmidi_priv> d;
};

} // namespace Aulib

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
