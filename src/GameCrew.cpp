/**
 * @file GameCrew.cpp
 * @brief Dorra's hammer, shield and blast bolts; the boarders with batons and carbines and their bolts in flight; how
 *        they are drawn; and how the autopilot fights with the hammer.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "Rig.h"

#include <raymath.h>

#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float SwingTime[3] = { 0.42f, 0.42f, 0.66f };   ///< forehand, backhand, overhead (seconds)
constexpr float BatonTime = 0.55f;                        ///< a boarder's blow
constexpr float BatonReach = 2.0f;
constexpr float BatonDamage = 12.0f;
constexpr float ShotSpeed = 24.0f;   ///< a carbine's bolt (m/s): slow enough to see
constexpr float ShotHurt = 7.0f;
constexpr float BlastRadius = 2.4f;

}   // namespace

void Game::buildCrew( const std::string & )
{
    // The boarders: red-black plates and red visors; batons or carbines.
    FigureSurfaces baton = { S_BOARDER, S_DARK, S_METAL, S_GLOW };
    baton.m_visor = { 3.5f, 0.25f, 0.15f, 0.0f };
    baton.m_weapon = WeaponE::BATON;
    m_boarderBaton.build( baton );
    FigureSurfaces gun = baton;
    gun.m_weapon = WeaponE::RIFLE;
    m_boarderGun.build( gun );
    // People without a suit: three kinds of worn clothes, Ilka in her yellow jacket (nine years old), Ysolde tall in
    // her dark violet coat, Kiri in her orange coverall; for a crowd two more colours and a child.
    const int cloth[FolkCount] = {
        S_CLOTH, S_CLOTH2, S_CLOTH3, S_ILKA, S_YSOLDE, S_KIRI, S_CLOTH4, S_CLOTH5, S_CLOTH4
    };
    // Ysolde, Kiri and the crowd's extra people go bare-headed (grey, dark, red hair); the others wear hoods.
    const int hair[FolkCount] = { -1, -1, -1, -1, S_HAIR3, S_HAIR, S_HAIR2, S_HAIR, S_HAIR2 };
    for ( int i = 0; i < FolkCount; ++i )
    {
        FigureSurfaces f = { cloth[i], S_DARK, S_METAL, S_GLOW };
        f.m_suit = false;
        f.m_weapon = WeaponE::NONE;
        if ( hair[i] >= 0 ) f.m_skin = S_SKIN, f.m_hair = hair[i];
        if ( i == 3 ) f.m_scale = { 0.62f, 0.62f, 0.62f };
        if ( i == 4 ) f.m_scale = { 0.95f, 1.06f, 0.95f };
        if ( i == 7 ) f.m_scale = { 1.05f, 1.03f, 1.05f };
        if ( i == 8 ) f.m_scale = { 0.66f, 0.68f, 0.66f };
        m_folk[i].build( f );
    }
}

void Game::updateMelee( float dt, const GameInput &in, Vector2 move )
{
    // The shield comes up while the left trigger is held (not in the middle of a blow).
    m_block = Clamp( m_block + ( in.m_strafe && m_swing < 0 ? 1.0f : -1.0f ) * dt * 8.0f, 0.0f, 1.0f );

    // Who is near enough to strike: the nearest ahead, softly locked on.
    Vector3 chest = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
    Vector3 fwd = { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
    Vector3 look = Vector2Length( move ) > 0.2f ? Vector3{ move.x, 0.0f, move.y } : fwd;
    if ( Vector2Length( in.m_stick ) > 0.3f ) look = { in.m_stick.x, 0.0f, in.m_stick.y };
    m_target = findTarget( chest, Vector3Normalize( look ), 1.4f );
    bool near = m_target.m_kind >= 0 &&
                Vector2Distance( { m_target.m_point.x, m_target.m_point.z }, { m_pos.x, m_pos.z } ) < 4.5f;

    // Blows: the trigger starts one; held or pressed again, the next follows, up to the overhead blow.
    m_firing = in.m_fire;
    m_fireWas = in.m_fire;
    if ( m_swing < 0 )
    {
        if ( in.m_fire && m_block < 0.5f && m_scene == 0 )
        {
            m_swing = 0;
            m_swingT = 0.0f;
            m_comboNext = false;
            m_audio.play( SfxE::SWING, m_pos, m_camTarget, 0.6f );
        }
    }
    else
    {
        if ( in.m_fire && m_swingT > 0.3f ) m_comboNext = true;
        float before = m_swingT;
        m_swingT += dt / ( SwingTime[m_swing] * m_stats.m_interval );
        if ( before < 0.5f && m_swingT >= 0.5f ) strike( m_swing );
        if ( m_swingT >= 1.0f )
        {
            if ( m_comboNext )
            {
                m_swing = ( m_swing + 1 ) % 3;
                m_swingT = 0.0f;
                m_comboNext = false;
                m_audio.play( SfxE::SWING, m_pos, m_camTarget, 0.6f );
            }
            else
                m_swing = -1;
        }
    }

    // Facing: the one she strikes at, else where she goes (the shield keeps it).
    float faceYaw = m_yaw;
    if ( near && ( m_swing >= 0 || in.m_fire ) )
        faceYaw = atan2f( m_target.m_point.x - m_pos.x, m_target.m_point.z - m_pos.z );
    else if ( m_block < 0.5f && Vector2Length( move ) > 0.2f && ( m_swing < 0 || m_swingT < 0.3f ) )
        faceYaw = atan2f( move.x, move.y );
    float turn = ( m_swing >= 0 ? 9.0f : 11.0f ) * dt;
    m_yaw += Clamp( angleTo( m_yaw, faceYaw ), -turn, turn );
    m_aimYaw = m_yaw;
    m_aimDir = { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };

    // Blast bolts.
    m_boltTimer -= dt;
    if ( in.m_special && m_boltTimer <= 0.0f && m_scene == 0 )
    {
        // Bolzenmeisterin: harder bolts, sooner again.
        int r = rank( activeSkill( 1, 1 ) );
        throwBolt( BoltDamage * weaponBoost() * ( 1.0f + 0.15f * r ) * skillPower() );
        m_boltTimer = BoltCooldown * ( 1.0f - 0.1f * m_progress.m_weapon ) * ( 1.0f - 0.08f * r ) *
                      ( 1.0f - fminf( 0.6f, stat( StatE::COOLDOWN ) / 100.0f ) );
    }
}

void Game::strike( int swing )
{
    Vector3 fwd = { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
    bool    slam = swing == 2;
    float   reach = ( slam ? 2.3f : 2.2f ) * m_stats.m_range / 2.0f, cone = slam ? 0.9f : 1.25f,
            damage = m_stats.m_damage * ( slam ? SlamDamage / SwingDamage : 1.0f );
    Vector3 head = Vector3Add( m_pos, Vector3Scale( fwd, slam ? 1.3f : 1.1f ) );
    bool    hit = false;
    for ( Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden ) continue;
        Vector3 d = { f.m_pos.x - m_pos.x, 0.0f, f.m_pos.z - m_pos.z };
        float   dist = Vector3Length( d );
        if ( dist > reach + foeRadius( f ) || fabsf( f.m_pos.y - m_pos.y ) > 2.0f ) continue;
        if ( dist > 0.3f && acosf( Clamp( Vector3DotProduct( d, fwd ) / dist, -1.0f, 1.0f ) ) > cone ) continue;
        Vector3 dir = dist > 1e-3f ? Vector3Scale( d, 1.0f / dist ) : fwd;
        dealHit( f, damage, dir, Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ) );
        // A blow knocks back; the overhead one throws them (the Taxator stands, a heavy hardly moves).
        float knock =
            ( slam ? 5.0f : 2.8f ) * ( f.m_kind == FoeE::TAXATOR ? 0.0f : ( f.m_kind == FoeE::HEAVY ? 0.3f : 1.0f ) );
        if ( knock > 0.0f )
        {
            f.m_vel = Vector3Add( f.m_vel, { dir.x * knock, slam ? 2.2f : 0.8f, dir.z * knock } );
            f.m_ground = f.m_kind == FoeE::DRONE;
        }
        if ( human( f ) && f.m_kind != FoeE::TAXATOR )
        {
            // Staggered: the blow or the aim breaks off.
            f.m_swingT = -1.0f;
            f.m_aim = 0.0f;
            f.m_burst = 0;
            f.m_timer = 0.6f;
        }
        sparks( Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ), Vector3Negate( dir ), 10, { 255, 210, 150, 255 },
                3.0f );
        hit = true;
    }
    // Crates and drums in the swing.
    for ( int i = 0; i < (int)m_breakables.size(); ++i )
    {
        const Breakable &b = m_breakables[i];
        if ( !b.m_alive || fabsf( b.m_pos.y - m_pos.y ) > 1.5f ) continue;
        Vector3 d = { b.m_pos.x - m_pos.x, 0.0f, b.m_pos.z - m_pos.z };
        float   dist = Vector3Length( d );
        if ( dist > reach + 0.4f || ( dist > 0.3f && Vector3DotProduct( d, fwd ) / dist < cosf( cone ) ) ) continue;
        hitBreakable( i, damage, Vector3Add( b.m_pos, { 0.0f, 0.5f, 0.0f } ),
                      dist > 1e-3f ? Vector3Scale( d, 1.0f / dist ) : fwd );
        hit = true;
    }
    // The tug's weak points within reach of the deck's edge, Mother Sevenfold's relays.
    if ( partsLive() )
        for ( int i = 0; i < (int)m_parts.size(); ++i )
        {
            const BossPart &q = m_parts[i];
            if ( q.m_hp <= 0.0f ) continue;
            Vector3 d = Vector3Subtract( q.m_pos, Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ) );
            float   len = Vector3Length( d );
            if ( len - q.m_radius > reach || d.x * fwd.x + d.z * fwd.z < 0.0f ) continue;
            hitPart( i, damage, Vector3Subtract( q.m_pos, Vector3Scale( Vector3Normalize( d ), q.m_radius ) ) );
            hit = true;
        }
    // The press: its tanks stand on the floor within reach; its core hangs high (blast bolts, or from the anvil).
    Vector3 o = m_pressModel.m_origin;
    if ( o.x != 0.0f && m_pressActive && !m_pressDead && !m_pressDying )
    {
        for ( int side = 0; side < 2; ++side )
        {
            Vector3 d = { o.x + ( side ? 9.2f : -9.2f ) - m_pos.x, 0.0f, o.z + 0.5f - m_pos.z };
            float   len = Vector3Length( d );
            if ( len - 1.15f > reach || len < 1e-3f || Vector3DotProduct( d, fwd ) / len < 0.3f ) continue;
            Vector3 n = Vector3Scale( d, -1.0f / len );
            hitTank( side, damage,
                     Vector3Add( Vector3Add( m_pos, Vector3Scale( d, ( len - 1.15f ) / len ) ), { 0.0f, 1.0f, 0.0f } ),
                     n );
            hit = true;
        }
        Vector3 core = { o.x, 3.4f, o.z - 0.4f };
        if ( Vector3Distance( core, Vector3Add( m_pos, { 0.0f, 1.6f, 0.0f } ) ) < reach + 0.9f )
        {
            hitCore( damage, core, Vector3Negate( fwd ) );
            hit = true;
        }
    }
    if ( hit )
    {
        m_audio.play( SfxE::SMASH, head, m_camTarget, 0.9f );
        m_shake = fmaxf( m_shake, slam ? 0.35f : 0.15f );
        // The bolt breaker blasts with every blow that lands.
        if ( m_stats.m_splash > 0.0f )
            blast( Vector3Add( head, { 0.0f, 0.5f, 0.0f } ), damage * 0.4f, m_stats.m_splash );
    }
    if ( slam )
    {
        // The bolt in the head fires as it lands: a ring of dust and sparks, a short flash.
        Vector3 floor = { head.x, groundAt( head ) + 0.05f, head.z };
        if ( floor.y < -100.0f ) floor.y = m_pos.y;
        for ( int i = 0; i < 24; ++i )
        {
            float a = i * PI * 2.0f / 24.0f;
            m_particles.push_back( { floor,
                                     { cosf( a ) * 3.0f, 0.4f + rnd() * 0.6f, sinf( a ) * 3.0f },
                                     0.5f,
                                     0.5f,
                                     0.25f,
                                     { 190, 170, 150, 18 },
                                     1 } );
        }
        sparks( floor, { 0.0f, 1.0f, 0.0f }, 16, { 255, 180, 90, 255 }, 3.5f );
        m_flashes.push_back(
            { Vector3Add( floor, { 0.0f, 0.4f, 0.0f } ), { 20.0f, 10.0f, 3.0f }, 5.0f, 0.15f, 0.15f } );
        m_audio.play( SfxE::POP, floor, m_camTarget, 0.8f );
        for ( Foe &f : m_foes )
        {
            if ( !f.m_alive || f.m_hidden ) continue;
            Vector3 d = { f.m_pos.x - floor.x, 0.0f, f.m_pos.z - floor.z };
            float   dist = Vector3Length( d );
            if ( dist > 1.6f ) continue;
            hurtFoe( f, 15.0f, dist > 1e-3f ? Vector3Scale( d, 1.0f / dist ) : fwd );
        }
    }
}

void Game::throwBolt( float damage, float radius )
{
    // Lobbed at what the auto-aim picks, else a few metres ahead.
    Vector3 from = Vector3Add( m_pos, { 0.0f, 1.7f, 0.0f } );
    Vector3 fwd = { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
    Target  t = findTarget( from, fwd, AutoCone );
    Vector3 to =
        t.m_kind >= 0 ? targetPoint( t ) : Vector3Add( m_pos, Vector3Add( Vector3Scale( fwd, 8.0f ), { 0, 0.3f, 0 } ) );
    if ( t.m_kind == 1 || t.m_kind == 2 ) to.y = 1.5f;
    Vector3 d = Vector3Subtract( to, from );
    float   flat = sqrtf( d.x * d.x + d.z * d.z );
    float   time = Clamp( flat / 12.0f, 0.35f, 1.1f );
    float   g = m_level.m_gravity;
    Vector3 v =
        turnYaw( { d.x / time, ( d.y + 0.5f * g * time * time ) / time, d.z / time }, m_level.m_coriolis * time );
    m_bolts.push_back( { from, v, time + 0.6f, damage, radius } );
    m_audio.play( SfxE::THROW, from, m_camTarget, 0.7f );
}

void Game::blast( Vector3 p, float damage, float radius )
{
    if ( damage <= 0.0f ) damage = BoltDamage * weaponBoost();
    if ( radius <= 0.0f ) radius = BlastRadius;
    radius *= areaScale();
    explode( p, 1.1f );
    m_shake = fmaxf( m_shake, 0.4f );
    for ( Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden ) continue;
        Vector3 d = Vector3Subtract( Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ), p );
        float   dist = Vector3Length( d );
        if ( dist > radius + foeRadius( f ) ) continue;
        Vector3 dir = dist > 1e-3f ? Vector3Scale( d, 1.0f / dist ) : Vector3{ 0, 1, 0 };
        dealHit( f, damage * ( 1.0f - 0.5f * fminf( 1.0f, dist / radius ) ), dir, f.m_pos );
        if ( f.m_kind == FoeE::TAXATOR || f.m_kind == FoeE::DRONE ) continue;
        f.m_vel = Vector3Add( f.m_vel, { dir.x * 3.0f, 1.5f, dir.z * 3.0f } );
        f.m_ground = false;
    }
    for ( int i = 0; i < (int)m_racks.size(); ++i )
        if ( m_racks[i].m_alive &&
             Vector2Distance( { p.x, p.z }, { m_racks[i].m_pos.x, m_racks[i].m_pos.z } ) < radius * 0.8f )
            breakRack( i );
    breakablesInRadius( p, radius, damage );
    if ( partsLive() )
        for ( int i = 0; i < (int)m_parts.size(); ++i )
        {
            float d = Vector3Distance( p, m_parts[i].m_pos ) - m_parts[i].m_radius;
            if ( m_parts[i].m_hp > 0.0f && d < radius )
                hitPart( i, damage * 2.0f * ( 1.0f - 0.5f * fmaxf( 0.0f, d ) / radius ), p );   // charges on plating
        }
    // The press's parts take more: it is what the bolts are for.
    Vector3 o = m_pressModel.m_origin;
    if ( o.x != 0.0f && m_pressActive && !m_pressDead && !m_pressDying )
    {
        for ( int side = 0; side < 2; ++side )
        {
            float flat = Vector2Distance( { p.x, p.z }, { o.x + ( side ? 9.2f : -9.2f ), o.z + 0.5f } ) - 1.15f;
            if ( flat < radius && p.y < 5.2f ) hitTank( side, damage * 1.3f, p, { 0, 1, 0 } );
        }
        if ( Vector3Distance( p, { o.x, 3.4f, o.z - 0.4f } ) < radius + 0.4f ) hitCore( damage * 1.3f, p, { 0, 0, 1 } );
    }
}

void Game::updateBoarder( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    bool facing = ( sinf( f.m_yaw ) * dir.x + cosf( f.m_yaw ) * dir.z ) > 0.6f;
    if ( f.m_kind == FoeE::BATON )
    {
        if ( f.m_swingT >= 0.0f )
        {
            // A blow under way: it tracks the hero until it falls.
            float before = f.m_swingT;
            f.m_swingT += dt / BatonTime;
            if ( f.m_swingT < 0.4f ) faceYaw = atan2f( dir.x, dir.z );
            if ( before < 0.5f && f.m_swingT >= 0.5f && dist < BatonReach + 0.3f && facing &&
                 fabsf( m_pos.y - f.m_pos.y ) < 1.5f )
            {
                bool blocked = shielded( f.m_pos );
                hurtHero( BatonDamage, { dir.x * 2.0f, 0.3f, dir.z * 2.0f }, f.m_pos );
                sparks( Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), Vector3Negate( dir ), 8, { 150, 200, 255, 255 },
                        2.5f );
                if ( blocked )
                {
                    // The shield throws it back.
                    f.m_vel = Vector3Add( f.m_vel, Vector3Scale( dir, -3.5f ) );
                    f.m_swingT = -1.0f;
                    f.m_timer = 0.9f;
                }
            }
            if ( f.m_swingT >= 1.0f )
            {
                f.m_swingT = -1.0f;
                f.m_timer = 0.5f + rnd() * 0.4f;
            }
            return;
        }
        want = Vector3Scale( path, f.m_timer > 0.0f ? 1.2f : 3.6f );
        faceYaw = dist < 3.0f ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
        if ( dist < 1.9f && f.m_los && f.m_timer <= 0.0f && fabsf( m_pos.y - f.m_pos.y ) < 1.2f )
        {
            f.m_swingT = 0.0f;
            m_audio.play( SfxE::SWING, f.m_pos, m_camTarget, 0.4f );
        }
        return;
    }

    // The carbine: keep a distance, take aim (a red laser), fire a burst of three.
    f.m_spit -= dt;
    if ( f.m_burst > 0 )
    {
        faceYaw = atan2f( dir.x, dir.z );
        if ( f.m_timer <= 0.0f )
        {
            Vector3 muzzle = Vector3Add( f.m_pos, { sinf( f.m_yaw ) * 0.7f, 1.35f, cosf( f.m_yaw ) * 0.7f } );
            Vector3 aim = Vector3Add(
                m_pos, { m_vel.x * 0.15f + ( rnd() - 0.5f ) * 0.5f, 1.2f, m_vel.z * 0.15f + ( rnd() - 0.5f ) * 0.5f } );
            Vector3 v = Vector3Scale( Vector3Normalize( Vector3Subtract( aim, muzzle ) ), ShotSpeed );
            v = turnYaw( v, m_level.m_coriolis * Vector3Distance( aim, muzzle ) / ShotSpeed );
            m_shotsInFlight.push_back( { muzzle, v, 1.6f, ShotHurt * m_hitScale } );
            m_flashes.push_back( { muzzle, { 16.0f, 5.0f, 1.5f }, 3.5f, 0.06f, 0.06f } );
            m_audio.play( SfxE::ZAP, muzzle, m_camTarget, 0.6f );
            --f.m_burst;
            f.m_timer = 0.14f;
            if ( f.m_burst == 0 )
            {
                f.m_aim = 0.0f;
                f.m_spit = 2.2f + rnd() * 1.2f;
            }
        }
        return;
    }
    if ( f.m_aim > 0.0f )
    {
        f.m_aim += dt / 0.7f;
        faceYaw = atan2f( dir.x, dir.z );
        if ( !f.m_los ) f.m_aim = 0.0f;
        if ( f.m_aim >= 1.0f )
        {
            f.m_burst = 3;
            f.m_timer = 0.0f;
        }
        return;
    }
    float side = ( ( f.m_id + (int)( m_time / 3.0f ) ) % 2 ) ? 1.0f : -1.0f;
    if ( !f.m_los || dist > 11.0f )
        want = Vector3Scale( path, 2.6f );
    else if ( dist < 5.0f )
        want = Vector3Scale( dir, -2.2f );
    else
        want = { -dir.z * side * 1.4f, 0.0f, dir.x * side * 1.4f };
    faceYaw = f.m_los ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
    if ( f.m_spit <= 0.0f && f.m_los && dist < 16.0f && f.m_timer <= 0.0f ) f.m_aim = 0.01f;
}

void Game::updateShots( float dt )
{
    float g = m_level.m_gravity;
    // Dorra's blast bolts: they burst on whatever they meet.
    for ( size_t i = 0; i < m_bolts.size(); )
    {
        Bolt   &b = m_bolts[i];
        Vector3 prev = b.m_pos;
        b.m_vel.y -= g * dt;
        b.m_vel = Vector3Add( b.m_vel, Vector3Scale( drift( b.m_pos, b.m_vel ), dt ) );
        b.m_pos = Vector3Add( b.m_pos, Vector3Scale( b.m_vel, dt ) );
        b.m_life -= dt;
        Vector3 hit, n;
        bool    done = b.m_life <= 0.0f;
        if ( !done && m_level.raycast( prev, b.m_pos, hit, n ) ) b.m_pos = hit, done = true;
        for ( const Foe &f : m_foes )
            if ( !done && f.m_alive && !f.m_hidden &&
                 Vector3Distance( b.m_pos, Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ) ) <
                     foeRadius( f ) + 0.25f )
                done = true;
        if ( partsLive() )
            for ( const BossPart &q : m_parts )
                if ( !done && q.m_hp > 0.0f && Vector3Distance( b.m_pos, q.m_pos ) < q.m_radius + 0.3f ) done = true;
        Vector3 o = m_pressModel.m_origin;
        if ( !done && o.x != 0.0f && m_pressActive && !m_pressDead )
        {
            for ( int side = 0; side < 2; ++side )
                if ( m_tank[side] > 0.0f && b.m_pos.y < 4.7f &&
                     Vector2Distance( { b.m_pos.x, b.m_pos.z }, { o.x + ( side ? 9.2f : -9.2f ), o.z + 0.5f } ) < 1.3f )
                    done = true;
            if ( m_pressPose.m_shutter > 0.85f && Vector3Distance( b.m_pos, { o.x, 3.4f, o.z - 1.0f } ) < 1.1f )
                done = true;
            if ( b.m_pos.z < o.z + 0.1f && fabsf( b.m_pos.x - o.x ) < 8.0f && b.m_pos.y < 6.5f ) done = true;
        }
        if ( rnd() < dt * 40.0f )
            m_particles.push_back( { b.m_pos, { 0.0f, 0.3f, 0.0f }, 0.3f, 0.3f, 0.025f, { 255, 160, 60, 255 }, 3 } );
        if ( done )
        {
            blast( b.m_pos, b.m_damage, b.m_radius );
            m_bolts[i] = m_bolts.back();
            m_bolts.pop_back();
        }
        else
            ++i;
    }
    // The carbines' bolts: the hero (or her shield), or a wall.
    for ( size_t i = 0; i < m_shotsInFlight.size(); )
    {
        Shot   &s = m_shotsInFlight[i];
        Vector3 prev = s.m_pos;
        s.m_vel = Vector3Add( s.m_vel, Vector3Scale( drift( s.m_pos, s.m_vel ), dt ) );
        s.m_pos = Vector3Add( s.m_pos, Vector3Scale( s.m_vel, dt ) );
        s.m_life -= dt;
        Vector3 hit, n;
        bool    done = s.m_life <= 0.0f;
        float   flat = Vector2Distance( { s.m_pos.x, s.m_pos.z }, { m_pos.x, m_pos.z } );
        if ( !done && !m_dead && flat < 0.45f && s.m_pos.y > m_pos.y + 0.2f && s.m_pos.y < m_pos.y + 1.9f )
        {
            m_hurtBy = "bolt";
            if ( s.m_kind != 4 )
                hurtHero( s.m_hurt > 0.0f ? s.m_hurt : ShotHurt, Vector3Scale( Vector3Normalize( s.m_vel ), 0.6f ),
                          Vector3Subtract( s.m_pos, s.m_vel ) );
            shotEffect( s );
            done = true;
        }
        if ( !done && m_level.raycast( prev, s.m_pos, hit, n ) )
        {
            sparks( hit, n, 6, { 255, 140, 80, 255 }, 2.0f );
            if ( s.m_kind == 4 )
            {
                s.m_pos = hit;
                shotEffect( s );
            }
            done = true;
        }
        if ( done )
        {
            m_shotsInFlight[i] = m_shotsInFlight.back();
            m_shotsInFlight.pop_back();
        }
        else
            ++i;
    }
}

void Game::burst( const Foe &f )
{
    // The suit as it stood, each plate its own piece: flung out from the chest, away from the hero, up.
    std::vector<Drawable> parts;
    FigurePose            p;
    p.m_pos = f.m_pos;
    p.m_yaw = f.m_yaw;
    p.m_phase = f.m_phase;
    p.m_time = m_time + f.m_id;
    foeFigure( f ).pose( p, { 0.25f, 0.02f, 0.01f, 0.0f }, parts );
    Vector3 chest = Vector3Add( f.m_pos, { 0.0f, foeTall( f ) * 0.6f, 0.0f } );
    Vector3 away = { f.m_pos.x - m_pos.x, 0.0f, f.m_pos.z - m_pos.z };
    away = Vector3Length( away ) > 1e-3f ? Vector3Normalize( away ) : Vector3{ 0.0f, 0.0f, 1.0f };
    for ( const Drawable &d : parts )
    {
        Piece q;
        q.m_mesh = d.m_mesh;
        q.m_surface = d.m_surface;
        q.m_emissive = d.m_emissive;
        q.m_turn = d.m_transform;
        q.m_turn.m12 = q.m_turn.m13 = q.m_turn.m14 = 0.0f;
        q.m_pos = { d.m_transform.m12, d.m_transform.m13, d.m_transform.m14 };
        Vector3 out = Vector3Subtract( q.m_pos, chest );
        out = Vector3Length( out ) > 0.05f ? Vector3Normalize( out )
                                           : Vector3Normalize( { rnd() - 0.5f, 0.3f, rnd() - 0.5f } );
        q.m_vel = Vector3Add(
            Vector3Add( Vector3Scale( out, 2.0f + rnd() * 3.0f ), Vector3Scale( away, 1.5f + rnd() * 2.5f ) ),
            { 0.0f, 2.0f + rnd() * 3.5f, 0.0f } );
        q.m_axis = Vector3Normalize( { rnd() - 0.5f, rnd() - 0.5f, rnd() - 0.5f } );
        q.m_angle = 0.0f;
        q.m_spin = ( rnd() - 0.5f ) * 26.0f;
        q.m_life = 8.0f + rnd() * 3.0f;
        m_pieces.push_back( q );
    }
    // The power cell: a blue-white flash, sparks, then smoke; a crackle under the bang.
    sparks( chest, { 0.0f, 1.0f, 0.0f }, 70, { 170, 215, 255, 255 }, 7.0f );
    sparks( chest, { 0.0f, 1.0f, 0.0f }, 30, { 255, 170, 80, 255 }, 5.0f );
    m_particles.push_back( { chest, { 0, 0, 0 }, 0.15f, 0.15f, 0.45f, { 170, 205, 255, 200 }, 2 } );
    for ( int i = 0; i < 10; ++i )
    {
        float life = 1.8f + rnd() * 1.5f;
        m_particles.push_back(
            { Vector3Add( chest, { ( rnd() - 0.5f ) * 0.5f, ( rnd() - 0.5f ) * 0.5f, ( rnd() - 0.5f ) * 0.5f } ),
              { ( rnd() - 0.5f ) * 1.2f, 0.6f + rnd() * 0.8f, ( rnd() - 0.5f ) * 1.2f },
              life,
              life,
              0.35f + rnd() * 0.3f,
              { 70, 72, 78, 170 },
              1 } );
    }
    m_flashes.push_back( { chest, { 12.0f, 18.0f, 30.0f }, 6.0f, 0.3f, 0.3f } );
    m_audio.play( SfxE::POP, chest, m_camTarget, 0.9f );
    m_audio.play( SfxE::CRACKLE, chest, m_camTarget, 1.0f );
    m_shake = fmaxf( m_shake, 0.2f );
}

void Game::updatePieces( float dt )
{
    float g = m_level.m_gravity;
    for ( size_t i = 0; i < m_pieces.size(); )
    {
        Piece &q = m_pieces[i];
        q.m_life -= dt;
        q.m_vel = Vector3Add( q.m_vel, Vector3Scale( thrust(), dt ) );
        q.m_vel.y -= g * dt;
        q.m_pos = Vector3Add( q.m_pos, Vector3Scale( q.m_vel, dt ) );
        q.m_angle += q.m_spin * dt;
        // Off the walls; bouncing on the floor, slower each time; at the end they sink away.
        Vector3 before = q.m_pos;
        if ( m_level.collide( q.m_pos, 0.12f, 0.3f ) )
        {
            if ( fabsf( q.m_pos.x - before.x ) > 1e-4f ) q.m_vel.x *= -0.3f;
            if ( fabsf( q.m_pos.z - before.z ) > 1e-4f ) q.m_vel.z *= -0.3f;
        }
        float floor = groundAt( q.m_pos ) + 0.06f;
        if ( q.m_life < 1.5f ) floor -= ( 1.5f - q.m_life ) * 0.4f;
        if ( q.m_pos.y < floor && floor > -100.0f )
        {
            q.m_pos.y = floor;
            q.m_vel = { q.m_vel.x * 0.55f, fmaxf( 0.0f, -q.m_vel.y * 0.3f ), q.m_vel.z * 0.55f };
            q.m_spin *= 0.55f;
        }
        if ( q.m_life <= 0.0f || q.m_pos.y < -20.0f )
        {
            m_pieces[i] = m_pieces.back();
            m_pieces.pop_back();
        }
        else
            ++i;
    }
}

void Game::poseCrew( std::vector<Drawable> &out )
{
    for ( const Piece &q : m_pieces )
    {
        Drawable d;
        d.m_mesh = q.m_mesh;
        d.m_surface = q.m_surface;
        d.m_emissive = q.m_emissive;
        d.m_transform = MatrixMultiply( MatrixMultiply( q.m_turn, MatrixRotate( q.m_axis, q.m_angle ) ),
                                        MatrixTranslate( q.m_pos.x, q.m_pos.y, q.m_pos.z ) );
        d.m_spotShadow = false;
        out.push_back( d );
    }
    for ( const Foe &f : m_foes )
    {
        if ( f.m_hidden || !human( f ) || !f.m_alive ) continue;
        FigurePose p;
        p.m_pos = f.m_pos;
        p.m_yaw = f.m_yaw;
        p.m_vel = f.m_alive ? Vector2{ f.m_vel.x, f.m_vel.z } : Vector2{ 0.0f, 0.0f };
        p.m_phase = f.m_phase;
        p.m_time = m_time + f.m_id;
        p.m_air = !f.m_ground && f.m_alive;
        p.m_down = f.m_down;
        p.m_recoil = f.m_burst > 0 ? 1.0f - f.m_timer / 0.14f : 0.0f;
        if ( f.m_swingT >= 0.0f )
        {
            p.m_swing = 0;
            p.m_swingT = f.m_swingT;
        }
        else if ( f.m_kind == FoeE::SHIELD )
            p.m_block = 1.0f;   // the shield up while it walks
        if ( f.m_kind == FoeE::HEAVY || f.m_kind == FoeE::TAXATOR )
            p.m_recoil = f.m_burst > 0 && f.m_timer > 0.0f ? 0.6f : 0.0f;
        // The visor: red (a guard's white, a champion's gold), white when hit; the Taxator's burns before a charge.
        Vector4 visor = { 3.5f, 0.25f, 0.15f, 0.0f };
        if ( f.m_kind == FoeE::SHIELD ) visor = { 2.4f, 2.6f, 3.0f, 0.0f };
        if ( f.m_variant ) visor = { 0.4f, 1.8f, 4.2f, 0.0f };   // the shock gunner, the Hydrauliker
        if ( f.m_kind == FoeE::TAXATOR ) visor = { 3.8f, 2.2f, 0.3f, 0.0f };
        if ( f.m_elite ) visor = { 4.5f, 3.2f, 0.6f, 0.0f };
        if ( f.m_kind == FoeE::TAXATOR && f.m_state == FoeStateE::LEAP ) visor = { 9.0f, 0.4f, 0.2f, 0.0f };
        visor = Vector4Add( visor, { 4.0f * f.m_flash, 4.0f * f.m_flash, 4.0f * f.m_flash, 0.0f } );
        foeFigure( f ).pose( p, visor, out );
    }
}

bool Game::autopilotMelee( GameInput &in, const Target &best, float bestDist )
{
    // Dorra closes in on what is nearest and strikes; blast bolts for the core and for shooters far off.
    (void)bestDist;
    if ( best.m_kind < 0 ) return false;
    Vector3 p = targetPoint( best );
    Vector2 d = { p.x - m_pos.x, p.z - m_pos.z };
    float   dist = Vector2Length( d );
    if ( dist < 1e-3f ) return false;
    in.m_stick = Vector2Scale( d, 1.0f / dist );
    FoeE kind = best.m_kind == 0 ? m_foes[best.m_index].m_kind : FoeE::LOUSE;
    bool shooter = best.m_kind == 0 && ( kind == FoeE::GUNNER || kind == FoeE::HEAVY || kind == FoeE::DRONE );
    bool tank = best.m_kind == 1 || best.m_kind == 2;
    if ( m_boltTimer <= 0.0f &&
         ( best.m_kind == 3 || best.m_kind == 5 || ( shooter && dist > 5.0f ) || ( tank && dist > 4.0f ) ) )
        in.m_special = true;
    if ( best.m_kind == 5 )
    {
        // A part of the tug: strike it where it is in reach, else bolts while the path leads to the edge.
        in.m_fire = dist < 3.4f;
        return dist < 3.4f;
    }
    if ( best.m_kind == 3 || dist > 9.0f ) return false;
    bool aimedAt = false;
    for ( const Foe &f : m_foes )
        if ( f.m_alive && !f.m_hidden && ( f.m_kind == FoeE::GUNNER || f.m_kind == FoeE::HEAVY ) && f.m_aim > 0.3f )
            aimedAt = true;
    float reach = best.m_kind == 0 ? 1.6f : 2.9f;
    // Never straight across a shaft, nor into a stall or a block in the way: then the path decides (and a bolt flies).
    for ( float step : { 0.8f, 1.6f, 2.4f } )
        if ( step < dist &&
             groundAt( { m_pos.x + in.m_stick.x * step, 0.0f, m_pos.z + in.m_stick.y * step } ) < -100.0f )
            return false;
    Vector3 hit, n;
    if ( dist > reach &&
         m_level.raycast( Vector3Add( m_pos, { 0.0f, 0.6f, 0.0f } ), { p.x, m_pos.y + 0.6f, p.z }, hit, n ) )
        return false;
    if ( dist > reach )
    {
        in.m_move = in.m_stick;
        in.m_strafe = aimedAt && dist > 4.0f;
    }
    in.m_fire = !in.m_strafe && dist < reach + 0.8f;
    return true;
}

}   // namespace spaceopera
