/**
 * @file    GameFamilies.cpp
 * @brief   The places' own creatures, each family in three tiers: crystal wardens and phase shades in the object of
 *          Tesh, cable eels and diving drones in the drowned quarter, arena beasts and strangler vines in the dying
 *          gardens, frost cores and ice spiders on the computing moon, slag golems and magma crawlers in the Mill,
 *          memory phantoms and glitches in the net, crust crabs on Brannt. What hits are made of (kinetic, heat,
 *          shock, acid) and what every enemy shrugs off or fears; the champions' twelve gifts (two in the later acts),
 *          the leaders with their escorts; burning, corroding and freezing ground; the hero chilled, burning,
 *          corroded.
 * @author  Rene Weller
 */

#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"
#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace spaceopera
{

using namespace play;

namespace
{

/** @brief The families (level files name them in "natives"). */
enum FamilyE
{
    CRYSTAL,
    SHADE,
    EEL,
    DIVER,
    BEAST,
    VINE,
    FROST,
    SPIDER,
    GOLEM,
    MAGMA,
    PHANTOM,
    GLITCH,
    CRAB,
    FAMILIES
};

/** @brief How a family fights. */
enum ArchE
{
    CASTER,    ///< keeps its distance, fires
    PHASER,    ///< blinks next to the hero, strikes, blinks away
    LUNGER,    ///< runs in and leaps
    CHARGER,   ///< charges in a line, swipes or slams close by
    ROOTED,    ///< stays where it grew: whips close, spits far
    SWARM      ///< small and quick, splits when it dies
};

/** @brief The champions' gifts (Foe::m_affix, m_affix2). */
enum GiftE
{
    SWIFT,
    ARMOURED,
    VOLATILE,
    BERSERK,
    FIERY,
    SHOCKING,
    BLINKING,
    SHIELDED,
    SPLITTING,
    VAMPIRIC,
    ICY,
    SUMMONER,
    GIFTS
};

struct FamilyDef
{
    const char *m_key;         ///< texts foe.<key>.<tier>; level files' "natives"
    ArchE       m_arch;        ///< how it fights
    float       m_hp;          ///< health at tier 0 (area 1)
    float       m_speed;       ///< how fast it walks
    float       m_hurt;        ///< a blow or a shot
    float       m_radius;      ///< its body
    float       m_height;      ///< where it is hit (above the ground)
    int         m_resist[4];   ///< kinetic, heat, shock, acid in percent (100 immune, below 0 weak)
    int         m_shot;        ///< its shot (Shot::m_kind), 0 none
    int         m_xp;          ///< experience at tier 0
    Vector3     m_glow[3];     ///< its glow per tier
};

const FamilyDef Families[FAMILIES] = {
    { "crystal",
      CASTER,
      80.0f,
      2.2f,
      9.0f,
      0.5f,
      1.05f,
      { 50, 0, -50, 25 },
      1,
      18,
      { { 2.0f, 0.8f, 4.0f }, { 0.8f, 3.0f, 4.2f }, { 4.4f, 3.2f, 0.9f } } },
    { "shade",
      PHASER,
      60.0f,
      3.6f,
      12.0f,
      0.45f,
      1.0f,
      { 50, -50, 0, 0 },
      0,
      16,
      { { 2.4f, 1.6f, 4.0f }, { 4.5f, 0.6f, 0.6f }, { 4.0f, 4.0f, 4.4f } } },
    { "eel",
      LUNGER,
      48.0f,
      4.4f,
      10.0f,
      0.4f,
      0.3f,
      { 0, 25, 100, -50 },
      0,
      12,
      { { 0.6f, 2.6f, 4.4f }, { 1.6f, 4.0f, 4.4f }, { 4.4f, 4.4f, 5.0f } } },
    { "diver",
      CASTER,
      55.0f,
      2.6f,
      18.0f,
      0.5f,
      0.65f,
      { 0, 0, -50, 25 },
      4,
      14,
      { { 0.4f, 3.0f, 3.6f }, { 4.0f, 2.6f, 0.4f }, { 5.0f, 0.6f, 0.3f } } },
    { "beast",
      CHARGER,
      150.0f,
      3.1f,
      22.0f,
      0.8f,
      0.95f,
      { 25, 0, 0, -50 },
      0,
      28,
      { { 4.0f, 1.6f, 0.4f }, { 4.6f, 0.6f, 0.3f }, { 4.6f, 3.6f, 1.0f } } },
    { "vine",
      ROOTED,
      95.0f,
      0.0f,
      11.0f,
      0.55f,
      1.3f,
      { 25, -75, 25, 50 },
      3,
      16,
      { { 1.4f, 4.0f, 0.6f }, { 4.4f, 0.8f, 0.8f }, { 3.0f, 0.8f, 4.4f } } },
    { "frost",
      CASTER,
      65.0f,
      2.4f,
      8.0f,
      0.5f,
      1.35f,
      { 0, -50, 0, 25 },
      2,
      15,
      { { 1.2f, 2.8f, 4.6f }, { 2.6f, 3.8f, 4.8f }, { 4.6f, 4.8f, 5.0f } } },
    { "spider",
      LUNGER,
      58.0f,
      4.0f,
      10.0f,
      0.55f,
      0.5f,
      { 25, -50, 0, 0 },
      5,
      15,
      { { 0.8f, 2.4f, 4.2f }, { 2.4f, 3.8f, 4.6f }, { 4.4f, 1.4f, 4.2f } } },
    { "golem",
      CHARGER,
      210.0f,
      2.0f,
      26.0f,
      0.85f,
      1.5f,
      { 40, 100, 0, -50 },
      0,
      34,
      { { 4.6f, 1.6f, 0.3f }, { 5.0f, 2.6f, 0.6f }, { 5.0f, 4.4f, 2.4f } } },
    { "magma",
      LUNGER,
      62.0f,
      3.4f,
      12.0f,
      0.5f,
      0.35f,
      { 0, 100, -50, 0 },
      0,
      15,
      { { 4.6f, 1.4f, 0.2f }, { 5.0f, 2.4f, 0.4f }, { 5.0f, 4.0f, 1.6f } } },
    { "phantom",
      PHASER,
      62.0f,
      3.0f,
      9.0f,
      0.45f,
      1.0f,
      { 50, 0, -50, 25 },
      6,
      16,
      { { 1.6f, 2.8f, 4.4f }, { 1.2f, 4.2f, 2.0f }, { 4.6f, 1.2f, 1.6f } } },
    { "glitch",
      SWARM,
      24.0f,
      5.2f,
      6.0f,
      0.32f,
      0.35f,
      { -25, 0, -25, 50 },
      0,
      5,
      { { 4.4f, 0.6f, 4.0f }, { 0.6f, 4.6f, 2.0f }, { 4.8f, 4.8f, 4.8f } } },
    { "crab",
      CHARGER,
      115.0f,
      2.6f,
      16.0f,
      0.7f,
      0.5f,
      { 25, 0, 0, -50 },
      0,
      22,
      { { 3.6f, 2.6f, 1.0f }, { 1.0f, 3.6f, 3.0f }, { 4.4f, 1.0f, 3.6f } } },
};

constexpr float TierHp[3] = { 1.0f, 1.6f, 2.4f };
constexpr float TierHurt[3] = { 1.0f, 1.3f, 1.7f };
constexpr float TierSize[3] = { 1.0f, 1.12f, 1.25f };
constexpr float TierPace[3] = { 1.0f, 0.85f, 0.7f };   ///< how long it waits between attacks

/** @brief A gift as a colour (the champion's ring, its name). */
const Color GiftColor[GIFTS] = { { 120, 255, 150, 255 }, { 140, 185, 255, 255 }, { 255, 200, 60, 255 },
                                 { 255, 55, 70, 255 },   { 255, 110, 30, 255 },  { 110, 230, 255, 255 },
                                 { 200, 120, 255, 255 }, { 230, 240, 255, 255 }, { 200, 255, 80, 255 },
                                 { 170, 20, 50, 255 },   { 190, 240, 255, 255 }, { 255, 80, 220, 255 } };

int familyNamed( const std::string &name )
{
    for ( int i = 0; i < FAMILIES; ++i )
        if ( name == Families[i].m_key ) return i;
    return -1;
}

/** @brief A box from a to b, `w` thick (a shard, a horn, a leg). */
void beam( MeshBuilder &b, Vector3 a, Vector3 c, float w )
{
    Vector3 d = Vector3Subtract( c, a );
    float   len = Vector3Length( d );
    if ( len < 1e-4f ) return;
    Vector3 y = Vector3Scale( d, 1.0f / len );
    Vector3 ref = fabsf( y.y ) < 0.9f ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
    Vector3 x = Vector3Normalize( Vector3CrossProduct( ref, y ) );
    Vector3 z = Vector3CrossProduct( x, y );
    b.box( frame( x, y, z, Vector3Lerp( a, c, 0.5f ) ), { w, len * 0.5f, w } );
}

/** @brief A matrix that lays a cylinder (along y) along `dir` from `at`. */
Matrix along( Vector3 dir, Vector3 at )
{
    Vector3 y = Vector3Normalize( dir );
    Vector3 ref = fabsf( y.y ) < 0.9f ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
    Vector3 x = Vector3Normalize( Vector3CrossProduct( ref, y ) );
    return frame( x, y, Vector3CrossProduct( x, y ), at );
}

Mesh meshOf( MeshBuilder &b )
{
    if ( b.empty() ) return Mesh{};
    std::vector<Mesh> m = b.build();
    return m.empty() ? Mesh{} : m.front();
}

}   // namespace

// ------------------------------------------------------------------------------------------------- the families

void Game::buildFamilies()
{
    // Each family's meshes per tier: a body, its moving part, its glowing parts (origin on the ground, facing +z).
    for ( int fam = 0; fam < FAMILIES; ++fam )
        for ( int t = 0; t < 3; ++t )
        {
            MeshBuilder body, part, glow;
            switch ( fam )
            {
            case CRYSTAL:
            {
                // Shards round a core that shows between them; more and longer ones as it grows.
                int n = 5 + 2 * t;
                body.m_color = t == 2 ? Color{ 235, 205, 150, 255 } : Color{ 205, 175, 255, 255 };
                for ( int i = 0; i < n; ++i )
                {
                    float   a = i * 2.0f * PI / n, lean = 0.45f + 0.15f * ( i % 2 );
                    Vector3 dir = { cosf( a ) * sinf( lean ), cosf( lean ), sinf( a ) * sinf( lean ) };
                    Vector3 base = { cosf( a ) * 0.12f, -0.15f, sinf( a ) * 0.12f };
                    beam( body, base, Vector3Add( base, Vector3Scale( dir, 0.55f + 0.12f * ( i % 3 ) + 0.1f * t ) ),
                          0.06f + 0.01f * t );
                    beam( body, base, Vector3Add( base, Vector3Scale( { dir.x, -dir.y * 0.6f, dir.z }, 0.35f ) ),
                          0.05f );
                }
                glow.roundedBox( MatrixIdentity(), { 0.14f, 0.14f, 0.14f }, 0.07f, 2 );
                for ( int i = 0; i < 3; ++i )
                {
                    float a = i * 2.0f * PI / 3.0f;
                    part.box( place( { cosf( a ) * 0.62f, 0.0f, sinf( a ) * 0.62f }, { 0.4f, a, 0.6f } ),
                              { 0.04f, 0.12f, 0.04f } );
                }
                break;
            }
            case SHADE:
            {
                // A hooded thing without legs, long blades for arms; eyes in the dark of the hood.
                body.m_color = { 40, 38, 52, 255 };
                body.lathe( MatrixIdentity(),
                            { { 0.05f, 0.1f },
                              { 0.3f, 0.15f },
                              { 0.22f, 0.7f },
                              { 0.2f, 1.15f },
                              { 0.12f, 1.35f },
                              { 0.0f, 1.4f } },
                            10 );
                body.roundedBox( place( { 0.0f, 1.5f, 0.06f }, { 0.35f, 0.0f, 0.0f } ), { 0.15f, 0.17f, 0.15f }, 0.07f,
                                 2 );
                if ( t >= 1 )
                    for ( float x : { -0.24f, 0.24f } )
                        beam( body, { x, 1.25f, 0.0f }, { x * 1.8f, 1.6f, -0.1f }, 0.03f );
                part.m_color = { 70, 70, 85, 255 };
                beam( part, { 0.0f, 0.0f, 0.0f }, { 0.0f, -0.75f - 0.1f * t, 0.25f }, 0.025f );
                beam( part, { 0.0f, -0.75f - 0.1f * t, 0.25f }, { 0.0f, -0.95f - 0.1f * t, 0.5f }, 0.018f );
                for ( float x : { -0.06f, 0.06f } ) glow.box( place( { x, 1.5f, 0.2f } ), { 0.025f, 0.012f, 0.01f } );
                break;
            }
            case EEL:
            case MAGMA:
            {
                // A head with eyes and fins; a segment drawn again and again behind it.
                bool hot = fam == MAGMA;
                body.m_color = hot ? Color{ 60, 48, 42, 255 } : Color{ 50, 95, 100, 255 };
                body.roundedBox( place( { 0.0f, 0.0f, 0.1f } ), { 0.17f, 0.13f, 0.3f }, 0.1f, 2 );
                body.roundedBox( place( { 0.0f, -0.06f, 0.28f }, { 0.25f, 0.0f, 0.0f } ), { 0.12f, 0.04f, 0.14f },
                                 0.03f, 2 );
                if ( !hot )
                    for ( float x : { -1.0f, 1.0f } )
                        beam( body, { x * 0.12f, 0.06f, 0.0f }, { x * 0.3f, 0.12f, -0.15f }, 0.02f );
                else
                    for ( int i = 0; i < 3 + t; ++i )
                        beam( body, { 0.0f, 0.1f, 0.2f - i * 0.12f }, { 0.0f, 0.24f + 0.04f * t, 0.15f - i * 0.12f },
                              0.03f );
                for ( float x : { -0.09f, 0.09f } ) glow.box( place( { x, 0.06f, 0.36f } ), { 0.03f, 0.02f, 0.02f } );
                part.m_color = body.m_color;
                part.roundedBox( MatrixIdentity(), { 0.15f, 0.12f, 0.17f }, 0.09f, 2 );
                glow.box( place( { 0.0f, hot ? -0.1f : 0.12f, -0.4f } ), { 0.04f, 0.015f, 0.12f } );
                break;
            }
            case DIVER:
            {
                // A yellow torpedo with fins, its eye at the front, a screw at the back.
                body.m_color = t == 2 ? Color{ 200, 60, 40, 255 } : Color{ 220, 180, 45, 255 };
                body.lathe( along( { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -0.55f } ),
                            { { 0.0f, 0.0f },
                              { 0.12f, 0.05f },
                              { 0.22f, 0.3f },
                              { 0.22f, 0.8f },
                              { 0.12f, 1.05f },
                              { 0.0f, 1.12f } },
                            14 );
                for ( int i = 0; i < 4; ++i )
                {
                    float a = i * PI * 0.5f + PI * 0.25f;
                    beam( body, { cosf( a ) * 0.18f, sinf( a ) * 0.18f, -0.4f },
                          { cosf( a ) * 0.36f, sinf( a ) * 0.36f, -0.55f }, 0.02f );
                }
                if ( t >= 1 )
                    for ( float x : { -0.3f, 0.3f } )
                        body.cylinder( along( { 0.0f, 0.0f, 1.0f }, { x, -0.05f, -0.2f } ), 0.07f, 0.5f, 8 );
                glow.roundedBox( place( { 0.0f, 0.0f, 0.57f } ), { 0.08f, 0.08f, 0.03f }, 0.03f, 2 );
                part.box( MatrixIdentity(), { 0.2f, 0.02f, 0.01f } );
                part.box( MatrixIdentity(), { 0.02f, 0.2f, 0.01f } );
                break;
            }
            case BEAST:
            {
                // A heavy four-legged runner: a broad back, a low head with horns, a tail.
                body.m_color = t == 2 ? Color{ 110, 80, 70, 255 } : Color{ 170, 115, 80, 255 };
                body.roundedBox( place( { 0.0f, 0.95f, 0.0f } ), { 0.42f, 0.36f, 0.72f }, 0.18f, 3 );
                body.roundedBox( place( { 0.0f, 0.95f, 0.85f }, { 0.25f, 0.0f, 0.0f } ), { 0.26f, 0.23f, 0.3f }, 0.12f,
                                 2 );
                for ( float x : { -1.0f, 1.0f } )
                    beam( body, { x * 0.2f, 1.12f, 0.95f }, { x * ( 0.45f + 0.1f * t ), 1.45f + 0.1f * t, 0.75f },
                          0.04f );
                beam( body, { 0.0f, 1.05f, -0.7f }, { 0.0f, 0.75f, -1.2f }, 0.05f );
                if ( t >= 1 )
                {
                    body.m_color = { 120, 120, 125, 255 };
                    for ( int i = 0; i < 3; ++i )
                        body.box( place( { 0.0f, 1.33f, 0.4f - i * 0.35f }, { 0.15f, 0.0f, 0.0f } ),
                                  { 0.3f, 0.04f, 0.16f } );
                }
                if ( t == 2 )
                    for ( int i = 0; i < 5; ++i )
                        beam( body, { 0.0f, 1.3f, 0.5f - i * 0.25f }, { 0.0f, 1.6f, 0.45f - i * 0.25f }, 0.03f );
                part.m_color = body.m_color;
                beam( part, { 0.0f, 0.0f, 0.0f }, { 0.0f, -0.62f, 0.05f }, 0.09f );
                for ( float x : { -0.1f, 0.1f } ) glow.box( place( { x, 1.0f, 1.14f } ), { 0.03f, 0.02f, 0.01f } );
                if ( t == 2 )
                    for ( float x : { -0.43f, 0.43f } )
                        glow.box( place( { x, 0.95f, 0.0f } ), { 0.005f, 0.05f, 0.5f } );
                break;
            }
            case VINE:
            {
                // A stalk from roots, a bulb with petals and a mouth that glows; whips (drawn apart).
                body.m_color = t == 1 ? Color{ 80, 70, 40, 255 }
                                      : ( t == 2 ? Color{ 90, 40, 50, 255 } : Color{ 60, 110, 50, 255 } );
                for ( int i = 0; i < 5; ++i )
                {
                    float a = i * 2.0f * PI / 5.0f;
                    beam( body, { 0.0f, 0.15f, 0.0f }, { cosf( a ) * 0.7f, 0.02f, sinf( a ) * 0.7f }, 0.05f );
                }
                body.lathe( MatrixIdentity(), { { 0.2f, 0.0f }, { 0.16f, 0.6f }, { 0.12f, 1.3f }, { 0.0f, 1.35f } },
                            10 );
                body.lathe( place( { 0.0f, 1.3f, 0.0f } ),
                            { { 0.0f, 0.0f }, { 0.28f, 0.12f }, { 0.32f, 0.3f }, { 0.18f, 0.48f }, { 0.0f, 0.5f } },
                            12 );
                for ( int i = 0; i < 6; ++i )
                {
                    float a = i * PI / 3.0f;
                    beam( body, { cosf( a ) * 0.25f, 1.7f, sinf( a ) * 0.25f },
                          { cosf( a ) * ( 0.55f + 0.1f * t ), 1.95f, sinf( a ) * ( 0.55f + 0.1f * t ) }, 0.06f );
                }
                if ( t >= 1 )
                    for ( int i = 0; i < 8; ++i )
                    {
                        float a = i * PI / 4.0f, y = 0.3f + ( i % 4 ) * 0.25f;
                        beam( body, { cosf( a ) * 0.14f, y, sinf( a ) * 0.14f },
                              { cosf( a ) * 0.32f, y + 0.08f, sinf( a ) * 0.32f }, 0.015f );
                    }
                glow.roundedBox( place( { 0.0f, 1.72f, 0.0f } ), { 0.17f, 0.04f, 0.17f }, 0.03f, 2 );
                part.m_color = body.m_color;
                beam( part, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.2f + 0.2f * t }, 0.035f );
                break;
            }
            case FROST:
            {
                // Plates of ice round a cold core, rings turning about it.
                body.m_color = { 210, 230, 250, 255 };
                for ( int i = 0; i < 8; ++i )
                {
                    float   a = i * PI / 4.0f, e = ( i % 2 ) ? 0.5f : -0.4f;
                    Vector3 n = { cosf( a ) * cosf( e ), sinf( e ), sinf( a ) * cosf( e ) };
                    Vector3 x = Vector3Normalize( Vector3CrossProduct( { 0.0f, 1.0f, 0.0f }, n ) );
                    body.box( frame( x, Vector3CrossProduct( n, x ), n, Vector3Scale( n, 0.33f + 0.03f * t ) ),
                              { 0.13f, 0.13f, 0.03f } );
                }
                glow.roundedBox( MatrixIdentity(), { 0.2f, 0.2f, 0.2f }, 0.1f, 2 );
                part.lathe( MatrixIdentity(),
                            { { 0.5f + 0.05f * t, -0.02f }, { 0.55f + 0.05f * t, 0.0f }, { 0.5f + 0.05f * t, 0.02f } },
                            24 );
                break;
            }
            case SPIDER:
            case CRAB:
            {
                // On the leg model; here only what sits on it: a crest of ice or a crab's shield and claws.
                if ( fam == SPIDER )
                {
                    body.m_color = { 210, 230, 250, 255 };
                    for ( int i = 0; i < 3 + 2 * t; ++i )
                        beam( body, { 0.0f, 0.5f, 0.15f - i * 0.12f },
                              { ( i % 2 ? 0.08f : -0.08f ), 0.8f + 0.06f * t, 0.1f - i * 0.12f }, 0.03f );
                    for ( float x : { -0.07f, 0.07f } )
                        glow.box( place( { x, 0.5f, 0.42f } ), { 0.025f, 0.02f, 0.02f } );
                    glow.box( place( { 0.0f, 0.55f, 0.44f } ), { 0.02f, 0.02f, 0.02f } );
                }
                else
                {
                    body.m_color = t == 1 ? Color{ 90, 110, 120, 255 }
                                          : ( t == 2 ? Color{ 140, 140, 150, 255 } : Color{ 130, 90, 60, 255 } );
                    body.box( frame( { 1.0f, 0.0f, 0.0f }, Vector3Normalize( { 0.0f, 1.0f, -0.35f } ),
                                     Vector3Normalize( { 0.0f, 0.35f, 1.0f } ), { 0.0f, 0.55f, 0.55f } ),
                              { 0.5f + 0.05f * t, 0.3f, 0.05f } );
                    for ( float x : { -1.0f, 1.0f } )
                    {
                        beam( body, { x * 0.3f, 0.45f, 0.4f }, { x * 0.5f, 0.35f, 0.8f }, 0.06f );
                        beam( body, { x * 0.5f, 0.35f, 0.8f }, { x * 0.38f, 0.32f, 1.05f }, 0.08f );
                    }
                    for ( float x : { -0.12f, 0.12f } )
                        glow.box( place( { x, 0.82f, 0.48f } ), { 0.03f, 0.03f, 0.02f } );
                }
                break;
            }
            case GOLEM:
            {
                // Heaped slag: a torso, a small head sunk between the shoulders, cracks that glow; limbs apart.
                body.m_color = { 60, 52, 48, 255 };
                body.roundedBox( place( { 0.0f, 1.65f, 0.0f } ), { 0.58f, 0.6f, 0.42f }, 0.2f, 2 );
                body.roundedBox( place( { 0.0f, 2.35f, 0.12f } ), { 0.2f, 0.18f, 0.2f }, 0.08f, 2 );
                body.roundedBox( place( { 0.0f, 1.0f, 0.0f } ), { 0.42f, 0.2f, 0.32f }, 0.1f, 2 );
                if ( t >= 1 )
                    for ( float x : { -0.62f, 0.62f } )
                        body.roundedBox( place( { x, 2.15f, 0.0f } ), { 0.22f, 0.16f, 0.26f }, 0.08f, 2 );
                part.m_color = body.m_color;
                part.roundedBox( place( { 0.0f, -0.45f, 0.0f } ), { 0.2f, 0.5f, 0.2f }, 0.08f, 2 );
                for ( int i = 0; i < 4 + 2 * t; ++i )
                {
                    float y = 1.3f + i * 0.13f, x = ( i % 2 ? 0.2f : -0.25f );
                    glow.box( place( { x, y, 0.42f }, { 0.0f, 0.0f, 0.5f * ( i % 3 ) - 0.4f } ),
                              { 0.18f, 0.012f, 0.01f } );
                }
                for ( float x : { -0.08f, 0.08f } ) glow.box( place( { x, 2.38f, 0.32f } ), { 0.03f, 0.02f, 0.01f } );
                break;
            }
            case PHANTOM:
            {
                // A person made of light: a memory standing up.
                glow.roundedBox( place( { 0.0f, 1.25f, 0.0f } ), { 0.22f, 0.35f, 0.13f }, 0.06f, 2 );
                glow.roundedBox( place( { 0.0f, 1.78f, 0.0f } ), { 0.12f, 0.14f, 0.12f }, 0.06f, 2 );
                for ( float x : { -0.12f, 0.12f } ) beam( glow, { x, 0.9f, 0.0f }, { x * 1.1f, 0.1f, 0.0f }, 0.06f );
                part.box( place( { 0.0f, -0.3f, 0.0f } ), { 0.05f, 0.32f, 0.05f } );
                break;
            }
            case GLITCH:
            {
                // A cube of light in a frame of dead pixels.
                float h = 0.2f;
                body.m_color = { 30, 30, 36, 255 };
                for ( int a = 0; a < 3; ++a )
                    for ( float u : { -h, h } )
                        for ( float v : { -h, h } )
                        {
                            Vector3 p = a == 0 ? Vector3{ 0.0f, u, v }
                                               : ( a == 1 ? Vector3{ u, 0.0f, v } : Vector3{ u, v, 0.0f } );
                            Vector3 half = a == 0
                                               ? Vector3{ h, 0.02f, 0.02f }
                                               : ( a == 1 ? Vector3{ 0.02f, h, 0.02f } : Vector3{ 0.02f, 0.02f, h } );
                            body.box( place( p ), half );
                        }
                glow.box( MatrixIdentity(), { h * 0.8f, h * 0.8f, h * 0.8f } );
                break;
            }
            default: break;
            }
            m_familyLook[fam][t] = { meshOf( body ), meshOf( part ), meshOf( glow ) };
        }
    // The ice spiders and crust crabs walk on the leg model; ground that burns or corrodes is a disc.
    m_spider.build( S_ICE, S_SNOW, S_GLOW, false, 1.25f );
    m_crab.build( S_RUST, S_DARK, S_GLOW, false, 1.5f );
    MeshBuilder disc;
    disc.cylinder( MatrixIdentity(), 1.0f, 0.02f, 20 );
    m_hazardDisc = meshOf( disc );
}

