/**
 * @file Level.cpp
 * @brief Reading a level file, the grid's queries, and the geometry with its dressing.
 * @author Rene Weller
 */
#include "Level.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace spaceopera
{

namespace
{

/** @brief A cheap hash of a cell and a salt. */
unsigned hashCell( int r, int c, int salt )
{
    unsigned h = (unsigned)( r * 73856093 ) ^ (unsigned)( c * 19349663 ) ^ (unsigned)( salt * 83492791 );
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ ( h >> 15 );
}

/** @brief Cuts the blanks off both ends. */
std::string trim( const std::string &s )
{
    size_t a = s.find_first_not_of( " \t\r" );
    size_t b = s.find_last_not_of( " \t\r" );
    return a == std::string::npos ? std::string() : s.substr( a, b - a + 1 );
}

/** @brief Splits at '|' and trims the parts. */
std::vector<std::string> fields( const std::string &s )
{
    std::vector<std::string> out;
    std::stringstream        ss( s );
    std::string              part;
    while ( std::getline( ss, part, '|' ) ) out.push_back( trim( part ) );
    return out;
}

const int DR[4] = { -1, 0, 1, 0 };   ///< north, east, south, west
const int DC[4] = { 0, 1, 0, -1 };

}   // namespace

bool Level::load( const std::string &path )
{
    char *text = LoadFileText( path.c_str() );
    if ( !text ) return false;
    std::stringstream in( text );
    UnloadFileText( text );

    enum
    {
        HEAD,
        MAP,
        LINES,
        HINTS
    } section = HEAD;
    std::vector<std::string> map;
    std::string              line;
    while ( std::getline( in, line ) )
    {
        if ( !line.empty() && line.back() == '\r' ) line.pop_back();
        std::string t = trim( line );
        if ( t == "[map]" )
        {
            section = MAP;
            continue;
        }
        if ( t == "[lines]" )
        {
            section = LINES;
            continue;
        }
        if ( t == "[hints]" )
        {
            section = HINTS;
            continue;
        }
        if ( section == MAP )
        {
            if ( t.empty() )
            {
                section = HEAD;
                continue;
            }
            map.push_back( line );
            continue;
        }
        if ( t.empty() || t[0] == '#' ) continue;
        if ( section == LINES )
        {
            auto f = fields( t );
            if ( f.size() >= 4 ) m_lines.push_back( { f[0], f[1], f[2], f[3] } );
            continue;
        }
        if ( section == HINTS )
        {
            auto f = fields( t );
            if ( f.size() >= 3 ) m_hints.push_back( { f[0], f[1], f[2] } );
            continue;
        }
        std::stringstream kv( t );
        std::string       key;
        kv >> key;
        if ( key == "name" )
            m_name = trim( t.substr( 4 ) );
        else if ( key == "act" )
            m_act = trim( t.substr( 3 ) );
        else if ( key == "gravity" )
            kv >> m_gravity;
        else if ( key == "fog" )
            kv >> m_fog.x >> m_fog.y >> m_fog.z >> m_fog.w;
        else if ( key == "fogcolor" )
            kv >> m_fogColor.x >> m_fogColor.y >> m_fogColor.z;
        else if ( key == "ambient" )
            kv >> m_ambUp.x >> m_ambUp.y >> m_ambUp.z >> m_ambDown.x >> m_ambDown.y >> m_ambDown.z;
        else if ( key == "walls" )
            kv >> m_wallHeight;
    }

    m_rows = (int)map.size();
    m_cols = 0;
    for ( auto &row : map ) m_cols = std::max( m_cols, (int)row.size() );
    m_cells.assign( (size_t)m_rows * m_cols, Cell() );
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            char  ch = c < (int)map[r].size() ? map[r][c] : '#';
            Cell &cell = m_cells[(size_t)r * m_cols + c];
            if ( ch == '#' ) continue;
            cell.m_kind = CellE::FLOOR;
            if ( ch == '~' )
                cell.m_kind = CellE::VOID;
            else if ( ch == '=' )
                cell.m_kind = CellE::GRATE;
            else if ( ch == 'F' )
                cell.m_kind = CellE::FIN, cell.m_height = FinHeight;
            else if ( ch >= '1' && ch <= '9' )
                cell.m_height = ( ch - '0' ) * 0.5f;
            else if ( ch >= 'a' && ch <= 'z' )
                cell.m_trigger = ch;
            else if ( ch >= 'A' && ch <= 'Z' )
                m_markers.push_back( { ch, r, c, {} } );
        }
    for ( Marker &m : m_markers ) m.m_pos = centre( m.m_row, m.m_col );
    return m_rows > 0;
}

