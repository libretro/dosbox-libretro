// This is copyrighted software. More information is at the end of this file.
#pragma once

#include "aulib_export.h"
#include <SDL_stdinc.h>
#include <SDL_version.h>
#include <aulib.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

struct SDL_RWops;
struct SDL_AudioSpec;

namespace Aulib {

class Decoder;
class Resampler;
class Processor;

/*!
 * \brief A \ref Stream handles playback for audio produced by a Decoder.
 *
 * All public functions of this class will lock the SDL audio device when they are called, and
 * unlock it when they return. Therefore, it is safe to manipulate a Stream that is currently
 * playing without having to manually lock the SDL audio device.
 *
 * This class is re-entrant but not thread-safe. You can call functions of this class from different
 * threads only if those calls operate on different objects. If you need to control the same Stream
 * object from multiple threads, you need to synchronize access to that object. This includes Stream
 * destruction, meaning you should not create a Stream in one thread and destroy it in another
 * without synchronization.
 */
class AULIB_EXPORT Stream
{
public:
    using Callback = std::function<void(Stream&)>;

    /*!
     * \brief Constructs an audio stream from the given file name, decoder and resampler.
     *
     * \param filename
     *  File name from which to feed data to the decoder. Must not be null.
     *
     * \param decoder
     *  Decoder to use for decoding the contents of the file. Must not be null.
     *
     * \param resampler
     *  Resampler to use for converting the sample rate of the audio we get from the decoder. If
     *  this is null, then no resampling will be performed.
     */
    explicit Stream(const std::string& filename, std::unique_ptr<Decoder> decoder,
                    std::unique_ptr<Resampler> resampler);

    //! \overload
    explicit Stream(const std::string& filename, std::unique_ptr<Decoder> decoder);

    /*!
     * \brief Constructs an audio stream from the given SDL_RWops, decoder and resampler.
     *
     * \param rwops
     *  SDL_RWops from which to feed data to the decoder. Must not be null.
     *
     * \param decoder
     *  Decoder to use for decoding the contents of the SDL_RWops. Must not be null.
     *
     * \param resampler
     *  Resampler to use for converting the sample rate of the audio we get from the decoder. If
     *  this is null, then no resampling will be performed.
     *
     * \param closeRw
     *  Specifies whether 'rwops' should be automatically closed when the stream is destroyed.
     */
    explicit Stream(SDL_RWops* rwops, std::unique_ptr<Decoder> decoder,
                    std::unique_ptr<Resampler> resampler, bool closeRw);

    //! \overload
    explicit Stream(SDL_RWops* rwops, std::unique_ptr<Decoder> decoder, bool closeRw);

    virtual ~Stream();

    Stream(const Stream&) = delete;
    auto operator=(const Stream&) -> Stream& = delete;

    /*!
     * \brief Open the stream and prepare it for playback.
     *
     * Although calling this function is not required, you can use it in order to determine whether
     * the stream can be loaded successfully prior to starting playback.
     *
     * \return
     *  \retval true Stream was opened successfully.
     *  \retval false The stream could not be opened.
     */
    virtual auto open() -> bool;

    /*!
     * \brief Start playback.
     *
     * \param iterations
     *  The amount of times the stream should be played. If zero, the stream will loop forever.
     *
     * \param fadeTime
     *  Fade-in over the specified amount of time.
     *
     * \return
     *  \retval true Playback was started successfully, or it was already started.
     *  \retval false Playback could not be started.
     */
    virtual auto play(int iterations = 1, std::chrono::microseconds fadeTime = {}) -> bool;

    /*!
     * \brief Stop playback.
     *
     * When calling this, the stream is reset to the beginning again.
     *
     * \param fadeTime
     *  Fade-out over the specified amount of time.
     */
    virtual void stop(std::chrono::microseconds fadeTime = {});

    /*!
     * \brief Pause playback.
     *
     * \param fadeTime
     *  Fade-out over the specified amount of time.
     */
    virtual void pause(std::chrono::microseconds fadeTime = {});

    /*!
     * \brief Resume playback.
     *
     * \param fadeTime
     *  Fade-in over the specified amount of time.
     */
    virtual void resume(std::chrono::microseconds fadeTime = {});

    /*!
     * \brief Rewind stream to the beginning.
     *
     * \return
     *  \retval true Stream was rewound successfully.
     *  \retval false Stream could not be rewound.
     */
    virtual auto rewind() -> bool;

    /*!
     * \brief Change playback volume.
     *
     * \param volume
     *  0.0 means total silence, while 1.0 means non-attenuated, 100% (0db) volume. Values above 1.0
     *  are possible and will result in gain being applied (which might result in distortion.) For
     *  example, 3.5 would result in 350% percent volume. There is no upper limit.
     */
    virtual void setVolume(float volume);

