// This is copyrighted software. More information is at the end of this file.
#pragma once
#include "libretro.h"
#include <chrono>
#include <fmt/core.h>
#include <string>

namespace retro::internal {

void showOsdImpl(
    const char* msg, std::chrono::milliseconds duration, int priority, retro_log_level log_level,
    retro_message_type msg_type) noexcept;

inline void showOsdImpl(
    const std::string& msg, const std::chrono::milliseconds duration, const int priority,
    const retro_log_level log_level, const retro_message_type msg_type) noexcept
{
    showOsdImpl(msg.data(), duration, priority, log_level, msg_type);
}

} // namespace retro::internal

namespace retro {
using namespace std::chrono_literals;

void setMessageEnvCb(retro_environment_t cb);

inline void showOsdInfo(
    const std::string& msg, const retro_message_type msg_type,
    const std::chrono::milliseconds duration = 2s, const int priority = 50) noexcept
{
    internal::showOsdImpl(msg, duration, priority, RETRO_LOG_INFO, msg_type);
}

inline void showOsdInfo(
    const char* const msg, const retro_message_type msg_type,
    const std::chrono::milliseconds duration = 2s, const int priority = 50) noexcept
{
    internal::showOsdImpl(msg, duration, priority, RETRO_LOG_INFO, msg_type);
}

inline void showOsdWarn(
    const std::string& msg, const retro_message_type msg_type,
    const std::chrono::milliseconds duration = 4s, const int priority = 50) noexcept
{
    internal::showOsdImpl(msg, duration, priority, RETRO_LOG_WARN, msg_type);
}

inline void showOsdWarn(
    const char* const msg, const retro_message_type msg_type,
    const std::chrono::milliseconds duration = 4s, const int priority = 50) noexcept
{
    internal::showOsdImpl(msg, duration, priority, RETRO_LOG_WARN, msg_type);
}

inline void showOsdError(
    const std::string& msg, const retro_message_type msg_type,
    const std::chrono::milliseconds duration = 6s, const int priority = 50) noexcept
{
    internal::showOsdImpl(msg, duration, priority, RETRO_LOG_ERROR, msg_type);
}

inline void showOsdError(
    const char* const msg, const retro_message_type msg_type,
    const std::chrono::milliseconds duration = 6s, const int priority = 50) noexcept
{
    internal::showOsdImpl(msg, duration, priority, RETRO_LOG_ERROR, msg_type);
}

} // namespace retro

/*

Copyright (C) 2022 Nikos Chantziaras.

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
