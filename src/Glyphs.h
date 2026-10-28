/**
 * @file Glyphs.h
 * @brief The buttons in the texts as pictures of the device in hand: each action (use, jump, the special, the skills
 *        …) is a character of its own in the private use area, `{use}`, `{special}` … in the texts. The font draws it
 *        as the Steam Deck's, an Xbox pad's or a PlayStation pad's button, or as the key or mouse button, whichever
 *        was used last; changing devices only builds the font anew, the texts stay as they are.
 * @author Rene Weller
 */
#pragma once

#include <raylib.h>

#include <string>
#include <vector>

namespace spaceopera
{

/** @brief What the player holds. */
enum class DeviceE
{
    KEYBOARD,      ///< keyboard and mouse
    XBOX,          ///< an Xbox pad (and any pad not known otherwise)
    PLAYSTATION,   ///< a DualShock or DualSense
    DECK           ///< the Steam Deck's own controls
};

/** @brief The actions the texts name (each one character from GlyphBase on). */
enum class ActionE
{
    USE,
    JUMP,
    SPECIAL,
    FIRE,
    STIM,
    SKILL2,
    SKILL3,
    STRAFE,
    CONFIRM,
    BACK,
    PAUSE,
    MAP,
    INVENTORY,
    CHARACTER,
    SALVAGE,
    PAGE,
    CHOOSE,
    CHANGE,
    MOVE,
    CAMERA,
    CAMRESET,
    NEXT,
    DODGE,
    STORE,
    PHOTO,
    COUNT
};

constexpr int GlyphBase = 0xE000;   ///< the first action's character

/** @return the device used last */
DeviceE device();

/**
 * @brief Sets the device (tests: --pad).
 * @param[in] d  the device
 */
void setDevice( DeviceE d );

/**
 * @brief Looks at this frame's input: the device used last becomes the one the texts show.
 * @return true if it changed (the font must be built anew)
 */
bool noteInput();

/**
 * @brief Turns the actions' names in braces into their characters (`{use}` and so on); other braces stay.
 * @param[in] text  a text as written
 * @return the text to draw
 */
std::string actionTokens( const std::string &text );

/**
 * @brief Loads a font with the actions' characters drawn for the device in hand.
 * @param[in] path        the TrueType file
 * @param[in] size        the size to load at
 * @param[in] codepoints  the other characters
 * @return the font (its texture id 0 if the file cannot be read)
 */
Font glyphFont( const char *path, int size, const std::vector<int> &codepoints );

}   // namespace spaceopera
