#include <vector>
#include <stdio.h>

#include "libretro.h"
#include "retrodos.h"

#include "dosbox.h"
#include "mapper.h"
#include "keyboard.h"
#include "mouse.h"
#include "joystick.h"

#define RDEV(x) RETRO_DEVICE_##x
#define RDIX(x) RETRO_DEVICE_INDEX_##x
#define RDID(x) RETRO_DEVICE_ID_##x

struct Processable;
static std::vector<Processable*> inputList;

extern retro_log_printf_t log_cb;
extern bool p1_is_gamepad;
extern bool p2_is_gamepad;

JoystickType p1_joystick_type;
JoystickType p2_joystick_type;

static bool keyboardState[KBD_LAST];

static const struct { unsigned retroID; KBD_KEYS dosboxID; } keyMap[] =
{
    {RETROK_1, KBD_1}, {RETROK_2, KBD_2}, {RETROK_3, KBD_3}, {RETROK_4, KBD_4},
    {RETROK_5, KBD_5}, {RETROK_6, KBD_6}, {RETROK_7, KBD_7}, {RETROK_8, KBD_8},
    {RETROK_9, KBD_9}, {RETROK_0, KBD_0}, {RETROK_a, KBD_a}, {RETROK_b, KBD_b},
    {RETROK_c, KBD_c}, {RETROK_d, KBD_d}, {RETROK_e, KBD_e}, {RETROK_f, KBD_f},
    {RETROK_g, KBD_g}, {RETROK_h, KBD_h}, {RETROK_i, KBD_i}, {RETROK_j, KBD_j},
    {RETROK_k, KBD_k}, {RETROK_l, KBD_l}, {RETROK_m, KBD_m}, {RETROK_n, KBD_n},
    {RETROK_o, KBD_o}, {RETROK_p, KBD_p}, {RETROK_q, KBD_q}, {RETROK_r, KBD_r},
    {RETROK_s, KBD_s}, {RETROK_t, KBD_t}, {RETROK_u, KBD_u}, {RETROK_v, KBD_v},
    {RETROK_w, KBD_w}, {RETROK_x, KBD_x}, {RETROK_y, KBD_y}, {RETROK_z, KBD_z}, //35
    {RETROK_F1, KBD_f1}, {RETROK_F2, KBD_f2}, {RETROK_F3, KBD_f3}, 
    {RETROK_F4, KBD_f4}, {RETROK_F5, KBD_f5}, {RETROK_F6, KBD_f6}, 
    {RETROK_F7, KBD_f7}, {RETROK_F8, KBD_f8}, {RETROK_F9, KBD_f9}, //44
    {RETROK_F10, KBD_f10}, {RETROK_F11, KBD_f11}, {RETROK_F12, KBD_f12}, 
    {RETROK_ESCAPE, KBD_esc}, {RETROK_TAB, KBD_tab}, {RETROK_BACKSPACE, KBD_backspace},
    {RETROK_RETURN, KBD_enter}, {RETROK_SPACE, KBD_space}, {RETROK_LALT, KBD_leftalt}, //53 
    {RETROK_RALT, KBD_rightalt}, {RETROK_LCTRL, KBD_leftctrl}, {RETROK_RCTRL, KBD_rightctrl}, //55
    {RETROK_LSHIFT, KBD_leftshift}, {RETROK_RSHIFT, KBD_rightshift},
    {RETROK_CAPSLOCK, KBD_capslock}, {RETROK_SCROLLOCK, KBD_scrolllock},
    {RETROK_NUMLOCK, KBD_numlock}, {RETROK_MINUS, KBD_minus}, {RETROK_EQUALS, KBD_equals},
    {RETROK_BACKSLASH, KBD_backslash}, {RETROK_LEFTBRACKET, KBD_leftbracket},
    {RETROK_RIGHTBRACKET, KBD_rightbracket}, {RETROK_SEMICOLON, KBD_semicolon},
    {RETROK_QUOTE, KBD_quote}, {RETROK_PERIOD, KBD_period}, {RETROK_COMMA, KBD_comma},
    {RETROK_SLASH, KBD_slash}, {RETROK_SYSREQ, KBD_printscreen}, {RETROK_PAUSE, KBD_pause},
    {RETROK_INSERT, KBD_insert}, {RETROK_HOME, KBD_home}, {RETROK_PAGEUP, KBD_pageup},
    {RETROK_PAGEDOWN, KBD_pagedown}, {RETROK_DELETE, KBD_delete}, {RETROK_END, KBD_end},
    {RETROK_LEFT, KBD_left}, {RETROK_UP, KBD_up}, {RETROK_DOWN, KBD_down}, {RETROK_RIGHT, KBD_right},
    {RETROK_KP1, KBD_kp1}, {RETROK_KP2, KBD_kp2}, {RETROK_KP3, KBD_kp3}, {RETROK_KP4, KBD_kp4}, 
    {RETROK_KP5, KBD_kp5}, {RETROK_KP6, KBD_kp6}, {RETROK_KP7, KBD_kp7}, {RETROK_KP8, KBD_kp8}, 
    {RETROK_KP9, KBD_kp9}, {RETROK_KP0, KBD_kp0}, {RETROK_KP_DIVIDE, KBD_kpdivide},
    {RETROK_KP_MULTIPLY, KBD_kpmultiply}, {RETROK_KP_MINUS, KBD_kpminus},
    {RETROK_KP_PLUS, KBD_kpplus}, {RETROK_KP_ENTER, KBD_kpenter}, {RETROK_KP_PERIOD, KBD_kpperiod},
    {RETROK_BACKQUOTE, KBD_grave}, { 0 }
    /* KBD_extra_lt_gt */
};

