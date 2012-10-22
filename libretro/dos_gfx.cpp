#include <string.h>

#include "libretro.h"

#include "dosbox.h"
#include "video.h"

extern retro_video_refresh_t video_cb;
extern void retro_handle_dos_events();

// GFX
static Bit8u screenBuffer[1024*768*4];
static Bitu screenWidth, screenHeight;
static unsigned screenColorMode = RETRO_PIXEL_FORMAT_0RGB1555;

void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries)
{
    // Nothing
}

Bitu GFX_GetBestMode(Bitu flags)
{
    switch(screenColorMode)
    {
        case RETRO_PIXEL_FORMAT_0RGB1555: return GFX_CAN_15 | GFX_RGBONLY;
        case RETRO_PIXEL_FORMAT_XRGB8888: return GFX_CAN_32 | GFX_RGBONLY;
        case RETRO_PIXEL_FORMAT_RGB565: return GFX_CAN_16 | GFX_RGBONLY;
    }
    
    return 0;
}

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue)
{
    switch(screenColorMode)
    {
        case RETRO_PIXEL_FORMAT_0RGB1555: return ((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3);
        case RETRO_PIXEL_FORMAT_XRGB8888: return (red << 16) | (green << 8) | (blue);
        case RETRO_PIXEL_FORMAT_RGB565: return ((red >> 3) << 11) | ((green >> 3) << 6) | (blue >> 3);
    }
    
    return 0xFFFFFF00;
}

Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley,GFX_CallBack_t cb)
{
    memset(screenBuffer, 0, sizeof(screenBuffer));
    
    screenWidth = width;
    screenHeight = height;
    
    return GFX_GetBestMode(0);
}

bool GFX_StartUpdate(Bit8u * & pixels,Bitu & pitch)
{
    pixels = screenBuffer;
    pitch = screenWidth * ((RETRO_PIXEL_FORMAT_XRGB8888 == screenColorMode) ? 4 : 2);
    
    return true;
}

void GFX_EndUpdate( const Bit16u *changedLines )
{
    if(screenWidth && screenHeight)
    {
        video_cb(screenBuffer, screenWidth, screenHeight, screenWidth * ((RETRO_PIXEL_FORMAT_XRGB8888 == screenColorMode) ? 4 : 2));
    }
}

void GFX_SetTitle(Bit32s cycles,Bits frameskip,bool paused)
{
    // Nothing
}


void GFX_ShowMsg(char const* format,...)
{
    // Nothing
}

void GFX_Events()
{
    retro_handle_dos_events();
}

void GFX_GetSize(int &width, int &height, bool &fullscreen)
{
	width = screenWidth;
	height = screenHeight;
	fullscreen = false;
}
