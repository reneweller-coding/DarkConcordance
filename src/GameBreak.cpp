/**
 * @file GameBreak.cpp
 * @brief Things that break when hit (the level places them by the walls of its wider rooms): supply crates, fuel
 *        drums, gas canisters, equipment cases, coolant tanks, cable spools, crystals. Slugs, swarms, hammer blows and
 *        blasts break them; their pieces fly and fall and lie a while; now and then scrap, a stim or a find is in them.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"
#include "Journal.h"
#include "MeshBuilder.h"

#include <raymath.h>

#include <algorithm>

namespace spaceopera
{

using namespace play;

namespace
{

/** @brief What each kind is: how wide, how tall, how much it takes, its surface, its colour, its glow. */
struct BreakLook
{
    float   m_radius;
    float   m_height;
    float   m_hp;
    int     m_surface;
    Color   m_color;
    Vector4 m_glow;   ///< x < 0: none
};

const BreakLook Looks[(int)BreakE::COUNT] = {
    { 0.42f, 0.75f, 25.0f, S_CRATE, { 150, 146, 110, 255 }, { -1.0f, 0.0f, 0.0f, 0.0f } },
    { 0.32f, 0.95f, 40.0f, S_RUST, { 110, 130, 150, 255 }, { -1.0f, 0.0f, 0.0f, 0.0f } },
    { 0.3f, 1.15f, 20.0f, S_METAL, { 200, 190, 80, 255 }, { -1.0f, 0.0f, 0.0f, 0.0f } },
    { 0.4f, 0.62f, 20.0f, S_METAL, { 70, 76, 84, 255 }, { -1.0f, 0.0f, 0.0f, 0.0f } },
    { 0.3f, 1.25f, 15.0f, S_METAL, { 190, 196, 200, 255 }, { 0.3f, 2.6f, 2.4f, 0.0f } },
    { 0.42f, 0.8f, 35.0f, S_CRATE, { 120, 96, 70, 255 }, { -1.0f, 0.0f, 0.0f, 0.0f } },
    { 0.36f, 1.1f, 30.0f, S_DARK, { 60, 56, 70, 255 }, { 2.0f, 0.7f, 3.4f, 0.0f } },
    { 0.38f, 1.8f, 45.0f, S_METAL, { 110, 120, 104, 255 }, { -1.0f, 0.0f, 0.0f, 0.0f } },
    { 0.3f, 1.25f, 15.0f, S_METAL, { 64, 68, 74, 255 }, { 0.4f, 2.4f, 3.2f, 0.0f } },
    { 0.36f, 0.62f, 20.0f, S_METAL, { 90, 94, 100, 255 }, { 3.2f, 2.2f, 0.4f, 0.0f } },
    { 0.42f, 0.6f, 12.0f, S_CLOTH, { 170, 150, 110, 255 }, { -1.0f, 0.0f, 0.0f, 0.0f } },
    { 0.3f, 0.95f, 10.0f, S_CRATE, { 120, 140, 100, 255 }, { -1.0f, 0.0f, 0.0f, 0.0f } },
    { 0.3f, 0.9f, 10.0f, S_METAL, { 200, 205, 210, 255 }, { 0.6f, 3.2f, 1.0f, 0.0f } },
};

constexpr size_t MaxChunks = 360;   ///< pieces lying about at once (the oldest go first)

}   // namespace

