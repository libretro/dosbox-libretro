// This is copyrighted software. More information is at the end of this file.
#pragma once
#include <condition_variable>
#include <mutex>

struct SDL_Thread;
using SDL_mutex = std::mutex;
using SDL_cond = std::condition_variable_any;

auto SDL_CreateThread(int (*fn)(void*), void* const data) noexcept -> SDL_Thread*;
void SDL_WaitThread(SDL_Thread* const thread, int* const status);

inline auto SDL_CreateMutex() noexcept -> SDL_mutex*
{
    try {
        return new std::mutex;
    }
    catch (...) {
        return nullptr;
    }
}

inline void SDL_DestroyMutex(SDL_mutex* const mutex) noexcept
{
    delete mutex;
}

inline auto SDL_CreateCond() noexcept -> SDL_cond*
{
    try {
        return new std::condition_variable_any;
    }
    catch (...) {
        return nullptr;
    }
}

inline auto SDL_CondWait(SDL_cond* const cond, SDL_mutex* const mutex) noexcept -> int
{
    try {
        cond->wait(*mutex);
        return 0;
    }
    catch (...) {
        return -1;
    }
}

inline void SDL_DestroyCond(SDL_cond* const cond) noexcept
{
    delete cond;
}

inline auto SDL_LockMutex(SDL_mutex* const mutex) noexcept -> int
{
    try {
        mutex->lock();
        return 0;
    }
    catch (...) {
        return -1;
    }
}

inline auto SDL_mutexP(SDL_mutex* const mutex) noexcept -> int
{
    return SDL_LockMutex(mutex);
}

inline auto SDL_UnlockMutex(SDL_mutex* const mutex) noexcept -> int
{
    mutex->unlock();
    return 0;
}

inline auto SDL_mutexV(SDL_mutex* const mutex) noexcept -> int
{
    return SDL_UnlockMutex(mutex);
}

inline auto SDL_CondSignal(SDL_cond* const cond) noexcept -> int
{
    cond->notify_one();
    return 0;
}

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
