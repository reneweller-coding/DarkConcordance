/**
 * @file GameTug.cpp
 * @brief The machines at the edge of the deck, bosses of three weak points each: the tug on the station's skin (level
 *        3) with a cargo pod under it, the Icebreaker (Seren: a drilling rig standing in the open water, its drill
 *        striking the ice where the hero stands), the Bell (Hesper: a guard airship hovering off the platforms), the
 *        Plasma Lance (Kael: a defence platform with two coils and its lance). Each lobs strikes whose landing shows as
 *        a red ring, fires bolts and lets out drones; hurt, it gets fiercer (the tug and the Bell try to cast off).
 *        With all three weak points gone it breaks up.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float GunHp = 1400.0f;
constexpr float EngineHp = 1100.0f;
constexpr float ShellDamage = 30.0f;
constexpr float ShellRadius = 3.2f;
constexpr float ShellTime = 1.6f;   ///< from the shot to the landing (the ring's warning)

}   // namespace

void Game::loadTug()
{
    if ( m_level.m_tugRow < 0 ) return;
    // Alongside the deck's edge, a metre off it; its own +x points away from the deck.
    Vector3 edge = m_level.centre( m_level.m_tugRow, m_level.m_tugCol );
    float   out = m_level.m_tugSide == 1 ? 1.0f : -1.0f;
    m_tugHome = { edge.x + out * 5.0f, 0.0f, edge.z };
    m_tugPos = m_tugHome;
    m_tugYaw = out > 0.0f ? 0.0f : PI;
    m_tugStand = { edge.x - out * 2.0f, 0.0f, edge.z };
    m_tugOn = true;
    m_tugGun = 3.0f, m_tugPd = 2.0f, m_tugDrone = 12.0f;
    float              tough = toughness( m_difficulty ) * ( 1.0f + 0.25f * ( m_level.m_area - 1 ) );
    const std::string &kind = m_level.m_machine;
    if ( kind != "tug" )
    {
        loadMachine( kind, out, tough );
        return;
    }
    m_parts = { { "tug.engine", { -2.6f, 0.5f, -8.3f }, 1.2f, EngineHp * tough, EngineHp * tough, {} },
                { "tug.gun", { 0.0f, 3.5f, -1.0f }, 1.4f, GunHp * tough, GunHp * tough, {} },
                { "tug.engine", { 2.6f, 0.5f, -8.3f }, 1.2f, EngineHp * tough, EngineHp * tough, {} } };

    // The hull: a long rounded body, a bridge with lit windows at the bow, hazard bands, the pod slung under it.
    auto add = [&]( MeshBuilder &b, int surface )
    {
        for ( Mesh m : b.build() ) m_tugMeshes.push_back( m ), m_tugSurfaces.push_back( surface );
    };
    MeshBuilder hull;
    hull.roundedBox( place( { 0.0f, 1.0f, 0.0f } ), { 3.0f, 1.6f, 9.0f }, 0.6f, 3 );
    hull.roundedBox( place( { 0.5f, 3.4f, 5.6f } ), { 2.0f, 1.0f, 2.2f }, 0.3f, 2 );
    hull.box( place( { 0.0f, 2.7f, -3.5f } ), { 2.2f, 0.12f, 3.5f } );
    for ( float z : { -6.0f, 0.0f, 6.0f } ) hull.box( place( { -3.05f, 1.0f, z } ), { 0.08f, 1.2f, 0.25f } );
    add( hull, S_HULL );
    MeshBuilder bands;
    bands.box( place( { 0.0f, 2.62f, 8.6f } ), { 3.02f, 0.05f, 0.4f } );
    bands.box( place( { -3.02f, 0.2f, 0.0f } ), { 0.03f, 0.15f, 8.5f } );
    add( bands, S_HAZARD );
    MeshBuilder windows;
    windows.box( place( { -1.52f, 3.6f, 5.6f } ), { 0.02f, 0.25f, 1.8f } );
    windows.box( place( { 0.5f, 3.6f, 7.82f } ), { 1.7f, 0.25f, 0.02f } );
    add( windows, -1 );
    MeshBuilder pod;
    pod.box( place( { 0.0f, -2.0f, 1.5f } ), { 1.7f, 1.4f, 4.5f } );
    for ( float z : { -2.5f, 1.5f, 5.5f } ) pod.box( place( { 0.0f, -0.4f, z } ), { 0.15f, 0.3f, 0.15f } );
    m_tugPodMesh = pod.build().front();
    MeshBuilder gun;
    gun.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 1.2f, 0.7f, 18 );
    gun.roundedBox( place( { 0.0f, 0.9f, 0.0f } ), { 0.9f, 0.45f, 1.0f }, 0.2f, 2 );
    gun.box( place( { -0.25f, 1.0f, 2.2f } ), { 0.16f, 0.16f, 1.6f } );
    gun.box( place( { 0.25f, 1.0f, 2.2f } ), { 0.16f, 0.16f, 1.6f } );
    m_tugGunMesh = gun.build().front();
    MeshBuilder engine;
    engine.cylinder( place( { 0.0f, 0.0f, -1.6f }, { PI * 0.5f, 0.0f, 0.0f } ), 1.0f, 3.2f, 18 );
    engine.cylinder( place( { 0.0f, 0.0f, -1.9f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.75f, 0.3f, 18 );
    m_tugEngineMesh = engine.build().front();
    MeshBuilder nozzle;
    nozzle.cylinder( place( { 0.0f, 0.0f, -1.95f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.65f, 0.06f, 18 );
    m_tugNozzle = nozzle.build().front();
}

void Game::loadMachine( const std::string &kind, float out, float tough )
{
    auto add = [&]( MeshBuilder &b, int surface )
    {
        for ( Mesh m : b.build() ) m_tugMeshes.push_back( m ), m_tugSurfaces.push_back( surface );
    };
    Vector3     edge = m_level.centre( m_level.m_tugRow, m_level.m_tugCol );
    MeshBuilder frameB, dark, bands, lights, gun, engine, nozzle;
    if ( kind == "rig" )
    {
        // The Icebreaker: a lattice tower standing in the open water, a platform, a boom with the drill, two pumps.
        m_tugHome = { edge.x + out * 6.0f, -0.4f, edge.z };
        m_tugSlide = 0.0f, m_tugBob = 0.05f;
        m_tugGunAt = { 0.0f, 5.0f, 0.0f };
        m_tugBridge = { 0.0f, 7.5f, 0.0f };
        m_parts = { { "rig.pump", { -2.6f, 2.0f, -2.6f }, 1.3f, 1150.0f * tough, 1150.0f * tough, {} },
                    { "rig.drill", { 0.0f, 5.4f, 0.0f }, 1.6f, 1500.0f * tough, 1500.0f * tough, {} },
                    { "rig.pump", { 2.6f, 2.0f, -2.6f }, 1.3f, 1150.0f * tough, 1150.0f * tough, {} } };
        for ( float x : { -2.2f, 2.2f } )
            for ( float z : { -2.2f, 2.2f } ) frameB.box( place( { x, 4.0f, z } ), { 0.22f, 6.0f, 0.22f } );
        for ( float y : { 1.0f, 4.0f, 7.0f } )
        {
            frameB.box( place( { 0.0f, y, -2.2f } ), { 2.3f, 0.12f, 0.12f } );
            frameB.box( place( { 0.0f, y, 2.2f } ), { 2.3f, 0.12f, 0.12f } );
            frameB.box( place( { -2.2f, y, 0.0f } ), { 0.12f, 0.12f, 2.3f } );
            frameB.box( place( { 2.2f, y, 0.0f } ), { 0.12f, 0.12f, 2.3f } );
        }
        frameB.box( place( { 0.0f, 3.2f, 0.0f } ), { 2.8f, 0.15f, 2.8f } );
        add( frameB, S_RUST );
        dark.roundedBox( place( { 0.0f, 8.8f, 0.0f } ), { 1.6f, 0.9f, 1.6f }, 0.3f, 2 );
        add( dark, S_CONCESSION );
        bands.box( place( { 0.0f, 3.38f, 2.8f } ), { 2.8f, 0.04f, 0.15f } );
        add( bands, S_HAZARD );
        lights.box( place( { 0.0f, 9.75f, 0.0f } ), { 0.25f, 0.1f, 0.25f } );
        add( lights, -1 );
        // The drill: a boom out over the deck and the bit hanging from it.
        gun.box( place( { 0.0f, 0.4f, 2.5f } ), { 0.35f, 0.35f, 3.0f } );
        gun.cylinder( place( { 0.0f, -1.8f, 5.0f } ), 0.6f, 2.2f, 16 );
        gun.lathe( place( { 0.0f, -2.6f, 5.0f } ), { { 0.0f, -0.8f }, { 0.6f, 0.0f } }, 16 );
        m_tugGunMesh = gun.build().front();
        engine.cylinder( place( { 0.0f, -1.2f, 0.0f } ), 0.9f, 2.4f, 16 );
        m_tugEngineMesh = engine.build().front();
        nozzle.cylinder( place( { 0.0f, 1.25f, 0.0f } ), 0.5f, 0.1f, 16 );
        m_tugNozzle = nozzle.build().front();
    }
    else if ( kind == "bell" )
    {
        // The Bell: an airship off the platforms, an envelope over a gondola, the bell-mouthed gun under its nose,
        // engines on pylons.
        m_tugHome = { edge.x + out * 8.0f, 3.5f, edge.z };
        m_tugSlide = 9.0f, m_tugBob = 0.6f;
        m_tugGunAt = { 0.0f, -1.2f, 4.5f };
        m_tugBridge = { 0.0f, -0.6f, 0.0f };
        m_parts = { { "bell.engine", { -4.6f, 1.6f, -5.5f }, 1.3f, 1150.0f * tough, 1150.0f * tough, {} },
                    { "bell.gun", { 0.0f, -1.0f, 4.5f }, 1.5f, 1500.0f * tough, 1500.0f * tough, {} },
                    { "bell.engine", { 4.6f, 1.6f, -5.5f }, 1.3f, 1150.0f * tough, 1150.0f * tough, {} } };
        frameB.roundedBox( place( { 0.0f, 3.2f, 0.0f } ), { 3.6f, 2.8f, 10.0f }, 2.4f, 4 );
        add( frameB, S_CLOTH2 );
        dark.roundedBox( place( { 0.0f, -0.4f, 1.0f } ), { 1.6f, 0.9f, 4.5f }, 0.4f, 2 );
        for ( float x : { -1.0f, 1.0f } ) dark.box( place( { x * 1.4f, 1.0f, 1.0f } ), { 0.08f, 0.9f, 0.08f } );
        for ( float x : { -1.0f, 1.0f } ) dark.box( place( { x * 3.0f, 1.6f, -5.5f } ), { 1.4f, 0.12f, 0.3f } );
        add( dark, S_CONCESSION );
        bands.box( place( { 0.0f, 3.2f, 9.6f } ), { 2.4f, 0.25f, 0.4f } );
        add( bands, S_HAZARD );
        lights.box( place( { -1.62f, -0.3f, 1.5f } ), { 0.02f, 0.2f, 2.5f } );
        lights.box( place( { 1.62f, -0.3f, 1.5f } ), { 0.02f, 0.2f, 2.5f } );
        add( lights, -1 );
        gun.lathe( place( { 0.0f, 0.0f, 0.0f }, { PI * 0.5f, 0.0f, 0.0f } ),
                   { { 0.0f, -0.6f }, { 0.5f, -0.6f }, { 0.7f, 0.4f }, { 1.2f, 1.0f }, { 1.25f, 1.1f } }, 20 );
        m_tugGunMesh = gun.build().front();
        engine.cylinder( place( { 0.0f, 0.0f, -1.2f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.9f, 2.4f, 16 );
        m_tugEngineMesh = engine.build().front();
        nozzle.cylinder( place( { 0.0f, 0.0f, -1.25f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.6f, 0.06f, 16 );
        m_tugNozzle = nozzle.build().front();
    }
    else
    {
        // The Plasma Lance: a platform off the deck, two coils that feed it, the lance on its turret.
        m_tugHome = { edge.x + out * 6.0f, 0.0f, edge.z };
        m_tugSlide = 0.0f, m_tugBob = 0.0f;
        m_tugGunAt = { 0.0f, 3.2f, 1.0f };
        m_tugBridge = { 0.0f, 3.0f, -1.0f };
        m_parts = { { "lance.coil", { -3.0f, 3.0f, -2.5f }, 1.3f, 1150.0f * tough, 1150.0f * tough, {} },
                    { "lance.emitter", { 0.0f, 4.0f, 1.0f }, 1.5f, 1500.0f * tough, 1500.0f * tough, {} },
                    { "lance.coil", { 3.0f, 3.0f, -2.5f }, 1.3f, 1150.0f * tough, 1150.0f * tough, {} } };
        frameB.cylinder( place( { 0.0f, -1.5f, 0.0f } ), 5.0f, 1.5f, 24 );
        frameB.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 1.2f, 3.0f, 16 );
        add( frameB, S_METAL );
        bands.lathe( place( { 0.0f, 0.02f, 0.0f } ), { { 4.6f, 0.0f }, { 5.0f, 0.0f } }, 32 );
        add( bands, S_HAZARD );
        lights.lathe( place( { 0.0f, 0.03f, 0.0f } ), { { 4.2f, 0.0f }, { 4.35f, 0.0f } }, 32 );
        add( lights, -1 );
        gun.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 1.0f, 0.7f, 1.2f }, 0.3f, 2 );
        gun.cylinder( place( { 0.0f, 0.2f, 3.0f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.3f, 5.0f, 12 );
        for ( float z : { 1.5f, 2.6f, 3.7f } )
            gun.cylinder( place( { 0.0f, 0.2f, z }, { PI * 0.5f, 0.0f, 0.0f } ), 0.5f, 0.25f, 14 );
        m_tugGunMesh = gun.build().front();
        engine.cylinder( place( { 0.0f, -3.0f, 0.0f } ), 0.7f, 3.0f, 14 );
        for ( float y : { -2.4f, -1.5f, -0.6f } ) engine.cylinder( place( { 0.0f, y, 0.0f } ), 1.1f, 0.25f, 18 );
        m_tugEngineMesh = engine.build().front();
        nozzle.cylinder( place( { 0.0f, 0.1f, 0.0f } ), 0.45f, 0.4f, 14 );
        m_tugNozzle = nozzle.build().front();
    }
    m_tugPos = m_tugHome;
}

Matrix Game::tugFrame() const
{
    return MatrixMultiply( MatrixRotateY( m_tugYaw ), MatrixTranslate( m_tugPos.x, m_tugPos.y, m_tugPos.z ) );
}

void Game::hitPart( int i, float damage, Vector3 at )
{
    BossPart &p = m_parts[i];
    if ( p.m_hp <= 0.0f || !partsLive() ) return;
    p.m_hp -= damage;
    sparks( at, Vector3Normalize( Vector3Subtract( m_pos, at ) ), 8, { 255, 200, 130, 255 }, 3.0f );
    if ( p.m_hp > 0.0f ) return;
    // A weak point gone: a blast, a line, the hint for the next.
    explode( p.m_pos, 2.0f );
    m_shake = fmaxf( m_shake, 0.6f );
    fire( std::string( p.m_key ) + "down" );
    // Mother Sevenfold feels a mast go: all seven bodies stumble at once.
    if ( std::string( p.m_key ) == "mother.relay" )
        for ( Foe &f : m_foes )
            if ( f.m_kind == FoeE::SHELL && f.m_variant == 2 && f.m_alive ) f.m_stun = 1.5f, f.m_swingT = -1.0f;
}

void Game::updateTug( float dt )
{
    if ( !m_tugOn ) return;
    Matrix frame = tugFrame();
    for ( BossPart &p : m_parts ) p.m_pos = Vector3Transform( p.m_local, frame );
    // The shells in flight: each lands where its ring was.
    for ( size_t i = 0; i < m_shells.size(); )
    {
        Shell &s = m_shells[i];
        s.m_t -= dt;
        if ( s.m_t > 0.0f )
        {
            ++i;
            continue;
        }
        explode( Vector3Add( s.m_at, { 0.0f, 0.4f, 0.0f } ), 1.8f );
        m_shake = fmaxf( m_shake, 0.45f );
        float d = Vector2Distance( { m_pos.x, m_pos.z }, { s.m_at.x, s.m_at.z } );
        if ( d < ShellRadius && fabsf( m_pos.y - s.m_at.y ) < 2.5f )
        {
            Vector3 away = Vector3Normalize( { m_pos.x - s.m_at.x + 1e-3f, 0.0f, m_pos.z - s.m_at.z } );
            m_hurtBy = "shell";
            hurtHero( ShellDamage * ( 1.0f - 0.5f * d / ShellRadius ), { away.x * 5.0f, 2.0f, away.z * 5.0f }, s.m_at );
            m_hurtBy = "";
        }
        for ( Foe &f : m_foes )
            if ( f.m_alive && !f.m_hidden && Vector3Distance( f.m_pos, s.m_at ) < ShellRadius )
                hurtFoe( f, ShellDamage * 1.5f, Vector3Normalize( Vector3Subtract( f.m_pos, s.m_at ) ) );
        m_shells[i] = m_shells.back();
        m_shells.pop_back();
    }
    if ( m_tugDead )
    {
        // Breaking up: blasts along the hull, it drifts off and sinks; the pod with the bodies tumbles away.
        m_tugDying += dt;
        m_tugPos = Vector3Add( m_tugPos, Vector3Scale( { m_level.m_tugSide == 1 ? 1.5f : -1.5f, -0.6f, 0.0f }, dt ) );
        m_podPos = Vector3Add( m_podPos, Vector3Scale( m_podVel, dt ) );
        m_podSpin += dt * 0.4f;
        if ( m_tugDying < 3.0f && rnd() < dt * 4.0f )
            explode( Vector3Transform( { ( rnd() - 0.5f ) * 5.0f, 2.0f, ( rnd() - 0.5f ) * 16.0f }, frame ), 1.6f );
        return;
    }
    float d = Vector3Distance( m_pos, m_tugPos );
    if ( !m_tugAwake )
    {
        if ( d > 32.0f || m_dead ) return;
        m_tugAwake = true;
        fire( "bossstart" );
    }
    float total = 0.0f, max = 0.0f;
    for ( const BossPart &p : m_parts ) total += fmaxf( 0.0f, p.m_hp ), max += p.m_max;
    if ( total <= 0.0f )
    {
        m_tugDead = true;
        m_bossAt = m_tugStand;
        m_shells.clear();
        fire( "bossdead" );
        m_podPos = Vector3Transform( { 0.0f, -2.0f, 1.5f }, frame );
        m_podVel = { m_level.m_tugSide == 1 ? 2.5f : -2.5f, -1.2f, 0.6f };
        for ( int i = 0; i < 4; ++i ) dropItem( Vector3Add( m_tugStand, { 0.0f, 1.2f, 0.0f } ), 5.0f );
        m_pickups.push_back( { 0, 70, Vector3Add( m_tugStand, { 0.0f, 1.0f, 0.0f } ), { 0.5f, 3.0f, 0.5f } } );
        gainXp( (int)( 650 * ( 1.0f + 0.15f * ( areaLevel() - 1 ) ) ) );
        m_shake = 1.0f;
        return;
    }
    // Hurt to half, it tries to cast off: sliding along the edge with its engines lit.
    if ( m_tugStage == 0 && total < max * 0.5f ) m_tugStage = 1, fire( "tug2" );
    m_tugT += dt;
    float slide = m_tugStage ? m_tugSlide * sinf( m_tugT * 0.35f ) : 0.0f;
    m_tugPos = { m_tugHome.x, m_tugHome.y + m_tugBob * sinf( m_tugT * 0.8f ), m_tugHome.z + slide };
    if ( m_dead || m_scene > 0 ) return;
    // The deck gun: volleys of three shells at where the hero will be.
    m_tugGun -= dt;
    if ( m_parts[1].m_hp > 0.0f && m_tugGun <= 0.0f && d < 40.0f )
    {
        Vector3 at = Vector3Add(
            m_pos, { m_vel.x * 1.0f + ( rnd() - 0.5f ) * 3.0f, 0.0f, m_vel.z * 1.0f + ( rnd() - 0.5f ) * 3.0f } );
        float floor = groundAt( at );
        if ( floor > -100.0f )
        {
            at.y = floor;
            m_shells.push_back( { at, ShellTime } );
            m_flashes.push_back( { m_parts[1].m_pos, { 30.0f, 16.0f, 6.0f }, 8.0f, 0.12f, 0.12f } );
            m_audio.play( SfxE::GUNSHOT, m_parts[1].m_pos, m_camTarget, 0.8f );
        }
        ++m_tugVolley;
        m_tugGun = m_tugVolley % 3 ? 0.45f : ( m_tugStage ? 4.0f : 5.5f );
    }
    // The bridge's guns: bursts of bolts.
    m_tugPd -= dt;
    if ( m_tugPd <= 0.0f && d < 24.0f )
    {
        // Its guns cannot point down its own side: close along the hull one is safe from them (not from the shells).
        Vector3 bridge = Vector3Transform( m_tugBridge, frame );
        Vector3 local = Vector3Transform( m_pos, MatrixInvert( frame ) );
        bool    under = fabsf( local.x ) < 7.0f && fabsf( local.z ) < 10.0f;
        if ( !under && los( bridge, Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ) ) )
            for ( int k = 0; k < 3; ++k )
                fireShot( bridge, Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), 22.0f, 6.0f, 1.4f );
        m_tugPd = 3.4f;
    }
    // The hangar: a drone or two now and then, never more than two in the air (Dorra has only her bolts for them).
    m_tugDrone -= dt;
    if ( m_tugDrone <= 0.0f )
    {
        int drones = 0;
        for ( const Foe &f : m_foes ) drones += f.m_alive && f.m_kind == FoeE::DRONE ? 1 : 0;
        for ( int k = 0; k < 2 && drones + k < 2; ++k )
            m_summons.push_back( { FoeE::DRONE, Vector3Transform( { -2.0f, 4.0f, -3.0f + 3.0f * k }, frame ),
                                   m_level.m_machine == "rig" ? 1 : 0 } );
        m_tugDrone = 28.0f;
    }
}

void Game::poseTug( std::vector<Drawable> &out )
{
    if ( !m_tugOn ) return;
    Matrix frame = tugFrame();
    if ( m_tugDying < 8.0f )
    {
        for ( size_t i = 0; i < m_tugMeshes.size(); ++i )
        {
            Drawable d;
            d.m_mesh = m_tugMeshes[i];
            d.m_transform = frame;
            d.m_surface = m_tugSurfaces[i] < 0 ? S_GLOW : m_tugSurfaces[i];
            if ( m_tugSurfaces[i] < 0 )
                d.m_emissive = m_tugDead ? Vector4{ 0.1f, 0.05f, 0.02f, 0.0f } : Vector4{ 2.6f, 2.2f, 1.4f, 0.0f };
            out.push_back( d );
        }
        // The gun turns to the hero; the engines glow while they live (brighter once it casts off).
        Vector3  gunAt = m_parts[1].m_pos;
        float    aim = atan2f( m_pos.x - gunAt.x, m_pos.z - gunAt.z ) - m_tugYaw;
        Drawable g;
        g.m_mesh = m_tugGunMesh;
        g.m_surface = m_parts[1].m_hp > 0.0f ? S_METAL : S_RUST;
        g.m_transform = MatrixMultiply( MatrixMultiply( MatrixRotateY( m_parts[1].m_hp > 0.0f ? aim : 0.6f ),
                                                        MatrixTranslate( m_tugGunAt.x, m_tugGunAt.y, m_tugGunAt.z ) ),
                                        frame );
        out.push_back( g );
        for ( int e : { 0, 2 } )
        {
            const BossPart &p = m_parts[e];
            Drawable        d;
            d.m_mesh = m_tugEngineMesh;
            d.m_surface = p.m_hp > 0.0f ? S_METAL : S_RUST;
            d.m_transform = MatrixMultiply( MatrixTranslate( p.m_local.x, p.m_local.y, p.m_local.z ), frame );
            out.push_back( d );
            if ( p.m_hp <= 0.0f ) continue;
            float glow = ( m_tugStage ? 7.0f : 3.0f ) * ( 0.85f + 0.15f * sinf( m_time * 30.0f + e ) );
            d.m_mesh = m_tugNozzle;
            d.m_surface = S_GLOW;
            d.m_emissive = { 0.6f * glow, 0.9f * glow, 1.6f * glow, 0.0f };
            d.m_spotShadow = d.m_sunShadow = false;
            out.push_back( d );
        }
    }
    // The pod with the bodies: under the tug, then tumbling away.
    if ( m_tugPodMesh.vertexCount > 0 )
    {
        Drawable pod;
        pod.m_mesh = m_tugPodMesh;
        pod.m_surface = S_BOX;
        pod.m_transform =
            m_tugDead ? MatrixMultiply( MatrixMultiply( MatrixTranslate( 0.0f, 2.0f, -1.5f ),
                                                        MatrixRotateXYZ( { m_podSpin, m_podSpin * 0.3f, 0.2f } ) ),
                                        MatrixMultiply( MatrixRotateY( m_tugYaw ),
                                                        MatrixTranslate( m_podPos.x, m_podPos.y, m_podPos.z ) ) )
                      : frame;
        out.push_back( pod );
    }
    // Where a shell will land: a red ring that closes as it comes.
    for ( const Shell &s : m_shells )
    {
        float    f = s.m_t / ShellTime;
        Drawable d;
        d.m_mesh = m_ring;
        d.m_surface = S_GLOW;
        float size = ShellRadius / 0.78f * ( 0.4f + 0.6f * f );
        d.m_transform =
            MatrixMultiply( MatrixScale( size, 1.0f, size ), MatrixTranslate( s.m_at.x, s.m_at.y + 0.05f, s.m_at.z ) );
        float pulse = 0.7f + 0.3f * sinf( m_time * 20.0f );
        d.m_emissive = { 6.0f * pulse, 0.4f * pulse, 0.2f * pulse, 0.0f };
        d.m_spotShadow = d.m_sunShadow = false;
        out.push_back( d );
    }
}

void Game::drawTugHud( int w, int h, float s )
{
    if ( !m_tugOn || m_tugDead || !m_tugAwake ) return;
    auto text = [&]( const char *t, float x, float y, float size, Color c )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        DrawTextEx( m_font, t, { x - m.x * 0.5f + 2.0f * s, y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
        DrawTextEx( m_font, t, { x - m.x * 0.5f, y }, size * s, 1.0f, c );
    };
    // Its bar in three parts (engine, gun, engine) and a marker over each weak point still there.
    float pw = 640.0f * s, px = w * 0.5f - pw * 0.5f, py = 40.0f * s, gap = 10.0f * s;
    float part = ( pw - 2.0f * gap ) / 3.0f;
    text( tr( ( "boss." + m_level.m_machine ).c_str() ), w * 0.5f, py - 4.0f * s, 24, { 255, 210, 150, 240 } );
    for ( int i = 0; i < 3; ++i )
    {
        const BossPart &p = m_parts[i];
        float           bx = px + i * ( part + gap );
        DrawRectangle( (int)bx, (int)( py + 26 * s ), (int)part, (int)( 12 * s ), { 30, 10, 0, 200 } );
        DrawRectangleGradientH( (int)bx, (int)( py + 26 * s ), (int)( part * Clamp( p.m_hp / p.m_max, 0.0f, 1.0f ) ),
                                (int)( 12 * s ), { 200, 70, 20, 255 }, { 255, 170, 60, 255 } );
        text( tr( p.m_key ), bx + part * 0.5f, py + 42 * s, 16,
              p.m_hp > 0.0f ? Color{ 230, 230, 230, 220 } : Color{ 120, 120, 120, 180 } );
        if ( p.m_hp <= 0.0f ) continue;
        float   pulse = 0.6f + 0.4f * sinf( m_time * 6.0f );
        Vector2 q = GetWorldToScreenEx( Vector3Add( p.m_pos, { 0.0f, p.m_radius + 0.6f, 0.0f } ), m_cam, w, h );
        float   r = ( 16.0f + 6.0f * pulse ) * s;
        DrawPolyLinesEx( q, 4, r, 45.0f, 3.0f * s, { 255, 170, 60, (unsigned char)( 255 * pulse ) } );
    }
}

}   // namespace spaceopera
