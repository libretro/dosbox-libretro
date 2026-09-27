#include "libretro_gfx.h"
#include "dosbox.h"
#include "emu_thread.h"
#include "libretro-vkbd.h"
#include "libretro.h"
#include "libretro_dosbox.h"
#include "log.h"
#include "vga.h"
#include "video.h"
#include <algorithm>
#include <cstring>

namespace gfx {

std::array<std::vector<Bit8u>, 2> framebuffers;
std::vector<Bit8u>* frontbuffer = &framebuffers[0];
static std::vector<Bit8u>* backbuffer = &framebuffers[1];
bool frontbuffer_uploaded = false;
Bitu width;
Bitu height;
Bitu pitch;
float aspect_ratio = 0;
unsigned pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
static GFX_CallBack_t dosbox_cb = nullptr;
#ifdef WITH_PINHACK
bool request_VGA_SetupDrawing = false;
#endif

} // namespace gfx

auto GFX_GetBestMode(const Bitu /*flags*/) -> Bitu
{
    return GFX_CAN_32 | GFX_RGBONLY;
}

auto GFX_GetRGB(const Bit8u red, const Bit8u green, const Bit8u blue) -> Bitu
{
    return (red << 16) | (green << 8) | (blue << 0);
}

auto GFX_SetSize(
    const Bitu width, const Bitu height, const Bitu /*flags*/, const double scalex,
    const double scaley, const GFX_CallBack_t cb) -> Bitu
{
    for (auto& buf : gfx::framebuffers) {
        std::fill(buf.begin(), buf.end(), 0);
    }
    gfx::width = width;
    gfx::height = height;
    gfx::pitch = width * 4;
    gfx::aspect_ratio = (width * scalex) / (height * scaley);
    gfx::dosbox_cb = cb;

    if (gfx::width > gfx::max_width || gfx::height > gfx::max_height) {
        return 0;
    }

    const auto fb_size = gfx::width * gfx::height * 4;
    if (fb_size > gfx::framebuffers[0].size()) {
        retro::logDebug("Increasing max framebuffer size to {}x{}", gfx::width, gfx::height);
        for (auto& buf : gfx::framebuffers) {
            buf.resize(fb_size);
        }
    }
    switchThread(ThreadSwitchReason::VideoModeChange);
    return GFX_GetBestMode(0);
}

auto GFX_StartUpdate(Bit8u*& pixels, Bitu& pitch) -> bool
{
    pixels = run_synced ? gfx::framebuffers[0].data() : gfx::backbuffer->data();
    pitch = gfx::pitch;
    return true;
}

void GFX_EndUpdate(const Bit16u* const changedLines)
{
    if (retro_vkbd) {
        gfx::frontbuffer_uploaded = false;
        gfx::dosbox_cb(GFX_CallBackRedraw);

        if (!run_synced) {
            std::swap(gfx::frontbuffer, gfx::backbuffer);
        }
        return;
    }

#ifdef WITH_PINHACK
    if (gfx::request_VGA_SetupDrawing) {
        gfx::request_VGA_SetupDrawing = false;
        VGA_SetupDrawing(0);
    }
#endif

    if (run_synced) {
        gfx::frontbuffer_uploaded = !changedLines;
    } else if (gfx::frontbuffer_uploaded && changedLines) {
        std::swap(gfx::frontbuffer, gfx::backbuffer);
        gfx::frontbuffer_uploaded = false;
        // Tell dosbox to draw the next frame completely, not just the scanlines that changed.
        gfx::dosbox_cb(GFX_CallBackRedraw);
    }
}

// Stubs
void GFX_SetTitle(Bit32s /*cycles*/, int /*frameskip*/, bool /*paused*/)
{ }

void GFX_Events()
{ }

void GFX_SetPalette(Bitu /*start*/, Bitu /*count*/, GFX_PalEntry* /*entries*/)
{ }

auto GFX_LazyFullscreenRequested() -> bool
{
    return false;
}

void GFX_TearDown()
{ }

auto GFX_IsFullscreen() -> bool
{
    return false;
}

void GFX_UpdateSDLCaptureState()
{ }

void GFX_RestoreMode()
{ }

void GFX_SwitchLazyFullscreen(bool /*lazy*/)
{ }

void GFX_SwitchFullscreenNoReset()
{ }

void GFX_SetShader(const char* /*src*/)
{ }
