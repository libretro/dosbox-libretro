// Automatic gamepad mappings, after DOSBox Pure: when the content is a game
// the keyb2joypad database knows - recognised by the names and sizes of its
// files - the first gamepad presses the game's keys instead of driving the
// DOS joystick. The database and its layout are DOSBox Pure's (keyb2joypad.cpp;
// original data from the Keyb2Joypad Project by Jemy Murphy and bigjim, used by
// DOSBox Pure with permission); the lookup and the decoding follow its
// init_dosbox_parse_drives and BindDecoder.

#include "automap.h"
#include "keyb2joypad.h"
#include "dosbox.h"
#include "dos_inc.h"
#include "../../src/dos/drives.h"
#include "keyboard.h"
#include <cstring>

static std::vector<AutoMapBind> binds;
static std::string title;
static int year = 0;
static bool pending = false;

enum { WHEEL_ID = 20 };

bool automap_is_playable(const unsigned char code)
{
    // Keys, and the mouse and joystick actions of DOSBox Pure's special
    // mappings (200 and up) this core can play: mouse movement and buttons,
    // and joystick axes and buttons. Not its mouse speed, hat, menu or port
    // shifting entries.
    return (code > KBD_NONE && code < KBD_LAST) || (code >= AUTOMAP_MOUSE_UP && code <= AUTOMAP_MOUSE_MIDDLE)
        || (code >= AUTOMAP_JOY_UP && code <= AUTOMAP_JOY_BUTTON4)
        || (code >= AUTOMAP_JOY2_UP && code <= AUTOMAP_JOY2_RIGHT);
}

bool automap_uses_joystick()
{
    for (const auto& bind : binds) {
        for (const auto k : bind.keys) {
            if (k >= AUTOMAP_JOY_UP && k <= AUTOMAP_JOY2_RIGHT) {
                return true;
            }
        }
    }
    return false;
}

static void decode(const Bit8u* p, const char* names)
{
    binds.clear();
    int remain = *(p++);
    while (remain-- > 0) {
        const Bit8u v = *(p++);
        AutoMapBind bind;
        const int key_count = 1 + (v >> 6);
        bind.button = v & 31;
        bind.analog = (bind.button >> 2) == 4; // 16 - 19
        if (v & 32) {
            Bit32u offset = 0;
            do {
                offset = (offset << 7) | (*p & 127);
            } while (*(p++) & 128);
            if (names) {
                bind.name = names + offset;
            }
        }
        for (int i = 0; i != key_count * (bind.analog ? 2 : 1); ++i) {
            bind.keys.push_back(p[i]);
        }
        p += key_count * (bind.analog ? 2 : 1);

        bool usable = bind.button < WHEEL_ID;
        for (const auto k : bind.keys) {
            usable = usable && automap_is_playable(k);
        }
        if (usable) {
            binds.push_back(std::move(bind));
        }
    }
}

// Every database entry a file matches, with how many files matched it: a CD
// often carries other games' shareware next to its own (Duke Nukem 3D's has
// Wacky Wheels in its goodies), so the entry most files point at wins.
struct Lookup {
    std::vector<std::pair<Bit32u, int>> hits; // database index, matching files
};

static void file_iter(const char* path, bool is_dir, Bit32u size, Bit16u, Bit16u, Bit8u, Bitu data)
{
    auto& lookup = *reinterpret_cast<Lookup*>(data);
    if (is_dir) {
        return;
    }
    const char* const lastslash = strrchr(path, '\\');
    const char* const fname = lastslash ? lastslash + 1 : path;
    Bit32u hash = 0x811c9dc5;
    for (const char* p = fname; *p; p++) {
        hash = ((hash * 0x01000193) ^ (Bit8u)*p);
    }
    hash ^= (size << 3);

    for (Bit32u idx = hash;; idx++) {
        if (!map_keys[idx %= MAP_TABLE_SIZE]) {
            break;
        }
        if (map_keys[idx] != hash) {
            continue;
        }
        for (auto& hit : lookup.hits) {
            if (hit.first == idx) {
                ++hit.second;
                return;
            }
        }
        lookup.hits.emplace_back(idx, 1);
        return;
    }
}

// Decodes the database entry at idx into the bindings and the title.
static void load_entry(const Bit32u idx)
{
    const MAPBucket& idents_bk = map_buckets[idx % MAP_BUCKETS];
    std::vector<Bit8u> idents(idents_bk.idents_size_uncompressed);
    zipDrive::Uncompress(idents_bk.idents_compressed, idents_bk.idents_size_compressed,
        idents.data(), idents_bk.idents_size_uncompressed);
    const Bit8u* ident = idents.data() + (idx / MAP_BUCKETS) * 5;
    const MAPBucket& mappings_bk = map_buckets[ident[0] % MAP_BUCKETS];
    const Bit16u map_offset = (ident[1] << 8) + ident[2];
    const char* map_title =
        (char*)idents.data() + (MAP_TABLE_SIZE / MAP_BUCKETS) * 5 + (ident[3] << 8) + ident[4];
    title = map_title + 1;
    year = 1970 + (Bit8u)map_title[0];

    static std::vector<Bit8u> mappings;
    mappings.resize(mappings_bk.mappings_size_uncompressed);
    zipDrive::Uncompress(mappings_bk.mappings_compressed, mappings_bk.mappings_size_compressed,
        mappings.data(), mappings_bk.mappings_size_uncompressed);
    decode(mappings.data() + map_offset,
        (const char*)mappings.data() + mappings_bk.mappings_action_offset);
}

static bool contains_nocase(const std::string& haystack, const std::string& needle)
{
    if (needle.empty() || needle.size() > haystack.size()) {
        return false;
    }
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        if (!strncasecmp(haystack.c_str() + i, needle.c_str(), needle.size())) {
            return true;
        }
    }
    return false;
}

void automap_detect(const std::string& content_name, const unsigned dir_visit_limit)
{
    Lookup lookup;
    for (const char letter : {'C', 'D', 'E', 'A'}) {
        if (Drives[letter - 'A']) {
            DriveFileIterator(Drives[letter - 'A'], file_iter, reinterpret_cast<Bitu>(&lookup),
                dir_visit_limit);
        }
    }
    binds.clear();
    title.clear();
    // Most matching files first; between entries as good as each other - a CD
    // with several games, each recognised by one file - the one whose title is
    // in the content's name, the longest such title (Duke Nukem 3D rather
    // than Duke Nukem), and otherwise the first found.
    const std::pair<Bit32u, int>* best = nullptr;
    size_t best_title_len = 0;
    for (const auto& hit : lookup.hits) {
        load_entry(hit.first);
        const size_t title_len = contains_nocase(content_name, title) ? title.size() : 0;
        if (!best || hit.second > best->second
            || (hit.second == best->second && title_len > best_title_len))
        {
            best = &hit;
            best_title_len = title_len;
        }
    }
    if (best) {
        load_entry(best->first);
    }
    if (!binds.empty()) {
        pending = true;
    } else {
        title.clear();
    }
}

const std::vector<AutoMapBind>& automap_binds()
{
    return binds;
}

const std::string& automap_title()
{
    return title;
}

int automap_year()
{
    return binds.empty() ? 0 : year;
}

bool automap_take_pending()
{
    const bool was = pending;
    pending = false;
    return was;
}

void automap_reset()
{
    binds.clear();
    title.clear();
    pending = false;
}
