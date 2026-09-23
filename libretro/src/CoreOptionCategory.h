// This is copyrighted software. More information is at the end of this file.
#pragma once
#include "CoreOptionDefinition.h"
#include <string>
#include <utility>
#include <vector>

namespace retro {

class CoreOptionCategory final
{
public:
    template <typename... Ts>
    CoreOptionCategory(std::string key, std::string desc, std::string info, Ts&&... Args);

    template <typename... Ts>
    CoreOptionCategory(
        std::string key, std::string desc, CoreOptionDefinition&& first, Ts&&... Args);

    [[nodiscard]]
    auto key() const noexcept -> const std::string&;

    [[nodiscard]]
    auto desc() const noexcept -> const std::string&;

    [[nodiscard]]
    auto info() const noexcept -> const std::string&;

    [[nodiscard]]
    auto options() const noexcept -> const std::vector<CoreOptionDefinition>&;

    [[nodiscard]]
    auto options() noexcept -> std::vector<CoreOptionDefinition>&;

private:
    std::string key_;
    std::string desc_;
    std::string info_;
    std::vector<CoreOptionDefinition> options_;
};

template <typename... Ts>
CoreOptionCategory::CoreOptionCategory(
    std::string key, std::string desc, std::string info, Ts&&... Args)
    : key_(std::move(key))
    , desc_(std::move(desc))
    , info_(std::move(info))
    , options_(std::vector<CoreOptionDefinition>{std::forward<CoreOptionDefinition>(Args)...})
{ }

template <typename... Ts>
CoreOptionCategory::CoreOptionCategory(
    std::string key, std::string desc, CoreOptionDefinition&& first, Ts&&... Args)
    : key_(std::move(key))
    , desc_(std::move(desc))
    , options_(std::vector<CoreOptionDefinition>{
          std::move(first), std::forward<CoreOptionDefinition>(Args)...})
{ }

inline auto CoreOptionCategory::key() const noexcept -> const std::string&
{
    return key_;
}

inline auto CoreOptionCategory::desc() const noexcept -> const std::string&
{
    return desc_;
}

inline auto CoreOptionCategory::info() const noexcept -> const std::string&
{
    return info_;
}

inline auto CoreOptionCategory::options() const noexcept -> const std::vector<CoreOptionDefinition>&
{
    return options_;
}

inline auto CoreOptionCategory::options() noexcept -> std::vector<CoreOptionDefinition>&
{
    return options_;
}

} // namespace retro

/*

Copyright (C) 2021 Nikos Chantziaras.

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
