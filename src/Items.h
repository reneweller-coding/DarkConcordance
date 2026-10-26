/**
 * @file Items.h
 * @brief What the heroes find and wear: weapons (each kind fights its own way), armour, helmets, gloves, boots and
 *        implants; common, magic, rare and unique, with affixes that grow with the item's level; their names in the
 *        loaded language, their worth in scrap, and a line of text for the save.
 * @author Rene Weller
 */
#pragma once

#include <raylib.h>

#include <string>
#include <vector>

namespace spaceopera
{

/** @brief Where an item is worn. */
enum class SlotE : int
{
    WEAPON,
    ARMOR,
    HELMET,
    GLOVES,
    BOOTS,
    IMPLANT,
    COUNT
};

constexpr int EquipSlots = 7;   ///< weapon, armour, helmet, gloves, boots, two implants
constexpr int BagSize = 30;     ///< what the hero carries besides

/** @brief How rare an item is (and how much it has). */
enum class RarityE : int
{
    COMMON,   ///< nothing besides its kind
    MAGIC,    ///< one or two affixes
    RARE,     ///< three or four, and a name
    UNIQUE,   ///< a fixed, strong set and its own name
    COUNT
};

/** @brief What an affix (or a skill, or a level) adds to. */
enum class StatE : int
{
    DAMAGE,        ///< % damage
    RATE,          ///< % faster attacks
    CRIT,          ///< % chance of a critical hit
    CRIT_DAMAGE,   ///< % more on a critical hit
    BURN,          ///< % chance to set burning (damage over three seconds)
    SHOCK,         ///< % chance to stun
    LEECH,         ///< % of damage dealt healed
    HEALTH,        ///< health
    ARMOR,         ///< armour
    REGEN,         ///< health per second
    SPEED,         ///< % faster running
    HEAT,          ///< % less heat per shot
    COOLDOWN,      ///< % shorter skill cooldowns
    FIND,          ///< % more items and scrap
    XP,            ///< % more experience
    THORNS,        ///< damage back to whoever strikes
    STIMS,         ///< % more healing from stims
    COUNT
};

/** @brief One affix: a stat and how much. */
struct Affix
{
    StatE m_stat;
    float m_value;
};

/** @brief A kind of item (the table in Items.cpp). */
struct ItemBase
{
    const char *m_key;          ///< its name's text key (and "<key>.about")
    SlotE       m_slot;         ///< where it is worn
    int         m_hero;         ///< -1 anyone, 0 Corvin, 1 Dorra
    int         m_level;        ///< the first item level it drops at
    float       m_damage;       ///< weapons: damage per hit (pellet) at item level 1
    float       m_interval;     ///< Corvin: seconds between shots; Dorra: how long a blow takes (1 = the hammer's)
    float       m_heat;         ///< heat per shot
    int         m_pellets;      ///< pellets per shot
    float       m_spread;       ///< their spread (radians)
    int         m_pierce;       ///< targets a slug goes through
    float       m_splash;       ///< radius of the blast where it hits (0 none)
    float       m_range;        ///< reach (m)
    int         m_chain;        ///< targets an arc jumps on to
    float       m_armor;        ///< armour pieces: armour at item level 1
    StatE       m_bonus;        ///< what the kind brings by itself
    float       m_bonusValue;   ///< and how much (0 nothing)
};

/** @brief An item as found. */
struct Item
{
    int                m_base = 0;
    RarityE            m_rarity = RarityE::COMMON;
    int                m_level = 1;   ///< item level
    int                m_name = -1;   ///< a rare's epithet, a unique's number
    std::vector<Affix> m_affixes;
};

/** @return the table of kinds */
const std::vector<ItemBase> &itemBases();

/**
 * @brief A new item: a kind fit for the hero and the level, its rarity by chance (luck raises it), its affixes.
 * @param[in]     level   item level
 * @param[in]     hero    0 Corvin, 1 Dorra
 * @param[in,out] seed    the random state
 * @param[in]     luck    1 normal, more for elites and bosses and item find
 * @param[in]     slot    only this slot (-1 any)
 * @return the item
 */
Item makeItem( int level, int hero, unsigned &seed, float luck, int slot = -1 );

/**
 * @brief The base kind a hero starts with in a slot (the Gauss rifle, the ram hammer, the suit).
 * @param[in] hero  0 Corvin, 1 Dorra
 * @param[in] slot  weapon or armour
 * @return the item
 */
Item startItem( int hero, SlotE slot );

/** @return the item's name in the loaded language */
std::string itemName( const Item &it );
/** @return the colour of a rarity */
Color rarityColor( RarityE r );
/** @return what the item is worth in scrap */
int itemValue( const Item &it );
/** @return a weapon's damage per hit at its level */
float itemDamage( const Item &it );
/** @return an armour piece's armour at its level */
float itemArmor( const Item &it );
/** @return the slot of an item */
SlotE itemSlot( const Item &it );
/** @return an affix as a line of text ("+12 % Schaden") */
std::string affixText( const Affix &a );

/** @return the item as one line for the save */
std::string saveItem( const Item &it );
/**
 * @brief Reads an item from its save line.
 * @param[in]  s   the line (without "item ")
 * @param[out] it  the item
 * @return false if it cannot be read
 */
bool loadItem( const std::string &s, Item &it );

}   // namespace spaceopera
