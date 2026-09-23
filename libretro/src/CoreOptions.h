// This is copyrighted software. More information is at the end of this file.
#pragma once
#include "CoreOptionCategory.h"
#include "CoreOptionDefinition.h"
#include "libretro.h"
#include <initializer_list>
#include <map>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace retro {

/* Wraps the libretro core options API. Supports options API version 2, but if the frontend doesn't,
 * options are automatically converted to the older v1 or the legacy "v0" format.
 *
 * Usage is fairly straightforward:
 *
 * CoreOptions core_options {
 *     // All option keys will get automatically prefixed with this.
 *     "core_name_",
 *
 *     // Core categories and core option definitions follow. They can be defined in any order.
 *
 *     // This is an option definition. Any options not defined within a category (see below) will
 *     // be displayed at the top-lop level of the frontend's option menu.
 *     CoreOptionDefinition {
 *         // Option key. Must be unique in regard to any other option key. Will be automatically
 *         // prefixed by the global prefix we specified above. So in this case, it will be
 *         // automatically converted to "core_name_overclock".
 *         "overclock",
 *
 *         // Label. This will be displayed in the options UI of the frontend.
 *         "Overclock CPU",
 *
 *         // Description. Frontends usually display this as small text below the label.
 *         // Specifying a description is optional and can be omitted.
 *         "Overclocks the emulated CPU. Might result in glitches with some games."
 *
 *         // Possible values and their labels. Can be any combination of string, bool and int.
 *         // The label string is optional and can be omitted.
 *         {
 *             { false, "OFF" },
 *             { true, "ON" },
 *         },
 *
 *         // Default value. Must be one of the previously specified values, otherwise the first
 *         // value will be used.
 *         false
 *     },
 *
 *     // This is an options category. If the frontend supports categories, all options defined
 *     // within the category will be displayed inside a submenu (or in some other fontend-dependent
 *     // way that group options of a category together.)
 *     //
 *     // If the frontend does not support categories, the options will be automatically converted
 *     // to top-level options and their labels prefixed with the name of the category.
 *     //
 *     // Note: Categories can NOT be nested. That is, you cannot define a category within another
 *     // category. The libretro options API (as of V2) does not allow nested categories.
 *     CoreOptionCategory {
 *         // Category key. Must be unique in regard to other categories. Allowed characters are
 *         // [a-z, A-Z, 0-9, _, -].
 *         "accuracy",
 *
 *         // Category label.
 *         "Accuracy",
 *
 *         // Category description. This is optional and can be completely omitted.
 *         "Emulation settings to choose between better accuracy or better performance",
 *
 *         // Any number of options can be defined within a category. Here, we define two options.
 *         CoreOptionDefinition {
 *             "cpu_accuracy",
 *             "CPU accuracy",
 *             "If the emulator runs too slow, lower this setting. ",
 *             {
 *                 "low",
 *                 "medium",
 *                 "high",
 *                 {"perfect", "perfect (warning: extremely demanding!)"},
 *             },
 *             "medium"
 *         },
 *         CoreOptionDefinition {
 *             "audio_accuracy",
 *             "Audio accuracy",
 *             "If the emulator runs too slow, lower this setting, but note that audio glitches can
 *                 occur at settings other than \"high\". ",
 *             {
 *                 "low",
 *                 "medium",
 *                 "high",
 *             },
 *             "high"
 *         },
 *     },
 *     // The category definition ends here.
 *
 *     // Note how we omit the description here.
 *     CoreOptionDefinition {
 *         "frameskip",
 *         "Frame skipping",
 *         {
 *             { 0, "None" },
 *             { 1, "One frame" },
 *             { 2, "Two frames" },
 *             { 3, "Three frames" },
 *         },
 *         0
 *     },
 *
 *     // Note how we omit both a description as well as the values here. Read below on why
 *     // omitting values is sometimes useful.
 *     CoreOptionDefinition {
 *         "midi_device",
 *         "MIDI output device",
 *     }
 * };
 *
 * In the above example, we didn't speficy any values for the "midi_device" option. This allows for
 * options where the values need to be constructed at run-time. In this case, we would get the list
 * of available MIDI devices from the operating system and then add them as values with something
 * like:
 *
 *     vector<CoreOptionValue> midiValues;
 *     for (...) {
 *         midiValues.push_back({midi_device_id, label});
 *     }
 *     midiValues.push_back("none");
 *
 *     core_options.option("midi_device")->setValues(std::move(midiValues), "none");
 *
 * You MUST add values for any options originally defined without them, and you must do so BEFORE
 * you submit the options to the frontend. Otherwise, the behavior is undefined.
 *
 * After creating your core options, you must set the frontend environment callback with
 * setEnvironmentCallback() and submit the core options to the frontend with updateFrontend(). This
 * should happen as early as possible. retro_set_environment() is the best place to do it.
 *
 * To query the frontend for the current value of an option, use the [] operator:
 *
 *     bool overclock = core_options["overclock"].toBool();
 *     int frames_to_skip = core_options["frameskip"].toInt();
 *     string midi_device = core_options["midi_device"].toString();
 *
 * You can also change the current value of core options programmatically from within your core's
 * code. If the frontend supports the RETRO_ENVIRONMENT_SET_VARIABLE command, then that will be
 * used to change the current value. If the frontend does not support that command, then a trick
 * will be used to accomplish this. Well-behaved frontends that follow the spec to the letter should
 * work fine.
 *
 * To change the current value of an option, just use the setCurrentValue() function of your options
 * object. To change the "audio_accuracy" option to "low", and the "frameskip" option to 3 from the
 * above example, you would do:
 *
 *     core_options.setCurrentValue("audio_accuracy", "low");
 *     core_options.setCurrentValue("frameskip", 3);
 *
 * Note that the omission of the key prefix when using functions of this class is only a
 * convenience. The actual CoreOptionDefinition instances contain the full key. For example:
 *
 *     cout << options.option("audio_accuracy")->key();
 *
 * will print "core_name_audio_accuracy", not "audio_accuracy".
 */
