/**
 * @file Bindings.h
 * @brief The player's keys and pad buttons for the play's actions and the inventory's two: one key (or mouse button)
 *        and one pad button each, chosen in the options and kept in the settings. The menus keep their own keys (the
 *        arrows, Enter, Esc; the pad's cross, A, B, Start); walking also goes with the arrows and the left stick.
 * @author Rene Weller
 */
#pragma once

#include <string>

namespace spaceopera
{

/** @brief What can be bound. */
enum class BindE : int
{
    UP,          ///< walk up (on screen)
    DOWN,        ///< walk down
    LEFT,        ///< walk left
    RIGHT,       ///< walk right
    FIRE,        ///< fire / strike (held)
    JUMP,        ///< jump (and talk, use where there is something)
    DODGE,       ///< a dodge roll
    SPECIAL,     ///< the special (Dorra's blast bolt, Corvin's grenade ...)
    SKILL2,      ///< the second skill
    SKILL3,      ///< the third skill
    STIM,        ///< a stim
    USE,         ///< use, talk
    STRAFE,      ///< keep facing (Dorra: the shield up)
    MAP,         ///< the map over the game
    INVENTORY,   ///< the inventory
    CHARACTER,   ///< the character page
    CAMRESET,    ///< the camera back behind the start's view
    PHOTO,       ///< the photo mode
    SALVAGE,     ///< in the inventory: salvage
    STORE,       ///< in the inventory: into the stash and out of it
    COUNT        ///< how many
};

constexpr int MouseLeft = -1;     ///< the left mouse button as a key
constexpr int MouseMiddle = -3;   ///< the middle one (the right one turns the camera and cannot be bound)

/** @brief One action's key (a raylib key, or a mouse button) and pad button (a raylib pad button; 0 none). */
struct Binding
{
    int m_key = 0;
    int m_pad = 0;
};

/**
 * @param[in] b  the action
 * @return its key and button
 */
const Binding &binding( BindE b );

/**
 * @brief Binds an action; another action of the same place (play, or the inventory) that had the key or button gets
 *        this one's old one.
 * @param[in] b     the action
 * @param[in] pad   the pad's button (false: the key)
 * @param[in] code  the key or button
 */
void bind( BindE b, bool pad, int code );

/** @brief Back to the keys and buttons the game comes with. */
void resetBindings();

/** @brief Once a frame, before the input is read: the triggers' states (they may be axes). */
void updateBindings();

/** @return whether the action's key or button is held */
bool held( BindE b );

/** @return whether its key or button went down this frame */
bool pressed( BindE b );

/** @return its name in the settings (the texts call it "bind.<name>") */
const char *bindName( BindE b );

/** @return the line the settings keep for it: "<name> <key> <pad>" */
std::string bindingLine( BindE b );

/**
 * @brief Reads one such line.
 * @param[in] line  "<name> <key> <pad>"
 * @return whether it named an action
 */
bool readBinding( const std::string &line );

/** @return a key's label (a letter, "Leertaste", "Maus links" ...) */
std::string keyLabel( int key );

/** @return a pad button's label (as the pad used last names it: A or Kreuz, LB or L1 ...) */
std::string padLabel( int button );

/** @return a key or mouse button pressed this frame that may be bound (0: none; never Esc, F1, F12) */
int capturedKey();

/** @return a pad button pressed this frame that may be bound (0: none; never Start) */
int capturedPad();

}   // namespace spaceopera
