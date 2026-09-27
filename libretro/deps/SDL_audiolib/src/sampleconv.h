// This is copyrighted software. More information is at the end of this file.
#pragma once

#include "aulib_global.h"
#include <SDL_stdinc.h>

template <typename T>
class Buffer;

namespace Aulib {

AULIB_NO_EXPORT void floatToS8(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToU8(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToS16LSB(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToU16LSB(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToS16MSB(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToU16MSB(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToS32LSB(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToS32MSB(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToFloatLSB(Uint8 dst[], const Buffer<float>& src) noexcept;
AULIB_NO_EXPORT void floatToFloatMSB(Uint8 dst[], const Buffer<float>& src) noexcept;

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