class CoreOptions final
{
public:
    template <typename... Ts>
    CoreOptions(std::string key_prefix, Ts&&... Args);

    /* Set the frontend environment callback.
     */
    void setEnvironmentCallback(retro_environment_t cb);

    /* Query frontend for the current value of the option corresponding to the specified key.
     * Returns the default value of the option if the query fails. Returns an invalid value if 'key'
     * doesn't correspond to an option.
     */
    [[nodiscard]]
    auto operator[](std::string_view key) const -> const CoreOptionValue&;

    /* Returns true if any values were changed by the frontend since the last query.
     */
    [[nodiscard]]
    auto changed() const noexcept -> bool;

    /* Returns a pointer to the CoreOptionDefinition for the given key. Returns null if no option
     * with that key exists.
     */
    [[nodiscard]]
    auto option(std::string_view key) -> CoreOptionDefinition*;

    [[nodiscard]]
    auto option(std::string_view key) const -> const CoreOptionDefinition*;

    /* Submit the options to the frontend. Should be called as early as possible - ideally inside
     * retro_set_environment(), and no later than retro_load_game().
     */
    void updateFrontend();

    /* Show/hide the specified option(s).
     *
     * Returns whether or not the visibility of any of the options has changed. That is, trying to
     * show or hide options that are all already visible or hidden will return false. If any of the
     * options had their visiblity status changed, true is returned.
     */
    auto setVisible(std::string_view key, bool visible) noexcept -> bool;
    auto setVisible(std::initializer_list<const std::string_view> keys, bool visible) noexcept
        -> bool;
    auto setVisible(const std::regex& exp, bool visible) noexcept -> bool;

    /* Change the current value of the specified option. The function will attempt to make this work
     * (through a trick) even if the frontend does not support the RETRO_ENVIRONMENT_SET_VARIABLE
     * command.
     */
    void setCurrentValue(std::string_view key, const CoreOptionValue& value);

private:
    std::vector<std::variant<CoreOptionDefinition, CoreOptionCategory>> options_and_categories_;
    std::map<std::string, CoreOptionDefinition*, std::less<>> options_map_;
    std::vector<retro_core_option_v2_category> retro_categories_v2_;
    std::vector<retro_core_option_v2_definition> retro_options_v2_;
    std::vector<std::string> categorized_option_descriptions_;
    std::string key_prefix_;
    retro_environment_t env_cb_ = envCbFallback;
    CoreOptionValue invalid_value_{""};
    bool api_has_set_variable = false;

    void updateRetroOptions();
    void updateFrontendV0();
    void updateFrontendV1();

    static RETRO_CALLCONV auto envCbFallback(unsigned /*cmd*/, void* /*data*/) -> bool;
};

template <typename... Ts>
CoreOptions::CoreOptions(std::string key_prefix, Ts&&... Args)
    : options_and_categories_(std::vector<std::variant<CoreOptionDefinition, CoreOptionCategory>>{
        std::forward<std::variant<CoreOptionDefinition, CoreOptionCategory>>(Args)...})
    , key_prefix_(std::move(key_prefix))
{
    // TODO: check for duplicate option keys.
    for (auto& option_or_category : options_and_categories_) {
        if (std::holds_alternative<CoreOptionDefinition>(option_or_category)) {
            auto& option = std::get<CoreOptionDefinition>(option_or_category);
            options_map_[option.key()] = &option;
            option.setKey(key_prefix_ + option.key());
        } else {
            for (auto& option : std::get<CoreOptionCategory>(option_or_category).options()) {
                options_map_[option.key()] = &option;
                option.setKey(key_prefix_ + option.key());
            }
        }
    }
}

inline auto CoreOptions::option(const std::string_view key) -> CoreOptionDefinition*
{
    auto it = options_map_.find(key);
    return it != options_map_.end() ? it->second : nullptr;
}

inline auto CoreOptions::option(const std::string_view key) const -> const CoreOptionDefinition*
{
    auto it = options_map_.find(key);
    return it != options_map_.end() ? it->second : nullptr;
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
