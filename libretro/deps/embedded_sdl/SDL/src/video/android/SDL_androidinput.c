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

#include "SDL_config.h"

#include "SDL_version.h"
#include "SDL_mutex.h"
#include "SDL_events.h"
#if SDL_VERSION_ATLEAST(1,3,0)
#include "SDL_touch.h"
#include "../../events/SDL_touch_c.h"
#endif

#include "../SDL_sysvideo.h"
#include "SDL_androidvideo.h"
#include "SDL_androidinput.h"
#include "SDL_screenkeyboard.h"
#include "jniwrapperstuff.h"
#include "atan2i.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

static SDLKey SDL_android_keymap[KEYCODE_LAST+1];

static inline SDL_scancode TranslateKey(int scancode)
{
	if ( scancode >= SDL_arraysize(SDL_android_keymap) )
		scancode = KEYCODE_UNKNOWN;
	return SDL_android_keymap[scancode];
}

static int isTrackballUsed = 0;
static int isMouseUsed = 0;

enum { RIGHT_CLICK_NONE = 0, RIGHT_CLICK_WITH_MULTITOUCH = 1, RIGHT_CLICK_WITH_PRESSURE = 2, 
		RIGHT_CLICK_WITH_KEY = 3, RIGHT_CLICK_WITH_TIMEOUT = 4 };
enum { LEFT_CLICK_NORMAL = 0, LEFT_CLICK_NEAR_CURSOR = 1, LEFT_CLICK_WITH_MULTITOUCH = 2, LEFT_CLICK_WITH_PRESSURE = 3,
		LEFT_CLICK_WITH_KEY = 4, LEFT_CLICK_WITH_TIMEOUT = 5, LEFT_CLICK_WITH_TAP = 6, LEFT_CLICK_WITH_TAP_OR_TIMEOUT = 7 };
static int leftClickMethod = LEFT_CLICK_NORMAL;
static int rightClickMethod = RIGHT_CLICK_NONE;
static int leftClickKey = KEYCODE_DPAD_CENTER;
static int rightClickKey = KEYCODE_MENU;
int SDL_ANDROID_ShowScreenUnderFinger = ZOOM_NONE;
SDL_Rect SDL_ANDROID_ShowScreenUnderFingerRect = {0, 0, 0, 0}, SDL_ANDROID_ShowScreenUnderFingerRectSrc = {0, 0, 0, 0};
static int moveMouseWithArrowKeys = 0;
static int clickMouseWithDpadCenter = 0;
static int moveMouseWithKbSpeed = 0;
static int moveMouseWithKbAccel = 0;
static int moveMouseWithKbX = -1, moveMouseWithKbY = -1;
static int moveMouseWithKbSpeedX = 0, moveMouseWithKbSpeedY = 0;
static int moveMouseWithKbAccelX = 0, moveMouseWithKbAccelY = 0;
static int moveMouseWithKbAccelUpdateNeeded = 0;
static int maxForce = 0;
static int maxRadius = 0;
int SDL_ANDROID_isJoystickUsed = 0;
static int SDL_ANDROID_isAccelerometerUsed = 0;
static int isMultitouchUsed = 0;
SDL_Joystick *SDL_ANDROID_CurrentJoysticks[MAX_MULTITOUCH_POINTERS+1] = {NULL};
static int TrackballDampening = 0; // in milliseconds
static Uint32 lastTrackballAction = 0;
enum { TOUCH_PTR_UP = 0, TOUCH_PTR_MOUSE = 1, TOUCH_PTR_SCREENKB = 2 };
static int touchPointers[MAX_MULTITOUCH_POINTERS] = {0};
static int firstMousePointerId = -1;
enum { MAX_MULTITOUCH_GESTURES = 4 };
static int multitouchGestureKeycode[MAX_MULTITOUCH_GESTURES] = {
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_6)),
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_7)),
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_8)),
SDL_KEY(SDL_KEY_VAL(SDL_ANDROID_SCREENKB_KEYCODE_9))
};
static int multitouchGestureKeyPressed[MAX_MULTITOUCH_GESTURES] = { 0, 0, 0, 0 };
static int multitouchGestureSensitivity = 0;
static int multitouchGestureDist = -1;
static int multitouchGestureAngle = 0;
static int multitouchGestureX = -1;
static int multitouchGestureY = -1;
int SDL_ANDROID_TouchscreenCalibrationWidth = 480;
int SDL_ANDROID_TouchscreenCalibrationHeight = 320;
int SDL_ANDROID_TouchscreenCalibrationX = 0;
int SDL_ANDROID_TouchscreenCalibrationY = 0;
static int leftClickTimeout = 0;
static int rightClickTimeout = 0;
static int mouseInitialX = -1;
static int mouseInitialY = -1;
static unsigned int mouseInitialTime = 0;
static volatile int deferredMouseTap = 0;
static int relativeMovement = 0;
static int relativeMovementSpeed = 2;
static int relativeMovementAccel = 0;
static int relativeMovementX = 0;
static int relativeMovementY = 0;
static unsigned int relativeMovementTime = 0;
static int currentMouseX = 0;
static int currentMouseY = 0;
static int currentMouseButtons = 0;

static int hardwareMouseDetected = 0;
enum { MOUSE_HW_BUTTON_LEFT = 1, MOUSE_HW_BUTTON_RIGHT = 2, MOUSE_HW_BUTTON_MIDDLE = 4, MOUSE_HW_BUTTON_BACK = 8, MOUSE_HW_BUTTON_FORWARD = 16, MOUSE_HW_BUTTON_MAX = MOUSE_HW_BUTTON_FORWARD };

static int UnicodeToUtf8(int src, char * dest)
{
    int len = 0;
    if ( src <= 0x007f) {
        *dest++ = (char)src;
        len = 1;
    } else if (src <= 0x07ff) {
        *dest++ = (char)0xc0 | (src >> 6);
        *dest++ = (char)0x80 | (src & 0x003f);
        len = 2;
    } else if (src == 0xFEFF) {
        // nop -- zap the BOM
    } else if (src >= 0xD800 && src <= 0xDFFF) {
        // surrogates not supported
    } else if (src <= 0xffff) {
        *dest++ = (char)0xe0 | (src >> 12);
        *dest++ = (char)0x80 | ((src >> 6) & 0x003f);
        *dest++ = (char)0x80 | (src & 0x003f);
        len = 3;
    } else if (src <= 0xffff) {
        *dest++ = (char)0xf0 | (src >> 18);
        *dest++ = (char)0x80 | ((src >> 12) & 0x3f);
        *dest++ = (char)0x80 | ((src >> 6) & 0x3f);
        *dest++ = (char)0x80 | (src & 0x3f);
        len = 4;
    } else {
        // out of Unicode range
    }
    *dest = 0;
    return len;
}

static inline int InsideRect(const SDL_Rect * r, int x, int y)
{
	return ( x >= r->x && x <= r->x + r->w ) && ( y >= r->y && y <= r->y + r->h );
}

void UpdateScreenUnderFingerRect(int x, int y)
{
#if SDL_VERSION_ATLEAST(1,3,0)
	return;
#else
	int screenX = SDL_ANDROID_sFakeWindowWidth, screenY = SDL_ANDROID_sFakeWindowHeight;
	if( SDL_ANDROID_ShowScreenUnderFinger == ZOOM_NONE )
		return;

	if( SDL_ANDROID_ShowScreenUnderFinger == ZOOM_MAGNIFIER )
	{
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.w = screenX / 4;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.h = screenY / 4;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = x - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w/2;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = y - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h/2;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.x < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.y < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = 0;

		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.x > screenX - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = screenX - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.y > screenY - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = screenY - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h;

		SDL_ANDROID_ShowScreenUnderFingerRect.w = SDL_ANDROID_ShowScreenUnderFingerRectSrc.w * 3 / 2;
		SDL_ANDROID_ShowScreenUnderFingerRect.h = SDL_ANDROID_ShowScreenUnderFingerRectSrc.h * 3 / 2;
		SDL_ANDROID_ShowScreenUnderFingerRect.x = x + SDL_ANDROID_ShowScreenUnderFingerRect.w/10;
		SDL_ANDROID_ShowScreenUnderFingerRect.y = y - SDL_ANDROID_ShowScreenUnderFingerRect.h*11/10;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.x < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRect.x = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.y < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRect.y = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.x + SDL_ANDROID_ShowScreenUnderFingerRect.w >= screenX )
			SDL_ANDROID_ShowScreenUnderFingerRect.x = screenX - SDL_ANDROID_ShowScreenUnderFingerRect.w - 1;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.y + SDL_ANDROID_ShowScreenUnderFingerRect.h >= screenY )
			SDL_ANDROID_ShowScreenUnderFingerRect.y = screenY - SDL_ANDROID_ShowScreenUnderFingerRect.h - 1;
		if( InsideRect(&SDL_ANDROID_ShowScreenUnderFingerRect, x, y) )
			SDL_ANDROID_ShowScreenUnderFingerRect.x = x - SDL_ANDROID_ShowScreenUnderFingerRect.w*11/10 - 1;
	}
	if( SDL_ANDROID_ShowScreenUnderFinger == ZOOM_SCREEN_TRANSFORM )
	{
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.w = screenX / 3;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.h = screenY / 3;
		//SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = x - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w/2;
		//SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = y - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h/2;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = x * (screenX - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w) / screenX;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = y * (screenY - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h) / screenY;

		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.x < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.y < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.x > screenX - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = screenX - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.y > screenY - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = screenY - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h;

		SDL_ANDROID_ShowScreenUnderFingerRect.w = screenX * 2 / 3;
		SDL_ANDROID_ShowScreenUnderFingerRect.h = screenY * 2 / 3;
		//SDL_ANDROID_ShowScreenUnderFingerRect.x = x - SDL_ANDROID_ShowScreenUnderFingerRect.w/2;
		//SDL_ANDROID_ShowScreenUnderFingerRect.y = y - SDL_ANDROID_ShowScreenUnderFingerRect.h/2;
		SDL_ANDROID_ShowScreenUnderFingerRect.x = x * (screenX - SDL_ANDROID_ShowScreenUnderFingerRect.w) / screenX;
		SDL_ANDROID_ShowScreenUnderFingerRect.y = y * (screenY - SDL_ANDROID_ShowScreenUnderFingerRect.h) / screenY;
		

		if( SDL_ANDROID_ShowScreenUnderFingerRect.x > SDL_ANDROID_ShowScreenUnderFingerRectSrc.x )
			SDL_ANDROID_ShowScreenUnderFingerRect.x = SDL_ANDROID_ShowScreenUnderFingerRectSrc.x;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.y > SDL_ANDROID_ShowScreenUnderFingerRectSrc.y )
			SDL_ANDROID_ShowScreenUnderFingerRect.y = SDL_ANDROID_ShowScreenUnderFingerRectSrc.y;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.x + SDL_ANDROID_ShowScreenUnderFingerRect.w < SDL_ANDROID_ShowScreenUnderFingerRectSrc.x + SDL_ANDROID_ShowScreenUnderFingerRectSrc.w )
			SDL_ANDROID_ShowScreenUnderFingerRect.x = SDL_ANDROID_ShowScreenUnderFingerRectSrc.x + SDL_ANDROID_ShowScreenUnderFingerRectSrc.w - SDL_ANDROID_ShowScreenUnderFingerRect.w;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.y + SDL_ANDROID_ShowScreenUnderFingerRect.h < SDL_ANDROID_ShowScreenUnderFingerRectSrc.y + SDL_ANDROID_ShowScreenUnderFingerRectSrc.h )
			SDL_ANDROID_ShowScreenUnderFingerRect.y = SDL_ANDROID_ShowScreenUnderFingerRectSrc.y + SDL_ANDROID_ShowScreenUnderFingerRectSrc.h - SDL_ANDROID_ShowScreenUnderFingerRect.h;

		if( SDL_ANDROID_ShowScreenUnderFingerRect.x < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRect.x = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.y < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRect.y = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.x > screenX - SDL_ANDROID_ShowScreenUnderFingerRect.w )
			SDL_ANDROID_ShowScreenUnderFingerRect.x = screenX - SDL_ANDROID_ShowScreenUnderFingerRect.w;
		if( SDL_ANDROID_ShowScreenUnderFingerRect.y > screenY - SDL_ANDROID_ShowScreenUnderFingerRect.h )
			SDL_ANDROID_ShowScreenUnderFingerRect.y = screenY - SDL_ANDROID_ShowScreenUnderFingerRect.h;
	}
	if( SDL_ANDROID_ShowScreenUnderFinger == ZOOM_FULLSCREEN_MAGNIFIER )
	{
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.w = screenX / 2;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.h = screenY / 2;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = x - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w/2;
		SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = y - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h/2;

		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.x < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.y < 0 )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = 0;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.x > screenX - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.x = screenX - SDL_ANDROID_ShowScreenUnderFingerRectSrc.w;
		if( SDL_ANDROID_ShowScreenUnderFingerRectSrc.y > screenY - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h )
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.y = screenY - SDL_ANDROID_ShowScreenUnderFingerRectSrc.h;

		SDL_ANDROID_ShowScreenUnderFingerRect.x = 0;
		SDL_ANDROID_ShowScreenUnderFingerRect.y = 0;
		SDL_ANDROID_ShowScreenUnderFingerRect.w = screenX;
		SDL_ANDROID_ShowScreenUnderFingerRect.h = screenY;
	}
