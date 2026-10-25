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
    // her dark violet coat, Kiri in her orange coverall.
    const int cloth[6] = { S_CLOTH, S_CLOTH2, S_CLOTH3, S_ILKA, S_YSOLDE, S_KIRI };
    for ( int i = 0; i < 6; ++i )
    {
        FigureSurfaces f = { cloth[i], S_DARK, S_METAL, S_GLOW };
        f.m_suit = false;
        f.m_weapon = WeaponE::NONE;
        if ( i == 3 ) f.m_scale = { 0.62f, 0.62f, 0.62f };
        if ( i == 4 ) f.m_scale = { 0.95f, 1.06f, 0.95f };
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
        m_swingT += dt / SwingTime[m_swing];
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
        throwBolt();
        m_boltTimer = BoltCooldown * ( 1.0f - 0.1f * m_progress.m_weapon );
    }
}

void Game::strike( int swing )
{
    Vector3 fwd = { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
    bool    slam = swing == 2;
    float   reach = slam ? 2.3f : 2.2f, cone = slam ? 0.9f : 1.25f,
            damage = ( slam ? SlamDamage : SwingDamage ) * weaponBoost();
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
        hurtFoe( f, damage, dir );
        // A blow knocks back; the overhead one throws them.
        float knock = slam ? 5.0f : 2.8f;
        f.m_vel = Vector3Add( f.m_vel, { dir.x * knock, slam ? 2.2f : 0.8f, dir.z * knock } );
        f.m_ground = false;
        if ( human( f ) )
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

void Game::throwBolt()
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
    m_bolts.push_back( { from, { d.x / time, ( d.y + 0.5f * g * time * time ) / time, d.z / time }, time + 0.6f } );
    m_audio.play( SfxE::THROW, from, m_camTarget, 0.7f );
}

void Game::blast( Vector3 p )
{
    explode( p, 1.1f );
    m_shake = fmaxf( m_shake, 0.4f );
    for ( Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden ) continue;
        Vector3 d = Vector3Subtract( Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ), p );
        float   dist = Vector3Length( d );
        if ( dist > BlastRadius + foeRadius( f ) ) continue;
        Vector3 dir = dist > 1e-3f ? Vector3Scale( d, 1.0f / dist ) : Vector3{ 0, 1, 0 };
        hurtFoe( f, BoltDamage * weaponBoost() * ( 1.0f - 0.5f * fminf( 1.0f, dist / BlastRadius ) ), dir );
        f.m_vel = Vector3Add( f.m_vel, { dir.x * 3.0f, 1.5f, dir.z * 3.0f } );
        f.m_ground = false;
    }
    // The press's parts take more: it is what the bolts are for.
    Vector3 o = m_pressModel.m_origin;
    if ( o.x != 0.0f && m_pressActive && !m_pressDead && !m_pressDying )
    {
        for ( int side = 0; side < 2; ++side )
        {
            float flat = Vector2Distance( { p.x, p.z }, { o.x + ( side ? 9.2f : -9.2f ), o.z + 0.5f } ) - 1.15f;
            if ( flat < BlastRadius && p.y < 5.2f ) hitTank( side, BoltDamage * 1.3f * weaponBoost(), p, { 0, 1, 0 } );
        }
        if ( Vector3Distance( p, { o.x, 3.4f, o.z - 0.4f } ) < BlastRadius + 0.4f )
            hitCore( BoltDamage * 1.3f * weaponBoost(), p, { 0, 0, 1 } );
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
            m_shotsInFlight.push_back( { muzzle, v, 1.6f } );
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
            blast( b.m_pos );
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
        s.m_pos = Vector3Add( s.m_pos, Vector3Scale( s.m_vel, dt ) );
        s.m_life -= dt;
        Vector3 hit, n;
        bool    done = s.m_life <= 0.0f;
        float   flat = Vector2Distance( { s.m_pos.x, s.m_pos.z }, { m_pos.x, m_pos.z } );
        if ( !done && !m_dead && flat < 0.45f && s.m_pos.y > m_pos.y + 0.2f && s.m_pos.y < m_pos.y + 1.9f )
        {
            hurtHero( ShotHurt, Vector3Scale( Vector3Normalize( s.m_vel ), 0.6f ),
                      Vector3Subtract( s.m_pos, s.m_vel ) );
            done = true;
        }
        if ( !done && m_level.raycast( prev, s.m_pos, hit, n ) )
        {
            sparks( hit, n, 6, { 255, 140, 80, 255 }, 2.0f );
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

void Game::poseCrew( std::vector<Drawable> &out )
{
    for ( const Foe &f : m_foes )
    {
        if ( f.m_hidden || !human( f ) ) continue;
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
        // The visor: red, white when hit, dark once fallen.
        Vector4 visor =
            f.m_alive ? Vector4{ 3.5f + 4.0f * f.m_flash, 0.25f + 4.0f * f.m_flash, 0.15f + 4.0f * f.m_flash, 0.0f }
                      : Vector4{ 0.25f, 0.02f, 0.01f, 0.0f };
        ( f.m_kind == FoeE::BATON ? m_boarderBaton : m_boarderGun ).pose( p, visor, out );
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
    bool shooter = best.m_kind == 0 && m_foes[best.m_index].m_kind == FoeE::GUNNER;
    bool tank = best.m_kind == 1 || best.m_kind == 2;
    if ( m_boltTimer <= 0.0f && ( best.m_kind == 3 || ( shooter && dist > 5.0f ) || ( tank && dist > 4.0f ) ) )
        in.m_special = true;
    if ( best.m_kind == 3 || dist > 9.0f ) return false;
    bool aimedAt = false;
    for ( const Foe &f : m_foes )
        if ( f.m_alive && !f.m_hidden && f.m_kind == FoeE::GUNNER && f.m_aim > 0.3f ) aimedAt = true;
    float reach = best.m_kind == 0 ? 1.6f : 2.9f;
    // Never straight across a shaft: then the path decides (and a bolt flies).
    for ( float step : { 0.8f, 1.6f, 2.4f } )
        if ( step < dist &&
             groundAt( { m_pos.x + in.m_stick.x * step, 0.0f, m_pos.z + in.m_stick.y * step } ) < -100.0f )
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
