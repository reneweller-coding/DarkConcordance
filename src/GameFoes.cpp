/**
 * @file GameFoes.cpp
 * @brief The later enemies: market guards behind riot shields, heavies with rotary guns, hovering drones, saw bots,
 *        bombers, and the Taxator in his exoskeleton; champions (a gift each, a ring and a name) and the guardian at a
 *        generated level's end; their bars and names over the game.
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

constexpr float GuardTime = 0.7f;   ///< a guard's blow with the maul (seconds)
constexpr float GuardReach = 2.1f;
constexpr float GuardDamage = 13.0f;
constexpr float HeavyShot = 4.0f;   ///< a rotary gun's slug
constexpr float DroneShot = 6.0f;
constexpr float SawCut = 5.0f;
constexpr float BomberDamage = 32.0f;
constexpr float BomberRadius = 3.2f;
constexpr float HeavyScale = 1.22f;
constexpr float TaxatorScale = 2.1f;
constexpr float StompDamage = 24.0f;
constexpr float StompRadius = 5.0f;
constexpr float RushDamage = 30.0f;

/** @brief A champion's gift as a colour: swift green, armoured blue, volatile orange, berserk red. */
const Color AffixColor[4] = {
    { 120, 255, 150, 255 }, { 140, 185, 255, 255 }, { 255, 140, 50, 255 }, { 255, 55, 70, 255 }
};

/** @brief The key of a kind's name in the texts. */
const char *foeKey( int kind )
{
    static const char *keys[] = { "foe.louse", "foe.welder", "foe.baton",  "foe.gunner",  "foe.shield", "foe.heavy",
                                  "foe.drone", "foe.saw",    "foe.bomber", "foe.taxator", "foe.shell" };
    return keys[kind];
}

}   // namespace

void Game::buildFoes()
{
    // Market guards: blued plates, a riot shield on the forearm and a maul; white visors.
    FigureSurfaces guard = { S_GUARD, S_DARK, S_METAL, S_GLOW };
    guard.m_visor = { 2.4f, 2.6f, 3.0f, 0.0f };
    guard.m_weapon = WeaponE::HAMMER;
    m_guardFigure.build( guard );
    // Heavies: the boarders' red-black, broader and taller, a rotary gun.
    FigureSurfaces heavy = { S_BOARDER, S_DARK, S_METAL, S_GLOW };
    heavy.m_visor = { 3.5f, 0.9f, 0.1f, 0.0f };
    heavy.m_weapon = WeaponE::RIFLE;
    heavy.m_scale = { HeavyScale * 1.12f, HeavyScale, HeavyScale * 1.1f };
    m_heavyFigure.build( heavy );
    // The Taxator: brass and black, twice a man's height, an arm cannon.
    FigureSurfaces tax = { S_TAXATOR, S_DARK, S_METAL, S_GLOW };
    tax.m_visor = { 3.8f, 2.2f, 0.3f, 0.0f };
    tax.m_weapon = WeaponE::RIFLE;
    tax.m_scale = { TaxatorScale * 1.15f, TaxatorScale, TaxatorScale * 1.1f };
    m_taxatorFigure.build( tax );
    // The Hydrauliker: oxidised green-blue, broad, a great wrench in his fist.
    FigureSurfaces hydra = { S_GUARD, S_RUST, S_METAL, S_GLOW };
    hydra.m_visor = { 0.4f, 2.6f, 3.0f, 0.0f };
    hydra.m_weapon = WeaponE::HAMMER;
    hydra.m_scale = { 1.9f * 1.25f, 1.9f, 1.9f * 1.15f };
    m_hydraFigure.build( hydra );
    // Saw bots: small dark lice with a blade on the back; bombers: hazard-striped, a charge that glows.
    m_saw.build( S_METAL, S_DARK, S_GLOW, false, 0.72f );
    m_bomber.build( S_HAZARD, S_METAL, S_GLOW, false, 1.05f );
    MeshBuilder blade;
    blade.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.3f, 0.02f, 20 );
    for ( int i = 0; i < 10; ++i )
    {
        float a = i * PI * 0.2f;
        blade.box( place( { cosf( a ) * 0.31f, 0.01f, sinf( a ) * 0.31f }, { 0.0f, -a + 0.6f, 0.0f } ),
                   { 0.05f, 0.012f, 0.025f } );
    }
    m_blade = blade.build().front();
    MeshBuilder sac;
    sac.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 0.22f, 0.16f, 0.26f }, 0.14f, 3 );
    m_sac = sac.build().front();
    // The drone: a flat body, four arms with rotors, an eye in front.
    MeshBuilder body;
    body.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 0.3f, 0.11f, 0.34f }, 0.08f, 3 );
    for ( int i = 0; i < 4; ++i )
    {
        float a = PI * 0.25f + i * PI * 0.5f;
        body.box( place( { cosf( a ) * 0.36f, 0.02f, sinf( a ) * 0.36f }, { 0.0f, -a, 0.0f } ),
                  { 0.22f, 0.025f, 0.035f } );
        body.cylinder( place( { cosf( a ) * 0.56f, 0.0f, sinf( a ) * 0.56f } ), 0.05f, 0.08f, 10 );
    }
    body.box( place( { 0.0f, -0.16f, 0.1f } ), { 0.05f, 0.06f, 0.18f } );
    m_droneBody = body.build().front();
    MeshBuilder rotor;
    rotor.box( place( { 0.0f, 0.0f, 0.0f } ), { 0.22f, 0.008f, 0.025f } );
    rotor.box( place( { 0.0f, 0.0f, 0.0f } ), { 0.025f, 0.008f, 0.22f } );
    m_droneRotor = rotor.build().front();
    MeshBuilder eye;
    eye.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 0.07f, 0.05f, 0.03f }, 0.02f, 2 );
    m_droneEye = eye.build().front();
    // A champion's ring on the floor (both faces, so it shows from any side).
    MeshBuilder ring;
    ring.lathe( place( { 0.0f, 0.0f, 0.0f } ), { { 0.62f, 0.0f }, { 0.78f, 0.0f } }, 36 );
    ring.lathe( place( { 0.0f, 0.0f, 0.0f } ), { { 0.78f, 0.0f }, { 0.62f, 0.0f } }, 36 );
    m_ring = ring.build().front();
}