#endif
}


JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(DemoGLSurfaceView_nativeMotionEvent) ( JNIEnv*  env, jobject  thiz, jint x, jint y, jint action, jint pointerId, jint force, jint radius )
{
	// TODO: this method is damn huge
	int i;
#if SDL_VERSION_ATLEAST(1,3,0)

	SDL_Window * window = SDL_GetFocusWindow();
	if( !window )
		return;

#define SDL_ANDROID_sFakeWindowWidth window->w
#define SDL_ANDROID_sFakeWindowHeight window->h

#else
	if( !SDL_CurrentVideoSurface )
		return;
#endif
	if(pointerId < 0)
		pointerId = 0;
	if(pointerId > MAX_MULTITOUCH_POINTERS)
		pointerId = MAX_MULTITOUCH_POINTERS;
	
	// The touch is passed either to on-screen keyboard or as mouse event for all duration of touch between down and up,
	// even if the finger is not anymore above screen kb button it will not acr as mouse event, and if it's initially
	// touches the screen outside of screen kb it won't trigger button keypress -
	// I think it's more logical this way
	if( SDL_ANDROID_isTouchscreenKeyboardUsed && ( action == MOUSE_DOWN || touchPointers[pointerId] & TOUCH_PTR_SCREENKB ) )
	{
		unsigned processed = SDL_ANDROID_processTouchscreenKeyboard(x, y, action, pointerId);
		//__android_log_print(ANDROID_LOG_INFO, "libSDL", "SDL_ANDROID_processTouchscreenKeyboard: ptr %d action %d ret 0x%08x", pointerId, action, processed);
		if( processed && action == MOUSE_DOWN )
			touchPointers[pointerId] |= TOUCH_PTR_SCREENKB;
		if( touchPointers[pointerId] & TOUCH_PTR_SCREENKB )
		{
			if( action == MOUSE_UP )
				touchPointers[pointerId] = TOUCH_PTR_UP;
			if( !(processed & TOUCHSCREEN_KEYBOARD_PASS_EVENT_DOWN_TO_SDL) )
				return;
		}
	}
	
	if( action == MOUSE_DOWN )
	{
		touchPointers[pointerId] |= TOUCH_PTR_MOUSE;
		firstMousePointerId = -1;
		for( i = 0; i < MAX_MULTITOUCH_POINTERS; i++ )
		{
			if( touchPointers[i] & TOUCH_PTR_MOUSE )
			{
				firstMousePointerId = i;
				break;
			}
		}
	}

	x -= SDL_ANDROID_TouchscreenCalibrationX;
	y -= SDL_ANDROID_TouchscreenCalibrationY;
#if SDL_VIDEO_RENDER_RESIZE
	// Translate mouse coordinates

	x = x * SDL_ANDROID_sFakeWindowWidth / SDL_ANDROID_TouchscreenCalibrationWidth;
	y = y * SDL_ANDROID_sFakeWindowHeight / SDL_ANDROID_TouchscreenCalibrationHeight;
	if( x < 0 )
		x = 0;
	if( x > SDL_ANDROID_sFakeWindowWidth )
		x = SDL_ANDROID_sFakeWindowWidth;
	if( y < 0 )
		y = 0;
	if( y > SDL_ANDROID_sFakeWindowHeight )
		y = SDL_ANDROID_sFakeWindowHeight;
#else
	x = x * SDL_ANDROID_sRealWindowWidth / SDL_ANDROID_TouchscreenCalibrationWidth;
	y = y * SDL_ANDROID_sRealWindowHeight / SDL_ANDROID_TouchscreenCalibrationHeight;
#endif

	if( action == MOUSE_UP )
	{
		multitouchGestureX = -1;
		multitouchGestureY = -1;
		multitouchGestureDist = -1;
		for(i = 0; i < MAX_MULTITOUCH_GESTURES; i++)
		{
			if( multitouchGestureKeyPressed[i] )
			{
				multitouchGestureKeyPressed[i] = 0;
				SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, multitouchGestureKeycode[i], 0 );
			}
		}
	}
	else if( !hardwareMouseDetected )
	{
		if( firstMousePointerId != pointerId )
		{
			multitouchGestureX = x;
			multitouchGestureY = y;
		}
		if( firstMousePointerId == pointerId && multitouchGestureX >= 0 )
		{
			int dist = abs( x - multitouchGestureX ) + abs( y - multitouchGestureY );
			int angle = atan2i( y - multitouchGestureY, x - multitouchGestureX );
			if( multitouchGestureDist < 0 )
			{
				multitouchGestureDist = dist;
				multitouchGestureAngle = angle;
			}
			else
			{
				int distMaxDiff = SDL_ANDROID_sFakeWindowHeight / ( 1 + (1 + multitouchGestureSensitivity) * 2 );
				int angleMaxDiff = atan2i_PI / 2 / ( 1 + (1 + multitouchGestureSensitivity) * 2 );
				if( dist - multitouchGestureDist > distMaxDiff )
				{
					multitouchGestureKeyPressed[0] = 1;
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, multitouchGestureKeycode[0], 0 );
				}
				else
				if( multitouchGestureKeyPressed[0] )
				{
					multitouchGestureKeyPressed[0] = 0;
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, multitouchGestureKeycode[0], 0 );
				}
				if( multitouchGestureDist - dist > distMaxDiff )
				{
					multitouchGestureKeyPressed[1] = 1;
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, multitouchGestureKeycode[1], 0 );
				}
				else
				if( multitouchGestureKeyPressed[1] )
				{
					multitouchGestureKeyPressed[1] = 0;
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, multitouchGestureKeycode[1], 0 );
				}

				int angleDiff = angle - multitouchGestureAngle;

				while( angleDiff < atan2i_PI )
					angleDiff += atan2i_PI * 2;
				while( angleDiff > atan2i_PI )
					angleDiff -= atan2i_PI * 2;

				if( angleDiff < -angleMaxDiff )
				{
					multitouchGestureKeyPressed[2] = 1;
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, multitouchGestureKeycode[2], 0 );
				}
				else
				if( multitouchGestureKeyPressed[2] )
				{
					multitouchGestureKeyPressed[2] = 0;
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, multitouchGestureKeycode[2], 0 );
				}
				if( angleDiff > angleMaxDiff )
				{
					multitouchGestureKeyPressed[3] = 1;
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, multitouchGestureKeycode[3], 0 );
				}
				else
				if( multitouchGestureKeyPressed[3] )
				{
					multitouchGestureKeyPressed[3] = 0;
					SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, multitouchGestureKeycode[3], 0 );
				}
			}
		}
	}

	if( isMultitouchUsed )
	{
#if SDL_VERSION_ATLEAST(1,3,0)
		// Use nifty SDL 1.3 multitouch API
		if( action == MOUSE_MOVE )
			SDL_ANDROID_MainThreadPushMultitouchMotion(pointerId, x, y, force + radius);
		else
			SDL_ANDROID_MainThreadPushMultitouchButton(pointerId, action == MOUSE_DOWN ? 1 : 0, x, y, force + radius);
#endif

		if( action == MOUSE_DOWN )
			SDL_ANDROID_MainThreadPushJoystickButton(0, pointerId, SDL_PRESSED);
		SDL_ANDROID_MainThreadPushJoystickBall(0, pointerId, x, y);
		SDL_ANDROID_MainThreadPushJoystickAxis(0, pointerId+4, force + radius); // Radius is more sensitive usually
		if( action == MOUSE_UP )
			SDL_ANDROID_MainThreadPushJoystickButton(0, pointerId, SDL_RELEASED);
	}
	if( !isMouseUsed && !SDL_ANDROID_isTouchscreenKeyboardUsed )
	{
		SDL_keysym keysym;
		if( action != MOUSE_MOVE )
			SDL_ANDROID_MainThreadPushKeyboardKey( action == MOUSE_DOWN ? SDL_PRESSED : SDL_RELEASED, SDL_ANDROID_GetScreenKeyboardButtonKey(SDL_ANDROID_SCREENKEYBOARD_BUTTON_0), 0 );
		return;
	}

	if( !isMouseUsed )
		return;

	if( pointerId == firstMousePointerId )
	{
		if( relativeMovement )
		{
			if( action == MOUSE_DOWN )
			{
				relativeMovementX = currentMouseX - x;
				relativeMovementY = currentMouseY - y;
			}
			x += relativeMovementX;
			y += relativeMovementY;
			
			int diffX = x - currentMouseX;
			int diffY = y - currentMouseY;
			int coeff = relativeMovementSpeed + 2;
			if( relativeMovementSpeed > 2 )
				coeff += relativeMovementSpeed - 2;
			diffX = diffX * coeff / 4;
			diffY = diffY * coeff / 4;
			if( relativeMovementAccel > 0 )
			{
				unsigned int newTime = SDL_GetTicks();
				if( newTime - relativeMovementTime > 0 )
				{
					diffX += diffX * ( relativeMovementAccel * 30 ) / (int)(newTime - relativeMovementTime);
					diffY += diffY * ( relativeMovementAccel * 30 ) / (int)(newTime - relativeMovementTime);
				}
				relativeMovementTime = newTime;
			}
			diffX -= x - currentMouseX;
			diffY -= y - currentMouseY;
			x += diffX;
			y += diffY;
			relativeMovementX += diffX;
			relativeMovementY += diffY;

			diffX = x;
			diffY = y;
			if( x < 0 )
				x = 0;
			if( x > SDL_ANDROID_sFakeWindowWidth )
				x = SDL_ANDROID_sFakeWindowWidth;
			if( y < 0 )
				y = 0;
			if( y > SDL_ANDROID_sFakeWindowHeight )
				y = SDL_ANDROID_sFakeWindowHeight;
			relativeMovementX += x - diffX;
			relativeMovementY += y - diffY;
		}
		if( action == MOUSE_UP )
		{
			if( rightClickMethod != RIGHT_CLICK_WITH_KEY )
				SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_RIGHT );

			if( mouseInitialX >= 0 && mouseInitialY >= 0 && (
				leftClickMethod == LEFT_CLICK_WITH_TAP || leftClickMethod == LEFT_CLICK_WITH_TAP_OR_TIMEOUT ) &&
				abs(mouseInitialX - x) < SDL_ANDROID_sFakeWindowHeight / 16 &&
				abs(mouseInitialY - y) < SDL_ANDROID_sFakeWindowHeight / 16 &&
				SDL_GetTicks() - mouseInitialTime < 700 )
			{
				SDL_ANDROID_MainThreadPushMouseMotion(mouseInitialX, mouseInitialY);
				SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_LEFT );
				deferredMouseTap = 2;
				mouseInitialX = -1;
				mouseInitialY = -1;
			}
			else
			{
				if( leftClickMethod != LEFT_CLICK_WITH_KEY )
					SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_LEFT );
			}

			SDL_ANDROID_ShowScreenUnderFingerRect.w = SDL_ANDROID_ShowScreenUnderFingerRect.h = 0;
			SDL_ANDROID_ShowScreenUnderFingerRectSrc.w = SDL_ANDROID_ShowScreenUnderFingerRectSrc.h = 0;
			if( SDL_ANDROID_ShowScreenUnderFinger == ZOOM_MAGNIFIER )
			{
				// Move mouse by 1 pixel so it will force screen update and mouse-under-finger window will be removed
				if( moveMouseWithKbX >= 0 )
					SDL_ANDROID_MainThreadPushMouseMotion(moveMouseWithKbX > 0 ? moveMouseWithKbX-1 : 0, moveMouseWithKbY);
				else
					SDL_ANDROID_MainThreadPushMouseMotion(x > 0 ? x-1 : 0, y);
			}
			moveMouseWithKbX = -1;
			moveMouseWithKbY = -1;
			moveMouseWithKbSpeedX = 0;
			moveMouseWithKbSpeedY = 0;
		}
		if( action == MOUSE_DOWN )
		{
			if( (moveMouseWithKbX >= 0 || leftClickMethod == LEFT_CLICK_NEAR_CURSOR) &&
				abs(currentMouseX - x) < SDL_ANDROID_sFakeWindowWidth / 10 && abs(currentMouseY - y) < SDL_ANDROID_sFakeWindowHeight / 10 )
			{
				SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_LEFT );
				moveMouseWithKbX = currentMouseX;
				moveMouseWithKbY = currentMouseY;
				moveMouseWithKbSpeedX = 0;
				moveMouseWithKbSpeedY = 0;
				action = MOUSE_MOVE;
			}
			else
			if( leftClickMethod == LEFT_CLICK_NORMAL )
			{
				SDL_ANDROID_MainThreadPushMouseMotion(x, y);
				if( !hardwareMouseDetected || currentMouseButtons == 0 )
					SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_LEFT );
			}
			else
			{
				SDL_ANDROID_MainThreadPushMouseMotion(x, y);
				action == MOUSE_MOVE;
				mouseInitialX = x;
				mouseInitialY = y;
				mouseInitialTime = SDL_GetTicks();
			}
			UpdateScreenUnderFingerRect(x, y);
		}
		if( action == MOUSE_MOVE )
		{
			if( moveMouseWithKbX >= 0 )
			{
				// Mouse lazily follows magnifying glass, not very intuitive for drag&drop
				/*
				if( abs(moveMouseWithKbX - x) > SDL_ANDROID_sFakeWindowWidth / 12 )
					moveMouseWithKbSpeedX += moveMouseWithKbX > x ? -1 : 1;
				else
					moveMouseWithKbSpeedX = moveMouseWithKbSpeedX * 2 / 3;
				if( abs(moveMouseWithKbY - y) > SDL_ANDROID_sFakeWindowHeight / 12 )
					moveMouseWithKbSpeedY += moveMouseWithKbY > y ? -1 : 1;
				else
					moveMouseWithKbSpeedY = moveMouseWithKbSpeedY * 2 / 3;

				moveMouseWithKbX += moveMouseWithKbSpeedX;
				moveMouseWithKbY += moveMouseWithKbSpeedY;
				*/
				// Mouse follows touch instantly, when it's out of the snapping distance from mouse cursor
				if( abs(moveMouseWithKbX - x) >= SDL_ANDROID_sFakeWindowWidth / 10 ||
					abs(moveMouseWithKbY - y) >= SDL_ANDROID_sFakeWindowHeight / 10 )
				{
					moveMouseWithKbX = -1;
					moveMouseWithKbY = -1;
					moveMouseWithKbSpeedX = 0;
					moveMouseWithKbSpeedY = 0;
					SDL_ANDROID_MainThreadPushMouseMotion(x, y);
				}
				else
					SDL_ANDROID_MainThreadPushMouseMotion(moveMouseWithKbX, moveMouseWithKbY);
			}
			else
			{
				SDL_ANDROID_MainThreadPushMouseMotion(x, y);
			}

			if( rightClickMethod == RIGHT_CLICK_WITH_PRESSURE || leftClickMethod == LEFT_CLICK_WITH_PRESSURE )
			{
				int button = (leftClickMethod == LEFT_CLICK_WITH_PRESSURE) ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
				int buttonState = ( force > maxForce || radius > maxRadius );
				if( button == SDL_BUTTON_RIGHT )
					SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_LEFT );
				SDL_ANDROID_MainThreadPushMouseButton( buttonState ? SDL_PRESSED : SDL_RELEASED, button );
			}
			if( mouseInitialX >= 0 && mouseInitialY >= 0 && (
				leftClickMethod == LEFT_CLICK_WITH_TIMEOUT || leftClickMethod == LEFT_CLICK_WITH_TAP ||
				leftClickMethod == LEFT_CLICK_WITH_TAP_OR_TIMEOUT || rightClickMethod == RIGHT_CLICK_WITH_TIMEOUT ) )
			{
				if( abs(mouseInitialX - x) >= SDL_ANDROID_sFakeWindowHeight / 15 || abs(mouseInitialY - y) >= SDL_ANDROID_sFakeWindowHeight / 15 )
				{
					mouseInitialX = -1;
					mouseInitialY = -1;
				}
				else
				{
					if( leftClickMethod == LEFT_CLICK_WITH_TIMEOUT || leftClickMethod == LEFT_CLICK_WITH_TAP_OR_TIMEOUT )
					{
						if( SDL_GetTicks() - mouseInitialTime > leftClickTimeout )
						{
							//SDL_ANDROID_MainThreadPushMouseMotion(mouseInitialX, mouseInitialY);
							SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_LEFT );
							mouseInitialX = -1;
							mouseInitialY = -1;
						}
					}
					if( rightClickMethod == RIGHT_CLICK_WITH_TIMEOUT )
					{
						if( SDL_GetTicks() - mouseInitialTime > rightClickTimeout )
						{
							//SDL_ANDROID_MainThreadPushMouseMotion(mouseInitialX, mouseInitialY);
							SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_RIGHT );
							mouseInitialX = -1;
							mouseInitialY = -1;
						}
					}
				}
			}
			if( SDL_ANDROID_ShowScreenUnderFinger == ZOOM_MAGNIFIER )
				UpdateScreenUnderFingerRect(x, y);
		}
		if( SDL_ANDROID_ShowScreenUnderFinger == ZOOM_SCREEN_TRANSFORM ||
			SDL_ANDROID_ShowScreenUnderFinger == ZOOM_FULLSCREEN_MAGNIFIER )
			UpdateScreenUnderFingerRect(x, y);
	}
	if( pointerId != firstMousePointerId && (action == MOUSE_DOWN || action == MOUSE_UP) )
	{
		if( leftClickMethod == LEFT_CLICK_WITH_MULTITOUCH )
		{
			SDL_ANDROID_MainThreadPushMouseButton( (action == MOUSE_DOWN) ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT );
		}
		else if( rightClickMethod == RIGHT_CLICK_WITH_MULTITOUCH )
		{
			SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_LEFT );
			SDL_ANDROID_MainThreadPushMouseButton( (action == MOUSE_DOWN) ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_RIGHT );
		}
	}

	if( action == MOUSE_UP )
	{
		touchPointers[pointerId] = TOUCH_PTR_UP;
		firstMousePointerId = -1;
		for( i = 0; i < MAX_MULTITOUCH_POINTERS; i++ )
		{
			if( touchPointers[i] |= TOUCH_PTR_MOUSE )
			{
				firstMousePointerId = i;
				break;
			}
		}
	}
	
	if( action == MOUSE_HOVER && !relativeMovement )
	{
		SDL_ANDROID_MainThreadPushMouseMotion(x, y);
	}
}

