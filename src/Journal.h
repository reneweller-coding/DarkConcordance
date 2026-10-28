/**
 * @file Journal.h
 * @brief What the player has done across all games (journal.txt beside the program): counts (kills, finds, deaths,
 *        rolls …), the foes and bosses met, the notes read, the awards won. An award is won by a count reaching its
 *        goal or by a moment in the game; the game shows each new one.
 * @author Rene Weller
 */
#pragma once

#include <string>
#include <vector>

namespace spaceopera
{
namespace journal
{

/** @brief An award: its key (texts award.<key>, award.<key>.about), the count it waits for and its goal. */
struct AwardDef
{
    const char *m_key;
    const char *m_count;   ///< nullptr: a moment in the game wins it
    int         m_goal;
};

/** @return every award, in the order they are listed */
const std::vector<AwardDef> &awards();

/**
 * @brief Reads the journal (once; nothing happens without it).
 * @param[in] path  the file
 */
void load( const std::string &path );

/** @brief Writes it if something changed. */
void flush();

/**
 * @brief Adds to a count (and wins the awards it reaches).
 * @param[in] name  the count
 * @param[in] add   how much
 * @return the count now
 */
int count( const std::string &name, int add = 1 );

/** @return a count as it stands */
int counted( const std::string &name );

/**
 * @brief Wins an award (once).
 * @param[in] key  the award
 */
void win( const std::string &key );

/** @return whether an award is won */
bool won( const std::string &key );

/**
 * @brief Marks something as met: a foe's or a boss's key, a note, a tale.
 * @param[in] tag  what
 * @return true the first time
 */
bool meet( const std::string &tag );

/** @return whether it has been met */
bool met( const std::string &tag );

/**
 * @brief Everything met whose tag begins so (the notes read: "note:").
 * @param[in] prefix  the beginning
 * @return the tags, in order
 */
std::vector<std::string> metWith( const std::string &prefix );

/** @return the awards won since the last call (to show) */
std::vector<std::string> fresh();

}   // namespace journal
}   // namespace spaceopera