void Game::makeElite( Foe &f, int elite )
{
    // A champion: much tougher, with one gift; the guardian of a level's end tougher still.
    f.m_elite = elite;
    f.m_affix = (int)( rnd() * 4.0f ) % 4;
    float more = elite == 2 ? 7.0f : 3.2f;
    if ( f.m_affix == 1 ) more *= 0.75f;   // it halves what hits it anyway
    f.m_hp *= more;
    f.m_hpMax = f.m_hp;
}

float Game::foeTall( const Foe &f ) const
{
    return 1.85f * ( f.m_kind == FoeE::HEAVY ? HeavyScale : ( f.m_kind == FoeE::TAXATOR ? TaxatorScale : 1.0f ) );
}

Figure &Game::foeFigure( const Foe &f )
{
    switch ( f.m_kind )
    {
    case FoeE::BATON: return m_boarderBaton;
    case FoeE::SHIELD: return m_guardFigure;
    case FoeE::HEAVY: return m_heavyFigure;
    case FoeE::TAXATOR:
        return f.m_variant == 1   ? m_hydraFigure
               : f.m_variant == 2 ? m_okaforFigure
               : f.m_variant == 3 ? m_leaderFigure
               : f.m_variant == 4 ? m_salvagerFigure
               : f.m_variant == 5 ? m_lammFigure
               : f.m_variant == 6 ? m_chiefFigure
               : f.m_variant == 7 ? m_foremanFigure
               : f.m_variant == 8 ? m_varnFigure
                                  : m_taxatorFigure;
    default: return m_boarderGun;
    }
}

int Game::boss() const
{
    for ( int i = 0; i < (int)m_foes.size(); ++i )
        if ( m_foes[i].m_alive && ( m_foes[i].m_kind == FoeE::TAXATOR || m_foes[i].m_elite == 2 ) ) return i;
    return -1;
}

void Game::fireShot( Vector3 muzzle, Vector3 at, float speed, float hurt, float spread )
{
    Vector3 aim =
        Vector3Add( at, { ( rnd() - 0.5f ) * spread, ( rnd() - 0.5f ) * spread * 0.4f, ( rnd() - 0.5f ) * spread } );
    Vector3 v = Vector3Scale( Vector3Normalize( Vector3Subtract( aim, muzzle ) ), speed );
    v = turnYaw( v, m_level.m_coriolis * Vector3Distance( aim, muzzle ) / speed );   // leads the drum's bend
    Shot s;
    s.m_pos = muzzle;
    s.m_vel = v;
    s.m_life = 1.8f;
    s.m_hurt = hurt * m_hitScale;
    m_shotsInFlight.push_back( s );
    m_flashes.push_back( { muzzle, { 16.0f, 5.0f, 1.5f }, 3.5f, 0.06f, 0.06f } );
    m_audio.play( SfxE::ZAP, muzzle, m_camTarget, 0.45f );
}

void Game::updateGuard( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    // Behind the shield it walks up steadily; close by it bashes with the shield and the maul.
    bool facing = ( sinf( f.m_yaw ) * dir.x + cosf( f.m_yaw ) * dir.z ) > 0.6f;
    if ( f.m_swingT >= 0.0f )
    {
        float before = f.m_swingT;
        f.m_swingT += dt / GuardTime;
        if ( f.m_swingT < 0.4f ) faceYaw = atan2f( dir.x, dir.z );
        if ( before < 0.5f && f.m_swingT >= 0.5f && dist < GuardReach + 0.4f && facing &&
             fabsf( m_pos.y - f.m_pos.y ) < 1.5f )
        {
            hurtHero( GuardDamage, { dir.x * 4.5f, 0.6f, dir.z * 4.5f }, f.m_pos );
            sparks( Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), Vector3Negate( dir ), 10, { 150, 200, 255, 255 }, 3.0f );
        }
        if ( f.m_swingT >= 1.0f ) f.m_swingT = -1.0f, f.m_timer = 0.8f + rnd() * 0.5f;
        return;
    }
    want = Vector3Scale( path, f.m_timer > 0.0f ? 1.0f : 2.4f );
    faceYaw = dist < 7.0f && f.m_los ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
    if ( dist < 2.0f && f.m_los && f.m_timer <= 0.0f && fabsf( m_pos.y - f.m_pos.y ) < 1.2f )
    {
        f.m_swingT = 0.0f;
        m_audio.play( SfxE::SWING, f.m_pos, m_camTarget, 0.5f );
    }
}

void Game::updateHeavy( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    // The rotary gun spins up (its laser shows where), then hoses a long burst; it walks slowly and stands to fire.
    f.m_spit -= dt;
    f.m_spin += dt * ( f.m_aim > 0.0f || f.m_burst > 0 ? 30.0f : 2.0f );
    Vector3 fwd = { sinf( f.m_yaw ), 0.0f, cosf( f.m_yaw ) };
    if ( f.m_burst > 0 )
    {
        faceYaw = atan2f( dir.x, dir.z );
        if ( f.m_timer <= 0.0f )
        {
            Vector3 muzzle =
                Vector3Add( f.m_pos, { fwd.x * 0.9f * HeavyScale, 1.35f * HeavyScale, fwd.z * 0.9f * HeavyScale } );
            fireShot( muzzle, Vector3Add( m_pos, { m_vel.x * 0.12f, 1.2f, m_vel.z * 0.12f } ), 22.0f, HeavyShot, 1.6f );
            --f.m_burst;
            f.m_timer = 0.085f;
            if ( f.m_burst == 0 ) f.m_aim = 0.0f, f.m_spit = 2.8f + rnd() * 1.5f;
        }
        return;
    }
    if ( f.m_aim > 0.0f )
    {
        f.m_aim += dt / 1.0f;
        faceYaw = atan2f( dir.x, dir.z );
        if ( !f.m_los ) f.m_aim = 0.0f;
        if ( f.m_aim >= 1.0f ) f.m_burst = 10, f.m_timer = 0.0f;
        return;
    }
    if ( !f.m_los || dist > 13.0f )
        want = Vector3Scale( path, 1.7f );
    else if ( dist < 6.0f )
        want = Vector3Scale( dir, -1.2f );
    else
    {
        float side = ( ( f.m_id + (int)( m_time / 4.0f ) ) % 2 ) ? 1.0f : -1.0f;
        want = { -dir.z * side * 0.7f, 0.0f, dir.x * side * 0.7f };
    }
    faceYaw = f.m_los ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
    if ( f.m_spit <= 0.0f && f.m_los && dist < 18.0f && f.m_timer <= 0.0f ) f.m_aim = 0.01f;
}

