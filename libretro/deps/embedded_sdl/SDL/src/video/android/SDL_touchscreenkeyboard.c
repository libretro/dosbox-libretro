/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include <jni.h>
#include <android/log.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <string.h> // for memset()
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <netinet/in.h>

#include "SDL_config.h"

#include "SDL_version.h"

//#include "SDL_opengles.h"
#include "SDL_screenkeyboard.h"
#include "../SDL_sysvideo.h"
#include "SDL_androidvideo.h"
#include "SDL_androidinput.h"
#include "jniwrapperstuff.h"

// #include "touchscreentheme.h" // Not used yet

// TODO: this code is a HUGE MESS

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

enum { MAX_BUTTONS = SDL_ANDROID_SCREENKEYBOARD_BUTTON_NUM-1, MAX_BUTTONS_AUTOFIRE = 2, BUTTON_TEXT_INPUT = SDL_ANDROID_SCREENKEYBOARD_BUTTON_TEXT, BUTTON_ARROWS = MAX_BUTTONS } ; // Max amount of custom buttons

int SDL_ANDROID_isTouchscreenKeyboardUsed = 0;
static short touchscreenKeyboardTheme = 0;
static short touchscreenKeyboardShown = 1;
static short AutoFireButtonsNum = 0;
static short buttonsize = 1;
static short buttonDrawSize = 1;
static short transparency = 128;

static SDL_Rect arrows, arrowsExtended, buttons[MAX_BUTTONS], buttonsAutoFireRect[MAX_BUTTONS_AUTOFIRE];
static SDL_Rect arrowsDraw, buttonsDraw[MAX_BUTTONS];
static SDLKey buttonKeysyms[MAX_BUTTONS] = { 
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_0)),
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_1)),
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_2)),
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_3)),
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_4)),
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_5)),
0
};

enum { ARROW_LEFT = 1, ARROW_RIGHT = 2, ARROW_UP = 4, ARROW_DOWN = 8 };
static short oldArrows = 0;
static short ButtonAutoFire[MAX_BUTTONS_AUTOFIRE];
static short ButtonAutoFireX[MAX_BUTTONS_AUTOFIRE*2];
static short ButtonAutoFireRot[MAX_BUTTONS_AUTOFIRE];
static short ButtonAutoFireDecay[MAX_BUTTONS_AUTOFIRE];

static short pointerInButtonRect[MAX_BUTTONS + 1];
static short buttonsGenerateSdlEvents[MAX_BUTTONS + 1];

typedef struct
{
    GLuint id;
    GLfloat w;
    GLfloat h;
} GLTexture_t;

static GLTexture_t arrowImages[5];
static GLTexture_t buttonAutoFireImages[MAX_BUTTONS_AUTOFIRE*2];
static GLTexture_t buttonImages[MAX_BUTTONS*2];
static GLTexture_t mousePointer;
enum { MOUSE_POINTER_W = 32, MOUSE_POINTER_H = 32, MOUSE_POINTER_X = 5, MOUSE_POINTER_Y = 7 }; // X and Y are offsets of the pointer tip

static int sunTheme = 0;
static int joystickTouchPoints[2];

static inline int InsideRect(const SDL_Rect * r, int x, int y)
{
	return ( x >= r->x && x <= r->x + r->w ) && ( y >= r->y && y <= r->y + r->h );
}

static struct ScreenKbGlState_t
{
	GLboolean texture2d;
	GLuint texunitId;
	GLuint clientTexunitId;
	GLuint textureId;
	GLfloat color[4];
	GLint texEnvMode;
	GLboolean blend;
	GLenum blend1, blend2;
	GLint texFilter1, texFilter2;
	GLboolean colorArray;
}
oldGlState;

static inline void beginDrawingTex()
{
	// Save OpenGL state
	glGetError(); // Clear error flag

	// This code does not work on 1.6 emulator, and on some older devices
	// However GLES 1.1 spec defines all theese values, so it's a device fault for not implementing them
        // despite that ...
	const char* vers = glGetString(GL_VERSION);
	if (vers != NULL && strlen(vers) > 3 && strcmp((vers+strlen(vers)-3),"1.0") == 0 )
	{
		oldGlState.texture2d = GL_FALSE; //glIsEnabled(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_2D);
		oldGlState.blend = GL_FALSE; //glIsEnabled(GL_BLEND);
		glEnable(GL_BLEND);
	}
	else
	{
		oldGlState.texture2d = glIsEnabled(GL_TEXTURE_2D);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &oldGlState.texunitId);
		glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &oldGlState.clientTexunitId);
		glActiveTexture(GL_TEXTURE0);
		glClientActiveTexture(GL_TEXTURE0);

		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldGlState.textureId);
		glGetFloatv(GL_CURRENT_COLOR, &(oldGlState.color[0]));
		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &oldGlState.texEnvMode);
		oldGlState.blend = glIsEnabled(GL_BLEND);
		glGetIntegerv(GL_BLEND_SRC, &oldGlState.blend1);
		glGetIntegerv(GL_BLEND_DST, &oldGlState.blend2);
		glGetBooleanv(GL_COLOR_ARRAY, &oldGlState.colorArray);

		// It's very unlikely that some app will use GL_TEXTURE_CROP_RECT_OES, so just skip it
		if( glGetError() != GL_NO_ERROR )
		{
			// Make the video somehow work on emulator
			oldGlState.texture2d = GL_FALSE;
			oldGlState.texunitId = GL_TEXTURE0;
			oldGlState.clientTexunitId = GL_TEXTURE0;
			oldGlState.textureId = 0;
			oldGlState.texEnvMode = GL_MODULATE;
			oldGlState.blend = GL_FALSE;
			oldGlState.blend1 = GL_SRC_ALPHA;
			oldGlState.blend2 = GL_ONE_MINUS_SRC_ALPHA;
			oldGlState.colorArray = GL_FALSE;
		}

		glEnable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

