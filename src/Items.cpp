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
#include <cstring>
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
    // Ione's swarms: cutters, needles that go through, sparks that jump, embers that burst; then their second tier.
    { "item.swarm",           SlotE::WEAPON,  2,   1,  7.5f, 0.07f,  2.4f, 1, 0.0f,   0, 0.0f, 19.f, 0, 0.f, None,            0.f },
    { "item.needle",          SlotE::WEAPON,  2,   3,  13.f, 0.13f,  4.0f, 1, 0.0f,   2, 0.0f, 22.f, 0, 0.f, None,            0.f },
    { "item.spark",           SlotE::WEAPON,  2,   5,   6.f, 0.08f,  2.6f, 1, 0.0f,   0, 0.0f, 17.f, 2, 0.f, StatE::SHOCK,   12.f },
    { "item.ember",           SlotE::WEAPON,  2,   7,  17.f, 0.24f,  5.0f, 1, 0.0f,   0, 1.6f, 18.f, 0, 0.f, StatE::BURN,    15.f },
    { "item.swarm2",          SlotE::WEAPON,  2,   9,  11.f, 0.065f, 2.2f, 1, 0.0f,   0, 0.0f, 20.f, 0, 0.f, None,            0.f },
    { "item.needle2",         SlotE::WEAPON,  2,  11,  19.f, 0.12f,  3.8f, 1, 0.0f,   3, 0.0f, 23.f, 0, 0.f, None,            0.f },
    { "item.spark2",          SlotE::WEAPON,  2,  13,   9.f, 0.075f, 2.4f, 1, 0.0f,   0, 0.0f, 18.f, 3, 0.f, StatE::SHOCK,   18.f },
    { "item.ember2",          SlotE::WEAPON,  2,  15,  25.f, 0.23f,  4.8f, 1, 0.0f,   0, 2.0f, 19.f, 0, 0.f, StatE::BURN,    20.f },
    // Unzu's survey drone (only in his log): cutting blades on short arms.
    { "item.blades",          SlotE::WEAPON,  3,  99,  11.f, 0.09f,  3.0f, 1, 0.0f,   0, 0.0f, 12.f, 1, 0.f, None,            0.f },
    // The third tier: Corvin's guns (from level 19).
    { "item.rifle3",          SlotE::WEAPON,  0,  19,  44.f, 0.14f,  6.5f, 1, 0.008f, 0, 0.0f, 34.f, 0, 0.f, None,            0.f },
    { "item.carbine3",        SlotE::WEAPON,  0,  20,  23.f, 0.07f,  3.8f, 1, 0.028f, 0, 0.0f, 28.f, 0, 0.f, None,            0.f },
    { "item.scatter3",        SlotE::WEAPON,  0,  21,  15.f, 0.46f, 13.f, 11, 0.12f,  0, 0.0f, 15.f, 0, 0.f, None,            0.f },
    { "item.rail3",           SlotE::WEAPON,  0,  23, 160.f, 0.78f, 25.f,  1, 0.0f,   5, 0.0f, 48.f, 0, 0.f, None,            0.f },
    { "item.plasma3",         SlotE::WEAPON,  0,  24,  68.f, 0.38f, 11.f,  1, 0.02f,  0, 3.4f, 30.f, 0, 0.f, StatE::BURN,    25.f },
    { "item.arc3",            SlotE::WEAPON,  0,  25,  31.f, 0.16f,  6.f,  1, 0.02f,  0, 0.0f, 22.f, 4, 0.f, StatE::SHOCK,   20.f },
    // Dorra's: the third tier, and the drill lance (long, through the plates).
    { "item.hammer3",         SlotE::WEAPON,  1,  19,  64.f, 1.0f,   0.f,  1, 0.f,    0, 0.0f, 2.2f, 0, 0.f, None,            0.f },
    { "item.axe3",            SlotE::WEAPON,  1,  20,  45.f, 0.68f,  0.f,  1, 0.f,    0, 0.0f, 2.1f, 0, 0.f, StatE::BURN,    25.f },
    { "item.pick3",           SlotE::WEAPON,  1,  21,  90.f, 1.12f,  0.f,  1, 0.f,    2, 0.0f, 2.3f, 0, 0.f, None,            0.f },
    { "item.maul3",           SlotE::WEAPON,  1,  23,  70.f, 0.98f,  0.f,  1, 0.f,    0, 0.0f, 2.4f, 0, 0.f, StatE::SHOCK,   30.f },
    { "item.breaker3",        SlotE::WEAPON,  1,  25,  96.f, 1.18f,  0.f,  1, 0.f,    0, 2.6f, 2.3f, 0, 0.f, None,            0.f },
    { "item.drill",           SlotE::WEAPON,  1,   2,  24.f, 0.8f,   0.f,  1, 0.f,    1, 0.0f, 2.6f, 0, 0.f, StatE::CRIT,     6.f },
    { "item.drill2",          SlotE::WEAPON,  1,  12,  36.f, 0.78f,  0.f,  1, 0.f,    2, 0.0f, 2.7f, 0, 0.f, StatE::CRIT,     8.f },
    { "item.drill3",          SlotE::WEAPON,  1,  22,  54.f, 0.76f,  0.f,  1, 0.f,    3, 0.0f, 2.8f, 0, 0.f, StatE::CRIT,    10.f },
    // Ione's: the third tier, acid swarms (they eat on), guardian swarms (some always stay on the suit).
    { "item.swarm3",          SlotE::WEAPON,  2,  19,  16.f, 0.06f,  2.0f, 1, 0.0f,   0, 0.0f, 21.f, 0, 0.f, None,            0.f },
    { "item.needle3",         SlotE::WEAPON,  2,  21,  28.f, 0.11f,  3.5f, 1, 0.0f,   4, 0.0f, 24.f, 0, 0.f, None,            0.f },
    { "item.spark3",          SlotE::WEAPON,  2,  23,  13.f, 0.07f,  2.2f, 1, 0.0f,   0, 0.0f, 19.f, 4, 0.f, StatE::SHOCK,   24.f },
    { "item.ember3",          SlotE::WEAPON,  2,  25,  37.f, 0.22f,  4.5f, 1, 0.0f,   0, 2.4f, 20.f, 0, 0.f, StatE::BURN,    25.f },
    { "item.acid",            SlotE::WEAPON,  2,   4,   9.f, 0.1f,   2.8f, 1, 0.0f,   0, 1.0f, 16.f, 0, 0.f, StatE::BURN,    30.f },
    { "item.acid2",           SlotE::WEAPON,  2,  13,  13.f, 0.095f, 2.6f, 1, 0.0f,   0, 1.2f, 17.f, 0, 0.f, StatE::BURN,    35.f },
    { "item.acid3",           SlotE::WEAPON,  2,  24,  19.f, 0.09f,  2.4f, 1, 0.0f,   0, 1.4f, 19.f, 0, 0.f, StatE::BURN,    40.f },
    { "item.guard",           SlotE::WEAPON,  2,   6,   6.f, 0.08f,  2.2f, 1, 0.0f,   0, 0.0f, 15.f, 0, 0.f, StatE::ARMOR,   10.f },
    { "item.guard2",          SlotE::WEAPON,  2,  14,   9.f, 0.075f, 2.0f, 1, 0.0f,   0, 0.0f, 16.f, 0, 0.f, StatE::ARMOR,   18.f },
    { "item.guard3",          SlotE::WEAPON,  2,  24,  13.f, 0.07f,  1.8f, 1, 0.0f,   0, 0.0f, 18.f, 0, 0.f, StatE::ARMOR,   28.f },
    // The third tier of what anyone wears, and the kinds added to helmets, gloves, boots and implants.
    { "item.vest3",           SlotE::ARMOR,  -1,  19,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 22.f, StatE::SPEED,   5.f },
    { "item.plate3",          SlotE::ARMOR,  -1,  21,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 40.f, StatE::SPEED,  -5.f },
    { "item.exo3",            SlotE::ARMOR,  -1,  24,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 50.f, StatE::SPEED,  10.f },
    { "item.rebreather2",     SlotE::HELMET, -1,  13,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 6.f, StatE::REGEN,    1.0f },
    { "item.helm3",           SlotE::HELMET, -1,  19,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 10.f, None,           0.f },
    { "item.visor3",          SlotE::HELMET, -1,  21,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 7.f, StatE::CRIT,    10.f },
    { "item.rebreather3",     SlotE::HELMET, -1,  23,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 8.f, StatE::REGEN,    1.6f },
    { "item.shockgloves",     SlotE::GLOVES, -1,   5,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 3.f, StatE::SHOCK,    6.f },
    { "item.shockgloves2",    SlotE::GLOVES, -1,  13,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 5.f, StatE::SHOCK,   10.f },
    { "item.gloves3",         SlotE::GLOVES, -1,  19,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 9.f, None,            0.f },
    { "item.grips3",          SlotE::GLOVES, -1,  21,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 7.f, StatE::RATE,    11.f },
    { "item.shockgloves3",    SlotE::GLOVES, -1,  23,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 7.f, StatE::SHOCK,   14.f },
    { "item.treads",          SlotE::BOOTS,  -1,   5,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 4.f, StatE::RESIST,  10.f },
    { "item.treads2",         SlotE::BOOTS,  -1,  13,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 6.f, StatE::RESIST,  15.f },
    { "item.boots3",          SlotE::BOOTS,  -1,  19,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 9.f, None,            0.f },
    { "item.magboots3",       SlotE::BOOTS,  -1,  21,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 9.f, StatE::SPEED,   11.f },
    { "item.treads3",         SlotE::BOOTS,  -1,  23,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 8.f, StatE::RESIST,  22.f },
    { "item.implant.lungs",   SlotE::IMPLANT,-1,   6,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::OXYGEN,  25.f },
    { "item.implant.filter",  SlotE::IMPLANT,-1,   6,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::RESIST,  15.f },
    { "item.implant.hunter",  SlotE::IMPLANT,-1,  10,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::VS_MACHINES, 15.f },
    { "item.implant.duelist", SlotE::IMPLANT,-1,  10,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::VS_PEOPLE, 15.f },
    { "item.implant.reaper",  SlotE::IMPLANT,-1,  12,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::LIFE_KILL, 4.f },
    { "item.implant.area",    SlotE::IMPLANT,-1,  14,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::AREA,    15.f },
    // The utility harness: room for stims, salvage, air.
    { "item.medbelt",         SlotE::HARNESS,-1,   1,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 1.f, StatE::STIM_SLOTS, 2.f },
    { "item.medbelt2",        SlotE::HARNESS,-1,  10,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 2.f, StatE::STIM_SLOTS, 3.f },
    { "item.medbelt3",        SlotE::HARNESS,-1,  20,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 3.f, StatE::STIM_SLOTS, 4.f },
    { "item.salvagebelt",     SlotE::HARNESS,-1,   2,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 1.f, StatE::FIND,    10.f },
    { "item.salvagebelt2",    SlotE::HARNESS,-1,  11,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 2.f, StatE::FIND,    18.f },
    { "item.salvagebelt3",    SlotE::HARNESS,-1,  21,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 3.f, StatE::FIND,    26.f },
    { "item.airbelt",         SlotE::HARNESS,-1,   3,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 1.f, StatE::OXYGEN,  20.f },
    { "item.airbelt2",        SlotE::HARNESS,-1,  12,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 2.f, StatE::OXYGEN,  35.f },
    { "item.airbelt3",        SlotE::HARNESS,-1,  22,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 3.f, StatE::OXYGEN,  50.f },
    // The shield emitter: a shield before the health; quick to recharge, or strong and slow.
    { "item.emitter",         SlotE::EMITTER,-1,   1,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, None,            0.f, 20.f },
    { "item.emitter2",        SlotE::EMITTER,-1,  10,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, None,            0.f, 40.f },
    { "item.emitter3",        SlotE::EMITTER,-1,  20,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, None,            0.f, 65.f },
    { "item.quickemitter",    SlotE::EMITTER,-1,   3,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SHIELD_RATE, 30.f, 14.f },
    { "item.quickemitter2",   SlotE::EMITTER,-1,  12,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SHIELD_RATE, 45.f, 28.f },
    { "item.quickemitter3",   SlotE::EMITTER,-1,  22,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SHIELD_RATE, 60.f, 46.f },
    { "item.bulwark",         SlotE::EMITTER,-1,   5,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SHIELD_RATE, -25.f, 32.f },
    { "item.bulwark2",        SlotE::EMITTER,-1,  14,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SHIELD_RATE, -25.f, 60.f },
    { "item.bulwark3",        SlotE::EMITTER,-1,  24,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SHIELD_RATE, -20.f, 95.f },
    // The power cell: what feeds the skills.
    { "item.fusioncell",      SlotE::CELL,   -1,   1,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::COOLDOWN,  6.f },
    { "item.fusioncell2",     SlotE::CELL,   -1,  10,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::COOLDOWN, 10.f },
    { "item.fusioncell3",     SlotE::CELL,   -1,  20,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::COOLDOWN, 15.f },
    { "item.plasmacell",      SlotE::CELL,   -1,   2,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SKILL_DAMAGE, 10.f },
    { "item.plasmacell2",     SlotE::CELL,   -1,  11,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SKILL_DAMAGE, 18.f },
    { "item.plasmacell3",     SlotE::CELL,   -1,  21,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SKILL_DAMAGE, 28.f },
    { "item.crystalcell",     SlotE::CELL,   -1,   4,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::AREA,     10.f },
    { "item.crystalcell2",    SlotE::CELL,   -1,  13,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::AREA,     16.f },
    { "item.crystalcell3",    SlotE::CELL,   -1,  23,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SKILLS,    1.f },
    // Modules for sockets (not found as kinds to wear: makeModule makes them).
    { "item.mod.damage",      SlotE::MODULE, -1,  99,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::DAMAGE,   0.f },
    { "item.mod.crit",        SlotE::MODULE, -1,  99,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::CRIT,     0.f },
    { "item.mod.rate",        SlotE::MODULE, -1,  99,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::RATE,     0.f },
    { "item.mod.burn",        SlotE::MODULE, -1,  99,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::BURN,     0.f },
    { "item.mod.shock",       SlotE::MODULE, -1,  99,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SHOCK,    0.f },
    { "item.mod.health",      SlotE::MODULE, -1,  99,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::HEALTH,   0.f },
    { "item.mod.armor",       SlotE::MODULE, -1,  99,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::ARMOR,    0.f },
    { "item.mod.shield",      SlotE::MODULE, -1,  99,   0.f, 0.f,    0.f,  0, 0.f,    0, 0.0f, 0.f,  0, 0.f, StatE::SHIELD,   0.f },
};
constexpr int IoneStart = 50;   ///< Ione's first swarm
constexpr int UnzuStart = 58;   ///< Unzu's blades
// clang-format on