void ProcessDeferredMouseTap()
{
	if( deferredMouseTap > 0 )
	{
		deferredMouseTap--;
		if( deferredMouseTap <= 0 )
		{
#if SDL_VERSION_ATLEAST(1,3,0)
			SDL_Window * window = SDL_GetFocusWindow();
			if( !window )
				return;
#define SDL_ANDROID_sFakeWindowWidth window->w
#define SDL_ANDROID_sFakeWindowHeight window->h
#endif
			if( currentMouseX + 1 < SDL_ANDROID_sFakeWindowWidth )
				SDL_ANDROID_MainThreadPushMouseMotion(currentMouseX + 1, currentMouseY);
			SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_LEFT );
		}
		else if( currentMouseX > 0 ) // Force application to redraw, and call SDL_Flip()
			SDL_ANDROID_MainThreadPushMouseMotion(currentMouseX - 1, currentMouseY);
	}
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(DemoGLSurfaceView_nativeTouchpad) ( JNIEnv*  env, jobject thiz, jint x, jint y, jint down, jint multitouch)
{
	if( !isMouseUsed )
		return;
	if( ! down )
	{
		SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_RIGHT );
		SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_LEFT );
		moveMouseWithKbX = -1;
		moveMouseWithKbY = -1;
		moveMouseWithKbAccelUpdateNeeded = 0;
	}
	else
	{
		// x and y from 0 to 65535
		if( moveMouseWithKbX < 0 )
		{
			moveMouseWithKbX = currentMouseX;
			moveMouseWithKbY = currentMouseY;
		}
		moveMouseWithKbSpeedX = (x - 32767) / 8192;
		moveMouseWithKbSpeedY = (y - 32767) / 8192;
		//moveMouseWithKbX += moveMouseWithKbSpeedX;
		//moveMouseWithKbY += moveMouseWithKbSpeedY;
		SDL_ANDROID_MainThreadPushMouseMotion(moveMouseWithKbX, moveMouseWithKbY);
		moveMouseWithKbAccelUpdateNeeded = 1;

		if( multitouch )
			SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_RIGHT );
		else
		if( abs(x - 32767) < 8192 && abs(y - 32767) < 8192 )
			SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_LEFT );
	}
}