void Game::spawnNatives()
{
    // "natives crystal shade": the generator's digits 1, 2 ... are these families (each foe its tier by the act).
    for ( const auto &n : m_level.m_natives )
    {
        int which = n.m_which - 1;
        if ( which < 0 || which >= (int)m_level.m_nativeNames.size() ) continue;
        int family = familyNamed( m_level.m_nativeNames[which] );
        if ( family < 0 ) continue;
        spawnNative( family, m_level.centre( n.m_row, n.m_col ), nativeTier() );
        auto elite = m_level.m_elites.find( n.m_row * 1000 + n.m_col );
        if ( elite != m_level.m_elites.end() ) makeElite( m_foes.back(), elite->second );
    }
}

void Game::giftsOnHit( float amount )
{
    // The enemy acting now hurt the hero: a bloodthirsty one drinks, an icy one chills.
    if ( m_actor < 0 || m_actor >= (int)m_foes.size() ) return;
    Foe &f = m_foes[m_actor];
    if ( hasGift( f, VAMPIRIC ) && f.m_alive )
    {
        f.m_hp = fminf( f.m_hpMax, f.m_hp + amount * 3.0f );
        Vector3 mid = Vector3Add( f.m_pos, { 0.0f, f.m_kind == FoeE::DRONE ? 0.0f : foeHeight( f ), 0.0f } );
        m_tracers.push_back( { Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), mid, 0.2f } );
        sparks( mid, { 0.0f, 1.0f, 0.0f }, 10, { 200, 20, 50, 255 }, 2.0f );
    }
    if ( hasGift( f, ICY ) ) m_chill = fmaxf( m_chill, 1.5f );
}

