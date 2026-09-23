// This is copyrighted software. More information is at the end of this file.
#include "SDL_sound.h"

#include "log.h"
#include <Aulib/DecoderDrflac.h>
#include <Aulib/DecoderDrwav.h>
#include <Aulib/DecoderMpg123.h>
#include <Aulib/DecoderOpus.h>
#include <Aulib/DecoderVorbis.h>
#include <Aulib/ResamplerSpeex.h>
#include <SDL_rwops.h>
#include <algorithm>
#include <aulib.h>
#include <chrono>
#include <vector>

// Audio tracks that only the frontend or an emulated drive can open: a cue
// sheet inside a ZIP names its tracks as "$C:\\..." paths (drive_dbp.cpp), and
// content behind Android's Storage Access Framework comes as saf:// paths.
// SDL_RWFromFile can open neither.
class DOS_File;
bool DBP_IsDosPath(const char* path);
DOS_File* DBP_OpenDosPath(const char* path, bool write);
uint64_t DBP_DosFileRead(DOS_File* file, void* buf, uint64_t size);
bool DBP_DosFileSeek(DOS_File* file, uint64_t* pos, int whence);
void DBP_DosFileClose(DOS_File* file);
FILE* fopen_wrap(const char* path, const char* mode);
bool host_is_vfs_path(const char* path);

static int SDLCALL dos_rw_seek(SDL_RWops* rw, int offset, int whence)
{
    auto* const file = static_cast<DOS_File*>(rw->hidden.unknown.data1);
    uint64_t base = 0;
    if (whence != SEEK_SET && !DBP_DosFileSeek(file, &base, whence)) {
        return -1;
    }
    // Relative seeks become absolute ones, so negative offsets never reach
    // the DOS file's unsigned position.
    const int64_t target = static_cast<int64_t>(base) + offset;
    uint64_t pos = target < 0 ? 0 : static_cast<uint64_t>(target);
    if (!DBP_DosFileSeek(file, &pos, SEEK_SET)) {
        return -1;
    }
    return static_cast<int>(pos);
}

static int SDLCALL dos_rw_read(SDL_RWops* rw, void* ptr, int size, int maxnum)
{
    if (size <= 0 || maxnum <= 0) {
        return 0;
    }
    auto* const file = static_cast<DOS_File*>(rw->hidden.unknown.data1);
    const uint64_t got = DBP_DosFileRead(file, ptr, static_cast<uint64_t>(size) * maxnum);
    return static_cast<int>(got / size);
}

static int SDLCALL dos_rw_write(SDL_RWops*, const void*, int, int)
{
    return -1;
}

static int SDLCALL dos_rw_close(SDL_RWops* rw)
{
    DBP_DosFileClose(static_cast<DOS_File*>(rw->hidden.unknown.data1));
    SDL_FreeRW(rw);
    return 0;
}

static auto open_rwops(const char* const fname) -> SDL_RWops*
{
    if (DBP_IsDosPath(fname)) {
        DOS_File* const file = DBP_OpenDosPath(fname, false);
        if (!file) {
            return nullptr;
        }
        SDL_RWops* const rw = SDL_AllocRW();
        if (!rw) {
            DBP_DosFileClose(file);
            return nullptr;
        }
        rw->seek = dos_rw_seek;
        rw->read = dos_rw_read;
        rw->write = dos_rw_write;
        rw->close = dos_rw_close;
        rw->hidden.unknown.data1 = file;
        return rw;
    }
    if (host_is_vfs_path(fname)) {
        FILE* const fp = fopen_wrap(fname, "rb");
        return fp ? SDL_RWFromFP(fp, 1) : nullptr;
    }
    return SDL_RWFromFile(fname, "rb");
}

auto Sound_Init() -> int
{
    return Aulib::initWithoutOutput(44100, 2);
}

auto Sound_Quit() -> int
{
    Aulib::quit();
    return true;
}

auto Sound_NewSampleFromFile(
    const char* const fname, Sound_AudioInfo* const desired, const uint32_t bufferSize)
    -> Sound_Sample*
{
    using namespace Aulib;

    auto* const rwops = open_rwops(fname);
    if (!rwops) {
        retro::logWarn("Failed to create rwops for file \"{}\". {}", fname, SDL_GetError());
        return nullptr;
    }

    std::shared_ptr<Decoder> decoder =
        Decoder::decoderFor<DecoderOpus, DecoderVorbis, DecoderDrflac, DecoderDrwav, DecoderMpg123>(
            rwops);
    if (!decoder) {
        retro::logWarn("No suitable audio decoder found for file {}.", fname);
        SDL_RWclose(rwops);
        return nullptr;
    }

    Sound_Sample* sample = new Sound_Sample;
    sample->rwops = rwops;
    sample->byte_buffer.resize(bufferSize);
    sample->float_buffer.resize(bufferSize / 2);
    sample->buffer = sample->byte_buffer.data();
    sample->buffer_size = bufferSize;
    decoder->open(sample->rwops);
    sample->decoder = std::move(decoder);
    sample->resampler.setQuality(3);
    sample->resampler.setDecoder(sample->decoder);
    sample->resampler.setSpec(desired->rate, desired->channels, sample->float_buffer.size() / 2);
    sample->flags = SOUND_SAMPLEFLAG_CANSEEK;
    return sample;
}

void Sound_FreeSample(Sound_Sample* const sample)
{
    if (!sample) {
        return;
    }
    SDL_RWclose(sample->rwops);
    delete sample;
}

auto Sound_SetBufferSize(Sound_Sample* const sample, const uint32_t new_size) -> int
{
    sample->byte_buffer.resize(new_size);
    sample->buffer = sample->byte_buffer.data();
    sample->float_buffer.resize(new_size / 2);
    sample->buffer_size = new_size;
    return true;
}

static constexpr auto floatSampleToInt16(const float src) noexcept -> int16_t
{
    return static_cast<int16_t>(std::clamp(src * 32768.f, -32768.f, 32767.f));
}

auto Sound_Decode(Sound_Sample* const sample) -> uint32_t
{
    const int float_sample_count =
        sample->resampler.resample(sample->float_buffer.data(), sample->float_buffer.size());
    auto* dst = sample->byte_buffer.data();

    for (int i = 0; i < float_sample_count; ++i) {
        const auto int_sample = floatSampleToInt16(sample->float_buffer[i]);
        memcpy(dst, &int_sample, sizeof(int_sample));
        dst += sizeof(int_sample);
    }

    static_assert(sizeof(sample->buffer[0]) == 1, "");
    return dst - sample->byte_buffer.data();
}

auto Sound_Seek(Sound_Sample* const sample, const uint32_t ms) -> int
{
    sample->resampler.discardPendingSamples();
    if (ms == 0) {
        return sample->decoder->rewind();
    }
    return sample->decoder->seekToTime(std::chrono::milliseconds{ms});
}

auto Sound_Duration(Sound_Sample* const sample) -> int
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(sample->decoder->duration()).count();
}

/*

Copyright (C) 2021 Nikos Chantziaras <realnc@gmail.com>

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