/** @brief An affix's range: at item level 1 and at 30. */
struct Range
{
    const char *m_name;
    float       m_lo, m_hi;
};
const Range Ranges[(int)StatE::COUNT] = {
    { "damage", 5.f, 45.f },   { "rate", 3.f, 20.f },     { "crit", 2.f, 12.f },       { "critdmg", 10.f, 70.f },
    { "burn", 5.f, 30.f },     { "shock", 4.f, 22.f },    { "leech", 1.f, 7.f },       { "health", 8.f, 90.f },
    { "armor", 3.f, 30.f },    { "regen", 0.2f, 2.5f },   { "speed", 3.f, 15.f },      { "heat", 5.f, 30.f },
    { "cooldown", 4.f, 22.f }, { "find", 5.f, 45.f },     { "xp", 3.f, 15.f },         { "thorns", 4.f, 60.f },
    { "stims", 10.f, 60.f },   { "shield", 6.f, 60.f },   { "shieldrate", 5.f, 40.f }, { "skills", 1.f, 1.f },
    { "skilldmg", 5.f, 40.f }, { "lifekill", 1.f, 12.f }, { "stimslots", 1.f, 3.f },   { "oxygen", 8.f, 45.f },
    { "resist", 5.f, 30.f },   { "area", 5.f, 30.f },     { "vsmachines", 5.f, 40.f }, { "vspeople", 5.f, 40.f }
};

