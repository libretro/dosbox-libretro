// This is copyrighted software. More information is at the end of this file.
#include "Aulib/Stream.h"

#include "Aulib/Decoder.h"
#include "Aulib/Processor.h"
#include "Aulib/Resampler.h"
#include "SdlAudioLocker.h"
#include "aulib.h"
#include "aulib_debug.h"
#include "aulib_global.h"
#include "missing/algorithm.h"
#include "sampleconv.h"
#include "stream_p.h"
#include <SDL_audio.h>
#include <SDL_timer.h>
#include <mutex>

Aulib::Stream::Stream(const std::string& filename, std::unique_ptr<Decoder> decoder,
                      std::unique_ptr<Resampler> resampler)
    : Stream(SDL_RWFromFile(filename.c_str(), "rb"), std::move(decoder), std::move(resampler), true)
{}

Aulib::Stream::Stream(const std::string& filename, std::unique_ptr<Decoder> decoder)
    : Stream(SDL_RWFromFile(filename.c_str(), "rb"), std::move(decoder), true)
{}

Aulib::Stream::Stream(SDL_RWops* rwops, std::unique_ptr<Decoder> decoder,
                      std::unique_ptr<Resampler> resampler, bool closeRw)
    : d(std::make_unique<Stream_priv>(this, std::move(decoder), std::move(resampler), rwops,
                                      closeRw))
{}

Aulib::Stream::Stream(SDL_RWops* rwops, std::unique_ptr<Decoder> decoder, bool closeRw)
    : d(std::make_unique<Stream_priv>(this, std::move(decoder), nullptr, rwops, closeRw))
{}

Aulib::Stream::~Stream()
{
    SdlAudioLocker lock;

    d->fStop();
}

auto Aulib::Stream::open() -> bool
{
    SdlAudioLocker lock;

    if (d->fIsOpen) {
        return true;
    }
    if (not d->fDecoder->open(d->fRWops)) {
        return false;
    }
    if (d->fResampler) {
        d->fResampler->setSpec(Aulib::sampleRate(), Aulib::channelCount(), Aulib::frameSize());
    }
    d->fIsOpen = true;
    return true;
}

auto Aulib::Stream::play(int iterations, std::chrono::microseconds fadeTime) -> bool
{
    if (not open()) {
        return false;
    }

    SdlAudioLocker locker;

    if (d->fIsPlaying) {
        return true;
    }
    d->fCurrentIteration = 0;
    d->fWantedIterations = iterations;
    d->fPlaybackStartTick = SDL_GetTicks();
    d->fStarting = true;
    if (fadeTime.count() > 0) {
        d->fInternalVolume = 0.f;
        d->fFadingIn = true;
        d->fFadingOut = false;
        d->fFadeInDuration = std::chrono::duration_cast<std::chrono::milliseconds>(fadeTime);
        d->fFadeInStartTick = d->fPlaybackStartTick;
    } else {
        d->fInternalVolume = 1.f;
        d->fFadingIn = false;
    }
    d->fIsPlaying = true;
    {
        std::lock_guard<SdlMutex> lock(d->fStreamListMutex);
        d->fStreamList.push_back(this);
    }
    return true;
}

void Aulib::Stream::stop(std::chrono::microseconds fadeTime)
{
    SdlAudioLocker lock;

    if (fadeTime.count() > 0) {
        d->fFadingIn = false;
        d->fFadingOut = true;
        d->fFadeOutDuration = std::chrono::duration_cast<std::chrono::milliseconds>(fadeTime);
        d->fFadeOutStartTick = SDL_GetTicks();
        d->fStopAfterFade = true;
    } else {
        d->fStop();
    }
}

void Aulib::Stream::pause(std::chrono::microseconds fadeTime)
{
    if (not open()) {
        return;
    }

    SdlAudioLocker locker;

    if (d->fIsPaused) {
        return;
    }
    if (fadeTime.count() > 0) {
        d->fFadingIn = false;
        d->fFadingOut = true;
        d->fFadeOutDuration = std::chrono::duration_cast<std::chrono::milliseconds>(fadeTime);
        d->fFadeOutStartTick = SDL_GetTicks();
        d->fStopAfterFade = false;
    } else {
        d->fIsPaused = true;
    }
}

