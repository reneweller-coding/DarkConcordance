/**
 * @file GameStory.cpp
 * @brief What runs through the whole story: the deaths counted since the prologue (Ysolde knows the number at the end
 *        of Act III), the night shift's trust (how the hero dealt with control cores on the way: it opens the third
 *        ending), the act's end card (white, then its words), the endings and the epilogue's cards (one per great
 *        choice and per figure), and the Collegium's mark on every respawn once one knows who prints the bodies.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "Text.h"

#include <raymath.h>

#include <cmath>
#include <string>
#include <vector>

namespace spaceopera
{

using namespace play;

namespace
{

/** @brief Choices that count for the night shift: a control core treated as someone, not as something. */
const char *const Trust[] = { "cores.spared", "seren.taken",   "hesper.off",
                              "kael.message", "brannt.spared", "foreman.spared" };
constexpr int     TrustNeeded = 3;   ///< of the six, for Ilka to take the codes from the hero

/** @brief The great choices in the order of the story: the card's title, then the flag of each way it went. */
struct Choice
{
    const char               *m_title;
    std::vector<const char *> m_ways;
};

const Choice Choices[] = {
    { "epi.t.cores", { "cores.spared" } },
    { "epi.t.seren", { "seren.taken", "seren.off" } },
    { "epi.t.hesper", { "hesper.off", "hesper.on" } },
    { "epi.t.okafor", { "okafor.deal", "okafor.taken" } },
    { "epi.t.arche", { "arche.wake", "arche.concord", "arche.hide" } },
    { "epi.t.pell", { "pell.saved", "pell.stays" } },
    { "epi.t.kael", { "kael.message", "kael.silent" } },
    { "epi.t.hale", { "hale.ysolde", "hale.free", "hale.witness" } },
    { "epi.t.brannt", { "brannt.spared", "brannt.cut" } },
    { "epi.t.foreman", { "foreman.spared", "foreman.off" } },
};

constexpr float CardMin = 1.5f;    ///< seconds a card shows before it can be turned
constexpr float CardAuto = 2.5f;   ///< ... before the autopilot turns it

/** @brief Splits a text into lines no wider than `width` in this font and size. */
std::vector<std::string> lines( const Font &font, const std::string &t, float size, float width )
{
    std::vector<std::string> out;
    std::string              line, word;
    auto                     flush = [&]()
    {
        std::string test = line.empty() ? word : line + " " + word;
        if ( MeasureTextEx( font, test.c_str(), size, 1.0f ).x > width && !line.empty() )
        {
            out.push_back( line );
            line = word;
        }
        else
            line = test;
        word.clear();
    };
    for ( char ch : t )
    {
        if ( ch == ' ' )
            flush();
        else
            word += ch;
    }
    flush();
    if ( !line.empty() ) out.push_back( line );
    return out;
}

}   // namespace

void Game::loadStory()
{
    // Nobody has died since the prologue: Ysolde's line is another one.
    if ( m_progress.m_deaths == 0 ) m_fired.insert( "deaths.none" );
    int trust = 0;
    for ( const char *t : Trust ) trust += m_fired.count( t ) ? 1 : 0;
    if ( trust >= TrustNeeded ) m_fired.insert( "trust" );
    // Who is played (who sits at the table and who tells): "hero.dorra" and so on.
    m_fired.insert( std::string( "hero." ) + heroKey( m_tale ? m_realHero : m_heroKind ) );
}

std::string Game::spoken( const std::string &key ) const
{
    // "{deaths}": every respawn since the prologue (this level's too).
    std::string t = trAs( key, heroKey( m_heroKind ) );
    size_t      at = t.find( "{deaths}" );
    if ( at != std::string::npos ) t.replace( at, 8, std::to_string( m_progress.m_deaths + m_deaths ) );
    return t;
}

const Texture2D *Game::portrait( const std::string &who )
{
    // A speaker's face (assets/textures/portrait_<who>.png), loaded the first time it is wanted.
    auto at = m_portraits.find( who );
    if ( at != m_portraits.end() ) return &at->second;
    const std::string face = m_assets + "/textures/portrait_" + who + ".png";
    if ( !FileExists( face.c_str() ) ) return nullptr;
    Texture2D t = LoadTexture( face.c_str() );
    SetTextureFilter( t, TEXTURE_FILTER_BILINEAR );
    return &( m_portraits[who] = t );
}

float Game::speak( const std::string &key )
{
    // The voice in the chosen language, else the German one (under the chosen language's subtitles). Each hero has
    // their own take of what they say, as has a line written differently for them: "<key>.<hero>" before "<key>".
    const std::string own = key + "." + heroKey( m_heroKind );
    for ( const std::string &lang : { language(), std::string( "de" ) } )
        for ( const std::string &file : { own, key } )
        {
            float len = m_audio.voice( m_assets + "/voice/" + lang + "/" + file );
            if ( len > 0.0f ) return len;
        }
    return 0.0f;
}