void Game::creatureLineup( int page )
{
    int first = page == 0 ? 0 : 7, last = page == 0 ? 7 : FAMILIES;
    for ( int fam = first; fam < last; ++fam )
        for ( int t = 0; t < 3; ++t )
        {
            Vector3 at = { m_pos.x + ( fam - first - ( last - first - 1 ) * 0.5f ) * 3.0f, m_pos.y,
                           m_pos.z - 4.0f - t * 3.2f };
            spawnNative( fam, at, t );
            Foe &f = m_foes.back();
            f.m_yaw = 0.0f;
            f.m_stun = 1e6f;
            f.m_awake = false;
            f.m_stage = 0;
        }
}

int Game::foeRole( const Foe &f ) const
{
    switch ( f.m_kind )
    {
    case FoeE::LOUSE:
    case FoeE::BATON:
    case FoeE::SAW:
    case FoeE::BOMBER: return 1;
    case FoeE::WELDER: return 2;
    case FoeE::GUNNER: return f.m_variant == 4 ? 0 : ( f.m_variant == 1 ? 1 : 2 );
    case FoeE::CREATURE:
    {
        if ( f.m_family < 0 || f.m_family >= FAMILIES ) return 0;
        ArchE a = Families[f.m_family].m_arch;
        return a == CASTER ? 2 : ( a == LUNGER || a == CHARGER || a == SWARM ) ? 1 : 0;
    }
    default: return 0;   // the guards, the heavies, the drones, the bosses and Mother's bodies: their own ways
    }
}

int Game::nativeTier()
{
    // Later acts bring the grown ones.
    const std::string &act = m_level.m_act;
    float              p1 = 0.15f, p2 = 0.0f;
    if ( act == "act.2" ) p1 = 0.4f, p2 = 0.05f;
    if ( act == "act.3" ) p1 = 0.55f, p2 = 0.2f;
    if ( act == "act.4" ) p1 = 0.5f, p2 = 0.4f;
    float r = rnd();
    return r < p2 ? 2 : ( r < p2 + p1 ? 1 : 0 );
}

void Game::spawnNative( int family, Vector3 p, int tier )
{
    if ( family < 0 || family >= FAMILIES ) return;
    const FamilyDef &d = Families[family];
    spawnFoe( FoeE::CREATURE, p, false );
    Foe &f = m_foes.back();
    f.m_family = family;
    f.m_tier = tier;
    f.m_hp = d.m_hp * TierHp[tier] * toughness( m_difficulty ) * areaTough() * m_foeHp;
    f.m_hpMax = f.m_hp;
    f.m_timer = 1.0f + rnd();
}

int Game::resist( const Foe &f, DamageE kind ) const
{
    int r[4] = { 0, 0, 0, 0 };
    switch ( f.m_kind )
    {
    case FoeE::LOUSE: r[2] = -50; break;
    case FoeE::WELDER: r[1] = 100, r[2] = -25; break;
    case FoeE::DRONE: r[2] = -50, r[3] = 25; break;
    case FoeE::SAW: r[0] = 25, r[2] = -50; break;
    case FoeE::BOMBER: r[1] = -50; break;
    case FoeE::SHELL: r[2] = -50, r[3] = -25; break;
    case FoeE::TAXATOR: break;
    case FoeE::HEAVY:
        if ( f.m_variant == 1 )
            r[0] = 25, r[2] = -50;   // a turret
        else
            r[0] = 25, r[2] = -25, r[3] = -25;
        break;
    case FoeE::CREATURE:
        if ( f.m_family >= 0 && f.m_family < FAMILIES )
            for ( int i = 0; i < 4; ++i ) r[i] = Families[f.m_family].m_resist[i];
        break;
    default: r[3] = -25; break;   // acid eats a suit
    }
    if ( hasGift( f, FIERY ) ) r[1] = 100;
    if ( hasGift( f, SHOCKING ) ) r[2] = 100;
    if ( hasGift( f, ICY ) ) r[1] = std::min( r[1], -25 );
    return std::clamp( r[(int)kind], -75, 100 );
}

int Game::weaponDamage( const Item &it )
{
    // What a weapon is made of: plasma and embers burn, arcs, mauls and sparks shock, acid eats, the rest strikes.
    const char *key = itemBases()[it.m_base].m_key;
    if ( strstr( key, "plasma" ) || strstr( key, "ember" ) ) return (int)DamageE::HEAT;
    if ( strstr( key, "arc" ) || strstr( key, "maul" ) || strstr( key, "spark" ) ) return (int)DamageE::SHOCK;
    if ( strstr( key, "acid" ) ) return (int)DamageE::ACID;
    return (int)DamageE::KINETIC;
}

Game::DamageE Game::heroDamage() const
{
    return m_progress.m_worn[0] ? (DamageE)weaponDamage( m_progress.m_gear[0] ) : DamageE::KINETIC;
}

