/**
 * @file GameCoop.cpp
 * @brief Two at one screen (Dark Alliance's way): a second player on a second pad plays one of the other two heroes
 *        beside the first. The partner is the hero's own set of members (position, health, skills, gear, the
 *        autopilot's memory), kept apart and swapped in for its turn each frame and for each enemy that goes for it;
 *        what is not the hero's (the world, scrap, stims, kills) the two share. The first player leads: talks, loot,
 *        doors and lifts are theirs; the partner follows on a leash and is up again beside the leader after a fall.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"
#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <type_traits>
#include <utility>

namespace spaceopera
{

// The hero's members: what the partner has of its own. (A member missing here would be shared between the two.)
#define PARTNER_FIELDS( X )                                                                                            \
    X( m_heroKind )                                                                                                    \
    X( m_progress )                                                                                                    \
    X( m_hero )                                                                                                        \
    X( m_stats )                                                                                                       \
    X( m_pos )                                                                                                         \
    X( m_vel )                                                                                                         \
    X( m_yaw )                                                                                                         \
    X( m_aimYaw )                                                                                                      \
    X( m_aimTimer )                                                                                                    \
    X( m_ground )                                                                                                      \
    X( m_hp )                                                                                                          \
    X( m_heat )                                                                                                        \
    X( m_coolDelay )                                                                                                   \
    X( m_fireTimer )                                                                                                   \
    X( m_invuln )                                                                                                      \
    X( m_hurtFlash )                                                                                                   \
    X( m_overheat )                                                                                                    \
    X( m_firing )                                                                                                      \
    X( m_dead )                                                                                                        \
    X( m_deadTimer )                                                                                                   \
    X( m_respawn )                                                                                                     \
    X( m_target )                                                                                                      \
    X( m_aimDir )                                                                                                      \
    X( m_swing )                                                                                                       \
    X( m_swingT )                                                                                                      \
    X( m_comboNext )                                                                                                   \
    X( m_block )                                                                                                       \
    X( m_boltTimer )                                                                                                   \
    X( m_fireWas )                                                                                                     \
    X( m_motes )                                                                                                       \
    X( m_cloudAt )                                                                                                     \
    X( m_cloudT )                                                                                                      \
    X( m_swarmHits )                                                                                                   \
    X( m_storm )                                                                                                       \
    X( m_stormTick )                                                                                                   \
    X( m_stormRank )                                                                                                   \
    X( m_repair )                                                                                                      \
    X( m_repairRate )                                                                                                  \
    X( m_power )                                                                                                       \
    X( m_setPieces )                                                                                                   \
    X( m_shield )                                                                                                      \
    X( m_shieldWait )                                                                                                  \
    X( m_shieldFlash )                                                                                                 \
    X( m_haste )                                                                                                       \
    X( m_rageT )                                                                                                       \
    X( m_compT )                                                                                                       \
    X( m_rage )                                                                                                        \
    X( m_blastCount )                                                                                                  \
    X( m_secondUsed )                                                                                                  \
    X( m_inPower )                                                                                                     \
    X( m_compPos )                                                                                                     \
    X( m_cool )                                                                                                        \
    X( m_overcharge )                                                                                                  \
    X( m_dash )                                                                                                        \
    X( m_dashLen )                                                                                                     \
    X( m_dodgeCool )                                                                                                   \
    X( m_knockT )                                                                                                      \
    X( m_dashDir )                                                                                                     \
    X( m_dashHit )                                                                                                     \
    X( m_dashKind )                                                                                                    \
    X( m_dashDamage )                                                                                                  \
    X( m_regenStore )                                                                                                  \
    X( m_chill )                                                                                                       \
    X( m_scorch )                                                                                                      \
    X( m_scorchDps )                                                                                                   \
    X( m_corrode )                                                                                                     \
    X( m_fuel )                                                                                                        \
    X( m_fuelMax )                                                                                                     \
    X( m_oxygen )                                                                                                      \
    X( m_oxygenMax )                                                                                                   \
    X( m_inWater )                                                                                                     \
    X( m_outside )                                                                                                     \
    X( m_seekAir )                                                                                                     \
    X( m_metering )                                                                                                    \
    X( m_meterHot )                                                                                                    \
    X( m_hurtBy )                                                                                                      \
    X( m_sinceHurt )                                                                                                   \
    X( m_safeSpot )                                                                                                    \
    X( m_path )                                                                                                        \
    X( m_planTimer )                                                                                                   \
    X( m_stuckTimer )                                                                                                  \
    X( m_runup )                                                                                                       \
    X( m_unstick )                                                                                                     \
    X( m_unstickDir )                                                                                                  \
    X( m_lastPos )                                                                                                     \
    X( m_waypoint )                                                                                                    \
    X( m_wpClose )                                                                                                     \
    X( m_seekCool )                                                                                                    \
    X( m_swapCool )                                                                                                    \
    X( m_stuckAt )                                                                                                     \
    X( m_stuckCount )                                                                                                  \
    X( m_stuckCells )                                                                                                  \
    X( m_jumpFrom )                                                                                                    \
    X( m_jumpTime )                                                                                                    \
    X( m_noJump )                                                                                                      \
    X( m_meleeFoe )                                                                                                    \
    X( m_meleeHp )                                                                                                     \
    X( m_meleeSince )                                                                                                  \
    X( m_meleeSkip )                                                                                                   \
    X( m_stallTime )                                                                                                   \
    X( m_stallMark )

struct Game::PartnerState
{
#define PARTNER_MEMBER( m ) decltype( Game::m ) m{};
    PARTNER_FIELDS( PARTNER_MEMBER )
#undef PARTNER_MEMBER
};

namespace
{

constexpr float Leash = 13.0f;        ///< farther from the leader the partner is put beside them
constexpr float TargetRetry = 1.5f;   ///< seconds before an enemy chooses between the two again

/** @brief One member into the other (arrays too). */
template <class T>
void assign( T &to, const T &from )
{
    if constexpr ( std::is_array_v<T> )
        std::copy( std::begin( from ), std::end( from ), std::begin( to ) );
    else
        to = from;
}

}   // namespace

