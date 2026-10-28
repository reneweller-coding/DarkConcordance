/**
 * @file GameShared.h
 * @brief What the parts of the game share (Game.cpp, GameCrew.cpp, GameShip.cpp): tuning, the surfaces' slots, ray
 *        tests, push-outs and the difficulty's scales.
 * @author Rene Weller
 */
#pragma once

#include "Game.h"
#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace spaceopera
{

/** @brief The game's internals. */
namespace play
{

// Corvin and his rifle.
constexpr float RunSpeed = 4.2f;
constexpr float GroundAccel = 22.0f;
constexpr float AirAccel = 7.0f;
constexpr float JumpSpeed = 4.3f;
constexpr float HeroRadius = 0.38f;
constexpr float StepUp = 0.55f;
constexpr float FireInterval = 0.17f;
constexpr float ShotDamage = 20.0f;
constexpr float HeatPerShot = 7.5f;
constexpr float Range = 30.0f;
constexpr float AutoCone = 0.7f;      ///< auto-aim looks this far either side (radians)
constexpr float AssistCone = 0.22f;   ///< aim assist around a manual aim
constexpr float StimHeal = 40.0f;
// The enemies.
constexpr float LouseHp = 36.0f;
constexpr float WelderHp = 110.0f;
constexpr float TankHp = 450.0f;
constexpr float CoreHp = 700.0f;
constexpr float BatonHp = 75.0f;
constexpr float GunnerHp = 60.0f;
constexpr float ShieldHp = 150.0f;
constexpr float HeavyHp = 320.0f;
constexpr float DroneHp = 42.0f;
constexpr float SawHp = 26.0f;
constexpr float BomberHp = 30.0f;
constexpr float TaxatorHp = 4500.0f;
constexpr float ShellHp = 85.0f;
// Dorra.
constexpr float DorraSpeed = 3.9f;
constexpr float DorraJump = 4.0f;
constexpr float DorraHp = 140.0f;
constexpr float SwingDamage = 30.0f;   ///< forehand and backhand
constexpr float SlamDamage = 58.0f;    ///< the overhead blow and its blast bolt
constexpr float BoltDamage = 55.0f;    ///< a thrown blast bolt (more on the press's parts)
constexpr float BoltCooldown = 1.6f;
// The ship.
constexpr float ThrustPeak = 6.5f;   ///< sideways push of a dodge (m/s²)

/** @brief The slots of the surfaces (the signs' follow after them). */
enum SurfE
{
    S_ROCK,
    S_FLOOR,
    S_GRATE,
    S_METAL,
    S_HAZARD,
    S_RUST,
    S_FOIL,
    S_GLOW,
    S_ARMOR,
    S_DARK,
    S_CRATE,
    S_BOX,       ///< corrugated container steel
    S_HULL,      ///< the ship's bulkheads
    S_DORRA,     ///< Dorra's old mining suit
    S_BOARDER,   ///< the boarders' red-black plates
    S_CLOTH,     ///< refugees' clothes (three colours) and Ilka's
    S_CLOTH2,
    S_CLOTH3,
    S_ILKA,
    S_YSOLDE,       ///< Ysolde's dark violet coat
    S_KIRI,         ///< Kiri's orange coverall
    S_CAVERN,       ///< the asteroid's cavern, seen from far
    S_CLOTH4,       ///< more clothes for a crowd: a faded red
    S_CLOTH5,       ///< and a worker's blue
    S_SKIN,         ///< faces (the hero bare-headed)
    S_HAIR,         ///< Corvin's dark hair
    S_HAIR2,        ///< Dorra's red hair
    S_HAIR3,        ///< grey hair (Ysolde)
    S_GUARD,        ///< the market guards' blued plates
    S_TAXATOR,      ///< the Taxator's brass
    S_CONCESSION,   ///< the Concession's white and orange plates
    S_CLEARANCE,    ///< the clearance squads' olive
    S_SECURITY,     ///< the works security's black
    S_ICE,          ///< walls of ice, pale blue
    S_SNOW,         ///< nitrogen snow, white
    S_COLLEGIUM,    ///< the Silent Collegium's pale grey
    S_CITIZEN,      ///< citizens who die for fun: lacquered teal
    S_LAMM,         ///< Teodor Lamm's crimson and gold
    S_FOREMAN,      ///< the foreman's crane walker: rust orange
    S_VARN,         ///< Ysolde's frame: violet
    S_MENDER,       ///< the computing moon's maintenance robots: white
    S_IONE,         ///< Ione's maintenance suit: grey-blue
    S_COUNT
};

/** @brief A direction turned about the vertical by `a` radians (towards a larger yaw, yaw = atan2( x, z )). */
inline Vector3 turnYaw( Vector3 v, float a )
{
    float c = cosf( a ), s = sinf( a );
    return { v.x * c + v.z * s, v.y, v.z * c - v.x * s };
}

constexpr float SlugSpeed = 90.0f;   ///< how fast a slug is taken to fly where Coriolis bends it (m/s)

/** @brief Loads a texture with mipmaps and anisotropic filtering (a grey checker if it is missing). */
inline Texture2D loadTexture( const std::string &path )
{
    Texture2D t = FileExists( path.c_str() ) ? LoadTexture( path.c_str() ) : Texture2D{};
    if ( t.id == 0 )
    {
        Image i = GenImageChecked( 64, 64, 8, 8, GRAY, DARKGRAY );
        t = LoadTextureFromImage( i );
        UnloadImage( i );
    }
    GenTextureMipmaps( &t );
    SetTextureFilter( t, TEXTURE_FILTER_TRILINEAR );
    SetTextureFilter( t, TEXTURE_FILTER_ANISOTROPIC_16X );
    return t;
}

/** @brief A sign's picture in the loaded language (the German one if there is none). */
inline Texture2D signTexture( const std::string &assets, const std::string &name )
{
    std::string path = assets + "/textures/" + name + "." + language() + ".png";
    if ( !FileExists( path.c_str() ) ) path = assets + "/textures/" + name + ".de.png";
    return loadTexture( path );
}

/** @brief Ray against a sphere: the distance along the ray, or -1. */
inline float raySphere( Vector3 o, Vector3 d, Vector3 c, float r )
{
    Vector3 m = Vector3Subtract( o, c );
    float   b = Vector3DotProduct( m, d );
    float   cc = Vector3DotProduct( m, m ) - r * r;
    if ( cc > 0.0f && b > 0.0f ) return -1.0f;
    float disc = b * b - cc;
    if ( disc < 0.0f ) return -1.0f;
    float t = -b - sqrtf( disc );
    return t < 0.0f ? 0.0f : t;
}

/** @brief Ray against an upright cylinder: the distance along the ray, or -1. */
inline float rayCylinder( Vector3 o, Vector3 d, Vector3 base, float r, float h )
{
    float dx = d.x, dz = d.z;
    float ox = o.x - base.x, oz = o.z - base.z;
    float a = dx * dx + dz * dz;
    if ( a < 1e-8f ) return -1.0f;
    float b = ox * dx + oz * dz;
    float c = ox * ox + oz * oz - r * r;
    float disc = b * b - a * c;
    if ( disc < 0.0f ) return -1.0f;
    float t = ( -b - sqrtf( disc ) ) / a;
    if ( t < 0.0f ) return -1.0f;
    float y = o.y + d.y * t;
    return ( y >= base.y && y <= base.y + h ) ? t : -1.0f;
}

/** @brief Ray against a box: the distance along the ray, or -1. */
inline float rayBox( Vector3 o, Vector3 d, Vector3 lo, Vector3 hi )
{
    float       tmin = 0.0f, tmax = 1e9f;
    const float oo[3] = { o.x, o.y, o.z }, dd[3] = { d.x, d.y, d.z }, l[3] = { lo.x, lo.y, lo.z },
                h[3] = { hi.x, hi.y, hi.z };
    for ( int i = 0; i < 3; ++i )
    {
        if ( fabsf( dd[i] ) < 1e-8f )
        {
            if ( oo[i] < l[i] || oo[i] > h[i] ) return -1.0f;
            continue;
        }
        float t1 = ( l[i] - oo[i] ) / dd[i];
        float t2 = ( h[i] - oo[i] ) / dd[i];
        if ( t1 > t2 ) std::swap( t1, t2 );
        tmin = std::max( tmin, t1 );
        tmax = std::min( tmax, t2 );
        if ( tmin > tmax ) return -1.0f;
    }
    return tmin;
}

/** @brief A cheap hash for flicker. */
inline float hash1( int n )
{
    n = ( n << 13 ) ^ n;
    return ( ( n * ( n * n * 15731 + 789221 ) + 1376312589 ) & 0x7fffffff ) / 2147483647.0f;
}

/** @brief Pushes a circle out of an axis-aligned rectangle on the ground. */
inline void pushOutRect( Vector3 &p, float r, float x0, float x1, float z0, float z1 )
{
    float cx = Clamp( p.x, x0, x1 ), cz = Clamp( p.z, z0, z1 );
    float dx = p.x - cx, dz = p.z - cz;
    float d2 = dx * dx + dz * dz;
    if ( d2 >= r * r ) return;
    if ( d2 > 1e-8f )
    {
        float d = sqrtf( d2 );
        p.x = cx + dx / d * r;
        p.z = cz + dz / d * r;
        return;
    }
    float m = std::min( std::min( p.x - x0, x1 - p.x ), std::min( p.z - z0, z1 - p.z ) );
    if ( m == p.x - x0 )
        p.x = x0 - r;
    else if ( m == x1 - p.x )
        p.x = x1 + r;
    else if ( m == p.z - z0 )
        p.z = z0 - r;
    else
        p.z = z1 + r;
}

/** @brief Pushes a circle out of a circle. */
inline void pushOutCircle( Vector3 &p, float r, float cx, float cz, float cr )
{
    float dx = p.x - cx, dz = p.z - cz;
    float d = sqrtf( dx * dx + dz * dz );
    float min = r + cr;
    if ( d < min && d > 1e-4f )
    {
        p.x = cx + dx / d * min;
        p.z = cz + dz / d * min;
    }
}

/** @brief How much an enemy can take, by difficulty. */
inline float toughness( DifficultyE d )
{
    return d == DifficultyE::EASY ? 0.75f : d == DifficultyE::HARD ? 1.3f : d == DifficultyE::NIGHTMARE ? 1.9f : 1.0f;
}

/** @brief How hard Corvin is hit, by difficulty. */
inline float harm( DifficultyE d )
{
    return d == DifficultyE::EASY ? 0.55f : d == DifficultyE::HARD ? 1.35f : d == DifficultyE::NIGHTMARE ? 1.7f : 1.0f;
}

}   // namespace play

}   // namespace spaceopera