void Game::updateCritter( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    float dy = fabsf( m_pos.y - f.m_pos.y );
    switch ( f.m_kind )
    {
    case FoeE::SAW:
    {
        // Straight at the hero, the blade whining; it cuts while it touches.
        f.m_spin += dt * 34.0f;
        want = Vector3Scale( path, 5.0f );
        faceYaw = atan2f( path.x, path.z );
        if ( dist < foeRadius( f ) + 0.55f && dy < 1.0f && f.m_timer <= 0.0f )
        {
            hurtHero( SawCut, { dir.x * 1.2f, 0.2f, dir.z * 1.2f }, f.m_pos );
            sparks( Vector3Add( m_pos, { 0.0f, 0.6f, 0.0f } ), Vector3Negate( dir ), 10, { 255, 220, 140, 255 }, 4.0f );
            f.m_timer = 0.45f;
        }
        break;
    }
    case FoeE::BOMBER:
    {
        // It runs at the hero; close by the fuse lights (it blinks), and then it goes off, whoever stands near.
        if ( f.m_fuse >= 0.0f )
        {
            f.m_fuse -= dt;
            want = Vector3Scale( dir, 1.2f );
            faceYaw = atan2f( dir.x, dir.z );
            if ( f.m_fuse <= 0.0f ) killFoe( f, false );
            break;
        }
        want = Vector3Scale( path, 3.9f );
        faceYaw = atan2f( path.x, path.z );
        if ( dist < 2.4f && f.m_los && dy < 1.5f )
        {
            f.m_fuse = 0.75f;
            m_audio.play( SfxE::SIZZLE, f.m_pos, m_camTarget, 0.9f );
            fire( "bomber" );
        }
        break;
    }
    case FoeE::DRONE:
    {
        // It hovers at head height, keeps seven metres off, circles and shoots.
        f.m_spin += dt * 45.0f;
        float floor = groundAt( f.m_pos );
        if ( floor > -100.0f ) f.m_rush.y = floor;
        float side = ( ( f.m_id + (int)( m_time / 4.0f ) ) % 2 ) ? 1.0f : -1.0f;
        if ( !f.m_los || dist > 11.0f )
            want = Vector3Scale( path, 3.2f );
        else if ( dist < 5.0f )
            want = Vector3Scale( dir, -2.6f );
        else
            want = { -dir.z * side * 2.2f, 0.0f, dir.x * side * 2.2f };
        float hover = f.m_rush.y + 1.75f + 0.15f * sinf( m_time * 2.0f + f.m_id );
        want.y = ( hover - f.m_pos.y ) * 3.0f;
        faceYaw = atan2f( dir.x, dir.z );
        f.m_spit -= dt;
        if ( f.m_spit <= 0.0f && f.m_los && dist < 16.0f )
        {
            Vector3 fwd = { sinf( f.m_yaw ), 0.0f, cosf( f.m_yaw ) };
            fireShot( Vector3Add( f.m_pos, Vector3Scale( fwd, 0.4f ) ), Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ),
                      18.0f, DroneShot, 0.6f );
            f.m_spit = 1.4f + rnd() * 0.8f;
        }
        break;
    }
    default: break;
    }
}