void Game::loadBreakables()
{
    // One mesh per kind, built once for the level; the instances only move it.
    for ( const BreakSpot &s : m_level.m_breaks )
        m_breakables.push_back( { s.m_pos, s.m_yaw, s.m_kind, Looks[(int)s.m_kind].m_hp, s.m_prop } );
    if ( m_breakables.empty() || m_breakMesh[0].vertexCount ) return;
    for ( int k = 0; k < (int)BreakE::COUNT; ++k )
    {
        MeshBuilder b, g;
        b.m_color = Looks[k].m_color;
        Color dark = { 50, 52, 56, 255 };
        switch ( (BreakE)k )
        {
        case BreakE::CRATE:
            b.box( place( { 0.0f, 0.36f, 0.0f } ), { 0.36f, 0.36f, 0.36f } );
            b.m_color = dark;
            for ( float y : { 0.12f, 0.6f } )
                b.box( place( { 0.0f, y, 0.0f } ), { 0.375f, 0.04f, 0.375f } );   // straps
            b.box( place( { 0.0f, 0.73f, 0.0f } ), { 0.3f, 0.015f, 0.3f } );      // the lid's panel
            break;
        case BreakE::DRUM:
            b.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.3f, 0.92f, 16 );
            b.m_color = dark;
            for ( float y : { 0.3f, 0.6f } ) b.cylinder( place( { 0.0f, y, 0.0f } ), 0.31f, 0.03f, 16 );
            b.cylinder( place( { 0.15f, 0.92f, 0.0f } ), 0.05f, 0.03f, 8 );   // its bung
            break;
        case BreakE::CANISTERS:
            for ( int i = 0; i < 3; ++i )
            {
                float a = i * 2.094f;
                b.m_color = Looks[k].m_color;
                b.cylinder( place( { cosf( a ) * 0.13f, 0.0f, sinf( a ) * 0.13f } ), 0.11f, 1.0f, 12 );
                b.m_color = dark;
                b.cylinder( place( { cosf( a ) * 0.13f, 1.0f, sinf( a ) * 0.13f } ), 0.045f, 0.12f, 8 );
            }
            b.box( place( { 0.0f, 0.55f, 0.0f } ), { 0.26f, 0.03f, 0.26f } );   // the strap
            break;
        case BreakE::CASE:
            b.box( place( { 0.0f, 0.15f, 0.0f } ), { 0.4f, 0.15f, 0.28f } );
            b.box( place( { 0.04f, 0.43f, 0.0f }, { 0.0f, 0.2f, 0.0f } ), { 0.34f, 0.13f, 0.24f } );
            b.m_color = { 200, 160, 40, 255 };
            for ( float x : { -0.25f, 0.25f } )
                b.box( place( { x, 0.22f, 0.285f } ), { 0.04f, 0.03f, 0.01f } );   // latches
            break;
        case BreakE::TANK:
            b.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.3f, 0.16f, 16 );
            b.cylinder( place( { 0.0f, 1.08f, 0.0f } ), 0.3f, 0.16f, 16 );
            for ( int i = 0; i < 4; ++i )
                b.box( place( { cosf( i * 1.571f + 0.785f ) * 0.27f, 0.62f, sinf( i * 1.571f + 0.785f ) * 0.27f } ),
                       { 0.025f, 0.46f, 0.025f } );
            g.cylinder( place( { 0.0f, 0.16f, 0.0f } ), 0.24f, 0.92f, 16 );   // the coolant
            break;
        case BreakE::SPOOL:
            for ( float y : { 0.0f, 0.74f } ) b.cylinder( place( { 0.0f, y, 0.0f } ), 0.42f, 0.05f, 18 );
            b.cylinder( place( { 0.0f, 0.05f, 0.0f } ), 0.17f, 0.69f, 12 );
            b.m_color = dark;
            b.cylinder( place( { 0.0f, 0.12f, 0.0f } ), 0.33f, 0.56f, 18 );   // the cable on it
            break;
        case BreakE::LOCKER:   // tall, its door's seam, vents, a handle
            b.box( place( { 0.0f, 0.9f, 0.0f } ), { 0.33f, 0.9f, 0.24f } );
            b.m_color = dark;
            b.box( place( { 0.0f, 0.9f, 0.245f } ), { 0.004f, 0.86f, 0.004f } );
            for ( float y : { 1.45f, 1.55f, 1.65f } ) b.box( place( { -0.16f, y, 0.245f } ), { 0.1f, 0.012f, 0.004f } );
            b.box( place( { 0.08f, 0.95f, 0.255f } ), { 0.015f, 0.08f, 0.012f } );
            break;
        case BreakE::CONSOLE:   // a post, a tilted screen
            b.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.18f, 0.06f, 12 );
            b.cylinder( place( { 0.0f, 0.06f, 0.0f } ), 0.05f, 0.85f, 8 );
            b.box( place( { 0.0f, 1.0f, 0.0f }, { -0.5f, 0.0f, 0.0f } ), { 0.28f, 0.2f, 0.04f } );
            g.box( place( { 0.0f, 1.01f, 0.025f }, { -0.5f, 0.0f, 0.0f } ), { 0.25f, 0.17f, 0.01f } );
            break;
        case BreakE::BATTERY:   // six cells in a frame, a lit band on each
            b.box( place( { 0.0f, 0.03f, 0.0f } ), { 0.34f, 0.03f, 0.24f } );
            for ( int i = 0; i < 6; ++i )
            {
                float x = -0.22f + ( i % 3 ) * 0.22f, z = i < 3 ? -0.11f : 0.11f;
                b.cylinder( place( { x, 0.06f, z } ), 0.08f, 0.5f, 10 );
                g.cylinder( place( { x, 0.38f, z } ), 0.083f, 0.05f, 10 );
            }
            break;
        case BreakE::SACKS:   // three sacks, one on the others
            b.roundedBox( place( { -0.17f, 0.15f, 0.0f } ), { 0.18f, 0.14f, 0.28f }, 0.1f, 3 );
            b.roundedBox( place( { 0.19f, 0.15f, 0.02f }, { 0.0f, 0.3f, 0.0f } ), { 0.18f, 0.14f, 0.28f }, 0.1f, 3 );
            b.roundedBox( place( { 0.0f, 0.42f, 0.0f }, { 0.0f, 1.4f, 0.0f } ), { 0.17f, 0.13f, 0.27f }, 0.1f, 3 );
            break;
        case BreakE::POT:   // a pot, its green
            b.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.22f, 0.36f, 12 );
            b.m_color = { 60, 110, 55, 255 };
            for ( int i = 0; i < 5; ++i )
                b.roundedBox(
                    place( { cosf( i * 1.257f ) * 0.1f, 0.5f + ( i % 2 ) * 0.15f, sinf( i * 1.257f ) * 0.1f } ),
                    { 0.13f, 0.13f, 0.13f }, 0.06f, 2 );
            break;
        case BreakE::VIALS:   // a rack, rows of vials lit from within
            for ( float y : { 0.02f, 0.45f, 0.86f } ) b.box( place( { 0.0f, y, 0.0f } ), { 0.28f, 0.02f, 0.14f } );
            for ( float x : { -0.27f, 0.27f } ) b.box( place( { x, 0.44f, 0.0f } ), { 0.012f, 0.44f, 0.13f } );
            for ( int row = 0; row < 2; ++row )
                for ( int i = 0; i < 6; ++i )
                    g.cylinder( place( { -0.2f + i * 0.08f, 0.04f + row * 0.43f, ( i % 2 ) * 0.06f - 0.03f } ), 0.022f,
                                0.24f, 6 );
            break;
        default:   // CRYSTAL: a dark foot of rock, shards of light out of it
            b.box( place( { 0.0f, 0.12f, 0.0f } ), { 0.32f, 0.12f, 0.28f } );
            for ( int i = 0; i < 5; ++i )
            {
                float a = i * 1.257f, lean = 0.25f + 0.1f * ( i % 2 );
                float h = 0.35f + 0.12f * ( ( i * 7 ) % 4 );
                g.box( place( { cosf( a ) * 0.12f, 0.2f + h, sinf( a ) * 0.12f },
                              { cosf( a ) * lean, 0.0f, -sinf( a ) * lean } ),
                       { 0.06f, h, 0.06f } );
            }
            break;
        }
        m_breakMesh[k] = b.build().front();
        if ( Looks[k].m_glow.x >= 0.0f ) m_breakGlow[k] = g.build().front();
        MeshBuilder chunk;
        chunk.m_color = Looks[k].m_color;
        chunk.box( place( { 0.0f, 0.0f, 0.0f } ), { 0.5f, 0.5f, 0.5f } );
        m_chunkMesh[k] = chunk.build().front();
    }
}

