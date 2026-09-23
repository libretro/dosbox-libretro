// This is copyrighted software. More information is at the end of this file.
#pragma once

#ifdef WITH_BASSMIDI

#include "midi.h"
#include "mixer.h"
#include <memory>
#include <tuple>

class Section_prop;

class MidiHandlerBassmidi final: public MidiHandler
{
public:
    using HSTREAM = uint32_t;

    ~MidiHandlerBassmidi() override;

    static void initDosboxSettings();

    auto Open(const char* conf) -> bool override;
    void Close() override;
    void PlayMsg(Bit8u* msg) override;
    void PlaySysex(Bit8u* sysex, const Bitu len) override;
    auto GetName() -> const char* override;

private:
    using MixerChannel_ptr_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;
    static void dlcloseWrapper(void* handle);
    using Dlhandle_t = std::unique_ptr<void, decltype(&dlcloseWrapper)>;

    MidiHandlerBassmidi() = default;

    static MidiHandlerBassmidi instance_;
    static bool bass_initialized_;
    static bool bass_libs_loaded_;
    static Dlhandle_t bass_lib_;
    static Dlhandle_t bassmidi_lib_;

    HSTREAM stream_ = 0;
    MixerChannel_ptr_t channel_{nullptr, MIXER_DelChannel};
    bool is_open_ = false;

    static void mixerCallback(Bitu len);
    static auto loadLibs() -> std::tuple<bool, std::string>;
};

#else

namespace MidiHandlerBassmidi {
inline void initDosboxSettings()
{ }
} // namespace MidiHandlerBassmidi

#endif

/*

Copyright (C) 2002-2011 The DOSBox Team
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