void Game::updateTaxator( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    if ( f.m_variant == 1 )
    {
        updateHydraulic( f, dt, dir, dist, path, want, faceYaw );
        return;
    }
    // Three stages as he weakens; at each new one he calls for help. He stomps when close, sweeps the cannon at
    // range, and in the last stage charges (into a wall, he stands stunned).
    float share = f.m_hp / fmaxf( 1.0f, f.m_hpMax );
    int   stage = share > 0.6f ? 0 : ( share > 0.3f ? 1 : 2 );
    if ( stage > f.m_stage )
    {
        f.m_stage = stage;
        fire( stage == 1 ? "taxator2" : "taxator3" );
        // Okafor calls her clearance squad and its hounds, the boarding leader his cutters.
        std::vector<FoeE> help = stage == 1
                                     ? std::vector<FoeE>{ FoeE::GUNNER, FoeE::GUNNER, FoeE::SHIELD }
                                     : std::vector<FoeE>{ FoeE::SAW, FoeE::SAW, FoeE::SAW, FoeE::SAW, FoeE::BOMBER };
        int               helpVariant = 0;
        if ( f.m_variant == 2 )
            help = stage == 1 ? std::vector<FoeE>{ FoeE::GUNNER, FoeE::GUNNER, FoeE::SAW }
                              : std::vector<FoeE>{ FoeE::SAW, FoeE::SAW, FoeE::SAW },
            helpVariant = stage == 1 ? 2 : 1;
        else if ( f.m_variant == 3 )
            help = std::vector<FoeE>{ FoeE::BATON, FoeE::BATON, FoeE::BATON }, helpVariant = 1;
        else if ( f.m_variant == 4 )
            help = std::vector<FoeE>{ FoeE::DRONE, FoeE::DRONE, FoeE::BATON, FoeE::BATON }, helpVariant = 0;
        // Lamm's admirers jump into the sand; the chief calls his cleaners and their snipers; the foreman his
        // cutting bots; Ysolde a squad that knows her.
        else if ( f.m_variant == 5 )
            help = std::vector<FoeE>{ FoeE::BATON, FoeE::BATON, FoeE::SAW, FoeE::SAW },
            helpVariant = stage == 1 ? 0 : 1;
        else if ( f.m_variant == 6 )
            help = stage == 1 ? std::vector<FoeE>{ FoeE::GUNNER, FoeE::GUNNER, FoeE::SHIELD }
                              : std::vector<FoeE>{ FoeE::GUNNER, FoeE::DRONE, FoeE::DRONE },
            helpVariant = stage == 1 ? 0 : 3;
        else if ( f.m_variant == 7 )
            help = std::vector<FoeE>{ FoeE::SAW, FoeE::SAW, FoeE::SAW, FoeE::DRONE, FoeE::DRONE }, helpVariant = 0;
        else if ( f.m_variant == 8 )
            help = stage == 1 ? std::vector<FoeE>{ FoeE::BATON, FoeE::BATON, FoeE::SHIELD }
                              : std::vector<FoeE>{ FoeE::GUNNER, FoeE::GUNNER, FoeE::BATON },
            helpVariant = stage == 1 ? 1 : 0;
        for ( size_t i = 0; i < help.size(); ++i )
        {
            // From the room's edges, where there is floor.
            for ( int t = 0; t < 12; ++t )
            {
                float   a = rnd() * 6.283f;
                Vector3 p = Vector3Add(
                    f.m_pos, { cosf( a ) * ( 8.0f + rnd() * 4.0f ), 0.0f, sinf( a ) * ( 8.0f + rnd() * 4.0f ) } );
                float floor = groundAt( p );
                if ( floor < -100.0f || floor > 100.0f ||
                     !los( Vector3Add( f.m_pos, { 0, 1.5f, 0 } ), Vector3Add( p, { 0, 1.5f, 0 } ) ) )
                    continue;
                p.y = floor;
                m_summons.push_back( { help[i], p, helpVariant } );
                break;
            }
        }
        m_audio.play( SfxE::KLAXON, f.m_pos, m_camTarget, 1.0f );
    }
    float speed = stage == 2 ? 2.5f : 1.9f;
    if ( f.m_variant == 5 || f.m_variant == 8 ) speed *= 1.35f;   // the duellists are quick on their feet
    if ( f.m_variant == 7 ) speed *= 0.85f;                       // the crane walker is not
    f.m_aim = fmaxf( 0.0f, f.m_aim - dt );                        // until he may charge again
    f.m_spit -= dt;
    switch ( f.m_state )
    {
    case FoeStateE::WINDUP:
        // The stomp: up on one leg, then down; a ring of dust that hurts within five metres.
        faceYaw = atan2f( dir.x, dir.z );
        f.m_crouch = fminf( 1.0f, f.m_crouch + dt * 1.6f );
        if ( f.m_timer <= 0.0f )
        {
            Vector3 floor = { f.m_pos.x, f.m_pos.y + 0.05f, f.m_pos.z };
            for ( int i = 0; i < 48; ++i )
            {
                float a = i * PI * 2.0f / 48.0f;
                m_particles.push_back( { floor,
                                         { cosf( a ) * 6.0f, 0.3f + rnd() * 0.5f, sinf( a ) * 6.0f },
                                         0.7f,
                                         0.7f,
                                         0.35f,
                                         { 200, 175, 140, 26 },
                                         1 } );
            }
            sparks( floor, { 0.0f, 1.0f, 0.0f }, 30, { 255, 180, 90, 255 }, 5.0f );
            m_audio.play( SfxE::SLAM, floor, m_camTarget, 1.0f );
            m_shake = fmaxf( m_shake, 0.6f );
            if ( dist < StompRadius && fabsf( m_pos.y - f.m_pos.y ) < 1.5f )
                hurtHero( StompDamage, { dir.x * 6.0f, 2.0f, dir.z * 6.0f }, f.m_pos );
            f.m_state = FoeStateE::RECOVER;
            f.m_timer = 0.9f;
        }
        return;
    case FoeStateE::SPIT:
        // The cannon: volleys of five in a fan.
        faceYaw = atan2f( dir.x, dir.z );
        if ( f.m_timer <= 0.0f && f.m_burst > 0 )
        {
            Vector3 fwd = { sinf( f.m_yaw ), 0.0f, cosf( f.m_yaw ) };
            Vector3 muzzle = Vector3Add( f.m_pos, { fwd.x * 1.6f, 2.8f, fwd.z * 1.6f } );
            for ( int k = -2; k <= 2; ++k )
            {
                float   a = atan2f( dir.x, dir.z ) + k * 0.13f;
                Vector3 at = Vector3Add( muzzle, { sinf( a ) * dist, 0.0f, cosf( a ) * dist } );
                at.y = m_pos.y + 1.1f;
                fireShot( muzzle, at, 20.0f, 7.0f, 0.3f );
            }
            --f.m_burst;
            f.m_timer = 0.38f;
        }
        if ( f.m_burst <= 0 && f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE, f.m_spit = 2.6f + rnd();
        return;
    case FoeStateE::LEAP:
        if ( f.m_burst >= 0 )
        {
            // Lowered, the visor red: then he goes.
            faceYaw = atan2f( dir.x, dir.z );
            f.m_rush = dir;
            f.m_crouch = fminf( 1.0f, f.m_crouch + dt * 2.0f );
            if ( f.m_timer <= 0.0f )
                f.m_burst = -1, f.m_timer = 1.1f, m_audio.play( SfxE::THRUST, f.m_pos, m_camTarget, 1.0f );
            return;
        }
        {
            want = Vector3Scale( f.m_rush, 11.0f );
            faceYaw = atan2f( f.m_rush.x, f.m_rush.z );
            float speedNow = sqrtf( f.m_vel.x * f.m_vel.x + f.m_vel.z * f.m_vel.z );
            bool  wall = f.m_timer < 0.85f && speedNow < 3.0f;
            if ( dist < 1.9f && fabsf( m_pos.y - f.m_pos.y ) < 1.5f )
            {
                hurtHero( RushDamage, { f.m_rush.x * 8.0f, 2.5f, f.m_rush.z * 8.0f }, f.m_pos );
                m_shake = fmaxf( m_shake, 0.6f );
                f.m_timer = 0.0f;
            }
            if ( wall )
            {
                // Into the wall: sparks, and he stands stunned.
                sparks( Vector3Add( f.m_pos, Vector3Add( Vector3Scale( f.m_rush, 1.0f ), { 0.0f, 2.0f, 0.0f } ) ),
                        Vector3Negate( f.m_rush ), 40, { 255, 200, 120, 255 }, 6.0f );
                m_audio.play( SfxE::SMASH, f.m_pos, m_camTarget, 1.0f );
                m_shake = fmaxf( m_shake, 0.7f );
                f.m_stun = 1.8f;
                f.m_timer = 0.0f;
            }
            if ( f.m_timer <= 0.0f )
            {
                f.m_burst = 0;
                f.m_state = FoeStateE::RECOVER;
                f.m_timer = 0.6f;
                f.m_aim = 5.0f;
            }
        }
        return;
    case FoeStateE::RECOVER:
        f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 2.0f );
        if ( f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE;
        return;
    default: break;
    }
    f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 2.0f );
    want = Vector3Scale( path, speed );
    faceYaw = f.m_los ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
    if ( dist < 4.5f && f.m_timer <= 0.0f )
        f.m_state = FoeStateE::WINDUP, f.m_timer = 0.8f;
    else if ( stage == 2 && f.m_los && dist > 6.0f && dist < 16.0f && f.m_aim <= 0.0f )
        f.m_state = FoeStateE::LEAP, f.m_timer = 0.7f, f.m_burst = 0;
    else if ( f.m_los && f.m_spit <= 0.0f && dist < 22.0f )
        f.m_state = FoeStateE::SPIT, f.m_burst = 3 + stage, f.m_timer = 0.2f;
}

