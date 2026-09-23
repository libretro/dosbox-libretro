// This is copyrighted software. More information is at the end of this file.
#pragma once

#include <Aulib/Resampler.h>

namespace Aulib {

/*!
 * \brief SRC (libsamplerate) resampler.
 */
class AULIB_EXPORT ResamplerSrc: public Resampler
{
public:
    /*!
     * \brief SRC resampler quality.
     *
     * These represent the five SRC resampling methods, with "Linear" and
     * "ZeroOrderHold" being the fastest but lowest quality, and "SincBest" the
     * slowest but highest quality.
     *
     * From [SRC's API documentation](http://www.mega-nerd.com/SRC/api_misc.html):
     *
     * - SincBest
     *   This is a bandlimited interpolator derived from the mathematical sinc
     *   function and this is the highest quality sinc based converter,
     *   providing a worst case Signal-to-Noise Ratio (SNR) of 97 decibels (dB)
     *   at a bandwidth of 97%. All three Sinc* converters are based on the
     *   techniques of Julius O. Smith although this code was developed
     *   independantly.
     *
     * - SincMedium
     *   This is another bandlimited interpolator much like the previous one.
     *   It has an SNR of 97dB and a bandwidth of 90%. The speed of the
     *   conversion is much faster than the previous one.
     *
     * - SincFastest
     *   This is the fastest bandlimited interpolator and has an SNR of 97dB
     *   and a bandwidth of 80%.
     *
     * - ZeroOrderHold
     *   A Zero Order Hold converter (interpolated value is equal to the last
     *   value). The quality is poor but the conversion speed is blindlingly
     *   fast.
     *
     * - Linear
     *   A linear converter. Again the quality is poor, but the conversion
     *   speed is blindingly fast.
     */
    enum class Quality
    {
        Linear,
        ZeroOrderHold,
        SincFastest,
        SincMedium,
        SincBest
    };

    /*!
     * \param quality
     *      Resampling quality. Note that the quality can *not* be changed
     *      later on.
     */
    explicit ResamplerSrc(Quality quality = Quality::SincMedium);
    ~ResamplerSrc() override;

    auto quality() const noexcept -> Quality;

protected:
    void doResampling(float dst[], const float src[], int& dstLen, int& srcLen) override;
    auto adjustForOutputSpec(int dstRate, int srcRate, int channels) -> int override;
    void doDiscardPendingSamples() override;

private:
    const std::unique_ptr<struct ResamplerSrc_priv> d;
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
