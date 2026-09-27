// This is copyrighted software. More information is at the end of this file.
#pragma once

// FIXME: That's dumb. We should add actual print functions instead of wrapping the std streams.
#include <iostream>

#ifdef AULIB_DEBUG
#    include <cassert>
#    define AM_debugAssert assert
#    define AM_debugPrint(x) std::cerr << x
#    define AM_debugPrintLn(x) AM_debugPrint(x) << '\n'
#else
#    define AM_debugAssert(x)
#    define AM_debugPrintLn(x)
#    define AM_debugPrint(x)
#endif

#define AM_warn(x) std::cerr << x
#define AM_warnLn(x) std::cerr << x << '\n'

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
