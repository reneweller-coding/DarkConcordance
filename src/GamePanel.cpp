/**
 * @file GamePanel.cpp
 * @brief The map (what the hero has seen, over the game or on its own page), the inventory (gear, supplies, jobs,
 *        notes read on terminals) and the character page (portrait, what they are, what they can do).
 * @author Rene Weller
 */
#include "Game.h"
#include "Files.h"
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

constexpr int   SightCells = 8;       ///< how far the hero sees for the map (cells)
constexpr float BaseCritPct = 5.0f;   ///< the crit chance without anything (as in GameLoot.cpp)

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

/** @brief Shortened with an ellipsis until it fits `width`. */
std::string fit( const Font &font, const std::string &t, float size, float width )
{
    if ( MeasureTextEx( font, t.c_str(), size, 1.0f ).x <= width ) return t;
    std::string s = t;
    while ( !s.empty() && MeasureTextEx( font, ( s + "…" ).c_str(), size, 1.0f ).x > width )
    {
        unsigned char last;
        do
        {
            last = (unsigned char)s.back();
            s.pop_back();
        } while ( !s.empty() && ( last & 0xC0 ) == 0x80 );
    }
    return s + "…";
}

/** @return the gear page an item belongs on: 0 weapons, 1 armour, 2 the suit's technology, 3 modules */
int gearPage( SlotE slot )
{
    switch ( slot )
    {
    case SlotE::WEAPON: return 0;
    case SlotE::ARMOR:
    case SlotE::HELMET:
    case SlotE::GLOVES:
    case SlotE::BOOTS: return 1;
    case SlotE::MODULE: return 3;
    default: return 2;
    }
}

/** @return the gear page a worn slot belongs on */
int slotPage( int slot )
{
    return slot == 0 ? 0 : slot <= 4 ? 1 : 2;
}

