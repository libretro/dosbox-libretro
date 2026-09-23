// This is copyrighted software. More information is at the end of this file.
#pragma once
#include "CoreOptionValue.h"
#include "log.h"
#include <string>
#include <utility>
#include <vector>

namespace retro {

class CoreOptions;

class CoreOptionDefinition final
{
public:
    CoreOptionDefinition(
        std::string key, std::string desc, std::string info, std::vector<CoreOptionValue> values,
        const CoreOptionValue& default_value);
    CoreOptionDefinition(
        std::string key, std::string desc, std::vector<CoreOptionValue> values,
        const CoreOptionValue& default_value);
    CoreOptionDefinition(std::string key, std::string desc, std::string info = {});

    [[nodiscard]]
    auto key() const noexcept -> const std::string&;

    void setKey(std::string key);

    [[nodiscard]]
    auto desc() const noexcept -> const std::string&;

    void setDesc(std::string new_desc) noexcept;

    [[nodiscard]]
    auto info() const noexcept -> const std::string&;

    void setInfo(std::string new_info) noexcept;

    /* Returns the description and information strings separated by a newline.
     */
    [[nodiscard]]
    auto descAndInfo() const noexcept -> std::string;

    [[nodiscard]]
    auto defaultValue() const noexcept -> const CoreOptionValue&;

    void setDefaultValue(const CoreOptionValue& default_value) noexcept;

    void setValues(
        std::vector<CoreOptionValue> values, const CoreOptionValue& default_value) noexcept;

    void addValue(CoreOptionValue value) noexcept;

    auto clearValues() noexcept -> std::vector<CoreOptionValue>;

    [[nodiscard]]
    auto begin() const noexcept;

    [[nodiscard]]
    auto end() const noexcept;

    [[nodiscard]]
    auto size() const noexcept -> int;

    [[nodiscard]]
    auto isEmpty() const noexcept -> bool;

private:
    std::string key_;
    std::string desc_;
    std::string info_;
    std::vector<CoreOptionValue> values_;
    int default_value_index_ = 0;
    CoreOptionValue invalid_value_{""};
    bool is_visible_ = true;

    friend CoreOptions;
};

inline CoreOptionDefinition::CoreOptionDefinition(
    std::string key, std::string desc, std::string info, std::vector<CoreOptionValue> values,
    const CoreOptionValue& default_value)
    : key_(std::move(key))
    , desc_(std::move(desc))
    , info_(std::move(info))
{
    setValues(std::move(values), default_value);
}

inline CoreOptionDefinition::CoreOptionDefinition(
    std::string key, std::string desc, std::vector<CoreOptionValue> values,
    const CoreOptionValue& default_value)
    : CoreOptionDefinition(std::move(key), std::move(desc), {}, std::move(values), default_value)
{ }

inline CoreOptionDefinition::CoreOptionDefinition(
    std::string key, std::string desc, std::string info)
    : key_(std::move(key))
    , desc_(std::move(desc))
    , info_(std::move(info))
{ }

inline auto CoreOptionDefinition::key() const noexcept -> const std::string&
{
    return key_;
}

inline void CoreOptionDefinition::setKey(std::string key)
{
    key_ = std::move(key);
}

inline auto CoreOptionDefinition::desc() const noexcept -> const std::string&
{
    return desc_;
}

inline void CoreOptionDefinition::setDesc(std::string new_desc) noexcept
{
    desc_ = std::move(new_desc);
}

inline auto CoreOptionDefinition::info() const noexcept -> const std::string&
{
    return info_;
}

inline void CoreOptionDefinition::setInfo(std::string new_info) noexcept
{
    info_ = std::move(new_info);
}

inline auto CoreOptionDefinition::descAndInfo() const noexcept -> std::string
{
    return desc() + '\n' + info();
}

inline auto CoreOptionDefinition::defaultValue() const noexcept -> const CoreOptionValue&
{
    if (values_.empty()) {
        retro::logError("Tried to query default value of empty core option \"{}\".", key());
        return invalid_value_;
    }
    return values_[default_value_index_];
}

inline void CoreOptionDefinition::setValues(
    std::vector<CoreOptionValue> values, const CoreOptionValue& default_value) noexcept
{
    values_ = std::move(values);
    setDefaultValue(default_value);
}

inline void CoreOptionDefinition::addValue(CoreOptionValue value) noexcept
{
    values_.emplace_back(std::move(value));
}

inline auto CoreOptionDefinition::begin() const noexcept
{
    return values_.cbegin();
}

inline auto CoreOptionDefinition::end() const noexcept
{
    return values_.cend();
}

inline auto CoreOptionDefinition::size() const noexcept -> int
{
    return static_cast<int>(values_.size());
}

inline auto CoreOptionDefinition::isEmpty() const noexcept -> bool
{
    return size() == 0;
}

} // namespace retro

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
