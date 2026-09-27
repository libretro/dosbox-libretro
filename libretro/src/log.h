// This is copyrighted software. More information is at the end of this file.
#pragma once
#include "libretro.h"
#include <filesystem>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <string_view>
#include <utility>

namespace retro::internal {

extern retro_log_printf_t log_cb;
extern retro_log_level log_level;

} // namespace retro::internal

namespace retro {

template <typename... Args>
void log(const retro_log_level msg_level, fmt::format_string<Args...>&& fmt_str, Args&&... args)
{
    if (internal::log_cb) {
        internal::log_cb(
            msg_level, "%s\n",
            fmt::format(
                std::forward<fmt::format_string<Args...>>(fmt_str), std::forward<Args>(args)...)
                .c_str());
    } else if (internal::log_level <= msg_level) {
        const auto level_str = [msg_level]() -> std::string_view {
            switch (msg_level) {
            case RETRO_LOG_DEBUG:
                return "DEBUG";
            case RETRO_LOG_INFO:
                return "INFO";
            case RETRO_LOG_WARN:
                return "WARN";
            case RETRO_LOG_ERROR:
                return "ERROR";
            case RETRO_LOG_DUMMY:
                break;
            }
            return {};
        }();

        fmt::print(
            msg_level >= RETRO_LOG_WARN ? stderr : stdout, "[libretro {}] {}\n", level_str,
            fmt::format(
                std::forward<fmt::format_string<Args...>>(fmt_str), std::forward<Args>(args)...));
    }
}

/* Set the libretro log callback to use. If this is never called, or called with a null argument,
 * stdout/stderr will be used for output.
 */
void setRetroLogCb(retro_log_printf_t cb);

void setLoggingLevel(const retro_log_level log_level);

template <typename... Args>
void logDebug(fmt::format_string<Args...>&& fmt_str, Args&&... args)
{
    log(RETRO_LOG_DEBUG, std::forward<fmt::format_string<Args...>>(fmt_str),
        std::forward<Args>(args)...);
}

template <typename... Args>
void logInfo(fmt::format_string<Args...>&& fmt_str, Args&&... args)
{
    log(RETRO_LOG_INFO, std::forward<fmt::format_string<Args...>>(fmt_str),
        std::forward<Args>(args)...);
}

template <typename... Args>
void logWarn(fmt::format_string<Args...>&& fmt_str, Args&&... args)
{
    log(RETRO_LOG_WARN, std::forward<fmt::format_string<Args...>>(fmt_str),
        std::forward<Args>(args)...);
}

template <typename... Args>
void logError(fmt::format_string<Args...>&& fmt_str, Args&&... args)
{
    log(RETRO_LOG_ERROR, std::forward<fmt::format_string<Args...>>(fmt_str),
        std::forward<Args>(args)...);
}

template <typename... Args>
void dosboxLogMsgHandler(const std::string_view format, Args&&... args)
{
    log(RETRO_LOG_INFO, "dosbox: {}", fmt::sprintf(format, std::forward<Args>(args)...));
}

} // namespace retro

/* Make std::filesystem::path formattable.
 */
template <>
struct fmt::formatter<std::filesystem::path>: formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const std::filesystem::path& path, FormatContext& ctx)
    {
        return formatter<std::string_view>::format(path.string(), ctx);
    }
};

/*

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
