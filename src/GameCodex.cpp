/**
 * @file GameCodex.cpp
 * @brief The codex and the awards (two pages of the inventory): the creatures, the foes and the bosses met with what
 *        is known of them and how many were felled, the notes read; the awards won and how far the others are. A
 *        new award shows as a banner. What is met and won is kept across all games (journal.txt).
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"
#include "Journal.h"

#include <raymath.h>

#include <algorithm>
#include <cstring>
#include <iterator>

namespace spaceopera
{

using namespace play;

namespace
{

/** @brief The codex's entries: the creature families, the foes, the bosses (their name keys). */
const char *const Families[] = { "foe.crystal", "foe.shade",  "foe.eel",    "foe.diver", "foe.beast",
                                 "foe.vine",    "foe.frost",  "foe.spider", "foe.golem", "foe.magma",
                                 "foe.phantom", "foe.glitch", "foe.crab" };
const char *const Foes[] = { "foe.louse",     "foe.welder",    "foe.baton",   "foe.gunner", "foe.shocker", "foe.shield",
                             "foe.heavy",     "foe.drone",     "foe.saw",     "foe.bomber", "foe.taxator", "foe.shell",
                             "foe.flamer",    "foe.sniper",    "foe.balloon", "foe.cutter", "foe.turret",  "foe.hound",
                             "foe.pumpdrone", "foe.swarmling", "foe.hive",    "foe.mender" };
const char *const Bosses[] = { "boss.press",    "boss.taxator",      "boss.tug",           "boss.hydraulic",
                               "boss.mother",   "boss.rig",          "boss.bell",          "boss.okafor",
                               "boss.leader",   "boss.salvager",     "boss.lance",         "boss.lamm",
                               "boss.chief",    "boss.swarm.drones", "boss.swarm.menders", "boss.swarm.archon",
                               "boss.platform", "boss.column",       "boss.catcher",       "boss.foreman",
                               "boss.varn",     "boss.swarm.delete", "boss.dreier",        "boss.siel",
                               "boss.brask",    "boss.editor" };

/** @brief A codex key from a foe's noun (a creature's tier left off: foe.crystal.2 is foe.crystal). */
std::string entryOf( const char *noun )
{
    std::string key = noun;
    if ( key.size() > 2 && key[key.size() - 2] == '.' && isdigit( (unsigned char)key.back() ) )
        key.resize( key.size() - 2 );
    return key;
}

/** @brief A boss's name as the codex lists it (the bars shout in capitals: "DER FÄNGER" stays so). */
std::string bossName( const char *key )
{
    return tr( key );
}

}   // namespace

void Game::meetFoe( const Foe &f )
{
    if ( f.m_hidden ) return;
    std::string key = entryOf( foeNoun( f ) );
    if ( !journal::meet( key ) ) return;
    checkCodex();
}

void Game::countKill( const Foe &f )
{
    meetFoe( f );
    journal::count( "kills" );
    journal::count( "kill:" + entryOf( foeNoun( f ) ) );
}

const char *Game::bossTitle( const std::string &key )
{
    if ( journal::meet( key ) ) checkCodex();
    return tr( key );
}

void Game::checkCodex()
{
    // Every family met; every entry met.
    int families = 0, all = 0;
    for ( const char *k : Families ) families += journal::met( k ), all += journal::met( k );
    for ( const char *k : Foes ) all += journal::met( k );
    for ( const char *k : Bosses ) all += journal::met( k );
    if ( families == (int)std::size( Families ) ) journal::win( "codex_families" );
    if ( all == (int)( std::size( Families ) + std::size( Foes ) + std::size( Bosses ) ) ) journal::win( "codex_all" );
}

void Game::updateJournal( float dt )
{
    // The foes awake near the hero are met (once a second).
    m_journalT -= dt;
    if ( m_journalT <= 0.0f )
    {
        m_journalT = 1.0f;
        for ( const Foe &f : m_foes )
            if ( f.m_alive && f.m_awake && Vector3Distance( f.m_pos, m_pos ) < 24.0f ) meetFoe( f );
        if ( ( m_flushT -= 1.0f ) <= 0.0f ) m_flushT = 10.0f, journal::flush();
    }
    // A level done: without a death, on Albtraum, a tale.
    if ( m_finished && !m_doneCounted )
    {
        m_doneCounted = true;
        if ( m_tale && journal::meet( "tale:" + m_levelFile ) ) journal::count( "tales" );
        if ( !m_level.m_peaceful && !m_tale && m_deaths == 0 && m_kills > 0 ) journal::count( "flawless" );
        if ( !m_level.m_peaceful && m_difficulty == DifficultyE::NIGHTMARE ) journal::win( "nightmare_level" );
        journal::flush();
    }
    // New awards, one banner after the other.
    for ( const std::string &a : journal::fresh() ) m_awardQueue.push_back( a );
    m_awardT = fmaxf( 0.0f, m_awardT - dt );
    if ( m_awardT <= 0.0f && !m_awardQueue.empty() )
    {
        m_awardShown = m_awardQueue.front();
        m_awardQueue.erase( m_awardQueue.begin() );
        m_awardT = 4.5f;
        m_audio.play( SfxE::PICKUP, m_pos, m_camTarget, 0.9f );
    }
}

