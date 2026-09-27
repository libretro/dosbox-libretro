// This is copyrighted software. More information is at the end of this file.
#include "midi_fluidsynth.h"

#include "control.h"
#include "libretro_dosbox.h"
#include "log.h"
#include "setup.h"
#include <string_view>

MidiHandlerFluidsynth MidiHandlerFluidsynth::instance_;

void init_fluid_dosbox_settings(Section_prop& secprop)
{
    auto* str_prop = secprop.Add_string("fluid.soundfont", Property::Changeable::WhenIdle, "");
    str_prop->Set_help("Soundfont to use with Fluidsynth. One must be specified.");

    auto* int_prop = secprop.Add_int("fluid.samplerate", Property::Changeable::WhenIdle, 44100);
    int_prop->SetMinMax(8000, 96000);
    int_prop->Set_help("Sample rate (Hz) Fluidsynth will render at. (min 8000, max 96000");

    str_prop = secprop.Add_string("fluid.gain", Property::Changeable::WhenIdle, "0.4");
    str_prop->Set_help("Fluidsynth volume gain. (min 0.0, max 10.0");

    int_prop = secprop.Add_int("fluid.polyphony", Property::Changeable::WhenIdle, 256);
    int_prop->Set_help("Fluidsynth polyphony.");

    int_prop = secprop.Add_int("fluid.cores", Property::Changeable::WhenIdle, 1);
    int_prop->SetMinMax(1, 256);
    int_prop->Set_help("Amount of CPU cores Fluidsynth will use. (min 1, max 256)");

    auto* bool_prop = secprop.Add_bool("fluid.reverb", Property::Changeable::WhenIdle, true);
    bool_prop->Set_help("Enable reverb.");

    bool_prop = secprop.Add_bool("fluid.chorus", Property::Changeable::WhenIdle, true);
    bool_prop->Set_help("Fluidsynth chorus.");

    str_prop = secprop.Add_string("fluid.reverb.roomsize", Property::Changeable::WhenIdle, "0.2");
    str_prop->Set_help("Fluidsynth reverb room size.");

    str_prop = secprop.Add_string("fluid.reverb.damping", Property::Changeable::WhenIdle, "0.0");
    str_prop->Set_help("Fluidsynth reverb damping.");

    str_prop = secprop.Add_string("fluid.reverb.width", Property::Changeable::WhenIdle, "0.5");
    str_prop->Set_help("Fluidsynth reverb width.");

    str_prop = secprop.Add_string("fluid.reverb.level", Property::Changeable::WhenIdle, "0.9");
    str_prop->Set_help("Fluidsynth reverb level.");

    int_prop = secprop.Add_int("fluid.chorus.number", Property::Changeable::WhenIdle, 3);
    int_prop->Set_help("Fluidsynth chorus voices");

    str_prop = secprop.Add_string("fluid.chorus.level", Property::Changeable::WhenIdle, "2.0");
    str_prop->Set_help("Fluidsynth chorus level.");

    str_prop = secprop.Add_string("fluid.chorus.speed", Property::Changeable::WhenIdle, "0.3");
    str_prop->Set_help("Fluidsynth chorus speed.");

    str_prop = secprop.Add_string("fluid.chorus.depth", Property::Changeable::WhenIdle, "8.0");
    str_prop->Set_help("Fluidsynth chorus depth.");
}

