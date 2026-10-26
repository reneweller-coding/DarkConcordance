/**
 * @file Skills.h
 * @brief What the heroes learn with their skill points: eight skills each, up to five ranks; the passive ones add to
 *        a stat, the active ones sit on a button (B / F, LB / R, RB / G) and have a cooldown.
 * @author Rene Weller
 */
#pragma once

#include "Items.h"

#include <vector>

namespace spaceopera
{

constexpr int MaxRank = 5;

/** @brief A skill. */
struct SkillDef
{
    const char *m_key;        ///< its name's text key (and "<key>.about")
    int         m_hero;       ///< 0 Corvin, 1 Dorra
    int         m_slot;       ///< 0 passive, 1..3 the button of an active one
    int         m_level;      ///< the hero level it can first be learnt at
    StatE       m_stat;       ///< a passive one's stat
    float       m_perRank;    ///< how much per rank
    float       m_cooldown;   ///< an active one's cooldown at rank 1 (seconds)
};

/** @return all skills of both heroes (their index is how they are saved) */
const std::vector<SkillDef> &skillDefs();

/**
 * @brief An active skill of a hero by its button.
 * @param[in] hero  0 Corvin, 1 Dorra
 * @param[in] slot  1..3
 * @return its index, or -1
 */
int activeSkill( int hero, int slot );

}   // namespace spaceopera