static const unsigned eventKeyMap[] =
{
    KBD_f1,KBD_f2,KBD_f3,KBD_f4,KBD_f5,KBD_f6,KBD_f7,KBD_f8,KBD_f9,KBD_f10,KBD_f11,KBD_f12,
    KBD_enter,KBD_kpminus,KBD_scrolllock,KBD_printscreen,KBD_pause,KBD_home
};
static const unsigned eventMOD1 = 55;
static const unsigned eventMOD2 = 53;

///

template<typename T>
struct InputItem
{
    bool down;
    
    InputItem() : down(false) {}
    
    void process(const T& aItem, bool aDownNow)
    {
        if(aDownNow && !down)          aItem.press();
        else if(!aDownNow && down)     aItem.release();
        down = aDownNow;
    }
};

struct Processable
{
    virtual void process() = 0;
};

struct EventHandler : public Processable
{

    MAPPER_Handler* handler;        
    unsigned key;
    unsigned mods;

    InputItem<EventHandler> item;
    
    EventHandler(MAPPER_Handler* handler, MapKeys key, unsigned mods) :
        handler(handler), key(eventKeyMap[key]), mods(mods) { }
    
    void process()
    {
        const uint32_t modsList = keyboardState[eventMOD1] ? 1 : 0 |
                                  keyboardState[eventMOD2] ? 1 : 0;
        item.process(*this, (mods == modsList) && keyboardState[key]);
    }

    void press() const          { handler(true); }
    void release() const        { handler(false); }
};

struct MouseButton : public Processable
{
    unsigned retroButton;
    unsigned dosboxButton;

    InputItem<MouseButton> item;

    MouseButton(unsigned retro, unsigned dosbox) : retroButton(retro), dosboxButton(dosbox) { }

    void process()       { item.process(*this, input_cb(1, RDEV(MOUSE), 0, retroButton)); }
    void press() const   { Mouse_ButtonPressed(dosboxButton); }
    void release() const { Mouse_ButtonReleased(dosboxButton); }
};

struct JoystickButton : public Processable
{
    unsigned retroPort;
    unsigned retroID;
    unsigned dosboxPort;
    unsigned dosboxID;
    
    InputItem<JoystickButton> item;
    
    JoystickButton(unsigned rP, unsigned rID, unsigned dP, unsigned dID) :
        retroPort(rP), retroID(rID), dosboxPort(dP), dosboxID(dID) { }
    
    void process()       { item.process(*this, input_cb(retroPort, RDEV(JOYPAD), 0, retroID)); }
    void press() const   { JOYSTICK_Button(dosboxPort, dosboxID & 1, true);  }
    void release() const { JOYSTICK_Button(dosboxPort, dosboxID & 1, false); }
};

struct JoystickAxis : public Processable
{
    unsigned retroPort;
    unsigned retroSide;
    unsigned retroAxis;
    
    unsigned dosboxPort;
    unsigned dosboxAxis;
    
