/**
 * @file LevelFloors.cpp
 * @brief The floors follow the rooms (the graphics round's phase F1): each open cell's kind of room (hall, corridor,
 *        side room), the band along a hall's walls, the ways along the route, the ground's shapes out in the open
 *        (trodden along the ways, scree at the rock's foot), and a motif in the middle of each hall.
 * @author Rene Weller
 */
#include "Level.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <set>

namespace spaceopera
{

namespace
{

const int DR[4] = { -1, 0, 1, 0 };   ///< north, east, south, west (as Level.cpp)
const int DC[4] = { 0, 1, 0, -1 };
const int CR[4] = { -1, -1, 1, 1 };   ///< a cell's corners: north-west, north-east, south-east, south-west
const int CC[4] = { -1, 1, 1, -1 };

constexpr int HallCells = 100;   ///< a room this large (open cells) is a hall
constexpr int Pocket = 3;        ///< open cells off a single room, no more than this many, are that room's

/** @brief The kind of motif a place has in its halls ("" none): motif_<kind>_<n> in the decals. */
const char *motifKind( ThemeE t, int group )
{
    switch ( t )
    {
    case ThemeE::DECK:
    case ThemeE::STATION:
    case ThemeE::YARD:
    case ThemeE::CONTAINER:
    case ThemeE::FURNACE:
    case ThemeE::PRISON:
    case ThemeE::HULL: return "plated";
    default: break;
    }
    switch ( group )
    {
    case 0: return "works";
    case 1: return "street";
    case 2: return "clean";
    case 3: return "cold";
    default: return "";
    }
}

/** @return the distance from p to the segment a..b (on the ground plan) */
float segmentDistance( Vector2 p, Vector2 a, Vector2 b )
{
    Vector2 ab = Vector2Subtract( b, a );
    float   len = Vector2DotProduct( ab, ab );
    float   t = len > 1e-6f ? Clamp( Vector2DotProduct( Vector2Subtract( p, a ), ab ) / len, 0.0f, 1.0f ) : 0.0f;
    return Vector2Distance( p, Vector2Add( a, Vector2Scale( ab, t ) ) );
}

}   // namespace

int Level::patchPick( int r, int c, bool wall, int n )
{
    unsigned h = wall ? hashCell( r / 2, ( c + ( r / 2 ) % 2 ) / 2, 141 )
                      : hashCell( ( r + (int)( hashCell( r / 3, c, 143 ) % 2 ) ) / 3,
                                  ( c + (int)( hashCell( r, c / 3, 144 ) % 2 ) ) / 3, 142 );
    return (int)( h % (unsigned)( n + 2 ) );
}

void Level::planRooms()
{
    size_t cells = (size_t)m_rows * m_cols;
    m_room.assign( cells, ROOM_NONE );
    m_roomOf.assign( cells, -1 );
    auto at = [&]( int r, int c ) { return (size_t)r * m_cols + c; };
    // Walls, doors and shafts bound the rooms (a shaft's open space beside a room is none of it).
    auto open = [&]( int r, int c )
    {
        CellE k = cell( r, c ).m_kind;
        return k != CellE::ROCK && k != CellE::DOOR && k != CellE::VOID;
    };
    // The open cells summed over the rectangle above and left of each corner: a block's count in four look-ups.
    std::vector<int> sum( (size_t)( m_rows + 1 ) * ( m_cols + 1 ), 0 );
    auto             si = [&]( int r, int c ) { return (size_t)r * ( m_cols + 1 ) + c; };
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
            sum[si( r + 1, c + 1 )] = sum[si( r, c + 1 )] + sum[si( r + 1, c )] - sum[si( r, c )] + open( r, c );
    auto block = [&]( int r, int c )
    {
        if ( r < 2 || c < 2 || r + 3 > m_rows || c + 3 > m_cols ) return false;
        return sum[si( r + 3, c + 3 )] - sum[si( r - 2, c + 3 )] - sum[si( r + 3, c - 2 )] + sum[si( r - 2, c - 2 )] ==
               25;
    };
    // The blocks' middles that touch are one room; each room is what its blocks cover.
    std::vector<int> core( cells, -1 );
    int              rooms = 0;
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            if ( core[at( r, c )] >= 0 || !block( r, c ) ) continue;
            std::deque<std::pair<int, int>> todo = { { r, c } };
            core[at( r, c )] = rooms;
            while ( !todo.empty() )
            {
                auto [qr, qc] = todo.front();
                todo.pop_front();
                for ( int d = 0; d < 4; ++d )
                {
                    int nr = qr + DR[d], nc = qc + DC[d];
                    if ( nr < 0 || nc < 0 || nr >= m_rows || nc >= m_cols || core[at( nr, nc )] >= 0 ||
                         !block( nr, nc ) )
                        continue;
                    core[at( nr, nc )] = rooms;
                    todo.push_back( { nr, nc } );
                }
            }
            ++rooms;
        }
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
            if ( core[at( r, c )] >= 0 )
                for ( int dr = -2; dr <= 2; ++dr )
                    for ( int dc = -2; dc <= 2; ++dc )
                        if ( m_roomOf[at( r + dr, c + dc )] < 0 ) m_roomOf[at( r + dr, c + dc )] = core[at( r, c )];
    // The open cells no block covers: corridors (each run of them a room of its own), or a pocket of a room.
    std::vector<unsigned char> kinds( (size_t)rooms, ROOM_SIDE );
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            if ( !open( r, c ) || m_roomOf[at( r, c )] >= 0 ) continue;
            std::vector<std::pair<int, int>> run = { { r, c } };
            std::set<int>                    touches;
            int                              id = (int)kinds.size();
            m_roomOf[at( r, c )] = id;
            for ( size_t i = 0; i < run.size(); ++i )
                for ( int d = 0; d < 4; ++d )
                {
                    int nr = run[i].first + DR[d], nc = run[i].second + DC[d];
                    if ( nr < 0 || nc < 0 || nr >= m_rows || nc >= m_cols || !open( nr, nc ) ) continue;
                    int o = m_roomOf[at( nr, nc )];
                    if ( o >= 0 && o < rooms )
                        touches.insert( o );
                    else if ( o < 0 )
                    {
                        m_roomOf[at( nr, nc )] = id;
                        run.push_back( { nr, nc } );
                    }
                }
            if ( (int)run.size() <= Pocket && touches.size() == 1 )
                for ( auto [pr, pc] : run ) m_roomOf[at( pr, pc )] = *touches.begin();
            else
                kinds.push_back( ROOM_CORRIDOR );
        }
    std::vector<int> area( (size_t)rooms, 0 );
    for ( int id : m_roomOf )
        if ( id >= 0 && id < rooms ) ++area[(size_t)id];
    for ( int i = 0; i < rooms; ++i ) kinds[(size_t)i] = area[(size_t)i] >= HallCells ? ROOM_HALL : ROOM_SIDE;
    int counts[4] = {};
    for ( size_t i = 0; i < cells; ++i )
        if ( m_roomOf[i] >= 0 )
        {
            m_room[i] = kinds[(size_t)m_roomOf[i]];
            ++counts[m_room[i]];
        }
    TraceLog( LOG_WARNING, "FLOORS: %d rooms (%d hall cells, %d side room cells), %d corridor cells", rooms,
              counts[ROOM_HALL], counts[ROOM_SIDE], counts[ROOM_CORRIDOR] );
}