void Game::drawAward( int w, int, float s )
{
    if ( m_awardT <= 0.0f || m_awardShown.empty() ) return;
    // Top right: a gold frame, the word, the award's name.
    float a = Clamp( fminf( m_awardT, 4.5f - m_awardT ) * 3.0f, 0.0f, 1.0f );
    float bw = 460.0f * s, bh = 86.0f * s, x = w - bw - 30.0f * s, y = 120.0f * s;
    DrawRectangle( (int)x, (int)y, (int)bw, (int)bh, { 12, 10, 6, (unsigned char)( 220 * a ) } );
    DrawRectangleLinesEx( { x, y, bw, bh }, 2.0f * s, { 255, 200, 90, (unsigned char)( 230 * a ) } );
    // A medal: a ring with a star of light.
    Vector2 c = { x + 44.0f * s, y + bh * 0.5f };
    DrawCircleV( c, 26.0f * s, { 255, 196, 70, (unsigned char)( 255 * a ) } );
    DrawCircleV( c, 20.0f * s, { 120, 80, 20, (unsigned char)( 255 * a ) } );
    DrawPoly( c, 5, 13.0f * s, 18.0f, { 255, 230, 150, (unsigned char)( 255 * a ) } );
    DrawTextEx( m_font, tr( "award.won" ), { x + 84.0f * s, y + 12.0f * s }, 20.0f * s, 1.0f,
                { 255, 200, 110, (unsigned char)( 255 * a ) } );
    DrawTextEx( m_font, tr( "award." + m_awardShown ), { x + 84.0f * s, y + 40.0f * s }, 30.0f * s, 1.0f,
                { 250, 240, 220, (unsigned char)( 255 * a ) } );
}

std::vector<Game::PanelRow> Game::codexRows() const
{
    std::vector<PanelRow> rows;
    auto                  entry = [&]( const std::string &key, const std::string &name, const std::string &extra )
    {
        PanelRow row;
        bool     known = journal::met( key );
        int      felled = journal::counted( "kill:" + key );
        row.m_title = known ? name : "???";
        row.m_value = felled > 0 ? TextFormat( tr( "codex.felled" ), felled ) : "";
        row.m_detail = known ? std::string( tr( "codex." + key ) ) + extra : tr( "codex.unknown" );
        row.m_color = known ? Color{ 210, 205, 195, 230 } : Color{ 110, 110, 110, 200 };
        rows.push_back( row );
    };
    auto header = [&]( const char *key, int known, int all )
    {
        PanelRow h;
        h.m_title = TextFormat( "%s  %d / %d", tr( key ), known, all );
        h.m_header = true;
        rows.push_back( h );
    };
    auto knownOf = []( auto &list )
    {
        int n = 0;
        for ( const char *k : list ) n += journal::met( k );
        return n;
    };
    header( "codex.creatures", knownOf( Families ), (int)std::size( Families ) );
    for ( const char *k : Families )
    {
        // A family: its first tier's name; the three tiers in the text.
        std::string tiers = std::string( "||" ) + tr( "codex.tiers" ) + " " + tr( std::string( k ) + ".0" ) + ", " +
                            tr( std::string( k ) + ".1" ) + ", " + tr( std::string( k ) + ".2" );
        entry( k, tr( std::string( k ) + ".0" ), tiers );
    }
    header( "codex.foes", knownOf( Foes ), (int)std::size( Foes ) );
    for ( const char *k : Foes ) entry( k, tr( k ), "" );
    header( "codex.bosses", knownOf( Bosses ), (int)std::size( Bosses ) );
    for ( const char *k : Bosses ) entry( k, bossName( k ), "" );
    // The notes read, in every game.
    std::vector<std::string> notes;
    for ( const std::string &f : m_progress.m_flags )
        if ( f.compare( 0, 5, "note:" ) == 0 ) journal::meet( f );
    for ( const char *prefix : { "note:" } )
        for ( const std::string &t : journal::metWith( prefix ) ) notes.push_back( t.substr( strlen( prefix ) ) );
    int all = 0;
    for ( const journal::AwardDef &a : journal::awards() )
        if ( !strcmp( a.m_key, "notes_all" ) ) all = a.m_goal;
    header( "codex.notes", (int)notes.size(), all );
    for ( const std::string &n : notes )
    {
        PanelRow row;
        row.m_title = hasText( n + ".title" ) ? tr( n + ".title" ) : tr( "codex.note" );
        row.m_detail = tr( n );
        rows.push_back( row );
    }
    return rows;
}

std::vector<Game::PanelRow> Game::awardRows() const
{
    std::vector<PanelRow> rows;
    int                   won = 0;
    for ( const journal::AwardDef &a : journal::awards() ) won += journal::won( a.m_key );
    PanelRow h;
    h.m_title = TextFormat( "%s  %d / %d", tr( "panel.awards" ), won, (int)journal::awards().size() );
    h.m_header = true;
    rows.push_back( h );
    for ( const journal::AwardDef &a : journal::awards() )
    {
        PanelRow row;
        bool     has = journal::won( a.m_key );
        row.m_title = tr( std::string( "award." ) + a.m_key );
        row.m_detail = tr( std::string( "award." ) + a.m_key + ".about" );
        if ( has )
            row.m_value = tr( "award.done" );
        else if ( a.m_count )
            row.m_value = TextFormat( "%d / %d", std::min( journal::counted( a.m_count ), a.m_goal ), a.m_goal );
        row.m_color = has ? Color{ 255, 205, 110, 255 } : Color{ 130, 130, 130, 210 };
        rows.push_back( row );
    }
    return rows;
}

}   // namespace spaceopera
