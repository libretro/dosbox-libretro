// This is copyrighted software. More information is at the end of this file.
#pragma once
#include "aulib_config.h"
#include "aulib_debug.h"
#include <algorithm>

namespace Aulib {
namespace priv {

#if HAVE_STD_CLAMP
    using std::clamp;
#else
    template <typename T>
    constexpr auto clamp(const T& val, const T& lo, const T& hi) -> const T&
    {
        AM_debugAssert(not(hi < lo));
        return (val < lo) ? lo : (hi < val) ? hi : val;
    }
#endif

} // namespace priv
} // namespace Aulib

/*

Copyright (C) 2021 Nikos Chantziaras.

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
