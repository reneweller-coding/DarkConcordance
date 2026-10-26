/**
 * @file GameUse.cpp
 * @brief What the hero works with A / E: doors that slide open (some locked until something has happened), lockers to
 *        search for scrap and stims, terminals with something to read (kept, the inventory lists it).
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"
#include "Text.h"

#include <raymath.h>

#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float DoorReach = 2.3f;   ///< from a door's middle
constexpr float UseReach = 1.2f;    ///< from where one stands before a locker or a terminal
constexpr int   StimLimit = 9;

}   // namespace

void Game::loadUsables()
{
    const int DR[4] = { -1, 0, 1, 0 }, DC[4] = { 0, 1, 0, -1 };
    auto      open = [&]( int r, int c ) { return m_level.cell( r, c ).m_kind != CellE::ROCK; };
    // The doors, and what locks them.
    for ( int r = 0; r < m_level.rows(); ++r )
        for ( int c = 0; c < m_level.cols(); ++c )
        {
            if ( m_level.cell( r, c ).m_kind != CellE::DOOR ) continue;
            Usable u;
            u.m_row = r;
            u.m_col = c;
            u.m_pos = m_level.centre( r, c );
            u.m_alongX = !open( r, c - 1 ) || !open( r, c + 1 );
            auto lock = m_level.m_locks.find( r * 1000 + c );
            if ( lock != m_level.m_locks.end() ) u.m_key = lock->second;
            m_usables.push_back( u );
        }
    // Lockers and terminals stand against a wall of their cell; one stands before them.
    for ( const Marker &m : m_level.m_markers )
    {
        if ( m.m_kind != '&' && m.m_kind != '!' ) continue;
        int wall = -1;
        for ( int d = 0; d < 4 && wall < 0; ++d )
            if ( !open( m.m_row + DR[d], m.m_col + DC[d] ) ) wall = d;
        Vector3 toWall = wall >= 0 ? Vector3{ (float)DC[wall], 0.0f, (float)DR[wall] } : Vector3{ 0.0f, 0.0f, -1.0f };
        Usable  u;
        u.m_kind = m.m_kind == '&' ? 1 : 2;
        u.m_row = m.m_row;
        u.m_col = m.m_col;
        u.m_out = Vector3Negate( toWall );
        u.m_at = Vector3Add( m.m_pos, Vector3Scale( toWall, CellSize * 0.5f - 0.3f ) );
        u.m_pos = Vector3Add( u.m_at, Vector3Scale( u.m_out, 0.95f ) );
        auto note = m_level.m_notes.find( m.m_row * 1000 + m.m_col );
        if ( note != m_level.m_notes.end() ) u.m_key = note->second;
        m_usables.push_back( u );
        m_level.m_props.push_back( { u.m_at, 0.42f } );
    }

    // A door's leaf (half the opening) with a hazard band; a locker and its door (hinged on the left); a lamp.
    MeshBuilder leaf;
    leaf.box( place( { 0.0f, 0.0f, 0.0f } ), { CellSize * 0.25f - 0.02f, DoorHeight * 0.5f, 0.07f } );
    m_leafMesh = leaf.build().front();
    MeshBuilder band;
    band.box( place( { 0.0f, -0.15f, 0.0f } ), { CellSize * 0.25f - 0.02f, 0.09f, 0.075f } );
    m_leafBand = band.build().front();
    MeshBuilder locker;
    locker.roundedBox( place( { 0.0f, 1.0f, 0.0f } ), { 0.42f, 1.0f, 0.25f }, 0.03f, 2 );
    m_lockerMesh = locker.build().front();
    MeshBuilder door;
    door.box( place( { 0.4f, 1.0f, 0.0f } ), { 0.4f, 0.94f, 0.02f } );
    for ( int i = 0; i < 4; ++i ) door.box( place( { 0.4f, 1.6f + i * 0.06f, 0.02f } ), { 0.25f, 0.012f, 0.012f } );
    m_lockerDoor = door.build().front();
    MeshBuilder lamp;
    lamp.box( place( { 0.0f, 0.0f, 0.0f } ), { 0.09f, 0.05f, 0.09f } );
    m_doorLamp = lamp.build().front();
}

bool Game::updateUse( float dt, GameInput &in )
{
    m_toastTimer -= dt;
    for ( Usable &u : m_usables )
        if ( u.m_used && u.m_open < 1.0f )
        {
            u.m_open = fminf( 1.0f, u.m_open + dt * ( u.m_kind == 0 ? 1.4f : 3.0f ) );
            if ( u.m_kind == 0 && u.m_open > 0.45f ) m_level.openDoor( u.m_row, u.m_col );
        }
    // A terminal's text on screen: any of the buttons puts it away.
    if ( !m_reading.empty() )
    {
        if ( in.m_jump || in.m_use || in.m_confirm || in.m_special ) m_reading.clear();
        return true;
    }
    m_near = -1;
    if ( m_dead || m_finished || m_scene > 0 || m_talk || m_menu ) return false;
    if ( m_level.m_peaceful && nearNpc() >= 0 ) return false;
    float best = 1e9f;
    for ( size_t i = 0; i < m_usables.size(); ++i )
    {
        const Usable &u = m_usables[i];
        if ( u.m_used && u.m_kind != 2 ) continue;
        float d = Vector2Distance( { m_pos.x, m_pos.z }, { u.m_pos.x, u.m_pos.z } );
        if ( d < ( u.m_kind == 0 ? DoorReach : UseReach ) && d < best ) best = d, m_near = (int)i;
    }
    if ( m_near < 0 || !( in.m_use || in.m_jump ) || !m_ground ) return false;
    in.m_use = false;
    in.m_jump = false;
    Usable &u = m_usables[m_near];
    if ( u.m_kind == 0 )
    {
        if ( !u.m_key.empty() && !m_fired.count( u.m_key ) )
        {
            m_toast = tr( "use.locked" );
            m_toastTimer = 2.0f;
            m_audio.play( SfxE::BLOCK, u.m_pos, m_camTarget, 0.5f );
            return false;
        }
        u.m_used = true;
        m_audio.play( SfxE::DOOR, u.m_pos, m_camTarget, 0.9f );
    }
    else if ( u.m_kind == 1 )
    {
        // What is in it is fixed by where it stands: mostly scrap, now and then a stim, sometimes nothing.
        u.m_used = true;
        unsigned h = (unsigned)( u.m_row * 92821 + u.m_col * 68917 ) % 10;
        if ( h < 6 )
        {
            int scrap = 6 + (int)h * 3;
            m_scrap += scrap;
            m_toast = TextFormat( tr( "use.scrap" ), scrap );
        }
        else if ( h < 9 && m_stims < StimLimit )
        {
            ++m_stims;
            m_toast = tr( "use.stim" );
        }
        else
            m_toast = tr( "use.empty" );
        m_toastColor = { 240, 235, 225, 255 };
        m_toastTimer = 2.5f;
        m_audio.play( SfxE::PICKUP, u.m_at, m_camTarget, 0.7f );
        // Often something more in it: a piece of gear.
        if ( ( h * 7u ) % 10 < 4 ) dropItem( Vector3Add( u.m_pos, { 0.0f, 1.0f, 0.0f } ), 1.2f );
    }
    else
    {
        // A terminal: its text, kept among the notes.
        m_reading = u.m_key.empty() ? "note.none" : u.m_key;
        if ( !u.m_key.empty() ) m_progress.m_flags.insert( "note:" + u.m_key );
    }
    return false;
}

void Game::poseUsables( std::vector<Drawable> &out )
{
    for ( const Usable &u : m_usables )
    {
        Drawable d;
        d.m_cutaway = true;
        if ( u.m_kind == 0 )
        {
            // Two leaves slide apart into the jambs; a lamp over the door on both sides: red locked, amber shut,
            // green open.
            float slide = u.m_open * ( CellSize * 0.5f - 0.12f );
            for ( float side : { -1.0f, 1.0f } )
            {
                float   off = side * ( CellSize * 0.25f + slide );
                Vector3 p = { u.m_pos.x + ( u.m_alongX ? off : 0.0f ), DoorHeight * 0.5f,
                              u.m_pos.z + ( u.m_alongX ? 0.0f : off ) };
                d.m_transform =
                    MatrixMultiply( MatrixRotateY( u.m_alongX ? 0.0f : PI * 0.5f ), MatrixTranslate( p.x, p.y, p.z ) );
                d.m_mesh = m_leafMesh;
                d.m_surface = S_HULL;
                out.push_back( d );
                d.m_mesh = m_leafBand;
                d.m_surface = S_HAZARD;
                out.push_back( d );
            }
            bool locked = !u.m_key.empty() && !m_fired.count( u.m_key );
            d.m_mesh = m_doorLamp;
            d.m_surface = S_GLOW;
            d.m_emissive = locked     ? Vector4{ 4.0f, 0.3f, 0.15f, 0.0f }
                           : u.m_used ? Vector4{ 0.3f, 3.0f, 0.7f, 0.0f }
                                      : Vector4{ 3.5f, 1.9f, 0.3f, 0.0f };
            for ( float side : { -1.0f, 1.0f } )
            {
                Vector3 l = { u.m_pos.x + ( u.m_alongX ? 0.0f : side * 0.34f ), DoorHeight + 0.15f,
                              u.m_pos.z + ( u.m_alongX ? side * 0.34f : 0.0f ) };
                d.m_transform = MatrixTranslate( l.x, l.y, l.z );
                out.push_back( d );
            }
            continue;
        }
        float  yaw = atan2f( u.m_out.x, u.m_out.z );
        Matrix place = MatrixMultiply( MatrixRotateY( yaw ), MatrixTranslate( u.m_at.x, u.m_at.y, u.m_at.z ) );
        if ( u.m_kind == 1 )
        {
            // A locker, its door swinging open once searched.
            d.m_transform = place;
            d.m_mesh = m_lockerMesh;
            d.m_surface = S_METAL;
            out.push_back( d );
            d.m_transform = MatrixMultiply(
                MatrixMultiply( MatrixRotateY( -1.9f * u.m_open ), MatrixTranslate( -0.4f, 0.0f, 0.26f ) ), place );
            d.m_mesh = m_lockerDoor;
            d.m_surface = S_HULL;
            out.push_back( d );
            continue;
        }
        // A terminal, its screen amber.
        d.m_transform = place;
        d.m_mesh = m_terminalMesh;
        d.m_surface = S_METAL;
        out.push_back( d );
        d.m_mesh = m_screenMesh;
        d.m_surface = S_GLOW;
        float flicker = 0.85f + 0.15f * sinf( m_time * 11.0f + u.m_col ) * sinf( m_time * 1.7f );
        d.m_emissive = { 3.0f * flicker, 1.7f * flicker, 0.4f * flicker, 0.0f };
        out.push_back( d );
    }
}

void Game::drawUseHud( int w, int h, float s )
{
    auto text = [&]( const char *t, float x, float y, float size, Color c, bool centre = false )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        Vector2 p = { centre ? x - m.x * 0.5f : x, y };
        DrawTextEx( m_font, t, { p.x + 2.0f * s, p.y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
        DrawTextEx( m_font, t, p, size * s, 1.0f, c );
    };
    // The prompt over what is within reach.
    if ( m_near >= 0 && m_reading.empty() )
    {
        const Usable &u = m_usables[m_near];
        bool          locked = u.m_kind == 0 && !u.m_key.empty() && !m_fired.count( u.m_key );
        const char   *key = u.m_kind == 0   ? ( locked ? "use.locked" : "use.open" )
                            : u.m_kind == 1 ? "use.search"
                                            : "use.read";
        Vector3       at = u.m_kind == 0 ? Vector3Add( u.m_pos, { 0.0f, DoorHeight + 0.5f, 0.0f } )
                                         : Vector3Add( u.m_at, { 0.0f, 2.2f, 0.0f } );
        Vector2       p = GetWorldToScreenEx( at, m_cam, w, h );
        text( tr( key ), p.x, p.y, 22, locked ? Color{ 255, 120, 100, 235 } : Color{ 255, 220, 160, 235 }, true );
    }
    // What a locker held, or "locked".
    if ( m_toastTimer > 0.0f )
        text( m_toast.c_str(), w * 0.5f, h * 0.62f, 28,
              { m_toastColor.r, m_toastColor.g, m_toastColor.b,
                (unsigned char)( 255 * Clamp( m_toastTimer * 2.0f, 0.0f, 1.0f ) ) },
              true );
    // A terminal's text.
    if ( !m_reading.empty() )
    {
        float       pw = 960.0f * s, x0 = ( w - pw ) * 0.5f, y0 = h * 0.18f;
        std::string title = hasText( m_reading + ".title" ) ? tr( m_reading + ".title" ) : "";
        // Words into lines.
        std::vector<std::string> lines;
        std::string              line, word, body = tr( m_reading );
        float                    size = 26.0f, width = pw - 80.0f * s;
        auto                     flush = [&]()
        {
            std::string test = line.empty() ? word : line + " " + word;
            if ( MeasureTextEx( m_font, test.c_str(), size * s, 1.0f ).x > width && !line.empty() )
            {
                lines.push_back( line );
                line = word;
            }
            else
                line = test;
            word.clear();
        };
        for ( char ch : body )
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
        float ph = ( 130.0f + lines.size() * size * 1.35f ) * s;
        DrawRectangle( (int)x0, (int)y0, (int)pw, (int)ph, { 6, 10, 8, 235 } );
        DrawRectangleLinesEx( { x0, y0, pw, ph }, 2.0f * s, { 255, 180, 80, 140 } );
        text( title.c_str(), x0 + 40.0f * s, y0 + 28.0f * s, 28, { 255, 190, 100, 255 } );
        for ( size_t i = 0; i < lines.size(); ++i )
            text( lines[i].c_str(), x0 + 40.0f * s, y0 + ( 78.0f + i * size * 1.35f ) * s, size,
                  { 225, 215, 190, 255 } );
        text( tr( "use.close" ), x0 + pw - 40.0f * s - MeasureTextEx( m_font, tr( "use.close" ), 20 * s, 1.0f ).x,
              y0 + ph - 36.0f * s, 20, { 160, 160, 150, 255 } );
    }
}

}   // namespace spaceopera