void Game::updateShocker( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    // A shock gun: it charges (the muzzle sparks blue), then an arc jumps to the hero; in water it bites much harder.
    f.m_spit -= dt;
    Vector3 fwd = { sinf( f.m_yaw ), 0.0f, cosf( f.m_yaw ) };
    Vector3 muzzle = Vector3Add( f.m_pos, { fwd.x * 0.7f, 1.35f, fwd.z * 0.7f } );
    if ( f.m_aim > 0.0f )
    {
        f.m_aim += dt / 0.7f;
        faceYaw = atan2f( dir.x, dir.z );
        if ( rnd() < dt * 30.0f ) sparks( muzzle, fwd, 2, { 140, 200, 255, 255 }, 1.5f );
        if ( !f.m_los || dist > 13.0f ) f.m_aim = 0.0f;
        if ( f.m_aim >= 1.0f )
        {
            Vector3 chest = Vector3Add( m_pos, { 0.0f, 1.1f, 0.0f } );
            m_tracers.push_back( { muzzle, chest, 0.15f } );
            m_tracers.push_back( { muzzle, Vector3Lerp( muzzle, chest, 0.5f ), 0.15f } );
            sparks( chest, Vector3Negate( dir ), 14, { 150, 205, 255, 255 }, 3.5f );
            m_flashes.push_back( { chest, { 8.0f, 14.0f, 26.0f }, 5.0f, 0.15f, 0.15f } );
            m_audio.play( SfxE::CRACKLE, chest, m_camTarget, 0.8f );
            m_hurtBy = "shock";
            hurtHero( 9.0f * ( m_inWater ? 2.5f : 1.0f ), { dir.x * 1.0f, 0.0f, dir.z * 1.0f }, f.m_pos );
            m_hurtBy = "";
            f.m_aim = 0.0f;
            f.m_spit = 2.4f + rnd() * 1.2f;
        }
        return;
    }
    float side = ( ( f.m_id + (int)( m_time / 3.0f ) ) % 2 ) ? 1.0f : -1.0f;
    if ( !f.m_los || dist > 10.0f )
        want = Vector3Scale( path, 2.6f );
    else if ( dist < 5.5f )
        want = Vector3Scale( dir, -2.0f );
    else
        want = { -dir.z * side * 1.4f, 0.0f, dir.x * side * 1.4f };
    faceYaw = f.m_los ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
    if ( f.m_spit <= 0.0f && f.m_los && dist < 11.0f && f.m_timer <= 0.0f ) f.m_aim = 0.01f;
}

