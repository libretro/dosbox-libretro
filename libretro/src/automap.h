// Automatic gamepad mappings from DOSBox Pure's keyb2joypad database.
#pragma once
#include <string>
#include <vector>

struct AutoMapBind {
    unsigned button;          // RETRO_DEVICE_ID_JOYPAD_*, or 16-19 for the stick axes
    bool analog;              // stick axis: keys come in pairs, negative then positive
    std::vector<unsigned char> keys; // KBD_KEYS
    std::string name;         // what it does in the game, when the database says
};

// DOSBox Pure's special mapping codes that are played here, in place of a key.
enum : unsigned char {
    AUTOMAP_MOUSE_UP = 200, AUTOMAP_MOUSE_DOWN, AUTOMAP_MOUSE_LEFT, AUTOMAP_MOUSE_RIGHT,
    AUTOMAP_MOUSE_LEFT_CLICK, AUTOMAP_MOUSE_RIGHT_CLICK, AUTOMAP_MOUSE_MIDDLE,
    AUTOMAP_JOY_UP = 209, AUTOMAP_JOY_DOWN, AUTOMAP_JOY_LEFT, AUTOMAP_JOY_RIGHT,
    AUTOMAP_JOY_BUTTON1, AUTOMAP_JOY_BUTTON2, AUTOMAP_JOY_BUTTON3, AUTOMAP_JOY_BUTTON4,
    AUTOMAP_JOY2_UP = 221, AUTOMAP_JOY2_DOWN, AUTOMAP_JOY2_LEFT, AUTOMAP_JOY2_RIGHT,
};
bool automap_is_playable(unsigned char code);
// Whether the mapping drives the DOS joystick, which then stays enabled.
bool automap_uses_joystick();

// Looks through the mounted drives for a game the database knows. Runs in
// DOS (from the autoexec), after the content and its images are mounted. The
// content's name settles a tie between games found on the same disc.
void automap_detect(const std::string& content_name, unsigned dir_visit_limit = 0xFFFFFFFFu);
// The bindings found, for the first gamepad port, and the game's title.
const std::vector<AutoMapBind>& automap_binds();
const std::string& automap_title();
// The year the database gives the game, or 0.
int automap_year();
// Set by automap_detect when something was found and not yet applied.
bool automap_take_pending();
void automap_reset();
