// This is copyrighted software. More information is at the end of this file.
#pragma once

#include <Aulib/Resampler.h>

namespace Aulib {

struct ResamplerSox_priv;

/*!
 * \brief Sox resampler.
 */
class AULIB_EXPORT ResamplerSox: public Resampler
{
public:
    /*!
     * \brief SoX resampler quality levels.
     *
     * These represent the five standard SoX resampler quality levels, where
     * "Quick" is the fastest but lowest quality, and "VeryHigh" is the slowest
     * but highest quality.
     */
    enum class Quality
    {
        Quick,
        Low,
        Medium,
        High,
        VeryHigh
    };

    /*!
     * \param quality
     *      Resampling quality. Note that the quality can *not* be changed
     *      later on.
     */
    explicit ResamplerSox(Quality quality = Quality::High);
    ~ResamplerSox() override;

    auto quality() const noexcept -> Quality;

protected:
    void doResampling(float dst[], const float src[], int& dstLen, int& srcLen) override;
    auto adjustForOutputSpec(int dstRate, int srcRate, int channels) -> int override;
    void doDiscardPendingSamples() override;

private:
    const std::unique_ptr<ResamplerSox_priv> d;
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