const Cell &Level::cell( int r, int c ) const
{
    static const Cell rock;
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols ) return rock;
    return m_cells[(size_t)r * m_cols + c];
}

void Level::cellOf( Vector3 p, int &r, int &c ) const
{
    c = (int)floorf( p.x / CellSize );
    r = (int)floorf( p.z / CellSize );
}

Vector3 Level::centre( int r, int c ) const
{
    const Cell &k = cell( r, c );
    float       y = k.m_kind == CellE::FIN ? FinHeight : ( k.m_kind == CellE::VOID ? 0.0f : k.m_height );
    return { ( c + 0.5f ) * CellSize, y, ( r + 0.5f ) * CellSize };
}

float Level::floorAt( float x, float z ) const
{
    const Cell &k = cell( (int)floorf( z / CellSize ), (int)floorf( x / CellSize ) );
    switch ( k.m_kind )
    {
    case CellE::ROCK: return 1000.0f;
    case CellE::VOID: return -1000.0f;
    case CellE::FIN: return FinHeight;
    default: return k.m_height;
    }
}

bool Level::walkable( int r, int c ) const
{
    CellE k = cell( r, c ).m_kind;
    return k == CellE::FLOOR || k == CellE::GRATE || k == CellE::FIN;
}

bool Level::collide( Vector3 &pos, float radius, float step ) const
{
    bool touched = false;
    for ( int pass = 0; pass < 2; ++pass )
    {
        int r0, c0;
        cellOf( pos, r0, c0 );
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
            {
                int         r = r0 + dr;
                int         c = c0 + dc;
                const Cell &k = cell( r, c );
                float       top = k.m_kind == CellE::ROCK   ? 1e9f
                                  : k.m_kind == CellE::VOID ? -1e9f
                                  : k.m_kind == CellE::FIN  ? FinHeight
                                                            : k.m_height;
                if ( top <= pos.y + step ) continue;
                float x0 = c * CellSize;
                float z0 = r * CellSize;
                float cx = Clamp( pos.x, x0, x0 + CellSize );
                float cz = Clamp( pos.z, z0, z0 + CellSize );
                float dx = pos.x - cx;
                float dz = pos.z - cz;
                float d2 = dx * dx + dz * dz;
                if ( d2 >= radius * radius ) continue;
                if ( d2 > 1e-8f )
                {
                    float d = sqrtf( d2 );
                    pos.x = cx + dx / d * radius;
                    pos.z = cz + dz / d * radius;
                }
                else
                {
                    // Inside the cell: out along the shortest way.
                    float left = pos.x - x0, right = x0 + CellSize - pos.x;
                    float up = pos.z - z0, down = z0 + CellSize - pos.z;
                    float m = std::min( std::min( left, right ), std::min( up, down ) );
                    if ( m == left )
                        pos.x = x0 - radius;
                    else if ( m == right )
                        pos.x = x0 + CellSize + radius;
                    else if ( m == up )
                        pos.z = z0 - radius;
                    else
                        pos.z = z0 + CellSize + radius;
                }
                touched = true;
            }
    }
    return touched;
}