/** @brief Which affixes a slot can roll. */
std::vector<StatE> allowed( SlotE slot, int hero )
{
    using S = StatE;
    switch ( slot )
    {
    case SlotE::WEAPON:
    {
        std::vector<StatE> v = { S::DAMAGE, S::RATE, S::CRIT,        S::CRIT_DAMAGE, S::BURN,     S::SHOCK,
                                 S::LEECH,  S::XP,   S::VS_MACHINES, S::VS_PEOPLE,   S::LIFE_KILL };
        if ( hero != 1 ) v.push_back( S::HEAT );
        return v;
    }
    case SlotE::ARMOR:
        return { S::HEALTH, S::ARMOR, S::REGEN, S::THORNS, S::FIND, S::STIMS, S::COOLDOWN, S::SHIELD, S::RESIST };
    case SlotE::HELMET:
        return { S::HEALTH, S::ARMOR, S::CRIT, S::XP, S::FIND, S::COOLDOWN, S::REGEN, S::SKILL_DAMAGE, S::OXYGEN };
    case SlotE::GLOVES:
        return { S::RATE, S::CRIT, S::CRIT_DAMAGE, S::LEECH, S::ARMOR, S::DAMAGE, S::VS_MACHINES, S::VS_PEOPLE };
    case SlotE::BOOTS: return { S::SPEED, S::ARMOR, S::HEALTH, S::FIND, S::REGEN, S::RESIST, S::SHIELD_RATE };
    case SlotE::HARNESS:
        return { S::STIM_SLOTS, S::STIMS, S::OXYGEN, S::FIND, S::HEALTH, S::REGEN, S::RESIST, S::LIFE_KILL };
    case SlotE::EMITTER: return { S::SHIELD, S::SHIELD_RATE, S::ARMOR, S::THORNS, S::RESIST, S::HEALTH };
    case SlotE::CELL: return { S::COOLDOWN, S::SKILL_DAMAGE, S::AREA, S::SKILLS, S::XP, S::DAMAGE };
    default:
        return { S::DAMAGE, S::RATE,     S::CRIT,         S::CRIT_DAMAGE, S::LEECH,       S::HEALTH,
                 S::REGEN,  S::COOLDOWN, S::FIND,         S::XP,          S::STIMS,       S::LIFE_KILL,
                 S::AREA,   S::OXYGEN,   S::SKILL_DAMAGE, S::RESIST,      S::VS_MACHINES, S::VS_PEOPLE };
    }
}

