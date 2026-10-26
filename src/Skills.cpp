/**
 * @file Skills.cpp
 * @brief The heroes' skills as data.
 * @author Rene Weller
 */
#include "Skills.h"

namespace spaceopera
{

namespace
{

constexpr StatE None = StatE::COUNT;

// clang-format off
const std::vector<SkillDef> Skills = {
    // Corvin: the rifle runs cooler and truer; he heals, finds more; overcharge, a grenade, a roll.
    { "skill.cold",       0, 0, 1, StatE::HEAT,     8.0f,  0.0f },
    { "skill.aim",        0, 0, 1, StatE::CRIT,     3.0f,  0.0f },
    { "skill.marksman",   0, 0, 2, StatE::DAMAGE,   7.0f,  0.0f },
    { "skill.nano",       0, 0, 3, StatE::REGEN,    0.5f,  0.0f },
    { "skill.scav",       0, 0, 2, StatE::FIND,    10.0f,  0.0f },
    { "skill.overcharge", 0, 1, 1, None,            0.0f, 18.0f },
    { "skill.grenade",    0, 2, 2, None,            0.0f,  9.0f },
    { "skill.roll",       0, 3, 1, None,            0.0f,  6.0f },
    // Dorra: tougher, harder, armoured, drinking the fight; the bolts, a quake, a charge.
    { "skill.heavy",      1, 0, 1, StatE::HEALTH,  15.0f,  0.0f },
    { "skill.force",      1, 0, 1, StatE::DAMAGE,   8.0f,  0.0f },
    { "skill.iron",       1, 0, 2, StatE::ARMOR,    6.0f,  0.0f },
    { "skill.blood",      1, 0, 3, StatE::LEECH,    2.0f,  0.0f },
    { "skill.wall",       1, 0, 2, StatE::THORNS,  10.0f,  0.0f },
    { "skill.bolts",      1, 1, 1, None,            0.0f,  1.6f },
    { "skill.quake",      1, 2, 2, None,            0.0f,  8.0f },
    { "skill.charge",     1, 3, 3, None,            0.0f,  7.0f },
};
// clang-format on

}   // namespace

const std::vector<SkillDef> &skillDefs()
{
    return Skills;
}

int activeSkill( int hero, int slot )
{
    for ( size_t i = 0; i < Skills.size(); ++i )
        if ( Skills[i].m_hero == hero && Skills[i].m_slot == slot ) return (int)i;
    return -1;
}

}   // namespace spaceopera
