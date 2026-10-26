/**
 * @file Level.cpp
 * @brief Reading a level file, the grid's queries, and the geometry with its dressing (per zone's theme), props and
 *        signs.
 * @author Rene Weller
 */
#include "Level.h"

#include "LevelGen.h"
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
    { 3,
      { 200, 190, 175, 255 },
      0.0f,
      0.0f,
      1,
      { 190, 170, 150, 255 },
      { 205, 196, 186, 255 },
      { 255, 255, 255, 255 },
      0.4f },
    { 2,
      { 168, 172, 180, 255 },
      0.4f,
      0.4f,
      1,
      { 185, 175, 155, 255 },
      { 176, 180, 188, 255 },
      { 255, 255, 255, 255 },
      0.0f },
    { 2,
      { 205, 210, 222, 255 },
      0.95f,
      0.0f,
      1,
      { 200, 205, 215, 255 },
      { 120, 124, 134, 255 },
      { 255, 255, 255, 255 },
      0.0f },
};

/** @brief Paint of the house fronts in a habitat, faded. */
const Color FrontPaint[5] = {
    { 170, 140, 105, 255 }, { 105, 135, 135, 255 }, { 150, 95, 80, 255 }, { 135, 135, 140, 255 }, { 165, 150, 120, 255 }
};

/** @brief Washing on the lines across a street. */
const Color Washing[5] = {
    { 190, 185, 170, 255 }, { 70, 90, 130, 255 }, { 160, 60, 50, 255 }, { 200, 170, 90, 255 }, { 90, 120, 80, 255 }
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
    if ( name == "street" ) return ThemeE::STREET;
    if ( name == "hull" ) return ThemeE::HULL;
    if ( name == "vault" ) return ThemeE::VAULT;
    return ThemeE::SERVICE;
}

}   // namespace

bool Level::load( const std::string &path, unsigned seed )
{
    char *text = LoadFileText( path.c_str() );
    if ( !text ) return false;
    std::string all( text );
    UnloadFileText( text );
    // A recipe becomes a level file first: generated from the seed.
    if ( isRecipe( all ) ) all = m_generated = generateLevel( all, seed );
    return loadText( all );
}

bool Level::loadText( const std::string &text )
{
    std::stringstream in( text );

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
        else if ( key == "intro" )
            kv >> m_intro;
        else if ( key == "exit" )
        {
            std::string to, flag, again;
            kv >> to >> flag >> again;
            m_exits.push_back( { to, flag } );
            if ( again == "again" ) m_repeat.insert( to );
        }
        else if ( key == "board" )
            kv >> m_boardRow >> m_boardCol >> m_boardWidth;
        else if ( key == "ilka" )
            kv >> m_ilkaRow >> m_ilkaCol;
        else if ( key == "area" )
            kv >> m_area;
        else if ( key == "coriolis" )
            kv >> m_coriolis;
        else if ( key == "space" || key == "vacuum" )
        {
            int on = 0;
            kv >> on;
            ( key == "space" ? m_space : m_vacuum ) = on != 0;
        }
        else if ( key == "flares" )
            kv >> m_flares;
        else if ( key == "star" )
            kv >> m_starAt.x >> m_starAt.y >> m_starAt.z >> m_starAt.w;
        else if ( key == "sundir" )
            kv >> m_sunDir.x >> m_sunDir.y >> m_sunDir.z;
        else if ( key == "suncolor" )
            kv >> m_sunColor.x >> m_sunColor.y >> m_sunColor.z;
        else if ( key == "tug" )
            kv >> m_tugRow >> m_tugCol >> m_tugSide;
        else if ( key == "relay" || key == "mother" )
        {
            int r = 0, c = 0;
            kv >> r >> c;
            ( key == "relay" ? m_relays : m_mothers ).push_back( { r, c } );
        }
        else if ( key == "water" )
        {
            Vector4 w = { 0, 0, 0, 0 };
            kv >> w.x >> w.y >> w.z;
            m_water.push_back( w );
        }
        else if ( key == "outdoor" )
        {
            std::string name;
            while ( kv >> name ) m_outdoorThemes.insert( (int)themeOf( name ) );
        }
        else if ( key == "birds" )
            kv >> m_birds;
        else if ( key == "ilkaway" )
        {
            int r, c;
            while ( kv >> r >> c ) m_ilkaWay.push_back( { r, c } );
        }
        else if ( key == "elite" )
        {
            int         r = 0, c = 0;
            std::string boss;
            kv >> r >> c >> boss;
            m_elites[r * 1000 + c] = boss == "boss" ? 2 : 1;
        }
        else if ( key == "lock" || key == "note" )
        {
            int         r = 0, c = 0;
            std::string what;
            kv >> r >> c >> what;
            ( key == "lock" ? m_locks : m_notes )[r * 1000 + c] = what;
        }
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
            else if ( ch == '/' )
                cell.m_kind = CellE::DOOR;
            else if ( ch == '&' || ch == '!' )
                m_markers.push_back( { ch, r, c, {} } );   // a locker to search, a terminal to read
            else if ( ch == '$' || ch == '%' || ch == '^' || ch == '*' || ch == '?' || ch == '@' || ch == ':' ||
                      ch == ';' || ch == '<' || ch == '>' || ch == '|' || ch == ',' )
                m_markers.push_back( { ch, r, c, {} } );   // the later enemies (see Game::load)
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

void Level::openDoor( int r, int c )
{
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols ) return;
    m_cells[(size_t)r * m_cols + c].m_open = true;
}