void Game::updateHydraulic( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    // The Hydrauliker: a water cannon that throws one back, a hydraulic slam close by; at two thirds and one third
    // he floods the place where the hero stands (a new ball of water round them) and calls for help.
    float share = f.m_hp / fmaxf( 1.0f, f.m_hpMax );
    int   stage = share > 0.66f ? 0 : ( share > 0.33f ? 1 : 2 );
    if ( stage > f.m_stage )
    {
        f.m_stage = stage;
        fire( stage == 1 ? "hydra2" : "hydra3" );
        if ( m_waters.size() < 12 && groundAt( m_pos ) > -100.0f )
            m_waters.push_back( { { m_pos.x, m_pos.y + 2.0f, m_pos.z }, 3.4f, stage == 1 ? 1.0f : -1.0f, 0.05f } );
        std::vector<FoeE> help = stage == 1 ? std::vector<FoeE>{ FoeE::GUNNER, FoeE::GUNNER }
                                            : std::vector<FoeE>{ FoeE::SAW, FoeE::SAW, FoeE::SAW, FoeE::BOMBER };
        for ( FoeE k : help )
            for ( int t = 0; t < 12; ++t )
            {
                float   a = rnd() * 6.283f;
                Vector3 p = Vector3Add( f.m_pos, { cosf( a ) * 9.0f, 0.0f, sinf( a ) * 9.0f } );
                float   floor = groundAt( p );
                if ( floor < -100.0f || floor > 100.0f ) continue;
                p.y = floor;
                m_summons.push_back( { k, p } );
                break;
            }
        m_audio.play( SfxE::BURST, f.m_pos, m_camTarget, 1.0f );
    }
    f.m_spit -= dt;
    switch ( f.m_state )
    {
    case FoeStateE::WINDUP:
        // The slam: the wrench up, then down into the deck; a ring of spray that hurts within five metres.
        faceYaw = atan2f( dir.x, dir.z );
        f.m_swingT = fminf( 0.45f, f.m_swingT < 0.0f ? 0.0f : f.m_swingT + dt * 0.6f );
        if ( f.m_timer <= 0.0f )
        {
            for ( int i = 0; i < 40; ++i )
            {
                float a = i * PI * 2.0f / 40.0f;
                m_particles.push_back( { Vector3Add( f.m_pos, { 0.0f, 0.2f, 0.0f } ),
                                         { cosf( a ) * 6.0f, 1.0f + rnd(), sinf( a ) * 6.0f },
                                         0.7f,
                                         0.7f,
                                         0.3f,
                                         { 170, 210, 230, 40 },
                                         1 } );
            }
            m_audio.play( SfxE::SLAM, f.m_pos, m_camTarget, 1.0f );
            m_shake = fmaxf( m_shake, 0.5f );
            if ( dist < 5.0f && fabsf( m_pos.y - f.m_pos.y ) < 1.5f )
                hurtHero( 24.0f, { dir.x * 6.0f, 2.0f, dir.z * 6.0f }, f.m_pos );
            f.m_swingT = -1.0f;
            f.m_state = FoeStateE::RECOVER;
            f.m_timer = 0.9f;
        }
        return;
    case FoeStateE::SPIT:
    {
        // The water cannon: a hard jet for two seconds; it hurts a little and throws one far back.
        Vector3 fwd = { sinf( f.m_yaw ), 0.0f, cosf( f.m_yaw ) };
        faceYaw = atan2f( dir.x, dir.z );
        Vector3 nozzle = Vector3Add( f.m_pos, { fwd.x * 1.6f, 2.2f, fwd.z * 1.6f } );
        for ( int k = 0; k < 3; ++k )
            m_particles.push_back( { nozzle,
                                     Vector3Add( Vector3Scale( fwd, 16.0f + rnd() * 4.0f ),
                                                 { ( rnd() - 0.5f ) * 1.5f, -1.0f + rnd(), ( rnd() - 0.5f ) * 1.5f } ),
                                     0.8f,
                                     0.8f,
                                     0.12f + rnd() * 0.1f,
                                     { 190, 225, 240, 70 },
                                     1 } );
        Vector3 to = Vector3Subtract( m_pos, f.m_pos );
        float   along = to.x * fwd.x + to.z * fwd.z;
        float   across = fabsf( to.x * fwd.z - to.z * fwd.x );
        f.m_aim -= dt;
        if ( along > 0.0f && along < 14.0f && across < 1.4f && f.m_aim <= 0.0f )
        {
            hurtHero( 5.0f, { fwd.x * 7.0f, 0.5f, fwd.z * 7.0f }, f.m_pos );
            f.m_aim = 0.25f;
        }
        if ( f.m_timer <= 0.0f ) f.m_state = FoeStateE::RECOVER, f.m_timer = 0.8f, f.m_spit = 3.0f + rnd() * 1.5f;
        return;
    }
    case FoeStateE::RECOVER:
        if ( f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE;
        return;
    default: break;
    }
    want = Vector3Scale( path, stage == 2 ? 2.4f : 1.9f );
    faceYaw = f.m_los ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
    if ( dist < 4.5f && f.m_timer <= 0.0f )
        f.m_state = FoeStateE::WINDUP, f.m_timer = 0.8f, f.m_swingT = 0.0f;
    else if ( f.m_los && f.m_spit <= 0.0f && dist < 14.0f )
    {
        f.m_state = FoeStateE::SPIT, f.m_timer = 2.0f, f.m_aim = 0.0f;
        m_audio.play( SfxE::BURST, f.m_pos, m_camTarget, 0.8f );
    }
}

void Game::detonate( Foe &f, float damage, float radius )
{
    // A ball of fire: it hurts the hero and every enemy near (bombers set each other off).
    Vector3 p = Vector3Add( f.m_pos, { 0.0f, 0.5f, 0.0f } );
    explode( p, 1.7f );
    sparks( p, { 0.0f, 1.0f, 0.0f }, 40, { 255, 170, 70, 255 }, 7.0f );
    m_shake = fmaxf( m_shake, 0.5f );
    float d = Vector3Distance( p, Vector3Add( m_pos, { 0.0f, 0.9f, 0.0f } ) );
    if ( d < radius && !m_dead )
    {
        float   keep = m_hitScale;
        Vector3 away = Vector3Normalize( { m_pos.x - p.x + 1e-3f, 0.0f, m_pos.z - p.z } );
        m_hitScale = f.m_elite ? 1.4f : 1.0f;
        hurtHero( damage * ( 1.0f - 0.5f * d / radius ), { away.x * 5.0f, 2.0f, away.z * 5.0f }, p );
        m_hitScale = keep;
    }
    for ( Foe &o : m_foes )
    {
        if ( &o == &f || !o.m_alive || o.m_hidden ) continue;
        Vector3 to = Vector3Subtract( Vector3Add( o.m_pos, { 0.0f, foeHeight( o ), 0.0f } ), p );
        float   dist = Vector3Length( to );
        if ( dist > radius + foeRadius( o ) ) continue;
        hurtFoe( o, damage * 1.2f * ( 1.0f - 0.5f * fminf( 1.0f, dist / radius ) ),
                 dist > 1e-3f ? Vector3Scale( to, 1.0f / dist ) : Vector3{ 0.0f, 1.0f, 0.0f } );
    }
}

void Game::bossDown( Foe &f )
{
    // The way out opens; plenty to pick up; the suit comes apart in three blasts.
    m_bossAt = f.m_pos;
    fire( "bossdead" );
    for ( int i = 0; i < 3; ++i )
        explode( Vector3Add( f.m_pos, { ( rnd() - 0.5f ) * 2.0f, 0.8f + i * 1.0f, ( rnd() - 0.5f ) * 2.0f } ), 2.0f );
    m_shake = fmaxf( m_shake, 0.9f );
    int items = f.m_kind == FoeE::TAXATOR ? 4 : 3;
    for ( int i = 0; i < items; ++i ) dropItem( Vector3Add( f.m_pos, { 0.0f, 1.2f, 0.0f } ), 5.0f );
    m_pickups.push_back( { 0, 60, Vector3Add( f.m_pos, { 0.0f, 1.0f, 0.0f } ), { 1.0f, 3.0f, 0.5f } } );
    m_pickups.push_back( { 1, 1, Vector3Add( f.m_pos, { 0.0f, 1.0f, 0.0f } ), { -1.0f, 3.0f, -0.5f } } );
}

void Game::poseFoes( std::vector<Drawable> &out )
{
    auto add = [&]( Mesh mesh, int surface, Matrix at, Vector4 glow = { -1, 0, 0, 0 } )
    {
        Drawable d;
        d.m_mesh = mesh;
        d.m_surface = surface;
        d.m_transform = at;
        d.m_emissive = glow;
        out.push_back( d );
    };
    for ( const Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden ) continue;
        Matrix turn = MatrixRotateY( f.m_yaw );
        if ( f.m_kind == FoeE::DRONE )
        {
            // Tilted into its flight; rotors spinning; the eye red, white when hit.
            Matrix tilt = MatrixMultiply( MatrixRotateX( Clamp( f.m_vel.z * 0.06f, -0.3f, 0.3f ) ),
                                          MatrixRotateZ( Clamp( -f.m_vel.x * 0.06f, -0.3f, 0.3f ) ) );
            float  size = f.m_variant == 5 ? 0.55f : ( f.m_variant == 6 ? 2.4f : 1.0f );
            Matrix at = MatrixMultiply( MatrixMultiply( MatrixMultiply( MatrixScale( size, size, size ), turn ), tilt ),
                                        MatrixTranslate( f.m_pos.x, f.m_pos.y, f.m_pos.z ) );
            add( m_droneBody, f.m_variant >= 5 && m_level.m_faction == "collegium" ? S_COLLEGIUM : S_DARK, at );
            if ( f.m_variant == 6 )
                add( m_ring, S_GLOW, MatrixMultiply( MatrixScale( 1.3f, 1.0f, 1.3f ), at ),
                     { 0.6f + f.m_flash * 3.0f, 2.0f + f.m_flash * 3.0f, 4.0f + f.m_flash * 3.0f, 0.0f } );
            for ( int i = 0; i < 4; ++i )
            {
                float a = PI * 0.25f + i * PI * 0.5f;
                add( m_droneRotor, S_METAL,
                     MatrixMultiply( MatrixMultiply( MatrixRotateY( f.m_spin * ( i % 2 ? 1.0f : -1.0f ) ),
                                                     MatrixTranslate( cosf( a ) * 0.56f, 0.06f, sinf( a ) * 0.56f ) ),
                                     at ) );
            }
            float w = 3.0f * f.m_flash;
            // The eye: red; the swarm's cold blue (white before it dives), the deletions white.
            Vector4 eye = { 6.0f + w, 0.5f + w, 0.2f + w, 0.0f };
            if ( f.m_variant == 5 )
                eye = f.m_state == FoeStateE::LEAP || m_fired.count( "formation" ) ? Vector4{ 8.0f, 8.0f, 9.0f, 0.0f }
                      : m_swarmKind == "delete" ? Vector4{ 6.0f + w, 6.0f + w, 6.5f + w, 0.0f }
                                                : Vector4{ 0.6f + w, 2.6f + w, 6.0f + w, 0.0f };
            add( m_droneEye, S_GLOW, MatrixMultiply( MatrixTranslate( 0.0f, -0.02f, 0.36f ), at ), eye );
        }
        else if ( f.m_kind == FoeE::SAW && f.m_variant == 0 )
        {
            Matrix at = MatrixMultiply( MatrixRotateY( f.m_spin ),
                                        MatrixTranslate( f.m_pos.x, f.m_pos.y + m_saw.m_height + 0.2f, f.m_pos.z ) );
            add( m_blade, S_METAL, at );
        }
        else if ( f.m_kind == FoeE::BOMBER )
        {
            // The charge glows, faster and brighter as the fuse burns.
            float  pulse = f.m_fuse >= 0.0f ? ( fmodf( f.m_fuse * 12.0f, 1.0f ) > 0.5f ? 1.0f : 0.25f )
                                            : 0.5f + 0.25f * sinf( m_time * 4.0f + f.m_id );
            float  size = f.m_fuse >= 0.0f ? 1.0f + ( 0.75f - f.m_fuse ) * 0.4f : 1.0f;
            Matrix at =
                MatrixMultiply( MatrixMultiply( MatrixScale( size, size, size ), turn ),
                                MatrixTranslate( f.m_pos.x, f.m_pos.y + m_bomber.m_height + 0.25f, f.m_pos.z ) );
            add( m_sac, S_GLOW, at, { 5.0f * pulse, 1.8f * pulse, 0.3f * pulse, 0.0f } );
        }
        if ( f.m_elite )
        {
            // The champion's ring, in its gift's colour.
            Color    c = AffixColor[std::max( 0, f.m_affix )];
            float    pulse = 0.7f + 0.3f * sinf( m_time * 5.0f + f.m_id );
            float    size = f.m_kind == FoeE::TAXATOR
                                ? 2.2f
                                : ( f.m_kind == FoeE::WELDER || f.m_kind == FoeE::HEAVY ? 1.5f : 1.0f );
            float    floor = f.m_kind == FoeE::DRONE ? f.m_rush.y : f.m_pos.y;
            Drawable d;
            d.m_mesh = m_ring;
            d.m_surface = S_GLOW;
            d.m_transform = MatrixMultiply( MatrixScale( size, 1.0f, size ),
                                            MatrixTranslate( f.m_pos.x, floor + 0.04f, f.m_pos.z ) );
            d.m_emissive = { c.r / 255.0f * 5.0f * pulse, c.g / 255.0f * 5.0f * pulse, c.b / 255.0f * 5.0f * pulse,
                             0.0f };
            d.m_sunShadow = d.m_spotShadow = false;
            out.push_back( d );
        }
    }
}

