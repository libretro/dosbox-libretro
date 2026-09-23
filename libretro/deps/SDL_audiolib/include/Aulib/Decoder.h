// This is copyrighted software. More information is at the end of this file.
#pragma once

#include "aulib_export.h"
#include <SDL_rwops.h>
#include <SDL_stdinc.h>
#include <chrono>
#include <memory>
#include <string>
#include <type_traits>

struct SDL_RWops;

namespace Aulib {

/*!
 * \brief Abstract base class for audio decoders.
 */
class AULIB_EXPORT Decoder
{
public:
    Decoder();
    virtual ~Decoder();

    Decoder(const Decoder&) = delete;
    auto operator=(const Decoder&) -> Decoder& = delete;

#if __cplusplus >= 201603
    /*!
     * \brief Find and return an instance of the first decoder that can open the specified file.
     *
     * Only the specified decoder types will be tried.
     *
     * \return A suitable decoder or nullptr if none of the decoders can open the file.
     */
    template <class... Decoders>
    static auto decoderFor(const std::string& filename) -> std::unique_ptr<Decoder>;
    //! \overload
    template <class... Decoders>
    static auto decoderFor(SDL_RWops* rwops) -> std::unique_ptr<Decoder>;
#endif

    /*!
     * \brief Find and return an instance of the first decoder that can open the specified file.
     *
     * All decoders known by SDL_Audiolib will be tried. If you want to try your own decoders or
     * limit the list of tried decoders, then use the templated version of this function instead.
     *
     * \return A suitable decoder or nullptr if none of the decoders can open the file.
     */
    static auto decoderFor(const std::string& filename) -> std::unique_ptr<Decoder>;
    //! \overload
    static auto decoderFor(SDL_RWops* rwops) -> std::unique_ptr<Decoder>;

    auto isOpen() const -> bool;
    auto decode(float buf[], int len, bool& callAgain) -> int;

    virtual auto open(SDL_RWops* rwops) -> bool = 0;
    virtual auto getChannels() const -> int = 0;
    virtual auto getRate() const -> int = 0;
    virtual auto rewind() -> bool = 0;
    virtual auto duration() const -> std::chrono::microseconds = 0;
    virtual auto seekToTime(std::chrono::microseconds pos) -> bool = 0;

protected:
    void setIsOpen(bool f);
    virtual auto doDecoding(float buf[], int len, bool& callAgain) -> int = 0;

private:
    const std::unique_ptr<struct Decoder_priv> d;
};

#if __cplusplus >= 201603
template <class... Decoders>
inline auto Decoder::decoderFor(const std::string& filename) -> std::unique_ptr<Decoder>
{
    auto rwopsClose = [](SDL_RWops* rwops) { SDL_RWclose(rwops); };
    std::unique_ptr<SDL_RWops, decltype(rwopsClose)> rwops(SDL_RWFromFile(filename.c_str(), "rb"),
                                                           rwopsClose);
    return Decoder::decoderFor<Decoders...>(rwops.get());
}

template <class... Decoders>
inline auto Decoder::decoderFor(SDL_RWops* rwops) -> std::unique_ptr<Decoder>
{
    static_assert(sizeof...(Decoders) > 0, "Need at least one decoder type.");
    static_assert((std::is_base_of_v<Aulib::Decoder, Decoders> && ...),
                  "Decoders must derive from Aulib::Decoder.");

    const auto rwPos = SDL_RWtell(rwops);

    auto rewindRwops = [rwops, rwPos] { SDL_RWseek(rwops, rwPos, RW_SEEK_SET); };

    auto tryDecoder = [rwops, &rewindRwops](auto dec) {
        rewindRwops();
        bool ret = dec->open(rwops);
        rewindRwops();
        return ret;
    };

    std::unique_ptr<Decoder> decoder;
    ((tryDecoder(std::make_unique<Decoders>()) && (decoder = std::make_unique<Decoders>())) || ...);
    return decoder;
}
#endif

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