void Aulib::Stream::resume(std::chrono::microseconds fadeTime)
{
    SdlAudioLocker locker;

    if (not d->fIsPaused) {
        return;
    }
    if (fadeTime.count() > 0) {
        d->fInternalVolume = 0.f;
        d->fFadingIn = true;
        d->fFadingOut = false;
        d->fFadeInDuration = std::chrono::duration_cast<std::chrono::milliseconds>(fadeTime);
        d->fFadeInStartTick = SDL_GetTicks();
    } else {
        d->fInternalVolume = 1.f;
    }
    d->fIsPaused = false;
}

auto Aulib::Stream::rewind() -> bool
{
    if (not open()) {
        return false;
    }

    SdlAudioLocker locker;
    return d->fDecoder->rewind();
}

void Aulib::Stream::setVolume(float volume)
{
    SdlAudioLocker locker;

    if (volume < 0.f) {
        volume = 0.f;
    }
    d->fVolume = volume;
}

auto Aulib::Stream::volume() const -> float
{
    SdlAudioLocker locker;

    return d->fVolume;
}

void Aulib::Stream::setStereoPosition(const float position)
{
    SdlAudioLocker locker;

    d->fStereoPos = Aulib::priv::clamp(position, -1.f, 1.f);
}

auto Aulib::Stream::getStereoPosition() const -> float
{
    SdlAudioLocker locker;

    return d->fStereoPos;
}

void Aulib::Stream::mute()
{
    SdlAudioLocker locker;

    d->fIsMuted = true;
}

void Aulib::Stream::unmute()
{
    SdlAudioLocker locker;

    d->fIsMuted = false;
}

auto Aulib::Stream::isMuted() const -> bool
{
    SdlAudioLocker locker;

    return d->fIsMuted;
}

auto Aulib::Stream::isPlaying() const -> bool
{
    SdlAudioLocker locker;

    return d->fIsPlaying;
}

auto Aulib::Stream::isPaused() const -> bool
{
    SdlAudioLocker locker;

    return d->fIsPaused;
}

auto Aulib::Stream::duration() const -> std::chrono::microseconds
{
    SdlAudioLocker locker;

    return d->fDecoder->duration();
}

auto Aulib::Stream::seekToTime(std::chrono::microseconds pos) -> bool
{
    SdlAudioLocker locker;

    return d->fDecoder->seekToTime(pos);
}

void Aulib::Stream::setFinishCallback(Callback func)
{
    SdlAudioLocker locker;

    d->fFinishCallback = std::move(func);
}

void Aulib::Stream::unsetFinishCallback()
{
    SdlAudioLocker locker;

    d->fFinishCallback = nullptr;
}

void Aulib::Stream::setLoopCallback(Callback func)
{
    SdlAudioLocker locker;

    d->fLoopCallback = std::move(func);
}

void Aulib::Stream::unsetLoopCallback()
{
    SdlAudioLocker locker;

    d->fLoopCallback = nullptr;
}

void Aulib::Stream::addProcessor(std::shared_ptr<Processor> processor)
{
    SdlAudioLocker locker;

    if (not processor
        or std::find_if(
               d->processors.begin(), d->processors.end(),
               [&processor](std::shared_ptr<Processor>& p) { return p.get() == processor.get(); })
               != d->processors.end()) {
        return;
    }
    d->processors.push_back(std::move(processor));
}

void Aulib::Stream::removeProcessor(Processor* processor)
{
    SdlAudioLocker locker;

    auto it =
        std::find_if(d->processors.begin(), d->processors.end(),
                     [&processor](std::shared_ptr<Processor>& p) { return p.get() == processor; });
    if (it == d->processors.end()) {
        return;
    }
    d->processors.erase(it);
}

void Aulib::Stream::clearProcessors()
{
    SdlAudioLocker locker;

    d->processors.clear();
}

void Aulib::Stream::invokeFinishCallback()
{
    if (d->fFinishCallback) {
        d->fFinishCallback(*this);
    }
}

void Aulib::Stream::invokeLoopCallback()
{
    if (d->fLoopCallback) {
        d->fLoopCallback(*this);
    }
}

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