static inline void endDrawingTex()
{
	const char* vers = glGetString(GL_VERSION);
	// Restore OpenGL state
	if( oldGlState.texture2d == GL_FALSE )
		glDisable(GL_TEXTURE_2D);
	if( oldGlState.blend == GL_FALSE )
		glDisable(GL_BLEND);

	if (vers != NULL && strlen(vers) > 3 && strcmp((vers+strlen(vers)-3),"1.0") != 0)
	{
		glBindTexture(GL_TEXTURE_2D, oldGlState.textureId);
		glColor4f(oldGlState.color[0], oldGlState.color[1], oldGlState.color[2], oldGlState.color[3]);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oldGlState.texEnvMode);
		glBlendFunc(oldGlState.blend1, oldGlState.blend2);
		glActiveTexture(oldGlState.texunitId);
		glClientActiveTexture(oldGlState.clientTexunitId);
		if( oldGlState.colorArray )
		        glEnableClientState(GL_COLOR_ARRAY);
	}
}

static inline void drawCharTexFlip(GLTexture_t * tex, SDL_Rect * src, SDL_Rect * dest, int flipX, int flipY, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	GLint cropRect[4];

	if( !dest->h || !dest->w )
		return;

	glBindTexture(GL_TEXTURE_2D, tex->id);

	glColor4x(r * 0x100, g * 0x100, b * 0x100,  a * 0x100 );

	if(src)
	{
		cropRect[0] = src->x;
		cropRect[1] = tex->h - src->y;
		cropRect[2] = src->w;
		cropRect[3] = -src->h;
	}
	else
	{
		cropRect[0] = 0;
		cropRect[1] = tex->h;
		cropRect[2] = tex->w;
		cropRect[3] = -tex->h;
	}
	if(flipX)
	{
		cropRect[0] += cropRect[2];
		cropRect[2] = -cropRect[2];
	}
	if(flipY)
	{
		cropRect[1] += cropRect[3];
		cropRect[3] = -cropRect[3];
	}
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, cropRect);
	glDrawTexiOES(dest->x, SDL_ANDROID_sWindowHeight - dest->y - dest->h, 0, dest->w, dest->h);
}

static inline void drawCharTex(GLTexture_t * tex, SDL_Rect * src, SDL_Rect * dest, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	drawCharTexFlip(tex, src, dest, 0, 0, r, g, b, a);
}

static void drawTouchscreenKeyboardLegacy()
{
	int i;
	int blendFactor;

	blendFactor =		( SDL_GetKeyboardState(NULL)[SDL_KEY(LEFT)] ? 1 : 0 ) +
						( SDL_GetKeyboardState(NULL)[SDL_KEY(RIGHT)] ? 1 : 0 ) +
						( SDL_GetKeyboardState(NULL)[SDL_KEY(UP)] ? 1 : 0 ) +
						( SDL_GetKeyboardState(NULL)[SDL_KEY(DOWN)] ? 1 : 0 );
	if( blendFactor == 0 )
		drawCharTex( &arrowImages[0], NULL, &arrowsDraw, 255, 255, 255, transparency );
	else
	{
		if( SDL_GetKeyboardState(NULL)[SDL_KEY(LEFT)] )
			drawCharTex( &arrowImages[1], NULL, &arrowsDraw, 255, 255, 255, transparency / blendFactor );
		if( SDL_GetKeyboardState(NULL)[SDL_KEY(RIGHT)] )
			drawCharTex( &arrowImages[2], NULL, &arrowsDraw, 255, 255, 255, transparency / blendFactor );
		if( SDL_GetKeyboardState(NULL)[SDL_KEY(UP)] )
			drawCharTex( &arrowImages[3], NULL, &arrowsDraw, 255, 255, 255, transparency / blendFactor );
		if( SDL_GetKeyboardState(NULL)[SDL_KEY(DOWN)] )
			drawCharTex( &arrowImages[4], NULL, &arrowsDraw, 255, 255, 255, transparency / blendFactor );
	}

	for( i = 0; i < MAX_BUTTONS; i++ )
	{
		if( ! buttons[i].h || ! buttons[i].w )
			continue;
		if( i < AutoFireButtonsNum )
		{
			if( ButtonAutoFire[i] == 1 && SDL_GetTicks() - ButtonAutoFireDecay[i] > 1000 )
			{
				ButtonAutoFire[i] = 0;
			}
			if( ! ButtonAutoFire[i] && SDL_GetTicks() - ButtonAutoFireDecay[i] > 300 )
			{
				if( ButtonAutoFireX[i*2] > 0 )
					ButtonAutoFireX[i*2] --;
				if( ButtonAutoFireX[i*2+1] > 0 )
					ButtonAutoFireX[i*2+1] --;
				ButtonAutoFireDecay[i] = SDL_GetTicks();
			}
		}

		if( i < AutoFireButtonsNum && ! ButtonAutoFire[i] && 
			( ButtonAutoFireX[i*2] > 0 || ButtonAutoFireX[i*2+1] > 0 ) )
		{
			int pos1src = buttonImages[i*2+1].w / 2 - ButtonAutoFireX[i*2];
			int pos1dst = buttonsDraw[i].w * pos1src / buttonImages[i*2+1].w;
			int pos2src = buttonImages[i*2+1].w - ( buttonImages[i*2+1].w / 2 - ButtonAutoFireX[i*2+1] );
			int pos2dst = buttonsDraw[i].w * pos2src / buttonImages[i*2+1].w;
			
			SDL_Rect autoFireCrop = { 0, 0, pos1src, buttonImages[i*2+1].h };
			SDL_Rect autoFireDest = buttonsDraw[i];
			autoFireDest.w = pos1dst;
			
			drawCharTex( &buttonImages[i*2+1],
						&autoFireCrop, &autoFireDest, 255, 255, 255, transparency );

			autoFireCrop.x = pos2src;
			autoFireCrop.w = buttonImages[i*2+1].w - pos2src;
			autoFireDest.x = buttonsDraw[i].x + pos2dst;
			autoFireDest.w = buttonsDraw[i].w - pos2dst;

			drawCharTex( &buttonImages[i*2+1],
						&autoFireCrop, &autoFireDest, 255, 255, 255, transparency );
			
			autoFireCrop.x = pos1src;
			autoFireCrop.w = pos2src - pos1src;
			autoFireDest.x = buttonsDraw[i].x + pos1dst;
			autoFireDest.w = pos2dst - pos1dst;

			drawCharTex( &buttonAutoFireImages[i*2+1],
						&autoFireCrop, &autoFireDest, 255, 255, 255, transparency );
		}
		else
		{
			drawCharTex( ( i < AutoFireButtonsNum && ButtonAutoFire[i] ) ? &buttonAutoFireImages[i*2] :
						&buttonImages[ SDL_GetKeyboardState(NULL)[buttonKeysyms[i]] ? (i * 2 + 1) : (i * 2) ],
						NULL, &buttonsDraw[i], 255, 255, 255, transparency );
		}
	}
}

