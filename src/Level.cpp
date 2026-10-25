/**
 * @file Level.cpp
 * @brief Reading a level file, the grid's queries, and the geometry with its dressing (per zone's theme), props and
 *        signs.
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

/** @brief The hash as a number in 0..1. */
float hash01( int r, int c, int salt )
{
    return ( hashCell( r, c, salt ) & 0xffff ) / 65535.0f;
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

/** @brief How a theme dresses its cells. */
struct Look
{
    int   m_walls;          ///< 0 rock, 1 stacked containers, 2 bulkheads
    Color m_lining;         ///< colour of the wall panels
    float m_liningChance;   ///< share of wall faces with panels
    float m_hazardChance;   ///< share of panels with a hazard band
    int   m_pipes;          ///< rows of pipes along the walls
    Color m_pipe;           ///< their colour
    Color m_floor;          ///< colour of the floors
    Color m_rock;           ///< colour of the rock
    float m_propChance;     ///< share of wall-side cells with a prop
};

const Look Looks[(int)ThemeE::COUNT] = {
    { 0,
      { 215, 205, 190, 255 },
      0.85f,
      0.15f,
      2,
      { 255, 255, 255, 255 },
      { 255, 248, 236, 255 },
      { 255, 255, 255, 255 },
      0.18f },
    { 0,
      { 150, 178, 210, 255 },
      0.7f,
      0.1f,
      3,
      { 175, 205, 240, 255 },
      { 195, 212, 235, 255 },
      { 185, 198, 220, 255 },
      0.14f },
    { 0,
      { 215, 140, 95, 255 },
      0.3f,
      0.3f,
      1,
      { 255, 195, 150, 255 },
      { 255, 228, 205, 255 },
      { 255, 210, 180, 255 },
      0.1f },
    { 0,
      { 236, 244, 252, 255 },
      1.0f,
      0.05f,
      2,
      { 220, 232, 250, 255 },
      { 222, 234, 248, 255 },
      { 210, 222, 240, 255 },
      0.16f },
    { 0,
      { 220, 168, 112, 255 },
      0.6f,
      0.5f,
      2,
      { 255, 205, 160, 255 },
      { 220, 196, 170, 255 },
      { 255, 232, 210, 255 },
      0.24f },
    { 1,
      { 200, 200, 200, 255 },
      0.0f,
      0.0f,
      0,
      { 255, 255, 255, 255 },
      { 205, 200, 190, 255 },
      { 255, 255, 255, 255 },
      0.3f },
    { 2,
      { 190, 196, 204, 255 },
      0.6f,
      0.2f,
      2,
      { 200, 190, 170, 255 },
      { 214, 212, 208, 255 },
      { 255, 255, 255, 255 },
      0.16f },
    { 2,
      { 225, 230, 236, 255 },
      0.95f,
      0.0f,
      1,
      { 220, 225, 235, 255 },
      { 225, 228, 235, 255 },
      { 255, 255, 255, 255 },
      0.45f },
    { 2,
      { 150, 112, 82, 255 },
      0.9f,
      0.0f,
      1,
      { 200, 150, 110, 255 },
      { 190, 160, 135, 255 },
      { 200, 170, 150, 255 },
      0.35f },
};

/** @brief Paint colours of the containers in a hold. */
const Color ContainerPaint[6] = { { 170, 70, 40, 255 },  { 60, 95, 140, 255 },   { 75, 120, 80, 255 },
                                  { 190, 140, 50, 255 }, { 120, 120, 125, 255 }, { 140, 55, 60, 255 } };

/** @brief A theme's name in the level file. */
ThemeE themeOf( const std::string &name )
{
    if ( name == "exchanger" ) return ThemeE::EXCHANGER;
    if ( name == "shaft" ) return ThemeE::SHAFT;
    if ( name == "coldstore" ) return ThemeE::COLDSTORE;
    if ( name == "works" ) return ThemeE::WORKS;
    if ( name == "cargo" ) return ThemeE::CARGO;
    if ( name == "gangway" ) return ThemeE::GANGWAY;
    if ( name == "bridge" ) return ThemeE::BRIDGE;
    if ( name == "bar" ) return ThemeE::BAR;
    return ThemeE::SERVICE;
}

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
        HINTS,
        NPCS,
        TALK
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
        if ( t == "[npcs]" )
        {
            section = NPCS;
            continue;
        }
        if ( t == "[talk]" )
        {
            section = TALK;
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
            if ( f.size() >= 3 ) m_lines.push_back( { f[0], f[1], f[2] } );
            continue;
        }
        if ( section == HINTS )
        {
            auto f = fields( t );
            if ( f.size() >= 2 ) m_hints.push_back( { f[0], f[1] } );
            continue;
        }
        if ( section == NPCS )
        {
            // name | row col | look | first node | flag:node ...
            auto f = fields( t );
            if ( f.size() < 4 ) continue;
            Npc               n;
            std::stringstream rc( f[1] );
            n.m_id = f[0];
            rc >> n.m_row >> n.m_col;
            n.m_look = f[2];
            n.m_start = f[3];
            if ( f.size() > 4 )
            {
                std::stringstream after( f[4] );
                std::string       pair;
                while ( after >> pair )
                {
                    size_t colon = pair.find( ':' );
                    if ( colon != std::string::npos )
                        n.m_after.push_back( { pair.substr( 0, colon ), pair.substr( colon + 1 ) } );
                }
            }
            m_npcs.push_back( n );
            continue;
        }
        if ( section == TALK )
        {
            // node | speaker | event | answer>node ...
            auto f = fields( t );
            if ( f.size() < 2 ) continue;
            TalkNode n;
            n.m_id = f[0];
            n.m_speaker = f[1];
            n.m_event = f.size() > 2 && f[2] != "-" ? f[2] : "";
            if ( f.size() > 3 )
            {
                std::stringstream answers( f[3] );
                std::string       pair;
                while ( answers >> pair )
                {
                    size_t arrow = pair.find( '>' );
                    if ( arrow != std::string::npos )
                        n.m_answers.push_back( { pair.substr( 0, arrow ), pair.substr( arrow + 1 ) } );
                }
            }
            m_talk.push_back( n );
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
        else if ( key == "zone" )
        {
            Zone        z;
            std::string theme;
            kv >> z.m_r0 >> z.m_c0 >> z.m_r1 >> z.m_c1 >> theme;
            z.m_theme = themeOf( theme );
            m_zones.push_back( z );
        }
        else if ( key == "sign" )
        {
            int         r = 0, c = 0;
            std::string texture;
            kv >> r >> c >> texture;
            m_signRequests.push_back( { texture, { r, c } } );
        }
        else if ( key == "sky" )
            kv >> m_sky;
        else if ( key == "thrust" )
        {
            int on = 0;
            kv >> on;
            m_thrust = on != 0;
        }
        else if ( key == "next" )
            kv >> m_next;
        else if ( key == "gate" )
            kv >> m_gate;
        else if ( key == "ambience" )
            kv >> m_ambience;
        else if ( key == "route" )
        {
            int r = 0, c = 0;
            while ( kv >> r >> c ) m_route.push_back( { r, c } );
        }
        else if ( key == "peaceful" )
        {
            int on = 0;
            kv >> on;
            m_peaceful = on != 0;
        }
        else if ( key == "view" )
            kv >> m_view;
        else if ( key == "back" )
            kv >> m_back;
        else if ( key == "autotalk" )
            kv >> m_autotalk;
        else if ( key == "exit" )
        {
            std::string to, flag;
            kv >> to >> flag;
            m_exits.push_back( { to, flag } );
        }
        else if ( key == "board" )
            kv >> m_boardRow >> m_boardCol >> m_boardWidth;
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
            else if ( ch == '_' )
                cell.m_kind = CellE::GLASS;
            else if ( ch == '+' )
                cell.m_kind = CellE::COUNTER;
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

void Level::setVoid( int r, int c )
{
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols ) return;
    Cell &k = m_cells[(size_t)r * m_cols + c];
    k.m_kind = CellE::VOID;
    k.m_height = 0.0f;
    k.m_trigger = 0;
}

ThemeE Level::theme( int r, int c ) const
{
    for ( const Zone &z : m_zones )
        if ( r >= z.m_r0 && r <= z.m_r1 && c >= z.m_c0 && c <= z.m_c1 ) return z.m_theme;
    return ThemeE::SERVICE;
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
    case CellE::COUNTER: return CounterHeight;
    default: return k.m_height;
    }
}

