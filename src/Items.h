/**
 * @file Items.h
 * @brief What the heroes find and wear: weapons (each kind fights its own way, in three tiers), armour, helmets,
 *        gloves, boots, implants, a utility harness, a shield emitter and a power cell; common, magic, rare, unique and
 *        set items, with affixes that grow with the item's level, uniques and sets with powers of their own, sockets
 *        for modules; their names in the loaded language, their worth in scrap, and a line of text for the save.
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
    HARNESS,   ///< the utility harness: stims, air, what one finds
    EMITTER,   ///< the shield emitter: an energy shield before the health
    CELL,      ///< the suit's power cell: what feeds the skills
    MODULE,    ///< a module for a socket (carried, not worn)
    COUNT
};

constexpr int EquipSlots = 10;   ///< weapon, armour, helmet, gloves, boots, two implants, harness, emitter, cell
constexpr int BagSize = 30;      ///< what the hero carries besides

/** @brief How rare an item is (and how much it has). */
enum class RarityE : int
{
    COMMON,   ///< nothing besides its kind
    MAGIC,    ///< one or two affixes
    RARE,     ///< three or four, and a name
    UNIQUE,   ///< a fixed, strong set and its own name
    SET,      ///< one piece of a set: more for every piece worn together
    COUNT
};

/** @brief What an affix (or a skill, or a level) adds to. */
enum class StatE : int
{
    DAMAGE,         ///< % damage
    RATE,           ///< % faster attacks
    CRIT,           ///< % chance of a critical hit
    CRIT_DAMAGE,    ///< % more on a critical hit
    BURN,           ///< % chance to set burning (damage over three seconds)
    SHOCK,          ///< % chance to stun
    LEECH,          ///< % of damage dealt healed
    HEALTH,         ///< health
    ARMOR,          ///< armour
    REGEN,          ///< health per second
    SPEED,          ///< % faster running
    HEAT,           ///< % less heat per shot
    COOLDOWN,       ///< % shorter skill cooldowns
    FIND,           ///< % more items and scrap
    XP,             ///< % more experience
    THORNS,         ///< damage back to whoever strikes
    STIMS,          ///< % more healing from stims
    SHIELD,         ///< energy shield
    SHIELD_RATE,    ///< % faster shield recharge
    SKILLS,         ///< ranks added to every skill learnt
    SKILL_DAMAGE,   ///< % more damage from skills (grenades, quakes, storms, takeovers)
    LIFE_KILL,      ///< health for every kill
    STIM_SLOTS,     ///< stims one can carry more
    OXYGEN,         ///< % more air (vacuum, water)
    RESIST,         ///< % less harm from the place (cold, heat, radiation, air)
    AREA,           ///< % larger blasts
    VS_MACHINES,    ///< % more damage against machines
    VS_PEOPLE,      ///< % more damage against people
    COUNT
};

/** @brief What a unique or a full set can do besides its numbers (Game applies them). */
enum class PowerE : int
{
    NONE,
    CHAIN_CRIT,      ///< a critical hit arcs on to two more
    EXPLODE_KILL,    ///< what dies bursts
    COMPANION,       ///< a small drone flies along and shoots
    NOVA_HURT,       ///< hit, now and then a shock ring stuns what is near
    HEAL_KILL,       ///< every kill heals a little
    SHIELD_KILL,     ///< every kill fills the shield a little
    HASTE_KILL,      ///< every kill: faster for a moment
    PIERCE,          ///< shots and blows go through two more
    DOUBLE,          ///< now and then a hit lands twice
    CULL,            ///< what is nearly dead dies
    BLAST,           ///< every tenth hit a blast where it lands
    THORN_NOVA,      ///< thorns three times as hard, and to all near
    LAST_STAND,      ///< badly hurt: harder hits, more armour
    SECOND_LIFE,     ///< once a level a deadly hit leaves one standing
    COOLDOWN_KILL,   ///< every kill: the skills come back sooner
    MAGNET,          ///< what lies about comes from far
    TWIN_TAKEOVER,   ///< Ione: the takeover takes two machines, for longer
    OVERHEAT_NOVA,   ///< running hot (or the swarm spent) lets out a fire ring
    RAGE,            ///< every blow that lands: harder, for a moment, stacking
    STORM_SHIELD,    ///< Ione: what hits the screen comes back as sparks
    GHOST,           ///< now and then a hit goes through nothing
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
    const char *m_key;             ///< its name's text key (and "<key>.about")
    SlotE       m_slot;            ///< where it is worn
    int         m_hero;            ///< -1 anyone, 0 Corvin, 1 Dorra, 2 Ione
    int         m_level;           ///< the first item level it drops at
    float       m_damage;          ///< weapons: damage per hit (pellet) at item level 1
    float       m_interval;        ///< Corvin: seconds between shots; Dorra: how long a blow takes (1 = the hammer's)
    float       m_heat;            ///< heat per shot
    int         m_pellets;         ///< pellets per shot
    float       m_spread;          ///< their spread (radians)
    int         m_pierce;          ///< targets a slug goes through
    float       m_splash;          ///< radius of the blast where it hits (0 none)
    float       m_range;           ///< reach (m)
    int         m_chain;           ///< targets an arc jumps on to
    float       m_armor;           ///< armour pieces: armour at item level 1
    StatE       m_bonus;           ///< what the kind brings by itself
    float       m_bonusValue;      ///< and how much (0 nothing)
    float       m_shield = 0.0f;   ///< shield emitters: the shield at item level 1
};