bool Level::raycast( Vector3 from, Vector3 to, Vector3 &hit, Vector3 &normal ) const
{
    Vector3 d = Vector3Subtract( to, from );
    float   len = Vector3Length( d );
    int     steps = std::max( 1, (int)( len / 0.08f ) );
    Vector3 prev = from;
    int     pr, pc;
    cellOf( from, pr, pc );
    for ( int i = 1; i <= steps; ++i )
    {
        Vector3 p = Vector3Add( from, Vector3Scale( d, (float)i / steps ) );
        int     r, c;
        cellOf( p, r, c );
        const Cell &k = cell( r, c );
        float       top = k.m_kind == CellE::ROCK    ? 1e9f
                          : k.m_kind == CellE::VOID  ? -PitDepth
                          : k.m_kind == CellE::FIN   ? FinHeight
                          : k.m_kind == CellE::GRATE ? 0.0f
                                                     : k.m_height;
        if ( p.y < top )
        {
            hit = prev;
            if ( prev.y >= top && k.m_kind != CellE::ROCK )
                normal = { 0.0f, 1.0f, 0.0f };
            else if ( c != pc )
                normal = { c > pc ? -1.0f : 1.0f, 0.0f, 0.0f };
            else if ( r != pr )
                normal = { 0.0f, 0.0f, r > pr ? -1.0f : 1.0f };
            else
                normal = Vector3Normalize( Vector3Negate( d ) );
            return true;
        }
        prev = p;
        pr = r;
        pc = c;
    }
    hit = to;
    normal = { 0.0f, 1.0f, 0.0f };
    return false;
}

