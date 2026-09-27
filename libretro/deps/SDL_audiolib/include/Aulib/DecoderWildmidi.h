// This is copyrighted software. More information is at the end of this file.
#pragma once

#include <Aulib/Decoder.h>
#include <string>

namespace Aulib {

/*!
 * \brief WildMIDI decoder.
 *
 * \note Before creating any instances of this class, you need to initialize the WildMIDI library
 * by calling the DecoderWildmidi::init() function once.
 */
class AULIB_EXPORT DecoderWildmidi: public Decoder
{
public:
    DecoderWildmidi();
    ~DecoderWildmidi() override;

    /*!
     * \brief Initialize the WildMIDI library.
     *
     * This needs to be called before creating any instances of this decoder.
     *
     * \param configFile
     *  Path to the wildmidi.cfg or timidity.cfg configuration file with which to initialize
     *  WildMIDI with.
     *
     * \param rate
     *  Internal WildMIDI sampling rate in Hz. Must be between 11025 and 65000.
     *
     * \param hqResampling
     *  Pass the WM_MO_ENHANCED_RESAMPLING flag to WildMIDI. By default libWildMidi uses linear
     *  interpolation for the resampling of the sound samples. Setting this option enables the
     *  library to use a resampling method that attempts to fill in the gaps giving richer sound.
     *
     * \param reverb
     *  Pass the WM_MO_REVERB flag to WildMIDI. libWildMidi has an 8 reflection reverb engine. Use
     *  this option to give more depth to the output.
     *
     * \return
     *  \retval true WildMIDI was initialized sucessfully.
     *  \retval false WildMIDI could not be initialized.
     */
    static auto init(const std::string& configFile, int rate, bool hqResampling, bool reverb)
        -> bool;

    /*!
     * \brief Shut down the WildMIDI library.
     *
     * Shuts down the wildmidi library, resetting data and freeing up memory used by the library.
     *
     * Once this is called, the library is no longer initialized and DecoderWildmidi::init() will
     * need to be called again.
     */
    static void quit();

    auto open(SDL_RWops* rwops) -> bool override;
    auto getChannels() const -> int override;
    auto getRate() const -> int override;
    auto rewind() -> bool override;
    auto duration() const -> std::chrono::microseconds override;
    auto seekToTime(std::chrono::microseconds pos) -> bool override;

protected:
    auto doDecoding(float buf[], int len, bool& callAgain) -> int override;

private:
    const std::unique_ptr<struct DecoderWildmidi_priv> d;
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
