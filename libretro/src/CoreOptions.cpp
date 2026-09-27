// This is copyrighted software. More information is at the end of this file.
#include "CoreOptions.h"

#include "log.h"
#include <algorithm>

namespace retro {

auto CoreOptions::envCbFallback(unsigned /*cmd*/, void* /*data*/) -> bool
{
    retro::logError("Core options running without environment callback.");
    return false;
}

void retro::CoreOptions::setEnvironmentCallback(const retro_environment_t cb)
{
    env_cb_ = cb;
    api_has_set_variable = env_cb_(RETRO_ENVIRONMENT_SET_VARIABLE, nullptr);
}

auto CoreOptions::operator[](const std::string_view key) const -> const CoreOptionValue&
{
    const auto* option = this->option(key);
    if (!option) {
        retro::logError("Tried to access non-existent core option \"{}\".", key);
        return invalid_value_;
    }

    retro_variable var{option->key().c_str(), nullptr};
    if (!env_cb_(RETRO_ENVIRONMENT_GET_VARIABLE, &var)) {
        retro::logError("RETRO_ENVIRONMENT_GET_VARIABLE failed for core option \"{}\".", key);
        return option->defaultValue();
    } else if (!var.value) {
        retro::logError(
            "RETRO_ENVIRONMENT_GET_VARIABLE found no core option \"{}\".", option->key());
        return option->defaultValue();
    }

    auto value = std::find_if(option->begin(), option->end(), [var_val = var.value](const auto& a) {
        return a.toString() == var_val;
    });
    if (value == option->end()) {
        retro::logError("Current value for core option \"{}\" not found.", key);
        return option->defaultValue();
    }
    return *value;
}

auto CoreOptions::changed() const noexcept -> bool
{
    bool updated = false;
    env_cb_(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated);
    return updated;
}

void CoreOptions::updateFrontend()
{
    updateRetroOptions();
    retro_core_options_v2 v2_options{retro_categories_v2_.data(), retro_options_v2_.data()};

    unsigned version = 0;
    env_cb_(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version);

    if (version >= 2) {
        retro_core_options_v2_intl core_options_intl{&v2_options, nullptr};
        if (!env_cb_(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL, &core_options_intl)) {
            env_cb_(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &v2_options);
        }
    } else if (version == 1) {
        updateFrontendV1();
    } else {
        updateFrontendV0();
    }

    // The frontend resets the visibility status of all options when re-submitting them, so we have
    // to restore it again manually.
    for (const auto& opt_or_cat : options_and_categories_) {
        if (std::holds_alternative<CoreOptionDefinition>(opt_or_cat)) {
            const auto& opt = std::get<CoreOptionDefinition>(opt_or_cat);
            retro_core_option_display option_display{opt.key().c_str(), opt.is_visible_};
            env_cb_(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
        } else {
            const auto& cat = std::get<CoreOptionCategory>(opt_or_cat);
            for (const auto& opt : cat.options()) {
                retro_core_option_display option_display{opt.key().c_str(), opt.is_visible_};
                env_cb_(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
            }
        }
    }
}

auto CoreOptions::setVisible(const std::string_view key, const bool visible) noexcept -> bool
{
    auto* const opt = option(key);
    if (!opt) {
        retro::logError("Tried to set visibility for non-existent core option \"{}\".", key);
        return false;
    }
    if (opt->is_visible_ == visible) {
        return false;
    }
    opt->is_visible_ = visible;
    retro_core_option_display option_display{opt->key().c_str(), visible};
    env_cb_(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
    return true;
}

auto CoreOptions::setVisible(
    const std::initializer_list<const std::string_view> keys, const bool visible) noexcept -> bool
{
    bool visibility_changed = false;
    for (const auto key : keys) {
        visibility_changed |= setVisible(key, visible);
    }
    return visibility_changed;
}

auto CoreOptions::setVisible(const std::regex& exp, bool visible) noexcept -> bool
{
    bool visibility_changed = false;
    for (const auto& i : options_map_) {
        if (std::regex_search(i.first, exp)) {
            visibility_changed |= setVisible(i.first, visible);
        }
    }
    return visibility_changed;
}

void CoreOptions::setCurrentValue(std::string_view key, const CoreOptionValue& value)
{
    auto* option = this->option(key);
    if (!option) {
        retro::logError("Tried to set current value of non-existent core option \"{}\".", key);
        return;
    }

    if (!api_has_set_variable) {
        // The frontend doesn't support the SET_VARIABLE env command. Use this hack instead in an
        // attempt to still make it work regardless. We remove all current option values and replace
        // them with a bogus, temporary one. Then we put the original values back but change the
        // default value to the value that was requested, which will result in it becoming the new
        // current value. Finally, we restore the original default value.
        auto default_value = option->defaultValue();
        auto values = option->clearValues();
        option->setValues({"_bogus_"}, "_bogus_");
        updateFrontend();
        option->setValues(values, value);
        updateFrontend();
        option->setDefaultValue(default_value);
        updateFrontend();
        return;
    }

    retro_variable var{option->key().c_str(), value.toString().c_str()};
    if (!env_cb_(RETRO_ENVIRONMENT_SET_VARIABLE, &var)) {
        retro::logError(
            "Failed to change current value of core option \"{}\" to \"{}\".", key,
            value.toString());
    }
}

static auto make_retro_core_option_v2_definition(
    const CoreOptionCategory* category, const CoreOptionDefinition& option,
    const char* desc_with_category) -> retro_core_option_v2_definition
{
    retro_core_option_v2_definition def{};

    def.key = option.key().c_str();
    def.desc = desc_with_category;
    def.desc_categorized = option.desc().c_str();
    def.info = option.info().empty() ? nullptr : option.info().c_str();
    def.category_key = category ? category->key().c_str() : nullptr;
    int i = 0;
    for (const auto& value : option) {
        if (i == RETRO_NUM_CORE_OPTION_VALUES_MAX - 1) {
            retro::logError(
                "Core option \"{}\" exceeds maximum value count of {}.", def.key,
                RETRO_NUM_CORE_OPTION_VALUES_MAX - 1);
            break;
        }
        def.values[i].value = value.toString().c_str();
        def.values[i].label = value.label().empty() ? nullptr : value.label().c_str();
        ++i;
    }
    def.values[i] = {};
    def.default_value =
        option.defaultValue().isValid() ? option.defaultValue().toString().c_str() : nullptr;
    return def;
}

static auto option_has_empty_string_value(const CoreOptionDefinition& option) -> bool
{
    for (const auto& val : option) {
        if (val.isString() && val.toString().empty()) {
            return true;
        }
    }
    return false;
}

void CoreOptions::updateRetroOptions()
{
    retro_options_v2_.clear();
    retro_options_v2_.reserve(options_map_.size() + 1);
    retro_categories_v2_.clear();
    categorized_option_descriptions_.clear();
    categorized_option_descriptions_.reserve(options_map_.size());
    for (const auto& option_or_category : options_and_categories_) {
        if (std::holds_alternative<CoreOptionDefinition>(option_or_category)) {
            const auto& option = std::get<CoreOptionDefinition>(option_or_category);
            const bool has_empty_value = option_has_empty_string_value(option);
            if (option.isEmpty()) {
                retro::logError("Tried to submit \"{}\" option with no values.", option.key());
            } else if (has_empty_value) {
                retro::logError(
                    "Tried to submit \"{}\" option containing empty string value.", option.key());
            } else {
                retro_options_v2_.push_back(
                    make_retro_core_option_v2_definition(nullptr, option, option.desc().c_str()));
            }
        } else {
            const auto& category = std::get<CoreOptionCategory>(option_or_category);
            retro_core_option_v2_category retro_category{};
            retro_category.key = category.key().c_str();
            retro_category.desc = category.desc().c_str();
            retro_category.info = category.info().c_str();
            retro_categories_v2_.push_back(retro_category);
            for (const auto& option : category.options()) {
                const bool has_empty_value = option_has_empty_string_value(option);
                if (option.isEmpty()) {
                    retro::logError("Tried to submit \"{}\" option with no values.", option.key());
                } else if (has_empty_value) {
                    retro::logError(
                        "Tried to submit \"{}\" option containing empty string value.",
                        option.key());
                } else {
                    categorized_option_descriptions_.emplace_back(
                        category.desc() + ": " + option.desc());
                    retro_options_v2_.push_back(make_retro_core_option_v2_definition(
                        &category, option, categorized_option_descriptions_.back().c_str()));
                }
            }
        }
    }
    retro_options_v2_.push_back({});
    retro_categories_v2_.push_back({});
}

void CoreOptions::updateFrontendV0()
{
    std::vector<retro_variable> variables;
    std::vector<std::string> value_strings;
    variables.reserve(retro_options_v2_.size());
    value_strings.reserve(retro_options_v2_.size());

    for (size_t i = 0; i < retro_options_v2_.size() - 1; ++i) {
        // Build values string.
        std::string str = retro_options_v2_[i].desc;
        str += "; ";
        // Default value goes first.
        str += retro_options_v2_[i].default_value;
        // Add remaining values.
        for (const auto& value : retro_options_v2_[i].values) {
            if (!value.label && !value.value) {
                break;
            }
            if (value.value != retro_options_v2_[i].default_value) {
                str += '|';
                str += value.value;
            }
        }
        value_strings.emplace_back(std::move(str));
        variables.push_back({retro_options_v2_[i].key, value_strings.back().c_str()});
    }
    variables.push_back({});
    env_cb_(RETRO_ENVIRONMENT_SET_VARIABLES, variables.data());
}

void CoreOptions::updateFrontendV1()
{
    std::vector<retro_core_option_definition> retro_options_;
    retro_options_.reserve(retro_options_v2_.size());

    for (const auto& option_v2 : retro_options_v2_) {
        retro_core_option_definition option_v1{};
        option_v1.key = option_v2.key;
        option_v1.desc = option_v2.desc;
        option_v1.info = option_v2.info;
        std::copy(
            std::begin(option_v2.values), std::end(option_v2.values), std::begin(option_v1.values));
        option_v1.default_value = option_v2.default_value;
        retro_options_.push_back(option_v1);
    }

    retro_core_options_intl core_options_intl{retro_options_.data(), nullptr};
    if (!env_cb_(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl)) {
        env_cb_(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, retro_options_.data());
    }
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