void SDL_ANDROID_WarpMouse(int x, int y)
{
	if(!relativeMovement)
	{
		//SDL_ANDROID_MainThreadPushMouseMotion(x, y);
	}
	else
	{
		//__android_log_print(ANDROID_LOG_INFO, "libSDL", "SDL_ANDROID_WarpMouse(): %dx%d rel %dx%d old %dx%d", x, y, relativeMovementX, relativeMovementY, currentMouseX, currentMouseY);
		relativeMovementX -= currentMouseX-x;
		relativeMovementY -= currentMouseY-y;
		SDL_ANDROID_MainThreadPushMouseMotion(x, y);
	}
};

static int processAndroidTrackball(int key, int action);

JNIEXPORT jint JNICALL
JAVA_EXPORT_NAME(DemoGLSurfaceView_nativeKey) ( JNIEnv*  env, jobject thiz, jint key, jint unicode, jint action )
{
#if SDL_VERSION_ATLEAST(1,3,0)
#else
	if( !SDL_CurrentVideoSurface )
		return 1;
#endif

	if( isTrackballUsed )
		if( processAndroidTrackball(key, action) )
			return 1;
	if( key == rightClickKey && rightClickMethod == RIGHT_CLICK_WITH_KEY )
	{
		SDL_ANDROID_MainThreadPushMouseButton( action ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_RIGHT );
		return 1;
	}
	if( (key == leftClickKey && leftClickMethod == LEFT_CLICK_WITH_KEY) || (clickMouseWithDpadCenter && key == KEYCODE_DPAD_CENTER) )
	{
		SDL_ANDROID_MainThreadPushMouseButton( action ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT );
		return 1;
	}

	if( TranslateKey(key) == SDLK_NO_REMAP || TranslateKey(key) == SDLK_UNKNOWN )
		return 0;

	SDL_ANDROID_MainThreadPushKeyboardKey( action ? SDL_PRESSED : SDL_RELEASED, TranslateKey(key), unicode );
	return 1;
}

static char * textInputBuffer = NULL;
int textInputBufferLen = 0;
int textInputBufferPos = 0;

void SDL_ANDROID_TextInputInit(char * buffer, int len)
{
	textInputBuffer = buffer;
	textInputBufferLen = len;
	textInputBufferPos = 0;
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(DemoRenderer_nativeTextInput) ( JNIEnv*  env, jobject thiz, jint ascii, jint unicode )
{
	if( ascii == 10 )
		ascii = SDLK_RETURN;

	if( !textInputBuffer )
		SDL_ANDROID_MainThreadPushText(ascii, unicode);
	else
	{
		if( textInputBufferPos < textInputBufferLen + 4 && ascii != SDLK_RETURN && ascii != '\r' && ascii != '\n' )
		{
			textInputBufferPos += UnicodeToUtf8(unicode, textInputBuffer + textInputBufferPos);
		}
	}
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(DemoRenderer_nativeTextInputFinished) ( JNIEnv*  env, jobject thiz )
{
	textInputBuffer = NULL;
	SDL_ANDROID_TextInputFinished = 1;
}

static void updateOrientation ( float accX, float accY, float accZ );

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(AccelerometerReader_nativeAccelerometer) ( JNIEnv*  env, jobject  thiz, jfloat accPosX, jfloat accPosY, jfloat accPosZ )
{
#if SDL_VERSION_ATLEAST(1,3,0)
#else
	if( !SDL_CurrentVideoSurface )
		return;
#endif
	// Calculate two angles from three coordinates - TODO: this is faulty!
	//float accX = atan2f(-accPosX, sqrtf(accPosY*accPosY+accPosZ*accPosZ) * ( accPosY > 0 ? 1.0f : -1.0f ) ) * M_1_PI * 180.0f;
	//float accY = atan2f(accPosZ, accPosY) * M_1_PI;
	
	float normal = sqrt(accPosX*accPosX+accPosY*accPosY+accPosZ*accPosZ);
	if(normal <= 0.0000001f)
		normal = 0.00001f;
	
	updateOrientation (accPosX/normal, accPosY/normal, 0.0f);
}


JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(AccelerometerReader_nativeOrientation) ( JNIEnv*  env, jobject  thiz, jfloat accX, jfloat accY, jfloat accZ )
{
#if SDL_VERSION_ATLEAST(1,3,0)
#else
	if( !SDL_CurrentVideoSurface )
		return;
#endif
	updateOrientation (accX, accY, accZ); // TODO: make values in range 0.0:1.0
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetTrackballUsed) ( JNIEnv*  env, jobject thiz)
{
	isTrackballUsed = 1;
}

static int getClickTimeout(int v)
{
	switch(v)
	{
		case 0: return 200;
		case 1: return 300;
		case 2: return 400;
		case 3: return 700;
		case 4: return 1000;
	}
	return 1000;
}

// Mwahaha overkill!
JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetMouseUsed) (JNIEnv* env, jobject thiz,
		jint RightClickMethod, jint ShowScreenUnderFinger, jint LeftClickMethod,
		jint MoveMouseWithJoystick, jint ClickMouseWithDpad,
		jint MaxForce, jint MaxRadius,
		jint MoveMouseWithJoystickSpeed, jint MoveMouseWithJoystickAccel,
		jint LeftClickKeycode, jint RightClickKeycode,
		jint LeftClickTimeout, jint RightClickTimeout,
		jint RelativeMovement, jint RelativeMovementSpeed, jint RelativeMovementAccel,
		jint ShowMouseCursor)
{
	isMouseUsed = 1;
	rightClickMethod = RightClickMethod;
	SDL_ANDROID_ShowScreenUnderFinger = ShowScreenUnderFinger;
	moveMouseWithArrowKeys = MoveMouseWithJoystick;
	clickMouseWithDpadCenter = ClickMouseWithDpad;
	leftClickMethod = LeftClickMethod;
	maxForce = MaxForce;
	maxRadius = MaxRadius;
	moveMouseWithKbSpeed = MoveMouseWithJoystickSpeed + 1;
	moveMouseWithKbAccel = MoveMouseWithJoystickAccel;
	leftClickKey = LeftClickKeycode;
	rightClickKey = RightClickKeycode;
	leftClickTimeout = getClickTimeout(LeftClickTimeout);
	rightClickTimeout = getClickTimeout(RightClickTimeout);
	relativeMovement = RelativeMovement;
	relativeMovementSpeed = RelativeMovementSpeed;
	relativeMovementAccel = RelativeMovementAccel;
	SDL_ANDROID_ShowMouseCursor = ShowMouseCursor;
	//__android_log_print(ANDROID_LOG_INFO, "libSDL", "relativeMovementSpeed %d relativeMovementAccel %d", relativeMovementSpeed, relativeMovementAccel);
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(DemoGLSurfaceView_nativeHardwareMouseDetected) (JNIEnv* env, jobject thiz, int detected)
{
	if( !isMouseUsed )
		return;

	static struct {
		int leftClickMethod;
		int ShowScreenUnderFinger;
		int leftClickTimeout;
		int relativeMovement;
		int ShowMouseCursor;
	} cfg = { 0 };

	if( hardwareMouseDetected != detected )
	{
		hardwareMouseDetected = detected;
		if(detected)
		{
			cfg.leftClickMethod = leftClickMethod;
			cfg.ShowScreenUnderFinger = SDL_ANDROID_ShowScreenUnderFinger;
			cfg.leftClickTimeout = leftClickTimeout;
			cfg.relativeMovement = relativeMovement;
			cfg.ShowMouseCursor = SDL_ANDROID_ShowMouseCursor;
			
			leftClickMethod = LEFT_CLICK_NORMAL;
			SDL_ANDROID_ShowScreenUnderFinger = 0;
			leftClickTimeout = 0;
			relativeMovement = 0;
			SDL_ANDROID_ShowMouseCursor = 0;
		}
		else
		{
			leftClickMethod = cfg.leftClickMethod;
			SDL_ANDROID_ShowScreenUnderFinger = cfg.ShowScreenUnderFinger;
			leftClickTimeout = cfg.leftClickTimeout;
			relativeMovement = cfg.relativeMovement;
			SDL_ANDROID_ShowMouseCursor = cfg.ShowMouseCursor;
		}
	}
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(DemoGLSurfaceView_nativeMouseButtonsPressed) (JNIEnv* env, jobject thiz, jint buttonId, jint pressedState)
{
	int btn = SDL_BUTTON_LEFT;
	if( !isMouseUsed )
		return;

	switch(buttonId)
	{
		case MOUSE_HW_BUTTON_LEFT:
			btn = SDL_BUTTON_LEFT;
			break;
		case MOUSE_HW_BUTTON_RIGHT:
			btn = SDL_BUTTON_RIGHT;
			break;
		case MOUSE_HW_BUTTON_MIDDLE:
			btn = SDL_BUTTON_MIDDLE;
			break;
		case MOUSE_HW_BUTTON_BACK:
			btn = SDL_BUTTON_X1;
			break;
		case MOUSE_HW_BUTTON_FORWARD:
			btn = SDL_BUTTON_X2;
			break;
	}
	SDL_ANDROID_MainThreadPushMouseButton( pressedState ? SDL_PRESSED : SDL_RELEASED, btn );
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(DemoGLSurfaceView_nativeMouseWheel) (JNIEnv* env, jobject thiz, jint scrollX, jint scrollY)
{
#if SDL_VERSION_ATLEAST(1,3,0)
	SDL_ANDROID_MainThreadPushMouseWheel( scrollX, scrollY );
#else
	// TODO: direction might get inverted
	for( ; scrollX > 0; scrollX-- )
	{
		SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_DPAD_RIGHT), 0 );
		SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_RIGHT), 0 );
	}
	for( ; scrollX < 0; scrollX++ )
	{
		SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_DPAD_LEFT), 0 );
		SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_LEFT), 0 );
	}
	for( ; scrollY > 0; scrollY-- )
	{
		if(!isMouseUsed)
		{
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_DPAD_UP), 0 );
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_UP), 0 );
		}
		else
		{
			SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_WHEELUP );
			SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_WHEELUP );
		}
	}
	for( ; scrollY < 0; scrollY++ )
	{
		if(!isMouseUsed)
		{
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_DPAD_DOWN), 0 );
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_DOWN), 0 );
		}
		else
		{
			SDL_ANDROID_MainThreadPushMouseButton( SDL_PRESSED, SDL_BUTTON_WHEELDOWN );
			SDL_ANDROID_MainThreadPushMouseButton( SDL_RELEASED, SDL_BUTTON_WHEELDOWN );
		}
	}
