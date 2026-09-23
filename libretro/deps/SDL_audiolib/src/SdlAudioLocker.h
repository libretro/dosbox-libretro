// This is copyrighted software. More information is at the end of this file.
#pragma once

#include "stream_p.h"
#include <SDL_audio.h>

/*
 * RAII wrapper for SDL_LockAudio().
 */
class SdlAudioLocker final
{
public:
    SdlAudioLocker()
    {
#if SDL_VERSION_ATLEAST(2, 0, 0)
        SDL_LockAudioDevice(Aulib::Stream_priv::fDeviceId);
#else
        SDL_LockAudio();
#endif
        fIsLocked = true;
    }

    ~SdlAudioLocker()
    {
        unlock();
    }

    SdlAudioLocker(const SdlAudioLocker&) = delete;
    auto operator=(const SdlAudioLocker&) -> SdlAudioLocker& = delete;

    void unlock()
    {
        if (fIsLocked) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
            SDL_UnlockAudioDevice(Aulib::Stream_priv::fDeviceId);
#else
            SDL_UnlockAudio();
#endif
            fIsLocked = false;
        }
    }

private:
    bool fIsLocked;
};

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
