/**
 * @file GameSwarm.cpp
 * @brief A boss of many: a swarm of small units fed by hives. The Collegium's drones over the hearing, the computing
 *        moon's maintenance robots and its computing cores, *Kein Grund Zur Sorge* embodied in thousands of drones, its
 *        deletions in the night shift's world. The hives make new units while they stand; the swarm's units circle
 *        the hero and dive at him (the robots run at him); when the hives are down and the last unit falls, it is
 *        over. In the net the night shift fights along: now and then a flash takes a few of the swarm.
 *
 *        "swarm kind cap hives": kind drones, menders, archon or delete; how many units at once; how many hives (placed
 *        by "thing r c hive", else round the level's guardian). "zaps s": the night shift's flashes every s seconds.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float SwarmWake = 18.0f;   ///< the hero this near the hives: it begins
constexpr float HiveEvery = 2.6f;    ///< seconds between a hive's new units
constexpr float DiveHurt = 7.0f;     ///< a unit diving into the hero
constexpr float Formation = 11.0f;   ///< the archon's swarm: seconds between its dives all at once

}   // namespace

void Game::loadSwarm()
{
    std::string kind = m_level.word( "swarm" );
    if ( kind.empty() ) return;
    m_swarmKind = kind.substr( 0, kind.find( ' ' ) );
    m_swarmCap = (int)m_level.num( "swarm", 1, 14.0f );
    int                  hives = (int)m_level.num( "swarm", 2, 3.0f );
    std::vector<Vector3> at;
    for ( const Thing &t : m_level.m_things )
        if ( t.m_name == "hive" ) at.push_back( t.m_pos );
    // No hives placed: round the level's guardian (who gives way to the swarm).
    if ( at.empty() )
        for ( Foe &f : m_foes )
            if ( f.m_alive && f.m_elite == 2 )
            {
                f.m_alive = false;
                f.m_hidden = true;
                for ( int i = 0; i < hives; ++i )
                {
                    float   a = i * 2.0f * PI / hives;
                    Vector3 p = Vector3Add( f.m_home, { cosf( a ) * 4.5f, 0.0f, sinf( a ) * 4.5f } );
                    if ( groundAt( p ) < -100.0f || groundAt( p ) > 100.0f ) p = f.m_home;
                    at.push_back( p );
                }
                break;
            }
    if ( at.empty() ) return;
    // Each carrier over open floor (not in a block of cover, not over a shaft): within reach of a hammer.
    for ( Vector3 &p : at )
    {
        int r0, c0, best = -1;
        m_level.cellOf( p, r0, c0 );
        for ( int d = 0; d <= 6 && best < 0; ++d )
            for ( int dr = -d; dr <= d && best < 0; ++dr )
                for ( int dc = -d; dc <= d && best < 0; ++dc )
                {
                    int r = r0 + dr, c = c0 + dc;
                    if ( std::max( abs( dr ), abs( dc ) ) != d || m_level.cell( r, c ).m_kind != CellE::FLOOR ||
                         m_level.cell( r, c ).m_height != 0.0f )
                        continue;
                    best = r * m_level.cols() + c;
                }
        if ( best >= 0 ) p = m_level.centre( best / m_level.cols(), best % m_level.cols() );
    }
    Vector3 sum = { 0.0f, 0.0f, 0.0f };
    for ( const Vector3 &p : at ) sum = Vector3Add( sum, p );
    m_swarmAt = Vector3Scale( sum, 1.0f / at.size() );
    for ( const Vector3 &p : at )
    {
        spawnFoe( FoeE::DRONE, Vector3Add( p, { 0.0f, 1.3f, 0.0f } ),
                  false );   // over the benches, low enough for a hammer
        Foe &h = m_foes.back();
        h.m_variant = 6;
        h.m_hp *= 7.0f;
        h.m_hpMax = h.m_hp;
        h.m_home = h.m_pos;
        m_hives.push_back( h.m_id );
    }
    m_swarmOn = true;
}

void Game::updateSwarm( float dt )
{
    if ( !m_swarmOn || m_swarmDone ) return;
    if ( !m_swarmAwake )
    {
        if ( Vector3Distance( m_pos, m_swarmAt ) > SwarmWake || m_dead ) return;
        m_swarmAwake = true;
        fire( "bossstart" );
        fire( "swarm" );
        for ( int id : m_hives ) m_foes[id].m_awake = true;
    }
    // The hives make new units while they stand.
    int hives = 0, units = 0;
    for ( int id : m_hives ) hives += m_foes[id].m_alive ? 1 : 0;
    for ( const Foe &f : m_foes )
        units += f.m_alive && ( ( f.m_kind == FoeE::DRONE && f.m_variant == 5 ) ||
                                ( f.m_kind == FoeE::SAW && f.m_variant == 3 ) )
                     ? 1
                     : 0;
    m_swarmT -= dt;
    if ( hives > 0 && m_swarmT <= 0.0f && units < m_swarmCap && !m_dead )
    {
        m_swarmT = HiveEvery;
        // One unit at a time, from a carrier still standing.
        int pick = (int)( rnd() * hives ), seen = 0;
        for ( int id : m_hives )
        {
            const Foe &h = m_foes[id];
            if ( !h.m_alive || seen++ != pick ) continue;
            bool    robot = m_swarmKind == "menders";
            Vector3 p = robot ? Vector3{ h.m_pos.x + ( rnd() - 0.5f ) * 2.0f, h.m_home.y - 1.3f,
                                         h.m_pos.z + ( rnd() - 0.5f ) * 2.0f }
                              : Vector3Add( h.m_pos, { ( rnd() - 0.5f ) * 1.5f, -0.5f, ( rnd() - 0.5f ) * 1.5f } );
            if ( robot && groundAt( p ) < -100.0f ) continue;
            m_summons.push_back( { robot ? FoeE::SAW : FoeE::DRONE, p, robot ? 3 : 5 } );
            ++units;
        }
    }
    // The archon's swarm: now and then all its drones at once (they flash white before).
    if ( m_swarmKind == "archon" )
    {
        m_formationT += dt;
        if ( m_formationT > Formation - 1.2f && !m_fired.count( "formation" ) ) fire( "formation" );
        if ( m_formationT > Formation )
        {
            m_formationT = 0.0f;
            m_fired.erase( "formation" );
            for ( Foe &f : m_foes )
                if ( f.m_alive && f.m_kind == FoeE::DRONE && f.m_variant == 5 )
                    f.m_state = FoeStateE::LEAP, f.m_timer = 0.9f;
        }
    }
    // The night shift fights along: a flash, and a few of the swarm are gone.
    float zaps = m_level.num( "zaps" );
    if ( zaps > 0.0f )
    {
        m_zapT -= dt;
        if ( m_zapT <= 0.0f )
        {
            m_zapT = zaps;
            int taken = 0;
            for ( Foe &f : m_foes )
            {
                if ( taken >= 3 || !f.m_alive || f.m_kind != FoeE::DRONE || f.m_variant != 5 ) continue;
                m_tracers.push_back( { Vector3Add( f.m_pos, { 0.0f, 14.0f, 0.0f } ), f.m_pos, 0.2f } );
                m_flashes.push_back( { f.m_pos, { 20.0f, 24.0f, 30.0f }, 6.0f, 0.25f, 0.25f } );
                killFoe( f, false );
                ++taken;
            }
            if ( taken ) fire( "nightshift" );
        }
    }
    // Over: the hives down and the last unit with them.
    if ( hives == 0 && units == 0 && m_summons.empty() )
    {
        m_swarmDone = true;
        m_bossAt = m_swarmAt;
        m_bossAt.y = fmaxf( 0.0f, groundAt( m_bossAt ) );
        fire( "bossdead" );
        for ( int i = 0; i < 3; ++i )
            explode( Vector3Add( m_swarmAt, { ( rnd() - 0.5f ) * 3.0f, 2.0f + i, ( rnd() - 0.5f ) * 3.0f } ), 1.6f );
        for ( int i = 0; i < 4; ++i ) dropItem( Vector3Add( m_bossAt, { 0.0f, 1.2f, 0.0f } ), 5.0f );
        m_pickups.push_back( { 0, 70, Vector3Add( m_bossAt, { 0.0f, 1.0f, 0.0f } ), { 0.5f, 3.0f, 0.5f } } );
        gainXp( (int)( 700 * ( 1.0f + 0.15f * ( areaLevel() - 1 ) ) ) );
        m_shake = 1.0f;
    }
}

bool Game::updateSwarmUnit( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    f.m_spin += dt * 50.0f;
    float floor = groundAt( f.m_pos );
    if ( floor > -100.0f ) f.m_rush.y = floor;
    if ( f.m_variant == 6 )
    {
        // A hive: it hangs where it was put, turning slowly, pulsing.
        want = Vector3Scale( Vector3Subtract( f.m_home, f.m_pos ), 2.0f );
        faceYaw = f.m_yaw + dt * 0.6f;
        return true;
    }
    // A unit of the swarm: it circles the hero, now and then dives at him, hits, pulls away.
    bool rage = bossRage();
    switch ( f.m_state )
    {
    case FoeStateE::LEAP:
    {
        f.m_timer -= dt;
        Vector3 to = Vector3Subtract( Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), f.m_pos );
        float   len = Vector3Length( to );
        want = Vector3Scale( to, ( rage ? 15.0f : 12.0f ) / fmaxf( 0.1f, len ) );
        faceYaw = atan2f( to.x, to.z );
        if ( len < 1.1f )
        {
            m_hurtBy = "swarm";
            hurtHero( DiveHurt * m_areaHit, Vector3Scale( Vector3Normalize( to ), 2.0f ), f.m_pos );
            f.m_state = FoeStateE::RECOVER, f.m_timer = 1.2f;
        }
        else if ( f.m_timer <= 0.0f )
            f.m_state = FoeStateE::RECOVER, f.m_timer = 0.8f;
        return true;
    }
    case FoeStateE::RECOVER:
        f.m_timer -= dt;
        want = Vector3Add( Vector3Scale( dir, -6.0f ), { 0.0f, 3.0f, 0.0f } );
        if ( f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE, f.m_spit = 2.0f + rnd() * 3.0f;
        return true;
    default:
    {
        float side = f.m_id % 2 ? 1.0f : -1.0f;
        float ring = 5.5f + ( f.m_id % 4 );
        if ( !f.m_los || dist > ring + 4.0f )
            want = Vector3Scale( path, 5.0f );
        else
            want = Vector3Add( Vector3Scale( dir, ( dist - ring ) * 1.5f ),
                               { -dir.z * side * 4.0f, 0.0f, dir.x * side * 4.0f } );
        float hover = f.m_rush.y + 2.4f + 0.8f * sinf( m_time * 1.7f + f.m_id );
        want.y = ( hover - f.m_pos.y ) * 3.0f;
        faceYaw = atan2f( dir.x, dir.z );
        f.m_spit -= dt * ( rage ? 1.6f : 1.0f );
        if ( f.m_spit <= 0.0f && f.m_los && dist < 14.0f ) f.m_state = FoeStateE::LEAP, f.m_timer = 1.4f;
        return true;
    }
    }
}

void Game::drawSwarmHud( int w, int, float s )
{
    if ( !m_swarmAwake || m_swarmDone ) return;
    float hp = 0.0f, max = 0.0f;
    int   units = 0;
    for ( int id : m_hives )
        hp += fmaxf( 0.0f, m_foes[id].m_alive ? m_foes[id].m_hp : 0.0f ), max += m_foes[id].m_hpMax;
    for ( const Foe &f : m_foes )
        units += f.m_alive && ( ( f.m_kind == FoeE::DRONE && f.m_variant == 5 ) ||
                                ( f.m_kind == FoeE::SAW && f.m_variant == 3 ) )
                     ? 1
                     : 0;
    std::string name = tr( "boss.swarm." + m_swarmKind );
    float       pw = 640.0f * s, px = w * 0.5f - pw * 0.5f, py = 40.0f * s;
    Vector2     m = MeasureTextEx( m_font, name.c_str(), 24 * s, 1.0f );
    DrawTextEx( m_font, name.c_str(), { w * 0.5f - m.x * 0.5f, py - 4.0f * s }, 24 * s, 1.0f, { 255, 210, 150, 240 } );
    DrawRectangle( (int)px, (int)( py + 26 * s ), (int)pw, (int)( 12 * s ), { 30, 10, 0, 200 } );
    DrawRectangleGradientH( (int)px, (int)( py + 26 * s ), (int)( pw * Clamp( hp / fmaxf( 1.0f, max ), 0.0f, 1.0f ) ),
                            (int)( 12 * s ), { 80, 140, 220, 255 }, { 180, 220, 255, 255 } );
    // The swarm itself: a thin bar of its units under it.
    DrawRectangle( (int)px, (int)( py + 42 * s ), (int)( pw * Clamp( units / (float)m_swarmCap, 0.0f, 1.0f ) ),
                   (int)( 4 * s ), { 230, 230, 240, 200 } );
}

}   // namespace spaceopera