/** @brief A page's sign on its tab, drawn in lines (no pictures needed, any size). */
void tabSign( int tab, Rectangle r, Color c )
{
    Vector2 m = { r.x + r.width * 0.5f, r.y + r.height * 0.5f };
    float   u = r.width * 0.3f, t = fmaxf( 1.5f, r.width * 0.045f );
    auto    at = [&]( float x, float y ) { return Vector2{ m.x + x * u, m.y + y * u }; };
    auto    line = [&]( float ax, float ay, float bx, float by ) { DrawLineEx( at( ax, ay ), at( bx, by ), t, c ); };
    auto    ring = [&]( float x, float y, float rad )
    { DrawRing( at( x, y ), rad * u - t * 0.5f, rad * u + t * 0.5f, 0, 360, 24, c ); };
    switch ( tab )
    {
    case 0:   // crossed blades
        line( -0.9f, 0.9f, 0.8f, -0.8f ), line( 0.9f, 0.9f, -0.8f, -0.8f );
        line( -0.8f, 0.3f, -0.3f, 0.8f ), line( 0.8f, 0.3f, 0.3f, 0.8f );
        break;
    case 1:   // a breastplate
        line( -0.8f, -0.85f, 0.8f, -0.85f ), line( 0.8f, -0.85f, 0.8f, 0.1f ), line( 0.8f, 0.1f, 0.0f, 0.95f );
        line( 0.0f, 0.95f, -0.8f, 0.1f ), line( -0.8f, 0.1f, -0.8f, -0.85f );
        break;
    case 2:   // a chip with its pins
        DrawRectangleLinesEx( { m.x - 0.6f * u, m.y - 0.6f * u, 1.2f * u, 1.2f * u }, t, c );
        for ( float k : { -0.3f, 0.0f, 0.3f } )
            line( k, -0.6f, k, -0.95f ), line( k, 0.6f, k, 0.95f ), line( -0.6f, k, -0.95f, k ),
                line( 0.6f, k, 0.95f, k );
        break;
    case 3:   // a module
        DrawPolyLinesEx( m, 6, 0.9f * u, 30.0f, t, c );
        DrawCircleV( m, 0.25f * u, c );
        break;
    case 4:   // a star
        for ( int i = 0; i < 5; ++i )
        {
            float a = -PI * 0.5f + i * 2.0f * PI / 5.0f, b = a + 4.0f * PI / 5.0f;
            line( cosf( a ) * 0.95f, sinf( a ) * 0.95f, cosf( b ) * 0.95f, sinf( b ) * 0.95f );
        }
        break;
    case 5:   // a figure
        ring( 0.0f, -0.65f, 0.28f );
        line( 0.0f, -0.37f, 0.0f, 0.35f ), line( -0.8f, -0.1f, 0.8f, -0.1f );
        line( 0.0f, 0.35f, -0.55f, 0.95f ), line( 0.0f, 0.35f, 0.55f, 0.95f );
        break;
    case 6:   // a page of notes
        DrawRectangleLinesEx( { m.x - 0.7f * u, m.y - 0.9f * u, 1.4f * u, 1.8f * u }, t, c );
        for ( float y : { -0.45f, -0.1f, 0.25f, 0.6f } ) line( -0.4f, y, 0.4f, y );
        break;
    case 7:   // a folded map
        line( -0.95f, -0.7f, -0.3f, -0.9f ), line( -0.3f, -0.9f, 0.3f, -0.7f ), line( 0.3f, -0.7f, 0.95f, -0.9f );
        line( 0.95f, -0.9f, 0.95f, 0.7f ), line( 0.95f, 0.7f, 0.3f, 0.9f ), line( 0.3f, 0.9f, -0.3f, 0.7f );
        line( -0.3f, 0.7f, -0.95f, 0.9f ), line( -0.95f, 0.9f, -0.95f, -0.7f );
        line( -0.3f, -0.9f, -0.3f, 0.7f ), line( 0.3f, -0.7f, 0.3f, 0.9f );
        break;
    case 8:   // an open book
        line( 0.0f, -0.6f, 0.0f, 0.85f );
        for ( float k : { -1.0f, 1.0f } )
            line( 0.0f, -0.6f, 0.95f * k, -0.8f ), line( 0.95f * k, -0.8f, 0.95f * k, 0.65f ),
                line( 0.95f * k, 0.65f, 0.0f, 0.85f );
        break;
    default:   // a medal on its ribbon
        line( -0.35f, -0.95f, 0.0f, -0.2f ), line( 0.35f, -0.95f, 0.0f, -0.2f );
        ring( 0.0f, 0.35f, 0.5f );
    }
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

void Game::drawMinimap( int w, int h, float s )
{
    // The overlay's map with its window round the hero, clipped to a square in the corner.
    const int reach = 11;
    int       r, c;
    m_level.cellOf( m_pos, r, c );
    int keep[4] = { m_mapR0, m_mapR1, m_mapC0, m_mapC1 };
    m_mapR0 = r - reach, m_mapR1 = r + reach, m_mapC0 = c - reach, m_mapC1 = c + reach;
    float     size = 230.0f * s;
    Rectangle area = { w - size - 30.0f * s, 30.0f * s, size, size };
    BeginScissorMode( (int)( area.x - 12.0f * s ), (int)( area.y - 12.0f * s ), (int)( size + 24.0f * s ),
                      (int)( size + 24.0f * s ) );
    drawMap( area, 0.85f, s );
    EndScissorMode();
    m_mapR0 = keep[0], m_mapR1 = keep[1], m_mapC0 = keep[2], m_mapC1 = keep[3];
    (void)h;
}

std::vector<Game::PanelRow> Game::gearRows( int tab ) const
{
    std::vector<PanelRow> rows;
    auto                  card = [&]( const Item &it )
    {
        PanelRow r;
        r.m_title = itemName( it );
        r.m_color = rarityColor( it.m_rarity );
        r.m_detail = itemDetail( it );
        return r;
    };
    // What is worn on this page (the second weapon after the hand's); the free places named in a line above.
    std::string free;
    for ( int i = 0; i < EquipSlots; ++i )
    {
        if ( slotPage( i ) != tab ) continue;
        if ( !m_progress.m_worn[i] )
        {
            free += ( free.empty() ? "" : ", " ) + std::string( tr( TextFormat( "slot.%d", i ) ) );
            continue;
        }
        PanelRow r = card( m_progress.m_gear[i] );
        r.m_slot = i;
        rows.push_back( r );
        if ( i == 0 && m_progress.m_hasSpare )
        {
            PanelRow spare = card( m_progress.m_spare );
            spare.m_spare = true;
            rows.push_back( spare );
        }
    }
    if ( !free.empty() )
    {
        PanelRow f;
        f.m_header = f.m_free = true;
        f.m_title = TextFormat( tr( "inv.free" ), free.c_str() );
        rows.insert( rows.begin(), f );
    }
    // What is carried of this kind, and in a hub what the stash holds of it.
    for ( size_t i = 0; i < m_progress.m_bag.size(); ++i )
    {
        if ( gearPage( itemSlot( m_progress.m_bag[i] ) ) != tab ) continue;
        PanelRow r = card( m_progress.m_bag[i] );
        r.m_bag = (int)i;
        rows.push_back( r );
    }
    if ( atStash() )
    {
        bool header = false;
        for ( size_t i = 0; i < m_stash.size(); ++i )
        {
            if ( gearPage( itemSlot( m_stash[i] ) ) != tab ) continue;
            if ( !header )
                rows.push_back( { TextFormat( tr( "inv.stash" ), (int)m_stash.size(), StashSize ), "", "", true } ),
                    header = true;
            PanelRow r = card( m_stash[i] );
            r.m_stash = (int)i;
            rows.push_back( r );
        }
    }
    return rows;
}

std::vector<Game::PanelRow> Game::notesRows() const
{
    std::vector<PanelRow> rows;
    auto has = [&]( const std::string &flag ) { return m_progress.m_flags.count( flag ) || m_fired.count( flag ); };
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
    if ( rows.empty() ) rows.push_back( { tr( "inv.nonotes" ), "", "", true } );
    return rows;
}

std::string Game::itemSummary( const Item &it ) const
{
    const ItemBase &b = itemBases()[it.m_base];
    if ( b.m_slot == SlotE::WEAPON )
    {
        std::string t = TextFormat( tr( "item.dmg" ), (int)roundf( itemDamage( it ) ) );
        if ( b.m_pellets > 1 ) t += TextFormat( " x %d", b.m_pellets );
        if ( b.m_hero != 1 ) t += std::string( " · " ) + TextFormat( tr( "item.rate" ), 1.0f / b.m_interval );
        return t;
    }
    if ( b.m_slot == SlotE::EMITTER ) return TextFormat( tr( "item.shieldval" ), (int)roundf( itemShield( it ) ) );
    if ( b.m_armor > 0.0f ) return TextFormat( tr( "item.armorval" ), (int)roundf( itemArmor( it ) ) );
    if ( !it.m_affixes.empty() ) return affixText( it.m_affixes[0] );
    if ( b.m_bonus != StatE::COUNT && b.m_bonusValue != 0.0f ) return affixText( { b.m_bonus, b.m_bonusValue } );
    return "";
}

std::string Game::itemDetail( const Item &it ) const
{
    // Kind, level and rarity; what it does; its affixes; what it is worth; and, for the bag, what it would replace.
    const ItemBase &b = itemBases()[it.m_base];
    std::string     d = std::string( tr( TextFormat( "rarity.%d", (int)it.m_rarity ) ) ) + " · " +
                        TextFormat( tr( "item.ilevel" ), it.m_level ) + " | ";
    if ( b.m_slot == SlotE::WEAPON )
    {
        d += TextFormat( tr( "item.dmg" ), (int)roundf( itemDamage( it ) ) );
        if ( b.m_pellets > 1 ) d += TextFormat( " x %d", b.m_pellets );
        if ( b.m_hero != 1 ) d += std::string( " · " ) + TextFormat( tr( "item.rate" ), 1.0f / b.m_interval );
        if ( b.m_pierce > 0 ) d += std::string( " · " ) + TextFormat( tr( "item.pierce" ), b.m_pierce );
        if ( b.m_splash > 0.0f ) d += std::string( " · " ) + tr( "item.splash" );
        if ( b.m_chain > 0 ) d += std::string( " · " ) + TextFormat( tr( "item.chain" ), b.m_chain );
        const char *kinds[4] = { "dmg.kinetic", "dmg.heat", "dmg.shock", "dmg.acid" };
        d += std::string( " · " ) + TextFormat( tr( "item.dmgkind" ), tr( kinds[weaponDamage( it )] ) );
        d += " | ";
    }
    else if ( b.m_armor > 0.0f )
        d += std::string( TextFormat( tr( "item.armorval" ), (int)roundf( itemArmor( it ) ) ) ) + " | ";
    if ( b.m_slot == SlotE::EMITTER )
        d += std::string( TextFormat( tr( "item.shieldval" ), (int)roundf( itemShield( it ) ) ) ) + " | ";
    if ( b.m_bonus != StatE::COUNT && b.m_slot != SlotE::MODULE && b.m_bonusValue != 0.0f )
        d += affixText( { b.m_bonus, b.m_bonusValue } ) + " | ";
    for ( const Affix &a : it.m_affixes ) d += affixText( a ) + " | ";
    // The sockets and their modules.
    if ( it.m_sockets > 0 )
    {
        d += std::string( TextFormat( tr( "item.sockets" ), (int)it.m_mods.size(), it.m_sockets ) ) + " | ";
        for ( const Affix &a : it.m_mods ) d += "  · " + affixText( a ) + " | ";
    }
    // A unique's power, a set's bonuses (how many of its pieces are worn now), the lore.
    PowerE power = itemPower( it );
    if ( power != PowerE::NONE ) d += std::string( "» " ) + powerText( power ) + " | ";
    int set = itemSet( it );
    if ( set >= 0 )
    {
        const SetDef &sd = itemSets()[set];
        d += std::string( TextFormat( tr( "item.setof" ), tr( sd.m_key ), m_setPieces[set] ) ) + " | ";
        auto list = [&]( const std::vector<Affix> &as )
        {
            std::string t;
            for ( const Affix &a : as ) t += ( t.empty() ? "" : ", " ) + affixText( a );
            return t;
        };
        d += std::string( TextFormat( tr( "item.set2" ), list( sd.m_two ).c_str() ) ) + " | ";
        d += std::string( TextFormat( tr( "item.set3" ), list( sd.m_three ).c_str() ) ) + " | ";
        d += std::string( TextFormat( tr( "item.set4" ), powerText( sd.m_four ).c_str() ) ) + " | ";
    }
    std::string lore = itemLore( it );
    if ( !lore.empty() ) d += lore + " | ";
    d += b.m_slot == SlotE::MODULE ? tr( "module.about" ) : tr( std::string( b.m_key ) + ".about" );
    if ( b.m_hero >= 0 && b.m_hero != heroIndex() ) d += std::string( " | " ) + tr( "loot.wrong" );
    d += std::string( " | " ) + TextFormat( tr( "item.value" ), std::max( 1, itemValue( it ) / 5 ) );
    return d;
}

std::vector<Game::PanelRow> Game::skillRows() const
{
    std::vector<PanelRow> rows;
    int                   hero = heroIndex();
    const char           *keys[4] = { "", "hud.key1", "hud.key2", "hud.key3" };
    rows.push_back( { TextFormat( tr( "skills.points" ), m_progress.m_points ), "", "", true } );
    const std::vector<SkillDef> &defs = skillDefs();
    for ( size_t k = 0; k < defs.size(); ++k )
    {
        const SkillDef &def = defs[k];
        if ( def.m_hero != hero ) continue;
        PanelRow row;
        row.m_skill = (int)k;
        row.m_title = tr( def.m_key );
        if ( def.m_slot > 0 ) row.m_title += std::string( "  (" ) + tr( keys[def.m_slot] ) + ")";
        int r = rank( (int)k );
        row.m_value = TextFormat( "%d / %d", r, MaxRank );
        row.m_color = r > 0 ? Color{ 140, 220, 255, 255 } : Color{ 160, 160, 160, 220 };
        row.m_detail = std::string( tr( std::string( def.m_key ) + ".about" ) ) + " | " +
                       TextFormat( tr( "skills.from" ), def.m_level );
        if ( def.m_slot > 0 && def.m_cooldown > 0.0f )
            row.m_detail += std::string( " | " ) + TextFormat( tr( "skills.cooldown" ), def.m_cooldown );
        if ( m_progress.m_heroLevel < def.m_level ) row.m_color = { 110, 110, 110, 200 };
        rows.push_back( row );
    }
    return rows;
}

int Game::panelRows( int tab ) const
{
    if ( tab == TabCharacter || tab == TabMap ) return 0;
    int n = 0;
    for ( const PanelRow &row : rowsOf( tab ) ) n += row.m_header ? 0 : 1;
    return n;
}

std::vector<Game::PanelRow> Game::rowsOf( int tab ) const
{
    return tab < GearTabs     ? gearRows( tab )
           : tab == TabSkills ? skillRows()
           : tab == TabNotes  ? notesRows()
           : tab == TabCodex  ? codexRows()
           : tab == TabAwards ? awardRows()
                              : std::vector<PanelRow>();
}

const Item *Game::rowItem( const PanelRow &row ) const
{
    if ( row.m_bag >= 0 && row.m_bag < (int)m_progress.m_bag.size() ) return &m_progress.m_bag[row.m_bag];
    if ( row.m_stash >= 0 && row.m_stash < (int)m_stash.size() ) return &m_stash[row.m_stash];
    if ( row.m_spare && m_progress.m_hasSpare ) return &m_progress.m_spare;
    if ( row.m_slot >= 0 && m_progress.m_worn[row.m_slot] ) return &m_progress.m_gear[row.m_slot];
    return nullptr;
}

void Game::loadStash( const std::string &path )
{
    m_stashPath = path;
    m_stash.clear();
    FILE *f = fopen( path.c_str(), "r" );
    if ( !f ) f = fopen( ( path + ".bak" ).c_str(), "r" );
    if ( !f ) return;
    char line[2048];
    while ( fgets( line, sizeof( line ), f ) )
    {
        std::string s = line;
        while ( !s.empty() && ( s.back() == '\n' || s.back() == '\r' ) ) s.pop_back();
        Item it;
        if ( s.rfind( "item ", 0 ) == 0 && loadItem( s.substr( 5 ), it ) && (int)m_stash.size() < StashSize )
            m_stash.push_back( it );
    }
    fclose( f );
}

void Game::saveStash() const
{
    if ( m_stashPath.empty() ) return;
    FILE *f = beginWrite( m_stashPath );
    if ( !f ) return;
    for ( const Item &it : m_stash ) fprintf( f, "item %s\n", saveItem( it ).c_str() );
    endWrite( f, m_stashPath );
}

bool Game::atStash() const
{
    // The hubs: Velm, the ship in both acts, the ring.
    const std::string &act = m_level.m_act;
    return m_level.m_peaceful && act.size() > 3 && act.compare( act.size() - 3, 3, "hub" ) == 0;
}

void Game::store( int bag )
{
    if ( !atStash() || bag < 0 || bag >= (int)m_progress.m_bag.size() ) return;
    if ( (int)m_stash.size() >= StashSize )
    {
        m_toast = tr( "stash.full" );
        m_toastColor = { 255, 140, 110, 255 };
        m_toastTimer = 2.0f;
        return;
    }
    m_stash.push_back( m_progress.m_bag[bag] );
    m_progress.m_bag.erase( m_progress.m_bag.begin() + bag );
    saveStash();
    m_audio.play( SfxE::PICKUP, m_pos, m_camTarget, 0.6f );
}

void Game::take( int stash )
{
    if ( !atStash() || stash < 0 || stash >= (int)m_stash.size() ) return;
    if ( (int)m_progress.m_bag.size() >= BagSize )
    {
        m_toast = tr( "loot.full" );
        m_toastColor = { 255, 140, 110, 255 };
        m_toastTimer = 2.0f;
        return;
    }
    m_progress.m_bag.push_back( m_stash[stash] );
    m_stash.erase( m_stash.begin() + stash );
    saveStash();
    m_audio.play( SfxE::PICKUP, m_pos, m_camTarget, 0.6f );
}

void Game::panelAction( int tab, int row, int action )
{
    if ( tab >= GearTabs && tab != TabSkills ) return;
    std::vector<PanelRow> rows = rowsOf( tab );
    int                   index = 0;
    for ( const PanelRow &r : rows )
    {
        if ( r.m_header ) continue;
        if ( index++ != row ) continue;
        if ( r.m_bag >= 0 && action == 2 )
            store( r.m_bag );
        else if ( r.m_bag >= 0 )
            action == 0 ? equip( r.m_bag ) : salvage( r.m_bag );
        else if ( r.m_stash >= 0 && action != 1 )
            take( r.m_stash );
        else if ( r.m_stash >= 0 )
        {
            // Salvaged from the stash: into the bag for a moment, then to scrap.
            if ( (int)m_progress.m_bag.size() < BagSize )
            {
                take( r.m_stash );
                salvage( (int)m_progress.m_bag.size() - 1 );
            }
        }
        else if ( r.m_spare && action == 0 )
            unequipSpare();
        else if ( r.m_slot == 0 && action == 0 )
        {
            // The hand's weapon off: the second one takes its place (the hand is never empty).
            if ( m_progress.m_hasSpare ) swapWeapon(), unequipSpare();
        }
        else if ( r.m_slot >= 0 && action == 0 )
            unequip( r.m_slot );
        else if ( r.m_skill >= 0 && action == 0 )
        {
            // Learn a rank: a point, the hero's level, not past the top.
            const SkillDef &def = skillDefs()[r.m_skill];
            int            &k = m_progress.m_ranks[r.m_skill];
            if ( m_progress.m_points > 0 && k < MaxRank && m_progress.m_heroLevel >= def.m_level )
            {
                ++k;
                --m_progress.m_points;
                refreshStats();
                m_audio.play( SfxE::PICKUP, m_pos, m_camTarget, 0.7f );
            }
        }
        return;
    }
}

void Game::drawPanel( int w, int h, int tab, int sel )
{
    float       s = textScale( h );
    const Font &font = m_font;
    DrawRectangle( 0, 0, w, h, { 3, 5, 8, 248 } );
    // As in Dark Alliance: what is carried at the top, the pages as signs, the page's name.
    float x0 = w * 0.03f, x1 = w * 0.485f;
    label( font, TextFormat( tr( "inv.bag" ), (int)m_progress.m_bag.size(), BagSize ), x0, 20.0f * s, 22.0f * s,
           { 255, 190, 120, 235 } );
    label( font, TextFormat( "%s %d   ·   %s %d", tr( "stat.scrap" ), m_scrap, tr( "stat.stims" ), m_stims ), x1,
           20.0f * s, 22.0f * s, { 235, 230, 220, 240 }, 2 );
    const char *names[PanelTabs] = { "panel.weapons",   "panel.armour", "panel.tech", "panel.modules", "panel.skills",
                                     "panel.character", "panel.notes",  "panel.map",  "panel.codex",   "panel.awards" };
    float       gap = 6.0f * s, tile = fminf( 64.0f * s, ( x1 - x0 - gap * ( PanelTabs - 1 ) ) / PanelTabs );
    float       ty = 58.0f * s;
    for ( int i = 0; i < PanelTabs; ++i )
    {
        Rectangle r = { x0 + i * ( tile + gap ), ty, tile, tile };
        bool      on = i == tab;
        DrawRectangleRec( r, on ? Color{ 120, 70, 30, 235 } : Color{ 24, 26, 30, 235 } );
        DrawRectangleLinesEx( r, 2.0f * s, on ? Color{ 255, 170, 80, 240 } : Color{ 70, 70, 74, 220 } );
        tabSign( i, r, on ? Color{ 255, 215, 160, 255 } : Color{ 165, 165, 160, 230 } );
    }
    label( font, tr( names[tab] ), x0, ty + tile + 10.0f * s, 26.0f * s, { 255, 200, 130, 255 } );
    const char *help = tab < GearTabs && atStash() ? "panel.help.gear.stash"
                       : tab < GearTabs            ? "panel.help.gear"
                       : tab == TabSkills          ? "panel.help.skills"
                                                   : "panel.help";
    float       top = ty + tile + 52.0f * s;
    if ( tab < GearTabs )
    {
        drawGear( w, h, tab, sel, s, top );
        DrawRectangle( 0, (int)( h - 64.0f * s ), w, (int)( 64.0f * s ) + 1, { 3, 5, 8, 235 } );
        label( font, tr( help ), w * 0.5f, h - 46.0f * s, 20.0f * s, { 165, 165, 160, 230 }, 1 );
        return;
    }
    label( font, tr( help ), w * 0.5f, h - 46.0f * s, 20.0f * s, { 150, 150, 150, 220 }, 1 );
    float left = w * 0.08f, right = w * 0.94f;

    if ( tab == TabSkills || tab == TabNotes || tab == TabCodex || tab == TabAwards )
    {
        // The list on the left (scrolled so the chosen row shows), what it is on the right.
        std::vector<PanelRow> rows = rowsOf( tab );
        float                 listW = w * 0.4f, rowH = 38.0f * s, bottom = h - 80.0f * s;
        // Where the chosen row would sit, to scroll.
        float y = 0.0f, chosenY = 0.0f;
        int   index = 0;
        for ( const PanelRow &row : rows )
        {
            if ( row.m_header )
                y += 48.0f * s;
            else
            {
                if ( index++ == sel ) chosenY = y;
                y += rowH;
            }
        }
        float scroll = fmaxf( 0.0f, chosenY - ( bottom - top ) * 0.6f );
        y = top - scroll;
        index = 0;
        const PanelRow *chosen = nullptr;
        BeginScissorMode( 0, (int)top - 4, w, (int)( bottom - top ) );
        for ( const PanelRow &row : rows )
        {
            if ( row.m_header )
            {
                y += 12.0f * s;
                label( font, row.m_title.c_str(), left, y, 22.0f * s, { 255, 170, 90, 230 } );
                y += 36.0f * s;
                continue;
            }
            bool on = index++ == sel;
            if ( on )
            {
                chosen = &row;
                DrawRectangle( (int)( left - 12.0f * s ), (int)( y - 5.0f * s ), (int)( listW + 24.0f * s ),
                               (int)( 36.0f * s ), { 255, 170, 80, 40 } );
            }
            bool  carried = row.m_bag >= 0 || row.m_stash >= 0;
            Color title = carried ? row.m_color : ( on ? Color{ 255, 220, 170, 255 } : Color{ 210, 205, 195, 230 } );
            const Item *item = rowItem( row );
            if ( const Texture2D *pic = item ? pictureOf( *item ) : nullptr )
                DrawTexturePro( *pic, { 0, 0, (float)pic->width, (float)pic->height },
                                { left - 44.0f * s, y - 4.0f * s, 34.0f * s, 34.0f * s }, { 0, 0 }, 0.0f, WHITE );
            label( font, row.m_title.c_str(), left, y, 24.0f * s, title );
            label( font, row.m_value.c_str(), left + listW, y, 24.0f * s,
                   carried ? Color{ 170, 170, 165, 220 } : row.m_color, 2 );
            y += rowH;
        }
        EndScissorMode();
        if ( chosen )
        {
            float       x = w * 0.54f, dy = top;
            bool        carried = chosen->m_bag >= 0 || chosen->m_stash >= 0;
            Color       c = carried || chosen->m_slot >= 0 ? chosen->m_color : Color{ 245, 236, 222, 255 };
            const Item *item = rowItem( *chosen );
            if ( const Texture2D *pic = item ? pictureOf( *item ) : nullptr )
            {
                // The item's picture, framed in its rarity's colour.
                float size = 210.0f * s;
                DrawTexturePro( *pic, { 0, 0, (float)pic->width, (float)pic->height }, { x, dy, size, size }, { 0, 0 },
                                0.0f, WHITE );
                DrawRectangleLinesEx( { x, dy, size, size }, 2.0f * s, { c.r, c.g, c.b, 170 } );
                dy += size + 18.0f * s;
            }
            std::string title = carried ? chosen->m_title : chosen->m_slot >= 0 ? chosen->m_value : chosen->m_title;
            label( font, title.c_str(), x, dy, 34.0f * s, c );
            dy += 56.0f * s;
            for ( const std::string &line : wrap( font, chosen->m_detail, 24.0f * s, right - x ) )
            {
                label( font, line.c_str(), x, dy, 24.0f * s, { 215, 210, 200, 240 } );
                dy += 32.0f * s;
            }
            // For something in the bag or the stash: what it would replace.
            if ( carried )
            {
                const Item &it = *item;
                int         slot = gearIndex( it );
                if ( slot >= 0 && m_progress.m_worn[slot] )
                {
                    dy += 20.0f * s;
                    label( font, TextFormat( tr( "inv.instead" ), itemName( m_progress.m_gear[slot] ).c_str() ), x, dy,
                           22.0f * s, rarityColor( m_progress.m_gear[slot].m_rarity ) );
                }
            }
        }
        return;
    }
    if ( tab == TabCharacter )
    {
        // The portrait, the name, what they are; then their numbers as they stand.
        float x = left;
        auto  portrait = m_portraits.find( "hero" );
        if ( portrait != m_portraits.end() )
        {
            float     size = 400.0f * s;
            Texture2D t = portrait->second;
            DrawTexturePro( t, { 0, 0, (float)t.width, (float)t.height }, { left, top, size, size }, { 0, 0 }, 0.0f,
                            WHITE );
            DrawRectangleLinesEx( { left, top, size, size }, 2.0f * s, { 255, 180, 100, 120 } );
            label( font, TextFormat( tr( "hud.level" ), m_progress.m_heroLevel ), left, top + size + 16 * s, 30.0f * s,
                   { 255, 210, 140, 240 } );
            label( font, TextFormat( tr( "stat.xp" ), m_progress.m_xp, xpNeed( m_progress.m_heroLevel ) ), left,
                   top + size + 56 * s, 22.0f * s, { 200, 195, 185, 230 } );
            x = left + size + 50.0f * s;
        }
        const char *who = heroKey( m_heroKind );
        float       y = top;
        label( font, tr( std::string( "hero." ) + who ), x, y, 44.0f * s, { 245, 236, 222, 255 } );
        y += 62.0f * s;
        for ( const std::string &line :
              wrap( font, tr( std::string( "choose." ) + who + ".about" ), 22.0f * s, right - x ) )
        {
            label( font, line.c_str(), x, y, 22.0f * s, { 200, 195, 185, 240 } );
            y += 30.0f * s;
        }
        y += 20.0f * s;
        bool dorra = m_heroKind == HeroE::DORRA;
        auto stat2 = [&]( const char *key, const std::string &value )
        {
            label( font, tr( key ), x, y, 24.0f * s, { 255, 190, 120, 235 } );
            label( font, value.c_str(), right, y, 24.0f * s, { 235, 230, 220, 255 }, 2 );
            y += 34.0f * s;
        };
        float perSecond = dorra ? 1.0f / ( 0.55f * m_stats.m_interval ) : 1.0f / m_stats.m_interval;
        stat2( "stat.health", TextFormat( "%d / %d", (int)ceilf( m_hp ), (int)hpMax() ) );
        stat2( "stat.armor", TextFormat( "%d  (-%d %%)", (int)m_stats.m_armor, (int)( armorCut() * 100.0f ) ) );
        if ( m_stats.m_shield > 0.0f )
            stat2( "stat.shield", TextFormat( "%d / %d", (int)ceilf( m_shield ), (int)m_stats.m_shield ) );
        stat2( "stat.damage", TextFormat( m_stats.m_pellets > 1 ? "%d x %d" : "%d", (int)roundf( m_stats.m_damage ),
                                          m_stats.m_pellets ) );
        stat2( "stat.speed", TextFormat( "%.1f", perSecond ) );
        stat2( "stat.crit", TextFormat( "%d %%  (x%.1f)", (int)( BaseCritPct + stat( StatE::CRIT ) ),
                                        1.5f + stat( StatE::CRIT_DAMAGE ) / 100.0f ) );
        if ( stat( StatE::BURN ) > 0.0f ) stat2( "stat.burn", TextFormat( "%d %%", (int)stat( StatE::BURN ) ) );
        if ( stat( StatE::SHOCK ) > 0.0f ) stat2( "stat.shock", TextFormat( "%d %%", (int)stat( StatE::SHOCK ) ) );
        if ( stat( StatE::LEECH ) > 0.0f ) stat2( "stat.leech", TextFormat( "%d %%", (int)stat( StatE::LEECH ) ) );
        if ( stat( StatE::REGEN ) > 0.0f ) stat2( "stat.regen", TextFormat( "%.1f", stat( StatE::REGEN ) ) );
        if ( stat( StatE::FIND ) > 0.0f ) stat2( "stat.find", TextFormat( "+%d %%", (int)stat( StatE::FIND ) ) );
        stat2( "stat.stims", TextFormat( "%d", m_stims ) );
        stat2( "stat.scrap", TextFormat( "%d", m_scrap ) );
        stat2( "stat.place", tr( m_level.m_name ) );
        if ( !m_level.m_anomalies.empty() ) stat2( "stat.anomalies", anomalyNames() );
        return;
    }
    // The map: the whole of what has been seen, the place's name over it.
    label( font, tr( m_level.m_name ), w * 0.5f, top - 10.0f * s, 28.0f * s, { 245, 236, 222, 255 }, 1 );
    drawMap( { left, top + 40.0f * s, right - left, h - top - 140.0f * s }, 1.0f, s );
}

void Game::drawGear( int w, int h, int tab, int sel, float s, float top )
{
    const Font &font = m_font;
    float       x0 = w * 0.03f, x1 = w * 0.485f, bottom = h - 80.0f * s;
    // The right half: the hero where they stand, in what they wear; their name above, a few numbers below.
    if ( m_heroShot.id > 0 )
    {
        float sw = (float)m_heroShot.texture.width, sh = (float)m_heroShot.texture.height;
        DrawTexturePro( m_heroShot.texture, { sw * 0.25f, 0.0f, sw * 0.5f, -sh },
                        { w * 0.5f, 0.0f, w * 0.5f, (float)h }, { 0, 0 }, 0.0f, WHITE );
        DrawRectangleGradientH( (int)( w * 0.5f ), 0, (int)( 60 * s ), h, { 3, 5, 8, 248 }, { 3, 5, 8, 0 } );
        DrawRectangleGradientV( (int)( w * 0.5f ), 0, (int)( w * 0.5f ) + 1, (int)( 110 * s ), { 3, 5, 8, 200 },
                                { 3, 5, 8, 0 } );
    }
    float cx = w * 0.75f;
    label( font, tr( std::string( "hero." ) + heroKey( m_heroKind ) ), cx, 20.0f * s, 32.0f * s, { 245, 236, 222, 255 },
           1 );
    label( font, TextFormat( tr( "hud.level" ), m_progress.m_heroLevel ), cx, 60.0f * s, 22.0f * s,
           { 255, 210, 140, 240 }, 1 );
    {
        bool  dorra = m_heroKind == HeroE::DORRA;
        float perSecond = dorra ? 1.0f / ( 0.55f * m_stats.m_interval ) : 1.0f / m_stats.m_interval;
        std::vector<std::pair<const char *, std::string>> nums = {
            { "stat.health", TextFormat( "%d / %d", (int)ceilf( m_hp ), (int)hpMax() ) },
            { "stat.armor", TextFormat( "%d", (int)m_stats.m_armor ) },
            { "stat.brief.damage", TextFormat( m_stats.m_pellets > 1 ? "%d x %d" : "%d",
                                               (int)roundf( m_stats.m_damage ), m_stats.m_pellets ) },
            { "stat.brief.speed", TextFormat( "%.1f", perSecond ) }
        };
        if ( m_stats.m_shield > 0.0f ) nums.push_back( { "stat.shield", TextFormat( "%d", (int)m_stats.m_shield ) } );
        float bw = w * 0.44f, bx = cx - bw * 0.5f, by = h - 150.0f * s, colW = bw / nums.size();
        DrawRectangle( (int)bx, (int)by, (int)bw, (int)( 62 * s ), { 0, 0, 0, 150 } );
        for ( size_t i = 0; i < nums.size(); ++i )
        {
            float x = bx + colW * ( i + 0.5f );
            label( font, fit( font, tr( nums[i].first ), 16.0f * s, colW - 8 * s ).c_str(), x, by + 6 * s, 16.0f * s,
                   { 255, 190, 120, 230 }, 1 );
            label( font, nums[i].second.c_str(), x, by + 28 * s, 24.0f * s, { 240, 235, 225, 255 }, 1 );
        }
    }
    // The cards: the chosen one opened with all it is.
    std::vector<PanelRow>                 rows = gearRows( tab );
    float                                 cardH = 96.0f * s, pic = cardH - 14.0f * s, textW = x1 - x0 - pic - 30.0f * s;
    std::vector<std::vector<std::string>> details( rows.size() );
    float                                 y = 0.0f, chosenTop = 0.0f, chosenBottom = 0.0f;
    int                                   index = 0;
    for ( size_t i = 0; i < rows.size(); ++i )
    {
        const PanelRow &row = rows[i];
        if ( row.m_header )
        {
            y += row.m_free ? 34.0f * s : 46.0f * s;
            continue;
        }
        float hgt = cardH;
        if ( index++ == sel )
        {
            std::vector<std::string> parts;
            for ( size_t at = 0;; )
            {
                size_t next = row.m_detail.find( " | ", at );
                parts.push_back( row.m_detail.substr( at, next == std::string::npos ? std::string::npos : next - at ) );
                if ( next == std::string::npos ) break;
                at = next + 3;
            }
            const Item *it = rowItem( row );
            bool        numbers =
                it && ( itemBases()[it->m_base].m_slot == SlotE::WEAPON ||
                        itemBases()[it->m_base].m_slot == SlotE::EMITTER || itemBases()[it->m_base].m_armor > 0.0f );
            for ( size_t p = numbers ? 2 : 1; p + 1 < parts.size(); ++p )
                for ( const std::string &line : wrap( font, parts[p], 19.0f * s, x1 - x0 - 24.0f * s ) )
                    details[i].push_back( line );
            hgt += details[i].size() * 25.0f * s + 10.0f * s;
            chosenTop = y, chosenBottom = y + hgt;
        }
        y += hgt + 8.0f * s;
    }
    float view = bottom - top, scroll = 0.0f;
    if ( chosenBottom - scroll > view ) scroll = chosenBottom - view;
    if ( chosenTop < scroll ) scroll = chosenTop;
    BeginScissorMode( (int)( x0 - 4 ), (int)( top - 4 ), (int)( x1 - x0 + 8 ), (int)( view + 8 ) );
    y = top - scroll;
    index = 0;
    const char *kinds[4] = { "dmg.kinetic", "dmg.heat", "dmg.shock", "dmg.acid" };
    const Color kindColor[4] = {
        { 205, 205, 200, 255 }, { 255, 140, 70, 255 }, { 120, 185, 255, 255 }, { 150, 230, 90, 255 }
    };
    if ( rows.empty() ) label( font, tr( "inv.none" ), x0, y, 22.0f * s, { 150, 150, 150, 220 } );
    for ( size_t i = 0; i < rows.size(); ++i )
    {
        const PanelRow &row = rows[i];
        if ( row.m_header )
        {
            label( font, row.m_title.c_str(), x0 + 4 * s, y + ( row.m_free ? 4.0f : 14.0f ) * s,
                   ( row.m_free ? 18.0f : 22.0f ) * s,
                   row.m_free ? Color{ 150, 150, 145, 220 } : Color{ 255, 170, 90, 230 } );
            y += row.m_free ? 34.0f * s : 46.0f * s;
            continue;
        }
        bool        on = index++ == sel;
        const Item *item = rowItem( row );
        float       hgt = cardH + ( on ? details[i].size() * 25.0f * s + 10.0f * s : 0.0f );
        bool        worn = row.m_slot >= 0;
        Color       bg = on                 ? Color{ 150, 118, 40, 228 }
                         : worn             ? Color{ 44, 86, 50, 220 }
                         : row.m_spare      ? Color{ 38, 70, 96, 220 }
                         : row.m_stash >= 0 ? Color{ 34, 36, 46, 220 }
                                            : Color{ 24, 26, 30, 230 };
        DrawRectangleRec( { x0, y, x1 - x0, hgt }, bg );
        DrawRectangleLinesEx( { x0, y, x1 - x0, hgt }, 1.5f * s, { 255, 255, 255, (unsigned char)( on ? 90 : 30 ) } );
        if ( item )
        {
            if ( const Texture2D *p = pictureOf( *item ) )
                DrawTexturePro( *p, { 0, 0, (float)p->width, (float)p->height },
                                { x1 - pic - 7 * s, y + 7 * s, pic, pic }, { 0, 0 }, 0.0f, WHITE );
            const ItemBase &b = itemBases()[item->m_base];
            Color           name = on ? Color{ 255, 245, 225, 255 } : row.m_color;
            label( font, fit( font, row.m_title, 24.0f * s, textW ).c_str(), x0 + 12 * s, y + 8 * s, 24.0f * s, name );
            std::string sum = itemSummary( *item );
            label( font, fit( font, sum, 19.0f * s, textW ).c_str(), x0 + 12 * s, y + 40 * s, 19.0f * s,
                   { 225, 220, 210, 240 } );
            // A weapon's kind of damage, in its colour, after the numbers.
            if ( b.m_slot == SlotE::WEAPON )
            {
                int   k = weaponDamage( *item );
                float after = MeasureTextEx( font, ( sum + "  " ).c_str(), 19.0f * s, 1.0f ).x;
                if ( after + 60 * s < textW )
                    label( font, tr( kinds[k] ), x0 + 12 * s + after, y + 40 * s, 19.0f * s, kindColor[k] );
            }
            // What it is (worn, the second weapon, in the stash), its level, its scrap; against what is worn there.
            std::string status = row.m_spare        ? tr( "inv.spare" )
                                 : worn             ? tr( "inv.worn" )
                                 : row.m_stash >= 0 ? tr( "inv.instash" )
                                                    : "";
            if ( !status.empty() ) status += "  ·  ";
            status += std::string( tr( TextFormat( "rarity.%d", (int)item->m_rarity ) ) ) + "  ·  ";
            status += std::string( TextFormat( tr( "item.ilevel" ), item->m_level ) ) + "  ·  " +
                      TextFormat( tr( "item.scrapval" ), std::max( 1, itemValue( *item ) / 5 ) );
            label( font, fit( font, status, 17.0f * s, textW ).c_str(), x0 + 12 * s, y + 68 * s, 17.0f * s,
                   worn          ? Color{ 170, 235, 170, 240 }
                   : row.m_spare ? Color{ 160, 205, 255, 240 }
                                 : Color{ 175, 175, 170, 230 } );
            if ( !worn && !row.m_spare )
            {
                int         slot = gearIndex( *item );
                const Item *now = slot >= 0 && m_progress.m_worn[slot] ? &m_progress.m_gear[slot] : nullptr;
                float       mine = 0.0f, theirs = 0.0f;
                bool        has = true;
                auto        rate = [&]( const Item &it )
                {
                    const ItemBase &ib = itemBases()[it.m_base];
                    return itemDamage( it ) * std::max( 1, ib.m_pellets ) / fmaxf( 0.05f, ib.m_interval );
                };
                if ( b.m_slot == SlotE::WEAPON )
                    mine = rate( *item ), theirs = now ? rate( *now ) : 0.0f;
                else if ( b.m_slot == SlotE::EMITTER )
                    mine = itemShield( *item ), theirs = now ? itemShield( *now ) : 0.0f;
                else if ( b.m_armor > 0.0f )
                    mine = itemArmor( *item ), theirs = now ? itemArmor( *now ) : 0.0f;
                else
                    has = false;
                int delta = (int)roundf( mine - theirs );
                if ( has && delta != 0 )
                    label( font, TextFormat( "%+d", delta ), x1 - pic - 16 * s, y + 40 * s, 22.0f * s,
                           delta > 0 ? Color{ 120, 230, 120, 255 } : Color{ 240, 110, 100, 255 }, 2 );
            }
        }
        if ( on )
        {
            float dy = y + cardH;
            for ( const std::string &line : details[i] )
            {
                label( font, line.c_str(), x0 + 12 * s, dy, 19.0f * s, { 245, 238, 225, 250 } );
                dy += 25.0f * s;
            }
        }
        y += hgt + 8.0f * s;
    }
    EndScissorMode();
}

void Game::preparePanel( Gfx &gfx, int tab, float turn, int w, int h )
{
    if ( tab >= GearTabs ) return;
    if ( m_heroShot.id == 0 || m_heroShot.texture.width != w || m_heroShot.texture.height != h )
    {
        if ( m_heroShot.id ) UnloadRenderTexture( m_heroShot );
        m_heroShot = LoadRenderTexture( w, h );
        m_heroShotSig.clear();
    }
    // Drawn again only when the view turns or the hero wears something else.
    std::string sig = TextFormat( "%.3f", turn );
    for ( int i = 0; i < EquipSlots; ++i ) sig += m_progress.m_worn[i] ? saveItem( m_progress.m_gear[i] ) : "-";
    if ( sig == m_heroShotSig ) return;
    m_heroShotSig = sig;
    renderHero( gfx, &m_heroShot, turn );
}

void Game::renderHero( Gfx &gfx, const RenderTexture2D *out, float turn )
{
    // The photo mode's camera close in front of the hero (the picture's middle half shows them), the HUD off.
    Camera3D cam = m_cam;
    bool     photo = m_photo;
    Vector3  target = m_photoTarget;
    m_photo = true;
    m_photoTarget = Vector3Add( m_pos, { 0.0f, 0.95f, 0.0f } );
    float yaw = m_yaw + turn;
    m_cam.target = m_photoTarget;
    m_cam.position = Vector3Add( m_photoTarget, { sinf( yaw ) * 3.6f, 0.5f, cosf( yaw ) * 3.6f } );
    m_cam.up = { 0.0f, 1.0f, 0.0f };
    m_cam.fovy = 38.0f;
    m_cam.projection = CAMERA_PERSPECTIVE;
    m_viewLight = 1.35f;
    render( gfx, out );
    m_viewLight = 1.0f;
    m_cam = cam;
    m_photo = photo;
    m_photoTarget = target;
}

}   // namespace spaceopera
