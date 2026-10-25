/**
 * @file Text.h
 * @brief The game's texts in the chosen language: assets/text/<language>/*.txt hold "key = text" lines (menus, HUD,
 *        each level's lines, hints and sign plates). German is the source; it fills every gap of another language, and
 *        a key without any text shows itself.
 * @author Rene Weller
 */
#pragma once

#include <string>
#include <vector>

namespace spaceopera
{

/**
 * @brief Loads the texts of a language (German behind it).
 * @param[in] assets    the assets folder
 * @param[in] language  the language's code (a folder in assets/text, e.g. de, en)
 * @return false if the language has no folder (German is loaded then)
 */
bool loadTexts( const std::string &assets, const std::string &language );

/**
 * @brief A text in the loaded language.
 * @param[in] key  its key
 * @return the text, the German one if the language has none, else the key itself
 */
const char *tr( const std::string &key );

/**
 * @brief Whether a key has a text (in the loaded language or in German).
 * @param[in] key  the key
 * @return true if it has one
 */
bool hasText( const std::string &key );

/**
 * @brief A text as a figure says it: "<key>.<who>" if there is one (Dorra and Corvin say some things differently),
 *        else the key's own text.
 * @param[in] key  the key
 * @param[in] who  the figure (dorra, corvin)
 * @return the text
 */
const char *trAs( const std::string &key, const std::string &who );

/** @return the loaded language's code */
const std::string &language();

/**
 * @brief The languages there are.
 * @param[in] assets  the assets folder
 * @return their codes (folders in assets/text), German first
 */
std::vector<std::string> languages( const std::string &assets );

/** @return the language of the system's user interface if there are texts for it, else English (German for German) */
std::string systemLanguage( const std::string &assets );

/** @return every character the loaded texts use (for the font), Latin-1, dashes, quotes and arrows included */
std::vector<int> textCodepoints();

}   // namespace spaceopera
