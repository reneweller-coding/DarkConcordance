/**
 * @file GameElements.cpp
 * @brief The elements of Act II's places: the suit's warmth in the cold, its dose of radiation, the day side's heat
 *        and the night side's frost; wind that bends what flies and gusts that throw the enemies off the edges; ice
 *        that breaks over a glowing ocean (falling in is cold, not deadly); nitrogen geysers; darkness and the hero's
 *        lamp; magnetic coils that pull at metal; a guard that keeps sending men; people who show up once something has
 *        happened (a beaten boss who offers a deal).
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"
#include "Text.h"

#include <raymath.h>

#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float IceHp = 2.0f;         ///< blasts a pane of ice takes
constexpr float DunkHarm = 0.18f;     ///< of the hero's health, for a fall into the ocean
constexpr float GustWarn = 2.5f;      ///< seconds of warning before a gust
constexpr float GustTime = 3.5f;      ///< seconds it blows
constexpr float GustPush = 4.5f;      ///< how fast it carries enemies (m/s)
constexpr float GeyserReach = 1.7f;   ///< the column's width (from the vent)
constexpr float GeyserHarm = 16.0f;   ///< health per second in it
constexpr float CoilReach = 14.0f;    ///< how far a coil bends what flies
constexpr float LampRange = 11.0f;    ///< the hero's lamp
constexpr float DarkSight = 7.5f;     ///< how near an enemy sees the hero in the dark (13 in the light)
constexpr int   Colonies = 70;        ///< glowing colonies under the ice

}   // namespace

void Game::loadElements()
{
    // A sky of one colour where the level has no picture of one (a gas giant's haze).
    if ( m_level.m_sky.empty() && m_level.m_skyColor.x >= 0.0f )
    {
        Color c = { (unsigned char)( Clamp( m_level.m_skyColor.x, 0.0f, 1.0f ) * 255 ),
                    (unsigned char)( Clamp( m_level.m_skyColor.y, 0.0f, 1.0f ) * 255 ),
                    (unsigned char)( Clamp( m_level.m_skyColor.z, 0.0f, 1.0f ) * 255 ), 255 };
        Image img = GenImageColor( 4, 4, c );
        m_skyColorTex = LoadTextureFromImage( img );
        UnloadImage( img );
    }
    // The suit's other need: as long as the level says where it drains.
    if ( !m_level.m_meter.empty() )
        m_oxygenMax = m_oxygen = m_level.m_meterSecs * ( 1.0f + stat( StatE::RESIST ) / 100.0f );
    m_gustTimer = m_level.m_gusts * 0.7f;
    m_safeSpot = m_pos;
    m_pressureT = m_level.m_pressure * 0.5f;
    m_clock = m_level.m_pressure > 0.0f ? 20.0f * 60.0f : 0.0f;
    for ( const Marker &m : m_level.m_markers )
    {
        if ( m.m_kind == ')' ) m_geysers.push_back( { m.m_pos, 2.0f + rnd() * 5.0f, 0 } );
        if ( m.m_kind == ']' )
        {
            // A wandering component: it glides along the longer free run through its cell, to and fro.
            auto run = [&]( int dr, int dc )
            {
                int n = 0;
                while ( n < 12 && m_level.walkable( m.m_row + dr * ( n + 1 ), m.m_col + dc * ( n + 1 ) ) ) ++n;
                return n;
            };
            int     e = run( 0, 1 ), w = run( 0, -1 ), s = run( 1, 0 ), nn = run( -1, 0 );
            bool    alongX = e + w >= s + nn;
            Vector3 a = alongX ? m_level.centre( m.m_row, m.m_col - w ) : m_level.centre( m.m_row - nn, m.m_col );
            Vector3 b = alongX ? m_level.centre( m.m_row, m.m_col + e ) : m_level.centre( m.m_row + s, m.m_col );
            float   len = Vector3Distance( a, b );
            if ( len > 5.0f ) m_movers.push_back( { a, b, rnd(), rnd() < 0.5f ? 1.0f : -1.0f, len } );
        }
        if ( m.m_kind == '[' )
        {
            m_coils.push_back( m.m_pos );
            m_level.m_props.push_back( { m.m_pos, 0.8f } );
        }
    }
    // Ice: each pane takes a blast or two; the ocean under it glows where the colonies live.
    if ( m_level.m_ice )
    {
        for ( auto rc : m_level.m_glass ) m_iceHp[rc.first * 1000 + rc.second] = IceHp;
        Vector3 lo = { 1e9f, 0.0f, 1e9f }, hi = { -1e9f, 0.0f, -1e9f };
        for ( int r = 0; r < m_level.rows(); ++r )
            for ( int c = 0; c < m_level.cols(); ++c )
            {
                Vector3 p = m_level.centre( r, c );
                lo.x = fminf( lo.x, p.x ), lo.z = fminf( lo.z, p.z ), hi.x = fmaxf( hi.x, p.x ),
                hi.z = fmaxf( hi.z, p.z );
            }
        m_oceanLo = lo, m_oceanHi = hi;
        // The sea under the ice: dark, still; the colonies hang just beneath the crust.
        MeshBuilder deep;
        deep.box( place( { ( lo.x + hi.x ) * 0.5f, -1.7f, ( lo.z + hi.z ) * 0.5f } ),
                  { ( hi.x - lo.x ) * 0.5f + 300.0f, 0.1f, ( hi.z - lo.z ) * 0.5f + 300.0f } );
        m_deepMesh = deep.build().front();
        MeshBuilder blob;
        blob.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 1.0f, 0.35f, 1.0f }, 0.34f, 3 );
        m_colonyMesh = blob.build().front();
        for ( int i = 0; i < ( m_level.m_keys.count( "crust" ) ? 0 : Colonies ); ++i )
            m_colonies.push_back( { lo.x - 20.0f + rnd() * ( hi.x - lo.x + 40.0f ), -0.9f - rnd() * 0.6f,
                                    lo.z - 20.0f + rnd() * ( hi.z - lo.z + 40.0f ), rnd() * 6.283f } );
    }
    // Clouds below: balloons the size of quarters hang about the refinery, on cables.
    if ( m_level.m_below == "clouds" )
    {
        MeshBuilder balloon;
        balloon.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 1.0f, 0.62f, 1.6f }, 0.6f, 4 );
        m_balloonMesh = balloon.build().front();
        for ( int i = 0; i < 9; ++i )
        {
            int     r = (int)( rnd() * m_level.rows() ), c = (int)( rnd() * m_level.cols() );
            Vector3 p = m_level.centre( r, c );
            m_balloons.push_back( { p.x + ( rnd() - 0.5f ) * 30.0f, 9.0f + rnd() * 9.0f, p.z + ( rnd() - 0.5f ) * 30.0f,
                                    6.0f + rnd() * 7.0f } );
        }
    }
    // A river of star matter through the sky: blobs along a great arc far off.
    if ( m_level.m_plasma )
    {
        MeshBuilder blob;
        blob.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 1.0f, 1.0f, 1.0f }, 0.9f, 3 );
        m_plasmaMesh = blob.build().front();
    }
    if ( !m_movers.empty() )
    {
        MeshBuilder block;
        block.roundedBox( place( { 0.0f, 1.9f, 0.0f } ), { 0.85f, 1.9f, 0.85f }, 0.25f, 2 );
        m_moverMesh = block.build().front();
        MeshBuilder seam;
        seam.box( place( { 0.0f, 1.9f, 0.0f } ), { 0.87f, 0.03f, 0.87f } );
        seam.box( place( { 0.0f, 3.0f, 0.0f } ), { 0.87f, 0.02f, 0.87f } );
        m_moverSeam = seam.build().front();
    }
    if ( !m_geysers.empty() || !m_coils.empty() )
    {
        MeshBuilder vent;
        vent.lathe( place( { 0.0f, 0.0f, 0.0f } ),
                    { { 0.0f, 0.02f }, { 0.55f, 0.02f }, { 0.9f, 0.18f }, { 1.15f, 0.0f } }, 20 );
        m_ventMesh = vent.build().front();
        MeshBuilder coil;
        coil.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.7f, 0.4f, 16 );
        coil.cylinder( place( { 0.0f, 0.4f, 0.0f } ), 0.25f, 3.2f, 12 );
        for ( float y : { 1.2f, 2.0f, 2.8f } )
            coil.lathe( place( { 0.0f, y, 0.0f } ),
                        { { 0.3f, 0.0f }, { 0.75f, 0.0f }, { 0.75f, 0.25f }, { 0.3f, 0.25f } }, 20 );
        m_coilMesh = coil.build().front();
        MeshBuilder core;
        core.cylinder( place( { 0.0f, 0.5f, 0.0f } ), 0.31f, 3.0f, 12 );
        m_coilCore = core.build().front();
    }
    // Someone who shows only once something has happened (the beaten boss, the ship's computer at the end).
    for ( NpcState &n : m_npcs )
    {
        auto after = m_level.m_npcAfter.find( m_level.m_npcs[n.m_def].m_id );
        n.m_hidden = after != m_level.m_npcAfter.end() && !m_fired.count( after->second );
    }
}

void Game::rebuildGlass()
{
    if ( m_glassMesh.vertexCount > 0 ) UnloadMesh( m_glassMesh );
    m_glassMesh = {};
    if ( m_level.m_glass.empty() ) return;
    MeshBuilder glass;
    for ( auto rc : m_level.m_glass )
    {
        Vector3 p = m_level.centre( rc.first, rc.second );
        if ( m_level.m_keys.count( "crust" ) )
            glass.box( place( { p.x, -0.25f, p.z } ), { CellSize * 0.5f, 0.25f, CellSize * 0.5f } );   // a slab
        else
            glass.box( place( { p.x, -0.02f, p.z } ), { CellSize * 0.5f, 0.02f, CellSize * 0.5f } );
    }
    m_glassMesh = glass.build().front();
}

void Game::crackIce( Vector3 p, float radius, float amount )
{
    if ( !m_level.m_ice || p.y > 2.5f ) return;
    bool broke = false;
    for ( auto it = m_iceHp.begin(); it != m_iceHp.end(); )
    {
        int     r = it->first / 1000, c = it->first % 1000;
        Vector3 q = m_level.centre( r, c );
        if ( Vector2Distance( { p.x, p.z }, { q.x, q.z } ) > radius )
        {
            ++it;
            continue;
        }
        it->second -= amount;
        if ( it->second > 0.0f )
        {
            ++it;
            continue;
        }
        // The pane goes: shards, a splash, open water where one stood.
        m_level.setVoid( r, c );
        if ( m_crust ) m_regrow.push_back( { r, c, 30.0f } );
        for ( size_t i = 0; i < m_level.m_glass.size(); ++i )
            if ( m_level.m_glass[i].first == r && m_level.m_glass[i].second == c )
            {
                m_level.m_glass.erase( m_level.m_glass.begin() + (long)i );
                break;
            }
        for ( int k = 0; k < 10; ++k )
            m_particles.push_back( { Vector3Add( q, { ( rnd() - 0.5f ) * 1.6f, 0.1f, ( rnd() - 0.5f ) * 1.6f } ),
                                     { ( rnd() - 0.5f ) * 3.0f, 2.0f + rnd() * 3.0f, ( rnd() - 0.5f ) * 3.0f },
                                     1.0f,
                                     1.0f,
                                     0.06f,
                                     { 215, 240, 255, 230 },
                                     0 } );
        it = m_iceHp.erase( it );
        broke = true;
    }
    if ( broke )
    {
        rebuildGlass();
        m_audio.play( SfxE::BURST, p, m_camTarget, 0.7f );
        fire( "icebreak" );
    }
}

Vector3 Game::drift( Vector3 p, Vector3 v ) const
{
    // What bends a shot in flight: the drum's spin, the wind (harder in a gust), the coils' fields.
    Vector3 a = coriolis( v );
    if ( m_level.m_windStrength > 0.0f )
        a = Vector3Add( a, Vector3Scale( m_level.m_windDir, m_level.m_windStrength * ( 1.0f + 2.0f * m_gust ) ) );
    for ( const Vector3 &c : m_coils )
    {
        Vector3 to = { c.x - p.x, 0.0f, c.z - p.z };
        float   d = Vector3Length( to );
        if ( d < CoilReach && d > 0.5f ) a = Vector3Add( a, Vector3Scale( to, fminf( 14.0f, 60.0f / ( d * d ) ) / d ) );
    }
    return a;
}

bool Game::heroLit() const
{
    // In the dark one is seen from afar only where the place's own lamps shine.
    for ( const LevelLight &l : m_level.m_lights )
        if ( Vector2Distance( { l.m_pos.x, l.m_pos.z }, { m_pos.x, m_pos.z } ) < 5.0f ) return true;
    return false;
}

float Game::sightRange() const
{
    return m_level.m_dark && !heroLit() ? DarkSight : 13.0f;
}

void Game::updateElements( float dt )
{
    int r, c;
    m_level.cellOf( m_pos, r, c );
    // The suit's need: warmth outdoors in the cold (indoors too where everything is cold), the dose out in the open by
    // the star, the day side's heat and the night side's frost; heaters, shields and the twilight between set it right.
    m_metering = false;
    if ( !m_level.m_meter.empty() )
    {
        bool post = false;
        for ( const Vector3 &p : m_airPosts )
            post = post || Vector2Distance( { p.x, p.z }, { m_pos.x, m_pos.z } ) < 2.4f;
        bool drain = false;
        if ( m_level.m_meter == "warmth" )
            drain = m_level.outdoor( r, c ) || m_level.m_meterIndoor;
        else if ( m_level.m_meter == "dose" )
            drain = exposed( m_pos );
        else
        {
            // "shadow": the shield's shadow sweeps; out of it, in the open, the star burns on both sides.
            bool shadow = m_level.m_meter == "shadow";
            m_meterHot = c < m_level.m_bandHot || shadow;
            drain = c < m_level.m_bandHot || c > m_level.m_bandCold;
            if ( shadow ) drain = drain && m_level.outdoor( r, c );
        }
        m_metering = drain && !post;
        float refill = post ? 25.0f : ( m_level.m_meter == "dose" ? 3.0f : 12.0f );
        if ( m_dead )
            m_oxygen = m_oxygenMax;
        else if ( m_metering && !m_finished && m_scene == 0 && !m_talk )
        {
            m_oxygen = fmaxf( 0.0f, m_oxygen - dt * ( m_flareState == 2 ? 3.0f : 1.0f ) );
            if ( m_oxygen <= 0.0f )
            {
                m_hurtBy = m_level.m_meter == "dose" ? "rad" : ( m_meterHot ? "heat" : "cold" );
                drainHero( 6.0f * harm( m_difficulty ) * dt );
            }
            if ( m_oxygen < m_oxygenMax * 0.25f ) fire( "lowmeter" );
        }
        else
            m_oxygen = fminf( m_oxygenMax, m_oxygen + refill * dt );
    }

    // Wind: gusts come with a warning; they push the enemies (off an edge, if they stand by one), the hero's boots
    // hold, and what flies is bent harder.
    if ( m_level.m_gusts > 0.0f && !m_finished && m_scene == 0 )
    {
        m_gustT += dt;
        if ( m_gustState == 0 && m_gustT > m_gustTimer )
            m_gustState = 1, m_gustT = 0.0f, fire( "gustwarn" );
        else if ( m_gustState == 1 && m_gustT > GustWarn )
            m_gustState = 2, m_gustT = 0.0f, fire( "gust" ), m_audio.play( SfxE::THRUST, m_pos, m_camTarget, 0.6f );
        else if ( m_gustState == 2 && m_gustT > GustTime )
            m_gustState = 0, m_gustT = 0.0f, m_gustTimer = m_level.m_gusts * ( 0.8f + 0.4f * rnd() );
        float want = m_gustState == 2 ? 1.0f : 0.0f;
        m_gust += ( want - m_gust ) * fminf( 1.0f, dt * 3.0f );
        if ( m_gust > 0.05f )
        {
            Vector3 push = Vector3Scale( m_level.m_windDir, GustPush * m_gust );
            m_vel = Vector3Add( m_vel, Vector3Scale( push, dt * ( m_block > 0.5f ? 0.2f : 0.8f ) ) );
            for ( Foe &f : m_foes )
            {
                if ( !f.m_alive || f.m_hidden || f.m_kind == FoeE::HEAVY || f.m_kind == FoeE::TAXATOR ||
                     f.m_elite == 2 || f.m_kind == FoeE::SHELL )
                    continue;
                f.m_pos = Vector3Add( f.m_pos, Vector3Scale( push, dt ) );
                if ( groundAt( f.m_pos ) < -100.0f && f.m_kind != FoeE::DRONE ) f.m_ground = false;
            }
            // Streaks of the wind, and loose stuff in it.
            if ( rnd() < dt * 40.0f )
            {
                Vector3 at =
                    Vector3Add( m_pos, { ( rnd() - 0.5f ) * 30.0f, 0.5f + rnd() * 4.0f, ( rnd() - 0.5f ) * 30.0f } );
                m_particles.push_back( { at,
                                         Vector3Scale( m_level.m_windDir, 18.0f + rnd() * 8.0f ),
                                         0.8f,
                                         0.8f,
                                         0.03f,
                                         { 230, 220, 200, 120 },
                                         1 } );
            }
        }
    }

    // On the ice: the last safe place; a fall through into the ocean brings one back there, cold and hurt.
    if ( m_level.m_ice && !m_dead )
    {
        bool solid = m_ground;
        for ( int dr = -1; dr <= 1 && solid; ++dr )
            for ( int dc = -1; dc <= 1 && solid; ++dc ) solid = m_level.cell( r + dr, c + dc ).m_kind != CellE::VOID;
        if ( solid ) m_safeSpot = m_pos;
        if ( m_pos.y < -0.9f )
        {
            for ( int k = 0; k < 14; ++k )
                m_particles.push_back(
                    { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 1.2f, 1.0f, ( rnd() - 0.5f ) * 1.2f } ),
                      { ( rnd() - 0.5f ) * 2.0f, 3.0f + rnd() * 3.0f, ( rnd() - 0.5f ) * 2.0f },
                      1.0f,
                      1.0f,
                      0.05f,
                      { 200, 230, 255, 220 },
                      0 } );
            m_audio.play( SfxE::SIZZLE, m_pos, m_camTarget, 0.8f );
            m_hurtBy = "cold";
            drainHero( hpMax() * DunkHarm );
            m_oxygen *= 0.6f;
            m_pos = m_safeSpot;
            m_vel = { 0.0f, 0.0f, 0.0f };
            m_invuln = fmaxf( m_invuln, 1.2f );
            m_path.clear();
            m_planTimer = 0.0f;
            fire( "dunk" );
        }
    }

    // Geysers: a hiss, then a column of frozen nitrogen that burns and throws one up.
    for ( Geyser &g : m_geysers )
    {
        g.m_t -= dt;
        if ( g.m_state == 0 && g.m_t <= 0.0f )
            g.m_state = 1, g.m_t = 1.4f;
        else if ( g.m_state == 1 && g.m_t <= 0.0f )
        {
            g.m_state = 2, g.m_t = 2.2f;
            m_audio.play( SfxE::BURST, g.m_pos, m_camTarget, 0.8f );
        }
        else if ( g.m_state == 2 && g.m_t <= 0.0f )
            g.m_state = 0, g.m_t = 4.0f + rnd() * 4.0f;
        if ( g.m_state == 0 ) continue;
        float rate = g.m_state == 1 ? 10.0f : 70.0f;
        if ( rnd() < dt * rate )
            m_particles.push_back(
                { Vector3Add( g.m_pos, { ( rnd() - 0.5f ) * 0.6f, 0.2f, ( rnd() - 0.5f ) * 0.6f } ),
                  { ( rnd() - 0.5f ) * 1.5f, g.m_state == 1 ? 1.5f : 9.0f + rnd() * 5.0f, ( rnd() - 0.5f ) * 1.5f },
                  g.m_state == 1 ? 0.6f : 1.1f,
                  g.m_state == 1 ? 0.6f : 1.1f,
                  g.m_state == 1 ? 0.04f : 0.12f,
                  { 225, 240, 255, 200 },
                  1 } );
        if ( g.m_state != 2 ) continue;
        if ( Vector2Distance( { g.m_pos.x, g.m_pos.z }, { m_pos.x, m_pos.z } ) < GeyserReach && m_pos.y < 4.0f )
        {
            m_hurtBy = "geyser";
            drainHero( GeyserHarm * harm( m_difficulty ) * dt );
            if ( m_ground ) m_vel.y = 6.0f, m_ground = false;
        }
        for ( Foe &f : m_foes )
            if ( f.m_alive && !f.m_hidden &&
                 Vector2Distance( { g.m_pos.x, g.m_pos.z }, { f.m_pos.x, f.m_pos.z } ) < GeyserReach )
            {
                f.m_hp -= GeyserHarm * 2.0f * dt;
                if ( f.m_hp <= 0.0f ) killFoe( f, true );
            }
    }

    // The wandering components glide to and fro; they push whoever is in the way, and crush one against a wall.
    for ( Mover &mv : m_movers )
    {
        mv.m_t += mv.m_dir * 0.6f * dt / mv.m_len;
        if ( mv.m_t > 1.0f ) mv.m_t = 1.0f, mv.m_dir = -1.0f;
        if ( mv.m_t < 0.0f ) mv.m_t = 0.0f, mv.m_dir = 1.0f;
        Vector3 p = Vector3Lerp( mv.m_a, mv.m_b, mv.m_t );
        float   hx = 0.9f, hz = 0.9f;
        if ( fabsf( m_pos.x - p.x ) < hx + HeroRadius && fabsf( m_pos.z - p.z ) < hz + HeroRadius && m_pos.y < 3.6f )
        {
            Vector3 before = m_pos;
            pushOutRect( m_pos, HeroRadius, p.x - hx, p.x + hx, p.z - hz, p.z + hz );
            m_level.collide( m_pos, HeroRadius, StepUp );
            // Pushed back in by a wall: caught between.
            if ( fabsf( m_pos.x - p.x ) < hx + HeroRadius * 0.6f && fabsf( m_pos.z - p.z ) < hz + HeroRadius * 0.6f )
            {
                m_hurtBy = "crushed";
                drainHero( 35.0f * harm( m_difficulty ) * dt );
                fire( "crushed" );
            }
            (void)before;
        }
        for ( Foe &f : m_foes )
            if ( f.m_alive && !f.m_hidden && fabsf( f.m_pos.x - p.x ) < hx + 0.5f &&
                 fabsf( f.m_pos.z - p.z ) < hz + 0.5f )
                pushOutRect( f.m_pos, 0.5f, p.x - hx, p.x + hx, p.z - hz, p.z + hz );
    }

    // The coils pull at the suit's metal (a little; they do more to shots).
    for ( const Vector3 &cp : m_coils )
    {
        Vector3 to = { cp.x - m_pos.x, 0.0f, cp.z - m_pos.z };
        float   d = Vector3Length( to );
        if ( d < 7.0f && d > 1.2f ) m_vel = Vector3Add( m_vel, Vector3Scale( to, 1.6f * dt / d ) );
    }

    // In the dark the hero's lamp is all the light he brings.
    if ( m_level.m_dark && !m_dead )
    {
        Vector3 ahead = { sinf( m_yaw ) * 2.0f, 2.4f, cosf( m_yaw ) * 2.0f };
        m_flashes.push_back( { Vector3Add( m_pos, ahead ), { 3.2f, 3.0f, 2.6f }, LampRange, dt * 1.5f, dt * 1.5f } );
    }

    // The guard keeps sending men while the hero is on board.
    if ( m_level.m_pressure > 0.0f && !m_finished && !m_dead && !bossFight() && !m_fired.count( "bossdead" ) )
    {
        m_pressureT += dt;
        m_clock = fmaxf( 0.0f, m_clock - dt );
        if ( m_pressureT > m_level.m_pressure * ( m_clock > 0.0f ? 1.0f : 0.6f ) )
        {
            m_pressureT = 0.0f;
            // Where they come from: a free cell some way off, behind the hero's back if there is one.
            for ( int t = 0; t < 40; ++t )
            {
                float   a = rnd() * 6.283f, d = 13.0f + rnd() * 8.0f;
                Vector3 p = { m_pos.x + cosf( a ) * d, 0.0f, m_pos.z + sinf( a ) * d };
                int     pr, pc;
                m_level.cellOf( p, pr, pc );
                if ( !m_level.walkable( pr, pc ) || m_level.cell( pr, pc ).m_kind == CellE::DOOR ) continue;
                p = m_level.centre( pr, pc );
                int n = 2 + m_level.m_area / 5;
                if ( m_level.word( "squad" ) == "drones" )
                {
                    // The Collegium sends drones, not men.
                    for ( int k = 0; k < n + 1; ++k )
                        m_summons.push_back( { FoeE::DRONE, Vector3Add( p, { ( rnd() - 0.5f ) * 2.0f, 1.8f,
                                                                             ( rnd() - 0.5f ) * 2.0f } ) } );
                    fire( "reinforce" );
                    break;
                }
                for ( int k = 0; k < n; ++k )
                    m_summons.push_back(
                        { k == 0 ? FoeE::SHIELD : ( k % 2 ? FoeE::GUNNER : FoeE::BATON ),
                          Vector3Add( p, { ( rnd() - 0.5f ) * 1.5f, 0.0f, ( rnd() - 0.5f ) * 1.5f } ) } );
                fire( "reinforce" );
                break;
            }
        }
    }

    // People who show once their moment has come.
    for ( NpcState &n : m_npcs )
        if ( n.m_hidden )
        {
            auto after = m_level.m_npcAfter.find( m_level.m_npcs[n.m_def].m_id );
            if ( after == m_level.m_npcAfter.end() || !m_fired.count( after->second ) ) continue;
            n.m_hidden = false;
            // Placed by the level (a written one) or where the boss fell (a generated one).
            const Npc &def = m_level.m_npcs[n.m_def];
            if ( def.m_row < 0 )
            {
                n.m_pos = Vector3Add( m_bossAt, { 1.2f + 3.2f * m_appeared, 0.0f, 1.2f } );
                ++m_appeared;
                // On floor one can reach: the nearest open cell (a boss may fall over a shaft or by cover).
                int nr, nc, best = -1;
                m_level.cellOf( n.m_pos, nr, nc );
                for ( int d = 0; d <= 8 && best < 0; ++d )
                    for ( int dr = -d; dr <= d && best < 0; ++dr )
                        for ( int dc = -d; dc <= d && best < 0; ++dc )
                        {
                            int rr = nr + dr, cc = nc + dc;
                            if ( std::max( abs( dr ), abs( dc ) ) != d || !m_level.walkable( rr, cc ) ) continue;
                            CellE k = m_level.cell( rr, cc ).m_kind;
                            if ( k != CellE::FLOOR && k != CellE::GRATE && k != CellE::GLASS ) continue;
                            if ( m_level.cell( rr, cc ).m_height != 0.0f ) continue;
                            best = rr * m_level.cols() + cc;
                        }
                if ( best >= 0 ) n.m_pos = m_level.centre( best / m_level.cols(), best % m_level.cols() );
                n.m_pos.y = fmaxf( 0.0f, groundAt( n.m_pos ) );
                n.m_figure.m_pos = n.m_pos;
            }
            fire( "appear." + def.m_id );
        }

    // The act's end: the boss down, a call (two ships), seven seconds, then everything goes white.
    if ( m_level.m_whiteout == "collegium" && m_fired.count( "bossdead" ) )
    {
        fire( "collegiumcall" );
        m_collegiumT += dt;
        if ( m_collegiumT > 7.0f ) fire( m_level.m_whiteout );
    }
    if ( !m_level.m_whiteout.empty() && m_fired.count( m_level.m_whiteout ) && !m_talk )
    {
        m_whiteT += dt;
        // Once all is white, the act's film (once); the words come after it.
        if ( m_whiteT > 5.5f && !m_fired.count( "endfilm" ) )
        {
            m_fired.insert( "endfilm" );
            startFilm( m_level.word( "endfilm" ) );
        }
        if ( m_whiteT > 11.0f && !m_finished )
        {
            runEvent( m_level.m_endFlag );   // kept: the people of the next hub know the act is over
            m_finished = true;
        }
    }

    // Lightning in the clouds below: a flash deep down now and then.
    if ( m_level.m_below == "clouds" && rnd() < dt * 0.25f )
    {
        Vector3 at =
            Vector3Add( m_pos, { ( rnd() - 0.5f ) * 60.0f, -25.0f - rnd() * 20.0f, ( rnd() - 0.5f ) * 60.0f } );
        m_flashes.push_back( { at, { 60.0f, 60.0f, 80.0f }, 60.0f, 0.18f, 0.18f } );
        m_lightning = 0.25f;
    }
    m_lightning = fmaxf( 0.0f, m_lightning - dt );
}

void Game::poseElements( std::vector<Drawable> &out )
{
    Drawable d;
    d.m_spotShadow = d.m_sunShadow = false;
    // The colonies under the ice: slow pulses of green and blue light, each in its own rhythm; the dark sea floor.
    if ( m_level.m_ice )
    {
        Drawable floor;
        floor.m_mesh = m_deepMesh;
        floor.m_transform = MatrixIdentity();
        floor.m_surface = S_CAVERN;   // matte and nearly black: the lamps must not light the sea's floor
        if ( m_level.m_keys.count( "crust" ) )
        {
            // Under a peeled moon's crust its inside glows.
            float pulse = 0.85f + 0.15f * sinf( m_time * 0.9f );
            floor.m_surface = S_GLOW;
            floor.m_emissive = { 4.0f * pulse, 1.1f * pulse, 0.18f * pulse, 0.0f };
            out.push_back( floor );
            floor.m_emissive = { -1.0f, 0.0f, 0.0f, 0.0f };
            floor.m_surface = S_CAVERN;
        }
        else
            floor.m_emissive = { 0.006f, 0.02f, 0.035f, 0.0f };
        floor.m_spotShadow = floor.m_sunShadow = false;
        out.push_back( floor );
        for ( const Vector4 &k : m_colonies )
        {
            float pulse = 0.55f + 0.45f * sinf( m_time * 0.6f + k.w );
            float size = 0.9f + fmodf( k.w * 37.0f, 1.8f );
            d.m_mesh = m_colonyMesh;
            d.m_surface = S_GLOW;
            d.m_transform = MatrixMultiply(
                MatrixMultiply( MatrixScale( size, size * 0.5f, size * 1.6f ), MatrixRotateY( k.w * 3.0f ) ),
                MatrixTranslate( k.x, k.y, k.z ) );
            d.m_emissive = { 0.015f * pulse, 0.14f * pulse, 0.12f * pulse, 0.0f };
            out.push_back( d );
        }
        // Cracks in panes that took a blast: dark lines across.
        for ( const auto &ice : m_iceHp )
        {
            if ( ice.second >= IceHp ) continue;
            Vector3 q = m_level.centre( ice.first / 1000, ice.first % 1000 );
            d.m_mesh = m_ring;
            d.m_surface = S_GLOW;
            d.m_transform = MatrixMultiply( MatrixScale( 1.2f, 1.0f, 1.2f ), MatrixTranslate( q.x, 0.03f, q.z ) );
            d.m_emissive = { 0.6f, 0.9f, 1.1f, 0.0f };
            out.push_back( d );
        }
    }
    // Balloons over the refinery.
    for ( const Vector4 &b : m_balloons )
    {
        Drawable bd;
        bd.m_mesh = m_balloonMesh;
        bd.m_surface = S_CLOTH;
        bd.m_transform = MatrixMultiply( MatrixScale( b.w, b.w, b.w ), MatrixTranslate( b.x, b.y, b.z ) );
        out.push_back( bd );
    }
    // The star's river: a great arc of glowing matter, far off and slow.
    if ( m_level.m_plasma )
        for ( int i = 0; i < 40; ++i )
        {
            float   t = i / 39.0f, a = -1.2f + 2.4f * t;
            Vector3 p = { m_pos.x + sinf( a ) * 700.0f, -120.0f + cosf( a * 1.3f ) * 260.0f,
                          m_pos.z - 600.0f + t * 50.0f };
            float   flow = 0.8f + 0.2f * sinf( m_time * 2.0f - i * 0.5f );
            d.m_mesh = m_plasmaMesh;
            d.m_surface = S_GLOW;
            d.m_transform = MatrixMultiply( MatrixScale( 45.0f, 30.0f, 45.0f ), MatrixTranslate( p.x, p.y, p.z ) );
            d.m_emissive = { 9.0f * flow, 4.0f * flow, 1.2f * flow, 0.0f };
            out.push_back( d );
        }
    // The wandering components: black, a seam of light round them.
    for ( const Mover &mv : m_movers )
    {
        Vector3  p = Vector3Lerp( mv.m_a, mv.m_b, mv.m_t );
        Drawable md;
        md.m_mesh = m_moverMesh;
        md.m_surface = S_DARK;
        md.m_transform = MatrixTranslate( p.x, 0.0f, p.z );
        out.push_back( md );
        md.m_mesh = m_moverSeam;
        md.m_surface = S_GLOW;
        float pulse = 0.6f + 0.4f * sinf( m_time * 0.7f + p.x );
        md.m_emissive = { 0.3f * pulse, 1.4f * pulse, 2.2f * pulse, 0.0f };
        md.m_spotShadow = md.m_sunShadow = false;
        out.push_back( md );
    }
    // Vents and coils.
    for ( const Geyser &g : m_geysers )
    {
        d.m_mesh = m_ventMesh;
        d.m_surface = S_ROCK;
        d.m_emissive = { -1.0f, 0.0f, 0.0f, 0.0f };
        d.m_transform = MatrixTranslate( g.m_pos.x, 0.0f, g.m_pos.z );
        out.push_back( d );
        if ( g.m_state == 0 ) continue;
        d.m_mesh = m_ring;
        d.m_surface = S_GLOW;
        d.m_transform = MatrixMultiply( MatrixScale( GeyserReach / 0.78f, 1.0f, GeyserReach / 0.78f ),
                                        MatrixTranslate( g.m_pos.x, 0.05f, g.m_pos.z ) );
        float w = g.m_state == 1 ? 0.6f + 0.4f * sinf( m_time * 25.0f ) : 1.0f;
        d.m_emissive = { 1.5f * w, 2.5f * w, 4.0f * w, 0.0f };
        out.push_back( d );
    }
    for ( const Vector3 &cp : m_coils )
    {
        Drawable cd;
        cd.m_mesh = m_coilMesh;
        cd.m_surface = S_METAL;
        cd.m_transform = MatrixTranslate( cp.x, 0.0f, cp.z );
        out.push_back( cd );
        cd.m_mesh = m_coilCore;
        cd.m_surface = S_GLOW;
        float hum = 0.75f + 0.25f * sinf( m_time * 9.0f + cp.x );
        cd.m_emissive = { 0.6f * hum, 1.6f * hum, 4.5f * hum, 0.0f };
        cd.m_spotShadow = cd.m_sunShadow = false;
        out.push_back( cd );
    }
}

void Game::drawSea( Gfx & ) {}

void Game::drawElementsHud( int w, int h, float s )
{
    auto text = [&]( const char *t, float x, float y, float size, Color col, bool centre )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        Vector2 p = { centre ? x - m.x * 0.5f : x, y };
        DrawTextEx( m_font, t, { p.x + 2.0f * s, p.y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( col.a * 0.8f ) } );
        DrawTextEx( m_font, t, p, size * s, 1.0f, col );
    };
    if ( !m_level.m_meter.empty() )
    {
        // The suit's warmth, dose or temperature over the hero's panel (red and blinking when it runs out).
        float x = 34.0f * s, y = h - 214.0f * s, bw = 266.0f * s;
        float f = Clamp( m_oxygen / m_oxygenMax, 0.0f, 1.0f );
        bool  low = f < 0.25f;
        Color full = m_level.m_meter == "dose"     ? Color{ 150, 230, 90, 255 }
                     : m_level.m_meter == "warmth" ? Color{ 255, 170, 80, 255 }
                     : m_meterHot                  ? Color{ 255, 120, 60, 255 }
                                                   : Color{ 120, 190, 255, 255 };
        Color c = low ? ( fmodf( m_time, 0.6f ) < 0.3f ? Color{ 255, 70, 60, 255 } : Color{ 140, 30, 25, 255 } ) : full;
        const char *name = m_level.m_meter == "dose"     ? "hud.dose"
                           : m_level.m_meter == "warmth" ? "hud.warmth"
                                                         : ( m_meterHot ? "hud.heat" : "hud.frost" );
        text( TextFormat( "%s  %s", tr( name ), m_metering ? "" : tr( "hud.safe" ) ), x, y - 24.0f * s, 18,
              { 235, 220, 200, 230 }, false );
        DrawRectangle( (int)x, (int)y, (int)bw, (int)( 9 * s ), { 30, 20, 15, 200 } );
        DrawRectangle( (int)x, (int)y, (int)( bw * f ), (int)( 9 * s ), c );
    }
    if ( m_gustState == 1 )
        text( tr( "hud.gustwarn" ), w * 0.5f, h * 0.16f, 34, { 230, 220, 190, 255 }, true );
    else if ( m_gustState == 2 )
        text( tr( "hud.gust" ), w * 0.5f, h * 0.16f, 34, { 255, 235, 190, 255 }, true );
    if ( m_level.m_pressure > 0.0f && !m_finished && m_level.m_whiteout == "collegium" )
    {
        int left = (int)m_clock;
        text( TextFormat( tr( "hud.clock" ), left / 60, left % 60 ), w * 0.5f, 92.0f * s, 22,
              m_clock > 0.0f ? Color{ 200, 220, 255, 220 } : Color{ 255, 90, 70, 255 }, true );
    }
    if ( m_lightning > 0.0f ) DrawRectangle( 0, 0, w, h, { 220, 225, 255, (unsigned char)( 120 * m_lightning ) } );
}

}   // namespace spaceopera