auto MidiHandlerFluidsynth::Open(const char* const /*conf*/) -> bool
{
    Close();

    auto* section = static_cast<Section_prop*>(control->GetSection("midi"));
    fluid_settings_ptr_t settings(new_fluid_settings(), delete_fluid_settings);

    auto get_double = [section](const char* const propname) {
        try {
            return std::stod(section->Get_string(propname));
        }
        catch (const std::exception& e) {
            retro::logError(
                "Error reading floating point '{}' conf setting: {}.", propname, e.what());
            return 0.0;
        }
    };

    fluid_settings_setnum(
        settings.get(), "synth.sample-rate", section->Get_int("fluid.samplerate"));
    fluid_settings_setnum(settings.get(), "synth.gain", get_double("fluid.gain"));
    fluid_settings_setint(settings.get(), "synth.polyphony", section->Get_int("fluid.polyphony"));
    fluid_settings_setint(settings.get(), "synth.cpu-cores", section->Get_int("fluid.cores"));

    fluid_settings_setint(settings.get(), "synth.reverb.active", section->Get_bool("fluid.reverb"));
    fluid_settings_setnum(
        settings.get(), "synth.reverb.room-size", get_double("fluid.reverb.roomsize"));
    fluid_settings_setnum(settings.get(), "synth.reverb.damp", get_double("fluid.reverb.damping"));
    fluid_settings_setnum(settings.get(), "synth.reverb.width", get_double("fluid.reverb.width"));
    fluid_settings_setnum(settings.get(), "synth.reverb.level", get_double("fluid.reverb.level"));

    fluid_settings_setint(settings.get(), "synth.chorus.active", section->Get_bool("fluid.chorus"));
    fluid_settings_setint(
        settings.get(), "synth.chorus.nr", section->Get_int("fluid.chorus.number"));
    fluid_settings_setnum(settings.get(), "", get_double("fluid.chorus.level"));
    fluid_settings_setnum(settings.get(), "synth.chorus.speed", get_double("fluid.chorus.speed"));
    fluid_settings_setnum(settings.get(), "synth.chorus.depth", get_double("fluid.chorus.depth"));

    fsynth_ptr_t synth(new_fluid_synth(settings.get()), delete_fluid_synth);
    if (!synth) {
        retro::logError("Error creating fluidsynth synthesiser.");
        return false;
    }

    if (std::string_view soundfont = section->Get_string("fluid.soundfont"); !soundfont.empty()) {
        if (fluid_synth_sfcount(synth.get()) > 0) {
            retro::logDebug("Fluidsynth soundfont already loaded. Not loading another one.");
        } else {
            retro::logDebug("Loading fluidsynth soundfont: {}.", soundfont);
            if (fluid_synth_sfload(synth.get(), soundfont.data(), true) == FLUID_FAILED) {
                retro::logError("Failed to load fluidsynth soundfont.");
            }
        }
    }

    MixerChannel_ptr_t channel(
        MIXER_AddChannel(mixerCallback, section->Get_int("fluid.samplerate"), "FSYNTH"),
        MIXER_DelChannel);
    channel->Enable(true);

    settings_ = std::move(settings);
    synth_ = std::move(synth);
    channel_ = std::move(channel);
    is_open_ = true;
    return true;
}

void MidiHandlerFluidsynth::Close()
{
    if (!is_open_) {
        return;
    }

    channel_->Enable(false);
    channel_ = nullptr;
    synth_ = nullptr;
    settings_ = nullptr;
    is_open_ = false;
}

void MidiHandlerFluidsynth::PlayMsg(Bit8u* const msg)
{
    const int chanID = msg[0] & 0b1111;

    switch (msg[0] & 0b1111'0000) {
    case 0b1000'0000:
        fluid_synth_noteoff(synth_.get(), chanID, msg[1]);
        break;

    case 0b1001'0000:
        fluid_synth_noteon(synth_.get(), chanID, msg[1], msg[2]);
        break;

    case 0b1010'0000:
        fluid_synth_key_pressure(synth_.get(), chanID, msg[1], msg[2]);
        break;

    case 0b1011'0000:
        fluid_synth_cc(synth_.get(), chanID, msg[1], msg[2]);
        break;

    case 0b1100'0000:
        fluid_synth_program_change(synth_.get(), chanID, msg[1]);
        break;

    case 0b1101'0000:
        fluid_synth_channel_pressure(synth_.get(), chanID, msg[1]);
        break;

    case 0b1110'0000:
        fluid_synth_pitch_bend(synth_.get(), chanID, msg[1] + (msg[2] << 7));
        break;

    default: {
        uint64_t tmp;
        static_assert(sizeof(tmp) == sizeof(DB_Midi::rt_buf));
        static_assert(sizeof(tmp) == sizeof(DB_Midi::cmd_buf));
        memcpy(&tmp, msg, sizeof(tmp));
        retro::logError("Unknown MIDI command: {:08x}.", tmp);
        break;
    }
    }
}

void MidiHandlerFluidsynth::PlaySysex(Bit8u* const sysex, const Bitu len)
{
    fluid_synth_sysex(
        synth_.get(), reinterpret_cast<const char*>(sysex), len, nullptr, nullptr, nullptr, false);
}

auto MidiHandlerFluidsynth::GetName() -> const char*
{
    return "fluidsynth";
}

void MidiHandlerFluidsynth::mixerCallback(const Bitu len)
{
    fluid_synth_write_s16(instance_.synth_.get(), len, MixTemp, 0, 2, MixTemp, 1, 2);
    instance_.channel_->AddSamples_s16(len, reinterpret_cast<Bit16s*>(MixTemp));
}

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