void Level::build( const LevelSurfaces &s, std::vector<Drawable> &out )
{
    MeshBuilder rock, floor, grate, metal, hazard, rust, foil, fins;
    struct Glow
    {
        Vector3 m_at;
        Vector3 m_half;
        Vector4 m_glow;
    };
    std::vector<Glow> glows;

    auto isVoid = [&]( int r, int c ) { return cell( r, c ).m_kind == CellE::VOID; };
    auto open = [&]( int r, int c ) { return cell( r, c ).m_kind != CellE::ROCK; };
    auto nearVoid = [&]( int r, int c )
    {
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
                if ( isVoid( r + dr, c + dc ) ) return true;
        return false;
    };

    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            const Cell &k = cell( r, c );
            float       x0 = c * CellSize;
            float       z0 = r * CellSize;
            Vector3     mid = { x0 + CellSize * 0.5f, 0.0f, z0 + CellSize * 0.5f };
            if ( k.m_kind == CellE::ROCK )
            {
                bool border = false;
                for ( int d = 0; d < 4; ++d ) border = border || open( r + DR[d], c + DC[d] );
                // Every rock cell gets a dark cap (seen from above); the ones by a tunnel a full block.
                rock.m_color = { 70, 66, 64, 255 };
                rock.box( place( { mid.x, m_wallHeight + 0.01f, mid.z } ),
                          { CellSize * 0.5f, 0.01f, CellSize * 0.5f } );
                rock.m_color = WHITE;
                if ( border )
                {
                    float bottom = nearVoid( r, c ) ? -PitDepth : -0.5f;
                    rock.box( place( { mid.x, ( bottom + m_wallHeight ) * 0.5f, mid.z } ),
                              { CellSize * 0.5f, ( m_wallHeight - bottom ) * 0.5f, CellSize * 0.5f } );
                }
                continue;
            }
            if ( k.m_kind == CellE::VOID )
            {
                // The shaft: a faint heat glow far down, and girders across at depth.
                glows.push_back( { { mid.x, -PitDepth, mid.z },
                                   { CellSize * 0.5f, 0.05f, CellSize * 0.5f },
                                   { 0.35f, 0.1f, 0.03f, 0.0f } } );
                unsigned h = hashCell( r, c, 7 );
                if ( h % 5 == 0 )
                    metal.box( place( { mid.x, -4.0f - ( h >> 8 ) % 4 * 3.5f, mid.z },
                                      { 0.0f, ( h >> 4 ) % 2 ? 0.0f : PI * 0.5f, 0.0f } ),
                               { 0.15f, 0.25f, CellSize * 0.5f } );
            }
            else if ( k.m_kind == CellE::GRATE )
            {
                grate.box( place( { mid.x, -0.06f, mid.z } ), { CellSize * 0.5f, 0.06f, CellSize * 0.5f } );
                for ( int d = 0; d < 4; ++d )
                {
                    if ( !isVoid( r + DR[d], c + DC[d] ) ) continue;
                    // Rails on the sides over the shaft.
                    Vector3 edge = { mid.x + DC[d] * 0.9f, 0.0f, mid.z + DR[d] * 0.9f };
                    float   yaw = DC[d] != 0 ? 0.0f : PI * 0.5f;
                    metal.box( place( { edge.x, 1.0f, edge.z }, { 0.0f, yaw, 0.0f } ),
                               { 0.03f, 0.03f, CellSize * 0.5f } );
                    metal.box( place( { edge.x, 0.5f, edge.z } ), { 0.03f, 0.5f, 0.03f } );
                }
            }
            else if ( k.m_kind == CellE::FIN )
            {
                // A heat exchanger: plates between a base and a grating top, pipes through them.
                metal.box( place( { mid.x, -0.25f + 0.25f, mid.z } ), { CellSize * 0.5f, 0.25f, CellSize * 0.5f } );
                for ( int i = 0; i < 9; ++i )
                    fins.box( place( { x0 + 0.12f + i * 0.22f, 1.35f, mid.z } ),
                              { 0.025f, 1.0f, CellSize * 0.5f - 0.06f } );
                grate.box( place( { mid.x, FinHeight - 0.06f, mid.z } ), { CellSize * 0.5f, 0.06f, CellSize * 0.5f } );
                for ( float y : { 0.9f, 1.7f } )
                    rust.cylinder( place( { x0, y, mid.z }, { 0.0f, 0.0f, -PI * 0.5f } ), 0.09f, CellSize, 10 );
                for ( float dx : { 0.05f, CellSize - 0.05f } )
                    metal.box( place( { x0 + dx, 1.25f, mid.z } ), { 0.05f, 1.25f, CellSize * 0.5f } );
            }
            else
            {
                float h = k.m_height;
                floor.box( place( { mid.x, ( h - 0.5f ) * 0.5f, mid.z } ),
                           { CellSize * 0.5f, ( h + 0.5f ) * 0.5f, CellSize * 0.5f } );
                if ( hashCell( r, c, 3 ) % 9 == 0 )
                    grate.box( place( { mid.x, h + 0.01f, mid.z } ), { 0.7f, 0.012f, 0.7f } );
            }

            // Under a floor next to a shaft: the shaft's wall.
            if ( k.m_kind != CellE::VOID && k.m_kind != CellE::GRATE && nearVoid( r, c ) )
                rock.box( place( { mid.x, ( -PitDepth - 0.5f ) * 0.5f, mid.z } ),
                          { CellSize * 0.5f, ( PitDepth - 0.5f ) * 0.5f, CellSize * 0.5f } );

            float base = k.m_kind == CellE::FLOOR ? k.m_height : 0.0f;
            for ( int d = 0; d < 4; ++d )
            {
                int nr = r + DR[d];
                int nc = c + DC[d];
                if ( k.m_kind == CellE::FLOOR && isVoid( nr, nc ) )
                {
                    // A hazard stripe along the edge of a shaft.
                    Vector3 e = { mid.x + DC[d] * 0.82f, base + 0.008f, mid.z + DR[d] * 0.82f };
                    hazard.box( place( e ),
                                { DC[d] ? 0.18f : CellSize * 0.5f, 0.008f, DR[d] ? 0.18f : CellSize * 0.5f } );
                }
                if ( open( nr, nc ) ) continue;
                // A wall face: lining, ribs, pipes.
                Vector3  n = { (float)-DC[d], 0.0f, (float)-DR[d] };   // into this cell
                Vector3  face = { mid.x + DC[d] * CellSize * 0.5f, 0.0f, mid.z + DR[d] * CellSize * 0.5f };
                float    yaw = DC[d] != 0 ? PI * 0.5f : 0.0f;   // a box's x along the face
                unsigned hh = hashCell( r, c, 11 + d );
                float    y0 = k.m_kind == CellE::VOID ? -PitDepth + 2.0f : base;
                if ( k.m_kind != CellE::VOID && hh % 3 != 0 )
                {
                    Vector3 p = Vector3Add( face, Vector3Scale( n, 0.05f ) );
                    metal.box( place( { p.x, y0 + 1.25f, p.z }, { 0.0f, yaw, 0.0f } ), { 0.93f, 1.15f, 0.04f } );
                    if ( hh % 5 == 0 )
                        hazard.box( place( { p.x + n.x * 0.02f, y0 + 0.15f, p.z + n.z * 0.02f }, { 0.0f, yaw, 0.0f } ),
                                    { 0.93f, 0.15f, 0.04f } );
                }
                if ( ( r + c ) % 2 == 0 )
                {
                    Vector3 p = Vector3Add( face, Vector3Scale( n, 0.12f ) );
                    float   bottom = k.m_kind == CellE::VOID ? -PitDepth : base;
                    metal.roundedBox( place( { p.x, ( bottom + m_wallHeight ) * 0.5f, p.z }, { 0.0f, yaw, 0.0f } ),
                                      { 0.18f, ( m_wallHeight - bottom ) * 0.5f, 0.12f }, 0.04f, 2 );
                }
                // Pipes along the face (they line up from cell to cell).
                Vector3 along = { DR[d] != 0 ? 1.0f : 0.0f, 0.0f, DC[d] != 0 ? 1.0f : 0.0f };
                Vector3 start = Vector3Subtract( face, Vector3Scale( along, CellSize * 0.5f ) );
                Matrix  axis = DR[d] != 0 ? MatrixRotateZ( -PI * 0.5f ) : MatrixRotateX( PI * 0.5f );
                for ( int i = 0; i < 2; ++i )
                {
                    float   y = k.m_kind == CellE::VOID ? -2.0f - i * 0.45f : base + ( i ? 3.75f : 3.35f );
                    float   rad = i ? 0.07f : 0.11f;
                    Vector3 p = Vector3Add( start, Vector3Scale( n, 0.28f + rad ) );
                    rust.cylinder( MatrixMultiply( axis, MatrixTranslate( p.x, y, p.z ) ), rad, CellSize, 10 );
                }
            }
        }

    // Markers that are only scenery: lamps, vents, the bodies.
    for ( const Marker &m : m_markers )
    {
        Vector3 p = m.m_pos;
        if ( m.m_kind == 'R' || m.m_kind == 'A' )
        {
            int wall = -1;
            for ( int d = 0; d < 4 && wall < 0; ++d )
                if ( !open( m.m_row + DR[d], m.m_col + DC[d] ) ) wall = d;
            bool    red = m.m_kind == 'R';
            Vector3 at;
            Vector3 n = { 0.0f, 0.0f, 1.0f };
            if ( wall >= 0 )
            {
                n = { (float)-DC[wall], 0.0f, (float)-DR[wall] };
                at = { p.x + DC[wall] * ( CellSize * 0.5f - 0.18f ), p.y + ( red ? 3.0f : 2.6f ),
                       p.z + DR[wall] * ( CellSize * 0.5f - 0.18f ) };
            }
            else
            {
                at = { p.x, p.y + 2.4f, p.z };
                metal.box( place( { p.x, p.y + 1.2f, p.z } ), { 0.04f, 1.2f, 0.04f } );
                for ( int i = 0; i < 3; ++i )
                {
                    float a = i * 2.094f;
                    metal.box(
                        place( { p.x + cosf( a ) * 0.35f, p.y + 0.3f, p.z + sinf( a ) * 0.35f }, { 0.0f, -a, 0.6f } ),
                        { 0.03f, 0.4f, 0.03f } );
                }
            }
            float yaw = fabsf( n.x ) > 0.5f ? PI * 0.5f : 0.0f;
            metal.roundedBox( place( at, { 0.0f, yaw, 0.0f } ), { 0.2f, 0.16f, 0.1f }, 0.04f, 2 );
            Vector4 glow = red ? Vector4{ 9.0f, 0.6f, 0.2f, 0.0f } : Vector4{ 7.0f, 4.0f, 1.5f, 0.0f };
            glows.push_back( { Vector3Add( at, Vector3Scale( n, 0.1f ) ),
                               { fabsf( n.x ) > 0.5f ? 0.02f : 0.15f, 0.1f, fabsf( n.z ) > 0.5f ? 0.02f : 0.15f },
                               glow } );
            unsigned h = hashCell( m.m_row, m.m_col, 5 );
            m_lights.push_back( { Vector3Add( at, Vector3Scale( n, 0.6f ) ),
                                  red ? Vector3{ 22.0f, 2.5f, 1.0f } : Vector3{ 26.0f, 16.0f, 6.5f },
                                  red ? 9.5f : 11.0f, h % 4 == 0 ? 0.7f : 0.0f } );
        }
        else if ( m.m_kind == 'V' )
        {
            grate.box( place( { p.x, p.y + 0.02f, p.z } ), { 0.6f, 0.02f, 0.6f } );
            rust.cylinder( place( { p.x, p.y, p.z } ), 0.45f, 0.01f, 16 );
        }
        else if ( m.m_kind == 'K' )
        {
            // The bodies, wrapped in thermal foil, stacked neatly against the wall on a pallet.
            metal.box( place( { p.x, p.y + 0.08f, p.z - 0.2f } ), { 1.0f, 0.08f, 0.7f } );
            for ( int i = 0; i < 7; ++i )
            {
                int   layer = i < 3 ? 0 : ( i < 5 ? 1 : 2 );
                int   slot = i < 3 ? i : ( i < 5 ? i - 3 : i - 5 );
                float x = p.x - 0.9f + 0.6f * slot + 0.3f * layer;
                float y = p.y + 0.32f + layer * 0.34f;
                foil.roundedBox( place( { x, y, p.z - 0.2f }, { 0.0f, 0.0f, 0.04f * ( slot - 1 ) } ),
                                 { 0.27f, 0.16f, 0.88f }, 0.12f, 3 );
            }
        }
    }

    auto add = [&]( MeshBuilder &b, int surface, bool cutaway, Vector4 glow = { -1.0f, 0.0f, 0.0f, 0.0f } )
    {
        for ( Mesh mesh : b.build() )
        {
            Drawable d;
            d.m_mesh = mesh;
            d.m_surface = surface;
            d.m_transform = MatrixIdentity();
            d.m_emissive = glow;
            d.m_cutaway = cutaway;
            out.push_back( d );
        }
    };
    add( rock, s.m_rock, true );
    add( floor, s.m_floor, false );
    add( grate, s.m_grate, false );
    add( metal, s.m_metal, true );
    add( hazard, s.m_hazard, false );
    add( rust, s.m_rust, true );
    add( foil, s.m_foil, false );
    add( fins, s.m_metal, true );
    // Glowing boxes of one colour go into one mesh.
    while ( !glows.empty() )
    {
        Vector4     colour = glows.front().m_glow;
        MeshBuilder b;
        for ( auto it = glows.begin(); it != glows.end(); )
            if ( Vector4Equals( it->m_glow, colour ) )
            {
                b.box( place( it->m_at ), it->m_half );
                it = glows.erase( it );
            }
            else
                ++it;
        add( b, s.m_glow, false, colour );
    }
}

}   // namespace spaceopera