float Game::creatureRadius( const Foe &f ) const
{
    float glitch = f.m_family == GLITCH ? powf( 0.75f, (float)f.m_stage ) : 1.0f;
    return Families[f.m_family].m_radius * TierSize[f.m_tier] * glitch;
}

float Game::creatureHeight( const Foe &f ) const
{
    return Families[f.m_family].m_height * TierSize[f.m_tier];
}

int Game::creatureXp( const Foe &f ) const
{
    const float more[3] = { 1.0f, 1.6f, 2.4f };
    return (int)( Families[f.m_family].m_xp * more[f.m_tier] * ( f.m_stage ? 0.5f : 1.0f ) );
}

float Game::damageScale( Foe &f, DamageE kind, Vector3 dir )
{
    float scale = fmaxf( 0.0f, 1.0f - resist( f, kind ) / 100.0f );
    // A crust crab's front is its shield.
    if ( f.m_kind == FoeE::CREATURE && f.m_family == CRAB )
    {
        Vector3 face = { sinf( f.m_yaw ), 0.0f, cosf( f.m_yaw ) };
        if ( -( face.x * dir.x + face.z * dir.z ) > 0.5f )
        {
            scale *= 0.35f;
            callout( f, "callout.front", { 200, 200, 210, 255 } );
            return scale;
        }
    }
    if ( scale >= 1.4f )
        callout( f, "callout.weak", { 255, 230, 90, 255 } );
    else if ( scale <= 0.0f )
        callout( f, "callout.immune", { 170, 170, 180, 255 } );
    else if ( scale <= 0.6f )
        callout( f, "callout.tough", { 190, 190, 200, 255 } );
    return scale;
}

void Game::callout( const Foe &f, const char *key, Color c )
{
    if ( f.m_callT > 0.0f ) return;
    const_cast<Foe &>( f ).m_callT = 2.5f;
    float top = f.m_kind == FoeE::DRONE ? 0.5f : foeHeight( f ) + 0.9f;
    m_callouts.push_back( { Vector3Add( f.m_pos, { 0.0f, top, 0.0f } ), tr( key ), c, 1.2f } );
}

// ------------------------------------------------------------------------------------------------- behaviour

