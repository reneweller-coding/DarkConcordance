/**
 * @file Figure.cpp
 * @brief The figure's parts (suit or clothes, rifle, hammer and shield, baton), walk cycle, blows and arm IK.
 * @author Rene Weller
 */
#include "Figure.h"

#include "MeshBuilder.h"
#include "Rig.h"

#include <raymath.h>

#include <cmath>
#include <functional>

namespace spaceopera
{

namespace
{

constexpr float HipHeight = 0.967f;   ///< pelvis origin above the ground at rest
constexpr float Thigh = 0.42f;        ///< hip to knee
constexpr float Shin = 0.40f;         ///< knee to ankle
constexpr float UpperArm = 0.31f;     ///< shoulder to elbow
constexpr float Forearm = 0.29f;      ///< elbow to wrist
constexpr float Stride = 2.0f;        ///< metres a full walk cycle
constexpr float RunSpeed = 4.2f;      ///< full stride amplitude at this speed

const Vector4 Status = { 0.2f, 2.0f, 0.4f, 0.0f };   ///< the backpack's green
const Vector4 Alert = { 2.5f, 0.3f, 0.1f, 0.0f };    ///< one red light
const Vector4 Lens = { 6.0f, 5.5f, 4.6f, 0.0f };     ///< the lamp's lens
const Vector4 Cell = { 0.3f, 1.6f, 3.0f, 0.0f };     ///< the rifle's energy cell
const Vector4 Bolt = { 3.0f, 1.1f, 0.25f, 0.0f };    ///< the hammer's bolt chamber
const Vector4 Shock = { 0.8f, 1.6f, 4.0f, 0.0f };    ///< the baton's tip

/** @brief Smoothstep from 0 at a to 1 at b. */
float ease( float a, float b, float t )
{
    float x = Clamp( ( t - a ) / ( b - a ), 0.0f, 1.0f );
    return x * x * ( 3.0f - 2.0f * x );
}

/** @brief A frame at p whose z axis points along d and whose y axis lies as near `up` as d allows. */
Matrix along( Vector3 d, Vector3 up, Vector3 p )
{
    Vector3 z = Vector3Normalize( d );
    Vector3 x = Vector3CrossProduct( up, z );
    if ( Vector3Length( x ) < 1e-3f ) x = Vector3CrossProduct( { 1.0f, 0.0f, 0.0f }, z );
    x = Vector3Normalize( x );
    return frame( x, Vector3CrossProduct( z, x ), z, p );
}

}   // namespace

void Figure::build( const FigureSurfaces &s )
{
    // Built again when the gear changes: the old parts go first.
    for ( Part &part : m_parts ) UnloadMesh( part.m_mesh );
    m_parts.clear();
    m_look = s;
    // Each part is its own small mesh in its bone's frame.
    auto add = [&]( BoneE bone, int surface, Vector4 glow, const std::function<void( MeshBuilder & )> &shape,
                    bool visor = false )
    {
        MeshBuilder b;
        shape( b );
        for ( Mesh m : b.build() ) m_parts.push_back( { m, surface, bone, glow, visor } );
    };
    const Vector4 own = { -1.0f, 0.0f, 0.0f, 0.0f };
    auto          rbox = [&]( BoneE bone, int surface, Vector3 at, Vector3 half, float r, Vector3 rot = { 0, 0, 0 },
                              Vector4 glow = { -1.0f, 0.0f, 0.0f, 0.0f } )
    { add( bone, surface, glow, [&]( MeshBuilder &b ) { b.roundedBox( place( at, rot ), half, r, 3 ); } ); };

    if ( !s.m_suit )
    {
        // Clothes: a padded thermal suit and a hood (the face in its shadow), soft boots; nothing in the hands.
        rbox( BoneE::PELVIS, s.m_armor, { 0.0f, 0.0f, 0.0f }, { 0.16f, 0.11f, 0.12f }, 0.07f );
        rbox( BoneE::SPINE, s.m_armor, { 0.0f, 0.3f, 0.0f }, { 0.2f, 0.22f, 0.13f }, 0.1f );
        rbox( BoneE::SPINE, s.m_dark, { 0.0f, 0.5f, 0.0f }, { 0.13f, 0.05f, 0.11f }, 0.04f );
        if ( s.m_skin >= 0 )
        {
            // No hood: face and hair.
            rbox( BoneE::HEAD, s.m_skin, { 0.0f, 0.13f, 0.02f }, { 0.088f, 0.112f, 0.1f }, 0.072f );
            rbox( BoneE::HEAD, s.m_hair, { 0.0f, 0.21f, -0.012f }, { 0.095f, 0.055f, 0.11f }, 0.05f );
            rbox( BoneE::HEAD, s.m_hair, { 0.0f, 0.13f, -0.07f }, { 0.093f, 0.1f, 0.05f }, 0.04f );
        }
        else
        {
            rbox( BoneE::HEAD, s.m_armor, { 0.0f, 0.13f, -0.01f }, { 0.13f, 0.16f, 0.15f }, 0.11f );
            rbox( BoneE::HEAD, s.m_dark, { 0.0f, 0.11f, 0.1f }, { 0.085f, 0.1f, 0.06f }, 0.05f );
        }
        for ( BoneE upper : { BoneE::UPPER_L, BoneE::UPPER_R } )
            rbox( upper, s.m_armor, { 0.0f, -0.15f, 0.0f }, { 0.06f, 0.16f, 0.06f }, 0.05f );
        for ( BoneE fore : { BoneE::FORE_L, BoneE::FORE_R } )
        {
            rbox( fore, s.m_armor, { 0.0f, -0.13f, 0.0f }, { 0.055f, 0.14f, 0.055f }, 0.045f );
            rbox( fore, s.m_dark, { 0.0f, -0.31f, 0.0f }, { 0.04f, 0.05f, 0.035f }, 0.025f );
        }
        for ( BoneE thigh : { BoneE::THIGH_L, BoneE::THIGH_R } )
            rbox( thigh, s.m_armor, { 0.0f, -0.2f, 0.0f }, { 0.08f, 0.2f, 0.085f }, 0.06f );
        for ( BoneE shin : { BoneE::SHIN_L, BoneE::SHIN_R } )
            rbox( shin, s.m_armor, { 0.0f, -0.18f, 0.0f }, { 0.065f, 0.2f, 0.07f }, 0.05f );
        for ( BoneE foot : { BoneE::FOOT_L, BoneE::FOOT_R } )
            rbox( foot, s.m_dark, { 0.0f, -0.05f, 0.04f }, { 0.065f, 0.05f, 0.12f }, 0.03f );
        return;
    }

    // Pelvis.
    rbox( BoneE::PELVIS, s.m_armor, { 0.0f, 0.0f, 0.0f }, { 0.17f, 0.1f, 0.13f }, 0.05f );
    rbox( BoneE::PELVIS, s.m_metal, { 0.0f, 0.09f, 0.0f }, { 0.18f, 0.03f, 0.14f }, 0.02f );
    rbox( BoneE::PELVIS, s.m_dark, { 0.0f, -0.07f, 0.11f }, { 0.08f, 0.07f, 0.03f }, 0.02f );
    for ( float side : { -1.0f, 1.0f } )
        rbox( BoneE::PELVIS, s.m_armor, { side * 0.19f, -0.06f, 0.0f }, { 0.035f, 0.1f, 0.1f }, 0.02f,
              { 0.0f, 0.0f, side * 0.15f } );

    // Chest, backpack, shoulders, lamp.
    rbox( BoneE::SPINE, s.m_metal, { 0.0f, 0.09f, 0.0f }, { 0.14f, 0.1f, 0.11f }, 0.04f );
    rbox( BoneE::SPINE, s.m_armor, { 0.0f, 0.33f, 0.0f }, { 0.24f, 0.17f, 0.16f }, 0.07f );
    rbox( BoneE::SPINE, s.m_armor, { 0.0f, 0.35f, 0.15f }, { 0.18f, 0.12f, 0.04f }, 0.03f );
    rbox( BoneE::SPINE, s.m_dark, { 0.0f, 0.35f, 0.19f }, { 0.03f, 0.1f, 0.015f }, 0.01f );
    rbox( BoneE::SPINE, s.m_armor, { 0.0f, 0.33f, -0.24f }, { 0.19f, 0.22f, 0.09f }, 0.04f );
    for ( float side : { -1.0f, 1.0f } )
    {
        add( BoneE::SPINE, s.m_metal, own,
             [&]( MeshBuilder &b ) { b.cylinder( place( { side * 0.11f, 0.12f, -0.33f } ), 0.045f, 0.42f, 12 ); } );
        rbox( BoneE::SPINE, s.m_armor, { side * 0.33f, 0.47f, 0.0f }, { 0.11f, 0.075f, 0.14f }, 0.04f,
              { 0.0f, 0.0f, -side * 0.3f } );
    }
    // The suit's weight: an exoskeleton's frame and heavy pauldrons, a plated chest, or little but padding.
    if ( s.m_heft >= 1 ) rbox( BoneE::SPINE, s.m_armor, { 0.0f, 0.36f, 0.17f }, { 0.2f, 0.14f, 0.045f }, 0.03f );
    if ( s.m_heft >= 2 )
    {
        for ( float side : { -1.0f, 1.0f } )
        {
            rbox( BoneE::SPINE, s.m_dark, { side * 0.36f, 0.53f, 0.0f }, { 0.13f, 0.05f, 0.16f }, 0.03f,
                  { 0.0f, 0.0f, -side * 0.35f } );
            add( BoneE::SPINE, s.m_metal, own,
                 [&]( MeshBuilder &b ) { b.cylinder( place( { side * 0.2f, 0.1f, -0.2f } ), 0.025f, 0.5f, 8 ); } );
        }
        rbox( BoneE::SPINE, s.m_metal, { 0.0f, 0.64f, -0.18f }, { 0.16f, 0.05f, 0.06f }, 0.02f );
    }
    if ( s.m_trim.x >= 0.0f )
        rbox( BoneE::SPINE, s.m_glow, { 0.0f, 0.44f, 0.215f }, { 0.14f, 0.006f, 0.006f }, 0.003f, { 0, 0, 0 },
              s.m_trim );
    rbox( BoneE::SPINE, s.m_glow, { 0.13f, 0.47f, -0.335f }, { 0.02f, 0.012f, 0.006f }, 0.004f, { 0, 0, 0 }, Status );
    rbox( BoneE::SPINE, s.m_glow, { 0.07f, 0.47f, -0.335f }, { 0.02f, 0.012f, 0.006f }, 0.004f, { 0, 0, 0 }, Status );
    rbox( BoneE::SPINE, s.m_glow, { -0.13f, 0.47f, -0.335f }, { 0.012f, 0.012f, 0.006f }, 0.004f, { 0, 0, 0 }, Alert );
    add( BoneE::SPINE, s.m_metal, own,
         [&]( MeshBuilder &b )
         {
             b.lathe( place( { 0.0f, 0.49f, 0.0f } ),
                      { { 0.0f, 0.0f },
                        { 0.1f, 0.0f },
                        { 0.1f, 0.0f },
                        { 0.13f, 0.02f },
                        { 0.13f, 0.05f },
                        { 0.1f, 0.07f },
                        { 0.0f, 0.07f } },
                      16 );
         } );
    add( BoneE::SPINE, s.m_metal, own, [&]( MeshBuilder &b )
         { b.cylinder( place( { -0.33f, 0.57f, 0.03f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.05f, 0.13f, 14 ); } );
    add( BoneE::SPINE, s.m_glow, Lens, [&]( MeshBuilder &b )
         { b.cylinder( place( { -0.33f, 0.57f, 0.16f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.04f, 0.012f, 14 ); } );

    if ( s.m_helmet )
    {
        // Helmet.
        rbox( BoneE::HEAD, s.m_armor, { 0.0f, 0.14f, 0.0f }, { 0.13f, 0.15f, 0.15f }, 0.09f );
        rbox( BoneE::HEAD, s.m_dark, { 0.0f, 0.11f, 0.12f }, { 0.11f, 0.09f, 0.05f }, 0.04f );
        add(
            BoneE::HEAD, s.m_glow, s.m_visor, [&]( MeshBuilder &b )
            { b.roundedBox( place( { 0.0f, 0.15f, 0.168f } ), { 0.095f, 0.016f, 0.012f }, 0.008f, 3 ); }, true );
        rbox( BoneE::HEAD, s.m_armor, { 0.0f, 0.29f, -0.01f }, { 0.025f, 0.03f, 0.13f }, 0.015f );
        for ( float side : { -1.0f, 1.0f } )
            rbox( BoneE::HEAD, s.m_metal, { side * 0.14f, 0.13f, 0.0f }, { 0.02f, 0.05f, 0.05f }, 0.015f );
        rbox( BoneE::HEAD, s.m_metal, { 0.1f, 0.33f, -0.08f }, { 0.006f, 0.08f, 0.006f }, 0.003f );
    }
    else
    {
        // Bare-headed where there is air: the face, the hair, the suit's neck ring below.
        rbox( BoneE::HEAD, s.m_metal, { 0.0f, 0.0f, 0.0f }, { 0.11f, 0.03f, 0.11f }, 0.025f );
        rbox( BoneE::HEAD, s.m_skin, { 0.0f, 0.13f, 0.02f }, { 0.09f, 0.115f, 0.105f }, 0.075f );
        rbox( BoneE::HEAD, s.m_hair, { 0.0f, 0.21f, -0.012f }, { 0.097f, 0.055f, 0.113f }, 0.05f );
        rbox( BoneE::HEAD, s.m_hair, { 0.0f, 0.14f, -0.07f }, { 0.095f, 0.09f, 0.05f }, 0.04f );
        for ( float side : { -1.0f, 1.0f } )
            rbox( BoneE::HEAD, s.m_dark, { side * 0.034f, 0.14f, 0.122f }, { 0.013f, 0.007f, 0.004f }, 0.003f );
    }

    // The weapon, along its bone's +z from the grip; its family decides its shape, its tier adds metal and light.
    auto barrel = [&]( float z, float r, float len )
    {
        add( BoneE::WEAPON, s.m_metal, own, [&]( MeshBuilder &b )
             { b.cylinder( place( { 0.0f, 0.015f, z }, { PI * 0.5f, 0.0f, 0.0f } ), r, len, 10 ); } );
    };
    switch ( s.m_weapon )
    {
    case WeaponE::RIFLE:
    {
        // The receiver, the grip and the stock all guns share.
        float body = s.m_shape == 1 ? 0.16f : ( s.m_shape == 4 ? 0.2f : 0.22f );
        rbox( BoneE::WEAPON, s.m_dark, { 0.0f, 0.0f, 0.0f }, { 0.04f, 0.065f, body }, 0.015f );
        rbox( BoneE::WEAPON, s.m_dark, { 0.0f, -0.1f, -0.08f }, { 0.022f, 0.06f, 0.03f }, 0.01f, { 0.3f, 0.0f, 0.0f } );
        if ( s.m_shape != 1 ) rbox( BoneE::WEAPON, s.m_dark, { 0.0f, -0.02f, -0.3f }, { 0.03f, 0.055f, 0.1f }, 0.015f );
        switch ( s.m_shape )
        {
        case 1:   // the carbine: short, a long magazine, a folding stock
            barrel( 0.24f, 0.014f, 0.14f );
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, -0.12f, 0.06f }, { 0.022f, 0.08f, 0.03f }, 0.01f,
                  { 0.25f, 0.0f, 0.0f } );
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, 0.0f, -0.25f }, { 0.012f, 0.04f, 0.1f }, 0.006f );
            break;
        case 2:   // the scattergun: a wide barrel, a drum below
            add( BoneE::WEAPON, s.m_metal, own, [&]( MeshBuilder &b )
                 { b.cylinder( place( { 0.0f, 0.02f, 0.38f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.038f, 0.24f, 12 ); } );
            add( BoneE::WEAPON, s.m_dark, own, [&]( MeshBuilder &b )
                 { b.cylinder( place( { 0.0f, -0.09f, 0.08f }, { 0.0f, 0.0f, PI * 0.5f } ), 0.06f, 0.05f, 14 ); } );
            break;
        case 3:   // the rail: very long, coils along it
            barrel( 0.5f, 0.016f, 0.42f );
            for ( int k = 0; k < 4; ++k )
                add( BoneE::WEAPON, s.m_metal, own,
                     [&]( MeshBuilder &b )
                     {
                         b.cylinder( place( { 0.0f, 0.015f, 0.28f + k * 0.11f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.032f,
                                     0.03f, 12 );
                     } );
            rbox( BoneE::WEAPON, s.m_glow, { 0.0f, 0.06f, 0.4f }, { 0.006f, 0.006f, 0.22f }, 0.003f, { 0, 0, 0 },
                  Cell );
            break;
        case 4:   // the plasma thrower: a bulky glowing chamber, a short muzzle
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, 0.03f, 0.2f }, { 0.055f, 0.06f, 0.09f }, 0.03f );
            rbox( BoneE::WEAPON, s.m_glow, { 0.0f, 0.03f, 0.2f }, { 0.058f, 0.025f, 0.05f }, 0.012f, { 0, 0, 0 },
                  Bolt );
            barrel( 0.34f, 0.026f, 0.1f );
            break;
        case 5:   // the arc projector: two prongs and the light between them
            for ( float side : { -1.0f, 1.0f } )
                rbox( BoneE::WEAPON, s.m_metal, { side * 0.03f, 0.015f, 0.34f }, { 0.008f, 0.012f, 0.13f }, 0.005f,
                      { 0.0f, side * 0.12f, 0.0f } );
            rbox( BoneE::WEAPON, s.m_glow, { 0.0f, 0.015f, 0.42f }, { 0.012f, 0.012f, 0.05f }, 0.01f, { 0, 0, 0 },
                  Shock );
            break;
        default:   // the Gauss rifle
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, 0.015f, 0.32f }, { 0.032f, 0.035f, 0.12f }, 0.012f );
            barrel( 0.42f, 0.016f, 0.2f );
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, -0.1f, 0.07f }, { 0.025f, 0.06f, 0.035f }, 0.01f );
            break;
        }
        rbox( BoneE::WEAPON, s.m_glow, { 0.041f, 0.02f, 0.02f }, { 0.004f, 0.012f, 0.13f }, 0.002f, { 0, 0, 0 }, Cell );
        rbox( BoneE::WEAPON, s.m_dark, { 0.0f, 0.085f, 0.0f }, { 0.018f, 0.022f, 0.06f }, 0.008f );
        if ( s.m_tier >= 2 )   // a scope, and on the third tier a second cell
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, 0.11f, 0.02f }, { 0.014f, 0.014f, 0.07f }, 0.007f );
        if ( s.m_tier >= 3 )
            rbox( BoneE::WEAPON, s.m_glow, { -0.041f, 0.02f, 0.02f }, { 0.004f, 0.012f, 0.13f }, 0.002f, { 0, 0, 0 },
                  Cell );
        if ( s.m_trim.x >= 0.0f )
            rbox( BoneE::WEAPON, s.m_glow, { 0.0f, -0.066f, 0.0f }, { 0.03f, 0.003f, 0.16f }, 0.002f, { 0, 0, 0 },
                  s.m_trim );
        break;
    }
    case WeaponE::HAMMER:
    {
        // A long haft; the head by the family.
        add( BoneE::WEAPON, s.m_metal, own,
             [&]( MeshBuilder &b )
             {
                 b.cylinder( place( { 0.0f, 0.0f, -0.18f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.026f,
                             s.m_shape == 5 ? 1.15f : 1.0f, 10 );
             } );
        rbox( BoneE::WEAPON, s.m_dark, { 0.0f, 0.0f, -0.02f }, { 0.034f, 0.034f, 0.12f }, 0.012f );
        switch ( s.m_shape )
        {
        case 1:   // the axe: a wide blade on one side, its edge glowing
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, 0.13f, 0.82f }, { 0.018f, 0.13f, 0.14f }, 0.01f );
            rbox( BoneE::WEAPON, s.m_glow, { 0.0f, 0.255f, 0.82f }, { 0.02f, 0.008f, 0.13f }, 0.004f, { 0, 0, 0 },
                  Bolt );
            rbox( BoneE::WEAPON, s.m_dark, { 0.0f, 0.0f, 0.82f }, { 0.04f, 0.05f, 0.06f }, 0.015f );
            break;
        case 2:   // the pick: a long spike both ways
            rbox( BoneE::WEAPON, s.m_dark, { 0.0f, 0.0f, 0.84f }, { 0.04f, 0.05f, 0.06f }, 0.015f );
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, 0.0f, 0.84f }, { 0.022f, 0.3f, 0.026f }, 0.012f );
            break;
        case 3:   // the shock maul: a round head in coils
            add( BoneE::WEAPON, s.m_dark, own, [&]( MeshBuilder &b )
                 { b.cylinder( place( { 0.0f, 0.0f, 0.86f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.11f, 0.22f, 14 ); } );
            for ( float z : { 0.8f, 0.92f } )
                add( BoneE::WEAPON, s.m_glow, Shock, [&]( MeshBuilder &b )
                     { b.cylinder( place( { 0.0f, 0.0f, z }, { PI * 0.5f, 0.0f, 0.0f } ), 0.118f, 0.02f, 14 ); } );
            break;
        case 4:   // the bolt breaker: a block with pistons
            rbox( BoneE::WEAPON, s.m_dark, { 0.0f, 0.0f, 0.86f }, { 0.13f, 0.16f, 0.13f }, 0.02f );
            for ( float side : { -1.0f, 1.0f } )
                add( BoneE::WEAPON, s.m_metal, own,
                     [&]( MeshBuilder &b )
                     {
                         b.cylinder( place( { side * 0.15f, 0.0f, 0.86f }, { 0.0f, 0.0f, PI * 0.5f } ), 0.04f, 0.06f,
                                     10 );
                     } );
            rbox( BoneE::WEAPON, s.m_glow, { 0.0f, 0.0f, 0.99f }, { 0.08f, 0.08f, 0.01f }, 0.006f, { 0, 0, 0 }, Bolt );
            break;
        case 5:   // the drill lance: a spiral cone at the end
            add( BoneE::WEAPON, s.m_metal, own,
                 [&]( MeshBuilder &b )
                 {
                     b.lathe( place( { 0.0f, 0.0f, 0.86f }, { PI * 0.5f, 0.0f, 0.0f } ),
                              { { 0.0f, 0.0f }, { 0.08f, 0.0f }, { 0.06f, 0.1f }, { 0.04f, 0.2f }, { 0.0f, 0.32f } },
                              10 );
                 } );
            rbox( BoneE::WEAPON, s.m_glow, { 0.0f, 0.0f, 0.84f }, { 0.085f, 0.012f, 0.012f }, 0.006f, { 0, 0, 0 },
                  Bolt );
            break;
        default:   // the ram hammer: a heavy head with the bolt chamber glowing in its back
            rbox( BoneE::WEAPON, s.m_dark, { 0.0f, 0.0f, 0.86f }, { 0.1f, 0.19f, 0.1f }, 0.025f );
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, 0.2f, 0.86f }, { 0.115f, 0.03f, 0.115f }, 0.012f );
            rbox( BoneE::WEAPON, s.m_metal, { 0.0f, -0.2f, 0.86f }, { 0.115f, 0.03f, 0.115f }, 0.012f );
            rbox( BoneE::WEAPON, s.m_glow, { 0.0f, 0.0f, 0.86f }, { 0.104f, 0.05f, 0.06f }, 0.01f, { 0, 0, 0 }, Bolt );
            break;
        }
        if ( s.m_tier >= 2 )   // bands on the haft
            for ( float z : { 0.2f, 0.45f } )
                add( BoneE::WEAPON, s.m_metal, own, [&]( MeshBuilder &b )
                     { b.cylinder( place( { 0.0f, 0.0f, z }, { PI * 0.5f, 0.0f, 0.0f } ), 0.032f, 0.03f, 10 ); } );
        if ( s.m_trim.x >= 0.0f )
            add( BoneE::WEAPON, s.m_glow, s.m_trim, [&]( MeshBuilder &b )
                 { b.cylinder( place( { 0.0f, 0.0f, 0.62f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.03f, 0.05f, 10 ); } );
        break;
    }
    case WeaponE::BATON:
        add( BoneE::WEAPON, s.m_dark, own, [&]( MeshBuilder &b )
             { b.cylinder( place( { 0.0f, 0.0f, -0.1f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.022f, 0.62f, 10 ); } );
        rbox( BoneE::WEAPON, s.m_metal, { 0.0f, 0.0f, 0.0f }, { 0.03f, 0.03f, 0.08f }, 0.01f );
        rbox( BoneE::WEAPON, s.m_glow, { 0.0f, 0.0f, 0.52f }, { 0.03f, 0.03f, 0.07f }, 0.012f, { 0, 0, 0 }, Shock );
        break;
    default: break;
    }

    // Arms (Dorra's left forearm carries the shield: plates of her old mining suit).
    for ( BoneE upper : { BoneE::UPPER_L, BoneE::UPPER_R } )
    {
        rbox( upper, s.m_armor, { 0.0f, -0.15f, 0.0f }, { 0.065f, 0.14f, 0.065f }, 0.03f );
        rbox( upper, s.m_metal, { 0.0f, -0.3f, 0.0f }, { 0.055f, 0.04f, 0.055f }, 0.03f );
    }
    for ( BoneE fore : { BoneE::FORE_L, BoneE::FORE_R } )
    {
        rbox( fore, s.m_armor, { 0.0f, -0.13f, 0.0f }, { 0.07f, 0.13f, 0.07f }, 0.03f );
        rbox( fore, s.m_dark, { 0.0f, -0.2f, 0.0f }, { 0.08f, 0.05f, 0.08f }, 0.025f );
        rbox( fore, s.m_metal, { 0.0f, -0.33f, 0.0f }, { 0.045f, 0.055f, 0.05f }, 0.02f );
    }
    if ( s.m_weapon == WeaponE::HAMMER )
    {
        rbox( BoneE::FORE_L, s.m_armor, { 0.0f, -0.17f, 0.11f }, { 0.25f, 0.3f, 0.025f }, 0.03f );
        rbox( BoneE::FORE_L, s.m_dark, { 0.0f, -0.17f, 0.14f }, { 0.2f, 0.025f, 0.012f }, 0.008f );
        rbox( BoneE::FORE_L, s.m_metal, { 0.0f, -0.17f, 0.088f }, { 0.26f, 0.31f, 0.012f }, 0.01f );
    }

    // Legs.
    for ( BoneE thigh : { BoneE::THIGH_L, BoneE::THIGH_R } )
    {
        rbox( thigh, s.m_armor, { 0.0f, -0.2f, 0.01f }, { 0.09f, 0.19f, 0.1f }, 0.04f );
        rbox( thigh, s.m_dark, { 0.0f, -0.18f, 0.1f }, { 0.06f, 0.12f, 0.025f }, 0.015f );
    }
    for ( BoneE shin : { BoneE::SHIN_L, BoneE::SHIN_R } )
    {
        rbox( shin, s.m_metal, { 0.0f, 0.0f, 0.0f }, { 0.07f, 0.06f, 0.07f }, 0.04f );
        rbox( shin, s.m_armor, { 0.0f, 0.0f, 0.08f }, { 0.07f, 0.075f, 0.035f }, 0.025f );
        rbox( shin, s.m_armor, { 0.0f, -0.2f, 0.0f }, { 0.08f, 0.18f, 0.085f }, 0.035f );
    }
    for ( BoneE foot : { BoneE::FOOT_L, BoneE::FOOT_R } )
    {
        rbox( foot, s.m_armor, { 0.0f, -0.045f, 0.05f }, { 0.08f, 0.055f, 0.15f }, 0.03f );
        rbox( foot, s.m_metal, { 0.0f, -0.095f, 0.05f }, { 0.085f, 0.012f, 0.155f }, 0.008f );
    }
}

Vector3 Figure::forward() const
{
    return { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
}

void Figure::update( float dt, Vector2 wanted, float time )
{
    m_time = time;
    // Accelerate towards the wanted velocity, turn towards where we go.
    Vector2 dv = Vector2Subtract( wanted, m_vel );
    float   step = 18.0f * dt;
    if ( Vector2Length( dv ) > step ) dv = Vector2Scale( Vector2Normalize( dv ), step );
    m_vel = Vector2Add( m_vel, dv );
    float speed = Vector2Length( m_vel );
    if ( Vector2Length( wanted ) > 0.1f )
    {
        float target = atan2f( wanted.x, wanted.y );
        float d = angleTo( m_yaw, target );
        float turn = 11.0f * dt;
        m_yaw += Clamp( d, -turn, turn );
    }
    m_phase = fmodf( m_phase + speed * dt * 2.0f * PI / Stride, 2.0f * PI );
    m_recoil = fmaxf( 0.0f, m_recoil - dt * 7.0f );
}

void Figure::animate( float dt, Vector3 pos, Vector2 vel, float yaw, bool air, float down, float time )
{
    m_pos = pos;
    m_vel = vel;
    m_yaw = yaw;
    m_air = air;
    m_down = down;
    m_time = time;
    FigurePose p = own();
    step( p, dt );
    m_phase = p.m_phase;
    m_recoil = fmaxf( 0.0f, m_recoil - dt * 7.0f );
}

void Figure::step( FigurePose &p, float dt )
{
    // Walking backwards runs the cycle backwards.
    float along = p.m_vel.x * sinf( p.m_yaw ) + p.m_vel.y * cosf( p.m_yaw );
    float speed = Vector2Length( p.m_vel ) * ( along < -0.3f ? -1.0f : 1.0f );
    if ( !p.m_air ) p.m_phase = fmodf( p.m_phase + speed * dt * 2.0f * PI / Stride + 2.0f * PI, 2.0f * PI );
}

FigurePose Figure::own() const
{
    FigurePose p;
    p.m_pos = m_pos;
    p.m_yaw = m_yaw;
    p.m_vel = m_vel;
    p.m_phase = m_phase;
    p.m_time = m_time;
    p.m_air = m_air;
    p.m_down = m_down;
    p.m_recoil = m_recoil;
    p.m_swing = m_swing;
    p.m_swingT = m_swingT;
    p.m_block = m_block;
    return p;
}

Matrix Figure::weaponFrame( const FigurePose &p, float amp, float bob, float ph ) const
{
    // Root space: x to the figure's left, y up, z ahead.
    Vector3 shoulder = { -0.27f, 1.45f + bob, 0.02f };
    float   sway = 0.03f * amp * sinf( 2.0f * ph );
    Vector3 carryAt = { -0.3f, 0.98f + bob + sway, 0.22f };
    Vector3 carryDir = { -0.1f, 0.95f, 0.3f };
    Vector3 carryUp = { 0.0f, 0.0f, 1.0f };
    if ( m_look.m_weapon == WeaponE::BATON ) carryDir = { -0.15f, -0.4f, 0.9f };
    if ( p.m_swing < 0 ) return along( carryDir, carryUp, carryAt );

    // A blow: windup to 0.35, the strike through 0.5 (the hit), follow-through, back to the carry from 0.7.
    float   t = p.m_swingT;
    Vector3 at, dir, up;
    if ( p.m_swing == 2 )
    {
        // Overhead: from behind the head down to the floor in front.
        float a = t < 0.35f  ? Lerp( 0.3f, -0.7f, ease( 0.0f, 0.35f, t ) )
                  : t < 0.5f ? Lerp( -0.7f, 2.3f, ease( 0.35f, 0.5f, t ) )
                  : t < 0.7f ? 2.3f
                             : Lerp( 2.3f, 0.4f, ease( 0.7f, 1.0f, t ) );
        dir = Vector3Normalize( { 0.12f, cosf( a ), sinf( a ) } );
        at = Vector3Add( Vector3Add( shoulder, { 0.15f, 0.05f, 0.0f } ), Vector3Scale( dir, 0.5f ) );
        up = { 0.0f, -sinf( a ), cosf( a ) };
    }
    else
    {
        // Forehand from the right to the left, backhand the other way.
        bool  fore = p.m_swing == 0;
        float a0 = fore ? -1.2f : 0.6f, a1 = fore ? -2.0f : 1.4f, a2 = fore ? 1.1f : -1.0f, a3 = fore ? 0.6f : -0.8f;
        float a = t < 0.35f  ? Lerp( a0, a1, ease( 0.0f, 0.35f, t ) )
                  : t < 0.6f ? Lerp( a1, a2, ease( 0.35f, 0.6f, t ) )
                             : Lerp( a2, a3, ease( 0.6f, 1.0f, t ) );
        dir = { sinf( a ), -0.05f, cosf( a ) };
        at = Vector3Add( shoulder, { sinf( a ) * 0.5f, fore ? -0.2f : -0.3f, cosf( a ) * 0.5f } );
        up = { cosf( a ), 0.0f, -sinf( a ) };
    }
    float w = ease( 0.0f, 0.2f, t ) * ( 1.0f - ease( 0.75f, 1.0f, t ) );
    at = Vector3Lerp( carryAt, at, w );
    dir = Vector3Lerp( Vector3Normalize( carryDir ), dir, w );
    up = Vector3Lerp( carryUp, up, w );
    return along( dir, up, at );
}

void Figure::solve( const FigurePose &p )
{
    float speed = p.m_air ? 0.0f : Vector2Length( p.m_vel );
    float amp = Clamp( speed / RunSpeed, 0.0f, 1.0f ) * ( 1.0f - p.m_sit ) * ( 1.0f - p.m_chair );
    float ph = p.m_phase;
    float breath = 0.006f * sinf( p.m_time * 1.8f ) * ( 1.0f - amp );
    float bob = 0.03f * amp * cosf( 2.0f * ph ) + breath;
    float twist = 0.1f * amp * sinf( ph );
    float lean = 0.15f * amp + 0.03f * p.m_recoil + ( p.m_air ? 0.1f : 0.0f );
    bool  rifle = m_look.m_weapon == WeaponE::RIFLE && m_look.m_suit;
    bool  hand = m_look.m_weapon == WeaponE::HAMMER || m_look.m_weapon == WeaponE::BATON;
    // A slam crouches and leans into the floor; a raised shield hunches.
    float slam = p.m_swing == 2 ? ease( 0.3f, 0.5f, p.m_swingT ) * ( 1.0f - ease( 0.7f, 1.0f, p.m_swingT ) ) : 0.0f;
    bob -= 0.2f * slam + 0.05f * p.m_block;
    lean += 0.45f * slam + 0.12f * p.m_block;

    // Falling over (dead): the whole figure tips backwards about its feet. Then the figure's own proportions.
    Vector3 sc = m_look.m_scale;
    Matrix  root = MatrixMultiply(
        MatrixMultiply( MatrixMultiply( MatrixScale( sc.x, sc.y, sc.z ), MatrixRotateX( -1.35f * p.m_down ) ),
                        MatrixRotateY( p.m_yaw ) ),
        MatrixTranslate( p.m_pos.x, p.m_pos.y, p.m_pos.z ) );
    float  hip = Lerp( Lerp( HipHeight + bob, 0.13f, p.m_sit ), ( p.m_seat + 0.08f ) / sc.y, p.m_chair );
    Matrix pelvis =
        MatrixMultiply( MatrixMultiply( MatrixRotateY( twist ), MatrixTranslate( 0.0f, hip, 0.0f ) ), root );
    // The chest turns to the right for the rifle stance and against the hips' swing; a blow turns it with the swing.
    float swingTurn = 0.0f;
    if ( p.m_swing == 0 || p.m_swing == 1 )
    {
        float t = p.m_swingT, s = p.m_swing == 0 ? 1.0f : -1.0f;
        swingTurn = s * ( t < 0.35f ? -0.5f * ease( 0.0f, 0.35f, t )
                                    : -0.5f + 1.0f * ease( 0.35f, 0.6f, t ) - 0.5f * ease( 0.6f, 1.0f, t ) );
    }
    float  chestYaw = ( rifle ? -0.35f : -0.1f ) - twist * 1.6f + swingTurn;
    Matrix spine = MatrixMultiply(
        MatrixMultiply( MatrixRotateY( chestYaw ), MatrixRotateX( lean + 0.35f * p.m_sit + 0.15f * p.m_chair ) ),
        MatrixMultiply( MatrixTranslate( 0.0f, 0.1f, 0.0f ), pelvis ) );
    Matrix head = MatrixMultiply( MatrixMultiply( MatrixRotateY( ( rifle ? 0.3f : 0.1f ) + twist * 1.6f -
                                                                 swingTurn * 0.7f + 0.05f * sinf( p.m_time * 0.7f ) ),
                                                  MatrixRotateX( -lean * 0.7f + 0.25f * p.m_sit - 0.12f * p.m_chair ) ),
                                  MatrixMultiply( MatrixTranslate( 0.0f, 0.56f, 0.0f ), spine ) );
    Matrix weapon;
    if ( rifle )
    {
        // The rifle stays aimed forward: it follows the root, the bob and a little sway, not the chest.
        float  sway = 0.02f * amp * sinf( 2.0f * ph ) + 0.008f * sinf( p.m_time * 1.3f );
        Matrix aim = MatrixMultiply( MatrixTranslate( 0.0f, HipHeight + bob + 0.1f, 0.0f ), root );
        weapon = MatrixMultiply(
            MatrixMultiply( MatrixRotateX( sway - 0.12f * p.m_recoil ), MatrixRotateY( 0.015f * amp * sinf( ph ) ) ),
            MatrixMultiply( MatrixTranslate( -0.06f, 0.24f, 0.24f - 0.05f * p.m_recoil ), aim ) );
    }
    else
        weapon = MatrixMultiply( weaponFrame( p, amp, bob, ph ), root );
    m_bones[(int)BoneE::PELVIS] = pelvis;
    m_bones[(int)BoneE::SPINE] = spine;
    m_bones[(int)BoneE::HEAD] = head;
    m_bones[(int)BoneE::WEAPON] = weapon;

    // Arms: the hands hold the rifle, the hammer or the baton; the shield comes up in front; empty hands swing,
    // or hold the knees.
    Vector3 fwd = { sinf( p.m_yaw ), 0.0f, cosf( p.m_yaw ) };
    Vector3 left = { fwd.z, 0.0f, -fwd.x };   // +x of the root
    Vector3 down = { 0.0f, -1.0f, 0.0f };
    Vector3 shoulderL = Vector3Transform( { 0.29f, 0.44f, 0.0f }, spine );
    Vector3 shoulderR = Vector3Transform( { -0.29f, 0.44f, 0.0f }, spine );
    auto    local = [&]( Vector3 v ) { return Vector3Transform( v, root ); };
    float   armSwing = 0.12f * amp * sinf( ph );
    Vector3 wristR, wristL;
    if ( rifle )
    {
        wristR = Vector3Transform( { 0.0f, -0.06f, -0.1f }, weapon );
        wristL = Vector3Transform( { 0.0f, -0.05f, 0.2f }, weapon );
    }
    else if ( hand )
    {
        wristR = Vector3Transform( { 0.0f, 0.0f, -0.06f }, weapon );
        Vector3 rest = { 0.3f, 0.98f + bob, 0.16f - armSwing };
        Vector3 guard = { 0.06f, 1.36f + bob, 0.44f };
        wristL = local( Vector3Lerp( rest, guard, p.m_block ) );
    }
    else
    {
        wristR = local( { -0.3f, 0.92f + bob, 0.05f + armSwing } );
        wristL = local( { 0.3f, 0.92f + bob, 0.05f - armSwing } );
    }
    if ( p.m_sit > 0.01f )
    {
        wristR = Vector3Lerp( wristR, local( { -0.07f, 0.44f, 0.34f } ), p.m_sit );
        wristL = Vector3Lerp( wristL, local( { 0.07f, 0.44f, 0.34f } ), p.m_sit );
    }
    if ( p.m_chair > 0.01f )
    {
        // Forearms on the table (or the counter), one hand a little forward.
        float y = ( p.m_seat + 0.33f ) / sc.y, nod = 0.03f * sinf( p.m_time * 0.9f );
        wristR = Vector3Lerp( wristR, local( { -0.15f, y, 0.4f + nod } ), p.m_chair );
        wristL = Vector3Lerp( wristL, local( { 0.17f, y, 0.33f } ), p.m_chair );
    }
    float   ua = UpperArm * sc.y, fa = Forearm * sc.y;
    Vector3 poleR = Vector3Add( Vector3Add( Vector3Scale( left, -1.0f ), down ), Vector3Scale( fwd, -0.3f ) );
    Vector3 poleL = Vector3Add( Vector3Add( Vector3Scale( left, 0.6f ), down ), Vector3Scale( fwd, -0.2f ) );
    if ( p.m_block > 0.01f ) poleL = Vector3Lerp( poleL, Vector3Add( Vector3Scale( left, 1.0f ), down ), p.m_block );
    twoBone( shoulderR, wristR, ua, fa, poleR, fwd, m_bones[(int)BoneE::UPPER_R], m_bones[(int)BoneE::FORE_R] );
    twoBone( shoulderL, wristL, ua, fa, poleL, fwd, m_bones[(int)BoneE::UPPER_L], m_bones[(int)BoneE::FORE_L] );
    Matrix armScale = MatrixScale( sc.x, sc.y, sc.z );
    for ( BoneE b : { BoneE::UPPER_R, BoneE::FORE_R, BoneE::UPPER_L, BoneE::FORE_L } )
        m_bones[(int)b] = MatrixMultiply( armScale, m_bones[(int)b] );

    // Legs: hips swing, knees bend in the swing, boots stay flat; sitting, the knees are drawn up.
    float a = 0.5f * amp;
    for ( int side = 0; side < 2; ++side )
    {
        float ph2 = ph + ( side ? PI : 0.0f );
        float s = a * sinf( ph2 );
        float k = 0.06f + amp * 1.05f * powf( fmaxf( 0.0f, cosf( ph2 ) ), 1.5f );
        if ( p.m_air )
        {
            // In the air: one knee drawn up, the other leg trailing.
            s = side ? -0.15f : 0.45f;
            k = side ? 0.5f : 0.95f;
        }
        if ( slam > 0.0f )
        {
            s = Lerp( s, side ? -0.3f : 0.5f, slam );
            k = Lerp( k, side ? 0.4f : 0.9f, slam );
        }
        s = Lerp( Lerp( s, 2.6f, p.m_sit ), 1.5f, p.m_chair );
        k = Lerp( Lerp( k, 2.1f, p.m_sit ), 1.45f + ( side ? 0.15f : 0.0f ), p.m_chair );
        float  x = side ? -0.11f : 0.11f;
        Matrix thigh =
            MatrixMultiply( MatrixMultiply( MatrixRotateX( -s ), MatrixTranslate( x, -0.04f, 0.0f ) ), pelvis );
        Matrix shin =
            MatrixMultiply( MatrixMultiply( MatrixRotateX( k ), MatrixTranslate( 0.0f, -Thigh, 0.0f ) ), thigh );
        Matrix foot =
            MatrixMultiply( MatrixMultiply( MatrixRotateX( s - k ), MatrixTranslate( 0.0f, -Shin, 0.0f ) ), shin );
        m_bones[(int)( side ? BoneE::THIGH_R : BoneE::THIGH_L )] = thigh;
        m_bones[(int)( side ? BoneE::SHIN_R : BoneE::SHIN_L )] = shin;
        m_bones[(int)( side ? BoneE::FOOT_R : BoneE::FOOT_L )] = foot;
    }

    m_lampPos = Vector3Transform( { -0.33f, 0.57f, 0.2f }, spine );
    m_lampDir = Vector3Normalize(
        Vector3Add( Vector3Scale( fwd, cosf( 24.0f * DEG2RAD ) ), { 0.0f, -sinf( 24.0f * DEG2RAD ), 0.0f } ) );
    m_muzzle = Vector3Transform( rifle ? Vector3{ 0.0f, 0.015f, 0.64f } : Vector3{ 0.0f, 0.0f, 0.86f }, weapon );
}

void Figure::pose( std::vector<Drawable> &out )
{
    pose( own(), { -1.0f, 0.0f, 0.0f, 0.0f }, out );
}

void Figure::pose( const FigurePose &p, Vector4 glow, std::vector<Drawable> &out )
{
    solve( p );
    for ( const Part &part : m_parts )
    {
        Drawable d;
        d.m_mesh = part.m_mesh;
        d.m_surface = part.m_surface;
        d.m_transform = m_bones[(int)part.m_bone];
        d.m_emissive = part.m_visor && glow.x >= 0.0f ? glow : part.m_emissive;
        d.m_spotShadow = false;   // the lamp sits on the figure: no shadow of itself
        out.push_back( d );
    }
}

}   // namespace spaceopera