#endif
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetJoystickUsed) (JNIEnv* env, jobject thiz)
{
	SDL_ANDROID_isJoystickUsed = 1;
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetAccelerometerUsed) (JNIEnv* env, jobject thiz)
{
	SDL_ANDROID_isAccelerometerUsed = 1;
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetMultitouchUsed) ( JNIEnv*  env, jobject thiz)
{
	isMultitouchUsed = 1;
}


static float dx = 0.04, dy = 0.1, dz = 0.1, joystickSensitivity = 400.0f; // For accelerometer
enum { ACCELEROMETER_CENTER_FLOATING, ACCELEROMETER_CENTER_FIXED_START, ACCELEROMETER_CENTER_FIXED_HORIZ };
static int accelerometerCenterPos = ACCELEROMETER_CENTER_FLOATING;

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetAccelerometerSettings) ( JNIEnv*  env, jobject thiz, jint sensitivity, jint centerPos)
{
	dx = 0.04; dy = 0.08; dz = 0.08; joystickSensitivity = 32767.0f * 3.0f; // Fast sensitivity
	if( sensitivity == 1 )
	{
		dx = 0.1; dy = 0.15; dz = 0.15; joystickSensitivity = 32767.0f * 2.0f; // Medium sensitivity
	}
	if( sensitivity == 2 )
	{
		dx = 0.2; dy = 0.25; dz = 0.25; joystickSensitivity = 32767.0f; // Slow sensitivity
	}
	accelerometerCenterPos = centerPos;
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetTrackballDampening) ( JNIEnv*  env, jobject thiz, jint value)
{
	TrackballDampening = (value * 200);
}

void updateOrientation ( float accX, float accY, float accZ )
{
	SDL_keysym keysym;
	// TODO: ask user for accelerometer precision from Java

	static float midX = 0, midY = 0, midZ = 0;
	static int pressLeft = 0, pressRight = 0, pressUp = 0, pressDown = 0, pressR = 0, pressL = 0;

	//__android_log_print(ANDROID_LOG_INFO, "libSDL", "updateOrientation(): %f %f %f", accX, accY, accZ);
	
	if( SDL_ANDROID_isAccelerometerUsed )
	{
		//__android_log_print(ANDROID_LOG_INFO, "libSDL", "updateOrientation(): sending joystick event");
		SDL_ANDROID_MainThreadPushJoystickAxis(0, 2, (Sint16)(fminf(32767.0f, fmax(-32767.0f, (accX) * 32767.0f))));
		SDL_ANDROID_MainThreadPushJoystickAxis(0, 3, (Sint16)(fminf(32767.0f, fmax(-32767.0f, -(accY) * 32767.0f))));
		//SDL_ANDROID_MainThreadPushJoystickAxis(0, 2, (Sint16)(fminf(32767.0f, fmax(-32767.0f, -(accZ) * 32767.0f))));
		return;
	}

	if( accelerometerCenterPos == ACCELEROMETER_CENTER_FIXED_START )
	{
		accelerometerCenterPos = ACCELEROMETER_CENTER_FIXED_HORIZ;
		midX = accX;
		midY = accY;
		midZ = accZ;
	}
	
	if( SDL_ANDROID_isJoystickUsed )
	{
		//__android_log_print(ANDROID_LOG_INFO, "libSDL", "updateOrientation(): sending joystick event");
		SDL_ANDROID_MainThreadPushJoystickAxis(0, 0, (Sint16)(fminf(32767.0f, fmax(-32767.0f, (accX - midX) * joystickSensitivity))));
		SDL_ANDROID_MainThreadPushJoystickAxis(0, 1, (Sint16)(fminf(32767.0f, fmax(-32767.0f, -(accY - midY) * joystickSensitivity))));
		//SDL_ANDROID_MainThreadPushJoystickAxis(0, 2, (Sint16)(fminf(32767.0f, fmax(-32767.0f, -(accZ - midZ) * joystickSensitivity))));

		if( accelerometerCenterPos == ACCELEROMETER_CENTER_FLOATING )
		{
			if( accY < midY - dy*2 )
				midY = accY + dy*2;
			if( accY > midY + dy*2 )
				midY = accY - dy*2;
			if( accZ < midZ - dz*2 )
				midZ = accZ + dz*2;
			if( accZ > midZ + dz*2 )
				midZ = accZ - dz*2;
		}
		return;
	}

	if( accX < midX - dx )
	{
		if( !pressLeft )
		{
			//__android_log_print(ANDROID_LOG_INFO, "libSDL", "Accelerometer: press left, acc %f mid %f d %f", accX, midX, dx);
			pressLeft = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_DPAD_LEFT), 0 );
		}
	}
	else
	{
		if( pressLeft )
		{
			//__android_log_print(ANDROID_LOG_INFO, "libSDL", "Accelerometer: release left, acc %f mid %f d %f", accX, midX, dx);
			pressLeft = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_LEFT), 0 );
		}
	}
	if( accX < midX - dx*2 )
		midX = accX + dx*2;

	if( accX > midX + dx )
	{
		if( !pressRight )
		{
			//__android_log_print(ANDROID_LOG_INFO, "libSDL", "Accelerometer: press right, acc %f mid %f d %f", accX, midX, dx);
			pressRight = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_DPAD_RIGHT), 0 );
		}
	}
	else
	{
		if( pressRight )
		{
			//__android_log_print(ANDROID_LOG_INFO, "libSDL", "Accelerometer: release right, acc %f mid %f d %f", accX, midX, dx);
			pressRight = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_RIGHT), 0 );
		}
	}
	if( accX > midX + dx*2 )
		midX = accX - dx*2;

	if( accY < midY - dy )
	{
		if( !pressUp )
		{
			//__android_log_print(ANDROID_LOG_INFO, "libSDL", "Accelerometer: press up, acc %f mid %f d %f", accY, midY, dy);
			pressUp = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_DPAD_DOWN), 0 );
		}
	}
	else
	{
		if( pressUp )
		{
			//__android_log_print(ANDROID_LOG_INFO, "libSDL", "Accelerometer: release up, acc %f mid %f d %f", accY, midY, dy);
			pressUp = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_DOWN), 0 );
		}
	}
	if( accY < midY - dy*2 )
		midY = accY + dy*2;

	if( accY > midY + dy )
	{
		if( !pressDown )
		{
			//__android_log_print(ANDROID_LOG_INFO, "libSDL", "Accelerometer: press down, acc %f mid %f d %f", accY, midY, dy);
			pressDown = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_DPAD_UP), 0 );
		}
	}
	else
	{
		if( pressDown )
		{
			//__android_log_print(ANDROID_LOG_INFO, "libSDL", "Accelerometer: release down, acc %f mid %f d %f", accY, midY, dy);
			pressDown = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_UP), 0 );
		}
	}
	if( accY > midY + dy*2 )
		midY = accY - dy*2;

	if( accZ < midZ - dz )
	{
		if( !pressL )
		{
			pressL = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_ALT_LEFT), 0 );
		}
	}
	else
	{
		if( pressL )
		{
			pressL = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_ALT_LEFT), 0 );
		}
	}
	if( accZ < midZ - dz*2 )
		midZ = accZ + dz*2;

	if( accZ > midZ + dz )
	{
		if( !pressR )
		{
			pressR = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(KEYCODE_ALT_RIGHT), 0 );
		}
	}
	else
	{
		if( pressR )
		{
			pressR = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_ALT_RIGHT), 0 );
		}
	}
	if( accZ > midZ + dz*2 )
		midZ = accZ - dz*2;

}