int Level::roomKind( int r, int c ) const
{
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols || m_room.empty() ) return ROOM_NONE;
    return m_room[(size_t)r * m_cols + c];
}

int Level::roomOf( int r, int c ) const
{
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols || m_roomOf.empty() ) return -1;
    return m_roomOf[(size_t)r * m_cols + c];
}

unsigned Level::bandSides( int r, int c ) const
{
    if ( roomKind( r, c ) != ROOM_HALL || cell( r, c ).m_kind != CellE::FLOOR ) return 0;
    auto     wall = [&]( int wr, int wc ) { return cell( wr, wc ).m_kind == CellE::ROCK && !garden( wr, wc ); };
    unsigned bits = 0;
    for ( int d = 0; d < 4; ++d )
        if ( wall( r + DR[d], c + DC[d] ) ) bits |= 1u << d;
    for ( int q = 0; q < 4; ++q )
        if ( !wall( r + CR[q], c ) && !wall( r, c + CC[q] ) && wall( r + CR[q], c + CC[q] ) &&
             cell( r + CR[q], c ).m_kind != CellE::ROCK && cell( r, c + CC[q] ).m_kind != CellE::ROCK )
            bits |= 1u << ( 4 + q );
    return bits;
}

bool Level::bandSquare( unsigned band, int i, int j, int steps )
{
    int w = std::max( 1, steps / 4 ), e = steps - w;
    return ( ( band & 1u ) && j < w ) || ( ( band & 2u ) && i >= e ) || ( ( band & 4u ) && j >= e ) ||
           ( ( band & 8u ) && i < w ) || ( ( band & 16u ) && i < w && j < w ) ||
           ( ( band & 32u ) && i >= e && j < w ) || ( ( band & 64u ) && i >= e && j >= e ) ||
           ( ( band & 128u ) && i < w && j >= e );
}