int Game::breakableOnRay( Vector3 from, Vector3 dir, float &t ) const
{
    int found = -1;
    for ( int i = 0; i < (int)m_breakables.size(); ++i )
    {
        const Breakable &b = m_breakables[i];
        if ( !b.m_alive ) continue;
        float u = rayCylinder( from, dir, b.m_pos, Looks[(int)b.m_kind].m_radius, Looks[(int)b.m_kind].m_height );
        if ( u >= 0.0f && u < t ) t = u, found = i;
    }
    return found;
}

void Game::breakablesInRadius( Vector3 p, float radius, float damage )
{
    for ( int i = 0; i < (int)m_breakables.size(); ++i )
    {
        const Breakable &b = m_breakables[i];
        if ( !b.m_alive ) continue;
        Vector3 c = Vector3Add( b.m_pos, { 0.0f, Looks[(int)b.m_kind].m_height * 0.5f, 0.0f } );
        float   d = Vector3Distance( p, c ) - Looks[(int)b.m_kind].m_radius;
        if ( d < radius )
            hitBreakable( i, damage * ( 1.0f - 0.5f * fmaxf( 0.0f, d ) / radius ), c,
                          Vector3Normalize( Vector3Subtract( c, p ) ) );
    }
}

void Game::hitBreakable( int i, float damage, Vector3 at, Vector3 dir )
{
    Breakable &b = m_breakables[i];
    if ( !b.m_alive ) return;
    const BreakLook &look = Looks[(int)b.m_kind];
    b.m_hp -= damage;
    b.m_shake = 0.25f;
    sparks( at, Vector3Negate( dir ), 6, { 230, 210, 170, 255 }, 2.5f );
    if ( b.m_hp > 0.0f )
    {
        m_audio.play( SfxE::HIT, at, m_camTarget, 0.5f );
        return;
    }
    // Broken: out of the way, its pieces flying off from the side it was hit on.
    b.m_alive = false;
    journal::count( "broken" );
    if ( b.m_prop >= 0 && b.m_prop < (int)m_level.m_props.size() ) m_level.m_props[b.m_prop].m_pos.y = -1000.0f;
    m_audio.play( SfxE::SMASH, at, m_camTarget, 0.9f );
    Vector3 mid = Vector3Add( b.m_pos, { 0.0f, look.m_height * 0.5f, 0.0f } );
    int     pieces = 9 + (int)( rnd() * 6 );
    for ( int k = 0; k < pieces; ++k )
    {
        Vector3 out = { rnd() - 0.5f, 0.3f + rnd() * 0.8f, rnd() - 0.5f };
        out = Vector3Add( Vector3Scale( Vector3Normalize( out ), 2.0f + rnd() * 3.0f ), Vector3Scale( dir, 2.0f ) );
        float s = look.m_radius * ( 0.25f + rnd() * 0.35f );
        Chunk c;
        c.m_pos = Vector3Add( mid, { ( rnd() - 0.5f ) * look.m_radius, ( rnd() - 0.5f ) * look.m_height * 0.6f,
                                     ( rnd() - 0.5f ) * look.m_radius } );
        c.m_vel = out;
        c.m_rot = { rnd() * 6.28f, rnd() * 6.28f, rnd() * 6.28f };
        c.m_spin = { ( rnd() - 0.5f ) * 14.0f, ( rnd() - 0.5f ) * 14.0f, ( rnd() - 0.5f ) * 14.0f };
        c.m_size = { s, s * ( 0.3f + rnd() * 0.7f ), s * ( 0.5f + rnd() * 0.5f ) };
        c.m_ground = b.m_pos.y;
        c.m_kind = (int)b.m_kind;
        c.m_life = 4.0f + rnd() * 3.0f;
        m_chunks.push_back( c );
    }
    if ( m_chunks.size() > MaxChunks ) m_chunks.erase( m_chunks.begin(), m_chunks.end() - MaxChunks );
    if ( look.m_glow.x >= 0.0f )
    {
        // Coolant or a crystal's light spills out.
        Color c = { (unsigned char)fminf( 255.0f, look.m_glow.x * 70.0f ),
                    (unsigned char)fminf( 255.0f, look.m_glow.y * 70.0f ),
                    (unsigned char)fminf( 255.0f, look.m_glow.z * 70.0f ), 255 };
        sparks( mid, { 0.0f, 1.0f, 0.0f }, 30, c, 4.0f );
        m_flashes.push_back(
            { mid, { look.m_glow.x * 3.0f, look.m_glow.y * 3.0f, look.m_glow.z * 3.0f }, 4.0f, 0.3f, 0.3f } );
    }
    else
        sparks( mid, { 0.0f, 1.0f, 0.0f }, 14, { 200, 190, 170, 255 }, 3.0f );
    // Now and then something in it: scrap mostly, a stim, rarely a find.
    float   r = rnd();
    Vector3 up = { ( rnd() - 0.5f ) * 1.5f, 3.0f, ( rnd() - 0.5f ) * 1.5f };
    if ( r < 0.25f )
        m_pickups.push_back( { 0, 3 + (int)( rnd() * 6 ), mid, up } );
    else if ( r < 0.32f )
        m_pickups.push_back( { 1, 1, mid, up } );
    else if ( r < 0.35f )
        dropItem( mid, 1.0f );
}