static int leftPressed = 0, rightPressed = 0, upPressed = 0, downPressed = 0;

int processAndroidTrackball(int key, int action)
{
	SDL_keysym keysym;
	
	if( ! action && (
		key == KEYCODE_DPAD_UP ||
		key == KEYCODE_DPAD_DOWN ||
		key == KEYCODE_DPAD_LEFT ||
		key == KEYCODE_DPAD_RIGHT ) )
		return 1;
	lastTrackballAction = SDL_GetTicks();

	if( key == KEYCODE_DPAD_UP )
	{
		if( downPressed )
		{
			downPressed = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_DOWN), 0 );
			return 1;
		}
		if( !upPressed )
		{
			upPressed = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(key), 0 );
		}
		else
		{
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(key), 0 );
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(key), 0 );
		}
		return 1;
	}

	if( key == KEYCODE_DPAD_DOWN )
	{
		if( upPressed )
		{
			upPressed = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_UP), 0 );
			return 1;
		}
		if( !upPressed )
		{
			downPressed = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(key), 0 );
		}
		else
		{
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(key), 0 );
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(key), 0 );
		}
		return 1;
	}

	if( key == KEYCODE_DPAD_LEFT )
	{
		if( rightPressed )
		{
			rightPressed = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_RIGHT), 0 );
			return 1;
		}
		if( !leftPressed )
		{
			leftPressed = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(key), 0 );
		}
		else
		{
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(key), 0 );
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(key), 0 );
		}
		return 1;
	}

	if( key == KEYCODE_DPAD_RIGHT )
	{
		if( leftPressed )
		{
			leftPressed = 0;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_LEFT), 0 );
			return 1;
		}
		if( !rightPressed )
		{
			rightPressed = 1;
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(key), 0 );
		}
		else
		{
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(key), 0 );
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_PRESSED, TranslateKey(key), 0 );
		}
		return 1;
	}

	return 0;
}

void SDL_ANDROID_processAndroidTrackballDampening()
{
	if( !TrackballDampening )
		return;
	if( SDL_GetTicks() > TrackballDampening + lastTrackballAction  )
	{
		if( upPressed )
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_UP), 0 );
		if( downPressed )
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_DOWN), 0 );
		if( leftPressed )
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_LEFT), 0 );
		if( rightPressed )
			SDL_ANDROID_MainThreadPushKeyboardKey( SDL_RELEASED, TranslateKey(KEYCODE_DPAD_RIGHT), 0 );
		upPressed = 0;
		downPressed = 0;
		leftPressed = 0;
		rightPressed = 0;
	}
}

int SDL_SYS_JoystickInit(void)
{
	SDL_numjoysticks = 0;
	if( SDL_ANDROID_isJoystickUsed || isMultitouchUsed || SDL_ANDROID_isAccelerometerUsed )
		SDL_numjoysticks = 1;
	//if( isMultitouchUsed )
	//	SDL_numjoysticks = MAX_MULTITOUCH_POINTERS+1;

	return(SDL_numjoysticks);
}

/* Function to get the device-dependent name of a joystick */
const char *SDL_SYS_JoystickName(int index)
{
	return("Android accelerometer/multitouch sensor");
}

int SDL_SYS_JoystickOpen(SDL_Joystick *joystick)
{
	joystick->nbuttons = 0;
	joystick->nhats = 0;
	joystick->nballs = 0;
	if( joystick->index == 0 )
	{
		joystick->naxes = 4; // Joystick plus accelerometer
		if(isMultitouchUsed)
		{
			joystick->naxes = 4 + MAX_MULTITOUCH_POINTERS; // Joystick plus accelerometer, plus touch pressure/size
			joystick->nbuttons = MAX_MULTITOUCH_POINTERS;
			joystick->nballs = MAX_MULTITOUCH_POINTERS;
		}
	}
	SDL_ANDROID_CurrentJoysticks[joystick->index] = joystick;
	return(0);
}

void SDL_SYS_JoystickUpdate(SDL_Joystick *joystick)
{
	return;
}

/* Function to close a joystick after use */
void SDL_SYS_JoystickClose(SDL_Joystick *joystick)
{
	SDL_ANDROID_CurrentJoysticks[joystick->index] = NULL;
	return;
}

/* Function to perform any system-specific joystick related cleanup */
void SDL_SYS_JoystickQuit(void)
{
	int i;
	SDL_ANDROID_CurrentJoysticks[0] = NULL;
	return;
}


enum { MAX_BUFFERED_EVENTS = 64 };
static SDL_Event BufferedEvents[MAX_BUFFERED_EVENTS];
static int BufferedEventsStart = 0, BufferedEventsEnd = 0;
static SDL_mutex * BufferedEventsMutex = NULL;

#if SDL_VERSION_ATLEAST(1,3,0)

#define SDL_SendKeyboardKey(state, keysym) SDL_SendKeyboardKey(state, (keysym)->sym)
extern SDL_Window * ANDROID_CurrentWindow;

#else

#define SDL_SendMouseMotion(A,B,X,Y) SDL_PrivateMouseMotion(0, 0, X, Y)
#define SDL_SendMouseButton(N, A, B) SDL_PrivateMouseButton( A, B, 0, 0 )
#define SDL_SendKeyboardKey(state, keysym) SDL_PrivateKeyboard(state, keysym)

#endif

/* We need our own event queue, because Free Heroes 2 game uses
 * SDL_SetEventFilter(), and it calls SDL_Flip() from inside
 * it's custom filter function, and SDL_Flip() does not work
 * when it's not called from the main() thread.
 * So we, like, push the events into our own queue,
 * read each event from that queue inside SDL_ANDROID_PumpEvents(),
 * unlock the mutex, and push the event to SDL queue,
 * which is then immediately read by SDL from the same thread,
 * and then SDL invokes event filter function from FHeroes2.
 * FHeroes2 call SDL_Flip() from inside that event filter function,
 * and it works, because it is called from the main() thread.
 */
extern void SDL_ANDROID_PumpEvents()
{
	static int oldMouseButtons = 0;
	SDL_Event ev;
	SDL_ANDROID_processAndroidTrackballDampening();
	SDL_ANDROID_processMoveMouseWithKeyboard();
#if SDL_VERSION_ATLEAST(1,3,0)
	SDL_Window * window = SDL_GetFocusWindow();
	if( !window )
		return;
#endif

	if( !BufferedEventsMutex )
		BufferedEventsMutex = SDL_CreateMutex();

	SDL_mutexP(BufferedEventsMutex);
	while( BufferedEventsStart != BufferedEventsEnd )
	{
		ev = BufferedEvents[BufferedEventsStart];
		BufferedEvents[BufferedEventsStart].type = 0;
		BufferedEventsStart++;
		if( BufferedEventsStart >= MAX_BUFFERED_EVENTS )
			BufferedEventsStart = 0;
		SDL_mutexV(BufferedEventsMutex);
		
		switch( ev.type )
		{
			case SDL_MOUSEMOTION:
				SDL_SendMouseMotion( ANDROID_CurrentWindow, 0, ev.motion.x, ev.motion.y );
				break;
			case SDL_MOUSEBUTTONDOWN:
				if( ((oldMouseButtons & SDL_BUTTON(ev.button.button)) != 0) != ev.button.state )
				{
					oldMouseButtons = (oldMouseButtons & ~SDL_BUTTON(ev.button.button)) | (ev.button.state ? SDL_BUTTON(ev.button.button) : 0);
					SDL_SendMouseButton( ANDROID_CurrentWindow, ev.button.state, ev.button.button );
				}
				break;
			case SDL_KEYDOWN:
				//__android_log_print(ANDROID_LOG_INFO, "libSDL", "SDL_KEYDOWN: %i %i", ev->key.keysym.sym, ev->key.state);
				SDL_SendKeyboardKey( ev.key.state, &ev.key.keysym );
				break;
			case SDL_JOYAXISMOTION:
				if( ev.jaxis.which < MAX_MULTITOUCH_POINTERS+1 && SDL_ANDROID_CurrentJoysticks[ev.jaxis.which] )
					SDL_PrivateJoystickAxis( SDL_ANDROID_CurrentJoysticks[ev.jaxis.which], ev.jaxis.axis, ev.jaxis.value );
				break;
			case SDL_JOYBUTTONDOWN:
				if( ev.jbutton.which < MAX_MULTITOUCH_POINTERS+1 && SDL_ANDROID_CurrentJoysticks[ev.jbutton.which] )
					SDL_PrivateJoystickButton( SDL_ANDROID_CurrentJoysticks[ev.jbutton.which], ev.jbutton.button, ev.jbutton.state );
				break;
			case SDL_JOYBALLMOTION:
				if( ev.jball.which < MAX_MULTITOUCH_POINTERS+1 && SDL_ANDROID_CurrentJoysticks[ev.jbutton.which] )
					SDL_PrivateJoystickBall( SDL_ANDROID_CurrentJoysticks[ev.jball.which], ev.jball.ball, ev.jball.xrel, ev.jball.yrel );
				break;
#if SDL_VERSION_ATLEAST(1,3,0)
				//if( ANDROID_CurrentWindow )
				//	SDL_SendWindowEvent(ANDROID_CurrentWindow, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
#else
			case SDL_ACTIVEEVENT:
				SDL_PrivateAppActive(ev.active.gain, ev.active.state);
				break;
#endif
#if SDL_VERSION_ATLEAST(1,3,0)
			case SDL_FINGERMOTION:
				SDL_SendTouchMotion(0, ev.tfinger.fingerId, 0, (float)ev.tfinger.x / (float)window->w, (float)ev.tfinger.y / (float)window->h, ev.tfinger.pressure);
				break;
			case SDL_FINGERDOWN:
				SDL_SendFingerDown(0, ev.tfinger.fingerId, ev.tfinger.state ? 1 : 0, (float)ev.tfinger.x / (float)window->w, (float)ev.tfinger.y / (float)window->h, ev.tfinger.pressure);
				break;
			case SDL_TEXTINPUT:
				SDL_SendKeyboardText(ev.text.text);
				break;
			case SDL_MOUSEWHEEL:
				SDL_SendMouseWheel( ANDROID_CurrentWindow, ev.wheel.x, ev.wheel.y );
				break;
#endif
		}

		SDL_mutexP(BufferedEventsMutex);
	}
	SDL_mutexV(BufferedEventsMutex);
};
// Queue events to main thread
static int getNextEventAndLock()
{
	int nextEvent;
	if( !BufferedEventsMutex )
		return -1;
	SDL_mutexP(BufferedEventsMutex);
	nextEvent = BufferedEventsEnd;
	nextEvent++;
	if( nextEvent >= MAX_BUFFERED_EVENTS )
		nextEvent = 0;
	while( nextEvent == BufferedEventsStart )
	{
		SDL_mutexV(BufferedEventsMutex);
		if( SDL_ANDROID_InsideVideoThread() )
			SDL_ANDROID_PumpEvents();
		else
			SDL_Delay(100);
		SDL_mutexP(BufferedEventsMutex);
		nextEvent = BufferedEventsEnd;
		nextEvent++;
		if( nextEvent >= MAX_BUFFERED_EVENTS )
			nextEvent = 0;
	}
	return nextEvent;
}