static void drawTouchscreenKeyboardSun()
{
	int i;

	drawCharTex( &arrowImages[0], NULL, &arrowsDraw, 255, 255, 255, transparency );
	if(pointerInButtonRect[BUTTON_ARROWS] != -1)
	{
		SDL_Rect touch = arrowsDraw;
		touch.w /= 2;
		touch.h /= 2;
		touch.x = joystickTouchPoints[0] - touch.w / 2;
		touch.y = joystickTouchPoints[1] - touch.h / 2;
		drawCharTex( &arrowImages[0], NULL, &touch, 255, 255, 255, transparency );
	}

	for( i = 0; i < MAX_BUTTONS; i++ )
	{
		int pressed = SDL_GetKeyboardState(NULL)[buttonKeysyms[i]];
		if( ! buttons[i].h || ! buttons[i].w )
			continue;
		if( i < AutoFireButtonsNum )
		{
			if( ButtonAutoFire[i] == 1 && SDL_GetTicks() - ButtonAutoFireDecay[i] > 1000 )
			{
				ButtonAutoFire[i] = 0;
			}
			if( ! ButtonAutoFire[i] && SDL_GetTicks() - ButtonAutoFireDecay[i] > 300 )
			{
				if( ButtonAutoFireX[i*2] > 0 )
					ButtonAutoFireX[i*2] --;
				if( ButtonAutoFireX[i*2+1] > 0 )
					ButtonAutoFireX[i*2+1] --;
				ButtonAutoFireDecay[i] = SDL_GetTicks();
			}
		}

		if( i < AutoFireButtonsNum && ButtonAutoFire[i] )
			drawCharTex( &buttonAutoFireImages[i*2+1],
						NULL, &buttonsDraw[i], 255, 255, 255, transparency );

		drawCharTexFlip( &buttonImages[ pressed ? (i * 2 + 1) : (i * 2) ],
						NULL, &buttonsDraw[i], (i >= 2 && pressed), 0, 255, 255, 255, transparency );

		if( i < AutoFireButtonsNum && ! ButtonAutoFire[i] &&
			( ButtonAutoFireX[i*2] > 0 || ButtonAutoFireX[i*2+1] > 0 ) )
		{
			int pos1src = buttonImages[i*2+1].w / 2 - ButtonAutoFireX[i*2];
			int pos1dst = buttonsDraw[i].w * pos1src / buttonImages[i*2+1].w;
			int pos2src = buttonImages[i*2+1].w - ( buttonImages[i*2+1].w / 2 - ButtonAutoFireX[i*2+1] );
			int pos2dst = buttonsDraw[i].w * pos2src / buttonImages[i*2+1].w;
			SDL_Rect autoFireDest;

			autoFireDest.w = pos2dst - pos1dst;
			autoFireDest.h = pos2dst - pos1dst;
			autoFireDest.x = buttonsDraw[i].x + buttonsDraw[i].w/2 - autoFireDest.w/2;
			autoFireDest.y = buttonsDraw[i].y + buttonsDraw[i].h/2 - autoFireDest.h/2;

			drawCharTex( &buttonAutoFireImages[i*2],
						NULL, &autoFireDest, 255, 255, 255, transparency );
		}
	}
}

int SDL_ANDROID_drawTouchscreenKeyboard()
{
	if( !SDL_ANDROID_isTouchscreenKeyboardUsed || !touchscreenKeyboardShown )
		return 0;

	beginDrawingTex();

	if(sunTheme)
		drawTouchscreenKeyboardSun();
	else
		drawTouchscreenKeyboardLegacy();

	endDrawingTex();

	return 1;
};

static inline int ArrowKeysPressed(int x, int y)
{
	int ret = 0, dx, dy;
	dx = x - arrows.x - arrows.w / 2;
	dy = y - arrows.y - arrows.h / 2;
	// Single arrow key pressed
	if( abs(dy / 2) >= abs(dx) )
	{
		if( dy < 0 )
			ret |= ARROW_UP;
		else
			ret |= ARROW_DOWN;
	}
	else
	if( abs(dx / 2) >= abs(dy) )
	{
		if( dx > 0 )
			ret |= ARROW_RIGHT;
		else
			ret |= ARROW_LEFT;
	}
	else // Two arrow keys pressed
	{
		if( dx > 0 )
			ret |= ARROW_RIGHT;
		else
			ret |= ARROW_LEFT;

		if( dy < 0 )
			ret |= ARROW_UP;
		else
			ret |= ARROW_DOWN;
	}
	return ret;
}