/** @return a kind by its key (the tables below name them; -1 none) */
int kind( const char *key )
{
    for ( size_t i = 0; i < Bases.size(); ++i )
        if ( !strcmp( Bases[i].m_key, key ) ) return (int)i;
    return -1;
}

/**
 * @brief A unique: its kind, the first level it drops at, its affixes (they grow a little past that level), its
 *        power. Its name is "unique.<n>", its lore "unique.<n>.about" (n from 1).
 */
struct Unique
{
    int                m_base;
    int                m_level;
    std::vector<Affix> m_affixes;
    PowerE             m_power = PowerE::NONE;
};
using S = StatE;
using P = PowerE;
// clang-format off
const std::vector<Unique> Uniques = {
    { 3, 6, { { S::DAMAGE, 40.f }, { S::CRIT, 10.f }, { S::CRIT_DAMAGE, 60.f } }, P::CULL },
    { 4, 7, { { S::DAMAGE, 30.f }, { S::BURN, 35.f }, { S::LEECH, 3.f } }, P::EXPLODE_KILL },
    { 25, 4, { { S::DAMAGE, 15.f }, { S::HEALTH, 40.f }, { S::STIMS, 50.f } }, P::SECOND_LIFE },
    { 6, 5, { { S::DAMAGE, 45.f }, { S::SHOCK, 25.f }, { S::THORNS, 30.f } }, P::RAGE },
    { 13, 8, { { S::ARMOR, 30.f }, { S::HEALTH, 80.f }, { S::REGEN, 2.f } }, P::LAST_STAND },
    { 15, 4, { { S::CRIT, 10.f }, { S::XP, 12.f }, { S::FIND, 30.f } }, P::GHOST },
    { 1, 3, { { S::RATE, 25.f }, { S::CRIT, 8.f }, { S::LEECH, 4.f } }, P::DOUBLE },
    { 7, 3, { { S::RATE, 20.f }, { S::BURN, 30.f }, { S::LEECH, 5.f } }, P::HEAL_KILL },
    { 27, 10, { { S::DAMAGE, 35.f }, { S::CRIT, 8.f }, { S::XP, 10.f } }, P::CHAIN_CRIT },
    { 30, 14, { { S::DAMAGE, 50.f }, { S::CRIT_DAMAGE, 80.f }, { S::RATE, 10.f } }, P::PIERCE },
    { 29, 12, { { S::DAMAGE, 30.f }, { S::SHOCK, 20.f }, { S::LEECH, 3.f } }, P::NOVA_HURT },
    { 33, 10, { { S::DAMAGE, 40.f }, { S::HEALTH, 50.f }, { S::FIND, 25.f } }, P::THORN_NOVA },
    { 37, 15, { { S::DAMAGE, 45.f }, { S::SHOCK, 25.f }, { S::THORNS, 40.f } }, P::BLAST },
    { 40, 14, { { S::ARMOR, 30.f }, { S::HEALTH, 90.f }, { S::SPEED, 8.f } }, P::COMPANION },
    { 44, 11, { { S::RATE, 18.f }, { S::CRIT, 8.f }, { S::LEECH, 3.f } }, P::COOLDOWN_KILL },
    { 46, 11, { { S::SPEED, 15.f }, { S::REGEN, 1.5f }, { S::FIND, 25.f } }, P::HASTE_KILL },
    { 47, 9, { { S::CRIT, 10.f }, { S::CRIT_DAMAGE, 40.f }, { S::XP, 8.f } }, P::MAGNET },
    { 34, 11, { { S::DAMAGE, 35.f }, { S::BURN, 35.f }, { S::LEECH, 5.f } }, P::CULL },
    { 50, 4, { { S::DAMAGE, 30.f }, { S::RATE, 15.f }, { S::LEECH, 3.f } }, P::TWIN_TAKEOVER },
    { 52, 7, { { S::DAMAGE, 25.f }, { S::SHOCK, 25.f }, { S::CRIT, 8.f } }, P::STORM_SHIELD },
    { 57, 14, { { S::DAMAGE, 45.f }, { S::BURN, 30.f }, { S::RATE, 10.f } }, P::OVERHEAT_NOVA },
    // Corvin's.
    { kind( "item.rifle" ), 2, { { S::DAMAGE, 25.f }, { S::CRIT, 6.f } }, P::PIERCE },
    { kind( "item.scatter" ), 6, { { S::DAMAGE, 30.f }, { S::LIFE_KILL, 5.f } }, P::EXPLODE_KILL },
    { kind( "item.arc" ), 9, { { S::SHOCK, 20.f }, { S::RATE, 10.f }, { S::VS_MACHINES, 20.f } }, P::CHAIN_CRIT },
    { kind( "item.rifle3" ), 19, { { S::DAMAGE, 40.f }, { S::CRIT, 10.f }, { S::CRIT_DAMAGE, 50.f } }, P::CULL },
    { kind( "item.carbine3" ), 20, { { S::RATE, 20.f }, { S::LEECH, 3.f }, { S::VS_PEOPLE, 25.f } }, P::DOUBLE },
    { kind( "item.scatter3" ), 22, { { S::DAMAGE, 45.f }, { S::AREA, 20.f }, { S::HEALTH, 50.f } }, P::NOVA_HURT },
    { kind( "item.plasma3" ), 24, { { S::DAMAGE, 40.f }, { S::BURN, 40.f }, { S::AREA, 25.f } }, P::EXPLODE_KILL },
    { kind( "item.arc3" ), 26, { { S::SHOCK, 30.f }, { S::VS_MACHINES, 40.f }, { S::RATE, 12.f } }, P::OVERHEAT_NOVA },
    // Dorra's.
    { kind( "item.hammer" ), 2, { { S::DAMAGE, 25.f }, { S::HEALTH, 30.f } }, P::NOVA_HURT },
    { kind( "item.pick" ), 5, { { S::DAMAGE, 30.f }, { S::CRIT, 8.f } }, P::CULL },
    { kind( "item.maul" ), 7, { { S::SHOCK, 25.f }, { S::ARMOR, 15.f } }, P::BLAST },
    { kind( "item.drill" ), 4, { { S::DAMAGE, 20.f }, { S::CRIT_DAMAGE, 40.f } }, P::PIERCE },
    { kind( "item.hammer3" ), 20, { { S::DAMAGE, 45.f }, { S::HEALTH, 80.f }, { S::LEECH, 3.f } }, P::SECOND_LIFE },
    { kind( "item.axe3" ), 21, { { S::RATE, 15.f }, { S::BURN, 30.f }, { S::LEECH, 4.f } }, P::HEAL_KILL },
    { kind( "item.drill3" ), 23, { { S::DAMAGE, 40.f }, { S::CRIT, 10.f }, { S::VS_MACHINES, 35.f } }, P::PIERCE },
    { kind( "item.breaker3" ), 26, { { S::DAMAGE, 50.f }, { S::AREA, 30.f }, { S::ARMOR, 20.f } }, P::EXPLODE_KILL },
    // Ione's.
    { kind( "item.needle" ), 4, { { S::DAMAGE, 25.f }, { S::RATE, 10.f } }, P::PIERCE },
    { kind( "item.ember" ), 8, { { S::BURN, 30.f }, { S::AREA, 20.f } }, P::EXPLODE_KILL },
    { kind( "item.acid" ), 6, { { S::BURN, 35.f }, { S::VS_MACHINES, 25.f } }, P::CULL },
    { kind( "item.guard" ), 9, { { S::ARMOR, 25.f }, { S::SHIELD, 30.f } }, P::COMPANION },
    { kind( "item.needle3" ), 21, { { S::DAMAGE, 40.f }, { S::CRIT, 10.f }, { S::CRIT_DAMAGE, 40.f } }, P::CHAIN_CRIT },
    { kind( "item.acid3" ), 24, { { S::BURN, 45.f }, { S::AREA, 25.f }, { S::LEECH, 3.f } }, P::HEAL_KILL },
    { kind( "item.guard3" ), 25, { { S::ARMOR, 40.f }, { S::SHIELD, 60.f }, { S::SHIELD_RATE, 30.f } }, P::SHIELD_KILL },
    // Suits.
    { kind( "item.vest" ), 3, { { S::HEALTH, 40.f }, { S::SPEED, 8.f } }, P::HASTE_KILL },
    { kind( "item.plate2" ), 12, { { S::ARMOR, 40.f }, { S::THORNS, 40.f } }, P::THORN_NOVA },
    { kind( "item.vest3" ), 20, { { S::SPEED, 12.f }, { S::SHIELD, 40.f }, { S::REGEN, 2.f } }, P::GHOST },
    { kind( "item.exo3" ), 25, { { S::ARMOR, 60.f }, { S::HEALTH, 120.f } }, P::LAST_STAND },
    // Helmets.
    { kind( "item.helm" ), 2, { { S::ARMOR, 10.f }, { S::FIND, 25.f } }, P::MAGNET },
    { kind( "item.rebreather2" ), 14, { { S::REGEN, 2.f }, { S::OXYGEN, 50.f } }, P::HEAL_KILL },
    { kind( "item.visor3" ), 22, { { S::CRIT, 12.f }, { S::CRIT_DAMAGE, 60.f } }, P::CHAIN_CRIT },
    // Gloves.
    { kind( "item.gloves" ), 3, { { S::RATE, 10.f }, { S::LEECH, 3.f } }, P::DOUBLE },
    { kind( "item.shockgloves2" ), 14, { { S::SHOCK, 15.f }, { S::VS_MACHINES, 30.f } }, P::NOVA_HURT },
    { kind( "item.grips3" ), 22, { { S::RATE, 15.f }, { S::CRIT, 8.f }, { S::COOLDOWN, 12.f } }, P::COOLDOWN_KILL },
    // Boots.
    { kind( "item.boots" ), 3, { { S::SPEED, 12.f }, { S::FIND, 15.f } }, P::MAGNET },
    { kind( "item.treads2" ), 13, { { S::RESIST, 25.f }, { S::ARMOR, 10.f } }, P::SHIELD_KILL },
    { kind( "item.magboots3" ), 23, { { S::SPEED, 15.f }, { S::SHIELD_RATE, 30.f } }, P::HASTE_KILL },
    // Implants.
    { kind( "item.implant.reaper" ), 12, { { S::LIFE_KILL, 8.f }, { S::DAMAGE, 15.f } }, P::HEAL_KILL },
    { kind( "item.implant.hunter" ), 10, { { S::VS_MACHINES, 30.f }, { S::CRIT, 6.f } }, P::CULL },
    { kind( "item.implant.duelist" ), 14, { { S::VS_PEOPLE, 30.f }, { S::LEECH, 4.f } }, P::DOUBLE },
    { kind( "item.implant.area" ), 18, { { S::AREA, 30.f }, { S::SKILL_DAMAGE, 20.f } }, P::BLAST },
    // Harnesses.
    { kind( "item.medbelt" ), 4, { { S::STIM_SLOTS, 3.f }, { S::STIMS, 40.f } }, P::HEAL_KILL },
    { kind( "item.salvagebelt2" ), 14, { { S::FIND, 40.f }, { S::XP, 10.f } }, P::MAGNET },
    { kind( "item.airbelt3" ), 22, { { S::OXYGEN, 60.f }, { S::RESIST, 30.f }, { S::HEALTH, 60.f } }, P::LAST_STAND },
    // Emitters.
    { kind( "item.emitter" ), 3, { { S::SHIELD, 25.f }, { S::SHIELD_RATE, 20.f } }, P::SHIELD_KILL },
    { kind( "item.quickemitter2" ), 14, { { S::SHIELD_RATE, 50.f }, { S::THORNS, 30.f } }, P::NOVA_HURT },
    { kind( "item.bulwark3" ), 24, { { S::SHIELD, 120.f }, { S::ARMOR, 25.f } }, P::SECOND_LIFE },
    // Cells.
    { kind( "item.fusioncell" ), 4, { { S::COOLDOWN, 12.f }, { S::SKILL_DAMAGE, 10.f } }, P::COOLDOWN_KILL },
    { kind( "item.plasmacell2" ), 14, { { S::SKILL_DAMAGE, 35.f }, { S::AREA, 15.f } }, P::EXPLODE_KILL },
    { kind( "item.crystalcell3" ), 24, { { S::SKILLS, 2.f }, { S::COOLDOWN, 15.f } }, P::COOLDOWN_KILL },
};