void Level::bandJoints( int r, int c, unsigned band, const LevelSurfaces &s, MeshBuilder &b ) const
{
    const Cell &k = cell( r, c );
    float       x0 = c * CellSize, z0 = r * CellSize, q = CellSize / 4.0f;
    b.m_color = { 38, 40, 44, 255 };
    for ( int j = 0; j < 4; ++j )
        for ( int i = 0; i < 4; ++i )
        {
            if ( !bandSquare( band, i, j, 4 ) ) continue;
            for ( int e = 0; e < 4; ++e )
            {
                int ni = i + DC[e], nj = j + DR[e];
                if ( ni >= 0 && nj >= 0 && ni < 4 && nj < 4 )
                {
                    if ( bandSquare( band, ni, nj, 4 ) ) continue;
                }
                else
                {
                    // In the next cell: only where it is floor at this height, and its square has no band.
                    int         nr = r + DR[e], nc = c + DC[e];
                    const Cell &n = cell( nr, nc );
                    if ( n.m_kind != CellE::FLOOR || n.m_height != k.m_height ) continue;
                    unsigned other = s.m_border[(int)theme( nr, nc )] ? bandSides( nr, nc ) : 0;
                    if ( bandSquare( other, ( ni + 4 ) % 4, ( nj + 4 ) % 4, 4 ) ) continue;
                }
                // Along the edge between the two squares.
                Vector3 at = { x0 + ( i + 0.5f + DC[e] * 0.5f ) * q, k.m_height + 0.003f,
                               z0 + ( j + 0.5f + DR[e] * 0.5f ) * q };
                b.box( place( at ), { DC[e] ? 0.012f : q * 0.5f, 0.003f, DR[e] ? 0.012f : q * 0.5f } );
            }
        }
    b.m_color = WHITE;
}

void Level::planWays()
{
    // Each step of the route (the autopilot's waypoints) by the cheapest way, a step costing more near the walls: the
    // ways keep to the middle of the open floor.
    size_t cells = (size_t)m_rows * m_cols;
    m_way.assign( cells, 0 );
    if ( m_route.size() < 2 ) return;
    std::vector<int> wall( cells, 0 );
    std::deque<int>  todo;
    for ( size_t i = 0; i < cells; ++i )
        if ( !walkable( (int)( i / m_cols ), (int)( i % m_cols ) ) )
            todo.push_back( (int)i );
        else
            wall[i] = -1;
    while ( !todo.empty() )
    {
        int i = todo.front();
        todo.pop_front();
        for ( int d = 0; d < 4; ++d )
        {
            int nr = i / m_cols + DR[d], nc = i % m_cols + DC[d];
            if ( nr < 0 || nc < 0 || nr >= m_rows || nc >= m_cols ) continue;
            size_t n = (size_t)nr * m_cols + nc;
            if ( wall[n] >= 0 ) continue;
            wall[n] = wall[i] + 1;
            todo.push_back( (int)n );
        }
    }
    std::vector<float> cost( cells );
    std::vector<int>   from( cells );
    for ( size_t w = 0; w + 1 < m_route.size(); ++w )
    {
        int a = m_route[w].first * m_cols + m_route[w].second,
            b = m_route[w + 1].first * m_cols + m_route[w + 1].second;
        if ( a < 0 || b < 0 || a >= (int)cells || b >= (int)cells ) continue;
        std::fill( cost.begin(), cost.end(), 1e9f );
        std::fill( from.begin(), from.end(), -1 );
        // Dijkstra on a small queue.
        std::vector<std::pair<float, int>> heap = { { 0.0f, a } };
        cost[a] = 0.0f;
        while ( !heap.empty() )
        {
            std::pop_heap( heap.begin(), heap.end(), std::greater<>() );
            auto [k, i] = heap.back();
            heap.pop_back();
            if ( k > cost[i] ) continue;
            if ( i == b ) break;
            for ( int d = 0; d < 4; ++d )
            {
                int nr = i / m_cols + DR[d], nc = i % m_cols + DC[d];
                if ( nr < 0 || nc < 0 || nr >= m_rows || nc >= m_cols || !walkable( nr, nc ) ) continue;
                int   n = nr * m_cols + nc;
                float step = cost[i] + 1.0f + 3.0f / (float)std::max( 1, wall[n] );
                if ( step >= cost[n] ) continue;
                cost[n] = step;
                from[n] = i;
                heap.push_back( { step, n } );
                std::push_heap( heap.begin(), heap.end(), std::greater<>() );
            }
        }
        if ( cost[b] >= 1e9f ) continue;
        for ( int i = b; i >= 0; i = from[i] )
        {
            m_way[(size_t)i] = 1;
            if ( i == a ) break;
        }
    }
}

