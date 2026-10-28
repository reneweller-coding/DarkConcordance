/**
 * @file Files.h
 * @brief The player's files: their folder (%APPDATA%/SpaceOpera on Windows, $XDG_DATA_HOME or ~/.local/share/SpaceOpera
 *        elsewhere, so Steam can keep it and a reinstall leaves it alone), and writing a file whole or not at all
 *        (into a .tmp beside it, then over it; the one before kept as .bak).
 * @author Rene Weller
 */
#pragma once

#include <cstdio>
#include <string>

namespace spaceopera
{

/**
 * @brief The player's folder, made if missing; the save, the settings, the journal and the play log from beside the
 *        program are moved into it once.
 * @return the folder, with a slash at its end
 */
std::string userDir();

/**
 * @brief Begins writing a file safely.
 * @param[in] path  the file
 * @return a file to write into (nullptr if it cannot be opened)
 */
FILE *beginWrite( const std::string &path );

/**
 * @brief Where the photo mode's pictures go (made if missing): "photos" in the player's folder.
 * @return the folder, with a slash at its end
 */
std::string photoDir();

/**
 * @brief Ends it: closes the file and puts it in place of the old one (kept as .bak).
 * @param[in] f     what beginWrite gave
 * @param[in] path  the file
 * @return whether all of it was written
 */
bool endWrite( FILE *f, const std::string &path );

}   // namespace spaceopera
