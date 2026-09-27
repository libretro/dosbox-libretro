// This is copyrighted software. More information is at the end of this file.
#include "libretro_message.h"
#include "config.h"
#include "log.h"
#include "render.h"

static RETRO_CALLCONV auto msgFallbackCb(unsigned, void*) -> bool
{
    retro::logError("Environment callback for libretro messages not set.");
    return false;
}

static retro_environment_t msg_env_cb = msgFallbackCb;
static unsigned int msg_api_version = 0;

namespace retro::internal {

void showOsdImpl(
    const char* const msg, const std::chrono::milliseconds duration, const int priority,
    const retro_log_level log_level, const retro_message_type msg_type) noexcept
{
    using namespace std::chrono;

    if (msg_api_version < 1) {
        const float fps = render.src.fps > 1 ? render.src.fps : 60;
        const unsigned int duration_frames = fps * duration.count() / 1000.0f;
        retro_message handle{msg, duration_frames};
        msg_env_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &handle);
        retro::log(log_level, "{}",  msg);
        return;
    }

    retro_message_ext msg_ext{
        msg,
        static_cast<unsigned int>(duration.count()),
        static_cast<unsigned int>(priority),
        log_level,
        RETRO_MESSAGE_TARGET_OSD,
        msg_type,
        -1,
    };
    msg_env_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg_ext);
    retro::log(log_level, "{}",  msg);
}

} // namespace retro::internal

namespace retro {
void setMessageEnvCb(retro_environment_t cb)
{
    msg_env_cb = cb;
    msg_env_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &msg_api_version);
    retro::logDebug("Using libretro message interface version {}.", msg_api_version);
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