bool Game::startFilm( const std::string &name )
{
    if ( m_autoplay || !m_film.open( m_assets + "/films/" + name ) ) return false;
    m_audio.stopVoice();
    m_filmCue = 0;
    m_filmKey.clear();
    m_filmLineT = m_filmSkipT = 0.0f;
    return true;
}

bool Game::updateFilm( float dt, const GameInput &in )
{
    if ( !m_film.playing() ) return false;
    m_film.update( dt );
    m_filmLineT -= dt;
    m_filmSkipT -= dt;
    if ( m_filmLineT <= 0.0f ) m_filmKey.clear();
    // The next line once its time has come and the one before it has been said (the languages differ in length).
    if ( m_filmCue < m_film.m_cues.size() && m_film.time() >= m_film.m_cues[m_filmCue].m_at && m_filmLineT <= 0.1f )
    {
        const FilmCue &c = m_film.m_cues[m_filmCue++];
        float          said = speak( c.m_key );
        m_filmWho = c.m_who == "hero" ? heroKey( m_heroKind ) : c.m_who;
        m_filmKey = c.m_key;
        m_filmLineT = said > 0.0f ? said + 0.3f
                                  : 1.5f + 0.055f * (float)GetCodepointCount( trAs( c.m_key, heroKey( m_heroKind ) ) );
    }
    // A press asks whether to skip; a second one skips.
    bool press = in.m_confirm || in.m_use || in.m_jump || in.m_special;
    bool skip = press && m_filmSkipT > 0.0f;
    if ( press && !skip ) m_filmSkipT = 3.0f;
    if ( skip || ( m_film.over() && m_filmCue >= m_film.m_cues.size() && m_filmLineT <= 0.0f ) )
    {
        m_film.close();
        m_audio.stopVoice();
        m_filmKey.clear();
    }
    return true;
}

void Game::drawFilm( int w, int h, float s )
{
    if ( !m_film.playing() ) return;
    m_film.draw( w, h );   // the subtitle is drawn by drawHud, with the lines' wrapping
    if ( m_filmSkipT > 0.0f )
    {
        const char *t = tr( "film.skip" );
        Vector2     m = MeasureTextEx( m_font, t, 18 * s, 1.0f );
        DrawTextEx( m_font, t, { w - m.x - 30 * s, 24 * s }, 18 * s, 1.0f,
                    { 200, 200, 205, (unsigned char)( 220 * fminf( 1.0f, m_filmSkipT ) ) } );
    }
}

void Game::startEpilogue()
{
    m_ending = m_fired.count( "end.c" ) ? "c" : ( m_fired.count( "end.b" ) ? "b" : "a" );
    m_cards.clear();
    m_cards.push_back( { "epi.t.end." + m_ending, "epi.end." + m_ending } );
    // One card per great choice the hero made (a place not visited, a choice not met: none).
    for ( const Choice &c : Choices )
        for ( const char *way : c.m_ways )
            if ( m_fired.count( way ) )
            {
                m_cards.push_back( { c.m_title, std::string( "epi." ) + way } );
                break;
            }
    if ( !m_fired.count( "cores.spared" ) && m_fired.count( "done.kernbank" ) )
        m_cards.insert( m_cards.begin() + 1, { "epi.t.cores", "epi.cores.broken" } );
    // The figures, by the ending: the hero and the other two, Unzu (who asks for a new name), Ysolde, the board.
    std::string hero = heroKey( m_heroKind );
    m_cards.push_back( { "hero." + hero, "epi." + hero + "." + m_ending } );
    for ( const char *other : { "dorra", "corvin", "ione" } )
        if ( hero != other )
            m_cards.push_back( { std::string( "hero." ) + other, std::string( "epi." ) + other + "." + m_ending } );
    m_cards.push_back( { "epi.t.unzu", "epi.unzu." + m_ending } );
    m_cards.push_back( { "epi.t.ysolde", "epi.ysolde." + m_ending } );
    m_cards.push_back( { "epi.t.board", "epi.board." + m_ending } );
    m_card = 0;
    m_cardT = 0.0f;
}

