/**
 * @file GameFoes2.cpp
 * @brief Act II's enemies: the Concession's men in white and orange (the clearance squads in olive, the works security
 *        in black); flamers who burn at close range, snipers whose laser gives them away, guards hanging under
 *        balloons over the abyss (cut the balloon and they fall), cutters with torches, pump drones that rise out of
 *        bore holes in the ice, turrets, and the clearance squads' hounds.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <cmath>
#include <set>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float FlameReach = 5.5f;     ///< a flamer's tongue
constexpr float FlameHurt = 4.0f;      ///< per tick (five ticks a second)
constexpr float SniperHurt = 26.0f;    ///< one slug after a long aim
constexpr float SniperAim = 1.6f;      ///< seconds the laser rests on the hero
constexpr float BalloonUp = 3.4f;      ///< the balloon over its guard's feet
constexpr float BalloonSize = 1.15f;   ///< its radius

}   // namespace

void Game::buildFoes2()
{
    // A faction's colours for the men: the Concession's white and orange, the clearance squads' olive, the works
    // security's black (their visors too).
    int     armour = m_level.m_faction == "concession"  ? S_CONCESSION
                     : m_level.m_faction == "clearance" ? S_CLEARANCE
                     : m_level.m_faction == "security"  ? S_SECURITY
                     : m_level.m_faction == "collegium" ? S_COLLEGIUM
                     : m_level.m_faction == "citizen"   ? S_CITIZEN
                                                        : -1;
    Vector4 visor = m_level.m_faction == "collegium"   ? Vector4{ 2.6f, 3.0f, 3.6f, 0.0f }
                    : m_level.m_faction == "citizen"   ? Vector4{ 3.4f, 2.2f, 0.6f, 0.0f }
                    : m_level.m_faction == "security"  ? Vector4{ 3.6f, 3.0f, 0.4f, 0.0f }
                    : m_level.m_faction == "clearance" ? Vector4{ 2.5f, 3.2f, 0.6f, 0.0f }
                                                       : Vector4{ 3.8f, 1.6f, 0.3f, 0.0f };
    if ( armour >= 0 )
    {
        FigureSurfaces guard = { armour, S_DARK, S_METAL, S_GLOW };
        guard.m_visor = visor;
        guard.m_weapon = WeaponE::HAMMER;
        m_guardFigure.build( guard );
        FigureSurfaces heavy = { armour, S_DARK, S_METAL, S_GLOW };
        heavy.m_visor = visor;
        heavy.m_weapon = WeaponE::RIFLE;
        heavy.m_scale = { 1.22f * 1.12f, 1.22f, 1.22f * 1.1f };
        m_heavyFigure.build( heavy );
        FigureSurfaces baton = { armour, S_DARK, S_METAL, S_GLOW };
        baton.m_visor = visor;
        baton.m_weapon = WeaponE::BATON;
        m_boarderBaton.build( baton );
        FigureSurfaces gun = baton;
        gun.m_weapon = WeaponE::RIFLE;
        m_boarderGun.build( gun );
    }
    // Okafor's walker: olive plates, broad and tall, a rifle arm; the boarding leader's exoskeleton: the Concession's
    // white, a cutting torch for a fist.
    FigureSurfaces okafor = { S_CLEARANCE, S_DARK, S_METAL, S_GLOW };
    okafor.m_visor = { 2.5f, 3.2f, 0.6f, 0.0f };
    okafor.m_weapon = WeaponE::RIFLE;
    okafor.m_scale = { 2.2f * 1.2f, 2.2f, 2.2f * 1.1f };
    m_okaforFigure.build( okafor );
    FigureSurfaces leader = { S_CONCESSION, S_DARK, S_METAL, S_GLOW };
    leader.m_visor = { 3.8f, 1.6f, 0.3f, 0.0f };
    leader.m_weapon = WeaponE::HAMMER;
    leader.m_scale = { 2.0f * 1.2f, 2.0f, 2.0f * 1.1f };
    m_leaderFigure.build( leader );
    FigureSurfaces salvager = { S_HAZARD, S_DARK, S_METAL, S_GLOW };
    salvager.m_visor = { 3.6f, 2.8f, 0.4f, 0.0f };
    salvager.m_weapon = WeaponE::RIFLE;
    salvager.m_scale = { 2.1f * 1.2f, 2.1f, 2.1f * 1.1f };
    m_salvagerFigure.build( salvager );
    // Act III and IV: Teodor Lamm in his arena frame (crimson, a hammer), the cleaners' chief (the Collegium's
    // grey, a rifle), the foreman's crane walker (rust, bigger than all), Ysolde's frame (violet, a blade).
    FigureSurfaces lamm = { S_LAMM, S_TAXATOR, S_METAL, S_GLOW };
    lamm.m_visor = { 4.0f, 2.6f, 0.8f, 0.0f };
    lamm.m_weapon = WeaponE::HAMMER;
    lamm.m_scale = { 1.9f * 1.15f, 1.9f, 1.9f * 1.1f };
    m_lammFigure.build( lamm );
    FigureSurfaces chief = { S_COLLEGIUM, S_DARK, S_METAL, S_GLOW };
    chief.m_visor = { 2.6f, 3.0f, 3.6f, 0.0f };
    chief.m_weapon = WeaponE::RIFLE;
    chief.m_scale = { 2.0f * 1.15f, 2.0f, 2.0f * 1.1f };
    m_chiefFigure.build( chief );
    FigureSurfaces foreman = { S_FOREMAN, S_DARK, S_RUST, S_GLOW };
    foreman.m_visor = { 3.8f, 2.0f, 0.3f, 0.0f };
    foreman.m_weapon = WeaponE::HAMMER;
    foreman.m_scale = { 2.5f * 1.25f, 2.5f, 2.5f * 1.15f };
    m_foremanFigure.build( foreman );
    FigureSurfaces varn = { S_VARN, S_DARK, S_METAL, S_GLOW };
    varn.m_visor = { 3.0f, 1.2f, 3.6f, 0.0f };
    varn.m_weapon = WeaponE::BATON;
    varn.m_scale = { 1.8f * 1.1f, 1.8f, 1.8f * 1.05f };
    m_varnFigure.build( varn );
    // The hounds: big low runners in dark plates.
    m_hound.build( S_DARK, S_CLEARANCE, S_GLOW, false, 1.35f );
    // The computing moon's maintenance robots: white, quick, on many legs.
    m_mender.build( S_MENDER, S_METAL, S_GLOW, false, 1.05f );
    // A turret: a squat base, a head with two barrels.
    MeshBuilder base;
    base.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.7f, 0.5f, 16 );
    base.cylinder( place( { 0.0f, 0.5f, 0.0f } ), 0.35f, 0.6f, 12 );
    m_turretBase = base.build().front();
    MeshBuilder head;
    head.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 0.55f, 0.32f, 0.6f }, 0.12f, 2 );
    head.box( place( { -0.16f, 0.05f, 0.9f } ), { 0.07f, 0.07f, 0.55f } );
    head.box( place( { 0.16f, 0.05f, 0.9f } ), { 0.07f, 0.07f, 0.55f } );
    m_turretHead = head.build().front();
    // A guard's balloon: an envelope on four lines over a little platform.
    MeshBuilder balloon;
    balloon.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { BalloonSize, BalloonSize * 0.9f, BalloonSize * 1.25f }, 0.85f,
                        3 );
    m_guardBalloon = balloon.build().front();
    MeshBuilder pallet;
    pallet.box( place( { 0.0f, -0.08f, 0.0f } ), { 0.65f, 0.06f, 0.65f } );
    for ( float x : { -0.6f, 0.6f } )
        for ( float z : { -0.6f, 0.6f } )
        {
            Vector3 a = { x, 0.0f, z }, e = { x * 0.4f, BalloonUp - BalloonSize * 0.7f, z * 0.4f };
            Vector3 mid = Vector3Lerp( a, e, 0.5f );
            float   len = Vector3Distance( a, e );
            Vector3 dz = Vector3Normalize( Vector3Subtract( e, a ) );
            Vector3 dx = Vector3Normalize( Vector3CrossProduct( { 0.0f, 0.0f, 1.0f }, dz ) );
            pallet.box( frame( dx, Vector3CrossProduct( dz, dx ), dz, mid ), { 0.012f, 0.012f, len * 0.5f } );
        }
    m_guardPallet = pallet.build().front();
}

bool Game::spawnMarker( char kind, Vector3 p )
{
    // The later symbols: who stands where, as what.
    auto variant = [&]( FoeE k, int v, Vector3 at )
    {
        spawnFoe( k, at, false );
        m_foes.back().m_variant = v;
    };
    switch ( kind )
    {
    case '}': variant( FoeE::GUNNER, 2, p ); return true;
    case '\'': variant( FoeE::GUNNER, 3, p ); return true;
    case '{':
        // Under its balloon, hovering at the deck's height beside it.
        variant( FoeE::GUNNER, 4, p );
        m_foes.back().m_home = p;
        return true;
    case '"':
        variant( FoeE::BATON, 1, p );
        m_foes.back().m_hp *= 1.4f, m_foes.back().m_hpMax = m_foes.back().m_hp;
        return true;
    case '`':
        variant( FoeE::HEAVY, 1, p );
        m_foes.back().m_hp *= 0.7f, m_foes.back().m_hpMax = m_foes.back().m_hp;
        return true;
    case '-': variant( FoeE::SAW, 1, p ); return true;
    case '(':
        // A bore hole: a pump drone waits under the ice until the hero comes near.
        variant( FoeE::DRONE, 1, Vector3Add( p, { 0.0f, -1.2f, 0.0f } ) );
        m_foes.back().m_hidden = true;
        m_boreHoles.push_back( p );
        return true;
    case '0': m_pickups.push_back( { 3, 1, p, { 0, 0, 0 } } ); return true;
    default: return false;
    }
}

bool Game::updateVariant( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    if ( f.m_variant == 0 ) return false;
    fire( foeNoun( f ) + 4 );   // the first one met: its hint ("flamer", "sniper", ...)
    if ( f.m_kind == FoeE::DRONE && f.m_variant >= 5 ) return updateSwarmUnit( f, dt, dir, dist, path, want, faceYaw );
    if ( f.m_kind == FoeE::SAW && f.m_variant == 3 ) return false;   // a robot runs in like a saw bot
    if ( f.m_kind == FoeE::GUNNER && f.m_variant == 2 )
    {
        // The flamer: closes in, then a tongue of fire for a second and a half, a pause to cool.
        if ( f.m_burst > 0 )
        {
            faceYaw = atan2f( dir.x, dir.z );
            want = Vector3Scale( path, 0.8f );
            if ( f.m_timer <= 0.0f )
            {
                --f.m_burst;
                f.m_timer = 0.2f;
                Vector3 muzzle = Vector3Add( f.m_pos, { sinf( f.m_yaw ) * 0.8f, 1.25f, cosf( f.m_yaw ) * 0.8f } );
                for ( int k = 0; k < 6; ++k )
                {
                    float a = f.m_yaw + ( rnd() - 0.5f ) * 0.5f, v = 7.0f + rnd() * 4.0f;
                    m_particles.push_back( { muzzle,
                                             { sinf( a ) * v, 0.3f + rnd(), cosf( a ) * v },
                                             0.55f,
                                             0.55f,
                                             0.16f + rnd() * 0.1f,
                                             { 255, (unsigned char)( 120 + rnd() * 80 ), 40, 255 },
                                             3 } );
                }
                m_flashes.push_back( { muzzle, { 14.0f, 6.0f, 1.5f }, 5.0f, 0.2f, 0.2f } );
                bool cone = ( sinf( f.m_yaw ) * dir.x + cosf( f.m_yaw ) * dir.z ) > 0.85f;
                if ( dist < FlameReach && cone && f.m_los )
                {
                    m_hurtBy = "fire";
                    hurtHero( FlameHurt * m_hitScale, { dir.x * 0.6f, 0.0f, dir.z * 0.6f }, f.m_pos );
                }
                if ( f.m_burst == 0 ) f.m_spit = 1.8f;
            }
            return true;
        }
        f.m_spit -= dt;
        want = Vector3Scale( path, dist > 4.0f ? 3.0f : 0.6f );
        faceYaw = dist < 6.0f ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
        if ( dist < FlameReach - 0.5f && f.m_los && f.m_spit <= 0.0f ) f.m_burst = 8, f.m_timer = 0.15f;
        return true;
    }
    if ( f.m_kind == FoeE::GUNNER && f.m_variant == 3 )
    {
        // The sniper: keeps far off, rests a laser on the hero for long, then one heavy, fast slug.
        f.m_spit -= dt;
        if ( f.m_aim > 0.0f )
        {
            faceYaw = atan2f( dir.x, dir.z );
            f.m_aim += dt / SniperAim;
            if ( !f.m_los ) f.m_aim = 0.0f;
            if ( f.m_aim >= 1.0f )
            {
                Vector3 muzzle = Vector3Add( f.m_pos, { sinf( f.m_yaw ) * 0.8f, 1.4f, cosf( f.m_yaw ) * 0.8f } );
                Vector3 aim = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
                Vector3 v = Vector3Scale( Vector3Normalize( Vector3Subtract( aim, muzzle ) ), 60.0f );
                m_shotsInFlight.push_back( { muzzle, v, 1.2f, SniperHurt * m_hitScale } );
                m_flashes.push_back( { muzzle, { 26.0f, 10.0f, 3.0f }, 6.0f, 0.08f, 0.08f } );
                m_audio.play( SfxE::GUNSHOT, muzzle, m_camTarget, 0.7f );
                f.m_aim = 0.0f;
                f.m_spit = 2.5f + rnd();
            }
            return true;
        }
        float want_d = 20.0f;
        want = dist < want_d - 3.0f ? Vector3Scale( dir, -2.2f )
                                    : ( dist > want_d + 6.0f ? Vector3Scale( path, 2.6f ) : Vector3{ 0, 0, 0 } );
        faceYaw = atan2f( dir.x, dir.z );
        if ( f.m_los && f.m_spit <= 0.0f && dist < 32.0f ) f.m_aim = 0.01f;
        return true;
    }
    if ( f.m_kind == FoeE::GUNNER && f.m_variant == 4 )
    {
        // Under its balloon it hangs and shoots (the carbine's bursts); fallen, it is a gunner like any.
        if ( f.m_stage == 0 )
        {
            updateBoarder( f, dt, dir, dist, path, want, faceYaw );
            want = { m_level.m_windDir.x * 0.4f, ( f.m_home.y - f.m_pos.y ) * 2.0f, m_level.m_windDir.z * 0.4f };
            Vector3 off = Vector3Subtract( f.m_pos, f.m_home );
            want.x -= off.x * 0.5f, want.z -= off.z * 0.5f;
            return true;
        }
        return false;
    }
    if ( f.m_kind == FoeE::BATON && f.m_variant == 1 )
    {
        // The cutter: a torch instead of a baton, quicker on its feet, and it bites through plate.
        float scale = m_hitScale;
        m_hitScale *= 1.5f;
        updateBoarder( f, dt, dir, dist, path, want, faceYaw );
        m_hitScale = scale;
        want = Vector3Scale( want, 1.2f );
        if ( rnd() < dt * 8.0f )
            m_particles.push_back( { Vector3Add( f.m_pos, { sinf( f.m_yaw ) * 0.6f, 1.1f, cosf( f.m_yaw ) * 0.6f } ),
                                     { ( rnd() - 0.5f ) * 1.5f, 1.0f, ( rnd() - 0.5f ) * 1.5f },
                                     0.3f,
                                     0.3f,
                                     0.03f,
                                     { 255, 220, 140, 255 },
                                     0 } );
        return true;
    }
    if ( f.m_kind == FoeE::HEAVY && f.m_variant == 1 )
    {
        // A turret turns and fires; it does not move.
        updateHeavy( f, dt, dir, dist, path, want, faceYaw );
        want = { 0.0f, 0.0f, 0.0f };
        return true;
    }
    if ( f.m_kind == FoeE::SAW && f.m_variant == 1 )
    {
        // A hound: a saw bot's way of hunting, faster and with teeth.
        float scale = m_hitScale;
        m_hitScale *= 1.8f;
        updateCritter( f, dt, dir, dist, path, want, faceYaw );
        m_hitScale = scale;
        want = Vector3Scale( want, 1.5f );
        return true;
    }
    return false;
}

void Game::updateBoreHoles()
{
    // A pump drone rises out of its hole when the hero comes near.
    for ( Foe &f : m_foes )
        if ( f.m_alive && f.m_hidden && f.m_kind == FoeE::DRONE && f.m_variant == 1 &&
             Vector2Distance( { f.m_pos.x, f.m_pos.z }, { m_pos.x, m_pos.z } ) < 11.0f )
        {
            f.m_hidden = false;
            f.m_awake = true;
            f.m_pos.y = 0.3f;
            for ( int k = 0; k < 12; ++k )
                m_particles.push_back(
                    { Vector3Add( f.m_pos, { ( rnd() - 0.5f ) * 0.8f, 0.0f, ( rnd() - 0.5f ) * 0.8f } ),
                      { ( rnd() - 0.5f ) * 2.0f, 3.0f + rnd() * 2.0f, ( rnd() - 0.5f ) * 2.0f },
                      0.9f,
                      0.9f,
                      0.05f,
                      { 190, 225, 255, 220 },
                      0 } );
            m_audio.play( SfxE::SIZZLE, f.m_pos, m_camTarget, 0.6f );
            fire( "pumpdrone" );
        }
}

void Game::popBalloon( int i )
{
    Foe &f = m_foes[i];
    if ( f.m_stage != 0 ) return;
    f.m_stage = 1;
    f.m_ground = false;
    f.m_vel = { 0.0f, 0.0f, 0.0f };
    Vector3 at = Vector3Add( f.m_pos, { 0.0f, BalloonUp, 0.0f } );
    sparks( at, { 0.0f, 1.0f, 0.0f }, 14, { 255, 230, 200, 255 }, 3.0f );
    m_audio.play( SfxE::POP, at, m_camTarget, 0.9f );
    fire( "balloonpop" );
}

int Game::balloonOnRay( Vector3 from, Vector3 dir, float &t ) const
{
    int best = -1;
    for ( int i = 0; i < (int)m_foes.size(); ++i )
    {
        const Foe &f = m_foes[i];
        if ( !f.m_alive || f.m_hidden || f.m_kind != FoeE::GUNNER || f.m_variant != 4 || f.m_stage != 0 ) continue;
        float u = raySphere( from, dir, Vector3Add( f.m_pos, { 0.0f, BalloonUp, 0.0f } ), BalloonSize );
        if ( u >= 0.0f && u < t ) t = u, best = i;
    }
    return best;
}

void Game::poseVariants( std::vector<Drawable> &out )
{
    for ( const Foe &f : m_foes )
    {
        if ( !f.m_alive ) continue;
        if ( f.m_kind == FoeE::GUNNER && f.m_variant == 4 && f.m_stage == 0 && !f.m_hidden )
        {
            Drawable d;
            d.m_mesh = m_guardBalloon;
            d.m_surface = S_CLOTH2;
            d.m_transform = MatrixTranslate( f.m_pos.x, f.m_pos.y + BalloonUp, f.m_pos.z );
            out.push_back( d );
            d.m_mesh = m_guardPallet;
            d.m_surface = S_METAL;
            d.m_transform = MatrixTranslate( f.m_pos.x, f.m_pos.y, f.m_pos.z );
            out.push_back( d );
        }
        if ( f.m_kind == FoeE::HEAVY && f.m_variant == 1 && !f.m_hidden )
        {
            Drawable d;
            d.m_mesh = m_turretBase;
            d.m_surface = S_METAL;
            d.m_transform = MatrixTranslate( f.m_pos.x, f.m_pos.y, f.m_pos.z );
            out.push_back( d );
            d.m_mesh = m_turretHead;
            d.m_surface = f.m_flash > 0.0f ? S_GLOW : S_DARK;
            d.m_emissive = f.m_flash > 0.0f ? Vector4{ 3.0f, 2.0f, 1.5f, 0.0f } : Vector4{ -1, 0, 0, 0 };
            d.m_transform =
                MatrixMultiply( MatrixRotateY( f.m_yaw ), MatrixTranslate( f.m_pos.x, f.m_pos.y + 1.25f, f.m_pos.z ) );
            out.push_back( d );
        }
    }
    // The bore holes in the ice: dark rings where the pumps reach down.
    for ( const Vector3 &p : m_boreHoles )
    {
        Drawable d;
        d.m_mesh = m_ring;
        d.m_surface = S_DARK;
        d.m_transform = MatrixMultiply( MatrixScale( 1.1f, 1.0f, 1.1f ), MatrixTranslate( p.x, 0.02f, p.z ) );
        out.push_back( d );
    }
}

const char *Game::foeNoun( const Foe &f ) const
{
    if ( f.m_kind == FoeE::GUNNER && f.m_variant )
        return f.m_variant == 1   ? "foe.shocker"
               : f.m_variant == 2 ? "foe.flamer"
               : f.m_variant == 3 ? "foe.sniper"
                                  : "foe.balloon";
    if ( f.m_kind == FoeE::BATON && f.m_variant ) return "foe.cutter";
    if ( f.m_kind == FoeE::HEAVY && f.m_variant ) return "foe.turret";
    if ( f.m_kind == FoeE::SAW && f.m_variant == 3 ) return "foe.mender";
    if ( f.m_kind == FoeE::SAW && f.m_variant ) return "foe.hound";
    if ( f.m_kind == FoeE::DRONE && f.m_variant == 5 ) return "foe.swarmling";
    if ( f.m_kind == FoeE::DRONE && f.m_variant == 6 ) return "foe.hive";
    if ( f.m_kind == FoeE::DRONE && f.m_variant ) return "foe.pumpdrone";
    // A faction's men by its own name (the Concession's guards and the Collegium's cleaners are no Skimmers).
    if ( !m_level.m_faction.empty() && ( f.m_kind == FoeE::BATON || f.m_kind == FoeE::GUNNER ||
                                         f.m_kind == FoeE::SHIELD || f.m_kind == FoeE::HEAVY ) )
    {
        static std::set<std::string> named;
        static const char *const     base[] = { "", "", "foe.baton", "foe.gunner", "foe.shield", "foe.heavy" };
        std::string                  key = std::string( base[(int)f.m_kind] ) + "." + m_level.m_faction;
        if ( hasText( key ) ) return named.insert( key ).first->c_str();
    }
    static const char *keys[] = { "foe.louse", "foe.welder", "foe.baton",  "foe.gunner",  "foe.shield", "foe.heavy",
                                  "foe.drone", "foe.saw",    "foe.bomber", "foe.taxator", "foe.shell" };
    return keys[(int)f.m_kind];
}

}   // namespace spaceopera
