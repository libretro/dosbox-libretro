// This is copyrighted software. More information is at the end of this file.
#include "midi_bassmidi.h"
#include "deps/char8_t-remediation/char8_t-remediation.h"
#include "control.h"
#include "libretro_dosbox.h"
#include "log.h"
#include <dlfcn.h>
#include <tuple>
#ifdef _WIN32
    #include <windef.h>
#else
    #define WINAPI
#endif

/* We don't link against the BASS/BASSMIDI libraries. We dlopen() them so that the core can be
 * distributed without those libs in a GPL-compliant way.
 */
namespace {
constexpr auto BASS_CONFIG_MIDI_DEFFONT = 0x10403;
constexpr auto BASS_CONFIG_MIDI_VOICES = 0x10401;
constexpr auto BASS_MIDI_EVENTS_NORSTATUS = 0x2000000;
constexpr auto BASS_MIDI_EVENTS_RAW = 0x10000;
constexpr auto BASS_MIDI_SINCINTER = 0x800000;
constexpr auto BASS_STREAM_DECODE = 0x200000;
} // namespace

using HSTREAM = MidiHandlerBassmidi::HSTREAM;
using HSOUNDFONT = uint32_t;

extern "C" {
struct BASS_MIDI_FONT final
{
    HSOUNDFONT font;
    int preset;
    int bank;
};

static WINAPI auto (*BASS_ChannelGetData)(uint32_t handle, void* buffer, uint32_t length)
    -> uint32_t;
static WINAPI auto (*BASS_ErrorGetCode)() -> int;
static WINAPI auto (*BASS_Init)(int device, uint32_t freq, uint32_t flags, void* win, void* dsguid)
    -> int;
static WINAPI auto (*BASS_SetConfig)(uint32_t option, uint32_t value) -> int;
static WINAPI auto (*BASS_SetConfigPtr)(uint32_t option, const void* value) -> int;
static WINAPI auto (*BASS_StreamFree)(HSTREAM handle) -> int;

static WINAPI auto (*BASS_MIDI_FontSetVolume)(HSOUNDFONT handle, float volume) -> int;
static WINAPI auto (*BASS_MIDI_StreamCreate)(uint32_t channels, uint32_t flags, uint32_t freq)
    -> HSTREAM;
static WINAPI auto (*BASS_MIDI_StreamEvents)(
    HSTREAM handle, uint32_t mode, const void* events, uint32_t length) -> uint32_t;
static WINAPI auto (*BASS_MIDI_StreamGetFonts)(HSTREAM handle, void* fonts, uint32_t count)
    -> uint32_t;
}

MidiHandlerBassmidi MidiHandlerBassmidi::instance_;
bool MidiHandlerBassmidi::bass_initialized_ = false;
bool MidiHandlerBassmidi::bass_libs_loaded_ = false;
MidiHandlerBassmidi::Dlhandle_t MidiHandlerBassmidi::bass_lib_{nullptr, dlcloseWrapper};
MidiHandlerBassmidi::Dlhandle_t MidiHandlerBassmidi::bassmidi_lib_{nullptr, dlcloseWrapper};

MidiHandlerBassmidi::~MidiHandlerBassmidi()
{
    Close();
}

void MidiHandlerBassmidi::initDosboxSettings()
{
    auto init_func = [](Section* const) {
        if (instance_.is_open_) {
            instance_.Open(nullptr);
        }
    };
    auto* secprop = control->AddSection_prop("bassmidi", init_func, true);
    secprop->AddDestroyFunction([](Section* const) { instance_.Close(); });

    auto* str_prop = secprop->Add_string("bassmidi.soundfont", Property::Changeable::WhenIdle, "");
    str_prop->Set_help("Soundfont to use with BASSMIDI. One must be specified.");

    str_prop = secprop->Add_string("bassmidi.sfvolume", Property::Changeable::WhenIdle, "0.6");
    str_prop->Set_help("BASSMIDI soundfont volume. (min 0.0, max 10.0");

    auto* int_prop = secprop->Add_int("bassmidi.voices", Property::Changeable::WhenIdle, 100);
    int_prop->SetMinMax(1, 1000);
    int_prop->Set_help("Maximum number of samples that can play together. (min 1, max 1000)");
}