void Game::updateBreakables( float dt )
{
    for ( Breakable &b : m_breakables ) b.m_shake = fmaxf( 0.0f, b.m_shake - dt );
    float g = m_level.m_gravity;
    for ( Chunk &c : m_chunks )
    {
        c.m_life -= dt;
        c.m_vel.y -= g * dt;
        c.m_pos = Vector3Add( c.m_pos, Vector3Scale( c.m_vel, dt ) );
        c.m_rot = Vector3Add( c.m_rot, Vector3Scale( c.m_spin, dt ) );
        float floor = c.m_ground + c.m_size.y * 0.5f;
        if ( c.m_pos.y < floor )
        {
            // On the floor: a bounce, then it lies.
            c.m_pos.y = floor;
            c.m_vel = { c.m_vel.x * 0.55f, fabsf( c.m_vel.y ) * 0.3f, c.m_vel.z * 0.55f };
            c.m_spin = Vector3Scale( c.m_spin, 0.5f );
            if ( fabsf( c.m_vel.y ) < 0.4f ) c.m_vel.y = 0.0f, c.m_spin = Vector3Scale( c.m_spin, 0.2f );
        }
    }
    m_chunks.erase(
        std::remove_if( m_chunks.begin(), m_chunks.end(), []( const Chunk &c ) { return c.m_life <= 0.0f; } ),
        m_chunks.end() );
}