unsigned SDL_ANDROID_processTouchscreenKeyboard(int x, int y, int action, int pointerId)
{
	int i;
	unsigned processed = 0;
	
	if( !touchscreenKeyboardShown )
		return 0;
	
	if( action == MOUSE_DOWN )
	{
		//__android_log_print(ANDROID_LOG_INFO, "libSDL", "touch %03dx%03d ptr %d action %d", x, y, pointerId, action);
		if( InsideRect( &arrows, x, y ) )
		{
			processed |= 1<<BUTTON_ARROWS;
			if( pointerInButtonRect[BUTTON_ARROWS] == -1 )
			{
				pointerInButtonRect[BUTTON_ARROWS] = pointerId;
				joystickTouchPoints[0] = x;
				joystickTouchPoints[1] = y;
				if( SDL_ANDROID_isJoystickUsed )
				{
					int xx = (x - arrows.x - arrows.w / 2) * 65534 / arrows.w;
					if( xx == 0 ) // Do not allow (0,0) coordinate, when the user touches the joystick
						xx = 1;
					SDL_ANDROID_MainThreadPushJoystickAxis(0, 0, xx );
					SDL_ANDROID_MainThreadPushJoystickAxis(0, 1, (y - arrows.y - arrows.h / 2) * 65534 / arrows.h );
				}
				else
				{
					i = ArrowKeysPressed(x, y);
					if( i & ARROW_UP )
						SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, SDL_KEY(UP), 0 );
					if( i & ARROW_DOWN )
						SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, SDL_KEY(DOWN), 0 );
					if( i & ARROW_LEFT )
						SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, SDL_KEY(LEFT), 0 );
					if( i & ARROW_RIGHT )
						SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, SDL_KEY(RIGHT), 0 );
					oldArrows = i;
				}
			}
		}

		for( i = 0; i < MAX_BUTTONS; i++ )
		{
			if( ! buttons[i].h || ! buttons[i].w )
				continue;
			if( InsideRect( &buttons[i], x, y) )
			{
				processed |= 1<<i;
				if( pointerInButtonRect[i] == -1 )
				{
					pointerInButtonRect[i] = pointerId;
					if( i == BUTTON_TEXT_INPUT )
						SDL_ANDROID_ToggleScreenKeyboardTextInput(NULL);
					else
						SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, buttonKeysyms[i], 0 );
					if( i < AutoFireButtonsNum )
					{
						ButtonAutoFire[i] = 0;
						ButtonAutoFireX[i*2] = 0;
						ButtonAutoFireX[i*2+1] = 0;
						ButtonAutoFireRot[i] = x;
						ButtonAutoFireDecay[i] = SDL_GetTicks();
					}
				}
			}
		}
	}
	else
	if( action == MOUSE_UP )
	{
		//__android_log_print(ANDROID_LOG_INFO, "libSDL", "touch %03dx%03d ptr %d action %d", x, y, pointerId, action);
		if( pointerInButtonRect[BUTTON_ARROWS] == pointerId )
		{
			processed |= 1<<BUTTON_ARROWS;
			pointerInButtonRect[BUTTON_ARROWS] = -1;
			if( SDL_ANDROID_isJoystickUsed )
			{
				SDL_ANDROID_MainThreadPushJoystickAxis(0, 0, 0 );
				SDL_ANDROID_MainThreadPushJoystickAxis(0, 1, 0 );
			}
			else
			{
				SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(UP), 0 );
				SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(DOWN), 0 );
				SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(LEFT), 0 );
				SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(RIGHT), 0 );
				oldArrows = 0;
			}
		}
		for( i = 0; i < MAX_BUTTONS; i++ )
		{
			if( ! buttons[i].h || ! buttons[i].w )
				continue;
			if( pointerInButtonRect[i] == pointerId )
			{
				processed |= 1<<i;
				pointerInButtonRect[i] = -1;
				if( i < AutoFireButtonsNum && ButtonAutoFire[i] )
				{
					ButtonAutoFire[i] = 2;
				}
				else
				{
					if( i != BUTTON_TEXT_INPUT )
						SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, buttonKeysyms[i], 0 );
				}
				if( i < AutoFireButtonsNum )
				{
					ButtonAutoFireX[i*2] = 0;
					ButtonAutoFireX[i*2+1] = 0;
				}
			}
		}
	}
	else
	if( action == MOUSE_MOVE )
	{
		// Process cases when pointer enters button area (it won't send keypress twice if button already pressed)
		processed |= SDL_ANDROID_processTouchscreenKeyboard(x, y, MOUSE_DOWN, pointerId);
		
		// Process cases when pointer leaves button area
		// TODO: huge code size, split it or somehow make it more readable
		if( pointerInButtonRect[BUTTON_ARROWS] == pointerId )
		{
			processed |= 1<<BUTTON_ARROWS;
			if( ! InsideRect( &arrowsExtended, x, y ) )
			{
				pointerInButtonRect[BUTTON_ARROWS] = -1;
				if( SDL_ANDROID_isJoystickUsed )
				{
					SDL_ANDROID_MainThreadPushJoystickAxis(0, 0, 0 );
					SDL_ANDROID_MainThreadPushJoystickAxis(0, 1, 0 );
				}
				else
				{
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(UP), 0 );
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(DOWN), 0 );
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(LEFT), 0 );
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(RIGHT), 0 );
					oldArrows = 0;
				}
			}
			else
			{
				joystickTouchPoints[0] = x;
				joystickTouchPoints[1] = y;
				if( SDL_ANDROID_isJoystickUsed )
				{
					SDL_ANDROID_MainThreadPushJoystickAxis(0, 0, (x - arrows.x - arrows.w / 2) * 65534 / arrows.w );
					SDL_ANDROID_MainThreadPushJoystickAxis(0, 1, (y - arrows.y - arrows.h / 2) * 65534 / arrows.h );
				}
				else
				{
					i = ArrowKeysPressed(x, y);
					if( i != oldArrows )
					{
						if( oldArrows & ARROW_UP && ! (i & ARROW_UP) )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(UP), 0 );
						if( oldArrows & ARROW_DOWN && ! (i & ARROW_DOWN) )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(DOWN), 0 );
						if( oldArrows & ARROW_LEFT && ! (i & ARROW_LEFT) )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(LEFT), 0 );
						if( oldArrows & ARROW_RIGHT && ! (i & ARROW_RIGHT) )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, SDL_KEY(RIGHT), 0 );
						if( i & ARROW_UP )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, SDL_KEY(UP), 0 );
						if( i & ARROW_DOWN )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, SDL_KEY(DOWN), 0 );
						if( i & ARROW_LEFT )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, SDL_KEY(LEFT), 0 );
						if( i & ARROW_RIGHT )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, SDL_KEY(RIGHT), 0 );
					}
					oldArrows = i;
				}
			}
		}
		for( i = 0; i < AutoFireButtonsNum; i++ )
		{
			if( pointerInButtonRect[i] == pointerId )
			{
				processed |= 1<<i;
				if( ! InsideRect( &buttonsAutoFireRect[i], x, y ) )
				{
					pointerInButtonRect[i] = -1;
					if( !ButtonAutoFire[i] )
					{
						if( i != BUTTON_TEXT_INPUT )
							SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, buttonKeysyms[i], 0 );
					}
					else
					{
						ButtonAutoFire[i] = 2;
					}
					ButtonAutoFireX[i*2] = 0;
					ButtonAutoFireX[i*2+1] = 0;
				}
				else
				{
					int coeff = (buttonAutoFireImages[i*2+1].w > buttons[i].w) ? buttonAutoFireImages[i*2+1].w / buttons[i].w + 1 : 1;
					if( ButtonAutoFireRot[i] < x )
						ButtonAutoFireX[i*2+1] += (x - ButtonAutoFireRot[i]) * coeff;
					if( ButtonAutoFireRot[i] > x )
						ButtonAutoFireX[i*2] += (ButtonAutoFireRot[i] - x) * coeff;

					ButtonAutoFireRot[i] = x;

					if( ButtonAutoFireX[i*2] < 0 )
						ButtonAutoFireX[i*2] = 0;
					if( ButtonAutoFireX[i*2+1] < 0 )
						ButtonAutoFireX[i*2+1] = 0;
					if( ButtonAutoFireX[i*2] > buttonAutoFireImages[i*2+1].w / 2 )
						ButtonAutoFireX[i*2] = buttonAutoFireImages[i*2+1].w / 2;
					if( ButtonAutoFireX[i*2+1] > buttonAutoFireImages[i*2+1].w / 2 )
						ButtonAutoFireX[i*2+1] = buttonAutoFireImages[i*2+1].w / 2;

					if( ButtonAutoFireX[i*2] == buttonAutoFireImages[i*2+1].w / 2 &&
						ButtonAutoFireX[i*2+1] == buttonAutoFireImages[i*2+1].w / 2 )
					{
						if( ! ButtonAutoFire[i] )
							ButtonAutoFireDecay[i] = SDL_GetTicks();
						ButtonAutoFire[i] = 1;
					}
				}
			}
		}
		for( i = AutoFireButtonsNum; i < MAX_BUTTONS; i++ )
		{
			if( ! buttons[i].h || ! buttons[i].w )
				continue;
			if( pointerInButtonRect[i] == pointerId )
			{
				processed |= 1<<i;
				if( ! InsideRect( &buttons[i], x, y ) && ! buttonsGenerateSdlEvents[i] )
				{
					pointerInButtonRect[i] = -1;
					if( i != BUTTON_TEXT_INPUT )
						SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, buttonKeysyms[i], 0 );
				}
			}
		}
	}

	for( i = 0; i <= MAX_BUTTONS ; i++ )
		if( ( processed & (1<<i) ) && buttonsGenerateSdlEvents[i] )
			processed |= TOUCHSCREEN_KEYBOARD_PASS_EVENT_DOWN_TO_SDL;
	return processed;
};

