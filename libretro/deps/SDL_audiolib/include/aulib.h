// This is copyrighted software. More information is at the end of this file.
#pragma once

#include "aulib_global.h"
#include <SDL_audio.h>
#include <SDL_version.h>
#include <string>

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#    include <SDL_stdinc.h>
#endif

struct SDL_AudioSpec;

namespace Aulib {

#if SDL_VERSION_ATLEAST(2, 0, 0)
using AudioFormat = SDL_AudioFormat;
#else
using AudioFormat = Uint16;
#endif

/*!
 * \brief Initializes the audio system.
 *
 * \param freq
 *  Sample rate that the audio device should be opened in. The sample rate that gets actually used
 *  might change though if the device does not support the requested rate. Use \ref sampleRate() to
 *  find out the actual sample rate.
 *
 * \param format
 *  Audio sample format. The most common format is AUDIO_S16SYS. The formats are defined by SDL
 *  (SDL_audio.h). The actual format we end up using might be different than the one we request if
 *  the audio device does not support it. Use \ref sampleFormat() to find out the actual sample
 *  format.
 *
 * \param channels
 *  Amount of output channels to use. Can either be 1 (mono) or 2 (stereo.) Lower or higher values
 *  will be adjusted. Unlike the other parameters, the channel count is enforced and will not
 *  change.
 *
 * \param frameSize
 *  Size in frames (samples per channel) of the internal buffer that is used to feed audio samples
 *  to SDL. Lower values provide lower latency on audio operations, at the cost of increased CPU
 *  usage and risk of audio drop-outs. A good value for 44.1kHz output for music players is 8192
 *  bytes (8kB), while a game that needs to play sound effects without much latency would use
 *  something like 2048 instead. The actual frame size we end up using might be different. This
 *  depends on the audio device and output driver used by SDL. Use \ref frameSize() to find out the
 *  actual frame size.
 *
 * \param device
 *  A UTF-8 string reported by SDL_GetAudioDeviceName() or a driver-specific name as appropriate.
 *  An empty string requests the most reasonable default device.
 *
 * \return
 *  \retval true The audio system was initialized successfully.
 *  \retval false The audio system could not be initialized.
 */
AULIB_EXPORT auto init(int freq, AudioFormat format, int channels, int frameSize,
                       const std::string& device = {}) -> bool;

/*!
 * \brief Initializes the library for decoding and resampling only.
 *
 * Use this instead of the normal init function if you only want to decode and/or resample without
 * actually playing any audio, or if you want to handle audio playback yourself.
 *
 * SDL_InitSubSystem() will not be called and thus the SDL audio subsystem is left uninitialized.
 *
 * You cannot use Aulib::Stream objects. Trying to do so will result in undefined behavior.
 *
 * \param freq
 * Target sample rate for decoders that generate audio themselves (like the MIDI and MOD decoders.)
 *
 * \param channels
 * Target channel count for decoders that generate audio themselves (like the MIDI and MOD
 * decoders.)
 *
 * \return
 *  \retval true The library was initialized successfully.
 *  \retval false The library could not be initialized.
 */
AULIB_EXPORT auto initWithoutOutput(int freq, int channels) -> bool;

/*!
 *  \brief Shuts down the SDL_audiolib library.
 *
 *  It is not required to call this function manually, as this happens automatically at program
 *  exit, but it is useful in cases where you want to shut down the audio system for some reason.
 */
AULIB_EXPORT void quit();

/*!
 * \brief Sample format the audio device is actually using.
 *
 * This can be different than the format that was requested.
 */
AULIB_EXPORT auto sampleFormat() noexcept -> AudioFormat;

/*!
 * \brief Sample rate the audio device is actually using.
 *
 * This can be different than the sample rate that was requested.
 */
AULIB_EXPORT auto sampleRate() noexcept -> int;

/*!
 * \brief Number of output channels.
 *
 * This always matches the amount of channels that was requested.
 */
AULIB_EXPORT auto channelCount() noexcept -> int;

/*!
 * \brief Number of frames (samples per channel) the audio device is actually using.
 *
 * This can be different than the frame size that was requested.
 */
AULIB_EXPORT auto frameSize() noexcept -> int;

} // namespace Aulib

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
