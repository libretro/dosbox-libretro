// This is copyrighted software. More information is at the end of this file.
#pragma once

#include <Aulib/Resampler.h>

namespace Aulib {

struct ResamplerSpeex_priv;

/*!
 * \brief Speex resampler.
 */
class AULIB_EXPORT ResamplerSpeex: public Resampler
{
public:
    /*!
     * \param quality
     *      Speex resampler quality level, from 0 to 10. The value is clamped
     *      if it lies outside this range. Note that the quality can be changed
     *      later on if required.
     */
    explicit ResamplerSpeex(int quality = 5);
    ~ResamplerSpeex() override;

    auto quality() const noexcept -> int;
    void setQuality(int quality);

protected:
    void doResampling(float dst[], const float src[], int& dstLen, int& srcLen) override;
    auto adjustForOutputSpec(int dstRate, int srcRate, int channels) -> int override;
    void doDiscardPendingSamples() override;

private:
    const std::unique_ptr<ResamplerSpeex_priv> d;
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
