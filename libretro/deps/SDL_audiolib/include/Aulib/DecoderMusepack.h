// This is copyrighted software. More information is at the end of this file.
#pragma once

#include <Aulib/Decoder.h>

namespace Aulib {

/*!
 * \brief libmpcdec decoder.
 */
class AULIB_EXPORT DecoderMusepack: public Decoder
{
public:
    DecoderMusepack();
    ~DecoderMusepack() override;

    auto open(SDL_RWops* rwops) -> bool override;
    auto getChannels() const -> int override;
    auto getRate() const -> int override;
    auto rewind() -> bool override;
    auto duration() const -> std::chrono::microseconds override;
    auto seekToTime(std::chrono::microseconds pos) -> bool override;

protected:
    auto doDecoding(float buf[], int len, bool& callAgain) -> int override;

private:
    const std::unique_ptr<struct DecoderMusepack_priv> d;
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