void shrinkButtonRect(SDL_Rect s, SDL_Rect * d)
{
	int i;

	if( !buttonDrawSize )
	{
		memcpy(d, &s, sizeof(s));
		return;
	}

	d->w = s.w * 2 / (buttonDrawSize+2);
	d->h = s.h * 2 / (buttonDrawSize+2);
	d->x = s.x + s.w / 2 - d->w / 2;
	d->y = s.y + s.h / 2 - d->h / 2;
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetupScreenKeyboard) ( JNIEnv*  env, jobject thiz, jint size, jint drawsize, jint theme, jint nbuttonsAutoFire, jint _transparency )
{
	int i, ii;
	int nbuttons1row, nbuttons2row;
	int _nbuttons = MAX_BUTTONS;
	SDL_Rect * r;
	touchscreenKeyboardTheme = theme;
	AutoFireButtonsNum = nbuttonsAutoFire;
	if( AutoFireButtonsNum > MAX_BUTTONS_AUTOFIRE )
		AutoFireButtonsNum = MAX_BUTTONS_AUTOFIRE;
	// TODO: works for horizontal screen orientation only!
	buttonsize = size;
	buttonDrawSize = drawsize;
	switch(_transparency)
	{
		case 0: transparency = 32; break;
		case 1: transparency = 64; break;
		case 2: transparency = 128; break;
		case 3: transparency = 192; break;
		case 4: transparency = 255; break;
		default: transparency = 192; break;
	}
	
	// Arrows to the lower-left part of screen
	arrows.w = SDL_ANDROID_sWindowWidth / (size + 2) * 2 / 3;
	arrows.h = arrows.w;
	// Move to the screen edge
	arrows.x = 0;
	arrows.y = SDL_ANDROID_sWindowHeight - arrows.h;

	arrowsExtended.w = arrows.w * 2;
	arrowsExtended.h = arrows.h * 2;
	arrowsExtended.x = arrows.x + arrows.w / 2 - arrowsExtended.w / 2;
	arrowsExtended.y = arrows.y + arrows.h / 2 - arrowsExtended.h / 2;
	/*
	// This will leave some unused space near the edge
	arrows.x = SDL_ANDROID_sWindowWidth / 4;
	arrows.y = SDL_ANDROID_sWindowHeight - SDL_ANDROID_sWindowWidth / 4;
	arrows.x -= arrows.w/2;
	arrows.y -= arrows.h/2;
	// Move arrows from the center of the screen
	arrows.x -= size * SDL_ANDROID_sWindowWidth / 32;
	arrows.y += size * SDL_ANDROID_sWindowWidth / 32;
	*/

	// Buttons to the lower-right in 2 rows
	for(i = 0; i < 3; i++)
	for(ii = 0; ii < 2; ii++)
	{
		// Custom button ordering
		int iii = ii + i*2;
		buttons[iii].w = SDL_ANDROID_sWindowWidth / (size + 2) / 3;
		buttons[iii].h = buttons[iii].w;
		// Move to the screen edge
		buttons[iii].x = SDL_ANDROID_sWindowWidth - buttons[iii].w * (ii + 1);
		buttons[iii].y = SDL_ANDROID_sWindowHeight - buttons[iii].h * (i + 1);
		/*
		// This will leave some unused space near the edge and between buttons
		buttons[iii].x = SDL_ANDROID_sWindowWidth - SDL_ANDROID_sWindowWidth / 12 - (SDL_ANDROID_sWindowWidth * ii / 6);
		buttons[iii].y = SDL_ANDROID_sWindowHeight - SDL_ANDROID_sWindowHeight / 8 - (SDL_ANDROID_sWindowHeight * i / 4);
		buttons[iii].x -= buttons[iii].w/2;
		buttons[iii].y -= buttons[iii].h/2;
		*/
	}
	buttons[6].x = 0;
	buttons[6].y = 0;
	buttons[6].w = SDL_ANDROID_sWindowHeight/10;
	buttons[6].h = SDL_ANDROID_sWindowHeight/10;

	for( i = 0; i < sizeof(pointerInButtonRect)/sizeof(pointerInButtonRect[0]); i++ )
	{
		pointerInButtonRect[i] = -1;
	}
	for( i = 0; i < nbuttonsAutoFire; i++ )
	{
		buttonsAutoFireRect[i].w = buttons[i].w * 2;
		buttonsAutoFireRect[i].h = buttons[i].h * 2;
		buttonsAutoFireRect[i].x = buttons[i].x - buttons[i].w / 2;
		buttonsAutoFireRect[i].y = buttons[i].y - buttons[i].h / 2;
	}
	shrinkButtonRect(arrows, &arrowsDraw);
	for(i = 0; i < MAX_BUTTONS; i++)
	{
		shrinkButtonRect(buttons[i], &buttonsDraw[i]);
	}
};

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetTouchscreenKeyboardUsed) ( JNIEnv*  env, jobject thiz)
{
	SDL_ANDROID_isTouchscreenKeyboardUsed = 1;
}