/** @brief The sets: four pieces each, the bonus by how many are worn ("set.<n>"; pieces "setitem.<n>"). */
const std::vector<SetDef> Sets = {
    { "set.1", 0, { { S::CRIT, 8.f }, { S::RATE, 10.f } }, { { S::DAMAGE, 25.f } }, P::CHAIN_CRIT },
    { "set.2", 0, { { S::ARMOR, 20.f }, { S::HEALTH, 50.f } }, { { S::CRIT_DAMAGE, 40.f } }, P::SECOND_LIFE },
    { "set.3", 1, { { S::HEALTH, 60.f }, { S::ARMOR, 15.f } }, { { S::THORNS, 50.f } }, P::RAGE },
    { "set.4", 1, { { S::DAMAGE, 20.f } }, { { S::AREA, 25.f } }, P::BLAST },
    { "set.5", 2, { { S::COOLDOWN, 12.f } }, { { S::RATE, 15.f } }, P::TWIN_TAKEOVER },
    { "set.6", 2, { { S::SHIELD, 40.f } }, { { S::SHOCK, 15.f } }, P::STORM_SHIELD },
    { "set.7", -1, { { S::SPEED, 10.f }, { S::OXYGEN, 30.f } }, { { S::RESIST, 25.f } }, P::HASTE_KILL },
    { "set.8", -1, { { S::SHIELD, 60.f } }, { { S::SKILL_DAMAGE, 25.f } }, P::GHOST },
};