const TalkNode *Level::talk( const std::string &id ) const
{
    for ( const TalkNode &n : m_talk )
        if ( n.m_id == id ) return &n;
    return nullptr;
}

bool Level::walkable( int r, int c ) const
{
    CellE k = cell( r, c ).m_kind;
    return k == CellE::FLOOR || k == CellE::GRATE || k == CellE::FIN || k == CellE::GLASS;
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
                float       top = k.m_kind == CellE::ROCK      ? 1e9f
                                  : k.m_kind == CellE::VOID    ? -1e9f
                                  : k.m_kind == CellE::FIN     ? FinHeight
                                  : k.m_kind == CellE::COUNTER ? CounterHeight
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
    // Crates, barrels, scrap.
    for ( const Prop &p : m_props )
    {
        if ( pos.y > p.m_pos.y + 1.4f ) continue;
        float dx = pos.x - p.m_pos.x, dz = pos.z - p.m_pos.z;
        float d = sqrtf( dx * dx + dz * dz );
        float min = radius + p.m_radius;
        if ( d < min && d > 1e-4f )
        {
            pos.x = p.m_pos.x + dx / d * min;
            pos.z = p.m_pos.z + dz / d * min;
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
        float       top = k.m_kind == CellE::ROCK                                ? 1e9f
                          : k.m_kind == CellE::VOID                              ? -PitDepth
                          : k.m_kind == CellE::FIN                               ? FinHeight
                          : k.m_kind == CellE::GRATE || k.m_kind == CellE::GLASS ? 0.0f
                          : k.m_kind == CellE::COUNTER                           ? CounterHeight
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
    MeshBuilder rock, floor, grate, metal, hazard, rust, foil, fins, crate, box, hull, cloth;
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
    // Props keep away from markers (spawns, capsules, doors, the swarms) and from shafts.
    auto nearMarker = [&]( int r, int c )
    {
        for ( const Marker &m : m_markers )
            if ( m.m_kind != 'R' && m.m_kind != 'A' && abs( m.m_row - r ) <= 1 && abs( m.m_col - c ) <= 1 ) return true;
        for ( const Npc &n : m_npcs )
            if ( abs( n.m_row - r ) <= 1 && abs( n.m_col - c ) <= 1 ) return true;
        return false;
    };

    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            const Cell &k = cell( r, c );
            const Look &look = Looks[(int)theme( r, c )];
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
                rock.m_color = look.m_rock;
                if ( !border ) continue;
                if ( look.m_walls == 1 )
                {
                    // A hold: containers stacked two high, each its own paint, steel rails where they meet.
                    float h = m_wallHeight * 0.5f;
                    for ( int level = 0; level < 2; ++level )
                    {
                        box.m_color = ContainerPaint[hashCell( r / 3, c / 3, 90 + level ) % 6];
                        box.box( place( { mid.x, h * ( level + 0.5f ), mid.z } ),
                                 { CellSize * 0.5f, h * 0.5f - 0.03f, CellSize * 0.5f } );
                    }
                    box.m_color = WHITE;
                    for ( int d = 0; d < 4; ++d )
                    {
                        if ( !open( r + DR[d], c + DC[d] ) ) continue;
                        Vector3 face = { mid.x + DC[d] * ( CellSize * 0.5f + 0.02f ), 0.0f,
                                         mid.z + DR[d] * ( CellSize * 0.5f + 0.02f ) };
                        float   yaw = DC[d] != 0 ? PI * 0.5f : 0.0f;
                        for ( float y : { 0.06f, h - 0.06f, h + 0.06f, m_wallHeight - 0.06f } )
                            metal.box( place( { face.x, y, face.z }, { 0.0f, yaw, 0.0f } ),
                                       { CellSize * 0.5f, 0.06f, 0.03f } );
                        // Door bars on some faces.
                        if ( hashCell( r, c, 95 + d ) % 3 == 0 )
                            for ( float u : { -0.45f, -0.15f, 0.15f, 0.45f } )
                            {
                                Vector3 at = Vector3Add( face, { DR[d] != 0 ? u : 0.0f, 0.0f, DC[d] != 0 ? u : 0.0f } );
                                metal.box( place( { at.x, h * 0.5f, at.z } ), { 0.025f, h * 0.45f, 0.025f } );
                            }
                    }
                    continue;
                }
                float        bottom = nearVoid( r, c ) ? -PitDepth : -0.5f;
                MeshBuilder &wall = look.m_walls == 2 ? hull : rock;
                wall.m_color = look.m_rock;
                wall.box( place( { mid.x, ( bottom + m_wallHeight ) * 0.5f, mid.z } ),
                          { CellSize * 0.5f, ( m_wallHeight - bottom ) * 0.5f, CellSize * 0.5f } );
                wall.m_color = WHITE;
                continue;
            }
            if ( k.m_kind == CellE::GLASS )
            {
                // A window in the floor: only its frame here (the game lays the panes over open space).
                m_glass.push_back( { r, c } );
                for ( int d = 0; d < 4; ++d )
                {
                    if ( cell( r + DR[d], c + DC[d] ).m_kind == CellE::GLASS ) continue;
                    Vector3 e = { mid.x + DC[d] * ( CellSize * 0.5f - 0.06f ), 0.0f,
                                  mid.z + DR[d] * ( CellSize * 0.5f - 0.06f ) };
                    metal.box( place( { e.x, -0.25f, e.z } ),
                               { DC[d] ? 0.08f : CellSize * 0.5f, 0.27f, DR[d] ? 0.08f : CellSize * 0.5f } );
                }
                metal.box( place( { mid.x - CellSize * 0.5f, -0.04f, mid.z } ), { 0.025f, 0.04f, CellSize * 0.5f } );
                metal.box( place( { mid.x, -0.04f, mid.z - CellSize * 0.5f } ), { CellSize * 0.5f, 0.04f, 0.025f } );
                continue;
            }
            if ( k.m_kind == CellE::COUNTER )
            {
                // A bar counter: the deck under it, a dark body, a worn top, a warm strip of light along its front.
                floor.box( place( { mid.x, -0.25f, mid.z } ), { CellSize * 0.5f, 0.25f, CellSize * 0.5f } );
                crate.m_color = { 70, 52, 40, 255 };
                crate.box( place( { mid.x, CounterHeight * 0.5f - 0.03f, mid.z } ),
                           { CellSize * 0.5f, CounterHeight * 0.5f - 0.03f, CellSize * 0.42f } );
                crate.m_color = WHITE;
                metal.m_color = { 120, 105, 90, 255 };
                metal.box( place( { mid.x, CounterHeight - 0.03f, mid.z } ),
                           { CellSize * 0.5f, 0.03f, CellSize * 0.47f } );
                metal.m_color = WHITE;
                for ( int d = 0; d < 4; ++d )
                    if ( DR[d] != 0 && walkable( r + DR[d], c ) )
                        glows.push_back( { { mid.x, 0.9f, mid.z + DR[d] * CellSize * 0.43f },
                                           { CellSize * 0.5f, 0.015f, 0.01f },
                                           { 3.0f, 1.6f, 0.6f, 0.0f } } );
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
                metal.m_color = look.m_lining;
                metal.box( place( { mid.x, 0.25f, mid.z } ), { CellSize * 0.5f, 0.25f, CellSize * 0.5f } );
                for ( int i = 0; i < 9; ++i )
                    fins.box( place( { x0 + 0.12f + i * 0.22f, 1.35f, mid.z } ),
                              { 0.025f, 1.0f, CellSize * 0.5f - 0.06f } );
                grate.box( place( { mid.x, FinHeight - 0.06f, mid.z } ), { CellSize * 0.5f, 0.06f, CellSize * 0.5f } );
                for ( float y : { 0.9f, 1.7f } )
                    rust.cylinder( place( { x0, y, mid.z }, { 0.0f, 0.0f, -PI * 0.5f } ), 0.09f, CellSize, 10 );
                for ( float dx : { 0.05f, CellSize - 0.05f } )
                    metal.box( place( { x0 + dx, 1.25f, mid.z } ), { 0.05f, 1.25f, CellSize * 0.5f } );
                metal.m_color = WHITE;
            }
            else
            {
                float h = k.m_height;
                floor.m_color = look.m_floor;
                floor.box( place( { mid.x, ( h - 0.5f ) * 0.5f, mid.z } ),
                           { CellSize * 0.5f, ( h + 0.5f ) * 0.5f, CellSize * 0.5f } );
                floor.m_color = WHITE;
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
                // A wall face: lining, ribs, pipes (as the zone's theme has them).
                Vector3 n = { (float)-DC[d], 0.0f, (float)-DR[d] };   // into this cell
                Vector3 face = { mid.x + DC[d] * CellSize * 0.5f, 0.0f, mid.z + DR[d] * CellSize * 0.5f };
                float   yaw = DC[d] != 0 ? PI * 0.5f : 0.0f;   // a box's x along the face
                float   y0 = k.m_kind == CellE::VOID ? -PitDepth + 2.0f : base;
                if ( k.m_kind != CellE::VOID && hash01( r, c, 11 + d ) < look.m_liningChance )
                {
                    Vector3      p = Vector3Add( face, Vector3Scale( n, 0.05f ) );
                    MeshBuilder &lining = theme( r, c ) == ThemeE::SHAFT ? rust : metal;
                    lining.m_color = look.m_lining;
                    lining.box( place( { p.x, y0 + 1.25f, p.z }, { 0.0f, yaw, 0.0f } ), { 0.93f, 1.15f, 0.04f } );
                    lining.m_color = WHITE;
                    if ( hash01( r, c, 21 + d ) < look.m_hazardChance )
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
                rust.m_color = look.m_pipe;
                for ( int i = 0; i < look.m_pipes; ++i )
                {
                    float   y = k.m_kind == CellE::VOID ? -2.0f - i * 0.45f
                                                        : base + 3.35f + i * 0.38f - ( i == 2 ? 1.35f : 0.0f );
                    float   rad = i == 1 ? 0.07f : 0.11f;
                    Vector3 p = Vector3Add( start, Vector3Scale( n, 0.28f + rad ) );
                    rust.cylinder( MatrixMultiply( axis, MatrixTranslate( p.x, y, p.z ) ), rad, CellSize, 10 );
                }
                rust.m_color = WHITE;
            }

            // Props by the walls, as the theme has them.
            if ( k.m_kind != CellE::FLOOR || k.m_height != 0.0f || k.m_trigger || nearVoid( r, c ) ||
                 nearMarker( r, c ) )
                continue;
            int wall = -1, walk = 0;
            for ( int d = 0; d < 4; ++d )
            {
                if ( !open( r + DR[d], c + DC[d] ) && wall < 0 ) wall = d;
                if ( walkable( r + DR[d], c + DC[d] ) ) ++walk;
            }
            if ( wall < 0 || walk < 3 || hash01( r, c, 31 ) >= look.m_propChance ) continue;
            Vector3 n = { (float)-DC[wall], 0.0f, (float)-DR[wall] };
            Vector3 at = Vector3Subtract( mid, Vector3Scale( n, 0.35f ) );
            float   turn = hash01( r, c, 33 ) * 0.6f - 0.3f;
            switch ( theme( r, c ) )
            {
            case ThemeE::SERVICE:
                crate.roundedBox( place( { at.x, 0.42f, at.z }, { 0.0f, turn, 0.0f } ), { 0.5f, 0.42f, 0.45f }, 0.04f,
                                  2 );
                if ( hash01( r, c, 34 ) < 0.5f )
                    crate.roundedBox( place( { at.x, 1.08f, at.z }, { 0.0f, -turn, 0.0f } ), { 0.36f, 0.24f, 0.32f },
                                      0.04f, 2 );
                metal.box( place( { at.x, 0.42f, at.z }, { 0.0f, turn, 0.0f } ), { 0.52f, 0.05f, 0.47f } );
                m_props.push_back( { at, 0.6f } );
                break;
            case ThemeE::EXCHANGER:
                // A valve stand and a puddle of condensate.
                metal.cylinder( place( { at.x, 0.0f, at.z } ), 0.1f, 1.3f, 10 );
                rust.lathe( place( { at.x, 1.15f, at.z }, { PI * 0.5f, 0.0f, 0.0f } ),
                            { { 0.25f, -0.03f },
                              { 0.3f, -0.03f },
                              { 0.3f, -0.03f },
                              { 0.3f, 0.03f },
                              { 0.3f, 0.03f },
                              { 0.25f, 0.03f } },
                            16 );
                floor.m_color = { 30, 36, 46, 255 };
                floor.lathe( place( { mid.x, 0.006f, mid.z } ), { { 0.0f, 0.0f }, { 0.75f, 0.0f } }, 18 );
                floor.m_color = WHITE;
                m_props.push_back( { at, 0.25f } );
                break;
            case ThemeE::SHAFT:
                rust.lathe( place( { at.x, 0.0f, at.z }, { 0.0f, turn, PI * 0.5f } ),
                            { { 0.0f, -0.4f },
                              { 0.55f, -0.4f },
                              { 0.55f, -0.4f },
                              { 0.55f, -0.32f },
                              { 0.3f, -0.32f },
                              { 0.3f, 0.32f },
                              { 0.55f, 0.32f },
                              { 0.55f, 0.32f },
                              { 0.55f, 0.4f },
                              { 0.0f, 0.4f } },
                            18 );
                m_props.push_back( { at, 0.55f } );
                break;
            case ThemeE::COLDSTORE:
                foil.roundedBox( place( { at.x, 0.4f, at.z }, { 0.0f, turn, 0.0f } ), { 0.45f, 0.4f, 0.4f }, 0.12f, 3 );
                floor.m_color = { 235, 245, 255, 255 };
                floor.lathe( place( { mid.x + 0.3f, 0.006f, mid.z }, { 0.0f, turn, 0.0f } ),
                             { { 0.0f, 0.0f }, { 0.6f, 0.0f } }, 7 );
                floor.m_color = WHITE;
                m_props.push_back( { at, 0.55f } );
                break;
            case ThemeE::CARGO:
            {
                // What refugees carry: soft bags and bundles heaped by the wall, a small crate among them.
                const Color tint[4] = {
                    { 110, 120, 140, 255 }, { 140, 112, 82, 255 }, { 90, 110, 92, 255 }, { 150, 76, 66, 255 }
                };
                int bags = 2 + hashCell( r, c, 79 ) % 3;
                for ( int i = 0; i < bags; ++i )
                {
                    float   a = turn * 3.0f + i * 2.3f, d = 0.1f + 0.3f * hash01( r, c, 80 + i );
                    Vector3 half = { 0.22f + 0.14f * hash01( r, c, 86 + i ), 0.15f + 0.06f * hash01( r, c, 88 + i ),
                                     0.18f + 0.1f * hash01( r, c, 90 + i ) };
                    cloth.m_color = tint[hashCell( r, c, 84 + i ) % 4];
                    cloth.roundedBox(
                        place( { at.x + cosf( a ) * d, half.y + ( i == 3 ? 0.25f : 0.0f ), at.z + sinf( a ) * d },
                               { 0.15f * hash01( r, c, 92 + i ), a * 1.7f, 0.1f } ),
                        half, fminf( half.y, 0.13f ), 3 );
                }
                cloth.m_color = WHITE;
                if ( hashCell( r, c, 94 ) % 2 )
                    crate.roundedBox( place( { at.x - 0.25f, 0.22f, at.z + 0.1f }, { 0.0f, turn, 0.0f } ),
                                      { 0.24f, 0.22f, 0.2f }, 0.03f, 2 );
                m_props.push_back( { at, 0.6f } );
                break;
            }
            case ThemeE::GANGWAY:
            {
                // A locker against the wall.
                Vector3 l = Vector3Subtract( at, Vector3Scale( n, 0.2f ) );
                float   yaw = fabsf( n.x ) > 0.5f ? PI * 0.5f : 0.0f;
                metal.m_color = { 150, 160, 170, 255 };
                metal.roundedBox( place( { l.x, 1.0f, l.z }, { 0.0f, yaw, 0.0f } ), { 0.45f, 1.0f, 0.25f }, 0.03f, 2 );
                metal.m_color = WHITE;
                for ( int i = 0; i < 3; ++i )
                    rust.box( place( Vector3Add( { l.x, 1.6f - i * 0.08f, l.z }, Vector3Scale( n, 0.26f ) ),
                                     { 0.0f, yaw, 0.0f } ),
                              { 0.25f, 0.015f, 0.01f } );
                m_props.push_back( { l, 0.5f } );
                break;
            }
            case ThemeE::BRIDGE:
            {
                // A console against the wall: a desk, a slanted screen glowing, a seat before it.
                Vector3 l = Vector3Subtract( at, Vector3Scale( n, 0.1f ) );
                float   yaw = atan2f( n.x, n.z );
                metal.m_color = { 170, 178, 190, 255 };
                metal.roundedBox( place( { l.x, 0.45f, l.z }, { 0.0f, yaw, 0.0f } ), { 0.8f, 0.45f, 0.35f }, 0.04f, 2 );
                metal.roundedBox(
                    place( Vector3Add( { l.x, 1.15f, l.z }, Vector3Scale( n, -0.2f ) ), { -0.5f, yaw, 0.0f } ),
                    { 0.75f, 0.3f, 0.04f }, 0.02f, 2 );
                metal.m_color = WHITE;
                Vector3 screen = Vector3Add( { l.x, 1.15f, l.z }, Vector3Scale( n, -0.16f ) );
                bool    alarm = hashCell( r, c, 96 ) % 3 == 0;
                glows.push_back( { screen,
                                   { fabsf( n.x ) > 0.5f ? 0.02f : 0.65f, 0.22f, fabsf( n.z ) > 0.5f ? 0.02f : 0.65f },
                                   alarm ? Vector4{ 2.6f, 0.4f, 0.2f, 0.0f } : Vector4{ 0.35f, 1.4f, 2.2f, 0.0f } } );
                Vector3 seat = Vector3Add( l, Vector3Scale( n, 1.0f ) );
                rust.cylinder( place( { seat.x, 0.0f, seat.z } ), 0.06f, 0.45f, 8 );
                rust.roundedBox( place( { seat.x, 0.5f, seat.z }, { 0.0f, yaw, 0.0f } ), { 0.25f, 0.06f, 0.25f }, 0.03f,
                                 2 );
                m_props.push_back( { l, 0.6f } );
                break;
            }
            case ThemeE::BAR:
            {
                // A round table on one leg, two stools.
                Vector3 t = Vector3Add( at, Vector3Scale( n, 0.25f ) );
                rust.cylinder( place( { t.x, 0.0f, t.z } ), 0.07f, 0.95f, 10 );
                crate.m_color = { 90, 64, 48, 255 };
                crate.lathe( place( { t.x, 0.95f, t.z } ),
                             { { 0.0f, 0.0f }, { 0.48f, 0.0f }, { 0.5f, 0.05f }, { 0.0f, 0.05f } }, 20 );
                crate.m_color = WHITE;
                for ( float side : { -1.0f, 1.0f } )
                {
                    float   a = turn + side * 1.2f;
                    Vector3 stool = { t.x + cosf( a ) * 0.8f, 0.0f, t.z + sinf( a ) * 0.8f };
                    rust.cylinder( place( stool ), 0.04f, 0.6f, 8 );
                    rust.lathe( place( { stool.x, 0.6f, stool.z } ),
                                { { 0.0f, 0.0f }, { 0.2f, 0.0f }, { 0.2f, 0.05f }, { 0.0f, 0.05f } }, 14 );
                }
                m_props.push_back( { t, 0.6f } );
                break;
            }
            case ThemeE::WORKS:
            default:
                if ( hash01( r, c, 35 ) < 0.5f )
                {
                    // Barrels.
                    for ( int i = 0; i < 3; ++i )
                    {
                        float   a = turn + i * 2.1f;
                        Vector3 b = { at.x + cosf( a ) * 0.32f, 0.0f, at.z + sinf( a ) * 0.32f };
                        ( i == 1 ? hazard : rust )
                            .lathe( place( b ),
                                    { { 0.0f, 0.0f },
                                      { 0.28f, 0.0f },
                                      { 0.28f, 0.0f },
                                      { 0.3f, 0.45f },
                                      { 0.28f, 0.9f },
                                      { 0.28f, 0.9f },
                                      { 0.0f, 0.9f } },
                                    14 );
                    }
                    m_props.push_back( { at, 0.65f } );
                }
                else
                {
                    // A heap of scrap.
                    for ( int i = 0; i < 9; ++i )
                    {
                        float a = hash01( r, c, 40 + i ) * 6.28f, d = hash01( r, c, 50 + i ) * 0.45f;
                        rust.box( place( { at.x + cosf( a ) * d, 0.12f + ( i % 3 ) * 0.14f, at.z + sinf( a ) * d },
                                         { hash01( r, c, 60 + i ), a, hash01( r, c, 70 + i ) } ),
                                  { 0.18f, 0.06f, 0.12f } );
                    }
                    m_props.push_back( { at, 0.55f } );
                }
                break;
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
            // The lamp's colour follows the zone: cold light in the exchangers and the cold store.
            ThemeE  t = theme( m.m_row, m.m_col );
            Vector3 colour = red ? Vector3{ 30.0f, 3.4f, 1.4f } : Vector3{ 34.0f, 21.0f, 8.5f };
            if ( !red && ( t == ThemeE::EXCHANGER || t == ThemeE::COLDSTORE ) ) colour = { 18.0f, 26.0f, 36.0f };
            unsigned h = hashCell( m.m_row, m.m_col, 5 );
            m_lights.push_back(
                { Vector3Add( at, Vector3Scale( n, 0.6f ) ), colour, red ? 10.0f : 12.5f, h % 4 == 0 ? 0.7f : 0.0f } );
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

    // The harvest board: a slate on the north wall of its cells, framed, two lamps above it.
    if ( m_boardRow >= 0 )
    {
        float w = m_boardWidth * CellSize - 0.6f, h = fminf( w * 0.5f, 3.2f );
        float x = ( m_boardCol + m_boardWidth * 0.5f ) * CellSize, z = m_boardRow * CellSize + 0.36f, y = 3.0f;
        metal.box( place( { x, y, z - 0.05f } ), { w * 0.5f + 0.1f, h * 0.5f + 0.1f, 0.04f } );
        m_boardCorner[0] = { x - w * 0.5f, y + h * 0.5f, z };
        m_boardCorner[1] = { x + w * 0.5f, y + h * 0.5f, z };
        m_boardCorner[2] = { x + w * 0.5f, y - h * 0.5f, z };
        m_boardCorner[3] = { x - w * 0.5f, y - h * 0.5f, z };
        for ( float side : { -0.3f, 0.3f } )
        {
            Vector3 lamp = { x + side * w, y + h * 0.5f + 0.3f, z + 0.3f };
            metal.box( place( lamp ), { 0.25f, 0.05f, 0.12f } );
            glows.push_back(
                { Vector3Add( lamp, { 0.0f, -0.05f, 0.0f } ), { 0.2f, 0.01f, 0.08f }, { 6.0f, 4.5f, 2.5f, 0.0f } } );
            m_lights.push_back( { Vector3Add( lamp, { 0.0f, -0.3f, 0.5f } ), { 9.0f, 7.0f, 4.5f }, 4.0f, 0.0f } );
        }
    }

    // Signs: on the wall north of their cell (the camera sees those faces), else on two posts facing the camera.
    for ( const auto &req : m_signRequests )
    {
        int     r = req.second.first, c = req.second.second;
        Vector3 mid = centre( r, c );
        float   w = 2.4f, h = w * 9.0f / 21.0f;
        Sign    sign;
        sign.m_texture = req.first;
        float z, y;
        if ( !open( r - 1, c ) )
        {
            z = r * CellSize + 0.34f;   // in front of the ribs
            y = mid.y + 2.75f;
            metal.box( place( { mid.x, y, z - 0.04f } ), { w * 0.5f + 0.06f, h * 0.5f + 0.06f, 0.03f } );
            for ( float side : { -1.0f, 1.0f } )
                metal.box( place( { mid.x + side * ( w * 0.5f - 0.2f ), y, z - 0.2f } ), { 0.04f, 0.04f, 0.16f } );
        }
        else
        {
            z = mid.z;
            y = mid.y + 2.0f;
            for ( float side : { -1.0f, 1.0f } )
                metal.box( place( { mid.x + side * ( w * 0.5f - 0.15f ), y * 0.5f, z - 0.06f } ),
                           { 0.05f, y * 0.5f, 0.05f } );
            metal.box( place( { mid.x, y, z - 0.05f } ), { w * 0.5f + 0.06f, h * 0.5f + 0.06f, 0.03f } );
        }
        sign.m_corner[0] = { mid.x - w * 0.5f, y + h * 0.5f, z };
        sign.m_corner[1] = { mid.x + w * 0.5f, y + h * 0.5f, z };
        sign.m_corner[2] = { mid.x + w * 0.5f, y - h * 0.5f, z };
        sign.m_corner[3] = { mid.x - w * 0.5f, y - h * 0.5f, z };
        sign.m_lamp = { mid.x, y + h * 0.5f + 0.25f, z + 0.25f };
        metal.box( place( { mid.x, y + h * 0.5f + 0.18f, z + 0.12f } ), { 0.3f, 0.04f, 0.12f } );
        glows.push_back(
            { { mid.x, y + h * 0.5f + 0.14f, z + 0.14f }, { 0.25f, 0.01f, 0.08f }, { 6.0f, 4.5f, 2.5f, 0.0f } } );
        m_lights.push_back( { sign.m_lamp, { 7.0f, 5.5f, 3.5f }, 3.2f, 0.0f } );
        m_signs.push_back( sign );
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
    add( crate, s.m_crate, false );
    add( box, s.m_box, true );
    add( hull, s.m_hull, true );
    add( cloth, s.m_cloth, false );
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