    JoystickAxis(unsigned rP, unsigned rS, unsigned rA, unsigned dP, unsigned dA) :
        retroPort(rP), retroSide(rS), retroAxis(rA), dosboxPort(dP), dosboxAxis(dA) { }    
    
    void process()
    {
        const float value = (float)input_cb(retroPort, RDEV(ANALOG), retroSide, retroAxis);        

        if(dosboxAxis == 0) JOYSTICK_Move_X(dosboxPort, value / 32768.0f);
        else                JOYSTICK_Move_Y(dosboxPort, value / 32768.0f);
    }
};

struct JoystickHat : public Processable
{
    unsigned retroPort;
    unsigned retroID;
    unsigned dosboxPort;
    unsigned dosboxAxis;    
    
    InputItem<JoystickHat> item;
    
    JoystickHat(unsigned rP, unsigned rID, unsigned dP, unsigned dA) :
        retroPort(rP), retroID(rID), dosboxPort(dP), dosboxAxis(dA) { }
    
    void process()       { item.process(*this, input_cb(retroPort, RDEV(JOYPAD), 0, retroID)); }
    void press() const   
    {   
        if(dosboxAxis==0)
        {
            if(retroID==RETRO_DEVICE_ID_JOYPAD_LEFT)
                JOYSTICK_Move_X(dosboxPort, -32767.0f/32768.0f); 
            if(retroID==RETRO_DEVICE_ID_JOYPAD_RIGHT)
                JOYSTICK_Move_X(dosboxPort, 32767.0f/32768.0f);         
        }
        else
        {  
            if(retroID==RETRO_DEVICE_ID_JOYPAD_UP)
                JOYSTICK_Move_Y(dosboxPort, -32767.0f/32768.0f); 
            if(retroID==RETRO_DEVICE_ID_JOYPAD_DOWN)
                JOYSTICK_Move_Y(dosboxPort, 32767.0f/32768.0f);                 
        }
        
    }
    void release() const 
    {
        if(dosboxAxis==0)
        {
            if(retroID==RETRO_DEVICE_ID_JOYPAD_LEFT)
                JOYSTICK_Move_X(dosboxPort, -0.0f/32768.0f); 
            if(retroID==RETRO_DEVICE_ID_JOYPAD_RIGHT)
                JOYSTICK_Move_X(dosboxPort, 0.0f/32768.0f);            
        }
        else
        {  
            if(retroID==RETRO_DEVICE_ID_JOYPAD_UP)
                JOYSTICK_Move_Y(dosboxPort, -0.0f/32768.0f); 
            if(retroID==RETRO_DEVICE_ID_JOYPAD_DOWN)
                JOYSTICK_Move_Y(dosboxPort, 0.0f/32768.0f);              
        }
    }
};


void keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
    
    for (int i = 0; keyMap[i].retroID; i ++)
    {
        if (keyMap[i].retroID == keycode)
        {
            keyboardState[keyMap[i].dosboxID] = down;
            KEYBOARD_AddKey(keyMap[i].dosboxID, down);
            return;
        }
    }
}

