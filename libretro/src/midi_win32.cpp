// This is copyrighted software. More information is at the end of this file.
#include "midi_win32.h"

#include "log.h"
#include <windows.h>

auto getWin32MidiPorts() -> std::vector<std::string>
{
    const UINT dev_count = midiOutGetNumDevs();
    std::vector<std::string> port_list;

    for (UINT i = 0; i < dev_count; ++i) {
        MIDIOUTCAPSA caps;
        MMRESULT mmr = midiOutGetDevCapsA(i, &caps, sizeof(caps));
        if (mmr != MMSYSERR_NOERROR) {
            retro::logError("Failed to get MIDI device capabilities. Error code: {}.", mmr);
            return {};
        }
        port_list.emplace_back(caps.szPname);
    }
    return port_list;
}

/*

Copyright (C) 2019 Nikos Chantziaras.

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
