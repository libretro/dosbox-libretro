// This is copyrighted software. More information is at the end of this file.
#include "sampleconv.h"

#include "Buffer.h"
#include "missing.h"
#include <SDL_endian.h>
#include <SDL_version.h>
#include <limits>

/* Convert and clip a float sample to an integer sample. This works for
 * all supported integer sample types (8-bit, 16-bit, 32-bit, signed or
 * unsigned.)
 */
template <typename T>
static constexpr auto floatSampleToInt(const float src) noexcept -> T
{
    if (src >= 1.f) {
        return std::numeric_limits<T>::max();
    }
    if (src < -1.f) {
        return std::numeric_limits<T>::min();
    }
    return src * static_cast<float>(1UL << (sizeof(T) * 8 - 1))
           + (static_cast<float>(1UL << (sizeof(T) * 8 - 1))
              + static_cast<float>(std::numeric_limits<T>::min()));
}

/* Convert float samples into integer samples.
 */
template <typename T>
static void floatToInt(Uint8 dst[], const Buffer<float>& src) noexcept
{
    for (auto i : src) {
        auto sample = floatSampleToInt<T>(i);
        memcpy(dst, &sample, sizeof(sample));
        dst += sizeof(sample);
    }
}

/* Convert float samples to endian-swapped integer samples.
 */
template <typename T>
static void floatToSwappedInt(Uint8 dst[], const Buffer<float>& src) noexcept
{
    static_assert(sizeof(T) == 2 or sizeof(T) == 4, "");

    for (const auto i : src) {
        const T sample = sizeof(sample) == 2 ? SDL_Swap16(floatSampleToInt<T>(i))
                                             : SDL_Swap32(floatSampleToInt<T>(i));
        memcpy(dst, &sample, sizeof(sample));
        dst += sizeof(sample);
    }
}

template <typename T>
static void floatToLsbInt(Uint8 dst[], const Buffer<float>& src) noexcept
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    floatToInt<T>(dst, src);
#else
    floatToSwappedInt<T>(dst, src);
#endif
}

template <typename T>
static void floatToMsbInt(Uint8 dst[], const Buffer<float>& src) noexcept
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    floatToInt<T>(dst, src);
#else
    floatToSwappedInt<T>(dst, src);
#endif
}

void Aulib::floatToS8(Uint8 dst[], const Buffer<float>& src) noexcept
{
    floatToInt<Sint8>(dst, src);
}

void Aulib::floatToU8(Uint8 dst[], const Buffer<float>& src) noexcept
{
    floatToInt<Uint8>(dst, src);
}

void Aulib::floatToS16LSB(Uint8 dst[], const Buffer<float>& src) noexcept
{
    floatToLsbInt<Sint16>(dst, src);
}

void Aulib::floatToU16LSB(Uint8 dst[], const Buffer<float>& src) noexcept
{
    floatToLsbInt<Uint16>(dst, src);
}

void Aulib::floatToS16MSB(Uint8 dst[], const Buffer<float>& src) noexcept
{
    floatToMsbInt<Sint16>(dst, src);
}

void Aulib::floatToU16MSB(Uint8 dst[], const Buffer<float>& src) noexcept
{
    floatToMsbInt<Uint16>(dst, src);
}

void Aulib::floatToS32LSB(Uint8 dst[], const Buffer<float>& src) noexcept
{
    floatToLsbInt<Sint32>(dst, src);
}

void Aulib::floatToS32MSB(Uint8 dst[], const Buffer<float>& src) noexcept
{
    floatToMsbInt<Sint32>(dst, src);
}

static void floatToSwappedFloat(Uint8 dst[], const Buffer<float>& src) noexcept
{
    for (const auto i : src) {
        const auto swapped = SDL_SwapFloat(i);
        memcpy(dst, &swapped, sizeof(swapped));
        dst += sizeof(swapped);
    }
}

void Aulib::floatToFloatLSB(Uint8 dst[], const Buffer<float>& src) noexcept
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    memcpy(dst, src.get(), src.usize() * sizeof(*src.get()));
#else
    floatToSwappedFloat(dst, src);
#endif
}

void Aulib::floatToFloatMSB(Uint8 dst[], const Buffer<float>& src) noexcept
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    memcpy(dst, src.get(), src.size() * sizeof(*src.get()));
#else
    floatToSwappedFloat(dst, src);
#endif
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