void MAPPER_Init(void)
{
    struct retro_keyboard_callback callback = { keyboard_event };
    environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &callback);

    inputList.clear();

    inputList.push_back(new MouseButton(RDID(MOUSE_LEFT), 0));
    inputList.push_back(new MouseButton(RDID(MOUSE_RIGHT), 1));
    
    if(log_cb)
        log_cb(RETRO_LOG_INFO, "Joystick Type: %d %d \n",p1_joystick_type,p2_joystick_type);
 
    if(p1_joystick_type != JOY_NONE && p2_joystick_type != JOY_NONE)
    {
        joytype=JOY_2AXIS;
    }
    else
    {
        p1_joystick_type != JOY_NONE ? joytype=p1_joystick_type : joytype=p2_joystick_type;
    }

    switch(joytype)
    {
        case JOY_AUTO: case JOY_2AXIS:
        {
            if(p1_joystick_type != JOY_NONE)
            {
                inputList.push_back(new JoystickButton(0, RDID(JOYPAD_Y), 0, 0));
                inputList.push_back(new JoystickButton(0, RDID(JOYPAD_B), 0, 1));
                if(p1_is_gamepad)
                {                
                    inputList.push_back(new JoystickHat(0, RDID(JOYPAD_LEFT), 0, 0));
                    inputList.push_back(new JoystickHat(0, RDID(JOYPAD_RIGHT), 0, 0));
                    inputList.push_back(new JoystickHat(0, RDID(JOYPAD_UP), 0, 1));
                    inputList.push_back(new JoystickHat(0, RDID(JOYPAD_DOWN), 0, 1));
                }
                else
                {
                    inputList.push_back(new JoystickAxis(0, RDIX(ANALOG_LEFT), RDID(ANALOG_X), 0, 0));
                    inputList.push_back(new JoystickAxis(0, RDIX(ANALOG_LEFT), RDID(ANALOG_Y), 0, 1));
                }
                JOYSTICK_Enable(0, true);
            }
            if(p2_joystick_type != JOY_NONE)
            {
                inputList.push_back(new JoystickButton(1, RDID(JOYPAD_Y), 1, 0));
                inputList.push_back(new JoystickButton(1, RDID(JOYPAD_B), 1, 1));
                if(p2_is_gamepad)
                {                
                    inputList.push_back(new JoystickHat(1, RDID(JOYPAD_LEFT), 1, 0));
                    inputList.push_back(new JoystickHat(1, RDID(JOYPAD_RIGHT), 1, 0));
                    inputList.push_back(new JoystickHat(1, RDID(JOYPAD_UP), 1, 1));
                    inputList.push_back(new JoystickHat(1, RDID(JOYPAD_DOWN), 1, 1));
                }
                else
                {
                    inputList.push_back(new JoystickAxis(1, RDIX(ANALOG_LEFT), RDID(ANALOG_X), 0, 0));
                    inputList.push_back(new JoystickAxis(1, RDIX(ANALOG_LEFT), RDID(ANALOG_Y), 0, 1));
                }
                JOYSTICK_Enable(1, true);
            }
            break;
        }
        case JOY_4AXIS: case JOY_4AXIS_2:
        {

            unsigned p = (joytype == JOY_4AXIS) ? 0 : 1;
            inputList.push_back(new JoystickButton(p, RDID(JOYPAD_Y), 0, 0));
            inputList.push_back(new JoystickButton(p, RDID(JOYPAD_X), 0, 1));
            inputList.push_back(new JoystickButton(p, RDID(JOYPAD_B), 1, 0));
            inputList.push_back(new JoystickButton(p, RDID(JOYPAD_A), 1, 1));
            if(p1_is_gamepad || p2_is_gamepad)
            {
                inputList.push_back(new JoystickHat(p, RDID(JOYPAD_LEFT), p, 0));
                inputList.push_back(new JoystickHat(p, RDID(JOYPAD_RIGHT), p, 0));
                inputList.push_back(new JoystickHat(p, RDID(JOYPAD_UP), p, 1));
                inputList.push_back(new JoystickHat(p, RDID(JOYPAD_DOWN), p, 1));            
            }
            else
            {
                inputList.push_back(new JoystickAxis(p, RDIX(ANALOG_LEFT),  RDID(ANALOG_X), 0, 0));
                inputList.push_back(new JoystickAxis(p, RDIX(ANALOG_LEFT),  RDID(ANALOG_Y), 0, 1));
                inputList.push_back(new JoystickAxis(p, RDIX(ANALOG_RIGHT), RDID(ANALOG_X), 1, 0));
                inputList.push_back(new JoystickAxis(p, RDIX(ANALOG_RIGHT), RDID(ANALOG_Y), 1, 1));
            }
            JOYSTICK_Enable(p, true);
            JOYSTICK_Enable(!p, true);
            break;
        }
    }
}

void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key,Bitu mods,char const * const eventname,char const * const buttonname)
{
    inputList.push_back(new EventHandler(handler, key, mods));
}

void MAPPER_Run(bool pressed)
{
    poll_cb();

    // Mouse movement
    const int16_t mouseX = input_cb(1, RDEV(MOUSE), 0, RDID(MOUSE_X));
    const int16_t mouseY = input_cb(1, RDEV(MOUSE), 0, RDID(MOUSE_Y));

    if(mouseX || mouseY)
    {
        Mouse_CursorMoved(mouseX, mouseY, 0, 0, true);
    }

    for (std::vector<Processable*>::iterator i = inputList.begin(); i != inputList.end(); i ++)
        (*i)->process();
}
