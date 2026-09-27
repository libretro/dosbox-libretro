// This is copyrighted software. More information is at the end of this file.
#pragma once

class Section_prop;

#ifdef WITH_FLUIDSYNTH

#include "midi.h"
#include "mixer.h"
#include <fluidsynth.h>
#include <memory>

void init_fluid_dosbox_settings(Section_prop& secprop);

class MidiHandlerFluidsynth final: public MidiHandler
{
public:
    auto Open(const char* conf) -> bool override;
    void Close() override;
    void PlayMsg(Bit8u* const msg) override;
    void PlaySysex(Bit8u* const sysex, const Bitu len) override;
    auto GetName() -> const char* override;

private:
    using fsynth_ptr_t = std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)>;
    using fluid_settings_ptr_t =
        std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>;
    using MixerChannel_ptr_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

    MidiHandlerFluidsynth() = default;

    static MidiHandlerFluidsynth instance_;

    fluid_settings_ptr_t settings_{nullptr, &delete_fluid_settings};
    fsynth_ptr_t synth_{nullptr, &delete_fluid_synth};
    MixerChannel_ptr_t channel_{nullptr, MIXER_DelChannel};
    bool is_open_ = false;

    static void mixerCallback(Bitu len);
};

#else

inline void init_fluid_dosbox_settings(Section_prop& /*secprop*/)
{ }

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
