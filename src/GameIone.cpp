/**
 * @file GameIone.cpp
 * @brief Ione Sarrath, the technician: a cloud of micro-drones round her. The trigger sends them cutting at what the
 *        aim picks (they swarm round it while the trigger is held), the left trigger couples them into a screen in
 *        front of her (it holds most of a hit, and it tires the swarm), B / F takes over a machine for a while (it turns
 *        on its own, then burns out) or jams a person's suit; her skills mend her and turn the cloud into a storm.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"
#include "Rig.h"
#include "Skills.h"
#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr int   Motes = 40;            ///< drones in the cloud
constexpr float ScreenLoad = 34.0f;    ///< the screen tires the swarm this much per second (100 = spent)
constexpr float ScreenCut = 0.2f;      ///< what of a hit comes through the screen
constexpr float HackReach = 13.0f;     ///< how far the takeover reaches
constexpr float HackCooldown = 9.0f;   ///< seconds until the next takeover
constexpr float HackTime = 8.0f;       ///< seconds a machine fights for her
constexpr float HackZap = 16.0f;       ///< what a taken machine does to its own, per shot
constexpr float HackBurn = 45.0f;      ///< what is left of it when it burns out
constexpr float StormReach = 4.2f;     ///< the storm's radius
constexpr float StormTick = 0.2f;      ///< seconds between the storm's cuts

}   // namespace

int Game::heroIndex() const
{
    switch ( m_heroKind )
    {
    case HeroE::DORRA: return 1;
    case HeroE::IONE: return 2;
    case HeroE::UNZU: return 3;
    default: return 0;
    }
}

void Game::buildIone()
{
    // A drone: a small dark wedge, and the light on its back.
    MeshBuilder b;
    b.box( MatrixIdentity(), { 0.038f, 0.011f, 0.048f } );
    b.box( place( { 0.0f, 0.0f, 0.04f } ), { 0.018f, 0.009f, 0.018f } );
    m_moteMesh = b.build().front();
    MeshBuilder l;
    l.box( place( { 0.0f, 0.012f, -0.01f } ), { 0.014f, 0.004f, 0.014f } );
    m_moteLight = l.build().front();
    m_motes.assign( Motes, Mote() );
    for ( int i = 0; i < Motes; ++i )
    {
        Mote &m = m_motes[i];
        m.m_phase = i * 2.39996f;   // the golden angle: evenly round her
        m.m_speed = 0.7f + 0.6f * fmodf( i * 0.618f, 1.0f );
        m.m_pos = Vector3Add( m_pos, { cosf( m.m_phase ) * 0.8f, 1.4f, sinf( m.m_phase ) * 0.8f } );
    }
}

bool Game::hackable( int i ) const
{
    // Machines without a person in them; never a guardian, a carrier of a swarm or what is already hers.
    const Foe &f = m_foes[i];
    if ( human( f ) || f.m_kind == FoeE::TAXATOR || f.m_kind == FoeE::SHELL || f.m_elite == 2 ) return false;
    if ( f.m_kind == FoeE::CREATURE && f.m_family != 3 && f.m_family != 11 ) return false;   // only divers, glitches
    return std::find( m_hives.begin(), m_hives.end(), i ) == m_hives.end();
}

void Game::hack()
{
    int     r = rank( activeSkill( 2, 1 ) );
    Vector3 chest = Vector3Add( m_pos, { 0.0f, 1.25f, 0.0f } );
    int     best = -1;
    float   bestScore = 1e9f;
    for ( int i = 0; i < (int)m_foes.size(); ++i )
    {
        const Foe &f = m_foes[i];
        if ( !f.m_alive || f.m_hidden || f.m_hacked > 0.0f ) continue;
        Vector3 d = Vector3Subtract( f.m_pos, m_pos );
        float   dist = sqrtf( d.x * d.x + d.z * d.z );
        if ( dist > HackReach || dist < 0.1f ) continue;
        float a = acosf( Clamp( ( d.x * m_aimDir.x + d.z * m_aimDir.z ) / dist, -1.0f, 1.0f ) );
        if ( a > 1.3f || !los( chest, Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ) ) ) continue;
        // Machines first: a person only when no machine is near the aim.
        float score = a * 2.0f + dist * 0.05f + ( hackable( i ) ? 0.0f : 1.5f );
        if ( score < bestScore ) bestScore = score, best = i;
    }
    if ( best < 0 )
    {
        m_boltTimer = 0.4f;
        return;
    }
    m_boltTimer = HackCooldown * ( 1.0f - 0.08f * r ) * ( 1.0f - fminf( 0.6f, stat( StatE::COOLDOWN ) / 100.0f ) );
    Foe    &f = m_foes[best];
    Vector3 at = Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } );
    m_tracers.push_back( { chest, at, 0.25f } );
    f.m_awake = true;
    if ( hackable( best ) )
    {
        // Into its maintenance line: it is hers for a while (two of them, and longer, with the right gear).
        bool twin = m_power[(int)PowerE::TWIN_TAKEOVER];
        f.m_hacked = ( HackTime + 1.5f * r ) * ( twin ? 1.5f : 1.0f );
        f.m_zapT = 0.4f;
        f.m_state = FoeStateE::CHASE;
        f.m_fuse = -1.0f;
        if ( twin )
            for ( int i = 0; i < (int)m_foes.size(); ++i )
            {
                Foe &o = m_foes[i];
                if ( i == best || !o.m_alive || o.m_hidden || o.m_hacked > 0.0f || !hackable( i ) ||
                     Vector3Distance( o.m_pos, f.m_pos ) > 8.0f )
                    continue;
                o.m_hacked = f.m_hacked;
                o.m_zapT = 0.6f;
                o.m_fuse = -1.0f;
                o.m_awake = true;
                m_tracers.push_back( { at, Vector3Add( o.m_pos, { 0.0f, foeHeight( o ), 0.0f } ), 0.25f } );
                break;
            }
        sparks( at, { 0.0f, 1.0f, 0.0f }, 24, { 120, 230, 255, 255 }, 3.0f );
        m_audio.play( SfxE::HACK, at, m_camTarget, 0.9f );
        fire( "hack" );
        return;
    }
    // A person (or a boss): the suit's systems jammed for a moment, the blow or the aim broken off.
    bool big = f.m_kind == FoeE::TAXATOR || f.m_elite == 2;
    f.m_stun = fmaxf( f.m_stun, ( big ? 0.6f : 1.6f ) + 0.15f * r );
    f.m_swingT = -1.0f;
    f.m_aim = 0.0f;
    f.m_burst = 0;
    sparks( at, { 0.0f, 1.0f, 0.0f }, 18, { 150, 200, 255, 255 }, 3.0f );
    m_audio.play( SfxE::ZAP, at, m_camTarget, 0.8f );
    fire( "jam" );
}

Vector3 Game::hackedWant( const Foe &f, float &faceYaw ) const
{
    // A taken machine goes for the nearest of its own and keeps a little off it.
    const Foe *near = nullptr;
    float      best = 14.0f;
    for ( const Foe &o : m_foes )
    {
        if ( &o == &f || !o.m_alive || o.m_hidden || o.m_hacked > 0.0f ) continue;
        float d = Vector3Distance( o.m_pos, f.m_pos );
        if ( d < best ) best = d, near = &o;
    }
    if ( !near ) return { 0.0f, 0.0f, 0.0f };
    Vector3 d = Vector3Subtract( near->m_pos, f.m_pos );
    d.y = 0.0f;
    float len = Vector3Length( d );
    if ( len < 1e-3f ) return { 0.0f, 0.0f, 0.0f };
    faceYaw = atan2f( d.x, d.z );
    return len > 4.0f ? Vector3Scale( d, 2.6f / len ) : Vector3{ 0.0f, 0.0f, 0.0f };
}

void Game::updateHacked( float dt )
{
    int   r = rank( activeSkill( 2, 1 ) );
    float power = skillPower();
    for ( int i = 0; i < (int)m_foes.size(); ++i )
    {
        if ( !m_foes[i].m_alive || m_foes[i].m_hacked <= 0.0f ) continue;
        m_foes[i].m_hacked -= dt;
        m_foes[i].m_zapT -= dt;
        Vector3 at = Vector3Add( m_foes[i].m_pos, { 0.0f, foeHeight( m_foes[i] ) + 0.2f, 0.0f } );
        if ( rnd() < dt * 12.0f )
            m_particles.push_back(
                { Vector3Add( at, { ( rnd() - 0.5f ) * 0.8f, ( rnd() - 0.5f ) * 0.6f, ( rnd() - 0.5f ) * 0.8f } ),
                  { 0.0f, 0.4f, 0.0f },
                  0.5f,
                  0.5f,
                  0.015f,
                  { 120, 230, 255, 255 },
                  3 } );
        if ( m_foes[i].m_zapT <= 0.0f )
        {
            // It turns its weapon on its own: the nearest in sight.
            m_foes[i].m_zapT = 0.7f;
            int   target = -1;
            float best = 10.0f;
            for ( int k = 0; k < (int)m_foes.size(); ++k )
            {
                const Foe &o = m_foes[k];
                if ( k == i || !o.m_alive || o.m_hidden || o.m_hacked > 0.0f ) continue;
                float d = Vector3Distance( o.m_pos, m_foes[i].m_pos );
                if ( d < best && los( at, Vector3Add( o.m_pos, { 0.0f, foeHeight( o ), 0.0f } ) ) )
                    best = d, target = k;
            }
            if ( target >= 0 )
            {
                Vector3 to = Vector3Add( m_foes[target].m_pos, { 0.0f, foeHeight( m_foes[target] ), 0.0f } );
                Vector3 dir = Vector3Normalize( Vector3Subtract( to, at ) );
                m_tracers.push_back( { at, to, 0.12f } );
                sparks( to, Vector3Negate( dir ), 8, { 120, 230, 255, 255 }, 2.5f );
                m_audio.play( SfxE::ZAP, at, m_camTarget, 0.5f );
                hurtFoe( m_foes[target], HackZap * weaponBoost() * power * ( 1.0f + 0.15f * r ), dir );
            }
        }
        if ( m_foes[i].m_alive && m_foes[i].m_hacked <= 0.0f )
        {
            // Burnt out: what is left of it bursts among its own.
            m_foes[i].m_hacked = 0.0f;
            killFoe( m_foes[i], true );
            blast( at, HackBurn * weaponBoost() * power * ( 1.0f + 0.2f * r ), 2.6f );
            fire( "burnout" );
        }
    }
}

void Game::updateIone( float dt, const GameInput &in )
{
    // The screen: up while the left trigger is held and the swarm has strength left (holding it tires it).
    bool up = in.m_strafe && !m_overheat && m_scene == 0;
    m_block = Clamp( m_block + ( up ? 1.0f : -1.0f ) * dt * 7.0f, 0.0f, 1.0f );
    if ( m_block > 0.5f )
    {
        m_heat += ScreenLoad * dt;
        m_coolDelay = 0.3f;
        if ( m_heat >= 100.0f )
        {
            m_heat = 100.0f;
            m_overheat = true;
            fire( "overheat" );
            overheated();
            m_audio.play( SfxE::OVERHEAT, m_pos, m_camTarget, 0.6f );
        }
    }
    // The takeover.
    m_boltTimer -= dt;
    if ( in.m_special && m_boltTimer <= 0.0f && m_scene == 0 && !m_level.m_peaceful ) hack();
    updateHacked( dt );
    // Mending: health comes back over a few seconds, green motes rising.
    if ( m_repair > 0.0f )
    {
        m_repair -= dt;
        m_hp = fminf( hpMax(), m_hp + m_repairRate * dt );
        if ( rnd() < dt * 20.0f )
            m_particles.push_back(
                { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.7f, 0.3f + rnd() * 1.5f, ( rnd() - 0.5f ) * 0.7f } ),
                  { 0.0f, 0.7f, 0.0f },
                  0.7f,
                  0.7f,
                  0.014f,
                  { 120, 255, 170, 255 },
                  3 } );
    }
    // The storm: the whole cloud spins round her and cuts everything close.
    if ( m_storm > 0.0f )
    {
        m_storm -= dt;
        m_stormTick -= dt;
        if ( m_stormTick <= 0.0f )
        {
            m_stormTick = StormTick;
            float damage = ( 12.0f + 6.0f * m_stormRank ) * skillPower();
            bool  cut = false;
            for ( int i = 0; i < (int)m_foes.size(); ++i )
            {
                Foe &f = m_foes[i];
                if ( !f.m_alive || f.m_hidden || f.m_hacked > 0.0f ) continue;
                Vector3 d = { f.m_pos.x - m_pos.x, 0.0f, f.m_pos.z - m_pos.z };
                float   dist = Vector3Length( d );
                if ( dist > StormReach * areaScale() + foeRadius( f ) || fabsf( f.m_pos.y - m_pos.y ) > 2.5f ) continue;
                Vector3 dir = dist > 1e-3f ? Vector3Scale( d, 1.0f / dist ) : Vector3{ 0.0f, 0.0f, 1.0f };
                Vector3 at = Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } );
                dealHit( f, damage, dir, at );
                sparks( at, Vector3Negate( dir ), 5, { 160, 235, 255, 255 }, 2.5f );
                cut = true;
            }
            for ( int i = 0; i < (int)m_parts.size() && partsLive(); ++i )
                if ( m_parts[i].m_hp > 0.0f && Vector3Distance( m_parts[i].m_pos, m_pos ) < StormReach + 1.0f )
                    hitPart( i, damage, m_parts[i].m_pos ), cut = true;
            if ( cut ) m_audio.play( SfxE::SWARM, m_pos, m_camTarget, 0.6f );
        }
    }
    if ( m_cloudT > 0.0f ) m_cloudT -= dt;

    // The cloud: each drone eases to its place in what the swarm is doing now.
    Vector3 fwd = { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
    int     taken = -1;
    for ( int i = 0; i < (int)m_foes.size(); ++i )
        if ( m_foes[i].m_alive && m_foes[i].m_hacked > 0.0f ) taken = i;
    for ( int i = 0; i < (int)m_motes.size(); ++i )
    {
        Mote   &m = m_motes[i];
        float   u = fmodf( i * 0.618f, 1.0f );
        float   a = m.m_phase + m_time * ( 0.8f + 0.5f * u );
        Vector3 want = Vector3Add( m_pos, { cosf( a ) * ( 0.75f + 0.35f * sinf( m.m_phase * 3.0f ) ),
                                            1.25f + 0.65f * sinf( m_time * 1.3f + m.m_phase * 5.0f ),
                                            sinf( a ) * ( 0.75f + 0.35f * u ) } );
        float   follow = 7.0f;
        if ( m_storm > 0.0f )
        {
            float s = m.m_phase + m_time * 9.0f;
            float rad = 1.2f + ( StormReach - 1.2f ) * u;
            want = Vector3Add( m_pos, { cosf( s ) * rad, 0.5f + 1.1f * fmodf( i * 0.37f, 1.0f ), sinf( s ) * rad } );
            follow = 14.0f;
        }
        else if ( m_block > 0.3f )
        {
            // Coupled into a curved screen before her: eight columns of five.
            int     col = i % 8, row = i / 8;
            float   side = ( col - 3.5f ) / 3.5f * 0.95f;
            Vector3 dir = turnYaw( fwd, side );
            want = Vector3Add( m_pos, { dir.x * 1.15f, 0.35f + row * 0.42f, dir.z * 1.15f } );
            want = Vector3Lerp( m.m_pos, want, m_block );
            follow = 18.0f;
        }
        else if ( taken >= 0 && i < 6 )
        {
            // A few stay with the machine she has taken.
            const Foe &f = m_foes[taken];
            float      s = m.m_phase + m_time * 5.0f;
            want = Vector3Add(
                f.m_pos, { cosf( s ) * 0.8f, foeHeight( f ) + 0.3f + 0.2f * sinf( s * 2.0f ), sinf( s ) * 0.8f } );
            follow = 10.0f;
        }
        else if ( m_cloudT > 0.0f && i % 5 != 0 )
        {
            // Out cutting: tight round where the swarm works.
            float s = m.m_phase + m_time * 7.0f;
            float rad = 0.25f + 0.3f * u;
            want = Vector3Add( m_cloudAt, { cosf( s ) * rad, ( u - 0.5f ) * 0.7f, sinf( s ) * rad } );
            follow = 16.0f;
        }
        else if ( m_repair > 0.0f )
        {
            float s = m.m_phase + m_time * 4.0f;
            want = Vector3Add(
                m_pos, { cosf( s ) * 0.45f, 0.2f + fmodf( u + m_time * 0.6f, 1.0f ) * 1.8f, sinf( s ) * 0.45f } );
            follow = 10.0f;
        }
        m.m_pos = Vector3Lerp( m.m_pos, want, 1.0f - expf( -dt * follow * m.m_speed ) );
    }
}

void Game::ioneSkill( const std::string &key, int r )
{
    if ( key == "skill.repair" )
    {
        // First aid: the drones close the suit and the wounds under it.
        m_repair = 2.5f;
        m_repairRate = ( 26.0f + 11.0f * r ) * ( 1.0f + stat( StatE::REGEN ) * 0.05f ) / m_repair;
        m_audio.play( SfxE::STIM, m_pos, m_camTarget, 0.7f );
    }
    else if ( key == "skill.storm" )
    {
        m_storm = 2.4f + 0.3f * r;
        m_stormTick = 0.0f;
        m_stormRank = r;
        m_audio.play( SfxE::SWARM, m_pos, m_camTarget, 1.0f );
        m_shake = fmaxf( m_shake, 0.2f );
    }
}

void Game::poseIone( std::vector<Drawable> &out )
{
    if ( m_heroKind != HeroE::IONE || m_motes.empty() || m_dead ) return;
    // Dark drones; their lights in the swarm's colour (cyan cutters, white needles, blue sparks, orange embers, green
    // acid, deep blue guardians), blue-white as a screen, green while mending.
    const Vector4 family[6] = { { 0.3f, 1.6f, 2.2f, 0.0f },  { 1.6f, 1.8f, 2.0f, 0.0f }, { 1.0f, 1.4f, 3.2f, 0.0f },
                                { 2.6f, 1.0f, 0.25f, 0.0f }, { 0.6f, 2.4f, 0.3f, 0.0f }, { 0.4f, 0.8f, 2.8f, 0.0f } };
    Vector4       glow = family[m_progress.m_worn[0] ? std::clamp( itemLook( m_progress.m_gear[0] ), 0, 5 ) : 0];
    if ( m_block > 0.3f ) glow = { 0.8f, 1.5f, 2.6f, 0.0f };
    if ( m_repair > 0.0f ) glow = { 0.3f, 2.2f, 0.8f, 0.0f };
    if ( m_storm > 0.0f ) glow = { 0.9f, 2.4f, 2.8f, 0.0f };
    for ( const Mote &m : m_motes )
    {
        Drawable d;
        d.m_mesh = m_moteMesh;
        d.m_surface = S_DARK;
        float spin = m.m_phase + m_time * 3.0f;
        d.m_transform = MatrixMultiply( MatrixRotateY( spin ), MatrixTranslate( m.m_pos.x, m.m_pos.y, m.m_pos.z ) );
        d.m_sunShadow = d.m_spotShadow = false;
        out.push_back( d );
        d.m_mesh = m_moteLight;
        d.m_surface = S_GLOW;
        d.m_emissive = glow;
        out.push_back( d );
    }
}

void Game::autoIone( GameInput &in, const Target &best, float bestDist )
{
    // Take over what is near the aim, mend when hurt, storm when crowded.
    if ( m_boltTimer <= 0.0f && best.m_kind == 0 && bestDist < HackReach - 1.0f ) in.m_special = true;
    if ( m_hp < hpMax() * 0.55f ) in.m_skill2 = true;
    int close = 0;
    for ( const Foe &f : m_foes )
        if ( f.m_alive && !f.m_hidden && f.m_awake && f.m_hacked <= 0.0f && Vector3Distance( f.m_pos, m_pos ) < 4.0f )
            ++close;
    if ( close >= 2 ) in.m_skill3 = true;
}

float Game::screenCut() const
{
    return ScreenCut;
}

}   // namespace spaceopera