static int getPrevEventNoLock()
{
	int prevEvent;
	if(BufferedEventsStart == BufferedEventsEnd)
		return -1;
	prevEvent = BufferedEventsEnd;
	prevEvent--;
	if( prevEvent < 0 )
		prevEvent = MAX_BUFFERED_EVENTS - 1;
	return prevEvent;
}

extern void SDL_ANDROID_MainThreadPushMouseMotion(int x, int y)
{
	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	int prevEvent = getPrevEventNoLock();
	if( prevEvent > 0 && BufferedEvents[prevEvent].type == SDL_MOUSEMOTION )
	{
		// Reuse previous mouse motion event, to prevent mouse movement lag
		BufferedEvents[prevEvent].motion.x = x;
		BufferedEvents[prevEvent].motion.y = y;
	}
	else
	{
		SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
		ev->type = SDL_MOUSEMOTION;
		ev->motion.x = x;
		ev->motion.y = y;
	}
	currentMouseX = x;
	currentMouseY = y;
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
};
extern void SDL_ANDROID_MainThreadPushMouseButton(int pressed, int button)
{
	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	ev->type = SDL_MOUSEBUTTONDOWN;
	ev->button.state = pressed;
	ev->button.button = button;

	if(pressed)
		currentMouseButtons |= SDL_BUTTON(button);
	else
		currentMouseButtons &= ~(SDL_BUTTON(button));
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
};

extern void SDL_ANDROID_MainThreadPushKeyboardKey(int pressed, SDL_scancode key, int unicode)
{
	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	if( moveMouseWithArrowKeys && (
		key == SDL_KEY(UP) || key == SDL_KEY(DOWN) ||
		key == SDL_KEY(LEFT) || key == SDL_KEY(RIGHT) ) )
	{
		if( moveMouseWithKbX < 0 )
		{
			moveMouseWithKbX = currentMouseX;
			moveMouseWithKbY = currentMouseY;
		}

		if( pressed )
		{
			if( key == SDL_KEY(LEFT) )
			{
				if( moveMouseWithKbSpeedX > 0 )
					moveMouseWithKbSpeedX = 0;
				moveMouseWithKbSpeedX -= moveMouseWithKbSpeed;
				moveMouseWithKbAccelX = -moveMouseWithKbAccel;
				moveMouseWithKbAccelUpdateNeeded |= 1;
			}
			else if( key == SDL_KEY(RIGHT) )
			{
				if( moveMouseWithKbSpeedX < 0 )
					moveMouseWithKbSpeedX = 0;
				moveMouseWithKbSpeedX += moveMouseWithKbSpeed;
				moveMouseWithKbAccelX = moveMouseWithKbAccel;
				moveMouseWithKbAccelUpdateNeeded |= 1;
			}

			if( key == SDL_KEY(UP) )
			{
				if( moveMouseWithKbSpeedY > 0 )
					moveMouseWithKbSpeedY = 0;
				moveMouseWithKbSpeedY -= moveMouseWithKbSpeed;
				moveMouseWithKbAccelY = -moveMouseWithKbAccel;
				moveMouseWithKbAccelUpdateNeeded |= 2;
			}
			else if( key == SDL_KEY(DOWN) )
			{
				if( moveMouseWithKbSpeedY < 0 )
					moveMouseWithKbSpeedY = 0;
				moveMouseWithKbSpeedY += moveMouseWithKbSpeed;
				moveMouseWithKbAccelY = moveMouseWithKbAccel;
				moveMouseWithKbAccelUpdateNeeded |= 2;
			}
		}
		else
		{
			if( key == SDL_KEY(LEFT) || key == SDL_KEY(RIGHT) )
			{
				moveMouseWithKbSpeedX = 0;
				moveMouseWithKbAccelX = 0;
				moveMouseWithKbAccelUpdateNeeded &= ~1;
			}
			if( key == SDL_KEY(UP) || key == SDL_KEY(DOWN) )
			{
				moveMouseWithKbSpeedY = 0;
				moveMouseWithKbAccelY = 0;
				moveMouseWithKbAccelUpdateNeeded &= ~2;
			}
		}

		moveMouseWithKbX += moveMouseWithKbSpeedX;
		moveMouseWithKbY += moveMouseWithKbSpeedY;

		SDL_mutexV(BufferedEventsMutex);

		SDL_ANDROID_MainThreadPushMouseMotion(moveMouseWithKbX, moveMouseWithKbY);
		return;
	}

	ev->type = SDL_KEYDOWN;
	ev->key.state = pressed;
	ev->key.keysym.scancode = key;
	ev->key.keysym.sym = key;
	ev->key.keysym.mod = KMOD_NONE;
	ev->key.keysym.unicode = unicode > 0 ? unicode : key;
#if SDL_VERSION_ATLEAST(1,3,0)
#else
	if ( SDL_TranslateUNICODE )
#endif
		ev->key.keysym.unicode = key;

	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
};

extern void SDL_ANDROID_MainThreadPushJoystickAxis(int joy, int axis, int value)
{
	if( ! ( joy < MAX_MULTITOUCH_POINTERS+1 && SDL_ANDROID_CurrentJoysticks[joy] ) )
		return;

	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	ev->type = SDL_JOYAXISMOTION;
	ev->jaxis.which = joy;
	ev->jaxis.axis = axis;
	ev->jaxis.value = MAX( -32768, MIN( 32767, value ) );
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
};
extern void SDL_ANDROID_MainThreadPushJoystickButton(int joy, int button, int pressed)
{
	if( ! ( joy < MAX_MULTITOUCH_POINTERS+1 && SDL_ANDROID_CurrentJoysticks[joy] ) )
		return;

	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	ev->type = SDL_JOYBUTTONDOWN;
	ev->jbutton.which = joy;
	ev->jbutton.button = button;
	ev->jbutton.state = pressed;
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
};
extern void SDL_ANDROID_MainThreadPushJoystickBall(int joy, int ball, int x, int y)
{
	if( ! ( joy < MAX_MULTITOUCH_POINTERS+1 && SDL_ANDROID_CurrentJoysticks[joy] ) )
		return;

	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	ev->type = SDL_JOYBALLMOTION;
	ev->jball.which = joy;
	ev->jball.ball = ball;
	ev->jball.xrel = x;
	ev->jball.yrel = y;
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
}
extern void SDL_ANDROID_MainThreadPushMultitouchButton(int id, int pressed, int x, int y, int force)
{
#if SDL_VERSION_ATLEAST(1,3,0)
	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	ev->type = SDL_FINGERDOWN;
	ev->tfinger.fingerId = id;
	ev->tfinger.state = pressed;
	ev->tfinger.x = x;
	ev->tfinger.y = y;
	ev->tfinger.pressure = force;
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
#endif
};
extern void SDL_ANDROID_MainThreadPushMultitouchMotion(int id, int x, int y, int force)
{
#if SDL_VERSION_ATLEAST(1,3,0)
	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	ev->type = SDL_FINGERMOTION;
	ev->tfinger.fingerId = id;
	ev->tfinger.x = x;
	ev->tfinger.y = y;
	ev->tfinger.pressure = force;
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
#endif
};

extern void SDL_ANDROID_MainThreadPushMouseWheel(int x, int y)
{
#if SDL_VERSION_ATLEAST(1,3,0)
	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	ev->type = SDL_MOUSEWHEEL;
	ev->wheel.x = x;
	ev->wheel.y = y;
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
#endif
}

extern void SDL_ANDROID_MainThreadPushAppActive(int active)
{
#if SDL_VERSION_ATLEAST(1,3,0)
				//if( ANDROID_CurrentWindow )
				//	SDL_SendWindowEvent(ANDROID_CurrentWindow, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
#else
	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
	ev->type = SDL_ACTIVEEVENT;
	ev->active.gain = active;
	ev->active.state = SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;
	
	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
#endif
}


enum { DEFERRED_TEXT_COUNT = 256 };
static struct { int scancode; int unicode; int down; } deferredText[DEFERRED_TEXT_COUNT];
static int deferredTextIdx1 = 0;
static int deferredTextIdx2 = 0;
static SDL_mutex * deferredTextMutex = NULL;

static SDL_keysym asciiToKeysym(int ascii, int unicode)
{
	SDL_keysym keysym;
	keysym.scancode = ascii;
	keysym.sym = ascii;
	keysym.mod = KMOD_NONE;
	keysym.unicode = 0;
#if SDL_VERSION_ATLEAST(1,3,0)
	keysym.sym = SDL_GetScancodeFromKey(ascii);
#else
	if ( SDL_TranslateUNICODE )
#endif
		keysym.unicode = unicode;
	return keysym;
}

static int checkShiftRequired( int * sym )
{
	switch( *sym )
	{
		case '!': *sym = '1'; return 1;
		case '@': *sym = '2'; return 1;
		case '#': *sym = '3'; return 1;
		case '$': *sym = '4'; return 1;
		case '%': *sym = '5'; return 1;
		case '^': *sym = '6'; return 1;
		case '&': *sym = '7'; return 1;
		case '*': *sym = '8'; return 1;
		case '(': *sym = '9'; return 1;
		case ')': *sym = '0'; return 1;
		case '_': *sym = '-'; return 1;
		case '+': *sym = '='; return 1;
		case '|': *sym = '\\';return 1;
		case '<': *sym = ','; return 1;
		case '>': *sym = '.'; return 1;
		case '?': *sym = '/'; return 1;
		case ':': *sym = ';'; return 1;
		case '"': *sym = '\'';return 1;
		case '{': *sym = '['; return 1;
		case '}': *sym = ']'; return 1;
		case '~': *sym = '`'; return 1;
		default: if( *sym >= 'A' && *sym <= 'Z' ) { *sym += 'a' - 'A'; return 1; };
	}
	return 0;
}