    /*!
     * \brief Get current playback volume.
     *
     * \return
     *  Current playback volume.
     */
    virtual auto volume() const -> float;

    /*!
     * \brief Set stereo position.
     *
     * This only attenuates the left or right channel. It does not mix one into the other. For
     * example, when setting the position of a stereo stream all the way to the right, the left
     * channel will be completely inaudible. It will not be mixed into the right channel.
     *
     * \param position
     *  Must be between -1.0 (all the way to the left) and 1.0 (all the way to the right) with 0
     *  being the center position.
     */
    virtual void setStereoPosition(float position);

    /*!
     * \brief Returns the currently set stereo position.
     */
    virtual auto getStereoPosition() const -> float;

    /*!
     * \brief Mute the stream.
     *
     * A muted stream still accepts volume changes, but it will stay inaudible until it is unmuted.
     */
    virtual void mute();

    /*!
     * \brief Unmute the stream.
     */
    virtual void unmute();

    /*!
     * \brief Returns true if the stream is muted, false otherwise.
     */
    virtual auto isMuted() const -> bool;

    /*!
     * \brief Get current playback state.
     *
     * Note that a paused stream is still considered as being in the playback state.
     *
     * \return
     *  \retval true Playback has been started.
     *  \retval false Playback has not been started yet, or was stopped.
     */
    virtual auto isPlaying() const -> bool;

    /*!
     * \brief Get current pause state.
     *
     * Note that a stream that is not in playback state is not considered paused. This will return
     * false even for streams where playback is stopped.
     *
     * \return
     *  \retval true The stream is currently paused.
     *  \retval false The stream is currently not paused.
     */
    virtual auto isPaused() const -> bool;

    /*!
     * \brief Get stream duration.
     *
     * It is possible that for some streams (for example MOD files and some MP3 files), the reported
     * duration can be wrong. For some streams, it might not even be possible to get a duration at
     * all (MIDI files, for example.)
     *
     * \return
     * Stream duration. If the stream does not provide duration information, a zero duration is
     * returned.
     */
    virtual auto duration() const -> std::chrono::microseconds;

    /*!
     * \brief Seek to a time position in the stream.
     *
     * This will change the current playback position in the stream to the specified time.
     *
     * Note that for some streams (for example MOD files and some MP3 files), it might not be
     * possible to do an accurate seek, in which case the position that is set might be off by
     * some margin. In some streams, seeking is not possible at all (MIDI files, for example.)
     *
     * \param pos
     *  Position to seek to.
     *
     * \return
     *  \retval true The playback position was changed successfully.
     *  \retval false This stream does not support seeking.
     */
    virtual auto seekToTime(std::chrono::microseconds pos) -> bool;

    /*!
     * \brief Set a callback for when the stream finishes playback.
     *
     * The callback will be called when the stream finishes playing. This can happen when you
     * manually stop it, or when it finishes playing on its own.
     *
     * \param func
     *  Anything that can be wrapped by an std::function (like a function pointer, functor or
     *  lambda.)
     */
    void setFinishCallback(Callback func);

    /*!
     * \brief Removes any finish-playback callback that was previously set.
     */
    void unsetFinishCallback();

    /*!
     * \brief Set a callback for when the stream loops.
     *
     * The callback will be called when the stream loops. It will be called once per loop.
     *
     * \param func
     *  Anything that can be wrapped by an std::function (like a function pointer, functor or
     *  lambda.)
     */
    void setLoopCallback(Callback func);

    /*!
     * \brief Removes any loop callback that was previously set.
     */
    void unsetLoopCallback();

    /*!
     * \brief Add an audio processor to the bottom of the processor list.
     *
     * You can add multiple processors. They will be run in the order they were added, each one
     * using the previous processor's output as input. If the processor instance already exists in
     * the processor list, or is a nullptr, the function does nothing.
     *
     * \param processor The processor to add.
     */
    void addProcessor(std::shared_ptr<Processor> processor);

    /*!
     * \brief Remove a processor from the stream.
     *
     * If the processor instance is not found, the function does nothing.
     *
     * \param processor Processor to remove.
     */
    void removeProcessor(Processor* processor);

    /*!
     * \brief Remove all processors from the stream.
     */
    void clearProcessors();

protected:
    /*!
     * \brief Invokes the finish-playback callback, if there is one.
     *
     * In your subclass, you must call this function whenever your stream stops playing.
     */
    void invokeFinishCallback();

    /*!
     * \brief Invokes the loop callback, if there is one.
     *
     * In your subclass, you must call this function whenever your stream loops.
     */
    void invokeLoopCallback();

private:
    friend struct Stream_priv;
    friend auto Aulib::init(int, AudioFormat, int, int, const std::string&) -> bool;

    const std::unique_ptr<struct Stream_priv> d;
};

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