void Game::poseBreakables( std::vector<Drawable> &out )
{
    for ( size_t i = 0; i < m_breakables.size(); ++i )
    {
        const Breakable &b = m_breakables[i];
        if ( !b.m_alive ) continue;
        const BreakLook &look = Looks[(int)b.m_kind];
        // A hit makes it jolt.
        float    jolt = b.m_shake > 0.0f ? sinf( b.m_shake * 60.0f ) * b.m_shake * 0.3f : 0.0f;
        Matrix   m = MatrixMultiply( MatrixRotateXYZ( { jolt, b.m_yaw, jolt * 0.5f } ),
                                     MatrixTranslate( b.m_pos.x, b.m_pos.y, b.m_pos.z ) );
        Drawable d;
        d.m_mesh = m_breakMesh[(int)b.m_kind];
        d.m_surface = look.m_surface;
        d.m_transform = m;
        out.push_back( d );
        if ( look.m_glow.x < 0.0f ) continue;
        float pulse = 0.8f + 0.2f * sinf( m_time * 2.0f + b.m_pos.x );
        d.m_mesh = m_breakGlow[(int)b.m_kind];
        d.m_surface = S_GLOW;
        d.m_emissive = Vector4Scale( look.m_glow, pulse );
        d.m_spotShadow = d.m_sunShadow = false;
        out.push_back( d );
    }
    for ( const Chunk &c : m_chunks )
    {
        // The last second it sinks away.
        float    shrink = fminf( 1.0f, c.m_life );
        Drawable d;
        d.m_mesh = m_chunkMesh[c.m_kind];
        d.m_surface =
            Looks[c.m_kind].m_glow.x >= 0.0f && c.m_kind == (int)BreakE::CRYSTAL ? S_GLOW : Looks[c.m_kind].m_surface;
        if ( d.m_surface == S_GLOW ) d.m_emissive = Vector4Scale( Looks[c.m_kind].m_glow, 0.6f * shrink );
        d.m_transform = MatrixMultiply(
            MatrixMultiply( MatrixScale( c.m_size.x * shrink, c.m_size.y * shrink, c.m_size.z * shrink ),
                            MatrixRotateXYZ( c.m_rot ) ),
            MatrixTranslate( c.m_pos.x, c.m_pos.y, c.m_pos.z ) );
        d.m_spotShadow = false;
        out.push_back( d );
    }
}

}   // namespace spaceopera