void Game::drawFoeHud( int w, int h, float s )
{
    auto text = [&]( const char *t, float x, float y, float size, Color c )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        DrawTextEx( m_font, t, { x - m.x * 0.5f + 2.0f * s, y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
        DrawTextEx( m_font, t, { x - m.x * 0.5f, y }, size * s, 1.0f, c );
    };
    // Champions near by: name and gift over the head, a thin bar.
    for ( const Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden || f.m_elite != 1 || Vector3Distance( f.m_pos, m_pos ) > 18.0f ) continue;
        float   top = f.m_kind == FoeE::DRONE ? 0.6f : ( human( f ) ? foeTall( f ) + 0.4f : foeHeight( f ) + 0.9f );
        Vector3 head = Vector3Add( f.m_pos, { 0.0f, top, 0.0f } );
        if ( Vector3DotProduct( Vector3Subtract( head, m_cam.position ),
                                Vector3Subtract( m_cam.target, m_cam.position ) ) <= 0.0f )
            continue;
        Vector2     q = GetWorldToScreenEx( head, m_cam, w, h );
        Color       c = AffixColor[std::max( 0, f.m_affix )];
        const char *noun = foeNoun( f );
        // The gift's word agrees with the name (German: "Gepanzerte Hülle", "Gepanzerter Abschöpfer").
        bool fem = hasText( TextFormat( "%s.g", noun ) ) && std::string( tr( TextFormat( "%s.g", noun ) ) ) == "f";
        std::string name =
            std::string( tr( TextFormat( fem ? "elite.%d.f" : "elite.%d", f.m_affix ) ) ) + " " + tr( noun );
        text( name.c_str(), q.x, q.y - 30.0f * s, 18, c );
        float bw = 90.0f * s;
        DrawRectangle( (int)( q.x - bw * 0.5f ), (int)( q.y - 8.0f * s ), (int)bw, (int)( 6.0f * s ),
                       { 20, 0, 0, 200 } );
        DrawRectangle( (int)( q.x - bw * 0.5f ), (int)( q.y - 8.0f * s ),
                       (int)( bw * Clamp( f.m_hp / f.m_hpMax, 0.0f, 1.0f ) ), (int)( 6.0f * s ), c );
    }
    // The boss: a bar across the top once he is awake.
    int b = boss();
    if ( b < 0 || !m_foes[b].m_awake || m_foes[b].m_kind == FoeE::SHELL ) return;
    const Foe  &f = m_foes[b];
    std::string name = f.m_kind == FoeE::TAXATOR
                           ? std::string( tr( f.m_variant == 1   ? "boss.hydraulic"
                                              : f.m_variant == 2 ? "boss.okafor"
                                              : f.m_variant == 3 ? "boss.leader"
                                              : f.m_variant == 4 ? "boss.salvager"
                                              : f.m_variant == 5 ? "boss.lamm"
                                              : f.m_variant == 6 ? "boss.chief"
                                              : f.m_variant == 7 ? "boss.foreman"
                                              : f.m_variant == 8 ? "boss.varn"
                                                                 : "boss.taxator" ) )
                           : std::string( TextFormat( tr( "boss.guardian" ), tr( foeKey( (int)f.m_kind ) ) ) );
    float       pw = 640.0f * s, px = w * 0.5f - pw * 0.5f, py = 40.0f * s;
    text( name.c_str(), w * 0.5f, py - 4.0f * s, 24, { 255, 210, 150, 240 } );
    DrawRectangle( (int)px, (int)( py + 26 * s ), (int)pw, (int)( 12 * s ), { 30, 10, 0, 200 } );
    DrawRectangleGradientH( (int)px, (int)( py + 26 * s ), (int)( pw * Clamp( f.m_hp / f.m_hpMax, 0.0f, 1.0f ) ),
                            (int)( 12 * s ), { 200, 60, 20, 255 }, { 255, 170, 60, 255 } );
    if ( f.m_kind == FoeE::TAXATOR )
        for ( float mark : { 0.3f, 0.6f } )
            DrawRectangle( (int)( px + pw * mark ), (int)( py + 24 * s ), (int)( 2 * s ), (int)( 16 * s ),
                           { 255, 255, 255, 160 } );
}

