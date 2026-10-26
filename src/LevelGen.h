/**
 * @file LevelGen.h
 * @brief Generated levels: from a recipe in a level file, a way of rooms joined by corridors from the start to a set
 *        piece at its end, side rooms off the way, each room dressed by its theme, with enemy packs and champions,
 *        lockers, terminals, capsules and lights. The result is an ordinary level file, so the same seed builds the
 *        same level.
 * @author Rene Weller
 */
#pragma once

#include <string>

namespace spaceopera
{

/**
 * @brief Whether a level file is a recipe.
 * @param[in] text  the file
 * @return true if it has a "genrooms" line
 */
bool isRecipe( const std::string &text );

/**
 * @brief Builds a level file from a recipe.
 * @param[in] recipe  the recipe: its gen... lines say what to build, every other line is passed on
 * @param[in] seed    the layout's seed
 * @return the level file with its [map]
 */
std::string generateLevel( const std::string &recipe, unsigned seed );

}   // namespace spaceopera