void SDL_ANDROID_DrawMouseCursor(int x, int y, int size, int alpha)
{
	SDL_Rect r;
	// I've failed with size calcualtions, so leaving it as-is
	r.x = x - MOUSE_POINTER_X;
	r.y = y - MOUSE_POINTER_Y;
	r.w = MOUSE_POINTER_W;
	r.h = MOUSE_POINTER_H;
	beginDrawingTex();
	drawCharTex( &mousePointer, NULL, &r, 255, 255, 255, alpha );
	endDrawingTex();
}

static int
power_of_2(int input)
{
    int value = 1;

    while (value < input) {
        value <<= 1;
    }
    return value;
}

static int setupScreenKeyboardButtonTexture( GLTexture_t * data, Uint8 * charBuf )
{
	int w, h, format, bpp;
	int texture_w, texture_h;

	memcpy(&w, charBuf, sizeof(int));
	memcpy(&h, charBuf + sizeof(int), sizeof(int));
	memcpy(&format, charBuf + 2*sizeof(int), sizeof(int));
	w = ntohl(w);
	h = ntohl(h);
	format = ntohl(format);
	bpp = 2;
	if(format == 2)
		bpp = 4;

	texture_w = power_of_2(w);
	texture_h = power_of_2(h);
	data->w = w;
	data->h = h;

	glEnable(GL_TEXTURE_2D);

	glGenTextures(1, &data->id);
	glBindTexture(GL_TEXTURE_2D, data->id);
	//__android_log_print(ANDROID_LOG_INFO, "libSDL", "On-screen keyboard generated OpenGL texture ID %d", data->id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_w, texture_h, 0, GL_RGBA,
					bpp == 4 ? GL_UNSIGNED_BYTE : (format ? GL_UNSIGNED_SHORT_4_4_4_4 : GL_UNSIGNED_SHORT_5_5_5_1), NULL);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA,
						bpp == 4 ? GL_UNSIGNED_BYTE : (format ? GL_UNSIGNED_SHORT_4_4_4_4 : GL_UNSIGNED_SHORT_5_5_5_1),
						charBuf + 3*sizeof(int) );

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if( SDL_ANDROID_VideoLinearFilter )
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glDisable(GL_TEXTURE_2D);

	return 3*sizeof(int) + w * h * bpp;
}