/** @brief A set's piece: which set, its kind, the level it drops from, its own affixes. */
struct SetItem
{
    int                m_set;
    int                m_base;
    int                m_level;
    std::vector<Affix> m_affixes;
};
const std::vector<SetItem> SetItems = {
    { 0, kind( "item.carbine2" ), 10, { { S::DAMAGE, 20.f }, { S::CRIT, 5.f } } },
    { 0, kind( "item.vest2" ), 10, { { S::HEALTH, 40.f }, { S::SPEED, 6.f } } },
    { 0, kind( "item.visor2" ), 11, { { S::CRIT, 6.f }, { S::FIND, 15.f } } },
    { 0, kind( "item.grips2" ), 11, { { S::RATE, 8.f }, { S::LEECH, 2.f } } },
    { 1, kind( "item.rail2" ), 13, { { S::DAMAGE, 25.f }, { S::CRIT_DAMAGE, 30.f } } },
    { 1, kind( "item.plate2" ), 12, { { S::ARMOR, 20.f }, { S::HEALTH, 40.f } } },
    { 1, kind( "item.helm2" ), 11, { { S::ARMOR, 8.f }, { S::REGEN, 1.f } } },
    { 1, kind( "item.boots2" ), 10, { { S::SPEED, 8.f }, { S::ARMOR, 6.f } } },
    { 2, kind( "item.hammer2" ), 10, { { S::DAMAGE, 25.f }, { S::HEALTH, 30.f } } },
    { 2, kind( "item.plate2" ), 12, { { S::ARMOR, 25.f }, { S::THORNS, 20.f } } },
    { 2, kind( "item.helm2" ), 11, { { S::HEALTH, 40.f }, { S::ARMOR, 6.f } } },
    { 2, kind( "item.boots2" ), 10, { { S::HEALTH, 30.f }, { S::REGEN, 1.f } } },
    { 3, kind( "item.breaker2" ), 15, { { S::DAMAGE, 25.f }, { S::AREA, 15.f } } },
    { 3, kind( "item.exo2" ), 14, { { S::ARMOR, 25.f }, { S::HEALTH, 40.f } } },
    { 3, kind( "item.gloves2" ), 11, { { S::DAMAGE, 10.f }, { S::RATE, 6.f } } },
    { 3, kind( "item.medbelt2" ), 11, { { S::STIM_SLOTS, 2.f }, { S::STIMS, 30.f } } },
    { 4, kind( "item.swarm2" ), 10, { { S::DAMAGE, 20.f }, { S::RATE, 8.f } } },
    { 4, kind( "item.vest2" ), 10, { { S::HEALTH, 30.f }, { S::COOLDOWN, 8.f } } },
    { 4, kind( "item.shockgloves2" ), 13, { { S::SHOCK, 10.f }, { S::VS_MACHINES, 20.f } } },
    { 4, kind( "item.fusioncell2" ), 11, { { S::COOLDOWN, 8.f }, { S::SKILL_DAMAGE, 15.f } } },
    { 5, kind( "item.spark2" ), 13, { { S::DAMAGE, 20.f }, { S::SHOCK, 10.f } } },
    { 5, kind( "item.rebreather2" ), 13, { { S::REGEN, 1.f }, { S::OXYGEN, 30.f } } },
    { 5, kind( "item.emitter2" ), 11, { { S::SHIELD, 30.f }, { S::SHIELD_RATE, 20.f } } },
    { 5, kind( "item.implant.filter" ), 10, { { S::RESIST, 15.f }, { S::HEALTH, 30.f } } },
    { 6, kind( "item.magboots2" ), 11, { { S::SPEED, 8.f }, { S::RESIST, 10.f } } },
    { 6, kind( "item.airbelt2" ), 12, { { S::OXYGEN, 30.f }, { S::FIND, 15.f } } },
    { 6, kind( "item.quickemitter2" ), 12, { { S::SHIELD_RATE, 25.f }, { S::SHIELD, 20.f } } },
    { 6, kind( "item.implant.lungs" ), 10, { { S::OXYGEN, 30.f }, { S::REGEN, 1.f } } },
    { 7, kind( "item.exo2" ), 14, { { S::ARMOR, 25.f }, { S::SHIELD, 30.f } } },
    { 7, kind( "item.visor2" ), 11, { { S::CRIT, 6.f }, { S::SKILL_DAMAGE, 15.f } } },
    { 7, kind( "item.bulwark2" ), 14, { { S::SHIELD, 40.f }, { S::ARMOR, 10.f } } },
    { 7, kind( "item.crystalcell2" ), 13, { { S::AREA, 15.f }, { S::COOLDOWN, 8.f } } },
};
// clang-format on

