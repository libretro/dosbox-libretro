#include <vector>

#include "libretro.h"

#include "dosbox.h"
#include "mapper.h"
#include "keyboard.h"
#include "mouse.h"
#include "joystick.h"

////////////////////
// RETRO CALLBACKS
////////////////////
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;

void retro_set_input_poll(retro_input_poll_t cb)
{
   poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_cb = cb;
}

//////////
// INPUT
//////////
namespace INPUT
{
    template<typename T>
    struct Item
    {
        bool down;
        
        Item() : down(false) {}
        
        void process(const T& aItem, bool aDownNow)
        {
            if(aDownNow && !down)
            {
                aItem.press();
            }
            else if(!aDownNow && down)
            {
                aItem.release();
            }
            
            down = aDownNow;
        }
    };
}

//////////
// MOUSE
//////////
namespace MOUSE
{
    template<int BUTTON, int I>
    struct Handler
    {
        INPUT::Item<Handler> item;
    
        void process()
        {
            item.process(*this, input_cb(1, RETRO_DEVICE_MOUSE, 0, BUTTON));
        }
    
        void press() const
        {
            Mouse_ButtonPressed(I);
        }
        
        void release() const
        {
            Mouse_ButtonReleased(I);
        }
    };

    static Handler<RETRO_DEVICE_ID_MOUSE_LEFT, 0> left;
    static Handler<RETRO_DEVICE_ID_MOUSE_RIGHT, 1> right;
}

/////////////
// KEYBOARD
/////////////
namespace KEYBOARD
{
    struct Handler
    {    
        unsigned retroID;
        KBD_KEYS dosboxID;
     
        INPUT::Item<Handler> item;     
        
        void process()
        {
            item.process(*this, input_cb(0, RETRO_DEVICE_KEYBOARD, 0, retroID));
        }
        
        void press() const
        {
            KEYBOARD_AddKey(dosboxID, true);
        }
        
        void release() const
        {
            KEYBOARD_AddKey(dosboxID, false);
        }
    };
    
    static Handler map[] =
    {
        {RETROK_1, KBD_1}, {RETROK_2, KBD_2}, {RETROK_3, KBD_3}, {RETROK_4, KBD_4},
        {RETROK_5, KBD_5}, {RETROK_6, KBD_6}, {RETROK_7, KBD_7}, {RETROK_8, KBD_8},
        {RETROK_9, KBD_9}, {RETROK_0, KBD_0}, {RETROK_a, KBD_a}, {RETROK_b, KBD_b},
        {RETROK_c, KBD_c}, {RETROK_d, KBD_d}, {RETROK_e, KBD_e}, {RETROK_f, KBD_f},
        {RETROK_g, KBD_g}, {RETROK_h, KBD_h}, {RETROK_i, KBD_i}, {RETROK_j, KBD_j},
        {RETROK_k, KBD_k}, {RETROK_l, KBD_l}, {RETROK_m, KBD_m}, {RETROK_n, KBD_n},
        {RETROK_o, KBD_o}, {RETROK_p, KBD_p}, {RETROK_q, KBD_q}, {RETROK_r, KBD_r},
        {RETROK_s, KBD_s}, {RETROK_t, KBD_t}, {RETROK_u, KBD_u}, {RETROK_v, KBD_v},
        {RETROK_w, KBD_w}, {RETROK_x, KBD_x}, {RETROK_y, KBD_y}, {RETROK_z, KBD_z},
        {RETROK_F1, KBD_f1}, {RETROK_F2, KBD_f2}, {RETROK_F3, KBD_f3}, 
        {RETROK_F4, KBD_f4}, {RETROK_F5, KBD_f5}, {RETROK_F6, KBD_f6}, 
        {RETROK_F7, KBD_f7}, {RETROK_F8, KBD_f8}, {RETROK_F9, KBD_f9}, 
        {RETROK_F10, KBD_f10}, {RETROK_F11, KBD_f11}, {RETROK_F12, KBD_f12}, 
        {RETROK_ESCAPE, KBD_esc}, {RETROK_TAB, KBD_tab}, {RETROK_BACKSPACE, KBD_backspace},
        {RETROK_RETURN, KBD_enter}, {RETROK_SPACE, KBD_space}, {RETROK_LALT, KBD_leftalt},
        {RETROK_RALT, KBD_rightalt}, {RETROK_LCTRL, KBD_leftctrl}, {RETROK_RCTRL, KBD_rightctrl},
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
        {RETROK_KP_PLUS, KBD_kpplus}, {RETROK_KP_ENTER, KBD_kpenter}, {RETROK_KP_PERIOD, KBD_kpperiod}
    };
    
    static const unsigned count = sizeof(map) / sizeof(map[0]);
};