bool Game::updateEpilogue( float dt, GameInput &in )
{
    if ( m_level.m_epilogue.empty() ) return false;
    if ( m_card < 0 )
    {
        // It begins once its event has happened and the conversation that set it off is closed.
        if ( !m_fired.count( m_level.m_epilogue ) || m_talk ) return false;
        startEpilogue();
        startFilm( "end_" + m_ending );   // the ending's film first, the cards after it
    }
    if ( m_finished ) return true;
    // Each card is read out as it shows (the first one after the ending's film).
    if ( m_card != m_cardSpoken && m_card < (int)m_cards.size() && !m_film.playing() )
    {
        m_cardSpoken = m_card;
        speak( m_cards[m_card].second );
    }
    m_cardT += dt;
    bool turn = ( in.m_confirm || in.m_jump || in.m_use || in.m_fire ) && m_cardT > CardMin;
    if ( m_autoplay && m_cardT > CardAuto ) turn = true;
    if ( turn )
    {
        m_cardT = 0.0f;
        m_audio.stopVoice();
        if ( ++m_card >= (int)m_cards.size() )
        {
            runEvent( "gamedone" );
            m_finished = true;
        }
    }
    return true;
}

void Game::drawEndCard( int w, int h, float s )
{
    auto text = [&]( const char *t, float x, float y, float size, Color col )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        DrawTextEx( m_font, t, { x - m.x * 0.5f, y }, size * s, 1.0f, col );
    };
    // The act's end: white over everything, then its words; once it is over, how to go on.
    if ( m_whiteT > 3.0f )
    {
        float a = Clamp( ( m_whiteT - 3.0f ) / 2.5f, 0.0f, 1.0f );
        DrawRectangle( 0, 0, w, h, { 255, 255, 255, (unsigned char)( 255 * a ) } );
        if ( m_whiteT > 6.5f )
        {
            unsigned char t = (unsigned char)( 255 * Clamp( ( m_whiteT - 6.5f ) / 1.5f, 0.0f, 1.0f ) );
            text( tr( m_level.m_endCard ), w * 0.5f, h * 0.45f, 40, { 60, 60, 70, t } );
        }
        if ( m_finished )
        {
            unsigned char t = (unsigned char)( 200 * Clamp( m_endTimer - 1.0f, 0.0f, 1.0f ) );
            text( tr( "hud.next" ), w * 0.5f, h * 0.58f, 22, { 90, 90, 100, t } );
        }
    }
    // The epilogue: a card at a time on black.
    if ( m_card >= 0 )
    {
        DrawRectangle( 0, 0, w, h, { 0, 0, 0, 255 } );
        if ( m_card < (int)m_cards.size() )
        {
            float         fade = Clamp( m_cardT * 1.5f, 0.0f, 1.0f );
            unsigned char a = (unsigned char)( 255 * fade );
            float         y = h * 0.24f;
            text( tr( m_card == 0 ? "epi.ending" : "epi.head" ), w * 0.5f, y, 24, { 255, 190, 120, a } );
            text( tr( m_cards[m_card].first ), w * 0.5f, y + 36 * s, 54, { 240, 235, 230, a } );
            y += 140 * s;
            for ( const std::string &l : lines( m_font, tr( m_cards[m_card].second ), 30 * s, w * 0.58f ) )
            {
                text( l.c_str(), w * 0.5f, y, 30, { 215, 210, 200, a } );
                y += 30 * 1.3f * s;
            }
            if ( m_cardT > CardMin )
                text( tr( "talk.next" ), w * 0.5f, h * 0.9f, 20, { 150, 150, 150, (unsigned char)( 200 * fade ) } );
        }
        else
        {
            text( tr( "epi.fin" ), w * 0.5f, h * 0.42f, 48, { 235, 230, 225, 255 } );
            text( tr( "hud.next" ), w * 0.5f, h * 0.56f, 22, { 150, 150, 155, 220 } );
        }
    }
    // The Collegium's mark on a respawn: a closed eye in a ring, for a moment.
    if ( m_markT > 0.0f )
    {
        float   a = fminf( 1.0f, m_markT ) * fminf( 1.0f, ( 1.6f - m_markT ) * 4.0f );
        Color   c = { 235, 240, 250, (unsigned char)( 220 * a ) };
        Vector2 m = { w * 0.5f, h * 0.42f };
        float   r = 70.0f * s;
        DrawRing( m, r - 3.0f * s, r, 0.0f, 360.0f, 64, c );
        DrawRing( { m.x, m.y - r * 0.55f }, r * 0.95f, r * 0.95f + 3.0f * s, 125.0f, 235.0f, 32, c );
        DrawLineEx( { m.x - r * 0.62f, m.y }, { m.x + r * 0.62f, m.y }, 3.0f * s, c );
        for ( int k = -2; k <= 2; ++k )
            DrawLineEx( { m.x + k * r * 0.2f, m.y + 2.0f * s }, { m.x + k * r * 0.24f, m.y + r * 0.22f }, 2.0f * s, c );
    }
}

}   // namespace spaceopera