constexpr int Qualities = 4;   ///< modules: raw, refined, military, archon's
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
                if ( a.m_stat != StatE::SKILLS && a.m_stat != StatE::STIM_SLOTS ) a.m_value *= grow;
                it.m_affixes.push_back( a );
            }
            return it;
        }
    }
    // A set's piece now and then, if one fits.
    if ( roll( seed ) < 0.012f * luck )
    {
        std::vector<int> fit;
        for ( size_t i = 0; i < SetItems.size(); ++i )
        {
            const ItemBase &b = Bases[SetItems[i].m_base];
            int             owner = Sets[SetItems[i].m_set].m_hero;
            if ( SetItems[i].m_level <= level && ( owner < 0 || owner == hero ) &&
                 ( b.m_hero < 0 || b.m_hero == hero ) && ( slot < 0 || (int)b.m_slot == slot ) )
                fit.push_back( (int)i );
        }
        if ( !fit.empty() )
        {
            int k = fit[(size_t)( roll( seed ) * fit.size() ) % fit.size()];
            it.m_base = SetItems[k].m_base;
            it.m_rarity = RarityE::SET;
            it.m_name = k;
            float grow = 1.0f + 0.02f * std::max( 0, level - SetItems[k].m_level );
            for ( Affix a : SetItems[k].m_affixes )
            {
                if ( a.m_stat != StatE::STIM_SLOTS ) a.m_value *= grow;
                it.m_affixes.push_back( a );
            }
            return it;
        }
    }
    // A kind for this hero and level (slots weighted: weapons and armour more often; the tiers near the level more
    // often than the ones long left behind).
    std::vector<int> kinds;
    for ( size_t i = 0; i < Bases.size(); ++i )
    {
        const ItemBase &b = Bases[i];
        if ( b.m_level > level || ( b.m_hero >= 0 && b.m_hero != hero ) || ( slot >= 0 && (int)b.m_slot != slot ) ||
             b.m_slot == SlotE::MODULE )
            continue;
        int weight = b.m_slot == SlotE::WEAPON ? 6 : b.m_slot == SlotE::ARMOR ? 4 : 2;
        if ( b.m_level + 12 < level ) weight = std::max( 1, weight / 3 );
        for ( int w = 0; w < weight; ++w ) kinds.push_back( (int)i );
    }
    if ( kinds.empty() ) kinds.push_back( hero == 1 ? 6 : ( hero == 2 ? IoneStart : 0 ) );
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
        if ( s == StatE::SKILLS )
        {
            // A rank on every skill is rare: not before level 12, and then one.
            if ( level < 12 ) continue;
            v = 1.0f;
        }
        it.m_affixes.push_back( { s, s == StatE::REGEN ? roundf( v * 10.0f ) / 10.0f : roundf( v ) } );
    }
    // Sockets: the plainer the item, the more room it leaves.
    int most = maxSockets( Bases[it.m_base].m_slot );
    if ( most > 0 )
    {
        float chance = it.m_rarity == RarityE::COMMON ? 0.3f : it.m_rarity == RarityE::MAGIC ? 0.18f : 0.1f;
        if ( roll( seed ) < chance * ( level >= 6 ? 1.0f : 0.4f ) )
            it.m_sockets = std::min( most, 1 + (int)( roll( seed ) * ( 1.0f + level / 12.0f ) ) );
    }
    return it;
}

Item makeModule( int level, unsigned &seed )
{
    // A kind, and its worth by the level: half an affix's.
    Item it;
    it.m_base = kind( "item.mod.damage" ) + (int)( roll( seed ) * 8.0f ) % 8;
    it.m_level = std::max( 1, level );
    it.m_rarity = RarityE::MAGIC;
    StatE        st = Bases[it.m_base].m_bonus;
    const Range &g = Ranges[(int)st];
    float        v = Lerp( g.m_lo, g.m_hi, Clamp( ( level - 1 ) / 29.0f, 0.0f, 1.0f ) ) * 0.5f;
    it.m_affixes.push_back( { st, fmaxf( 1.0f, roundf( v ) ) } );
    it.m_name = std::min( Qualities - 1, ( level - 1 ) / 8 );
    return it;
}

Item namedItem( int n, bool set, int level )
{
    Item it;
    it.m_level = std::max( 1, level );
    if ( set && n >= 1 && n <= (int)SetItems.size() )
    {
        it.m_base = SetItems[n - 1].m_base;
        it.m_rarity = RarityE::SET;
        it.m_name = n - 1;
        it.m_affixes = SetItems[n - 1].m_affixes;
    }
    else if ( !set && n >= 1 && n <= (int)Uniques.size() )
    {
        it.m_base = Uniques[n - 1].m_base;
        it.m_rarity = RarityE::UNIQUE;
        it.m_name = n - 1;
        it.m_affixes = Uniques[n - 1].m_affixes;
    }
    return it;
}

const std::vector<SetDef> &itemSets()
{
    return Sets;
}

int itemSet( const Item &it )
{
    return it.m_rarity == RarityE::SET && it.m_name >= 0 && it.m_name < (int)SetItems.size() ? SetItems[it.m_name].m_set
                                                                                             : -1;
}