void Game::swapPartner()
{
    using std::swap;
#define PARTNER_SWAP( m ) swap( m, m_partner->m );
    PARTNER_FIELDS( PARTNER_SWAP )
#undef PARTNER_SWAP
    m_partnerTurn = !m_partnerTurn;
}

HeroE Game::partnerKind() const
{
    return m_partner ? m_partner->m_heroKind : m_heroKind;
}

Vector3 Game::partnerPos() const
{
    return m_partner ? m_partner->m_pos : m_pos;
}

void Game::joinPartner( HeroE kind, bool autopilot )
{
    if ( kind == m_heroKind || kind == HeroE::UNZU || m_heroKind == HeroE::UNZU || m_tale ) return;
    // A fresh set from the leader's (what is not the hero's own stays shared anyway), then made the partner's own.
    m_partner = std::make_shared<PartnerState>();
#define PARTNER_COPY( m ) assign( m_partner->m, m );
    PARTNER_FIELDS( PARTNER_COPY )
#undef PARTNER_COPY
    m_partner->m_hero = Figure();   // its own parts (a copy would share the leader's meshes, and free them)
    m_partnerAuto = autopilot;
    Vector3 leader = m_pos;
    swapPartner();
    m_heroKind = kind;
    m_progress.m_hero = kind;
    m_progress.m_heroLevel = -1;   // gearPartner() makes them at the leader's level
    m_path.clear();
    m_stuckCells.clear();
    m_noJump.clear();
    m_meleeSkip.clear();
    m_dead = false;
    m_motes.clear();
    gearPartner();
    putHero( leader.x + 1.2f, leader.z + 0.8f, m_yaw * RAD2DEG );
    m_respawn = m_pos;
    bodyFigures();
    buildIone();
    Vector3 cam = m_camTarget;
    swapPartner();
    m_camTarget = cam;
    bodyFigures();   // the leader's body again (bodyFigures dresses whoever is the hero)
}