static int setupScreenKeyboardButtonLegacy( int buttonID, Uint8 * charBuf )
{
	GLTexture_t * data = NULL;

	if( buttonID < 5 )
		data = &(arrowImages[buttonID]);
	else
	if( buttonID < 9 )
		data = &(buttonAutoFireImages[buttonID-5]);
	else
		data = &(buttonImages[buttonID-9]);

	if( buttonID == 23 )
		data = &mousePointer;
	else if( buttonID > 22 ) // Error, array too big
		return 12; // Return value bigger than zero to iterate it

	return setupScreenKeyboardButtonTexture(data, charBuf);
}

static int setupScreenKeyboardButtonSun( int buttonID, Uint8 * charBuf )
{
	GLTexture_t * data = NULL;
	int i, ret;

	if( buttonID == 0 )
		data = &(arrowImages[0]);
	if( buttonID >= 1 && buttonID <= 4 )
		data = &(buttonImages[buttonID-1]);
	if( buttonID >= 5 && buttonID <= 8 )
		data = &(buttonImages[4+(buttonID-5)*2]);
	if( buttonID == 9 )
		data = &mousePointer;
	else if( buttonID > 9 ) // Error, array too big
		return 12; // Return value bigger than zero to iterate it

	ret = setupScreenKeyboardButtonTexture(data, charBuf);

	for( i = 1; i <=4; i++ )
		arrowImages[i] = arrowImages[0];
	
	for( i = 2; i < MAX_BUTTONS; i++ )
		buttonImages[i * 2 + 1] = buttonImages[i * 2];

	for( i = 0; i < MAX_BUTTONS_AUTOFIRE*2; i++ )
		buttonAutoFireImages[i] = arrowImages[0];

	buttonImages[BUTTON_TEXT_INPUT*2] = buttonImages[10];
	buttonImages[BUTTON_TEXT_INPUT*2+1] = buttonImages[10];

	return ret;
}

static int setupScreenKeyboardButton( int buttonID, Uint8 * charBuf, int count )
{
	if(count == 24)
	{
		sunTheme = 0;
		return setupScreenKeyboardButtonLegacy(buttonID, charBuf);
	}
	else if(count == 10)
	{
		sunTheme = 1;
		return setupScreenKeyboardButtonSun(buttonID, charBuf);
	}
	else
	{
		__android_log_print(ANDROID_LOG_FATAL, "libSDL", "On-screen keyboard buton img count = %d, should be 10 or 24", count);
		return 12; // Return value bigger than zero to iterate it
	}
}


JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetupScreenKeyboardButtons) ( JNIEnv*  env, jobject thiz, jbyteArray charBufJava )
{
	jboolean isCopy = JNI_TRUE;
	int len = (*env)->GetArrayLength(env, charBufJava);
	Uint8 * charBuf = (Uint8 *) (*env)->GetByteArrayElements(env, charBufJava, &isCopy);
	int but, pos, count;
	memcpy(&count, charBuf, sizeof(int));
	count = ntohl(count);
	
	for( but = 0, pos = sizeof(int); pos < len; but ++ )
		pos += setupScreenKeyboardButton( but, charBuf + pos, count );
	
	(*env)->ReleaseByteArrayElements(env, charBufJava, (jbyte *)charBuf, 0);
}


int SDL_ANDROID_SetScreenKeyboardButtonPos(int buttonId, SDL_Rect * pos)
{
	if( buttonId < 0 || buttonId >= SDL_ANDROID_SCREENKEYBOARD_BUTTON_NUM || ! pos )
		return 0;
	
	if( buttonId == SDL_ANDROID_SCREENKEYBOARD_BUTTON_DPAD )
	{
		arrows = *pos;
		arrowsExtended.w = arrows.w * 2;
		arrowsExtended.h = arrows.h * 2;
		arrowsExtended.x = arrows.x + arrows.w / 2 - arrowsExtended.w / 2;
		arrowsExtended.y = arrows.y + arrows.h / 2 - arrowsExtended.h / 2;
		shrinkButtonRect(arrows, &arrowsDraw);
	}
	else
	{
		int i = buttonId;
		buttons[i] = *pos;
		shrinkButtonRect(buttons[i], &buttonsDraw[i]);
		if( i < AutoFireButtonsNum )
		{
			buttonsAutoFireRect[i].w = buttons[i].w * 3 / 2;
			buttonsAutoFireRect[i].h = buttons[i].h * 3 / 2;
			buttonsAutoFireRect[i].x = buttons[i].x + buttons[i].w / 2 - buttonsAutoFireRect[i].w / 2;
			buttonsAutoFireRect[i].y = buttons[i].y + buttons[i].h / 2 - buttonsAutoFireRect[i].h / 2;
		}
	}
	return 1;
};

int SDL_ANDROID_GetScreenKeyboardButtonPos(int buttonId, SDL_Rect * pos)
{
	if( buttonId < 0 || buttonId >= SDL_ANDROID_SCREENKEYBOARD_BUTTON_NUM || ! pos )
		return 0;
	
	if( buttonId == SDL_ANDROID_SCREENKEYBOARD_BUTTON_DPAD )
	{
		*pos = arrows;
	}
	else
	{
		*pos = buttons[buttonId];
	}
	return 1;
};