Vector2 Level::groundShape( Vector3 p ) const
{
    int r, c;
    cellOf( p, r, c );
    Vector2 q = { p.x, p.z };
    float   rock = 9.0f, way = 9.0f;
    auto    onWay = [&]( int wr, int wc )
    { return wr >= 0 && wc >= 0 && wr < m_rows && wc < m_cols && !m_way.empty() && m_way[(size_t)wr * m_cols + wc]; };
    for ( int dr = -2; dr <= 2; ++dr )
        for ( int dc = -2; dc <= 2; ++dc )
        {
            int     nr = r + dr, nc = c + dc;
            Vector2 lo = { nc * CellSize, nr * CellSize }, hi = { lo.x + CellSize, lo.y + CellSize };
            if ( cell( nr, nc ).m_kind == CellE::ROCK )
            {
                // To the rock cell's square (0 inside it: the ground goes on into its cut corners).
                float dx = std::max( { lo.x - q.x, 0.0f, q.x - hi.x } ),
                      dz = std::max( { lo.y - q.y, 0.0f, q.y - hi.y } );
                rock = std::min( rock, sqrtf( dx * dx + dz * dz ) );
            }
            if ( !onWay( nr, nc ) ) continue;
            // To the way's middle line: from cell to cell along it.
            Vector2 mid = { ( nc + 0.5f ) * CellSize, ( nr + 0.5f ) * CellSize };
            way = std::min( way, Vector2Distance( q, mid ) );
            for ( int d = 1; d <= 2; ++d )
                if ( onWay( nr + DR[d], nc + DC[d] ) )
                    way = std::min( way,
                                    segmentDistance( q, mid, { mid.x + DC[d] * CellSize, mid.y + DR[d] * CellSize } ) );
        }
    // A way some two metres wide fading out over a metre more; the scree thickest at the foot, gone 1.6 m out.
    return { 1.0f - Clamp( ( way - 0.7f ) / 1.1f, 0.0f, 1.0f ), 1.0f - Clamp( ( rock - 0.1f ) / 1.5f, 0.0f, 1.0f ) };
}

