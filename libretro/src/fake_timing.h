// This is copyrighted software. More information is at the end of this file.
#pragma once
#include <cstdint>

/*
 * Routines for faking sleep() calls to dosbox.
 *
 * Normally, dosbox does its own timing so that each emulated frame loop takes 1000/FPS
 * milliseconds. This is not good for us. We need dosbox to deliver each frame as soon as possible
 * and let the frontend deal with timing. Do do that, we provide fake delay and tick routines
 * and let dosbox use those instead of SDL_Delay() and SDL_GetTick().
 *
 * (Instead of doing any of this, we could instead run dosbox in DOSBOX_UnlockSpeed(true) mode, but
 * this disables dosbox's automatic CPU cycle adjustment.)
 */

/* Fake sleep function that doesn't sleep but returns immediately.
 */
void fakeDelay(std::uint32_t ms) noexcept;

/* Returns the current tick but offset to the future by the total amount of fake sleeps performed so
 * far.
 */
[[nodiscard]]
auto fakeGetTicks() noexcept -> std::uint32_t;

/* Reset fake sleep offset back to 0. Should be called once each frame.
 */
void fakeTimingReset() noexcept;

/*

Copyright (C) 2019 Nikos Chantziaras.

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
