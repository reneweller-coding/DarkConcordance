/**
 * @file GamePanel.cpp
 * @brief The map (what the hero has seen, over the game or on its own page), the inventory (gear, supplies, jobs,
 *        notes read on terminals) and the character page (portrait, what they are, what they can do).
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr int SightCells = 8;   ///< how far the hero sees for the map (cells)

/** @brief Text with a shadow; sizes in pixels. */
void label( const Font &font, const char *t, float x, float y, float size, Color c, int align = 0 )
{
    Vector2 m = MeasureTextEx( font, t, size, 1.0f );
    Vector2 p = { align == 1 ? x - m.x * 0.5f : ( align == 2 ? x - m.x : x ), y };
    DrawTextEx( font, t, { p.x + size * 0.07f, p.y + size * 0.07f }, size, 1.0f,
                { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
    DrawTextEx( font, t, p, size, 1.0f, c );
}

/** @brief Words into lines no wider than `width` ('|' starts a new paragraph). */
std::vector<std::string> wrap( const Font &font, const std::string &t, float size, float width )
{
    std::vector<std::string> lines;
    std::string              line, word;
    auto                     flush = [&]()
    {
        std::string test = line.empty() ? word : line + " " + word;
        if ( MeasureTextEx( font, test.c_str(), size, 1.0f ).x > width && !line.empty() )
        {
            lines.push_back( line );
            line = word;
        }
        else
            line = test;
        word.clear();
    };
    for ( char ch : t )
    {
        if ( ch == '|' )
        {
            flush();
            lines.push_back( line );
            lines.push_back( "" );
            line.clear();
        }
        else if ( ch == ' ' )
            flush();
        else
            word += ch;
    }
    flush();
    if ( !line.empty() ) lines.push_back( line );
    return lines;
}

/** @brief The jobs and the event that ends each. */
const char *const Jobs[][2] = { { "job1", "done.kuehlgedaerme" }, { "job2", "done.knochenmarkt" } };

}   // namespace

void Game::loadPanel()
{
    // In a hub everything is known; elsewhere the map fills in as the hero sees it.
    int rows = m_level.rows(), cols = m_level.cols();
    m_seen.assign( (size_t)rows * cols, m_level.m_peaceful ? 1 : 0 );
    m_mapR0 = rows, m_mapR1 = 0, m_mapC0 = cols, m_mapC1 = 0;
    for ( int r = 0; r < rows; ++r )
        for ( int c = 0; c < cols; ++c )
            if ( m_level.cell( r, c ).m_kind != CellE::ROCK )
            {
                m_mapR0 = std::min( m_mapR0, r - 1 ), m_mapR1 = std::max( m_mapR1, r + 1 );
                m_mapC0 = std::min( m_mapC0, c - 1 ), m_mapC1 = std::max( m_mapC1, c + 1 );
            }
    std::string portrait = m_assets + "/textures/portrait_" + heroKey( m_heroKind ) + ".png";
    if ( !m_portraits.count( "hero" ) && FileExists( portrait.c_str() ) )
    {
        m_portraits["hero"] = LoadTexture( portrait.c_str() );
        SetTextureFilter( m_portraits["hero"], TEXTURE_FILTER_BILINEAR );
    }
}

void Game::updateMap( float dt )
{
    // Every fifth of a second: the cells in sight (no wall or shut door between their middles) are seen.
    m_seenTimer -= dt;
    if ( m_seenTimer > 0.0f || m_seen.empty() ) return;
    m_seenTimer = 0.2f;
    int rows = m_level.rows(), cols = m_level.cols(), r0, c0;
    m_level.cellOf( m_pos, r0, c0 );
    for ( int dr = -SightCells; dr <= SightCells; ++dr )
        for ( int dc = -SightCells; dc <= SightCells; ++dc )
        {
            int r = r0 + dr, c = c0 + dc;
            if ( dr * dr + dc * dc > SightCells * SightCells || r < 0 || c < 0 || r >= rows || c >= cols ) continue;
            int  n = std::max( abs( dr ), abs( dc ) ) * 2;
            bool clear = true;
            for ( int i = 1; i < n && clear; ++i )
            {
                float       t = (float)i / n;
                const Cell &k = m_level.cell( (int)floorf( r0 + 0.5f + dr * t ), (int)floorf( c0 + 0.5f + dc * t ) );
                clear = !( k.m_kind == CellE::ROCK || ( k.m_kind == CellE::DOOR && !k.m_open ) );
            }
            if ( clear ) m_seen[(size_t)r * cols + c] = 1;
        }
}

void Game::drawMap( Rectangle area, float alpha, float s )
{
    int   cols = m_level.cols(), rows = m_mapR1 - m_mapR0 + 1, wide = m_mapC1 - m_mapC0 + 1;
    float k = fminf( area.width / wide, area.height / rows );
    float x0 = area.x + ( area.width - k * wide ) * 0.5f, y0 = area.y + ( area.height - k * rows ) * 0.5f;
    auto  at = [&]( Vector3 p )
    { return Vector2{ x0 + ( p.x / CellSize - m_mapC0 ) * k, y0 + ( p.z / CellSize - m_mapR0 ) * k }; };
    auto      fade = [&]( Color c ) { return Color{ c.r, c.g, c.b, (unsigned char)( c.a * alpha ) }; };
    const int DR[4] = { -1, 0, 1, 0 }, DC[4] = { 0, 1, 0, -1 };
    DrawRectangleRec( { x0 - 12.0f * s, y0 - 12.0f * s, k * wide + 24.0f * s, k * rows + 24.0f * s },
                      fade( { 0, 0, 0, 120 } ) );
    // Floors faint, windows blue, doors by their state; a bright line where a seen floor meets a wall.
    for ( int r = m_mapR0; r <= m_mapR1; ++r )
        for ( int c = m_mapC0; c <= m_mapC1; ++c )
        {
            if ( r < 0 || c < 0 || r >= m_level.rows() || c >= cols || !m_seen[(size_t)r * cols + c] ) continue;
            const Cell &cell = m_level.cell( r, c );
            if ( cell.m_kind == CellE::ROCK || cell.m_kind == CellE::VOID ) continue;
            Rectangle box = { x0 + ( c - m_mapC0 ) * k, y0 + ( r - m_mapR0 ) * k, k + 0.5f, k + 0.5f };
            Color     fill = { 190, 180, 160, 55 };
            if ( cell.m_kind == CellE::GLASS ) fill = { 80, 140, 210, 120 };
            if ( cell.m_kind == CellE::FIN || cell.m_kind == CellE::COUNTER ) fill = { 150, 140, 120, 120 };
            if ( cell.m_kind == CellE::DOOR )
            {
                auto lock = m_level.m_locks.find( r * 1000 + c );
                bool locked = lock != m_level.m_locks.end() && !m_fired.count( lock->second );
                fill = cell.m_open ? Color{ 90, 220, 120, 170 }
                       : locked    ? Color{ 240, 70, 50, 200 }
                                   : Color{ 250, 180, 60, 190 };
            }
            DrawRectangleRec( box, fade( fill ) );
            for ( int d = 0; d < 4; ++d )
            {
                if ( m_level.cell( r + DR[d], c + DC[d] ).m_kind != CellE::ROCK ) continue;
                Vector2 a = { box.x + ( DC[d] > 0 ? k : 0.0f ), box.y + ( DR[d] > 0 ? k : 0.0f ) };
                Vector2 b = { a.x + ( DR[d] != 0 ? k : 0.0f ), a.y + ( DC[d] != 0 ? k : 0.0f ) };
                DrawLineEx( a, b, fmaxf( 1.5f, k * 0.12f ), fade( { 235, 220, 190, 230 } ) );
            }
        }
    auto seen = [&]( Vector3 p )
    {
        int r, c;
        m_level.cellOf( p, r, c );
        return r >= 0 && c >= 0 && r < m_level.rows() && c < cols && m_seen[(size_t)r * cols + c];
    };
    // Lockers still to search, terminals, people to talk to, the way out.
    for ( const Usable &u : m_usables )
    {
        if ( u.m_kind == 0 || !seen( u.m_at ) || ( u.m_kind == 1 && u.m_used ) ) continue;
        Vector2 p = at( u.m_at );
        Color   c = u.m_kind == 1 ? Color{ 255, 150, 60, 230 } : Color{ 90, 220, 230, 230 };
        DrawRectangleRec( { p.x - k * 0.22f, p.y - k * 0.22f, k * 0.44f, k * 0.44f }, fade( c ) );
    }
    for ( const NpcState &n : m_npcs ) DrawCircleV( at( n.m_pos ), k * 0.28f, fade( { 110, 170, 255, 240 } ) );
    if ( m_hasExit ) DrawCircleLinesV( at( m_exit ), k * 0.45f, fade( { 90, 230, 130, 240 } ) );
    if ( m_liftMax.x > m_liftMin.x && seen( m_lift ) )
        DrawCircleLinesV( at( m_lift ), k * 0.6f, fade( { 90, 230, 130, 240 } ) );
    // The hero: an arrow the way they face.
    Vector2 h = at( m_pos );
    Vector2 f = { sinf( m_yaw ), cosf( m_yaw ) }, side = { f.y, -f.x };
    float   len = fmaxf( k * 0.9f, 9.0f * s );
    Vector2 tip = Vector2Add( h, Vector2Scale( f, len ) );
    Vector2 l = Vector2Add( Vector2Subtract( h, Vector2Scale( f, len * 0.5f ) ), Vector2Scale( side, len * 0.55f ) );
    Vector2 r =
        Vector2Subtract( Vector2Subtract( h, Vector2Scale( f, len * 0.5f ) ), Vector2Scale( side, len * 0.55f ) );
    DrawTriangle( tip, l, r, fade( { 255, 170, 60, 255 } ) );
    DrawTriangle( tip, r, l, fade( { 255, 170, 60, 255 } ) );
}

std::vector<Game::PanelRow> Game::inventory() const
{
    std::vector<PanelRow> rows;
    bool                  dorra = m_heroKind == HeroE::DORRA;
    auto has = [&]( const std::string &flag ) { return m_progress.m_flags.count( flag ) || m_fired.count( flag ); };
    rows.push_back( { tr( "inv.gear" ), "", "", true } );
    rows.push_back( { tr( dorra ? "item.hammer" : "item.rifle" ), TextFormat( tr( "item.level" ), m_progress.m_weapon ),
                      tr( dorra ? "item.hammer.about" : "item.rifle.about" ) } );
    rows.push_back( { tr( dorra ? "item.armor.dorra" : "item.armor" ),
                      TextFormat( tr( "item.level" ), m_progress.m_armor ), tr( "item.armor.about" ) } );
    rows.push_back( { tr( "inv.supplies" ), "", "", true } );
    rows.push_back( { tr( "item.stims" ), TextFormat( "%d", m_stims ), tr( "item.stims.about" ) } );
    rows.push_back( { tr( "item.scrap" ), TextFormat( "%d", m_scrap ), tr( "item.scrap.about" ) } );
    bool header = false;
    for ( const auto &job : Jobs )
    {
        if ( !has( job[0] ) ) continue;
        if ( !header ) rows.push_back( { tr( "inv.jobs" ), "", "", true } ), header = true;
        std::string key = std::string( "job." ) + job[0];
        rows.push_back( { tr( key ), tr( has( job[1] ) ? "inv.done" : "inv.open" ), tr( key + ".about" ) } );
    }
    header = false;
    for ( const std::string &flag : m_progress.m_flags )
    {
        if ( flag.compare( 0, 5, "note:" ) != 0 ) continue;
        if ( !header ) rows.push_back( { tr( "inv.notes" ), "", "", true } ), header = true;
        std::string key = flag.substr( 5 );
        rows.push_back( { tr( key + ".title" ), "", tr( key ) } );
    }
    return rows;
}

int Game::panelRows() const
{
    int n = 0;
    for ( const PanelRow &row : inventory() ) n += row.m_header ? 0 : 1;
    return n;
}

void Game::drawPanel( int w, int h, int tab, int sel )
{
    float       s = h / 1080.0f;
    const Font &font = m_font;
    DrawRectangle( 0, 0, w, h, { 3, 5, 8, 248 } );
    // The tabs, the help line.
    const char *tabs[3] = { "panel.inventory", "panel.character", "panel.map" };
    for ( int i = 0; i < 3; ++i )
    {
        float x = w * 0.5f + ( i - 1 ) * 320.0f * s;
        bool  on = i == tab;
        label( font, tr( tabs[i] ), x, 48.0f * s, 34.0f * s,
               on ? Color{ 255, 200, 130, 255 } : Color{ 150, 150, 150, 220 }, 1 );
        if ( on )
            DrawRectangle( (int)( x - 90.0f * s ), (int)( 92.0f * s ), (int)( 180.0f * s ), (int)( 3.0f * s ),
                           { 255, 170, 80, 220 } );
    }
    label( font, tr( "panel.help" ), w * 0.5f, h - 56.0f * s, 20.0f * s, { 150, 150, 150, 220 }, 1 );
    float left = w * 0.12f, right = w * 0.88f, top = 140.0f * s;

    if ( tab == 0 )
    {
        // The list on the left (the chosen row lit), what it is on the right.
        std::vector<PanelRow> rows = inventory();
        float                 y = top, listW = w * 0.36f;
        int                   index = 0;
        const PanelRow       *chosen = nullptr;
        for ( const PanelRow &row : rows )
        {
            if ( row.m_header )
            {
                y += 14.0f * s;
                label( font, row.m_title.c_str(), left, y, 22.0f * s, { 255, 170, 90, 230 } );
                y += 38.0f * s;
                continue;
            }
            bool on = index++ == sel;
            if ( on )
            {
                chosen = &row;
                DrawRectangle( (int)( left - 12.0f * s ), (int)( y - 6.0f * s ), (int)( listW + 24.0f * s ),
                               (int)( 40.0f * s ), { 255, 170, 80, 40 } );
            }
            Color c = on ? Color{ 255, 220, 170, 255 } : Color{ 210, 205, 195, 230 };
            label( font, row.m_title.c_str(), left, y, 26.0f * s, c );
            label( font, row.m_value.c_str(), left + listW, y, 26.0f * s, c, 2 );
            y += 42.0f * s;
        }
        if ( chosen )
        {
            float x = w * 0.54f, dy = top;
            label( font, chosen->m_title.c_str(), x, dy, 36.0f * s, { 245, 236, 222, 255 } );
            dy += 60.0f * s;
            for ( const std::string &line : wrap( font, chosen->m_detail, 26.0f * s, right - x ) )
            {
                label( font, line.c_str(), x, dy, 26.0f * s, { 215, 210, 200, 240 } );
                dy += 36.0f * s;
            }
        }
        return;
    }
    if ( tab == 1 )
    {
        // The portrait, the name, what they are; then what they can do now.
        float x = left;
        auto  portrait = m_portraits.find( "hero" );
        if ( portrait != m_portraits.end() )
        {
            float     size = 420.0f * s;
            Texture2D t = portrait->second;
            DrawTexturePro( t, { 0, 0, (float)t.width, (float)t.height }, { left, top, size, size }, { 0, 0 }, 0.0f,
                            WHITE );
            DrawRectangleLinesEx( { left, top, size, size }, 2.0f * s, { 255, 180, 100, 120 } );
            x = left + size + 50.0f * s;
        }
        const char *who = heroKey( m_heroKind );
        float       y = top;
        label( font, tr( std::string( "hero." ) + who ), x, y, 48.0f * s, { 245, 236, 222, 255 } );
        y += 70.0f * s;
        for ( const std::string &line :
              wrap( font, tr( std::string( "choose." ) + who + ".about" ), 24.0f * s, right - x ) )
        {
            label( font, line.c_str(), x, y, 24.0f * s, { 200, 195, 185, 240 } );
            y += 34.0f * s;
        }
        y += 30.0f * s;
        bool  dorra = m_heroKind == HeroE::DORRA;
        float damage = ( dorra ? SwingDamage : ShotDamage ) * weaponBoost();
        auto  stat = [&]( const char *key, const std::string &value )
        {
            label( font, tr( key ), x, y, 26.0f * s, { 255, 190, 120, 235 } );
            label( font, value.c_str(), right, y, 26.0f * s, { 235, 230, 220, 255 }, 2 );
            y += 42.0f * s;
        };
        stat( "stat.health", TextFormat( "%d / %d", (int)ceilf( m_hp ), (int)hpMax() ) );
        stat( "stat.damage", TextFormat( "%d", (int)roundf( damage ) ) );
        stat( "stat.weapon", TextFormat( tr( "item.level" ), m_progress.m_weapon ) );
        stat( "stat.armor", TextFormat( tr( "item.level" ), m_progress.m_armor ) );
        stat( "stat.stims", TextFormat( "%d", m_stims ) );
        stat( "stat.scrap", TextFormat( "%d", m_scrap ) );
        stat( "stat.place", tr( m_level.m_name ) );
        y += 16.0f * s;
        for ( const std::string &line :
              wrap( font, tr( dorra ? "stat.special.dorra" : "stat.special.corvin" ), 22.0f * s, right - x ) )
        {
            label( font, line.c_str(), x, y, 22.0f * s, { 170, 170, 165, 230 } );
            y += 30.0f * s;
        }
        return;
    }
    // The map: the whole of what has been seen, the place's name over it.
    label( font, tr( m_level.m_name ), w * 0.5f, top - 10.0f * s, 28.0f * s, { 245, 236, 222, 255 }, 1 );
    drawMap( { left, top + 40.0f * s, right - left, h - top - 140.0f * s }, 1.0f, s );
}

}   // namespace spaceopera
