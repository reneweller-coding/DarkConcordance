/**
 * @file PlayLog.h
 * @brief A log of real play for balancing (playlog.txt in the user's folder): each level's start and end, every death
 *        with its place, cause and the foes near, the bosses, the skills and rolls used, the gear worn and salvaged,
 *        and where the hero is every ten seconds. Only a player's game writes it; tests keep nothing.
 * @author Rene Weller
 */
#pragma once

#include <string>

namespace spaceopera
{
namespace playlog
{

/**
 * @brief Opens the log for appending (once; without it nothing is written).
 * @param[in] path  the file
 */
void open( const std::string &path );

/** @return whether the log is open */
bool on();

/**
 * @brief Writes a line: the wall clock, then the text (printf style).
 * @param[in] format  the format
 */
void line( const char *format, ... );

/**
 * @brief The last lines (written or not), oldest first, for a crash report.
 * @param[out] lines  room for `most` pointers (into a static buffer)
 * @param[in]  most   how many at most
 * @return how many
 */
int recent( const char **lines, int most );

/** @return the folder the play log is written in, with a slash at its end ("" when it is not written) */
std::string folder();

}   // namespace playlog
}   // namespace spaceopera