void Game::updateCreature( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    const FamilyDef &d = Families[f.m_family];
    int              t = f.m_tier;
    float            speed = d.m_speed * ( 1.0f + 0.08f * t );
    float            hurt = d.m_hurt * TierHurt[t];
    Vector3          chest = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
    Vector3          me = Vector3Add( f.m_pos, { 0.0f, d.m_height * TierSize[t], 0.0f } );
    bool             sees = f.m_los && dist < 16.0f;
    f.m_phase += dt * ( 2.0f + Vector3Length( f.m_vel ) * 2.0f );
    f.m_spin += dt * 2.0f;
    auto blinkNear = [&]( Vector3 around, float reach ) -> bool
    {
        // A spot on the floor near `around` that it can see from there (it steps out of nothing).
        float start = rnd() * 2.0f * PI;
        for ( int i = 0; i < 8; ++i )
        {
            float   a = start + i * PI * 0.25f;
            Vector3 p = { around.x + cosf( a ) * reach, around.y, around.z + sinf( a ) * reach };
            float   g = groundAt( p );
            int     r, c;
            m_level.cellOf( p, r, c );
            if ( g < -100.0f || fabsf( g - around.y ) > 0.8f || !m_level.walkable( r, c ) ) continue;
            if ( !los( Vector3Add( around, { 0.0f, 1.0f, 0.0f } ), Vector3Add( p, { 0.0f, 1.0f, 0.0f } ) ) ) continue;
            sparks( Vector3Add( f.m_pos, { 0.0f, 1.0f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 14,
                    { (unsigned char)std::min( 255.0f, d.m_glow[t].x * 50.0f ),
                      (unsigned char)std::min( 255.0f, d.m_glow[t].y * 50.0f ),
                      (unsigned char)std::min( 255.0f, d.m_glow[t].z * 50.0f ), 255 },
                    2.5f );
            f.m_pos = { p.x, g, p.z };
            f.m_vel = { 0.0f, 0.0f, 0.0f };
            f.m_safe = f.m_pos;
            sparks( Vector3Add( f.m_pos, { 0.0f, 1.0f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 14, { 230, 230, 255, 255 },
                    2.5f );
            m_audio.play( SfxE::ZAP, f.m_pos, m_camTarget, 0.5f );
            return true;
        }
        return false;
    };
    auto volley = [&]( int count, float fan )
    {
        for ( int i = 0; i < count; ++i )
        {
            float   a = ( i - ( count - 1 ) * 0.5f ) * fan;
            Vector3 to = Vector3Subtract( chest, me );
            Vector3 at = Vector3Add( me, turnYaw( to, a ) );
            float   speed2 = d.m_shot == 4 ? 7.0f : ( d.m_shot == 2 ? 11.0f : ( d.m_shot == 3 ? 10.0f : 15.0f ) );
            enemyShot( me, at, speed2, hurt, d.m_shot, 0.4f );
        }
    };

    switch ( d.m_arch )
    {
    case CASTER:
    {
        // Keeps its distance and circles; winds up (its glow swells) and fires.
        float keep = f.m_family == DIVER ? 9.0f : 7.0f;
        faceYaw = atan2f( dir.x, dir.z );
        if ( f.m_state == FoeStateE::WINDUP )
        {
            f.m_torch = fminf( 1.0f, f.m_torch + dt * 2.5f );
            if ( f.m_timer <= 0.0f )
            {
                int count = f.m_family == CRYSTAL ? 1 + 2 * t : ( f.m_family == DIVER ? 1 + ( t == 2 ) : 1 + t );
                volley( count, f.m_family == CRYSTAL ? 0.16f : 0.25f );
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 0.5f;
                f.m_spit = ( 2.2f + rnd() ) * TierPace[t];
            }
            break;
        }
        f.m_torch = fmaxf( 0.0f, f.m_torch - dt * 1.5f );
        if ( f.m_state == FoeStateE::RECOVER && f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE;
        if ( !sees || dist > keep + 3.0f )
            want = Vector3Scale( path, speed );
        else if ( dist < keep - 2.0f )
            want = Vector3Scale( dir, -speed * 0.8f );
        else
        {
            float side = ( f.m_id % 2 ) ? 1.0f : -1.0f;
            want = { -dir.z * side * speed * 0.5f, 0.0f, dir.x * side * speed * 0.5f };
        }
        f.m_spit -= dt;
        if ( f.m_spit <= 0.0f && sees && dist < 15.0f && f.m_state == FoeStateE::CHASE )
        {
            f.m_state = FoeStateE::WINDUP;
            f.m_timer = 0.55f;
            m_audio.play( SfxE::CHITTER, f.m_pos, m_camTarget, 0.5f );
        }
        break;
    }
    case PHASER:
    {
        // Out of nothing beside the hero, a strike (or, a phantom, a volley from a little way off), gone again.
        faceYaw = atan2f( dir.x, dir.z );
        if ( f.m_state == FoeStateE::WINDUP )
        {
            f.m_torch = fminf( 1.0f, f.m_torch + dt * 3.0f );
            if ( f.m_timer <= 0.0f )
            {
                if ( f.m_family == PHANTOM )
                    volley( 1 + t, 0.2f );
                else if ( dist < 2.3f )
                {
                    hurtHero( hurt, { dir.x * 2.0f, 0.6f, dir.z * 2.0f }, f.m_pos );
                    m_audio.play( SfxE::SWING, f.m_pos, m_camTarget, 0.7f );
                }
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 0.35f;
            }
            break;
        }
        f.m_torch = fmaxf( 0.0f, f.m_torch - dt * 2.0f );
        if ( f.m_state == FoeStateE::RECOVER )
        {
            if ( f.m_timer <= 0.0f )
            {
                blinkNear( m_pos, 6.0f + rnd() * 2.0f );
                f.m_state = FoeStateE::CHASE;
                f.m_spit = ( 2.4f + rnd() * 1.2f ) * TierPace[t];
            }
            break;
        }
        want = !sees || dist > 9.0f ? Vector3Scale( path, speed ) : Vector3{ 0.0f, 0.0f, 0.0f };
        f.m_spit -= dt;
        if ( f.m_spit <= 0.0f && sees && dist < 12.0f )
        {
            bool near = f.m_family == PHANTOM ? blinkNear( m_pos, 4.5f ) : blinkNear( m_pos, 1.6f );
            if ( near || f.m_family == PHANTOM )
            {
                f.m_state = FoeStateE::WINDUP;
                f.m_timer = f.m_family == PHANTOM ? 0.5f : 0.4f;
            }
            else
                f.m_spit = 0.5f;
        }
        break;
    }
    case LUNGER:
    {
        // Runs in and leaps (an eel's bite shocks, a spider spits webs from afar, a crawler leaves fire behind).
        if ( f.m_family == MAGMA && f.m_ground && Vector3Length( f.m_vel ) > 0.5f )
        {
            f.m_giftT -= dt;
            if ( f.m_giftT <= 0.0f )
            {
                m_hazards.push_back( { f.m_pos, 0.7f + 0.1f * t, 2.5f, 2.5f, 7.0f * TierHurt[t], 0 } );
                f.m_giftT = 0.45f;
            }
        }
        if ( f.m_family == SPIDER && t >= 1 && f.m_state == FoeStateE::CHASE && sees && dist > 5.0f && dist < 11.0f )
        {
            f.m_spit -= dt;
            if ( f.m_spit <= 0.0f )
            {
                volley( 1, 0.0f );
                f.m_spit = 3.5f * TierPace[t];
            }
        }
        switch ( f.m_state )
        {
        case FoeStateE::CHASE:
        {
            Vector3 w = path;
            if ( f.m_family == EEL )
                w = Vector3Add( w, Vector3Scale( { -dir.z, 0.0f, dir.x }, sinf( f.m_phase ) * 0.4f ) );
            want = Vector3Scale( w, speed );
            faceYaw = atan2f( want.x, want.z );
            if ( dist < 4.2f && sees && f.m_ground && fabsf( m_pos.y - f.m_pos.y ) < 1.5f &&
                 !gapBetween( f.m_pos, m_pos ) )
            {
                f.m_state = FoeStateE::WINDUP;
                f.m_timer = 0.3f;
            }
            break;
        }
        case FoeStateE::WINDUP:
            faceYaw = atan2f( dir.x, dir.z );
            f.m_crouch = fminf( 1.0f, f.m_crouch + dt * 4.0f );
            if ( f.m_timer <= 0.0f )
            {
                f.m_state = FoeStateE::LEAP;
                f.m_vel = { dir.x * 8.0f, f.m_family == SPIDER ? 2.6f : 1.6f, dir.z * 8.0f };
                f.m_ground = false;
                f.m_timer = 3.0f;
                m_audio.play( SfxE::LEAP, f.m_pos, m_camTarget, 0.6f );
            }
            break;
        case FoeStateE::LEAP:
            faceYaw = atan2f( f.m_vel.x, f.m_vel.z );
            if ( Vector3Distance( me, chest ) < 1.2f )
            {
                hurtHero( hurt, { dir.x * 2.5f, 1.0f, dir.z * 2.5f }, f.m_pos );
                if ( f.m_family == EEL ) sparks( chest, { 0.0f, 1.0f, 0.0f }, 14, { 130, 210, 255, 255 }, 3.0f );
                if ( f.m_family == SPIDER ) m_chill = fmaxf( m_chill, 1.0f + 0.5f * t );
                if ( f.m_family == MAGMA ) m_scorch = fmaxf( m_scorch, 1.5f ), m_scorchDps = fmaxf( m_scorchDps, 5.0f );
                f.m_vel = { -dir.x * 2.0f, 1.0f, -dir.z * 2.0f };
            }
            if ( f.m_ground )
            {
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 0.55f * TierPace[t];
            }
            break;
        default:
            f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 3.0f );
            if ( f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE;
            break;
        }
        // A grown eel's charge jumps across to the hero now and then.
        if ( f.m_family == EEL && t >= 1 && sees && dist < 4.5f )
        {
            f.m_spit -= dt;
            if ( f.m_spit <= 0.0f )
            {
                m_tracers.push_back( { me, chest, 0.15f } );
                sparks( chest, { 0.0f, 1.0f, 0.0f }, 10, { 140, 210, 255, 255 }, 3.0f );
                hurtHero( hurt * 0.5f, { 0.0f, 0.0f, 0.0f }, f.m_pos );
                f.m_spit = 3.0f * TierPace[t];
            }
        }
        break;
    }
    case CHARGER:
    {
        // A wind-up, then a charge in a straight line; into a wall it stands dazed. Close by it swipes (a golem
        // slams the ground around it).
        switch ( f.m_state )
        {
        case FoeStateE::CHASE:
            want = Vector3Scale( path, speed );
            faceYaw = atan2f( path.x, path.z );
            f.m_spit -= dt;
            if ( dist < 2.2f && f.m_spit <= 0.0f )
            {
                f.m_state = FoeStateE::SPIT;   // the close blow
                f.m_timer = f.m_family == GOLEM ? 0.6f : 0.35f;
            }
            else if ( dist > 3.5f && dist < 10.0f && sees && f.m_timer <= 0.0f && !gapBetween( f.m_pos, m_pos ) )
            {
                f.m_state = FoeStateE::WINDUP;
                f.m_timer = 0.75f;
                f.m_rush = dir;
                m_audio.play( SfxE::CHITTER, f.m_pos, m_camTarget, 0.8f );
            }
            break;
        case FoeStateE::WINDUP:
            faceYaw = atan2f( dir.x, dir.z );
            f.m_rush = dir;
            f.m_crouch = fminf( 1.0f, f.m_crouch + dt * 2.0f );
            if ( rnd() < dt * 20.0f )
                m_particles.push_back(
                    { Vector3Add( f.m_pos, { ( rnd() - 0.5f ) * 0.8f, 0.05f, ( rnd() - 0.5f ) * 0.8f } ),
                      { -dir.x * 1.5f, 0.6f, -dir.z * 1.5f },
                      0.6f,
                      0.6f,
                      0.05f,
                      { 150, 130, 110, 200 },
                      1 } );
            if ( f.m_timer <= 0.0f )
            {
                f.m_state = FoeStateE::LEAP;
                f.m_timer = 1.1f;
                m_audio.play( SfxE::THRUST, f.m_pos, m_camTarget, 0.8f );
            }
            break;
        case FoeStateE::LEAP:
        {
            // The charge: fast and straight; it hits once.
            want = Vector3Scale( f.m_rush, speed * 3.4f );
            faceYaw = atan2f( f.m_rush.x, f.m_rush.z );
            f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 3.0f );
            float flat = Vector2Distance( { f.m_pos.x, f.m_pos.z }, { m_pos.x, m_pos.z } );
            if ( flat < foeRadius( f ) + 0.6f && fabsf( m_pos.y - f.m_pos.y ) < 1.6f )
            {
                hurtHero( hurt * 1.4f, { f.m_rush.x * 7.0f, 2.5f, f.m_rush.z * 7.0f }, f.m_pos );
                m_shake = fmaxf( m_shake, 0.4f );
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 0.8f;
                break;
            }
            float moved = Vector2Length( { f.m_vel.x, f.m_vel.z } );
            if ( f.m_timer < 0.85f && moved < speed * 0.8f )
            {
                // Into a wall: dazed.
                sparks(
                    Vector3Add( f.m_pos, Vector3Add( Vector3Scale( f.m_rush, foeRadius( f ) ), { 0.0f, 0.8f, 0.0f } ) ),
                    Vector3Negate( f.m_rush ), 18, { 255, 220, 160, 255 }, 3.0f );
                m_audio.play( SfxE::SLAM, f.m_pos, m_camTarget, 0.8f );
                f.m_stun = fmaxf( f.m_stun, 1.3f );
                callout( f, "callout.dazed", { 255, 240, 200, 255 } );
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 0.3f;
                break;
            }
            if ( f.m_timer <= 0.0f )
            {
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 0.6f;
            }
            break;
        }
        case FoeStateE::SPIT:
            // The close blow (a golem's slam reaches all round it).
            faceYaw = atan2f( dir.x, dir.z );
            f.m_crouch = fminf( 1.0f, f.m_crouch + dt * 3.0f );
            if ( f.m_timer <= 0.0f )
            {
                float reach = f.m_family == GOLEM ? 2.8f : 2.5f;
                if ( dist < reach && fabsf( m_pos.y - f.m_pos.y ) < 1.5f )
                    hurtHero( hurt, { dir.x * 3.0f, 1.0f, dir.z * 3.0f }, f.m_pos );
                if ( f.m_family == GOLEM )
                {
                    m_shake = fmaxf( m_shake, 0.35f );
                    m_audio.play( SfxE::SLAM, f.m_pos, m_camTarget, 1.0f );
                    for ( int i = 0; i < 24; ++i )
                    {
                        float a = i * 2.0f * PI / 24.0f;
                        m_particles.push_back( { Vector3Add( f.m_pos, { 0.0f, 0.1f, 0.0f } ),
                                                 { cosf( a ) * 4.0f, 0.5f, sinf( a ) * 4.0f },
                                                 0.5f,
                                                 0.5f,
                                                 0.06f,
                                                 { 255, 150, 60, 255 },
                                                 3 } );
                    }
                    if ( t >= 1 ) m_hazards.push_back( { f.m_pos, 1.6f, 2.5f, 2.5f, 8.0f * TierHurt[t], 0 } );
                }
                else
                    m_audio.play( SfxE::SWING, f.m_pos, m_camTarget, 0.7f );
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 0.5f;
                f.m_spit = 1.2f * TierPace[t];
            }
            break;
        default:
            f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 3.0f );
            if ( f.m_timer <= 0.0f )
            {
                f.m_state = FoeStateE::CHASE;
                f.m_timer = ( 2.5f + rnd() * 1.5f ) * TierPace[t];
            }
            break;
        }
        break;
    }
    case ROOTED:
    {
        // It grew here: whips what comes close, spits at what keeps away.
        f.m_vel = { 0.0f, 0.0f, 0.0f };
        want = { 0.0f, 0.0f, 0.0f };
        faceYaw = atan2f( dir.x, dir.z );
        if ( f.m_state == FoeStateE::WINDUP )
        {
            f.m_crouch = fminf( 1.0f, f.m_crouch + dt * 4.0f );
            if ( f.m_timer <= 0.0f )
            {
                if ( dist < 4.4f ) hurtHero( hurt, { dir.x * 3.0f, 0.8f, dir.z * 3.0f }, f.m_pos );
                m_audio.play( SfxE::SWING, f.m_pos, m_camTarget, 0.7f );
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 1.2f * TierPace[t];
            }
            break;
        }
        f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 2.5f );
        if ( f.m_state == FoeStateE::RECOVER && f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE;
        if ( f.m_state != FoeStateE::CHASE ) break;
        f.m_spit -= dt;
        if ( dist < 4.0f )
        {
            f.m_state = FoeStateE::WINDUP;
            f.m_timer = 0.35f;
        }
        else if ( sees && dist < 12.0f && f.m_spit <= 0.0f )
        {
            volley( 1 + ( t == 2 ), 0.2f );
            f.m_spit = 2.6f * TierPace[t];
        }
        break;
    }
    case SWARM:
    default:
    {
        // Quick, zigzagging, it bites on touch.
        Vector3 side = { -dir.z, 0.0f, dir.x };
        want = Vector3Scale( Vector3Add( path, Vector3Scale( side, sinf( f.m_phase * 1.7f ) * 0.8f ) ), speed );
        faceYaw = atan2f( want.x, want.z );
        f.m_spit -= dt;
        if ( dist < 0.9f && f.m_spit <= 0.0f && fabsf( m_pos.y - f.m_pos.y ) < 1.4f )
        {
            hurtHero( hurt, { dir.x, 0.3f, dir.z }, f.m_pos );
            sparks( chest, Vector3Negate( dir ), 8, { 255, 120, 240, 255 }, 2.5f );
            f.m_spit = 0.7f;
        }
        break;
    }
    }
}

void Game::stepCreatureLegs( Foe &f, float dt )
{
    if ( f.m_family != SPIDER && f.m_family != CRAB ) return;
    LousePose pose;
    pose.m_pos = f.m_pos;
    pose.m_yaw = f.m_yaw;
    pose.m_air = !f.m_ground;
    pose.m_crouch = f.m_crouch;
    ( f.m_family == SPIDER ? m_spider : m_crab )
        .stepLegs( f.m_legs, pose, f.m_vel, f.m_ground ? f.m_pos.y : groundAt( f.m_pos ), dt );
}

void Game::poseCreatures( std::vector<Drawable> &out )
{
    auto add = [&]( Mesh mesh, int surface, Matrix at, Vector4 glow = { -1, 0, 0, 0 } )
    {
        if ( mesh.vertexCount == 0 ) return;
        Drawable d;
        d.m_mesh = mesh;
        d.m_surface = surface;
        d.m_transform = at;
        d.m_emissive = glow;
        out.push_back( d );
    };
    for ( const Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden || f.m_kind != FoeE::CREATURE ) continue;
        const FamilyDef  &d = Families[f.m_family];
        const FamilyLook &look = m_familyLook[f.m_family][f.m_tier];
        int               t = f.m_tier;
        float             size = TierSize[t] * ( f.m_family == GLITCH ? powf( 0.75f, (float)f.m_stage ) : 1.0f );
        float             flash = f.m_flash * 3.0f;
        float             swell = 1.0f + f.m_torch * 1.5f;
        Vector4 glow = { d.m_glow[t].x * swell + flash, d.m_glow[t].y * swell + flash, d.m_glow[t].z * swell + flash,
                         0.0f };
        Matrix  turn = MatrixRotateY( f.m_yaw );
        auto    at = [&]( float lift )
        {
            return MatrixMultiply( MatrixMultiply( MatrixScale( size, size, size ), turn ),
                                   MatrixTranslate( f.m_pos.x, f.m_pos.y + lift, f.m_pos.z ) );
        };
        float bob = sinf( m_time * 2.0f + f.m_id ) * 0.08f;
        switch ( f.m_family )
        {
        case CRYSTAL:
        {
            Matrix m = MatrixMultiply( MatrixRotateY( f.m_spin * 0.3f ), at( 0.9f + bob ) );
            add( look.m_body, S_ICE, m );
            add( look.m_glow, S_GLOW, m, glow );
            if ( t >= 1 ) add( look.m_part, S_ICE, MatrixMultiply( MatrixRotateY( -f.m_spin * 1.2f ), m ) );
            break;
        }
        case SHADE:
        case PHANTOM:
        {
            Matrix m = at( 0.12f + bob * 0.5f );
            bool   phantom = f.m_family == PHANTOM;
            // A phantom flickers like a bad picture.
            float flicker = phantom ? ( fmodf( m_time * 13.0f + f.m_id * 0.37f, 1.0f ) < 0.12f ? 0.25f : 1.0f ) : 1.0f;
            Vector4 g = { glow.x * flicker, glow.y * flicker, glow.z * flicker, 0.0f };
            if ( !phantom ) add( look.m_body, S_DARK, m );
            add( look.m_glow, S_GLOW, m, phantom ? Vector4{ g.x * 0.15f, g.y * 0.15f, g.z * 0.15f, 0.0f } : g );
            if ( !phantom )
                for ( float side : { -1.0f, 1.0f } )
                {
                    float swing =
                        f.m_state == FoeStateE::WINDUP ? -1.2f * f.m_torch : 0.25f * sinf( m_time * 2.0f + side );
                    Matrix arm =
                        MatrixMultiply( MatrixRotateX( swing ), MatrixTranslate( side * 0.26f, 1.28f, 0.05f ) );
                    add( look.m_part, S_DARK, MatrixMultiply( arm, m ) );
                }
            break;
        }
        case EEL:
        case MAGMA:
        {
            // The head where it is; the body wriggles out behind it.
            Matrix m = at( f.m_family == EEL ? 0.22f : 0.25f );
            add( look.m_body, f.m_family == EEL ? S_DARK : S_ROCK, m );
            add( look.m_glow, S_GLOW, m, glow );
            int n = f.m_family == EEL ? 7 : 5;
            for ( int i = 1; i <= n; ++i )
            {
                float  k = (float)i / n;
                float  x = sinf( f.m_phase * 2.0f - i * 0.8f ) * 0.18f * ( 0.4f + k );
                float  s = 1.0f - 0.45f * k;
                Matrix seg =
                    MatrixMultiply( MatrixMultiply( MatrixScale( s, s, s ),
                                                    MatrixRotateY( cosf( f.m_phase * 2.0f - i * 0.8f ) * 0.4f ) ),
                                    MatrixTranslate( x, -0.02f * i, -0.3f * i ) );
                add( look.m_part, f.m_family == EEL ? S_DARK : S_ROCK, MatrixMultiply( seg, m ) );
                if ( i % 2 == 0 )
                    add( look.m_glow, S_GLOW,
                         MatrixMultiply(
                             MatrixMultiply( MatrixScale( s, s, s ), MatrixTranslate( x, 0.0f, -0.3f * i + 0.36f ) ),
                             m ),
                         glow );
            }
            break;
        }
        case DIVER:
        {
            Matrix m = MatrixMultiply( MatrixRotateX( Clamp( -f.m_vel.y * 0.1f, -0.3f, 0.3f ) ), at( 0.55f + bob ) );
            add( look.m_body, S_METAL, m );
            add( look.m_glow, S_GLOW, m, glow );
            add( look.m_part, S_DARK,
                 MatrixMultiply(
                     MatrixMultiply( MatrixRotateZ( f.m_spin * 6.0f ), MatrixTranslate( 0.0f, 0.0f, -0.62f ) ), m ) );
            break;
        }
        case BEAST:
        {
            float  lower = -0.2f * f.m_crouch;
            Matrix m = at( lower );
            add( look.m_body, S_ARMOR, m );
            add( look.m_glow, S_GLOW, m, glow );
            const Vector3 hips[4] = {
                { -0.3f, 0.62f, 0.5f }, { 0.3f, 0.62f, 0.5f }, { -0.3f, 0.62f, -0.5f }, { 0.3f, 0.62f, -0.5f }
            };
            for ( int i = 0; i < 4; ++i )
            {
                float  swing = sinf( f.m_phase + ( i == 0 || i == 3 ? 0.0f : PI ) ) * 0.5f *
                               fminf( 1.0f, Vector3Length( f.m_vel ) / 3.0f );
                Matrix leg = MatrixMultiply( MatrixRotateX( swing ),
                                             MatrixTranslate( hips[i].x, hips[i].y - lower, hips[i].z ) );
                add( look.m_part, S_ARMOR, MatrixMultiply( leg, m ) );
            }
            break;
        }
        case VINE:
        {
            Matrix m = at( 0.0f );
            add( look.m_body, S_CLOTH2, m );
            add( look.m_glow, S_GLOW, m, glow );
            for ( int i = 0; i < 2 + t; ++i )
            {
                float  a = i * 2.0f * PI / ( 2 + t ) + sinf( m_time * 1.5f + i ) * 0.4f;
                float  lash = f.m_state == FoeStateE::WINDUP ? -0.6f + 1.4f * f.m_crouch
                                                             : 0.3f * sinf( m_time * 2.0f + i * 1.3f );
                Matrix whip = MatrixMultiply( MatrixMultiply( MatrixRotateX( lash ), MatrixRotateY( a ) ),
                                              MatrixTranslate( 0.0f, 0.9f + 0.15f * i, 0.0f ) );
                add( look.m_part, S_CLOTH2, MatrixMultiply( whip, m ) );
            }
            break;
        }
        case FROST:
        {
            Matrix m = at( 1.1f + bob );
            add( look.m_body, S_ICE, MatrixMultiply( MatrixRotateY( f.m_spin * 0.5f ), m ) );
            add( look.m_glow, S_GLOW, m, glow );
            add( look.m_part, S_METAL, MatrixMultiply( MatrixRotateX( f.m_spin ), m ) );
            if ( t >= 1 ) add( look.m_part, S_METAL, MatrixMultiply( MatrixRotateZ( f.m_spin * 1.3f ), m ) );
            break;
        }
        case SPIDER:
        case CRAB:
        {
            LousePose pose;
            pose.m_pos = f.m_pos;
            pose.m_yaw = f.m_yaw;
            pose.m_crouch = f.m_crouch;
            pose.m_air = !f.m_ground;
            pose.m_pitch = f.m_ground ? -0.15f * f.m_crouch : Clamp( f.m_vel.y * 0.15f, -0.5f, 0.5f );
            pose.m_flash = f.m_flash;
            pose.m_time = m_time + f.m_id;
            const LouseModel &model = f.m_family == SPIDER ? m_spider : m_crab;
            model.draw( pose, f.m_legs, out );
            Matrix m = MatrixMultiply( MatrixRotateY( f.m_yaw ),
                                       MatrixTranslate( f.m_pos.x, f.m_pos.y - 0.1f * f.m_crouch, f.m_pos.z ) );
            add( look.m_body, f.m_family == SPIDER ? S_ICE : S_RUST, m );
            add( look.m_glow, S_GLOW, m, glow );
            break;
        }
        case GOLEM:
        {
            Matrix m = at( -0.15f * f.m_crouch );
            add( look.m_body, S_ROCK, m );
            add( look.m_glow, S_GLOW, m, glow );
            float walk = sinf( f.m_phase ) * 0.4f * fminf( 1.0f, Vector3Length( f.m_vel ) / 2.0f );
            float raise = f.m_state == FoeStateE::SPIT ? -2.4f * f.m_crouch : 0.0f;
            for ( float side : { -1.0f, 1.0f } )
            {
                add( look.m_part, S_ROCK,
                     MatrixMultiply( MatrixMultiply( MatrixRotateX( walk * side + raise ),
                                                     MatrixTranslate( side * 0.78f, 2.1f, 0.0f ) ),
                                     m ) );
                add( look.m_part, S_ROCK,
                     MatrixMultiply(
                         MatrixMultiply( MatrixRotateX( -walk * side ), MatrixTranslate( side * 0.25f, 0.95f, 0.0f ) ),
                         m ) );
            }
            break;
        }
        case GLITCH:
        default:
        {
            Matrix m =
                MatrixMultiply( MatrixMultiply( MatrixRotateX( f.m_spin * 2.0f ), MatrixRotateZ( f.m_spin * 1.4f ) ),
                                at( 0.35f + bob ) );
            float jitter = fmodf( m_time * 17.0f + f.m_id, 1.0f ) < 0.15f ? 0.2f : 1.0f;
            add( look.m_body, S_DARK, m );
            add( look.m_glow, S_GLOW, m, { glow.x * jitter, glow.y * jitter, glow.z * jitter, 0.0f } );
            break;
        }
        }
    }
}

void Game::creatureDown( Foe &f, bool drops )
{
    // Each dies its own way; machines and crabs leave scrap.
    const FamilyDef &d = Families[f.m_family];
    Vector3          mid = Vector3Add( f.m_pos, { 0.0f, d.m_height * TierSize[f.m_tier], 0.0f } );
    Color            c = { (unsigned char)std::min( 255.0f, d.m_glow[f.m_tier].x * 55.0f ),
                           (unsigned char)std::min( 255.0f, d.m_glow[f.m_tier].y * 55.0f ),
                           (unsigned char)std::min( 255.0f, d.m_glow[f.m_tier].z * 55.0f ), 255 };
    Vector3          v = { ( rnd() - 0.5f ) * 2.0f, 2.5f, ( rnd() - 0.5f ) * 2.0f };
    switch ( f.m_family )
    {
    case DIVER:
    case MAGMA: detonate( f, f.m_family == MAGMA ? 20.0f * TierHurt[f.m_tier] : 16.0f, 2.6f ); break;
    case FROST:
        sparks( mid, { 0.0f, 1.0f, 0.0f }, 40, { 220, 240, 255, 255 }, 5.0f );
        m_hazards.push_back( { f.m_pos, 2.2f, 3.0f, 3.0f, 0.0f, 2 } );
        m_audio.play( SfxE::BURST, mid, m_camTarget, 0.8f );
        break;
    case GLITCH:
        sparks( mid, { 0.0f, 1.0f, 0.0f }, 20, c, 4.0f );
        // It falls apart into two smaller ones (more often as it grows).
        if ( f.m_stage < ( f.m_tier >= 1 ? 2 : 1 ) )
            for ( int i = 0; i < 2; ++i )
            {
                Summon g{ FoeE::CREATURE, Vector3Add( f.m_pos, { ( i ? 0.4f : -0.4f ), 0.0f, 0.0f } ) };
                g.family = GLITCH;
                g.tier = f.m_tier;
                g.stage = f.m_stage + 1;
                g.hp = f.m_hpMax * 0.45f;
                g.vel = { ( i ? 2.0f : -2.0f ), 1.5f, 0.0f };
                m_summons.push_back( g );
            }
        break;
    case VINE:
        sparks( mid, { 0.0f, 1.0f, 0.0f }, 30, { 150, 255, 120, 255 }, 3.0f );
        if ( f.m_tier == 2 ) m_hazards.push_back( { f.m_pos, 2.0f, 4.0f, 4.0f, 6.0f, 1 } );
        break;
    case GOLEM:
        explode( mid, 1.2f );
        if ( f.m_tier >= 1 ) m_hazards.push_back( { f.m_pos, 1.8f, 3.0f, 3.0f, 8.0f, 0 } );
        break;
    default:
        sparks( mid, { 0.0f, 1.0f, 0.0f }, 30, c, 4.0f );
        m_particles.push_back( { mid, { 0.0f, 0.5f, 0.0f }, 0.5f, 0.5f, 0.6f * TierSize[f.m_tier], c, 2 } );
        m_audio.play( SfxE::POP, mid, m_camTarget, 0.8f );
        break;
    }
    if ( !drops ) return;
    float r = rnd();
    if ( f.m_family == DIVER || f.m_family == CRAB || f.m_family == GOLEM )
        m_pickups.push_back(
            { 0, 6 + (int)( rnd() * 10 ) * ( 1 + f.m_tier ), Vector3Add( f.m_pos, { 0, 0.4f, 0 } ), v } );
    else if ( r < 0.45f )
        m_pickups.push_back( { 0, 3 + (int)( rnd() * 6 ), Vector3Add( f.m_pos, { 0, 0.4f, 0 } ), v } );
    else if ( r < 0.58f )
        m_pickups.push_back( { 1, 1, Vector3Add( f.m_pos, { 0, 0.4f, 0 } ), v } );
}

// ------------------------------------------------------------------------------------------------- the gifts

bool Game::wardTakes( Foe &f, float &amount )
{
    // A shielded champion's shield takes the hit first.
    if ( !hasGift( f, SHIELDED ) || f.m_ward <= 0.0f ) return false;
    f.m_wardT = 0.0f;
    float taken = fminf( f.m_ward, amount );
    f.m_ward -= taken;
    amount -= taken;
    Vector3 mid = Vector3Add( f.m_pos, { 0.0f, f.m_kind == FoeE::DRONE ? 0.0f : foeHeight( f ), 0.0f } );
    sparks( mid, { 0.0f, 1.0f, 0.0f }, 4, { 200, 230, 255, 255 }, 2.0f );
    if ( f.m_ward <= 0.0f )
    {
        sparks( mid, { 0.0f, 1.0f, 0.0f }, 30, { 220, 240, 255, 255 }, 5.0f );
        m_audio.play( SfxE::BURST, mid, m_camTarget, 0.7f );
    }
    return amount <= 0.0f;
}

void Game::updateGifts( Foe &f, int index, float dt, float dist, Vector3 dir )
{
    f.m_callT -= dt;
    if ( !f.m_elite ) return;
    f.m_giftT -= dt;
    f.m_wardT += dt;
    // The shield comes back once nothing has hit it for a while.
    if ( hasGift( f, SHIELDED ) && f.m_wardT > 3.0f )
        f.m_ward = fminf( f.m_hpMax * 0.35f, f.m_ward + f.m_hpMax * 0.05f * dt );
    // Icy: the cold round it chills the hero.
    if ( hasGift( f, ICY ) && dist < 4.0f )
    {
        m_chill = fmaxf( m_chill, 0.4f );
        if ( rnd() < dt * 10.0f )
            m_particles.push_back( { Vector3Add( f.m_pos, { ( rnd() - 0.5f ) * 3.0f, 0.2f, ( rnd() - 0.5f ) * 3.0f } ),
                                     { 0.0f, 0.4f, 0.0f },
                                     0.8f,
                                     0.8f,
                                     0.04f,
                                     { 210, 240, 255, 200 },
                                     1 } );
    }
    // Fiery: its steps burn.
    if ( hasGift( f, FIERY ) && Vector3Length( f.m_vel ) > 0.6f && f.m_ground && fmodf( f.m_giftT, 0.4f ) < dt )
        m_hazards.push_back( { f.m_pos, 0.8f, 3.0f, 3.0f, 8.0f, 0 } );
    if ( !f.m_awake || f.m_giftT > 0.0f ) return;
    Vector3 mid = Vector3Add( f.m_pos, { 0.0f, f.m_kind == FoeE::DRONE ? 0.0f : foeHeight( f ), 0.0f } );
    Vector3 chest = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
    if ( hasGift( f, SHOCKING ) && dist < 5.5f && f.m_los )
    {
        // A zap across to the hero.
        m_tracers.push_back( { mid, chest, 0.15f } );
        sparks( chest, Vector3Negate( dir ), 12, { 140, 220, 255, 255 }, 3.0f );
        m_audio.play( SfxE::ZAP, mid, m_camTarget, 0.7f );
        hurtHero( 7.0f, { 0.0f, 0.0f, 0.0f }, f.m_pos );
        f.m_giftT = 2.2f;
        return;
    }
    if ( hasGift( f, BLINKING ) && dist > 4.0f && dist < 15.0f && f.m_kind != FoeE::DRONE &&
         !( f.m_kind == FoeE::CREATURE && Families[f.m_family].m_arch == ROOTED ) )
    {
        // A step through nothing to beside the hero.
        float start = rnd() * 2.0f * PI;
        for ( int i = 0; i < 8; ++i )
        {
            float   a = start + i * PI * 0.25f;
            Vector3 p = { m_pos.x + cosf( a ) * 3.0f, m_pos.y, m_pos.z + sinf( a ) * 3.0f };
            float   g = groundAt( p );
            int     r, c;
            m_level.cellOf( p, r, c );
            if ( g < -100.0f || fabsf( g - m_pos.y ) > 0.8f || !m_level.walkable( r, c ) ) continue;
            sparks( mid, { 0.0f, 1.0f, 0.0f }, 16, { 200, 120, 255, 255 }, 3.0f );
            f.m_pos = { p.x, g, p.z };
            f.m_safe = f.m_pos;
            f.m_vel = { 0.0f, 0.0f, 0.0f };
            sparks( Vector3Add( f.m_pos, { 0.0f, 1.0f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 16, { 200, 120, 255, 255 },
                    3.0f );
            m_audio.play( SfxE::ZAP, f.m_pos, m_camTarget, 0.6f );
            break;
        }
        f.m_giftT = 4.5f;
        return;
    }
    if ( hasGift( f, SUMMONER ) && dist < 16.0f )
    {
        // Calls helpers (as many as four at a time).
        int called = 0;
        for ( const Foe &o : m_foes )
            if ( o.m_alive && o.m_leader == index && !o.m_elite ) ++called;
        if ( called < 4 )
        {
            for ( int i = 0; i < 2; ++i )
            {
                // Its own kind, small (a creature), drones (a man), saw bots (a machine).
                Vector3 at = Vector3Add( f.m_pos, { ( i ? 0.9f : -0.9f ), 0.0f, 0.6f } );
                Summon  o{ human( f ) ? FoeE::DRONE : FoeE::SAW,
                           Vector3Add( at, { 0.0f, human( f ) ? 1.75f : 0.0f, 0.0f } ) };
                if ( f.m_kind == FoeE::CREATURE && Families[f.m_family].m_arch != ROOTED )
                {
                    o.first = FoeE::CREATURE;
                    o.family = f.m_family;
                    o.share = 0.4f;
                }
                o.leader = index;
                m_summons.push_back( o );
                sparks( Vector3Add( at, { 0.0f, 0.6f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 14, { 255, 90, 220, 255 }, 3.0f );
            }
            m_audio.play( SfxE::CHITTER, f.m_pos, m_camTarget, 0.9f );
        }
        f.m_giftT = 9.0f;
        return;
    }
    f.m_giftT = 1.0f;
}

void Game::giftsOnDeath( Foe &f )
{
    int index = (int)( &f - m_foes.data() );
    // A leader falls: its escort falters.
    if ( f.m_elite == 3 )
    {
        for ( Foe &o : m_foes )
            if ( o.m_alive && o.m_leader == index )
            {
                o.m_stun = fmaxf( o.m_stun, 1.6f );
                o.m_leader = -1;
            }
        callout( f, "callout.escort", { 255, 210, 120, 255 } );
    }
    if ( !hasGift( f, SPLITTING ) ) return;
    // It splits: two smaller ones of its kind (a man's drones leave his back).
    for ( int i = 0; i < 2; ++i )
    {
        Vector3 at = Vector3Add( f.m_pos, { ( i ? 0.7f : -0.7f ), 0.0f, 0.3f } );
        Summon  o{ f.m_kind, at, f.m_variant };
        if ( human( f ) || f.m_kind == FoeE::TAXATOR )
            o = Summon{ FoeE::DRONE, Vector3Add( at, { 0.0f, 1.75f, 0.0f } ) };
        else if ( f.m_kind == FoeE::SHELL )
            o = Summon{ FoeE::SAW, at };
        if ( f.m_kind == FoeE::CREATURE ) o.family = f.m_family, o.tier = f.m_tier, o.stage = f.m_stage;
        o.share = 0.35f;
        m_summons.push_back( o );
    }
    sparks( Vector3Add( f.m_pos, { 0.0f, 0.8f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 30, { 200, 255, 80, 255 }, 4.0f );
}

void Game::assignEscorts()
{
    // Who stands with a leader when the level starts is its escort.
    for ( int i = 0; i < (int)m_foes.size(); ++i )
    {
        if ( m_foes[i].m_elite != 3 ) continue;
        for ( Foe &o : m_foes )
            if ( &o != &m_foes[i] && !o.m_elite && o.m_leader < 0 &&
                 Vector3Distance( o.m_pos, m_foes[i].m_pos ) < 5.5f )
                o.m_leader = i;
    }
}

// ------------------------------------------------------------------------------------------------- shots, ground

void Game::enemyShot( Vector3 muzzle, Vector3 at, float speed, float hurt, int kind, float spread )
{
    Vector3 aim =
        Vector3Add( at, { ( rnd() - 0.5f ) * spread, ( rnd() - 0.5f ) * spread * 0.4f, ( rnd() - 0.5f ) * spread } );
    Shot s;
    s.m_pos = muzzle;
    s.m_vel = Vector3Scale( Vector3Normalize( Vector3Subtract( aim, muzzle ) ), speed );
    s.m_life = 2.4f;
    s.m_hurt = hurt * m_hitScale;
    s.m_kind = kind;
    m_shotsInFlight.push_back( s );
    m_audio.play( kind == 4 ? SfxE::THROW : ( kind == 3 ? SfxE::SPIT : SfxE::ZAP ), muzzle, m_camTarget, 0.45f );
}

void Game::shotEffect( const Shot &s )
{
    // What a creature's shot does besides hurting.
    switch ( s.m_kind )
    {
    case 2:
    case 5: m_chill = fmaxf( m_chill, s.m_kind == 5 ? 2.0f : 1.5f ); break;
    case 3: m_corrode = fmaxf( m_corrode, 3.0f ); break;
    case 4:
    {
        // A torpedo bursts.
        Vector3 p = s.m_pos;
        explode( p, 1.0f );
        float d = Vector3Distance( p, Vector3Add( m_pos, { 0.0f, 0.9f, 0.0f } ) );
        if ( d < 2.6f && !m_dead )
        {
            Vector3 away = Vector3Normalize( { m_pos.x - p.x + 1e-3f, 0.0f, m_pos.z - p.z } );
            hurtHero( s.m_hurt * ( 1.0f - 0.5f * d / 2.6f ), { away.x * 4.0f, 1.5f, away.z * 4.0f }, p );
        }
        break;
    }
    default: break;
    }
}

void Game::updateHazards( float dt )
{
    // Burning, corroding, freezing ground; the words over enemies rise and fade.
    for ( size_t i = 0; i < m_hazards.size(); )
    {
        Hazard &h = m_hazards[i];
        h.m_life -= dt;
        if ( h.m_life <= 0.0f )
        {
            m_hazards[i] = m_hazards.back();
            m_hazards.pop_back();
            continue;
        }
        float flat = Vector2Distance( { h.m_pos.x, h.m_pos.z }, { m_pos.x, m_pos.z } );
        if ( flat < h.m_radius && fabsf( m_pos.y - h.m_pos.y ) < 0.8f && !m_dead )
        {
            if ( h.m_kind == 0 ) m_scorch = fmaxf( m_scorch, 1.0f ), m_scorchDps = fmaxf( m_scorchDps, h.m_dps );
            if ( h.m_kind == 1 )
                m_corrode = fmaxf( m_corrode, 2.0f ), m_scorch = fmaxf( m_scorch, 0.6f ),
                m_scorchDps = fmaxf( m_scorchDps, h.m_dps * 0.6f );
            if ( h.m_kind == 2 ) m_chill = fmaxf( m_chill, 1.0f );
        }
        if ( rnd() < dt * 6.0f * h.m_radius )
        {
            float a = rnd() * 2.0f * PI, r = sqrtf( rnd() ) * h.m_radius;
            Color c = h.m_kind == 0 ? Color{ 255, 140, 40, 255 }
                                    : ( h.m_kind == 1 ? Color{ 150, 255, 90, 220 } : Color{ 210, 240, 255, 200 } );
            m_particles.push_back( { Vector3Add( h.m_pos, { cosf( a ) * r, 0.05f, sinf( a ) * r } ),
                                     { 0.0f, 1.0f, 0.0f },
                                     0.7f,
                                     0.7f,
                                     0.03f,
                                     c,
                                     h.m_kind == 0 ? 3 : 1 } );
        }
        ++i;
    }
    if ( m_hazards.size() > 160 ) m_hazards.erase( m_hazards.begin(), m_hazards.begin() + ( m_hazards.size() - 160 ) );
    for ( size_t i = 0; i < m_callouts.size(); )
    {
        m_callouts[i].m_life -= dt;
        m_callouts[i].m_pos.y += dt * 0.8f;
        if ( m_callouts[i].m_life <= 0.0f )
        {
            m_callouts[i] = m_callouts.back();
            m_callouts.pop_back();
        }
        else
            ++i;
    }
}

void Game::updateAilments( float dt )
{
    // Chilled: slower (Game::runSpeed). Burning: it eats at the hero in ticks. Corroded: the armour gives less.
    m_chill = fmaxf( 0.0f, m_chill - dt );
    m_corrode = fmaxf( 0.0f, m_corrode - dt );
    if ( m_scorch > 0.0f )
    {
        float before = m_scorch;
        m_scorch = fmaxf( 0.0f, m_scorch - dt );
        if ( floorf( before * 2.0f ) != floorf( m_scorch * 2.0f ) && !m_dead )
            hurtHero( m_scorchDps * 0.5f, { 0.0f, 0.0f, 0.0f }, m_pos );
        if ( rnd() < dt * 12.0f )
            m_particles.push_back(
                { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.5f, 0.3f + rnd() * 1.2f, ( rnd() - 0.5f ) * 0.5f } ),
                  { 0.0f, 1.2f, 0.0f },
                  0.5f,
                  0.5f,
                  0.02f,
                  { 255, 140, 50, 255 },
                  3 } );
        if ( m_scorch <= 0.0f ) m_scorchDps = 0.0f;
    }
    if ( m_chill > 0.0f && rnd() < dt * 8.0f )
        m_particles.push_back(
            { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.6f, 0.1f + rnd() * 0.6f, ( rnd() - 0.5f ) * 0.6f } ),
              { 0.0f, 0.3f, 0.0f },
              0.8f,
              0.8f,
              0.03f,
              { 210, 240, 255, 200 },
              1 } );
}

void Game::poseHazards( std::vector<Drawable> &out )
{
    for ( const Hazard &h : m_hazards )
    {
        float    fade = fminf( 1.0f, h.m_life / fmaxf( 0.01f, h.m_max * 0.4f ) );
        float    flick = 0.8f + 0.2f * sinf( m_time * 9.0f + h.m_pos.x * 3.0f );
        Vector4  e = h.m_kind == 0   ? Vector4{ 3.0f * fade * flick, 1.0f * fade * flick, 0.15f * fade, 0.0f }
                     : h.m_kind == 1 ? Vector4{ 0.6f * fade, 2.4f * fade * flick, 0.4f * fade, 0.0f }
                                     : Vector4{ 1.2f * fade, 1.8f * fade, 2.6f * fade, 0.0f };
        Drawable d;
        d.m_mesh = m_hazardDisc;
        d.m_surface = S_GLOW;
        d.m_transform = MatrixMultiply( MatrixScale( h.m_radius, 1.0f, h.m_radius ),
                                        MatrixTranslate( h.m_pos.x, h.m_pos.y + 0.02f, h.m_pos.z ) );
        d.m_emissive = e;
        d.m_sunShadow = d.m_spotShadow = false;
        out.push_back( d );
    }
}

// ------------------------------------------------------------------------------------------------- words

std::string Game::foeName( const Foe &f ) const
{
    // The gifts agree with the name (German: "Eisiger, rasender Kristallwächter", "Gepanzerte Hülle").
    const char *noun = foeNoun( f );
    std::string g = hasText( TextFormat( "%s.g", noun ) ) ? tr( TextFormat( "%s.g", noun ) ) : "";
    const char *form = g == "f" ? "elite.%d.f" : ( g == "n" ? "elite.%d.n" : "elite.%d" );
    std::string name;
    for ( int gift : { f.m_affix, f.m_affix2 } )
    {
        if ( gift < 0 ) continue;
        std::string word =
            hasText( TextFormat( form, gift ) ) ? tr( TextFormat( form, gift ) ) : tr( TextFormat( "elite.%d", gift ) );
        if ( !name.empty() )
        {
            name += ", ";
            if ( language() == "de" && !word.empty() && word[0] >= 'A' && word[0] <= 'Z' )
                word[0] = (char)( word[0] - 'A' + 'a' );
        }
        name += word;
    }
    return name.empty() ? std::string( tr( noun ) ) : name + " " + tr( noun );
}

void Game::drawCallouts( int w, int h, float s )
{
    for ( const Callout &c : m_callouts )
    {
        if ( Vector3DotProduct( Vector3Subtract( c.m_pos, m_cam.position ),
                                Vector3Subtract( m_cam.target, m_cam.position ) ) <= 0.0f )
            continue;
        Vector2       q = GetWorldToScreenEx( c.m_pos, m_cam, w, h );
        unsigned char a = (unsigned char)( 255 * Clamp( c.m_life / 0.5f, 0.0f, 1.0f ) );
        Vector2       m = MeasureTextEx( m_font, c.m_text.c_str(), 17 * s, 1.0f );
        DrawTextEx( m_font, c.m_text.c_str(), { q.x - m.x * 0.5f + 2 * s, q.y + 2 * s }, 17 * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( a * 0.8f ) } );
        DrawTextEx( m_font, c.m_text.c_str(), { q.x - m.x * 0.5f, q.y }, 17 * s, 1.0f,
                    { c.m_color.r, c.m_color.g, c.m_color.b, a } );
    }
}

void Game::drawTargetInfo( int w, int h, float s )
{
    // What the hero aims at: its name, what it shrugs off and what it fears (champions show theirs with the name).
    if ( m_target.m_kind != 0 || m_target.m_index < 0 || m_target.m_index >= (int)m_foes.size() ) return;
    const Foe &f = m_foes[m_target.m_index];
    if ( !f.m_alive || f.m_hidden || f.m_elite || f.m_kind == FoeE::TAXATOR ||
         Vector3Distance( f.m_pos, m_pos ) > 20.0f )
        return;
    std::string weak, tough, immune;
    const char *kinds[4] = { "dmg.kinetic", "dmg.heat", "dmg.shock", "dmg.acid" };
    for ( int k = 0; k < 4; ++k )
    {
        int          r = resist( f, (DamageE)k );
        std::string &list = r >= 100 ? immune : ( r >= 25 ? tough : ( r <= -25 ? weak : immune ) );
        if ( r > -25 && r < 25 ) continue;
        if ( !list.empty() ) list += ", ";
        list += tr( kinds[k] );
    }
    float   top = f.m_kind == FoeE::DRONE ? 0.6f : ( human( f ) ? foeTall( f ) + 0.4f : foeHeight( f ) + 1.0f );
    Vector3 head = Vector3Add( f.m_pos, { 0.0f, top, 0.0f } );
    if ( Vector3DotProduct( Vector3Subtract( head, m_cam.position ),
                            Vector3Subtract( m_cam.target, m_cam.position ) ) <= 0.0f )
        return;
    Vector2     q = GetWorldToScreenEx( head, m_cam, w, h );
    std::string line = tr( foeNoun( f ) );
    Vector2     m = MeasureTextEx( m_font, line.c_str(), 16 * s, 1.0f );
    DrawTextEx( m_font, line.c_str(), { q.x - m.x * 0.5f + 1.5f * s, q.y - 24 * s + 1.5f * s }, 16 * s, 1.0f,
                { 0, 0, 0, 170 } );
    DrawTextEx( m_font, line.c_str(), { q.x - m.x * 0.5f, q.y - 24 * s }, 16 * s, 1.0f, { 235, 225, 210, 220 } );
    std::string info;
    if ( !weak.empty() ) info += TextFormat( tr( "resist.weak" ), weak.c_str() );
    if ( !tough.empty() )
        info += std::string( info.empty() ? "" : "   " ) + TextFormat( tr( "resist.tough" ), tough.c_str() );
    if ( !immune.empty() )
        info += std::string( info.empty() ? "" : "   " ) + TextFormat( tr( "resist.immune" ), immune.c_str() );
    if ( info.empty() ) return;
    m = MeasureTextEx( m_font, info.c_str(), 14 * s, 1.0f );
    DrawTextEx( m_font, info.c_str(), { q.x - m.x * 0.5f + 1.5f * s, q.y - 7 * s + 1.5f * s }, 14 * s, 1.0f,
                { 0, 0, 0, 170 } );
    DrawTextEx( m_font, info.c_str(), { q.x - m.x * 0.5f, q.y - 7 * s }, 14 * s, 1.0f, { 255, 220, 120, 220 } );
}

void Game::drawAilments( float x, float y, float s )
{
    // What ails the hero, beside the stock line.
    std::string line;
    auto        add = [&]( float t, const char *key )
    {
        if ( t <= 0.0f ) return;
        if ( !line.empty() ) line += "   ";
        line += tr( key );
    };
    add( m_chill, "hud.chill" );
    add( m_scorch, "hud.scorch" );
    add( m_corrode, "hud.corrode" );
    if ( line.empty() ) return;
    DrawTextEx( m_font, line.c_str(), { x + 1.5f * s, y + 1.5f * s }, 17 * s, 1.0f, { 0, 0, 0, 170 } );
    DrawTextEx( m_font, line.c_str(), { x, y }, 17 * s, 1.0f, { 190, 230, 255, 240 } );
}

}   // namespace spaceopera
