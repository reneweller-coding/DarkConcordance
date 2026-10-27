/**
 * @file GameTales.cpp
 * @brief The companions and their tales. In the hubs the two heroes one does not play sit at the tables and talk; a
 *        conversation can take everyone into a tale ("go:<level>"): a short level played as the one who tells it -
 *        Dorra's war chronicle, Corvin's confession, Ione's incident report, Unzu's survey log - in the tale's own
 *        light (a grade, rain), and then back to the table. What the real hero carries waits outside the tale; Unzu's
 *        survey drone flies for his log.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "Items.h"
#include "MeshBuilder.h"
#include "Rig.h"
#include "Skills.h"
#include "Text.h"

#include <raymath.h>

#include <cmath>
#include <set>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float JamReach = 7.0f;      ///< Unzu's jammer: how far
constexpr float JamCooldown = 7.0f;   ///< ... and how often
constexpr float SeatReach = 14.0f;    ///< a companion sits on a free stool this near their place

HeroE heroOf( const std::string &key )
{
    if ( key == "dorra" ) return HeroE::DORRA;
    if ( key == "ione" ) return HeroE::IONE;
    if ( key == "unzu" ) return HeroE::UNZU;
    return HeroE::CORVIN;
}

}   // namespace

void Game::loadTale()
{
    std::string teller = m_level.word( "teller" );
    if ( teller.empty() ) return;
    // Played as the one who tells it, with the gear of their kind at the real hero's strength; the points spent as
    // they would have spent them. What the real hero has waits (progress() gives it back).
    m_tale = true;
    m_realHero = m_heroKind;
    m_realProgress = m_progress;
    m_heroKind = heroOf( teller );
    Progress p;
    p.m_hero = m_heroKind;
    p.m_level = m_realProgress.m_level;
    p.m_heroLevel = m_realProgress.m_heroLevel;
    p.m_points = m_heroKind == HeroE::UNZU ? 0 : m_realProgress.m_heroLevel;   // the drone learns nothing
    p.m_weapon = m_realProgress.m_weapon;
    p.m_armor = m_realProgress.m_armor;
    p.m_flags = m_realProgress.m_flags;
    p.m_seed = m_realProgress.m_seed;
    p.m_deaths = m_realProgress.m_deaths;
    p.m_stims = 3;
    int hero = heroIndex();
    p.m_gear[0] = taleItem( hero, SlotE::WEAPON, p.m_heroLevel ), p.m_worn[0] = true;
    p.m_gear[1] = taleItem( hero, SlotE::ARMOR, p.m_heroLevel ), p.m_worn[1] = true;
    p.m_ranks.assign( skillDefs().size(), 0 );
    m_progress = p;
    spendPoints();
}

Progress Game::taleProgress( const Progress &tale ) const
{
    // Back at the table: the real hero as they were, with what the tale left behind (what happened, half its
    // experience); where the story goes on.
    Progress p = m_realProgress;
    p.m_hero = m_realHero;
    for ( const std::string &f : tale.m_flags ) p.m_flags.insert( f );
    p.m_xp += m_taleXp / 2;
    p.m_level = tale.m_level;
    return p;
}

void Game::spendPoints()
{
    // Points go where they help most: the active skills first, then the hero's strengths (the autopilot's order too).
    static const int order[3][8] = { { 6, 5, 2, 1, 3, 0, 7, 4 },
                                     { 14, 8, 9, 10, 11, 15, 12, 13 },
                                     { 21, 22, 16, 17, 18, 23, 19, 20 } };
    int              hero = heroIndex();
    if ( hero > 2 ) return;
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
    }
    refreshStats();
}

bool Game::companion( const std::string &look )
{
    return look == "dorra" || look == "corvin" || look == "ione";
}

void Game::seatCompanions( std::set<size_t> &taken )
{
    // The two not played take the free stools nearest their places, the second beside the first (at one table);
    // the one played is not there twice.
    bool    first = true;
    Vector3 anchor = { 0.0f, 0.0f, 0.0f };
    for ( size_t i = 0; i < m_level.m_npcs.size(); ++i )
    {
        const Npc &n = m_level.m_npcs[i];
        if ( !companion( n.m_look ) || n.m_row < 0 || n.m_look == heroKey( m_tale ? m_realHero : m_heroKind ) )
            continue;
        Vector3 at = first ? m_level.centre( n.m_row, n.m_col ) : anchor;
        int     best = -1;
        float   bestDist = SeatReach;
        for ( size_t k = 0; k < m_level.m_seats.size(); ++k )
        {
            float d = Vector2Distance( { at.x, at.z }, { m_level.m_seats[k].m_pos.x, m_level.m_seats[k].m_pos.z } );
            if ( !taken.count( k ) && d < bestDist ) bestDist = d, best = (int)k;
        }
        if ( best < 0 ) continue;
        taken.insert( (size_t)best );
        m_companionSeat[n.m_id] = best;
        if ( first ) anchor = m_level.m_seats[best].m_pos, first = false;
    }
}

void Game::dressCompanion( NpcState &s, const std::string &look )
{
    // As the heroes look out of their suits: Dorra short, broad, red-haired in ochre; Corvin dark-haired in grey;
    // Ione thin, black-haired, in her grey-blue coverall.
    struct Look
    {
        const char *m_key;
        int         m_body, m_hair, m_colour, m_tone;
        Vector3     m_cloth;
        float       m_scale;
    };
    static const Look looks[] = { { "dorra", 1, 2, 3, 1, { 0.55f, 0.42f, 0.16f }, 0.94f },
                                  { "corvin", 0, 0, 1, 1, { 0.24f, 0.25f, 0.27f }, 1.0f },
                                  { "ione", 1, 0, 0, 1, { 0.3f, 0.38f, 0.45f }, 0.97f } };
    for ( const Look &l : looks )
        if ( look == l.m_key )
        {
            s.m_figure.m_body = l.m_body;
            s.m_figure.m_hair = l.m_hair;
            s.m_figure.m_hairColour = l.m_colour;
            s.m_figure.m_tone = l.m_tone;
            s.m_figure.m_cloth = l.m_cloth;
            s.m_figure.m_scale = l.m_scale;
        }
    auto seat = m_companionSeat.find( m_level.m_npcs[s.m_def].m_id );
    if ( seat == m_companionSeat.end() ) return;
    const Seat &st = m_level.m_seats[seat->second];
    s.m_pos = st.m_pos;
    s.m_yaw = st.m_yaw;
    s.m_figure.m_chair = 1.0f;
    s.m_figure.m_seat = st.m_height;
    s.m_seated = true;
}

void Game::buildUnzu()
{
    if ( m_heroKind != HeroE::UNZU ) return;
    // The survey drone: a flat hull, a sensor eye in front, two blade arms folded at the sides, four rotor rings.
    MeshBuilder body;
    body.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 0.42f, 0.12f, 0.5f }, 0.08f, 2 );
    body.roundedBox( place( { 0.0f, 0.13f, -0.1f } ), { 0.22f, 0.07f, 0.22f }, 0.05f, 2 );
    for ( float sx : { -1.0f, 1.0f } )
        for ( float sz : { -1.0f, 1.0f } )
            body.lathe( place( { sx * 0.55f, 0.04f, sz * 0.52f } ), { { 0.18f, -0.02f }, { 0.2f, 0.02f } }, 16 );
    m_unzuBody = body.build().front();
    MeshBuilder eye;
    eye.roundedBox( place( { 0.0f, 0.02f, 0.5f } ), { 0.12f, 0.07f, 0.03f }, 0.03f, 2 );
    m_unzuEye = eye.build().front();
    MeshBuilder blades;
    for ( float sx : { -1.0f, 1.0f } )
        blades.box( place( { sx * 0.5f, -0.06f, 0.2f }, { 0.0f, sx * 0.3f, 0.0f } ), { 0.025f, 0.05f, 0.42f } );
    m_unzuBlades = blades.build().front();
}

void Game::updateUnzu( float dt, const GameInput &in )
{
    // The jammer: every machine and suit close by stands for a moment.
    m_boltTimer -= dt;
    if ( !in.m_special || m_boltTimer > 0.0f || m_scene || m_level.m_peaceful ) return;
    m_boltTimer = JamCooldown;
    for ( Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden || Vector3Distance( f.m_pos, m_pos ) > JamReach ) continue;
        bool big = f.m_kind == FoeE::TAXATOR || f.m_elite == 2;
        f.m_stun = fmaxf( f.m_stun, big ? 0.7f : 2.0f );
        f.m_swingT = -1.0f;
        f.m_aim = 0.0f;
        f.m_burst = 0;
        sparks( Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ), { 0.0f, 1.0f, 0.0f }, 8, { 255, 200, 90, 255 },
                2.0f );
    }
    for ( int i = 0; i < 48; ++i )
    {
        float a = i * PI * 2.0f / 48.0f;
        m_particles.push_back( { Vector3Add( m_pos, { 0.0f, 1.1f, 0.0f } ),
                                 { cosf( a ) * JamReach * 1.8f, 0.0f, sinf( a ) * JamReach * 1.8f },
                                 0.5f,
                                 0.5f,
                                 0.02f,
                                 { 255, 190, 80, 255 },
                                 3 } );
    }
    m_audio.play( SfxE::HACK, m_pos, m_camTarget, 0.9f );
    fire( "jam" );
}

void Game::poseUnzu( std::vector<Drawable> &out )
{
    if ( m_dead && m_deadTimer > 0.4f ) return;
    // It hovers a little above the floor, leans into where it flies, its eye amber.
    float    bob = 0.08f * sinf( m_time * 2.3f );
    float    lean = Clamp( Vector2Length( { m_vel.x, m_vel.z } ) * 0.05f, 0.0f, 0.3f );
    Matrix   m = MatrixMultiply( MatrixMultiply( MatrixRotateX( lean ), MatrixRotateY( m_yaw ) ),
                                 MatrixTranslate( m_pos.x, m_pos.y + 1.1f + bob, m_pos.z ) );
    Drawable d;
    d.m_transform = m;
    d.m_mesh = m_unzuBody;
    d.m_surface = S_METAL;
    out.push_back( d );
    d.m_mesh = m_unzuBlades;
    d.m_surface = S_DARK;
    out.push_back( d );
    d.m_mesh = m_unzuEye;
    d.m_surface = S_GLOW;
    float pulse = 0.8f + 0.2f * sinf( m_time * 5.0f );
    d.m_emissive = { 3.0f * pulse, 1.7f * pulse, 0.3f * pulse, 0.0f };
    d.m_sunShadow = d.m_spotShadow = false;
    out.push_back( d );
}

void Game::updateTale( float dt )
{
    // Unzu's census: everyone he flies close to is counted, once.
    int want = (int)m_level.num( "census" );
    if ( want > 0 && !m_fired.count( "census" ) )
    {
        for ( size_t i = 0; i < m_people.size(); ++i )
            if ( !m_counted.count( i ) && m_people[i].m_down < 0.5f &&
                 Vector3Distance( m_people[i].m_pos, m_pos ) < 3.2f )
            {
                m_counted.insert( i );
                sparks( Vector3Add( m_people[i].m_pos, { 0.0f, 1.9f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 6,
                        { 255, 200, 90, 255 }, 1.5f );
                m_audio.play( SfxE::PICKUP, m_people[i].m_pos, m_camTarget, 0.35f );
                if ( m_counted.size() == 1 ) fire( "counted" );
            }
        if ( (int)m_counted.size() >= want ) fire( "census" );
    }
    // Rain where the tale has it: streaks out of the dark onto the streets.
    float rain = m_level.num( "rain" );
    if ( rain <= 0.0f ) return;
    m_rainT += dt * rain;
    while ( m_rainT >= 1.0f )
    {
        m_rainT -= 1.0f;
        Vector3 p =
            Vector3Add( m_camTarget, { ( rnd() - 0.5f ) * 30.0f, 9.0f + rnd() * 4.0f, ( rnd() - 0.5f ) * 30.0f } );
        m_particles.push_back( { p, { 1.2f, -16.0f, 0.4f }, 0.9f, 0.9f, 0.004f, { 70, 80, 95, 255 }, 0 } );
    }
}

void Game::drawTaleHud( int w, int, float s )
{
    int want = (int)m_level.num( "census" );
    if ( want <= 0 ) return;
    const char *t = TextFormat( tr( "hud.census" ), std::min( (int)m_counted.size(), want ), want );
    Vector2     m = MeasureTextEx( m_font, t, 22 * s, 1.0f );
    DrawTextEx( m_font, t, { w - m.x - 40 * s + 2 * s, 92 * s + 2 * s }, 22 * s, 1.0f, { 0, 0, 0, 200 } );
    DrawTextEx( m_font, t, { w - m.x - 40 * s, 92 * s }, 22 * s, 1.0f,
                m_fired.count( "census" ) ? Color{ 255, 210, 120, 255 } : Color{ 230, 225, 210, 235 } );
}

void Game::gradeFor( Gfx &gfx ) const
{
    // A tale's own light (identity elsewhere): colour, contrast, grain, vignette, a tint.
    gfx.m_grade = { m_level.num( "grade", 0, 1.0f ), m_level.num( "grade", 1, 1.0f ), m_level.num( "grade", 2, 0.0f ),
                    m_level.num( "grade", 3, 0.0f ) };
    gfx.m_gradeTint = { m_level.num( "grade", 4, 1.0f ), m_level.num( "grade", 5, 1.0f ),
                        m_level.num( "grade", 6, 1.0f ) };
}

}   // namespace spaceopera