int SDL_ANDROID_SetScreenKeyboardButtonKey(int buttonId, SDLKey key)
{
	if( buttonId < 0 || buttonId > SDL_ANDROID_SCREENKEYBOARD_BUTTON_5 || ! key )
		return 0;
	buttonKeysyms[buttonId] = key;
	return 1;
};

SDLKey SDL_ANDROID_GetScreenKeyboardButtonKey(int buttonId)
{
	if( buttonId < 0 || buttonId > SDL_ANDROID_SCREENKEYBOARD_BUTTON_5 )
		return SDLK_UNKNOWN;
	return buttonKeysyms[buttonId];
};

int SDL_ANDROID_SetScreenKeyboardAutoFireButtonsAmount(int nbuttons)
{
	if( nbuttons < 0 || nbuttons >= MAX_BUTTONS_AUTOFIRE )
		return 0;
	AutoFireButtonsNum = nbuttons;
	return 1;
};

int SDL_ANDROID_GetScreenKeyboardAutoFireButtonsAmount(void)
{
	return AutoFireButtonsNum;
};

int SDL_ANDROID_SetScreenKeyboardShown(int shown)
{
	touchscreenKeyboardShown = shown;
};

int SDL_ANDROID_GetScreenKeyboardShown(void)
{
	return touchscreenKeyboardShown;
};

int SDL_ANDROID_GetScreenKeyboardSize(void)
{
	return buttonsize;
};

int SDL_ANDROID_ToggleScreenKeyboardTextInput(const char * previousText)
{
	static char textIn[255];
	if( previousText == NULL )
		previousText = "";
	strncpy(textIn, previousText, sizeof(textIn));
	textIn[sizeof(textIn)-1] = 0;
	SDL_ANDROID_CallJavaShowScreenKeyboard(textIn, NULL, 0);
	return 1;
};

int SDLCALL SDL_ANDROID_GetScreenKeyboardTextInput(char * textBuf, int textBufSize)
{
	SDL_ANDROID_CallJavaShowScreenKeyboard(textBuf, textBuf, textBufSize);
	return 1;
};

// That's probably not the right file to put this func
JNIEXPORT jint JNICALL
JAVA_EXPORT_NAME(Settings_nativeChmod) ( JNIEnv*  env, jobject thiz, jstring j_name, jint mode )
{
    jboolean iscopy;
    const char *name = (*env)->GetStringUTFChars(env, j_name, &iscopy);
    int ret = chmod(name, mode);
    (*env)->ReleaseStringUTFChars(env, j_name, name);
    return (ret == 0);
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetEnv) ( JNIEnv*  env, jobject thiz, jstring j_name, jstring j_value )
{
    jboolean iscopy;
    const char *name = (*env)->GetStringUTFChars(env, j_name, &iscopy);
    const char *value = (*env)->GetStringUTFChars(env, j_value, &iscopy);
    setenv(name, value, 1);
    (*env)->ReleaseStringUTFChars(env, j_name, name);
    (*env)->ReleaseStringUTFChars(env, j_value, value);
}

int SDLCALL SDL_HasScreenKeyboardSupport(void *unused)
{
	return 1;
}

// SDL2 compatibility
int SDLCALL SDL_ShowScreenKeyboard(void *unused)
{
	return SDL_ANDROID_ToggleScreenKeyboardTextInput(NULL);
}

int SDLCALL SDL_HideScreenKeyboard(void *unused)
{
	SDL_ANDROID_CallJavaHideScreenKeyboard();
	return 1;
}

int SDLCALL SDL_IsScreenKeyboardShown(void *unused)
{
	return SDL_ANDROID_IsScreenKeyboardShown();
}

int SDLCALL SDL_ToggleScreenKeyboard(void *unused)
{
	if( SDL_IsScreenKeyboardShown(NULL) )
		return SDL_HideScreenKeyboard(NULL);
	else
		return SDL_ShowScreenKeyboard(NULL);
}

int SDLCALL SDL_ANDROID_SetScreenKeyboardButtonGenerateTouchEvents(int buttonId, int generateEvents)
{
	if( buttonId < 0 || buttonId >= SDL_ANDROID_SCREENKEYBOARD_BUTTON_NUM )
		return 0;
	buttonsGenerateSdlEvents[buttonId] = generateEvents;
	return 1;
}

static int ScreenKbRedefinedByUser = 0;

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetScreenKbKeyLayout) (JNIEnv* env, jobject thiz, jint keynum, jint x1, jint y1, jint x2, jint y2)
{
	SDL_Rect rect = {x1, y1, x2-x1, y2-y1};
	int key = -1;
	if( keynum == 0 )
		key = SDL_ANDROID_SCREENKEYBOARD_BUTTON_DPAD;
	if( keynum == 1 )
		key = SDL_ANDROID_SCREENKEYBOARD_BUTTON_TEXT;
	if( keynum - 2 >= 0 && keynum - 2 <= SDL_ANDROID_SCREENKEYBOARD_BUTTON_5 - SDL_ANDROID_SCREENKEYBOARD_BUTTON_0 )
		key = keynum - 2 + SDL_ANDROID_SCREENKEYBOARD_BUTTON_0;

	if( key >= 0 )
	{
		ScreenKbRedefinedByUser = 1;
		SDL_ANDROID_SetScreenKeyboardButtonPos(key, &rect);
	}
}

int SDL_ANDROID_GetScreenKeyboardRedefinedByUser()
{
	return ScreenKbRedefinedByUser;
}