PowerE itemPower( const Item &it )
{
    return it.m_rarity == RarityE::UNIQUE && it.m_name >= 0 && it.m_name < (int)Uniques.size()
               ? Uniques[it.m_name].m_power
               : PowerE::NONE;
}

std::string itemLore( const Item &it )
{
    if ( it.m_rarity == RarityE::UNIQUE ) return tr( TextFormat( "unique.%d.about", it.m_name + 1 ) );
    if ( it.m_rarity == RarityE::SET ) return tr( TextFormat( "setitem.%d.about", it.m_name + 1 ) );
    return "";
}

int itemTier( const Item &it )
{
    const char *key = Bases[it.m_base].m_key;
    char        last = key[strlen( key ) - 1];
    return last == '3' ? 3 : last == '2' ? 2 : 1;
}

int itemLook( const Item &it )
{
    // The family by the kind's name without its tier: the weapon's shape, the suit's weight.
    std::string key = Bases[it.m_base].m_key;
    while ( !key.empty() && isdigit( (unsigned char)key.back() ) ) key.pop_back();
    const char *families[][6] = {
        { "item.rifle", "item.carbine", "item.scatter", "item.rail", "item.plasma", "item.arc" },
        { "item.hammer", "item.axe", "item.pick", "item.maul", "item.breaker", "item.drill" },
        { "item.swarm", "item.needle", "item.spark", "item.ember", "item.acid", "item.guard" },
        { "item.vest", "item.plate", "item.exo", "", "", "" }
    };
    for ( const auto &f : families )
        for ( int i = 0; i < 6; ++i )
            if ( key == f[i] ) return i;
    return 0;
}

float itemShield( const Item &it )
{
    return Bases[it.m_base].m_shield * ( 1.0f + 0.1f * ( it.m_level - 1 ) );
}

int maxSockets( SlotE slot )
{
    switch ( slot )
    {
    case SlotE::WEAPON:
    case SlotE::ARMOR: return 3;
    case SlotE::HELMET: return 2;
    case SlotE::EMITTER: return 1;
    default: return 0;
    }
}

int baseOf( const char *key )
{
    return kind( key );
}

std::string itemPicture( const Item &it )
{
    if ( it.m_rarity == RarityE::UNIQUE ) return TextFormat( "unique_%d", it.m_name + 1 );
    if ( it.m_rarity == RarityE::SET ) return TextFormat( "set_%d", it.m_name + 1 );
    std::string key = Bases[it.m_base].m_key + 5;   // without "item."
    for ( char &c : key )
        if ( c == '.' ) c = '_';
    return "item_" + key;
}

std::string powerText( PowerE p )
{
    return tr( TextFormat( "power.%d", (int)p ) );
}

Item startItem( int hero, SlotE slot )
{
    Item it;
    it.m_base =
        slot == SlotE::WEAPON ? ( hero == 1 ? 6 : ( hero == 2 ? IoneStart : ( hero == 3 ? UnzuStart : 0 ) ) ) : 11;
    return it;
}

Item taleItem( int hero, SlotE slot, int level )
{
    // The kind they start with, its second tier from level 9; at the hero's level, with one good affix.
    Item it = startItem( hero, slot );
    if ( level >= 9 )
    {
        const int second[4] = { 27, 33, IoneStart + 4, UnzuStart };
        it.m_base = slot == SlotE::WEAPON ? second[std::clamp( hero, 0, 3 )] : 38;
    }
    it.m_level = std::max( 1, level );
    it.m_rarity = RarityE::MAGIC;
    const Range &g = Ranges[(int)( slot == SlotE::WEAPON ? StatE::DAMAGE : StatE::HEALTH )];
    it.m_affixes.push_back( { slot == SlotE::WEAPON ? StatE::DAMAGE : StatE::HEALTH,
                              roundf( Lerp( g.m_lo, g.m_hi, Clamp( ( level - 1 ) / 29.0f, 0.0f, 1.0f ) ) * 0.8f ) } );
    return it;
}

std::string itemName( const Item &it )
{
    const ItemBase &b = Bases[it.m_base];
    switch ( it.m_rarity )
    {
    case RarityE::UNIQUE: return tr( TextFormat( "unique.%d", it.m_name + 1 ) );
    case RarityE::SET: return tr( TextFormat( "setitem.%d", it.m_name + 1 ) );
    case RarityE::RARE:
        return std::string( tr( b.m_key ) ) + " " + tr( "item.quote.open" ) +
               tr( TextFormat( "rare.%d", it.m_name + 1 ) ) + tr( "item.quote.close" );
    case RarityE::MAGIC:
        if ( b.m_slot == SlotE::MODULE )
            return std::string( tr( TextFormat( "module.quality.%d", std::clamp( it.m_name, 0, Qualities - 1 ) ) ) ) +
                   " " + tr( b.m_key );
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
    case RarityE::SET: return { 110, 230, 120, 255 };
    default: return { 225, 222, 215, 255 };
    }
}

int itemValue( const Item &it )
{
    const float mul[5] = { 1.0f, 2.5f, 6.0f, 12.0f, 10.0f };
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
    // Sockets and the modules in them, if any.
    if ( it.m_sockets > 0 )
    {
        s += TextFormat( " m %d %d", it.m_sockets, (int)it.m_mods.size() );
        for ( const Affix &a : it.m_mods ) s += TextFormat( " %d %g", (int)a.m_stat, a.m_value );
    }
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
    std::string mark;
    it.m_sockets = 0;
    it.m_mods.clear();
    if ( in >> mark && mark == "m" )
    {
        int mods = 0;
        if ( !( in >> it.m_sockets >> mods ) ) return true;
        for ( int i = 0; i < mods; ++i )
        {
            int   stat = 0;
            float value = 0.0f;
            if ( !( in >> stat >> value ) || stat < 0 || stat >= (int)StatE::COUNT ) break;
            it.m_mods.push_back( { (StatE)stat, value } );
        }
    }
    return true;
}

}   // namespace spaceopera