void Game::autoSkills( GameInput &in, const Target &best, float bestDist )
{
    // Points go where they help most (the active skills first, then the hero's strengths), then the skills are used
    // as a player would: a grenade or a quake into a crowd, overcharge on a big one, a roll or a charge to get out.
    static const int order[2][8] = { { 6, 5, 2, 1, 3, 0, 7, 4 }, { 14, 8, 9, 10, 11, 15, 12, 13 } };
    int              hero = m_heroKind == HeroE::DORRA ? 1 : 0;
    while ( m_progress.m_points > 0 )
    {
        int pick = -1;
        for ( int k : order[hero] )
        {
            const SkillDef &def = skillDefs()[k];
            if ( m_progress.m_ranks[k] >= MaxRank || m_progress.m_heroLevel < def.m_level ) continue;
            if ( pick < 0 || m_progress.m_ranks[k] < m_progress.m_ranks[pick] ) pick = k;
        }
        if ( pick < 0 ) break;
        ++m_progress.m_ranks[pick];
        --m_progress.m_points;
        refreshStats();
    }
    if ( best.m_kind != 0 ) return;
    const Foe &t = m_foes[best.m_index];
    int        crowd = 0, close = 0;
    for ( const Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden || !f.m_awake ) continue;
        if ( Vector3Distance( f.m_pos, t.m_pos ) < 3.5f ) ++crowd;
        if ( Vector3Distance( f.m_pos, m_pos ) < 4.0f ) ++close;
    }
    bool big = t.m_elite || t.m_kind == FoeE::TAXATOR || t.m_kind == FoeE::HEAVY;
    if ( hero == 0 )
    {
        if ( ( crowd >= 3 || big ) && bestDist > 4.0f && bestDist < 14.0f ) in.m_skill2 = true;
        if ( big || crowd >= 4 ) in.m_special = true;
        if ( m_hp < hpMax() * 0.35f && close > 0 ) in.m_skill3 = true;
    }
    else
    {
        if ( close >= 3 || ( big && bestDist < 3.5f ) ) in.m_skill2 = true;
        if ( big && bestDist > 4.0f && bestDist < 8.0f ) in.m_skill3 = true;
    }
}

}   // namespace spaceopera