void Level::motifs( const LevelSurfaces &s )
{
    if ( !s.m_decals || m_roomOf.empty() ) return;
    const std::vector<DecalDef> &defs = *s.m_decals;
    // Each hall's cells and the middle of them.
    std::map<int, std::vector<std::pair<int, int>>> halls;
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
            if ( roomKind( r, c ) == ROOM_HALL ) halls[roomOf( r, c )].push_back( { r, c } );
    int laid = 0;
    for ( auto &[id, list] : halls )
    {
        Vector2 middle = { 0.0f, 0.0f };
        for ( auto [r, c] : list ) middle = Vector2Add( middle, { ( c + 0.5f ) * CellSize, ( r + 0.5f ) * CellSize } );
        middle = Vector2Scale( middle, 1.0f / (float)list.size() );
        // The cells nearest the middle first (ties by the hash: the same level, the same motifs).
        std::vector<std::pair<int, int>> order = list;
        std::sort(
            order.begin(), order.end(),
            [&]( const auto &a, const auto &b )
            {
                float da = Vector2Distance( middle, { ( a.second + 0.5f ) * CellSize, ( a.first + 0.5f ) * CellSize } );
                float db = Vector2Distance( middle, { ( b.second + 0.5f ) * CellSize, ( b.first + 0.5f ) * CellSize } );
                return da != db ? da < db : hashCell( a.first, a.second, 611 ) < hashCell( b.first, b.second, 611 );
            } );
        unsigned h = hashCell( list.front().first, list.front().second, 612 );
        float    scale = Clamp( sqrtf( (float)list.size() ) / 12.0f, 0.85f, 1.2f );
        for ( size_t k = 0; k < order.size() && k < 24; ++k )
        {
            auto [r, c] = order[k];
            const Cell &mid = cell( r, c );
            std::string kind = motifKind( theme( r, c ), dressGroup( theme( r, c ) ) );
            if ( kind.empty() || mid.m_kind != CellE::FLOOR ) continue;
            std::vector<int> mine;
            for ( size_t i = 0; i < defs.size(); ++i )
                if ( defs[i].m_name.rfind( "motif_" + kind + "_", 0 ) == 0 ) mine.push_back( (int)i );
            if ( mine.empty() ) break;
            const DecalDef &d = defs[(size_t)mine[h % mine.size()]];
            float           half = 0.5f * d.m_width * scale;
            Vector3         at = { ( c + 0.5f ) * CellSize, mid.m_height, ( r + 0.5f ) * CellSize };
            // Its square on even floor of the hall: no step, shaft, channel, band or other room under it.
            bool fine = true;
            int  r0, c0, r1, c1;
            cellOf( { at.x - half, 0.0f, at.z - half }, r0, c0 );
            cellOf( { at.x + half, 0.0f, at.z + half }, r1, c1 );
            for ( int qr = r0; qr <= r1 && fine; ++qr )
                for ( int qc = c0; qc <= c1 && fine; ++qc )
                {
                    const Cell &q = cell( qr, qc );
                    fine = q.m_kind == CellE::FLOOR && q.m_height == mid.m_height && roomOf( qr, qc ) == id &&
                           ( m_pit.empty() || m_pit[(size_t)qr * m_cols + qc] == PIT_NONE ) && !bandSides( qr, qc );
                }
            // Nothing standing on it, but a show piece right in its middle.
            for ( const Prop &p : m_props )
            {
                float dist = Vector2Distance( { p.m_pos.x, p.m_pos.z }, { at.x, at.z } );
                if ( dist > 0.6f && dist < half + p.m_radius ) fine = false;
            }
            if ( !fine ) continue;
            float   yaw = ( h >> 8 ) % 2 ? PI * 0.5f : 0.0f;
            float   w = d.m_width * scale * 0.5f, l = d.m_height * scale * 0.5f;
            Vector3 right = { cosf( yaw ) * w, 0.0f, sinf( yaw ) * w },
                    down = { -sinf( yaw ) * l, 0.0f, cosf( yaw ) * l };
            // Under the place's other floor decals (0.012 m up and more): it is drawn first.
            Vector3 m = { at.x, mid.m_height + 0.007f, at.z };
            Decal   decal;
            decal.m_def = (int)( &d - defs.data() );
            decal.m_corner[0] = Vector3Subtract( Vector3Subtract( m, right ), down );
            decal.m_corner[1] = Vector3Subtract( Vector3Add( m, right ), down );
            decal.m_corner[2] = Vector3Add( Vector3Add( m, right ), down );
            decal.m_corner[3] = Vector3Add( Vector3Subtract( m, right ), down );
            decal.m_cut = false;
            m_decals.push_back( decal );
            TraceLog( LOG_WARNING, "FLOORS: %s at %.1f,%.1f", d.m_name.c_str(), at.x, at.z );
            ++laid;
            break;
        }
    }
    TraceLog( LOG_WARNING, "FLOORS: %d halls, %d motifs", (int)halls.size(), laid );
}