void Game::leavePartner()
{
    m_partner.reset();
    m_partnerTurn = false;
    for ( Foe &f : m_foes ) f.m_onPartner = false;
}

void Game::gearPartner()
{
    // At the leader's level (read before the swap: m_partner holds the leader now): an item of their kind in every
    // slot, the same each time for that level and hero; the points spent as the autopilot spends them.
    int level = m_partner->m_progress.m_heroLevel;
    if ( m_progress.m_heroLevel == level ) return;
    m_progress.m_heroLevel = level;
    m_progress.m_xp = 0;
    m_progress.m_bag.clear();
    m_progress.m_hasSpare = false;
    for ( int i = 0; i < EquipSlots; ++i ) m_progress.m_worn[i] = false;
    unsigned seed = 7919u * (unsigned)level + 104729u * (unsigned)heroIndex();
    for ( int slot = 0; slot <= (int)SlotE::CELL; ++slot )
    {
        Item it = makeItem( level, heroIndex(), seed, 1.5f, slot );
        int  at = gearIndex( it );
        if ( at >= 0 && !m_progress.m_worn[at] ) m_progress.m_gear[at] = it, m_progress.m_worn[at] = true;
    }
    if ( !m_progress.m_worn[0] )
        m_progress.m_gear[0] = taleItem( heroIndex(), SlotE::WEAPON, level ), m_progress.m_worn[0] = true;
    m_progress.m_weapon = m_partner->m_progress.m_weapon;   // the workshop's work is the leader's
    m_progress.m_armor = m_partner->m_progress.m_armor;
    m_progress.m_ranks.assign( skillDefs().size(), 0 );
    m_progress.m_points = level;
    spendPoints();   // refreshes the stats
    dressHero();
    m_hp = hpMax();
    m_shield = m_stats.m_shield;
}

void Game::partnerTurn( float dt )
{
    if ( !m_partner ) return;
    Vector3   leader = m_pos;
    bool      leaderDown = m_dead;
    GameInput in = m_partnerIn;
    // What the story did meanwhile is the partner's to know too.
    size_t flags = m_progress.m_flags.size();
    if ( m_partner->m_progress.m_flags.size() != flags )
        for ( const std::string &f : m_progress.m_flags ) m_partner->m_progress.m_flags.insert( f );
    swapPartner();
    gearPartner();   // the leader has gone up a level
    // Talks, menus, the scenes: the partner waits too.
    if ( m_talk || m_menu || m_scene > 0 || m_finished ) in = GameInput();
    if ( m_partnerAuto )
    {
        // The autopilot fights what is near and otherwise keeps to the leader (it does not talk or lead).
        bool near = false;
        for ( const Foe &f : m_foes )
            near = near || ( f.m_alive && !f.m_hidden && f.m_awake && Vector3Distance( f.m_pos, m_pos ) < 12.0f );
        int  goal = m_talkGoal;
        auto talkTo = m_autoTalkTo;
        bool unstall = m_unstall;
        m_unstall = false;
        if ( near && !m_level.m_peaceful )
            autopilot( dt, in );
        else
        {
            in = GameInput();
            Vector2 to = { leader.x - m_pos.x, leader.z - m_pos.z };
            if ( Vector2Length( to ) > 3.0f ) in.m_move = Vector2Normalize( to );
        }
        m_unstall = unstall;
        m_talkGoal = goal;
        m_autoTalkTo = talkTo;
    }
    else
    {
        float   c = cosf( m_camYaw ), s = sinf( m_camYaw );
        Vector2 m = in.m_move;
        in.m_move = { m.x * c + m.y * s, -m.x * s + m.y * c };
    }
    // Up again beside the leader; on a leash (too far, fallen off, the leader through a door or a lift).
    m_respawn = leader;
    if ( !m_dead && !leaderDown && ( Vector3Distance( m_pos, leader ) > Leash || m_pos.y < -5.0f ) )
    {
        float yaw = m_yaw;
        putHero( leader.x, leader.z, yaw * RAD2DEG );
        for ( float a = 0.0f; a < 2.0f * PI; a += PI / 4.0f )
        {
            Vector3 to = { leader.x + cosf( a ) * 1.4f, leader.y + 1.0f, leader.z + sinf( a ) * 1.4f };
            if ( groundAt( to ) > -100.0f && los( Vector3Add( leader, { 0.0f, 1.0f, 0.0f } ), to ) )
            {
                putHero( to.x, to.z, yaw * RAD2DEG );
                break;
            }
        }
        m_path.clear();
        for ( int i = 0; i < 16; ++i )
            m_particles.push_back(
                { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.6f, rnd() * 1.8f, ( rnd() - 0.5f ) * 0.6f } ),
                  { 0, 0.5f, 0 },
                  0.8f,
                  0.8f,
                  0.02f,
                  { 120, 200, 255, 255 },
                  3 } );
    }
    Vector3 cam = m_camTarget;
    useSkills( dt, in );
    updateHero( dt, in );
    m_camTarget = cam;   // the camera is the leader's (main.cpp frames both)
    // A fall in its turn counted the leader's deaths (they share them); its finds are the leader's.
    swapPartner();
    if ( m_partner->m_progress.m_flags.size() != m_progress.m_flags.size() )
        for ( const std::string &f : m_partner->m_progress.m_flags ) m_progress.m_flags.insert( f );
}