auto MidiHandlerBassmidi::Open(const char* const /*conf*/) -> bool
{
    if (!bass_libs_loaded_) {
        if (const auto [ok, msg] = loadLibs(); !ok) {
            retro::logError("Failed to load BASS libraries: {}", msg);
            return false;
        }
    }

    Close();

    if (!bass_initialized_ && !BASS_Init(0, 44100, 0, nullptr, nullptr)) {
        retro::logError("Failed to initialize BASS: code {}.", BASS_ErrorGetCode());
        return false;
    }
    bass_initialized_ = true;

    auto* section = static_cast<Section_prop*>(control->GetSection("bassmidi"));

    auto get_double = [section](const char* const propname) {
        try {
            return std::stod(section->Get_string(propname));
        }
        catch (const std::exception& e) {
            retro::logError("Error reading floating point '{}' conf setting: {}.", propname, e.what());
            return 0.0;
        }
    };

    if (std::string_view soundfont = section->Get_string("bassmidi.soundfont"); !soundfont.empty())
    {
        retro::logDebug("Loading BASSMIDI soundfont: {}.", soundfont);
        if (!BASS_SetConfigPtr(BASS_CONFIG_MIDI_DEFFONT, soundfont.data())) {
            retro::logError("Failed to set BASSMIDI soundfont: code {}.", BASS_ErrorGetCode());
        }
    }

    stream_ = BASS_MIDI_StreamCreate(16, BASS_STREAM_DECODE | BASS_MIDI_SINCINTER, 0);
    if (stream_ == 0) {
        retro::logError("Failed to create BASSMIDI stream: code {}.", BASS_ErrorGetCode());
        return false;
    }

    if (BASS_MIDI_FONT font{}; BASS_MIDI_StreamGetFonts(stream_, &font, 1) != -1) {
        if (!BASS_MIDI_FontSetVolume(font.font, get_double("bassmidi.sfvolume"))) {
            retro::logError(
                "Failed to set BASSMIDI soundfont volume: code {}.", BASS_ErrorGetCode());
        }
    } else {
        retro::logError(
            "Failed to get BASSMIDI soundfont for stream: code {}.", BASS_ErrorGetCode());
    }

    if (!BASS_SetConfig(BASS_CONFIG_MIDI_VOICES, section->Get_int("bassmidi.voices"))) {
        retro::logError("Failed to set BASSMIDI max voice count: code {}.", BASS_ErrorGetCode());
    }

    MixerChannel_ptr_t channel(MIXER_AddChannel(mixerCallback, 44100, "BASSMID"), MIXER_DelChannel);
    channel->Enable(true);
    channel_ = std::move(channel);
    is_open_ = true;
    return true;
}

void MidiHandlerBassmidi::Close()
{
    if (!is_open_) {
        return;
    }

    channel_->Enable(false);
    channel_ = nullptr;
    BASS_StreamFree(stream_);
    stream_ = 0;
    is_open_ = false;
}

void MidiHandlerBassmidi::PlayMsg(Bit8u* const msg)
{
    // Worst-case size if we don't recognize the message.
    int msg_len = sizeof(DB_Midi::rt_buf);

    switch (msg[0] & 0b1111'0000) {
    case 0b1000'0000: // note off
    case 0b1001'0000: // note on
    case 0b1010'0000: // key pressure
    case 0b1011'0000: // control change
    case 0b1110'0000: // pitch bend
        msg_len = 3;
        break;

    case 0b1100'0000: // program change
    case 0b1101'0000: // channel pressure
        msg_len = 2;
        break;
    }

    if (BASS_MIDI_StreamEvents(
            stream_, BASS_MIDI_EVENTS_RAW | BASS_MIDI_EVENTS_NORSTATUS, msg, msg_len)
        != 1)
    {
        char msg_hex[sizeof(DB_Midi::rt_buf) * 2 + 1] = {0};
        for (size_t i = 0; i < msg_len; ++i) {
            sprintf(msg_hex + i * 2, "%02x", msg[i]);
        }
        if (msg_len == sizeof(msg_hex)) {
            retro::logError("Unknown MIDI message {}: code {}.", msg_hex, BASS_ErrorGetCode());
        } else {
            retro::logError(
                "BASSMIDI failed to play MIDI message {}: code {}.", msg_hex, BASS_ErrorGetCode());
        }
    }
}