void SDL_ANDROID_DeferredTextInput()
{
	if( !deferredTextMutex )
		deferredTextMutex = SDL_CreateMutex();

	SDL_mutexP(deferredTextMutex);
	
	if( deferredTextIdx1 != deferredTextIdx2 )
	{
		int nextEvent = getNextEventAndLock();
		if( nextEvent == -1 )
		{
			SDL_mutexV(deferredTextMutex);
			return;
		}
		SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
		
		deferredTextIdx1++;
		if( deferredTextIdx1 >= DEFERRED_TEXT_COUNT )
			deferredTextIdx1 = 0;
		
		ev->type = SDL_KEYDOWN;
		ev->key.state = deferredText[deferredTextIdx1].down;
		ev->key.keysym = asciiToKeysym( deferredText[deferredTextIdx1].scancode, deferredText[deferredTextIdx1].unicode );
		
		BufferedEventsEnd = nextEvent;
		SDL_mutexV(BufferedEventsMutex);
		if( isMouseUsed )
			SDL_ANDROID_MainThreadPushMouseMotion(currentMouseX + (currentMouseX % 2 ? -1 : 1), currentMouseY); // Force screen redraw
	}
	else
	{
		if( SDL_ANDROID_TextInputFinished )
		{
			SDL_ANDROID_TextInputFinished = 0;
			SDL_ANDROID_IsScreenKeyboardShownFlag = 0;
		}
	}
	
	SDL_mutexV(deferredTextMutex);
};

extern void SDL_ANDROID_MainThreadPushText( int ascii, int unicode )
{
	int shiftRequired;

	int nextEvent = getNextEventAndLock();
	if( nextEvent == -1 )
		return;
	
	SDL_Event * ev = &BufferedEvents[BufferedEventsEnd];
	
#if SDL_VERSION_ATLEAST(1,3,0)

	ev->type = SDL_TEXTINPUT;
	UnicodeToUtf8(unicode, ev->text.text);

#endif

	if( !deferredTextMutex )
		deferredTextMutex = SDL_CreateMutex();

	SDL_mutexP(deferredTextMutex);

	ev->type = 0;
	
	shiftRequired = checkShiftRequired(&ascii);
	
	if( shiftRequired )
	{
		deferredTextIdx2++;
		if( deferredTextIdx2 >= DEFERRED_TEXT_COUNT )
			deferredTextIdx2 = 0;
		deferredText[deferredTextIdx2].down = SDL_PRESSED;
		deferredText[deferredTextIdx2].scancode = SDLK_LSHIFT;
		deferredText[deferredTextIdx2].unicode = unicode;
	}
	deferredTextIdx2++;
	if( deferredTextIdx2 >= DEFERRED_TEXT_COUNT )
		deferredTextIdx2 = 0;
	deferredText[deferredTextIdx2].down = SDL_PRESSED;
	deferredText[deferredTextIdx2].scancode = ascii;
	deferredText[deferredTextIdx2].unicode = unicode;

	deferredTextIdx2++;
	if( deferredTextIdx2 >= DEFERRED_TEXT_COUNT )
		deferredTextIdx2 = 0;
	deferredText[deferredTextIdx2].down = SDL_RELEASED;
	deferredText[deferredTextIdx2].scancode = ascii;
	deferredText[deferredTextIdx2].unicode = unicode;
	if( shiftRequired )
	{
		deferredTextIdx2++;
		if( deferredTextIdx2 >= DEFERRED_TEXT_COUNT )
			deferredTextIdx2 = 0;
		deferredText[deferredTextIdx2].down = SDL_RELEASED;
		deferredText[deferredTextIdx2].scancode = SDLK_LSHIFT;
		deferredText[deferredTextIdx2].unicode = unicode;
	}

	SDL_mutexV(deferredTextMutex);

	BufferedEventsEnd = nextEvent;
	SDL_mutexV(BufferedEventsMutex);
};


Uint32 lastMoveMouseWithKeyboardUpdate = 0;

void SDL_ANDROID_processMoveMouseWithKeyboard()
{
	if( ! moveMouseWithKbAccelUpdateNeeded )
		return;

	Uint32 ticks = SDL_GetTicks();

	if( ticks - lastMoveMouseWithKeyboardUpdate < 20 ) // Update at 50 FPS max, or it will not work properlty on very fast devices
		return;

	lastMoveMouseWithKeyboardUpdate = ticks;

	moveMouseWithKbSpeedX += moveMouseWithKbAccelX;
	moveMouseWithKbSpeedY += moveMouseWithKbAccelY;

	moveMouseWithKbX += moveMouseWithKbSpeedX;
	moveMouseWithKbY += moveMouseWithKbSpeedY;
	SDL_ANDROID_MainThreadPushMouseMotion(moveMouseWithKbX, moveMouseWithKbY);
};

extern void SDL_ANDROID_ProcessDeferredEvents()
{
	SDL_ANDROID_DeferredTextInput();
	ProcessDeferredMouseTap();
};

void ANDROID_InitOSKeymap()
{
#if (SDL_VERSION_ATLEAST(1,3,0))
  SDLKey defaultKeymap[SDL_NUM_SCANCODES];
  SDL_GetDefaultKeymap(defaultKeymap);
  SDL_SetKeymap(0, defaultKeymap, SDL_NUM_SCANCODES);

  SDL_Touch touch;
  memset( &touch, 0, sizeof(touch) );
  touch.x_min = touch.y_min = touch.pressure_min = 0.0f;
  touch.pressure_max = 1000000;
  touch.x_max = SDL_ANDROID_sWindowWidth;
  touch.y_max = SDL_ANDROID_sWindowHeight;

  // These constants are hardcoded inside SDL_touch.c, which makes no sense for me.
  touch.xres = touch.yres = 32768;
  touch.native_xres = touch.native_yres = 32768.0f;

  touch.pressureres = 1;
  touch.native_pressureres = 1.0f;
  touch.id = 0;

  SDL_AddTouch(&touch, "Android touch screen");
#endif
}

JNIEXPORT jint JNICALL 
JAVA_EXPORT_NAME(Settings_nativeGetKeymapKey) (JNIEnv* env, jobject thiz, jint code)
{
	if( code < 0 || code > KEYCODE_LAST )
		return SDL_KEY(UNKNOWN);
	return SDL_android_keymap[code];
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeSetKeymapKey) (JNIEnv* env, jobject thiz, jint javakey, jint key)
{
	if( javakey < 0 || javakey > KEYCODE_LAST )
		return;
	SDL_android_keymap[javakey] = key;
}

JNIEXPORT jint JNICALL
JAVA_EXPORT_NAME(Settings_nativeGetKeymapKeyScreenKb) (JNIEnv* env, jobject thiz, jint keynum)
{
	if( keynum < 0 || keynum > SDL_ANDROID_SCREENKEYBOARD_BUTTON_5 - SDL_ANDROID_SCREENKEYBOARD_BUTTON_0 + 4 )
		return SDL_KEY(UNKNOWN);
		
	if( keynum <= SDL_ANDROID_SCREENKEYBOARD_BUTTON_5 - SDL_ANDROID_SCREENKEYBOARD_BUTTON_0 )
		return SDL_ANDROID_GetScreenKeyboardButtonKey(keynum + SDL_ANDROID_SCREENKEYBOARD_BUTTON_0);

	return SDL_KEY(UNKNOWN);
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetKeymapKeyScreenKb) (JNIEnv* env, jobject thiz, jint keynum, jint key)
{
	if( keynum < 0 || keynum > SDL_ANDROID_SCREENKEYBOARD_BUTTON_5 - SDL_ANDROID_SCREENKEYBOARD_BUTTON_0 + 4 )
		return;
		
	if( keynum <= SDL_ANDROID_SCREENKEYBOARD_BUTTON_5 - SDL_ANDROID_SCREENKEYBOARD_BUTTON_0 )
		SDL_ANDROID_SetScreenKeyboardButtonKey(keynum + SDL_ANDROID_SCREENKEYBOARD_BUTTON_0, key);
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetScreenKbKeyUsed) (JNIEnv*  env, jobject thiz, jint keynum, jint used)
{
	SDL_Rect rect = {0, 0, 0, 0};
	int key = -1;
	if( keynum == 0 )
		key = SDL_ANDROID_SCREENKEYBOARD_BUTTON_DPAD;
	if( keynum == 1 )
		key = SDL_ANDROID_SCREENKEYBOARD_BUTTON_TEXT;
	if( keynum - 2 >= 0 && keynum - 2 <= SDL_ANDROID_SCREENKEYBOARD_BUTTON_5 - SDL_ANDROID_SCREENKEYBOARD_BUTTON_0 )
		key = keynum - 2 + SDL_ANDROID_SCREENKEYBOARD_BUTTON_0;
		
	if( key >= 0 && !used )
		SDL_ANDROID_SetScreenKeyboardButtonPos(key, &rect);
}

JNIEXPORT jint JNICALL
JAVA_EXPORT_NAME(Settings_nativeGetKeymapKeyMultitouchGesture) (JNIEnv* env, jobject thiz, jint keynum)
{
	if( keynum < 0 || keynum >= MAX_MULTITOUCH_GESTURES )
		return SDL_KEY(UNKNOWN);
	return multitouchGestureKeycode[keynum];
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetKeymapKeyMultitouchGesture) (JNIEnv* env, jobject thiz, jint keynum, jint keycode)
{
	if( keynum < 0 || keynum >= MAX_MULTITOUCH_GESTURES )
		return;
	multitouchGestureKeycode[keynum] = keycode;
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetMultitouchGestureSensitivity) (JNIEnv* env, jobject thiz, jint sensitivity)
{
	multitouchGestureSensitivity = sensitivity;
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(Settings_nativeSetTouchscreenCalibration) (JNIEnv* env, jobject thiz, jint x1, jint y1, jint x2, jint y2)
{
	SDL_ANDROID_TouchscreenCalibrationX = x1;
	SDL_ANDROID_TouchscreenCalibrationY = y1;
	SDL_ANDROID_TouchscreenCalibrationWidth = x2 - x1;
	SDL_ANDROID_TouchscreenCalibrationHeight = y2 - y1;
}

JNIEXPORT void JNICALL 
JAVA_EXPORT_NAME(Settings_nativeInitKeymap) ( JNIEnv*  env, jobject thiz )
{
	SDL_android_init_keymap(SDL_android_keymap);
}