bool Game::foeOnPartner( Foe &f, float dt )
{
    // The nearer of the two who is standing; chosen again now and then, so that they do not flicker between them.
    if ( !m_partner ) return false;
    f.m_pickT -= dt;
    if ( f.m_pickT <= 0.0f )
    {
        f.m_pickT = TargetRetry * ( 0.7f + 0.6f * rnd() );
        float lead = m_dead ? 1e9f : Vector3Distance( f.m_pos, m_pos );
        float mate = m_partner->m_dead ? 1e9f : Vector3Distance( f.m_pos, m_partner->m_pos );
        f.m_onPartner = mate < lead * 0.9f;
    }
    if ( m_partner->m_dead ) f.m_onPartner = false;
    return f.m_onPartner;
}

void Game::posePartner( std::vector<Drawable> &out )
{
    if ( !m_partner ) return;
    swapPartner();
    m_hero.pose( out );
    poseIone( out );
    poseGear( out );
    swapPartner();
}

void Game::drawPartnerHud( int w, int h, float s )
{
    // Over the partner's head: who, the health, the shield.
    if ( !m_partner ) return;
    const PartnerState &p = *m_partner;
    Vector2             at = GetWorldToScreenEx( Vector3Add( p.m_pos, { 0.0f, 2.3f, 0.0f } ), m_cam, w, h );
    if ( at.x < 0.0f || at.y < 0.0f || at.x > w || at.y > h ) return;
    float bw = 90.0f * s, bh = 7.0f * s;
    float x = at.x - bw * 0.5f, y = at.y;
    float hp = p.m_dead ? 0.0f : Clamp( p.m_hp / fmaxf( 1.0f, p.m_stats.m_health ), 0.0f, 1.0f );
    DrawRectangle( (int)x, (int)y, (int)bw, (int)bh, { 0, 0, 0, 160 } );
    DrawRectangle( (int)x, (int)y, (int)( bw * hp ), (int)bh, { 210, 60, 50, 230 } );
    if ( p.m_stats.m_shield > 0.0f )
        DrawRectangle( (int)x, (int)( y + bh + 2.0f * s ),
                       (int)( bw * Clamp( p.m_shield / p.m_stats.m_shield, 0.0f, 1.0f ) ), (int)( 3.0f * s ),
                       { 120, 190, 255, 220 } );
    std::string name = std::string( "P2  " ) + tr( std::string( "hero." ) + heroKey( p.m_heroKind ) );
    if ( p.m_dead ) name += std::string( "  " ) + tr( "coop.down" );
    Vector2 m = MeasureTextEx( m_font, name.c_str(), 16 * s, 1.0f );
    DrawTextEx( m_font, name.c_str(), { at.x - m.x * 0.5f, y - 20 * s }, 16 * s, 1.0f, { 150, 210, 255, 230 } );
}

}   // namespace spaceopera
