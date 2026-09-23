// This is copyrighted software. More information is at the end of this file.
#pragma once
#include "aulib_export.h"

namespace Aulib {

/*!
 * \brief Abstract base class for audio processors.
 *
 * A processor receives input samples, processes them and produces output samples. It can be used to
 * alter the audio produced by a decoder. Processors run after resampling (if applicable.)
 */
class AULIB_EXPORT Processor
{
public:
    Processor();
    virtual ~Processor();

    Processor(const Processor&) = delete;
    auto operator=(const Processor&) -> Processor& = delete;

    /*!
     * \brief Process input samples and write output samples.
     *
     * This function will be called from the audio thread.
     *
     * \param[out] dest Output buffer.
     * \param[in] source Input buffer.
     * \param[in] len Input and output buffer size in samples.
     */
    virtual void process(float dest[], const float source[], int len) = 0;
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