/** @brief An item as found. */
struct Item
{
    int                m_base = 0;
    RarityE            m_rarity = RarityE::COMMON;
    int                m_level = 1;   ///< item level
    int                m_name = -1;   ///< a rare's epithet, a unique's or a set piece's number
    std::vector<Affix> m_affixes;
    int                m_sockets = 0;   ///< sockets for modules
    std::vector<Affix> m_mods;          ///< the modules in them
};

/** @brief A set: its pieces' bonus by how many are worn together. */
struct SetDef
{
    const char        *m_key;     ///< its name's text key
    int                m_hero;    ///< -1 anyone, else the hero it is for
    std::vector<Affix> m_two;     ///< with two pieces
    std::vector<Affix> m_three;   ///< with three
    PowerE             m_four;    ///< with all four
};

/** @return the table of kinds */
const std::vector<ItemBase> &itemBases();

/**
 * @brief A new item: a kind fit for the hero and the level, its rarity by chance (luck raises it), its affixes.
 * @param[in]     level   item level
 * @param[in]     hero    0 Corvin, 1 Dorra, 2 Ione
 * @param[in,out] seed    the random state
 * @param[in]     luck    1 normal, more for elites and bosses and item find
 * @param[in]     slot    only this slot (-1 any)
 * @return the item
 */
Item makeItem( int level, int hero, unsigned &seed, float luck, int slot = -1 );

/**
 * @brief The base kind a hero starts with in a slot (the Gauss rifle, the ram hammer, the suit).
 * @param[in] hero  0 Corvin, 1 Dorra, 2 Ione
 * @param[in] slot  weapon or armour
 * @return the item
 */
Item startItem( int hero, SlotE slot );

/**
 * @brief What a hero wears in a companion's tale: their kind (its second tier from level 9) at the real hero's level.
 * @param[in] hero   0 Corvin, 1 Dorra, 2 Ione, 3 Unzu
 * @param[in] slot   the weapon or the armour
 * @param[in] level  the real hero's level
 * @return the item
 */
Item taleItem( int hero, SlotE slot, int level );

/**
 * @brief A module for a socket, at an item level.
 * @param[in]     level  item level
 * @param[in,out] seed   the random state
 * @return the module
 */
Item makeModule( int level, unsigned &seed );

/**
 * @brief Kiri's workshop: one of an item's affixes rolled anew (another stat), or the item made one rarity better
 *        (common to magic: an affix; magic to rare: three, stronger, and a name). Not for uniques and set pieces.
 * @param[in,out] it       the item
 * @param[in]     hero     who it is for (the stats that fit)
 * @param[in,out] seed     the random state
 * @param[in]     upgrade  better (else an affix anew)
 * @return whether anything was done
 */
bool craftItem( Item &it, int hero, unsigned &seed, bool upgrade );

/**
 * @param[in] it       the item
 * @param[in] upgrade  making it better (else an affix anew)
 * @return what Kiri asks for it in scrap (0: it cannot be done)
 */
int craftPrice( const Item &it, bool upgrade );

/**
 * @brief A unique or a set's piece by its number (tests and pictures).
 * @param[in] n      its number from 1 ("u12" is unique.12)
 * @param[in] set    true for a set's piece
 * @param[in] level  item level
 * @return the item (a plain rifle if there is no such one)
 */
Item namedItem( int n, bool set, int level );

/** @return the table of sets */
const std::vector<SetDef> &itemSets();
/** @return the set a piece belongs to (-1 none) */
int itemSet( const Item &it );
/** @return the power of a unique (NONE otherwise) */
PowerE itemPower( const Item &it );
/** @return a unique's line of lore (empty for others) */
std::string itemLore( const Item &it );
/** @return the tier of an item's kind: 1, 2 or 3 */
int itemTier( const Item &it );
/** @return a kind's family (the weapon's shape, the armour's weight; 0..5) */
int itemLook( const Item &it );
/** @return a shield emitter's shield at its level */
float itemShield( const Item &it );
/** @return the most sockets a slot can have */
int maxSockets( SlotE slot );
/** @return the index of a kind by its text key (-1 none) */
int baseOf( const char *key );
/** @return the picture's name for an item ("item_<kind>", "unique_<n>", "set_<n>") */
std::string itemPicture( const Item &it );
/** @return a power's description in the loaded language */
std::string powerText( PowerE p );

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
