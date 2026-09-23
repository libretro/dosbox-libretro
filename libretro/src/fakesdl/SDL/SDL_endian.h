// This is copyrighted software. More information is at the end of this file.
#pragma once
#include <cstdint>

inline auto SDL_SwapLE32(const uint32_t val) noexcept -> uint32_t
{
    const auto* data = reinterpret_cast<const unsigned char*>(&val);
    return (uint32_t(data[0]) << 0) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16)
        | (uint32_t(data[3]) << 24);
}

/*

Copyright (C) 2020 Nikos Chantziaras <realnc@gmail.com>

This file is part of DOSBox-core.

DOSBox-core is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or (at your option) any later
version.

DOSBox-core is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
DOSBox-core. If not, see <https://www.gnu.org/licenses/>.

*/