void MidiHandlerBassmidi::PlaySysex(Bit8u* const sysex, const Bitu len)
{
    if (BASS_MIDI_StreamEvents(stream_, BASS_MIDI_EVENTS_RAW, sysex, len) == -1u) {
        retro::logError("BASSMIDI failed to play MIDI sysex: code {}.", BASS_ErrorGetCode());
    }
}

auto MidiHandlerBassmidi::GetName() -> const char*
{
    return "bassmidi";
}

void MidiHandlerBassmidi::mixerCallback(const Bitu len)
{
    if (BASS_ChannelGetData(instance_.stream_, MixTemp, len * 4) == -1u) {
        retro::logError("BASSMIDI failed to render audio: code {}.", BASS_ErrorGetCode());
    }
    instance_.channel_->AddSamples_s16(len, reinterpret_cast<Bit16s*>(MixTemp));
}

void MidiHandlerBassmidi::dlcloseWrapper(void* handle)
{
    if (handle) {
        dlclose(handle);
    }
}

auto MidiHandlerBassmidi::loadLibs() -> std::tuple<bool, std::string>
{
#ifdef WIN32
    constexpr const char* bass_name = "bass.dll";
    constexpr const char* bassmidi_name = "bassmidi.dll";
#elif defined(__MACOSX__)
    constexpr const char* bass_name = "libbass.dylib";
    constexpr const char* bassmidi_name = "libbassmidi.dylib";
#else
    constexpr const char* bass_name = "libbass.so";
    constexpr const char* bassmidi_name = "libbassmidi.so";
#endif

    dlerror();

    Dlhandle_t basslib(
        dlopen(
            from_u8string((retro_system_directory / bass_name).u8string()).c_str(),
            RTLD_NOW | RTLD_GLOBAL),
        dlcloseWrapper);
    if (!basslib) {
        return {false, dlerror()};
    }
    Dlhandle_t midilib(
        dlopen(
            from_u8string((retro_system_directory / bassmidi_name).u8string()).c_str(),
            RTLD_NOW | RTLD_GLOBAL),
        dlcloseWrapper);
    if (!midilib) {
        return {false, dlerror()};
    }

    if (!(BASS_ChannelGetData =
              (decltype(BASS_ChannelGetData))dlsym(basslib.get(), "BASS_ChannelGetData")))
    {
        return {false, dlerror()};
    }
    if (!(BASS_ErrorGetCode =
              (decltype(BASS_ErrorGetCode))dlsym(basslib.get(), "BASS_ErrorGetCode"))) {
        return {false, dlerror()};
    }
    if (!(BASS_Init = (decltype(BASS_Init))dlsym(basslib.get(), "BASS_Init"))) {
        return {false, dlerror()};
    }
    if (!(BASS_SetConfig = (decltype(BASS_SetConfig))dlsym(basslib.get(), "BASS_SetConfig"))) {
        return {false, dlerror()};
    }
    if (!(BASS_SetConfigPtr =
              (decltype(BASS_SetConfigPtr))dlsym(basslib.get(), "BASS_SetConfigPtr"))) {
        return {false, dlerror()};
    }
    if (!(BASS_StreamFree = (decltype(BASS_StreamFree))dlsym(basslib.get(), "BASS_StreamFree"))) {
        return {false, dlerror()};
    }
    if (!(BASS_MIDI_FontSetVolume =
              (decltype(BASS_MIDI_FontSetVolume))dlsym(midilib.get(), "BASS_MIDI_FontSetVolume")))
    {
        return {false, dlerror()};
    }
    if (!(BASS_MIDI_StreamCreate =
              (decltype(BASS_MIDI_StreamCreate))dlsym(midilib.get(), "BASS_MIDI_StreamCreate")))
    {
        return {false, dlerror()};
    }
    if (!(BASS_MIDI_StreamEvents =
              (decltype(BASS_MIDI_StreamEvents))dlsym(midilib.get(), "BASS_MIDI_StreamEvents")))
    {
        return {false, dlerror()};
    }
    if (!(BASS_MIDI_StreamGetFonts =
              (decltype(BASS_MIDI_StreamGetFonts))dlsym(midilib.get(), "BASS_MIDI_StreamGetFonts")))
    {
        return {false, dlerror()};
    }

    bass_lib_ = std::move(basslib);
    bassmidi_lib_ = std::move(midilib);
    bass_libs_loaded_ = true;
    return {true, ""};
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
