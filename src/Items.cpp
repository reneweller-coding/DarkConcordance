/**
 * @file Items.cpp
 * @brief The kinds of items, the affixes and how strong they roll, the uniques; making, naming, pricing, saving.
 * @author Rene Weller
 */
#include "Items.h"

#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace spaceopera
{

namespace
{

constexpr StatE None = StatE::COUNT;

// clang-format off
const std::vector<ItemBase> Bases = {
    //  key                    slot           hero lvl  dmg   int    heat  pel spread pierce splash range chain armor bonus           value
    { "item.rifle",           SlotE::WEAPON,  0,   1,  20.f, 0.16f,  7.5f, 1, 0.012f, 0, 0.0f, 30.f, 0, 0.f, None,            0.f },
    { "item.carbine",         SlotE::WEAPON,  0,   1,  11.f, 0.085f, 4.2f, 1, 0.035f, 0, 0.0f, 24.f, 0, 0.f, None,            0.f },
    { "item.scatter",         SlotE::WEAPON,  0,   2,   8.f, 0.5f,  15.f,  7, 0.14f,  0, 0.0f, 13.f, 0, 0.f, None,            0.f },
    { "item.rail",            SlotE::WEAPON,  0,   4,  75.f, 0.85f, 28.f,  1, 0.0f,   3, 0.0f, 42.f, 0, 0.f, None,            0.f },
    { "item.plasma",          SlotE::WEAPON,  0,   5,  34.f, 0.42f, 13.f,  1, 0.02f,  0, 2.4f, 26.f, 0, 0.f, StatE::BURN,    15.f },
    { "item.arc",             SlotE::WEAPON,  0,   7,  15.f, 0.18f,  7.f,  1, 0.02f,  0, 0.0f, 18.f, 2, 0.f, StatE::SHOCK,   10.f },
    { "item.hammer",          SlotE::WEAPON,  1,   1,  30.f, 1.0f,   0.f,  1, 0.f,    0, 0.0f, 2.0f, 0, 0.f, None,            0.f },
    { "item.axe",             SlotE::WEAPON,  1,   1,  21.f, 0.72f,  0.f,  1, 0.f,    0, 0.0f, 1.9f, 0, 0.f, StatE::BURN,    15.f },
    { "item.pick",            SlotE::WEAPON,  1,   3,  42.f, 1.2f,   0.f,  1, 0.f,    1, 0.0f, 2.1f, 0, 0.f, None,            0.f },
    { "item.maul",            SlotE::WEAPON,  1,   4,  33.f, 1.05f,  0.f,  1, 0.f,    0, 0.0f, 2.2f, 0, 0.f, StatE::SHOCK,   20.f },
    { "item.breaker",         SlotE::WEAPON,  1,   6,  46.f, 1.25f,  0.f,  1, 0.f,    0, 1.8f, 2.1f, 0, 0.f, None,            0.f },
    { "item.vest",            SlotE::ARMOR,  -1,   1,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 8.f, None,            0.f },
    { "item.plate",           SlotE::ARMOR,  -1,   3,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 16.f, StatE::SPEED,  -5.f },
    { "item.exo",             SlotE::ARMOR,  -1,   6,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 22.f, StatE::SPEED,   6.f },
    { "item.helm",            SlotE::HELMET, -1,   1,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 4.f, None,            0.f },
    { "item.visor",           SlotE::HELMET, -1,   3,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 3.f, StatE::CRIT,     4.f },
    { "item.rebreather",      SlotE::HELMET, -1,   5,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 4.f, StatE::REGEN,    0.6f },
    { "item.gloves",          SlotE::GLOVES, -1,   1,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 3.f, None,            0.f },
    { "item.grips",           SlotE::GLOVES, -1,   3,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 3.f, StatE::RATE,     5.f },
    { "item.boots",           SlotE::BOOTS,  -1,   1,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 3.f, None,            0.f },
    { "item.magboots",        SlotE::BOOTS,  -1,   3,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 4.f, StatE::SPEED,    5.f },
    { "item.implant.reflex",  SlotE::IMPLANT,-1,   2,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::CRIT,     5.f },
    { "item.implant.cardio",  SlotE::IMPLANT,-1,   2,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::REGEN,    0.8f },
    { "item.implant.cooler",  SlotE::IMPLANT,-1,   2,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::COOLDOWN, 8.f },
    { "item.implant.scav",    SlotE::IMPLANT,-1,   3,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::FIND,    15.f },
    { "item.implant.adren",   SlotE::IMPLANT,-1,   5,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::DAMAGE,   8.f },
    { "item.implant.stim",    SlotE::IMPLANT,-1,   4,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::STIMS,   30.f },
    // The second tier: stronger kinds from item level 7 on (the market, the depths).
    { "item.rifle2",          SlotE::WEAPON,  0,   9,  30.f, 0.15f,  7.0f, 1, 0.010f, 0, 0.0f, 32.f, 0, 0.f, None,            0.f },
    { "item.carbine2",        SlotE::WEAPON,  0,  10,  16.f, 0.075f, 4.0f, 1, 0.03f,  0, 0.0f, 26.f, 0, 0.f, None,            0.f },
    { "item.scatter2",        SlotE::WEAPON,  0,  11,  11.f, 0.48f, 14.f,  9, 0.13f,  0, 0.0f, 14.f, 0, 0.f, None,            0.f },
    { "item.rail2",           SlotE::WEAPON,  0,  13, 110.f, 0.8f,  26.f,  1, 0.0f,   4, 0.0f, 45.f, 0, 0.f, None,            0.f },
    { "item.plasma2",         SlotE::WEAPON,  0,  14,  48.f, 0.4f,  12.f,  1, 0.02f,  0, 3.0f, 28.f, 0, 0.f, StatE::BURN,    20.f },
    { "item.arc2",            SlotE::WEAPON,  0,  15,  22.f, 0.17f,  6.5f, 1, 0.02f,  0, 0.0f, 20.f, 3, 0.f, StatE::SHOCK,   15.f },
    { "item.hammer2",         SlotE::WEAPON,  1,   9,  44.f, 1.0f,   0.f,  1, 0.f,    0, 0.0f, 2.1f, 0, 0.f, None,            0.f },
    { "item.axe2",            SlotE::WEAPON,  1,  10,  31.f, 0.7f,   0.f,  1, 0.f,    0, 0.0f, 2.0f, 0, 0.f, StatE::BURN,    20.f },
    { "item.pick2",           SlotE::WEAPON,  1,  11,  62.f, 1.15f,  0.f,  1, 0.f,    1, 0.0f, 2.2f, 0, 0.f, None,            0.f },
    { "item.maul2",           SlotE::WEAPON,  1,  13,  48.f, 1.0f,   0.f,  1, 0.f,    0, 0.0f, 2.3f, 0, 0.f, StatE::SHOCK,   25.f },
    { "item.breaker2",        SlotE::WEAPON,  1,  15,  66.f, 1.2f,   0.f,  1, 0.f,    0, 2.2f, 2.2f, 0, 0.f, None,            0.f },
    { "item.vest2",           SlotE::ARMOR,  -1,   9,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 14.f, None,           0.f },
    { "item.plate2",          SlotE::ARMOR,  -1,  11,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 26.f, StatE::SPEED,  -5.f },
    { "item.exo2",            SlotE::ARMOR,  -1,  14,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 34.f, StatE::SPEED,   8.f },
    { "item.helm2",           SlotE::HELMET, -1,   9,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 7.f, None,            0.f },
    { "item.visor2",          SlotE::HELMET, -1,  11,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 5.f, StatE::CRIT,     7.f },
    { "item.gloves2",         SlotE::GLOVES, -1,   9,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 6.f, None,            0.f },
    { "item.grips2",          SlotE::GLOVES, -1,  11,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 5.f, StatE::RATE,     8.f },
    { "item.boots2",          SlotE::BOOTS,  -1,   9,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 6.f, None,            0.f },
    { "item.magboots2",       SlotE::BOOTS,  -1,  11,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 7.f, StatE::SPEED,    8.f },
    { "item.implant.focus",   SlotE::IMPLANT,-1,   8,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::CRIT_DAMAGE, 25.f },
    { "item.implant.leech",   SlotE::IMPLANT,-1,   9,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::LEECH,    3.f },
    { "item.implant.ward",    SlotE::IMPLANT,-1,   7,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::THORNS,  25.f },
};
// clang-format on

/** @brief An affix's range: at item level 1 and at 30. */
struct Range
{
    const char *m_name;
    float       m_lo, m_hi;
};
const Range Ranges[(int)StatE::COUNT] = { { "damage", 5.f, 45.f },   { "rate", 3.f, 20.f },   { "crit", 2.f, 12.f },
                                          { "critdmg", 10.f, 70.f }, { "burn", 5.f, 30.f },   { "shock", 4.f, 22.f },
                                          { "leech", 1.f, 7.f },     { "health", 8.f, 90.f }, { "armor", 3.f, 30.f },
                                          { "regen", 0.2f, 2.5f },   { "speed", 3.f, 15.f },  { "heat", 5.f, 30.f },
                                          { "cooldown", 4.f, 22.f }, { "find", 5.f, 45.f },   { "xp", 3.f, 15.f },
                                          { "thorns", 4.f, 60.f },   { "stims", 10.f, 60.f } };

/** @brief Which affixes a slot can roll. */
std::vector<StatE> allowed( SlotE slot, int hero )
{
    using S = StatE;
    switch ( slot )
    {
    case SlotE::WEAPON:
    {
        std::vector<StatE> v = { S::DAMAGE, S::RATE, S::CRIT, S::CRIT_DAMAGE, S::BURN, S::SHOCK, S::LEECH, S::XP };
        if ( hero == 0 ) v.push_back( S::HEAT );
        return v;
    }
    case SlotE::ARMOR: return { S::HEALTH, S::ARMOR, S::REGEN, S::THORNS, S::FIND, S::STIMS, S::COOLDOWN };
    case SlotE::HELMET: return { S::HEALTH, S::ARMOR, S::CRIT, S::XP, S::FIND, S::COOLDOWN, S::REGEN };
    case SlotE::GLOVES: return { S::RATE, S::CRIT, S::CRIT_DAMAGE, S::LEECH, S::ARMOR, S::DAMAGE };
    case SlotE::BOOTS: return { S::SPEED, S::ARMOR, S::HEALTH, S::FIND, S::REGEN };
    default:
        return { S::DAMAGE, S::RATE,     S::CRIT, S::CRIT_DAMAGE, S::LEECH, S::HEALTH,
                 S::REGEN,  S::COOLDOWN, S::FIND, S::XP,          S::STIMS };
    }
}

/** @brief A unique: its kind, the first level it drops at, its affixes (they grow a little past that level). */
struct Unique
{
    int                m_base;
    int                m_level;
    std::vector<Affix> m_affixes;
};
const std::vector<Unique> Uniques = {
    { 3, 6, { { StatE::DAMAGE, 40.f }, { StatE::CRIT, 10.f }, { StatE::CRIT_DAMAGE, 60.f } } },
    { 4, 7, { { StatE::DAMAGE, 30.f }, { StatE::BURN, 35.f }, { StatE::LEECH, 3.f } } },
    { 25, 4, { { StatE::DAMAGE, 15.f }, { StatE::HEALTH, 40.f }, { StatE::STIMS, 50.f } } },
    { 6, 5, { { StatE::DAMAGE, 45.f }, { StatE::SHOCK, 25.f }, { StatE::THORNS, 30.f } } },
    { 13, 8, { { StatE::ARMOR, 30.f }, { StatE::HEALTH, 80.f }, { StatE::REGEN, 2.f } } },
    { 15, 4, { { StatE::CRIT, 10.f }, { StatE::XP, 12.f }, { StatE::FIND, 30.f } } },
    { 1, 3, { { StatE::RATE, 25.f }, { StatE::CRIT, 8.f }, { StatE::LEECH, 4.f } } },
    { 7, 3, { { StatE::RATE, 20.f }, { StatE::BURN, 30.f }, { StatE::LEECH, 5.f } } },
    { 27, 10, { { StatE::DAMAGE, 35.f }, { StatE::CRIT, 8.f }, { StatE::XP, 10.f } } },
    { 30, 14, { { StatE::DAMAGE, 50.f }, { StatE::CRIT_DAMAGE, 80.f }, { StatE::RATE, 10.f } } },
    { 29, 12, { { StatE::DAMAGE, 30.f }, { StatE::SHOCK, 20.f }, { StatE::LEECH, 3.f } } },
    { 33, 10, { { StatE::DAMAGE, 40.f }, { StatE::HEALTH, 50.f }, { StatE::FIND, 25.f } } },
    { 37, 15, { { StatE::DAMAGE, 45.f }, { StatE::SHOCK, 25.f }, { StatE::THORNS, 40.f } } },
    { 40, 14, { { StatE::ARMOR, 30.f }, { StatE::HEALTH, 90.f }, { StatE::SPEED, 8.f } } },
    { 44, 11, { { StatE::RATE, 18.f }, { StatE::CRIT, 8.f }, { StatE::LEECH, 3.f } } },
    { 46, 11, { { StatE::SPEED, 15.f }, { StatE::REGEN, 1.5f }, { StatE::FIND, 25.f } } },
    { 47, 9, { { StatE::CRIT, 10.f }, { StatE::CRIT_DAMAGE, 40.f }, { StatE::XP, 8.f } } },
    { 34, 11, { { StatE::DAMAGE, 35.f }, { StatE::BURN, 35.f }, { StatE::LEECH, 5.f } } },
};
constexpr int RareNames = 24;

float roll( unsigned &seed )
{
    seed = seed * 1664525u + 1013904223u;
    return ( seed >> 8 ) * ( 1.0f / 16777216.0f );
}

}   // namespace

const std::vector<ItemBase> &itemBases()
{
    return Bases;
}

Item makeItem( int level, int hero, unsigned &seed, float luck, int slot )
{
    Item it;
    it.m_level = std::max( 1, level );
    // A unique now and then, if one fits.
    float u = roll( seed );
    if ( u < 0.012f * luck )
    {
        std::vector<int> fit;
        for ( size_t i = 0; i < Uniques.size(); ++i )
        {
            const ItemBase &b = Bases[Uniques[i].m_base];
            if ( Uniques[i].m_level <= level && ( b.m_hero < 0 || b.m_hero == hero ) &&
                 ( slot < 0 || (int)b.m_slot == slot ) )
                fit.push_back( (int)i );
        }
        if ( !fit.empty() )
        {
            int k = fit[(size_t)( roll( seed ) * fit.size() ) % fit.size()];
            it.m_base = Uniques[k].m_base;
            it.m_rarity = RarityE::UNIQUE;
            it.m_name = k;
            float grow = 1.0f + 0.02f * std::max( 0, level - Uniques[k].m_level );
            for ( Affix a : Uniques[k].m_affixes )
            {
                a.m_value *= grow;
                it.m_affixes.push_back( a );
            }
            return it;
        }
    }
    // A kind for this hero and level (slots weighted: weapons and armour more often).
    std::vector<int> kinds;
    for ( size_t i = 0; i < Bases.size(); ++i )
    {
        const ItemBase &b = Bases[i];
        if ( b.m_level > level || ( b.m_hero >= 0 && b.m_hero != hero ) || ( slot >= 0 && (int)b.m_slot != slot ) )
            continue;
        int weight = b.m_slot == SlotE::WEAPON ? 3 : b.m_slot == SlotE::ARMOR ? 2 : 1;
        for ( int w = 0; w < weight; ++w ) kinds.push_back( (int)i );
    }
    if ( kinds.empty() ) kinds.push_back( hero == 1 ? 6 : 0 );
    it.m_base = kinds[(size_t)( roll( seed ) * kinds.size() ) % kinds.size()];
    float r = roll( seed );
    int   count = 0;
    if ( r < 0.09f * luck )
    {
        it.m_rarity = RarityE::RARE;
        count = 3 + ( roll( seed ) < 0.4f ? 1 : 0 );
        it.m_name = (int)( roll( seed ) * RareNames ) % RareNames;
    }
    else if ( r < ( 0.09f + 0.32f ) * luck )
    {
        it.m_rarity = RarityE::MAGIC;
        count = 1 + ( roll( seed ) < 0.45f ? 1 : 0 );
    }
    // Affixes: different stats, each stronger the higher the item's level.
    std::vector<StatE> pool = allowed( Bases[it.m_base].m_slot, hero );
    float              t = Clamp( ( level - 1 ) / 29.0f, 0.0f, 1.0f );
    for ( int i = 0; i < count && !pool.empty(); ++i )
    {
        size_t k = (size_t)( roll( seed ) * pool.size() ) % pool.size();
        StatE  s = pool[k];
        pool.erase( pool.begin() + (long)k );
        const Range &g = Ranges[(int)s];
        float        v = Lerp( g.m_lo, g.m_hi, t ) * ( 0.65f + 0.35f * roll( seed ) );
        if ( it.m_rarity == RarityE::RARE ) v *= 1.15f;
        it.m_affixes.push_back( { s, s == StatE::REGEN ? roundf( v * 10.0f ) / 10.0f : roundf( v ) } );
    }
    return it;
}

Item startItem( int hero, SlotE slot )
{
    Item it;
    it.m_base = slot == SlotE::WEAPON ? ( hero == 1 ? 6 : 0 ) : 11;
    return it;
}

std::string itemName( const Item &it )
{
    const ItemBase &b = Bases[it.m_base];
    switch ( it.m_rarity )
    {
    case RarityE::UNIQUE: return tr( TextFormat( "unique.%d", it.m_name + 1 ) );
    case RarityE::RARE:
        return std::string( tr( b.m_key ) ) + " " + tr( "item.quote.open" ) +
               tr( TextFormat( "rare.%d", it.m_name + 1 ) ) + tr( "item.quote.close" );
    case RarityE::MAGIC:
        if ( !it.m_affixes.empty() )
            return std::string( tr( b.m_key ) ) + " " +
                   tr( std::string( "suffix." ) + Ranges[(int)it.m_affixes[0].m_stat].m_name );
        return tr( b.m_key );
    default: return tr( b.m_key );
    }
}

Color rarityColor( RarityE r )
{
    switch ( r )
    {
    case RarityE::MAGIC: return { 120, 160, 255, 255 };
    case RarityE::RARE: return { 255, 225, 90, 255 };
    case RarityE::UNIQUE: return { 255, 150, 50, 255 };
    default: return { 225, 222, 215, 255 };
    }
}

int itemValue( const Item &it )
{
    const float mul[4] = { 1.0f, 2.5f, 6.0f, 12.0f };
    return (int)( ( 8 + 5 * it.m_level ) * mul[(int)it.m_rarity] );
}

float itemDamage( const Item &it )
{
    return Bases[it.m_base].m_damage * ( 1.0f + 0.12f * ( it.m_level - 1 ) );
}

float itemArmor( const Item &it )
{
    return Bases[it.m_base].m_armor * ( 1.0f + 0.1f * ( it.m_level - 1 ) );
}

SlotE itemSlot( const Item &it )
{
    return Bases[it.m_base].m_slot;
}

std::string affixText( const Affix &a )
{
    return TextFormat( tr( std::string( "affix." ) + Ranges[(int)a.m_stat].m_name ), a.m_value );
}

std::string saveItem( const Item &it )
{
    std::string s =
        TextFormat( "%d %d %d %d %d", it.m_base, (int)it.m_rarity, it.m_level, it.m_name, (int)it.m_affixes.size() );
    for ( const Affix &a : it.m_affixes ) s += TextFormat( " %d %g", (int)a.m_stat, a.m_value );
    return s;
}

bool loadItem( const std::string &s, Item &it )
{
    std::istringstream in( s );
    int                rarity = 0, count = 0;
    if ( !( in >> it.m_base >> rarity >> it.m_level >> it.m_name >> count ) ) return false;
    if ( it.m_base < 0 || it.m_base >= (int)Bases.size() || rarity < 0 || rarity >= (int)RarityE::COUNT ) return false;
    it.m_rarity = (RarityE)rarity;
    it.m_affixes.clear();
    for ( int i = 0; i < count; ++i )
    {
        int   stat = 0;
        float value = 0.0f;
        if ( !( in >> stat >> value ) || stat < 0 || stat >= (int)StatE::COUNT ) return false;
        it.m_affixes.push_back( { (StatE)stat, value } );
    }
    return true;
}

}   // namespace spaceopera