void Level::bayLines( const LevelSurfaces &s )
{
    if ( !s.m_decals || m_roomOf.empty() ) return;
    const std::vector<DecalDef> &defs = *s.m_decals;
    int                          def = -1;
    for ( size_t i = 0; i < defs.size(); ++i )
        if ( defs[i].m_name == "bay_line" ) def = (int)i;
    if ( def < 0 ) return;
    // The halls whose place takes them, and the motifs to keep clear of.
    std::map<int, std::vector<std::pair<int, int>>> halls;
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
            if ( roomKind( r, c ) == ROOM_HALL && s.m_bays[(int)theme( r, c )] )
                halls[roomOf( r, c )].push_back( { r, c } );
    std::vector<Vector2> motifsAt;
    for ( const Decal &d : m_decals )
        if ( defs[(size_t)d.m_def].m_name.rfind( "motif_", 0 ) == 0 )
            motifsAt.push_back(
                { ( d.m_corner[0].x + d.m_corner[2].x ) * 0.5f, ( d.m_corner[0].z + d.m_corner[2].z ) * 0.5f } );
    int laid = 0, lined = 0;
    for ( auto &[id, list] : halls )
    {
        int r0 = m_rows, c0 = m_cols, r1 = -1, c1 = -1;
        for ( auto [r, c] : list )
            r0 = std::min( r0, r ), c0 = std::min( c0, c ), r1 = std::max( r1, r ), c1 = std::max( c1, c );
        // Only a hall near enough a rectangle has a layout to follow.
        if ( (float)list.size() < 0.7f * (float)( ( r1 - r0 + 1 ) * ( c1 - c0 + 1 ) ) ) continue;
        const float base = cell( ( r0 + r1 ) / 2, ( c0 + c1 ) / 2 ).m_height;
        // The hall's own frame: u along its long axis, v across.
        const bool  alongX = c1 - c0 >= r1 - r0;
        const float u0 = ( alongX ? c0 : r0 ) * CellSize, u1 = ( alongX ? c1 + 1 : r1 + 1 ) * CellSize;
        const float v0 = ( alongX ? r0 : c0 ) * CellSize, v1 = ( alongX ? r1 + 1 : c1 + 1 ) * CellSize;
        auto        world = [&]( float u, float v ) { return alongX ? Vector2{ u, v } : Vector2{ v, u }; };
        // A piece from a to b, where all under it is the hall's own even floor, nothing stands on it and no motif lies.
        auto piece = [&]( Vector2 a, Vector2 b )
        {
            float len = Vector2Distance( a, b );
            if ( len < 0.5f ) return;
            for ( float t = 0.0f; t <= len + 1e-3f; t += 0.4f )
            {
                Vector2 q = Vector2Lerp( a, b, t / len );
                int     r, c;
                cellOf( { q.x, 0.0f, q.y }, r, c );
                if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols ) return;
                const Cell &k = cell( r, c );
                if ( k.m_kind != CellE::FLOOR || k.m_height != base || roomOf( r, c ) != id ||
                     ( !m_pit.empty() && m_pit[(size_t)r * m_cols + c] != PIT_NONE ) )
                    return;
            }
            for ( const Vector2 &m : motifsAt )
                if ( segmentDistance( m, a, b ) < 2.6f ) return;
            for ( const Prop &p : m_props )
                if ( segmentDistance( { p.m_pos.x, p.m_pos.z }, a, b ) < p.m_radius + 0.15f ) return;
            Vector2 d = Vector2Scale( Vector2Subtract( b, a ), 1.0f / len );
            Vector2 n = { -d.y * 0.06f, d.x * 0.06f };
            float   y = base + 0.007f;   // as the motifs: under the place's other floor decals
            Decal   decal;
            decal.m_def = def;
            decal.m_corner[0] = { a.x - n.x, y, a.y - n.y };
            decal.m_corner[1] = { b.x - n.x, y, b.y - n.y };
            decal.m_corner[2] = { b.x + n.x, y, b.y + n.y };
            decal.m_corner[3] = { a.x + n.x, y, a.y + n.y };
            decal.m_cut = false;
            m_decals.push_back( decal );
            ++laid;
        };
        // The walkway: two lines along the middle, 2.4 m apart, in pieces of two metres (the picture's length).
        const float mid = ( v0 + v1 ) * 0.5f;
        for ( float side : { -1.2f, 1.2f } )
            for ( float u = u0 + 1.0f; u + 2.0f <= u1 - 1.0f + 1e-3f; u += 2.0f )
                piece( world( u, mid + side ), world( u + 2.0f, mid + side ) );
        // A hall wide enough has a row of bays by each long wall: a line 3 m out from the wall, open towards the
        // walkway, divided every four metres (in from the band along the wall).
        if ( v1 - v0 >= 14.0f )
            for ( float dir : { 1.0f, -1.0f } )
            {
                float wall = dir > 0.0f ? v0 + 0.7f : v1 - 0.7f, front = wall + dir * 2.3f;
                for ( float u = u0 + 1.0f; u + 2.0f <= u1 - 1.0f + 1e-3f; u += 2.0f )
                    piece( world( u, front ), world( u + 2.0f, front ) );
                for ( float u = u0 + 1.0f; u <= u1 - 1.0f + 1e-3f; u += 4.0f )
                    piece( world( u, wall ), world( u, front ) );
            }
        ++lined;
    }
    TraceLog( LOG_WARNING, "FLOORS: %d halls with bay lines, %d pieces", lined, laid );
}

}   // namespace spaceopera
