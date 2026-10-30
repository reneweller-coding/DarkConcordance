/**
 * @file Bindings.cpp
 * @brief The player's keys and pad buttons.
 * @author Rene Weller
 */
#include "Bindings.h"

#include "Glyphs.h"
#include "Text.h"

#include <raylib.h>

#include <cstdio>
#include <cstring>

namespace spaceopera
{

namespace
{

const char *const Names[(int)BindE::COUNT] = { "up",        "down",     "left",    "right",   "fire",
                                               "jump",      "dodge",    "special", "skill2",  "skill3",
                                               "stim",      "use",      "strafe",  "map",     "inventory",
                                               "character", "camreset", "photo",   "salvage", "store" };

/** @brief What the game comes with (the pad's triggers are buttons 10 and 12, also read as axes). */
const Binding Defaults[(int)BindE::COUNT] = { { KEY_W, 0 },
                                              { KEY_S, 0 },
                                              { KEY_A, 0 },
                                              { KEY_D, 0 },
                                              { MouseLeft, GAMEPAD_BUTTON_RIGHT_TRIGGER_2 },
                                              { KEY_SPACE, GAMEPAD_BUTTON_RIGHT_FACE_DOWN },
                                              { KEY_LEFT_CONTROL, GAMEPAD_BUTTON_LEFT_THUMB },
                                              { KEY_F, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT },
                                              { KEY_R, GAMEPAD_BUTTON_LEFT_TRIGGER_1 },
                                              { KEY_G, GAMEPAD_BUTTON_RIGHT_TRIGGER_1 },
                                              { KEY_Q, GAMEPAD_BUTTON_RIGHT_FACE_UP },
                                              { KEY_E, GAMEPAD_BUTTON_RIGHT_FACE_LEFT },
                                              { KEY_LEFT_SHIFT, GAMEPAD_BUTTON_LEFT_TRIGGER_2 },
                                              { KEY_M, GAMEPAD_BUTTON_LEFT_FACE_UP },
                                              { KEY_I, GAMEPAD_BUTTON_MIDDLE_LEFT },
                                              { KEY_C, 0 },
                                              { KEY_BACKSPACE, GAMEPAD_BUTTON_RIGHT_THUMB },
                                              { KEY_P, 0 },
                                              { KEY_X, GAMEPAD_BUTTON_RIGHT_FACE_LEFT },
                                              { KEY_F, GAMEPAD_BUTTON_RIGHT_FACE_UP } };

Binding g_bindings[(int)BindE::COUNT] = {};
bool    g_ready = false;
bool    g_trigger[2] = {}, g_wasTrigger[2] = {};   ///< the triggers as buttons: now, the frame before

Binding *table()
{
    if ( !g_ready )
    {
        for ( int i = 0; i < (int)BindE::COUNT; ++i ) g_bindings[i] = Defaults[i];
        g_ready = true;
    }
    return g_bindings;
}

/** @return 0 the play, 1 the inventory: two places whose keys may be the same */
int placeOf( int i )
{
    return i >= (int)BindE::SALVAGE ? 1 : 0;
}

/** @return 0 the left trigger, 1 the right one, -1 another button */
int triggerOf( int button )
{
    return button == GAMEPAD_BUTTON_LEFT_TRIGGER_2 ? 0 : button == GAMEPAD_BUTTON_RIGHT_TRIGGER_2 ? 1 : -1;
}

bool padHeld( int button )
{
    if ( button <= 0 || !IsGamepadAvailable( 0 ) ) return false;
    int t = triggerOf( button );
    return IsGamepadButtonDown( 0, button ) || ( t >= 0 && g_trigger[t] );
}

bool padPressed( int button )
{
    if ( button <= 0 || !IsGamepadAvailable( 0 ) ) return false;
    int t = triggerOf( button );
    return IsGamepadButtonPressed( 0, button ) || ( t >= 0 && g_trigger[t] && !g_wasTrigger[t] );
}

}   // namespace

const Binding &binding( BindE b )
{
    return table()[(int)b];
}

void bind( BindE b, bool pad, int code )
{
    Binding *t = table();
    int      i = (int)b;
    int     &mine = pad ? t[i].m_pad : t[i].m_key;
    for ( int j = 0; j < (int)BindE::COUNT; ++j )
    {
        int &theirs = pad ? t[j].m_pad : t[j].m_key;
        if ( j != i && placeOf( j ) == placeOf( i ) && theirs == code ) theirs = mine;
    }
    mine = code;
}

void completeBindings()
{
    Binding *t = table();
    for ( int i = 0; i < (int)BindE::COUNT; ++i )
    {
        int want = Defaults[i].m_pad;
        if ( t[i].m_pad != 0 || want == 0 ) continue;
        bool taken = false;
        for ( int j = 0; j < (int)BindE::COUNT; ++j )
            taken = taken || ( j != i && placeOf( j ) == placeOf( i ) && t[j].m_pad == want );
        if ( !taken ) t[i].m_pad = want;
    }
}

void resetBindings()
{
    for ( int i = 0; i < (int)BindE::COUNT; ++i ) table()[i] = Defaults[i];
}

void updateBindings()
{
    for ( int t = 0; t < 2; ++t )
    {
        g_wasTrigger[t] = g_trigger[t];
        g_trigger[t] = IsGamepadAvailable( 0 ) &&
                       GetGamepadAxisMovement( 0, t ? GAMEPAD_AXIS_RIGHT_TRIGGER : GAMEPAD_AXIS_LEFT_TRIGGER ) > 0.3f;
    }
}

bool held( BindE b )
{
    const Binding &k = binding( b );
    bool           key = k.m_key < 0 ? IsMouseButtonDown( -k.m_key - 1 ) : k.m_key > 0 && IsKeyDown( k.m_key );
    return key || padHeld( k.m_pad );
}

bool pressed( BindE b )
{
    const Binding &k = binding( b );
    bool           key = k.m_key < 0 ? IsMouseButtonPressed( -k.m_key - 1 ) : k.m_key > 0 && IsKeyPressed( k.m_key );
    return key || padPressed( k.m_pad );
}

const char *bindName( BindE b )
{
    return Names[(int)b];
}

std::string bindingLine( BindE b )
{
    const Binding &k = binding( b );
    return std::string( bindName( b ) ) + " " + std::to_string( k.m_key ) + " " + std::to_string( k.m_pad );
}

bool readBinding( const std::string &line )
{
    char name[32] = {};
    int  key = 0, pad = 0;
    if ( sscanf( line.c_str(), "%31s %d %d", name, &key, &pad ) != 3 ) return false;
    for ( int i = 0; i < (int)BindE::COUNT; ++i )
        if ( !strcmp( name, Names[i] ) )
        {
            table()[i] = { key, pad };
            return true;
        }
    return false;
}

std::string keyLabel( int key )
{
    if ( key == MouseLeft ) return tr( "key.mouse1" );
    if ( key == MouseMiddle ) return tr( "key.mouse3" );
    if ( ( key >= KEY_A && key <= KEY_Z ) || ( key >= KEY_ZERO && key <= KEY_NINE ) )
        return std::string( 1, (char)key );
    if ( key >= KEY_F1 && key <= KEY_F12 ) return "F" + std::to_string( key - KEY_F1 + 1 );
    if ( key >= KEY_KP_0 && key <= KEY_KP_9 ) return "Num " + std::to_string( key - KEY_KP_0 );
    switch ( key )
    {
    case KEY_SPACE: return tr( "key.space" );
    case KEY_LEFT_SHIFT: return tr( "key.shift" );
    case KEY_RIGHT_SHIFT: return std::string( tr( "key.shift" ) ) + " R";
    case KEY_LEFT_CONTROL: return tr( "key.ctrl" );
    case KEY_RIGHT_CONTROL: return std::string( tr( "key.ctrl" ) ) + " R";
    case KEY_LEFT_ALT: return "Alt";
    case KEY_RIGHT_ALT: return "Alt Gr";
    case KEY_TAB: return "Tab";
    case KEY_ENTER: return "Enter";
    case KEY_BACKSPACE: return tr( "key.backspace" );
    case KEY_DELETE: return tr( "key.delete" );
    case KEY_INSERT: return tr( "key.insert" );
    case KEY_HOME: return "Pos1";
    case KEY_END: return tr( "key.end" );
    case KEY_PAGE_UP: return tr( "key.pageup" );
    case KEY_PAGE_DOWN: return tr( "key.pagedown" );
    case KEY_UP: return tr( "key.up" );
    case KEY_DOWN: return tr( "key.down" );
    case KEY_LEFT: return tr( "key.left" );
    case KEY_RIGHT: return tr( "key.right" );
    case KEY_CAPS_LOCK: return "Caps";
    case 0: return "-";
    default: break;
    }
    if ( key > 32 && key < 127 ) return std::string( 1, (char)key );
    return "#" + std::to_string( key );
}

std::string padLabel( int button )
{
    bool ps = device() == DeviceE::PLAYSTATION;
    switch ( button )
    {
    case GAMEPAD_BUTTON_RIGHT_FACE_DOWN: return ps ? tr( "pad.cross" ) : "A";
    case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return ps ? tr( "pad.circle" ) : "B";
    case GAMEPAD_BUTTON_RIGHT_FACE_LEFT: return ps ? tr( "pad.square" ) : "X";
    case GAMEPAD_BUTTON_RIGHT_FACE_UP: return ps ? tr( "pad.triangle" ) : "Y";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_1: return ps ? "L1" : "LB";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_1: return ps ? "R1" : "RB";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_2: return ps ? "L2" : "LT";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_2: return ps ? "R2" : "RT";
    case GAMEPAD_BUTTON_LEFT_THUMB: return "L3";
    case GAMEPAD_BUTTON_RIGHT_THUMB: return "R3";
    case GAMEPAD_BUTTON_MIDDLE_LEFT: return ps ? "Create" : "View";
    case GAMEPAD_BUTTON_MIDDLE_RIGHT: return ps ? "Options" : "Menu";
    case GAMEPAD_BUTTON_MIDDLE: return "Guide";
    case GAMEPAD_BUTTON_LEFT_FACE_UP: return tr( "pad.up" );
    case GAMEPAD_BUTTON_LEFT_FACE_DOWN: return tr( "pad.down" );
    case GAMEPAD_BUTTON_LEFT_FACE_LEFT: return tr( "pad.left" );
    case GAMEPAD_BUTTON_LEFT_FACE_RIGHT: return tr( "pad.right" );
    default: return "-";
    }
}

int capturedKey()
{
    // Scanned rather than taken from the queue (the device's check empties that each frame).
    for ( int k = KEY_SPACE; k <= KEY_KB_MENU; ++k )
        if ( k != KEY_ESCAPE && k != KEY_F1 && k != KEY_F12 && IsKeyPressed( k ) ) return k;
    if ( IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) ) return MouseLeft;
    if ( IsMouseButtonPressed( MOUSE_BUTTON_MIDDLE ) ) return MouseMiddle;
    return 0;
}

int capturedPad()
{
    if ( !IsGamepadAvailable( 0 ) ) return 0;
    for ( int b = GAMEPAD_BUTTON_LEFT_FACE_UP; b <= GAMEPAD_BUTTON_RIGHT_THUMB; ++b )
        if ( b != GAMEPAD_BUTTON_MIDDLE_RIGHT && padPressed( b ) ) return b;
    for ( int t = 0; t < 2; ++t )
        if ( g_trigger[t] && !g_wasTrigger[t] )
            return t ? GAMEPAD_BUTTON_RIGHT_TRIGGER_2 : GAMEPAD_BUTTON_LEFT_TRIGGER_2;
    return 0;
}

}   // namespace spaceopera