bool Level::outdoor( int r, int c ) const
{
    return m_outdoorThemes.count( (int)theme( r, c ) ) > 0;
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
    case CellE::DOOR: return k.m_open ? 0.0f : 1000.0f;
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
    const Cell &k = cell( r, c );
    return k.m_kind == CellE::FLOOR || k.m_kind == CellE::GRATE || k.m_kind == CellE::FIN || k.m_kind == CellE::GLASS ||
           ( k.m_kind == CellE::DOOR && k.m_open );
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
                bool        shut = k.m_kind == CellE::DOOR && !k.m_open;
                float       top = k.m_kind == CellE::ROCK || shut ? 1e9f
                                  : k.m_kind == CellE::VOID       ? -1e9f
                                  : k.m_kind == CellE::FIN        ? FinHeight
                                  : k.m_kind == CellE::COUNTER    ? CounterHeight
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
        bool        shut = k.m_kind == CellE::DOOR && !k.m_open;
        float       top = k.m_kind == CellE::ROCK || shut                        ? 1e9f
                          : k.m_kind == CellE::VOID                              ? -PitDepth
                          : k.m_kind == CellE::FIN                               ? FinHeight
                          : k.m_kind == CellE::GRATE || k.m_kind == CellE::GLASS ? 0.0f
                          : k.m_kind == CellE::COUNTER                           ? CounterHeight
                                                                                 : k.m_height;
        if ( p.y < top )
        {
            hit = prev;
            if ( prev.y >= top && k.m_kind != CellE::ROCK && !shut )
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
    MeshBuilder rock, floor, grate, metal, hazard, rust, foil, fins, crate, box, hull, cloth, drape;
    struct Glow
    {
        Vector3 m_at;
        Vector3 m_half;
        Vector4 m_glow;
        bool    m_cut = false;   ///< in a wall: dissolves with it
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
    // A thin rod from a to e (cables, poles, washing lines).
    auto rod = [&]( MeshBuilder &b, Vector3 a, Vector3 e, float half )
    {
        Vector3 z = Vector3Subtract( e, a );
        float   len = Vector3Length( z );
        if ( len < 1e-4f ) return;
        z = Vector3Scale( z, 1.0f / len );
        Vector3 x = fabsf( z.y ) > 0.99f ? Vector3{ 1.0f, 0.0f, 0.0f }
                                         : Vector3Normalize( Vector3CrossProduct( { 0.0f, 1.0f, 0.0f }, z ) );
        b.box( frame( x, Vector3CrossProduct( z, x ), z, Vector3Lerp( a, e, 0.5f ) ), { half, half, len * 0.5f } );
    };
    // A house front on a wall face (`face` its middle on the floor, `n` out of the wall, `yaw` turns a box's x along
    // it): a window above, most of them lit; at street level a door with a lamp, a shutter or bare wall; a striped
    // awning over some; an air box; a cable under the eaves.
    auto front = [&]( int r, int c, int d, Vector3 face, Vector3 n, float yaw, float base )
    {
        unsigned h = hashCell( r, c, 120 + d );
        bool     alongX = fabsf( n.z ) > 0.5f;
        Vector3  along = { alongX ? 1.0f : 0.0f, 0.0f, alongX ? 0.0f : 1.0f };
        auto     flat = [&]( float u, float v ) { return Vector3{ alongX ? u : 0.012f, v, alongX ? 0.012f : u }; };
        Vector3  out = Vector3Add( face, Vector3Scale( n, 0.04f ) );
        if ( h % 5 != 0 )
        {
            const Vector4 lit[4] = { { 1.5f, 0.95f, 0.4f, 0.0f },
                                     { 1.2f, 0.9f, 0.55f, 0.0f },
                                     { 0.4f, 0.7f, 1.1f, 0.0f },
                                     { 0.03f, 0.03f, 0.035f, 0.0f } };
            float         y = base + 3.0f;
            metal.m_color = { 70, 66, 62, 255 };
            metal.box( place( { out.x, y, out.z }, { 0.0f, yaw, 0.0f } ), { 0.6f, 0.48f, 0.05f } );
            metal.m_color = WHITE;
            Vector3 pane = Vector3Add( out, Vector3Scale( n, 0.04f ) );
            glows.push_back( { { pane.x, y, pane.z }, flat( 0.5f, 0.38f ), lit[( h >> 4 ) % 4], true } );
            Vector3 bar = Vector3Add( pane, Vector3Scale( n, 0.02f ) );
            metal.box( place( { bar.x, y, bar.z }, { 0.0f, yaw, 0.0f } ), { 0.5f, 0.02f, 0.015f } );
            metal.box( place( { bar.x, y, bar.z }, { 0.0f, yaw, 0.0f } ), { 0.02f, 0.38f, 0.015f } );
        }
        int kind = ( h >> 8 ) % 4;
        if ( kind == 0 )
        {
            crate.m_color = { 60, 48, 40, 255 };
            crate.box( place( { out.x, base + 1.05f, out.z }, { 0.0f, yaw, 0.0f } ), { 0.5f, 1.05f, 0.05f } );
            crate.m_color = WHITE;
            Vector3 step = Vector3Add( out, Vector3Scale( n, 0.18f ) );
            metal.box( place( { step.x, base + 0.05f, step.z }, { 0.0f, yaw, 0.0f } ), { 0.6f, 0.05f, 0.2f } );
            Vector3 lamp = Vector3Add( out, Vector3Scale( n, 0.12f ) );
            metal.box( place( { lamp.x, base + 2.35f, lamp.z }, { 0.0f, yaw, 0.0f } ), { 0.12f, 0.06f, 0.1f } );
            glows.push_back(
                { { lamp.x, base + 2.28f, lamp.z }, { 0.08f, 0.012f, 0.08f }, { 7.0f, 4.2f, 1.6f, 0.0f }, true } );
            if ( ( h >> 12 ) % 2 )
                m_lights.push_back( { Vector3Add( { lamp.x, base + 2.1f, lamp.z }, Vector3Scale( n, 0.5f ) ),
                                      { 6.0f, 3.8f, 1.6f },
                                      4.0f,
                                      0.0f } );
        }
        else if ( kind == 1 )
        {
            rust.m_color = { 150, 140, 130, 255 };
            rust.box( place( { out.x, base + 1.1f, out.z }, { 0.0f, yaw, 0.0f } ), { 0.85f, 1.1f, 0.04f } );
            for ( int i = 0; i < 8; ++i )
            {
                Vector3 rib = Vector3Add( out, Vector3Scale( n, 0.04f ) );
                rust.box( place( { rib.x, base + 0.2f + i * 0.26f, rib.z }, { 0.0f, yaw, 0.0f } ),
                          { 0.85f, 0.02f, 0.02f } );
            }
            rust.m_color = WHITE;
        }
        if ( kind <= 1 && ( h >> 14 ) % 3 != 0 )
        {
            const Color stripes[3][2] = { { { 200, 80, 60, 255 }, { 235, 225, 205, 255 } },
                                          { { 70, 125, 155, 255 }, { 230, 225, 210, 255 } },
                                          { { 215, 165, 70, 255 }, { 120, 105, 90, 255 } } };
            int         paint = ( h >> 16 ) % 3;
            for ( int i = 0; i < 6; ++i )
            {
                drape.m_color = stripes[paint][i % 2];
                Vector3 a = Vector3Add( Vector3Add( out, Vector3Scale( along, -0.75f + i * 0.3f ) ),
                                        { 0.0f, base + 2.7f, 0.0f } );
                Vector3 e = Vector3Add( Vector3Add( a, Vector3Scale( n, 0.9f ) ), { 0.0f, -0.45f, 0.0f } );
                Vector3 z = Vector3Normalize( Vector3Subtract( e, a ) );
                drape.box( frame( along, Vector3CrossProduct( z, along ), z, Vector3Lerp( a, e, 0.5f ) ),
                           { 0.15f, 0.015f, Vector3Distance( a, e ) * 0.5f } );
            }
            drape.m_color = WHITE;
        }
        if ( ( h >> 18 ) % 6 == 0 )
            metal.box(
                place( Vector3Add( { out.x, base + 3.95f, out.z }, Vector3Scale( n, 0.25f ) ), { 0.0f, yaw, 0.0f } ),
                { 0.35f, 0.22f, 0.25f } );
        Vector3 cable = Vector3Add( out, Vector3Scale( n, 0.06f ) );
        cable.y = base + 4.15f;
        rod( rust, Vector3Subtract( cable, Vector3Scale( along, CellSize * 0.5f ) ),
             Vector3Add( cable, Vector3Scale( along, CellSize * 0.5f ) ), 0.025f );
    };

    // A plate of a station's skin: seams, a hazard band along an edge over open space; now and then a tie-down ring,
    // a conduit, a radiator panel or an antenna mast (those two stand in the way).
    auto hullPlate = [&]( int r, int c, Vector3 mid )
    {
        unsigned h = hashCell( r, c, 140 );
        metal.m_color = { 70, 72, 78, 255 };
        metal.box( place( { mid.x - CellSize * 0.5f + 0.02f, 0.006f, mid.z } ), { 0.02f, 0.006f, CellSize * 0.5f } );
        metal.box( place( { mid.x, 0.006f, mid.z - CellSize * 0.5f + 0.02f } ), { CellSize * 0.5f, 0.006f, 0.02f } );
        metal.m_color = WHITE;
        for ( int d = 0; d < 4; ++d )
        {
            if ( !isVoid( r + DR[d], c + DC[d] ) ) continue;
            Vector3 e = { mid.x + DC[d] * ( CellSize * 0.5f - 0.15f ), 0.01f,
                          mid.z + DR[d] * ( CellSize * 0.5f - 0.15f ) };
            hazard.box( place( e ), { DC[d] ? 0.12f : CellSize * 0.5f, 0.01f, DR[d] ? 0.12f : CellSize * 0.5f } );
        }
        if ( nearMarker( r, c ) ) return;
        bool edge = nearVoid( r, c );
        if ( h % 23 == 0 )
            rust.cylinder( place( { mid.x + 0.4f, 0.0f, mid.z - 0.3f } ), 0.12f, 0.06f, 10 );
        else if ( h % 31 == 1 && !edge )
        {
            // A radiator panel on its frame, faintly warm along its top.
            float yaw = ( h >> 8 ) % 2 ? 0.0f : PI * 0.5f;
            metal.box( place( { mid.x, 0.9f, mid.z }, { 0.0f, yaw, 0.0f } ), { 0.85f, 0.75f, 0.06f } );
            metal.box( place( { mid.x, 0.075f, mid.z }, { 0.0f, yaw, 0.0f } ), { 0.6f, 0.075f, 0.2f } );
            glows.push_back( { { mid.x, 1.66f, mid.z },
                               { yaw != 0.0f ? 0.02f : 0.8f, 0.02f, yaw != 0.0f ? 0.8f : 0.02f },
                               { 1.6f, 0.4f, 0.1f, 0.0f } } );
            m_props.push_back( { mid, 0.7f } );
        }
        else if ( h % 37 == 2 && !edge )
        {
            // An antenna mast with a dish and a red light on top.
            metal.cylinder( place( { mid.x, 0.0f, mid.z } ), 0.07f, 3.6f, 8 );
            metal.lathe( place( { mid.x, 3.2f, mid.z }, { 0.6f, ( h >> 6 ) % 6 * 1.0f, 0.0f } ),
                         { { 0.0f, 0.0f }, { 0.55f, 0.18f }, { 0.6f, 0.22f } }, 16 );
            glows.push_back( { { mid.x, 3.65f, mid.z }, { 0.05f, 0.05f, 0.05f }, { 8.0f, 0.4f, 0.2f, 0.0f } } );
            m_props.push_back( { mid, 0.3f } );
        }
        else if ( h % 13 == 3 )
        {
            // A conduit across the plate.
            if ( ( h >> 9 ) % 2 )
                rust.cylinder( place( { mid.x - CellSize * 0.5f, 0.1f, mid.z + 0.5f }, { 0.0f, 0.0f, -PI * 0.5f } ),
                               0.08f, CellSize, 8 );
            else
                rust.cylinder( place( { mid.x + 0.5f, 0.1f, mid.z - CellSize * 0.5f }, { PI * 0.5f, 0.0f, 0.0f } ),
                               0.08f, CellSize, 8 );
        }
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
                // Every wall cell gets a lid (seen from above) of what the wall is made of: rock, the containers' roofs,
                // a ship's plating, a house's flat roof with its clutter; the ones by an open cell a full block.
                Vector3 lid = { CellSize * 0.5f, 0.01f, CellSize * 0.5f };
                if ( look.m_walls == 0 )
                {
                    rock.m_color = { 70, 66, 64, 255 };
                    rock.box( place( { mid.x, m_wallHeight + 0.01f, mid.z } ), lid );
                    rock.m_color = look.m_rock;
                }
                else if ( look.m_walls == 1 && border )
                {
                    box.m_color = ContainerPaint[hashCell( r / 3, c / 3, 91 ) % 6];
                    box.box( place( { mid.x, m_wallHeight - 0.02f, mid.z } ), lid );
                    box.m_color = WHITE;
                }
                else if ( look.m_walls == 3 )
                {
                    // Flat roofs (the camera looks down on them): a skylight lit from below, a low vent.
                    hull.m_color = { 125, 116, 104, 255 };
                    hull.box( place( { mid.x, m_wallHeight + 0.01f, mid.z } ), lid );
                    hull.m_color = WHITE;
                    unsigned h = hashCell( r, c, 97 );
                    if ( h % 5 == 0 )
                    {
                        metal.box( place( { mid.x, m_wallHeight + 0.05f, mid.z } ), { 0.42f, 0.05f, 0.42f } );
                        glows.push_back( { { mid.x, m_wallHeight + 0.1f, mid.z },
                                           { 0.34f, 0.01f, 0.34f },
                                           { 0.07f, 0.05f, 0.025f, 0.0f },
                                           true } );
                    }
                    else if ( h % 5 == 1 )
                        metal.box(
                            place( { mid.x, m_wallHeight + 0.15f, mid.z }, { 0.0f, ( h >> 8 ) % 4 * 0.4f, 0.0f } ),
                            { 0.4f, 0.15f, 0.3f } );
                }
                else
                {
                    hull.m_color = { 92, 96, 104, 255 };
                    hull.box( place( { mid.x, m_wallHeight + 0.01f, mid.z } ), lid );
                    hull.m_color = WHITE;
                }
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
                float        bottom = nearVoid( r, c ) && !m_space ? -PitDepth : -0.5f;
                MeshBuilder &wall = look.m_walls >= 2 ? hull : rock;
                wall.m_color = look.m_walls == 3 ? FrontPaint[hashCell( r / 4, c / 4, 98 ) % 5] : look.m_rock;
                wall.box( place( { mid.x, ( bottom + m_wallHeight ) * 0.5f, mid.z } ),
                          { CellSize * 0.5f, ( m_wallHeight - bottom ) * 0.5f, CellSize * 0.5f } );
                wall.m_color = WHITE;
                continue;
            }
            if ( k.m_kind == CellE::DOOR )
            {
                // A door: the deck, two jambs, a lintel of what the walls are made of, a hazard band across the
                // threshold; the leaves are the game's (they move). They run along x when the walls are east and west.
                floor.box( place( { mid.x, -0.25f, mid.z } ), { CellSize * 0.5f, 0.25f, CellSize * 0.5f } );
                bool         alongX = !open( r, c - 1 ) || !open( r, c + 1 );
                Vector3      axis = alongX ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 1.0f };
                MeshBuilder &wall = look.m_walls == 0 ? rock : look.m_walls == 1 ? box : hull;
                wall.m_color = look.m_walls == 3   ? FrontPaint[hashCell( r / 4, c / 4, 98 ) % 5]
                               : look.m_walls == 0 ? look.m_rock
                                                   : WHITE;
                wall.box( place( { mid.x, ( DoorHeight + m_wallHeight ) * 0.5f, mid.z } ),
                          { alongX ? CellSize * 0.5f : 0.3f, ( m_wallHeight - DoorHeight ) * 0.5f,
                            alongX ? 0.3f : CellSize * 0.5f } );
                wall.m_color = WHITE;
                for ( float side : { -1.0f, 1.0f } )
                {
                    Vector3 j = Vector3Add( mid, Vector3Scale( axis, side * ( CellSize * 0.5f - 0.08f ) ) );
                    metal.box( place( { j.x, DoorHeight * 0.5f, j.z } ),
                               { alongX ? 0.08f : 0.34f, DoorHeight * 0.5f, alongX ? 0.34f : 0.08f } );
                }
                hazard.box( place( { mid.x, 0.008f, mid.z } ),
                            { alongX ? CellSize * 0.5f : 0.14f, 0.008f, alongX ? 0.14f : CellSize * 0.5f } );
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
                // A bar stool before it on the guests' side (the south).
                if ( walkable( r + 1, c ) )
                {
                    Vector3 stool = { mid.x, 0.0f, mid.z + 1.3f };
                    rust.cylinder( place( stool ), 0.04f, 0.74f, 8 );
                    rust.lathe( place( { stool.x, 0.74f, stool.z } ),
                                { { 0.0f, 0.0f }, { 0.2f, 0.0f }, { 0.2f, 0.05f }, { 0.0f, 0.05f } }, 14 );
                    rust.lathe( place( { stool.x, 0.3f, stool.z } ),
                                { { 0.14f, 0.0f }, { 0.16f, 0.0f }, { 0.16f, 0.02f }, { 0.14f, 0.02f } }, 14 );
                    m_seats.push_back( { stool, PI, 0.79f } );
                    m_props.push_back( { stool, 0.22f } );
                }
                continue;
            }
            if ( k.m_kind == CellE::VOID && m_space )
            {
                // Open space under the plates: now and then a girder of the station's frame far below.
                unsigned h = hashCell( r, c, 7 );
                if ( h % 7 == 0 )
                    metal.box( place( { mid.x, -6.0f - ( h >> 8 ) % 5 * 4.0f, mid.z },
                                      { 0.0f, ( h >> 4 ) % 2 ? 0.0f : PI * 0.5f, 0.0f } ),
                               { 0.3f, 0.4f, CellSize * 0.5f } );
            }
            else if ( k.m_kind == CellE::VOID )
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
                if ( theme( r, c ) == ThemeE::HULL ) hullPlate( r, c, mid );
            }

            // Under a floor next to a shaft: the shaft's wall (in open space only the plate's edge and a rib).
            if ( k.m_kind != CellE::VOID && k.m_kind != CellE::GRATE && nearVoid( r, c ) && m_space )
            {
                metal.box( place( { mid.x, -0.9f, mid.z } ), { CellSize * 0.5f, 0.4f, CellSize * 0.5f } );
                if ( ( r + c ) % 2 == 0 ) metal.box( place( { mid.x, -2.0f, mid.z } ), { 0.15f, 0.7f, 0.15f } );
            }
            else if ( k.m_kind != CellE::VOID && k.m_kind != CellE::GRATE && nearVoid( r, c ) )
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
                if ( theme( r, c ) == ThemeE::STREET && k.m_kind != CellE::VOID )
                {
                    front( r, c, d, face, n, yaw, base );
                    continue;
                }
                if ( theme( r, c ) == ThemeE::BAR && m_peaceful && DR[d] < 0 && k.m_kind == CellE::FLOOR &&
                     !( r == m_boardRow && c >= m_boardCol && c < m_boardCol + m_boardWidth ) )
                {
                    // Shelves of bottles on the back wall, the light coming through them.
                    const Vector4 glass[3] = { { 0.45f, 0.22f, 0.06f, 0.0f },
                                               { 0.12f, 0.3f, 0.1f, 0.0f },
                                               { 0.35f, 0.35f, 0.38f, 0.0f } };
                    for ( float y : { 1.35f, 2.05f } )
                    {
                        crate.m_color = { 70, 52, 40, 255 };
                        crate.box( place( { face.x, base + y, face.z + 0.36f } ), { 0.95f, 0.025f, 0.12f } );
                        crate.m_color = WHITE;
                        for ( int i = 0; i < 6; ++i )
                        {
                            unsigned hb = hashCell( r, c * 8 + i, (int)( y * 10.0f ) );
                            if ( hb % 5 == 0 ) continue;
                            float bh = 0.12f + ( hb >> 4 ) % 3 * 0.03f;
                            glows.push_back( { { face.x - 0.75f + i * 0.3f, base + y + 0.025f + bh, face.z + 0.36f },
                                               { 0.045f, bh, 0.045f },
                                               glass[( hb >> 8 ) % 3] } );
                        }
                    }
                }
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

            // Across a street, every third column: a string of bulbs from house to house, now and then washing.
            if ( theme( r, c ) == ThemeE::STREET && k.m_kind == CellE::FLOOR && !open( r - 1, c ) && c % 3 == 1 )
            {
                int run = 0;
                while ( run < 9 && open( r + run, c ) ) ++run;
                if ( run >= 2 && run <= 8 )
                {
                    float za = r * CellSize + 0.1f, zb = ( r + run ) * CellSize - 0.1f, y = m_wallHeight - 0.35f;
                    auto  at = [&]( float t )
                    { return Vector3{ mid.x, y - 2.0f * t * ( 1.0f - t ), za + ( zb - za ) * t }; };
                    bool washing = hashCell( r, c, 131 ) % 3 == 0;
                    int  segs = run * 4;
                    for ( int i = 0; i < segs; ++i )
                        rod( metal, at( (float)i / segs ), at( (float)( i + 1 ) / segs ), 0.01f );
                    const Vector4 bulbs[3] = { { 9.0f, 5.5f, 2.0f, 0.0f },
                                               { 9.0f, 3.0f, 1.2f, 0.0f },
                                               { 7.0f, 6.5f, 4.0f, 0.0f } };
                    for ( int i = 1; i < segs; ++i )
                    {
                        Vector3 p = at( (float)i / segs );
                        if ( !washing )
                            glows.push_back( { { p.x, p.y - 0.06f, p.z }, { 0.045f, 0.06f, 0.045f }, bulbs[i % 3] } );
                        else if ( i % 2 )
                        {
                            drape.m_color = Washing[hashCell( r, c * 31 + i, 132 ) % 5];
                            float hang = 0.2f + 0.15f * hash01( r, c * 31 + i, 133 );
                            drape.box( place( { p.x, p.y - hang, p.z } ), { 0.01f, hang, 0.2f } );
                        }
                    }
                    drape.m_color = WHITE;
                    if ( !washing )
                    {
                        Vector3 m = at( 0.5f );
                        m_lights.push_back( { { m.x, m.y - 0.4f, m.z }, { 5.0f, 3.4f, 1.6f }, 6.0f, 0.0f } );
                    }
                }
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
                // A round table on one leg, two stools to sit at it.
                Vector3 t = Vector3Add( at, Vector3Scale( n, 0.25f ) );
                rust.cylinder( place( { t.x, 0.0f, t.z } ), 0.07f, 0.74f, 10 );
                crate.m_color = { 90, 64, 48, 255 };
                crate.lathe( place( { t.x, 0.74f, t.z } ),
                             { { 0.0f, 0.0f }, { 0.48f, 0.0f }, { 0.5f, 0.05f }, { 0.0f, 0.05f } }, 20 );
                crate.m_color = WHITE;
                for ( float side : { -1.0f, 1.0f } )
                {
                    float   a = turn + side * 1.2f;
                    Vector3 stool = { t.x + cosf( a ) * 0.8f, 0.0f, t.z + sinf( a ) * 0.8f };
                    rust.cylinder( place( stool ), 0.04f, 0.45f, 8 );
                    rust.lathe( place( { stool.x, 0.45f, stool.z } ),
                                { { 0.0f, 0.0f }, { 0.2f, 0.0f }, { 0.2f, 0.05f }, { 0.0f, 0.05f } }, 14 );
                    m_seats.push_back( { stool, atan2f( t.x - stool.x, t.z - stool.z ), 0.5f } );
                }
                // A lamp hanging low over it.
                rod( metal, { t.x, 2.37f, t.z }, { t.x, m_wallHeight, t.z }, 0.012f );
                rust.lathe( place( { t.x, 2.15f, t.z } ),
                            { { 0.0f, 0.22f }, { 0.08f, 0.22f }, { 0.28f, 0.0f }, { 0.26f, 0.0f } }, 16 );
                glows.push_back( { { t.x, 2.13f, t.z }, { 0.1f, 0.02f, 0.1f }, { 8.0f, 5.0f, 2.0f, 0.0f } } );
                m_lights.push_back( { { t.x, 1.9f, t.z }, { 9.0f, 5.5f, 2.2f }, 4.5f, 0.0f } );
                m_props.push_back( { t, 0.6f } );
                break;
            }
            case ThemeE::STREET:
            {
                unsigned h = hashCell( r, c, 140 );
                float    yaw = fabsf( n.x ) > 0.5f ? PI * 0.5f : 0.0f;
                switch ( h % 5 )
                {
                case 0:
                    // A tub of greens.
                    rust.m_color = { 150, 90, 60, 255 };
                    rust.lathe( place( { at.x, 0.0f, at.z } ),
                                { { 0.0f, 0.0f }, { 0.3f, 0.0f }, { 0.38f, 0.55f }, { 0.34f, 0.55f }, { 0.0f, 0.5f } },
                                16 );
                    rust.m_color = WHITE;
                    for ( int i = 0; i < 7; ++i )
                    {
                        float a = i * 0.9f + turn;
                        cloth.m_color = { 55, (unsigned char)( 95 + i * 9 ), 45, 255 };
                        cloth.roundedBox(
                            place( { at.x + cosf( a ) * 0.15f, 0.75f + ( i % 3 ) * 0.12f, at.z + sinf( a ) * 0.15f },
                                   { 0.4f, a, 0.5f } ),
                            { 0.08f, 0.3f, 0.03f }, 0.02f, 2 );
                    }
                    cloth.m_color = WHITE;
                    m_props.push_back( { at, 0.42f } );
                    break;
                case 1:
                    // Crates and a couple of bags.
                    crate.roundedBox( place( { at.x, 0.38f, at.z }, { 0.0f, turn, 0.0f } ), { 0.45f, 0.38f, 0.4f },
                                      0.04f, 2 );
                    cloth.m_color = { 50, 52, 56, 255 };
                    for ( int i = 0; i < 2; ++i )
                        cloth.roundedBox( place( { at.x + ( i ? 0.5f : -0.5f ) * ( 1.0f - fabsf( n.x ) ), 0.25f,
                                                   at.z + ( i ? 0.5f : -0.5f ) * ( 1.0f - fabsf( n.z ) ) },
                                                 { 0.1f, turn * 2.0f + i, 0.0f } ),
                                          { 0.25f, 0.25f, 0.22f }, 0.15f, 3 );
                    cloth.m_color = WHITE;
                    m_props.push_back( { at, 0.6f } );
                    break;
                case 2:
                    // A bench along the wall.
                    metal.m_color = { 90, 80, 70, 255 };
                    metal.box( place( { at.x, 0.45f, at.z }, { 0.0f, yaw, 0.0f } ), { 0.8f, 0.04f, 0.25f } );
                    for ( float u : { -0.65f, 0.65f } )
                        metal.box(
                            place( { at.x + u * ( 1.0f - fabsf( n.x ) ), 0.22f, at.z + u * ( 1.0f - fabsf( n.z ) ) },
                                   { 0.0f, yaw, 0.0f } ),
                            { 0.04f, 0.22f, 0.22f } );
                    metal.m_color = WHITE;
                    m_props.push_back( { at, 0.45f } );
                    break;
                case 3:
                {
                    // A vending machine, its front lit.
                    Vector3 l = Vector3Subtract( at, Vector3Scale( n, 0.15f ) );
                    metal.m_color = h & 64 ? Color{ 130, 45, 40, 255 } : Color{ 40, 100, 110, 255 };
                    metal.roundedBox( place( { l.x, 1.0f, l.z }, { 0.0f, yaw, 0.0f } ), { 0.45f, 1.0f, 0.35f }, 0.03f,
                                      2 );
                    metal.m_color = WHITE;
                    Vector3 panel = Vector3Add( l, Vector3Scale( n, 0.36f ) );
                    glows.push_back(
                        { { panel.x, 1.25f, panel.z },
                          { fabsf( n.x ) > 0.5f ? 0.012f : 0.34f, 0.55f, fabsf( n.z ) > 0.5f ? 0.012f : 0.34f },
                          { 1.4f, 1.7f, 2.1f, 0.0f } } );
                    m_lights.push_back( { Vector3Add( { panel.x, 1.2f, panel.z }, Vector3Scale( n, 0.6f ) ),
                                          { 2.0f, 2.6f, 3.4f },
                                          3.0f,
                                          0.0f } );
                    m_props.push_back( { l, 0.5f } );
                    break;
                }
                default:
                    // Water drums.
                    for ( int i = 0; i < 2; ++i )
                    {
                        float   a = turn + i * 2.6f;
                        Vector3 b = { at.x + cosf( a ) * 0.3f, 0.0f, at.z + sinf( a ) * 0.3f };
                        rust.m_color = i ? Color{ 70, 100, 120, 255 } : Color{ 160, 150, 140, 255 };
                        rust.lathe( place( b ), { { 0.0f, 0.0f }, { 0.3f, 0.0f }, { 0.3f, 0.95f }, { 0.0f, 0.95f } },
                                    14 );
                    }
                    rust.m_color = WHITE;
                    m_props.push_back( { at, 0.6f } );
                    break;
                }
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
        else if ( m.m_kind == 'M' )
        {
            // A market stall against the wall: goods on a table under a striped awning on two poles, a bulb under it.
            int wall = -1;
            for ( int d = 0; d < 4 && wall < 0; ++d )
                if ( !open( m.m_row + DR[d], m.m_col + DC[d] ) ) wall = d;
            Vector3 n = wall >= 0 ? Vector3{ (float)-DC[wall], 0.0f, (float)-DR[wall] } : Vector3{ 0.0f, 0.0f, 1.0f };
            Vector3 along = { fabsf( n.z ) > 0.5f ? 1.0f : 0.0f, 0.0f, fabsf( n.z ) > 0.5f ? 0.0f : 1.0f };
            float   yaw = fabsf( n.x ) > 0.5f ? PI * 0.5f : 0.0f;
            Vector3 t = Vector3Subtract( p, Vector3Scale( n, 0.25f ) );
            crate.m_color = { 95, 72, 55, 255 };
            crate.box( place( { t.x, 0.42f, t.z }, { 0.0f, yaw, 0.0f } ), { 0.9f, 0.42f, 0.38f } );
            crate.m_color = WHITE;
            const Color goods[5] = { { 170, 60, 40, 255 },
                                     { 90, 140, 60, 255 },
                                     { 200, 160, 60, 255 },
                                     { 120, 90, 150, 255 },
                                     { 210, 120, 50, 255 } };
            for ( int i = 0; i < 6; ++i )
            {
                unsigned hg = hashCell( m.m_row, m.m_col * 7 + i, 150 );
                cloth.m_color = goods[hg % 5];
                Vector3 g = Vector3Add( t, Vector3Scale( along, -0.7f + i * 0.28f ) );
                cloth.roundedBox( place( { g.x, 0.92f, g.z }, { 0.0f, yaw + ( hg >> 4 ) % 7 * 0.2f, 0.0f } ),
                                  { 0.12f, 0.08f, 0.16f }, 0.06f, 2 );
            }
            cloth.m_color = WHITE;
            Vector3 lip = Vector3Add( p, Vector3Scale( n, 0.55f ) );
            for ( float u : { -0.95f, 0.95f } )
            {
                Vector3 pole = Vector3Add( lip, Vector3Scale( along, u ) );
                rod( metal, { pole.x, 0.0f, pole.z }, { pole.x, 2.3f, pole.z }, 0.03f );
            }
            Vector3 back = Vector3Subtract( p, Vector3Scale( n, CellSize * 0.5f - 0.05f ) );
            back.y = 2.65f;
            lip.y = 2.3f;
            Vector3 z = Vector3Normalize( Vector3Subtract( lip, back ) );
            Color   stripe =
                hashCell( m.m_row, m.m_col, 151 ) % 2 ? Color{ 200, 80, 60, 255 } : Color{ 70, 125, 155, 255 };
            for ( int i = 0; i < 7; ++i )
            {
                Vector3 off = Vector3Scale( along, -0.9f + i * 0.3f );
                Vector3 a = Vector3Add( back, off ), e = Vector3Add( lip, off );
                drape.m_color = i % 2 ? Color{ 235, 225, 205, 255 } : stripe;
                drape.box( frame( along, Vector3CrossProduct( z, along ), z, Vector3Lerp( a, e, 0.5f ) ),
                           { 0.15f, 0.015f, Vector3Distance( a, e ) * 0.5f } );
            }
            drape.m_color = WHITE;
            glows.push_back( { { p.x, 2.2f, p.z }, { 0.06f, 0.06f, 0.06f }, { 9.0f, 5.5f, 2.0f, 0.0f } } );
            m_lights.push_back( { { p.x, 1.9f, p.z }, { 8.0f, 5.0f, 2.0f }, 4.5f, 0.0f } );
            m_props.push_back( { t, 0.9f } );
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
    add( drape, s.m_cloth, true );
    // Glowing boxes of one colour go into one mesh.
    while ( !glows.empty() )
    {
        Vector4     colour = glows.front().m_glow;
        bool        cut = glows.front().m_cut;
        MeshBuilder b;
        for ( auto it = glows.begin(); it != glows.end(); )
            if ( Vector4Equals( it->m_glow, colour ) && it->m_cut == cut )
            {
                b.box( place( it->m_at ), it->m_half );
                it = glows.erase( it );
            }
            else
                ++it;
        add( b, s.m_glow, cut, colour );
    }
}

}   // namespace spaceopera