/////////////
// JOYSTICK
/////////////
/*namespace JOYSTICK
{
    struct ButtonHandler
    {
        unsigned retroPort;
        unsigned retroID;
        unsigned dosboxPort;
        unsigned dosboxID;
    
        INPUT::Item<ButtonHandler> item;
        
        void process()
        {
            item.process(*this, input_cb(retroPort, RETRO_DEVICE_JOYPAD, 0, retroID));
        }
        
        void press() const
        {
            JOYSTICK_Button(dosboxPort, dosboxID & 1, true);
        }

        void release() const
        {
            JOYSTICK_Button(dosboxPort, dosboxID & 1, false);
        }
    };
    
    struct AxisHandler
    {
        unsigned retroPort;
        unsigned retroSide;
        unsigned retroAxis;
        
        unsigned dosboxPort;
        unsigned dosboxAxis;
        
        void process()
        {
            const float value = (float)input_cb(retroPort, RETRO_DEVICE_ANALOG, retroSide, retroAxis);        
        
            if(dosboxAxis == 0)
            {
                JOYSTICK_Move_X(dosboxPort, value / 32768.0f);
            }
            else
            {
                JOYSTICK_Move_Y(dosboxPort, value / 32768.0f);
            }
        }
    };
    
    struct Handler
    {
        bool ports[2];
    
        unsigned buttonCount;
        ButtonHandler button[4]; 
        
        unsigned axisCount;
        AxisHandler axis[4];
        
        void enable()
        {
            for(int i = 0; i != 2; i ++)
            {
                JOYSTICK_Enable(i, ports[i]);
            }
        }
        
        void process()
        {
            for(int i = 0; i != buttonCount; i ++)
            {
                button[i].process();
            }
            
            for(int i = 0; i != axisCount; i ++)
            {
                axis[i].process();
            }
        }
    };

    static Handler JSNone = 
    {
        {false, false},
        0, {}, 0, {}
    };

    static Handler JS2Axis =
    {
        {true, true},
        4, {
            {0, RETRO_DEVICE_ID_JOYPAD_Y, 0, 0},
            {0, RETRO_DEVICE_ID_JOYPAD_B, 0, 1},
            {1, RETRO_DEVICE_ID_JOYPAD_Y, 1, 0},
            {1, RETRO_DEVICE_ID_JOYPAD_B, 1, 1}
        },
        4, {
            {0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, 0, 0},
            {0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, 0, 1},
            {1, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, 1, 0},
            {1, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, 1, 1}
        }
    };
    
    static Handler JS4Axis =
    {
        {true, false},
        4, {
            {0, RETRO_DEVICE_ID_JOYPAD_Y, 0, 0},
            {0, RETRO_DEVICE_ID_JOYPAD_B, 0, 1},
            {0, RETRO_DEVICE_ID_JOYPAD_X, 1, 0},
            {0, RETRO_DEVICE_ID_JOYPAD_A, 1, 1}
        },
        4, {
            {0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, 0, 0},
            {0, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, 0, 1},
            {0, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, 1, 0},
            {0, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, 1, 1}
        }
    };

    static Handler JS4Axis2 =
    {
        {false, true},
        4, {
            {1, RETRO_DEVICE_ID_JOYPAD_Y, 0, 0},
            {1, RETRO_DEVICE_ID_JOYPAD_B, 0, 1},
            {1, RETRO_DEVICE_ID_JOYPAD_X, 1, 0},
            {1, RETRO_DEVICE_ID_JOYPAD_A, 1, 1}
        },
        4, {
            {1, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, 0, 0},
            {1, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, 0, 1},
            {1, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, 1, 0},
            {1, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, 1, 1}
        }
    };
    
    static Handler* Joystick;
}*/

//////////
// EVENT
//////////
namespace EVENT
{
    //MapKeys->KBD Map: These are indices into the KEYBOARD::map, if it changes these need to be changed too
    const unsigned mapKeyMap[] = {36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 51, 96, 60, 72, 73, 75};

    struct Handler
    {
        MAPPER_Handler* handler;
        
        unsigned keys[3];
        unsigned keyCount;
    
        bool down;
        
        Handler(MAPPER_Handler* handler,MapKeys key,Bitu mods,char const * const eventname,char const * const buttonname) :
            handler(handler), keyCount(0), down(false)
        {
            keys[keyCount ++] = mapKeyMap[key];
    
            if(mods & MMOD1)
            {
                keys[keyCount ++] = 55;
            }
            
            if(mods & MMOD2)
            {
                keys[keyCount ++] = 53;
            }
        }
        
        void process()
        {
            // Check keys
            bool isDownNow = true;
            
            for(int i = 0; i != keyCount; i ++)
            {
                if(!KEYBOARD::map[keys[i]].item.down)
                {
                    isDownNow = false;
                    break;
                }
            }
            
            // Update event
            if(isDownNow && !down)
            {
                handler(true);
            }
            else if(!isDownNow && down)
            {
                handler(false);
            }
            
            down = isDownNow;
        }
    };
    
    static std::vector<Handler> list;
}

/////

void MAPPER_Init(void)
{
    // TODO: Select JS Type
}

void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key,Bitu mods,char const * const eventname,char const * const buttonname)
{
    EVENT::list.push_back(EVENT::Handler(handler, key, mods, eventname, buttonname));
}

void MAPPER_Run(bool pressed)
{
    poll_cb();

    // Mouse movement
    const int16_t mouseX = input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
    const int16_t mouseY = input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

    if(mouseX || mouseY)
    {
        Mouse_CursorMoved(mouseX, mouseY, 0, 0, true);
    }

    // Mouse Buttons
    MOUSE::left.process();
    MOUSE::right.process();

    // Keyboard
    for(int i = 0; i != KEYBOARD::count; i ++)
    {
        KEYBOARD::map[i].process();
    }

    // Joystick
//    if(JOYSTICK::Joystick) 
//    {
//        JOYSTICK::Joystick->process();
//    }

    // Run Events
    for(std::vector<EVENT::Handler>::iterator i = EVENT::list.begin(); i != EVENT::list.end(); i ++)
    {
        i->process();
    }
}