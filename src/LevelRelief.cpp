/**
 * @file    LevelRelief.cpp
 * @brief   The rock out in the open as relief instead of upright walls (a steep, lumpy face that leans back to an uneven
 *          top, stones at its foot), and the occlusion baked into the level's geometry from the grid: darker at the
 *          walls' feet, in inner corners, under and beside the things on the floor.
 * @author  Rene Weller
 */

#include "Level.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <deque>

namespace spaceopera
{

namespace
{

const int DR[4] = { -1, 0, 1, 0 };   ///< north, east, south, west (as Level.cpp)
const int DC[4] = { 0, 1, 0, -1 };
const int CR[4] = { -1, -1, 1, 1 };   ///< a cell's corners: north-west, north-east, south-east, south-west
const int CC[4] = { -1, 1, 1, -1 };

constexpr float Natural = 0.98f;   ///< raw rock's outer corners are cut this far (as Level.cpp)
constexpr float CutSize = 0.75f;   ///< ... the others' (as Level.cpp)
constexpr int   Fine = 6;          ///< the relief's grid: steps along a cell's edge
/// Where the grid's points lie in a cell: closer by its edges, where the rock rises from the floor.
const float     Steps[Fine + 1] = { 0.0f, 0.2f, 0.6f, 1.0f, 1.4f, 1.8f, 2.0f };
constexpr float Unknown = -1e30f;                                ///< a point of the relief not computed yet
constexpr float PropTop = 0.9f;                                  ///< a prop's height for the occlusion
constexpr float Reach = 1.6f;                                    ///< how far the occlusion looks
const float     Look[5] = { 0.15f, 0.35f, 0.6f, 0.95f, 1.4f };   ///< its steps along a direction

/** @brief The natural places whose rock slopes where they lie out in the open. */
bool wild( ThemeE t )
{
    return t == ThemeE::DUSK || t == ThemeE::SNOW || t == ThemeE::ICE || t == ThemeE::MOON;
}

/** @brief smoothstep. */
float smooth( float a, float b, float x )
{
    float t = Clamp( ( x - a ) / ( b - a ), 0.0f, 1.0f );
    return t * t * ( 3.0f - 2.0f * t );
}

/** @brief The distance from a point to a cell's square (0 inside). */
float toSquare( float x, float z, float x0, float z0 )
{
    float dx = std::max( { x0 - x, 0.0f, x - x0 - CellSize } ), dz = std::max( { z0 - z, 0.0f, z - z0 - CellSize } );
    return sqrtf( dx * dx + dz * dz );
}

/** @brief The distance from a point to a segment. */
float toSegment( Vector2 p, Vector2 a, Vector2 b )
{
    Vector2 ab = Vector2Subtract( b, a );
    float t = Clamp( Vector2DotProduct( Vector2Subtract( p, a ), ab ) / std::max( Vector2DotProduct( ab, ab ), 1e-9f ),
                     0.0f, 1.0f );
    return Vector2Distance( p, Vector2Add( a, Vector2Scale( ab, t ) ) );
}

/** @brief The distance from a point to a triangle (0 inside). */
float toTriangle( Vector2 p, Vector2 a, Vector2 b, Vector2 c )
{
    auto  side = [&]( Vector2 u, Vector2 v ) { return ( v.x - u.x ) * ( p.y - u.y ) - ( v.y - u.y ) * ( p.x - u.x ); };
    float s1 = side( a, b ), s2 = side( b, c ), s3 = side( c, a );
    bool  neg = s1 < 0.0f || s2 < 0.0f || s3 < 0.0f, pos = s1 > 0.0f || s2 > 0.0f || s3 > 0.0f;
    if ( !( neg && pos ) ) return 0.0f;
    return std::min( { toSegment( p, a, b ), toSegment( p, b, c ), toSegment( p, c, a ) } );
}

/** @brief Where a point of the relief's grid lies along one axis (m). */
float gridAt( int g )
{
    int cell = g >= 0 ? g / Fine : -( ( -g + Fine - 1 ) / Fine );
    return cell * CellSize + Steps[g - cell * Fine];
}

}   // namespace

bool Level::sloped( int r, int c ) const
{
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols || m_slope.empty() ) return false;
    return m_slope[(size_t)r * m_cols + c] != 0;
}

ThemeE Level::rockTheme( int r, int c ) const
{
    return (ThemeE)m_rockOf[(size_t)r * m_cols + c];
}

void Level::planRelief()
{
    size_t cells = (size_t)m_rows * m_cols;
    m_slope.assign( cells, 0 );
    m_rockOf.assign( cells, (unsigned char)ThemeE::SERVICE );
    m_relief.assign( (size_t)( m_cols * Fine + 1 ) * ( m_rows * Fine + 1 ), Unknown );
    // Each wall cell's nearest open cell (diagonals count, as the gardens reckon it): its rock is that place's.
    std::vector<int> nearest( cells, -1 );
    std::deque<int>  todo;
    for ( size_t i = 0; i < cells; ++i )
        if ( m_cells[i].m_kind != CellE::ROCK ) nearest[i] = (int)i, todo.push_back( (int)i );
    while ( !todo.empty() )
    {
        int i = todo.front();
        todo.pop_front();
        int r = i / m_cols, c = i % m_cols;
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
            {
                int nr = r + dr, nc = c + dc;
                if ( nr < 0 || nc < 0 || nr >= m_rows || nc >= m_cols ) continue;
                size_t n = (size_t)nr * m_cols + nc;
                if ( nearest[n] >= 0 ) continue;
                nearest[n] = nearest[i];
                todo.push_back( (int)n );
            }
    }
    auto wildAt = [&]( int r, int c ) { return wild( theme( r, c ) ) && outdoor( r, c ); };
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            size_t i = (size_t)r * m_cols + c;
            if ( m_cells[i].m_kind != CellE::ROCK || garden( r, c ) || nearest[i] < 0 ) continue;
            int nr = nearest[i] / m_cols, nc = nearest[i] % m_cols;
            if ( !wildAt( nr, nc ) ) continue;
            // Upright where a door, a shaft or another place's floor is beside it (frames, fittings, the abyss).
            bool open = true;
            for ( int dr = -1; dr <= 1 && open; ++dr )
                for ( int dc = -1; dc <= 1 && open; ++dc )
                {
                    const Cell &k = cell( r + dr, c + dc );
                    if ( k.m_kind == CellE::ROCK ) continue;
                    if ( k.m_kind == CellE::DOOR || k.m_kind == CellE::VOID || !wildAt( r + dr, c + dc ) ) open = false;
                }
            if ( !open ) continue;
            m_slope[i] = 1;
            m_rockOf[i] = (unsigned char)theme( nr, nc );
        }
    // Their outer corners all cut half a cell deep (as raw rock's), no niches; the corners filled in front of them go
    // (the slope rounds them); a cell with sloped rock all round has only its top to show (a coarser grid).
    auto flat = []( CellE e ) { return e == CellE::FLOOR || e == CellE::GLASS; };
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            size_t i = (size_t)r * m_cols + c;
            if ( m_slope[i] )
            {
                m_niche[i] = -1;
                m_cut[i] = 0;
                for ( int k = 0; k < 4; ++k )
                    if ( flat( cell( r + CR[k], c ).m_kind ) && flat( cell( r, c + CC[k] ).m_kind ) &&
                         flat( cell( r + CR[k], c + CC[k] ).m_kind ) )
                        m_cut[i] |= (unsigned char)( 1 << k );
                bool all = true;
                for ( int dr = -1; dr <= 1; ++dr )
                    for ( int dc = -1; dc <= 1; ++dc ) all = all && sloped( r + dr, c + dc );
                if ( all ) m_slope[i] = 2;
            }
            else if ( m_fill[i] )
                for ( int k = 0; k < 4; ++k )
                    if ( sloped( r + CR[k], c ) || sloped( r, c + CC[k] ) || sloped( r + CR[k], c + CC[k] ) )
                        m_fill[i] &= (unsigned char)~( 1 << k );
        }
}

float Level::reliefAt( float x, float z ) const
{
    // The nearest open ground (its cut corners count as such) and its floor; the nearest wall that stands upright.
    int   r = (int)floorf( z / CellSize ), c = (int)floorf( x / CellSize );
    float open = 1e9f, floor = 0.0f, upright = 1e9f;
    for ( int dr = -2; dr <= 2; ++dr )
        for ( int dc = -2; dc <= 2; ++dc )
        {
            int         rr = r + dr, cc = c + dc;
            const Cell &k = cell( rr, cc );
            float       x0 = cc * CellSize, z0 = rr * CellSize;
            if ( k.m_kind == CellE::VOID ) continue;
            if ( k.m_kind == CellE::ROCK || k.m_kind == CellE::DOOR )
            {
                if ( !sloped( rr, cc ) )
                {
                    upright = std::min( upright, toSquare( x, z, x0, z0 ) );
                    continue;
                }
                unsigned char cut = m_cut[(size_t)rr * m_cols + cc];
                for ( int q = 0; q < 4 && cut; ++q )
                {
                    if ( !( cut & ( 1 << q ) ) ) continue;
                    Vector2 corner = { x0 + ( CC[q] > 0 ? CellSize : 0.0f ), z0 + ( CR[q] > 0 ? CellSize : 0.0f ) };
                    float   d = toTriangle( { x, z }, corner, { corner.x - CC[q] * Natural, corner.y },
                                            { corner.x, corner.y - CR[q] * Natural } );
                    if ( d < open )
                    {
                        const Cell &f = cell( rr + CR[q], cc + CC[q] );
                        open = d, floor = f.m_kind == CellE::FLOOR ? f.m_height : 0.0f;
                    }
                }
                continue;
            }
            float d = toSquare( x, z, x0, z0 );
            if ( d < open ) open = d, floor = k.m_kind == CellE::FLOOR ? k.m_height : 0.0f;
        }
    if ( open <= 0.0f ) return floor - 0.05f;   // under the ground's own top
    // An uneven top; from the ground's edge a steep face (a shot at chest height strikes near it) that leans back to
    // it over an uneven reach, lumpy below.
    float top = m_wallHeight + 0.45f * smoothNoise( { x * 0.13f, 0.3f, z * 0.13f }, 811u ) +
                0.2f * smoothNoise( { x * 0.3f, 1.7f, z * 0.3f }, 812u );
    float h = top;
    if ( open < 1e8f )
    {
        float reach = 1.3f + 0.3f * smoothNoise( { x * 0.27f, 2.9f, z * 0.27f }, 813u );
        float steep = 1.0f - powf( 1.0f - Clamp( open / 0.3f, 0.0f, 1.0f ), 2.0f );
        float lean = smooth( 0.15f, 1.0f, open / reach );
        float lump = 0.25f * smoothNoise( { x * 0.8f, 4.1f, z * 0.8f }, 814u ) * Clamp( open / 0.35f, 0.0f, 1.0f ) *
                     ( 1.0f - lean );
        h = floor + ( top - floor ) * ( 0.45f * steep + 0.55f * lean ) + lump;
    }
    // Level with the top of a wall that stands upright beside it (its lid).
    if ( upright < 1.0f ) h = Lerp( m_wallHeight + 0.02f, h, smooth( 0.0f, 1.0f, upright ) );
    return h;
}

float Level::reliefPoint( int gx, int gz ) const
{
    int w = m_cols * Fine + 1, n = m_rows * Fine + 1;
    if ( gx < 0 || gz < 0 || gx >= w || gz >= n || m_relief.empty() ) return reliefAt( gridAt( gx ), gridAt( gz ) );
    float &v = m_relief[(size_t)gz * w + gx];
    if ( v == Unknown ) v = reliefAt( gridAt( gx ), gridAt( gz ) );
    return v;
}

float Level::reliefLookup( float x, float z ) const
{
    auto split = []( float v, int &g, float &t )
    {
        int   cell = (int)floorf( v / CellSize ), i = 0;
        float f = v - cell * CellSize;
        while ( i < Fine - 1 && f >= Steps[i + 1] ) ++i;
        g = cell * Fine + i;
        t = Clamp( ( f - Steps[i] ) / ( Steps[i + 1] - Steps[i] ), 0.0f, 1.0f );
    };
    int   gx, gz;
    float tx, tz;
    split( x, gx, tx );
    split( z, gz, tz );
    float a = Lerp( reliefPoint( gx, gz ), reliefPoint( gx + 1, gz ), tx );
    float b = Lerp( reliefPoint( gx, gz + 1 ), reliefPoint( gx + 1, gz + 1 ), tx );
    return Lerp( a, b, tz );
}

void Level::reliefCell( int r, int c, MeshBuilder &b, Color face, Color top )
{
    // Deep in the rock only its top shows: a point every metre (at 0, 3 and 6 of the grid).
    const bool       coarse = m_slope[(size_t)r * m_cols + c] == 2;
    std::vector<int> at = coarse ? std::vector<int>{ 0, 3, 6 } : std::vector<int>{ 0, 1, 2, 3, 4, 5, 6 };
    auto             coarseAt = [&]( int rr, int cc )
    { return rr >= 0 && cc >= 0 && rr < m_rows && cc < m_cols && m_slope[(size_t)rr * m_cols + cc] == 2; };
    const bool edge[4] = { !coarse && coarseAt( r - 1, c ), !coarse && coarseAt( r, c + 1 ),
                           !coarse && coarseAt( r + 1, c ), !coarse && coarseAt( r, c - 1 ) };
    auto       point = [&]( int i, int j ) { return reliefPoint( c * Fine + i, r * Fine + j ); };
    // Along an edge shared with a coarse cell: straight between its points, so that no crack opens between them.
    auto height = [&]( int i, int j )
    {
        if ( ( ( j == 0 && edge[0] ) || ( j == Fine && edge[2] ) ) && i % 3 != 0 )
        {
            int a = i < 3 ? 0 : 3;
            return Lerp( point( a, j ), point( a + 3, j ), ( Steps[i] - Steps[a] ) / ( Steps[a + 3] - Steps[a] ) );
        }
        if ( ( ( i == Fine && edge[1] ) || ( i == 0 && edge[3] ) ) && j % 3 != 0 )
        {
            int a = j < 3 ? 0 : 3;
            return Lerp( point( i, a ), point( i, a + 3 ), ( Steps[j] - Steps[a] ) / ( Steps[a + 3] - Steps[a] ) );
        }
        return point( i, j );
    };
    std::vector<Vector3> pos, nrm;
    std::vector<Color>   col;
    for ( int j : at )
        for ( int i : at )
        {
            int   gx = c * Fine + i, gz = r * Fine + j;
            float h = height( i, j );
            // The normal from the grid's neighbours (the same on both sides of a cell's edge: no seams in the light).
            float dx =
                ( reliefPoint( gx - 1, gz ) - reliefPoint( gx + 1, gz ) ) / ( gridAt( gx + 1 ) - gridAt( gx - 1 ) );
            float dz =
                ( reliefPoint( gx, gz - 1 ) - reliefPoint( gx, gz + 1 ) ) / ( gridAt( gz + 1 ) - gridAt( gz - 1 ) );
            Vector3 n = Vector3Normalize( { dx, 1.0f, dz } );
            // The faces in the rock's colour, what lies flat up high darker (as the walls' lids are).
            float k = smooth( 0.6f, 0.9f, n.y ) * smooth( m_wallHeight * 0.55f, m_wallHeight * 0.85f, h ) * 0.85f;
            col.push_back( { (unsigned char)Lerp( face.r, top.r, k ), (unsigned char)Lerp( face.g, top.g, k ),
                             (unsigned char)Lerp( face.b, top.b, k ), 255 } );
            pos.push_back( { c * CellSize + Steps[i], h, r * CellSize + Steps[j] } );
            nrm.push_back( n );
        }
    b.sheet( pos, nrm, col, (int)at.size(), (int)at.size() );
}

void Level::rubble( int r, int c, int d, MeshBuilder &b, Color colour )
{
    // Two to four stones, most of them small and close to the foot, flat and half sunk, a little paler than the rock
    // (the occlusion at the foot darkens them; round and dark they read as balls).
    const unsigned h = hashCell( r, c * 4 + d, 821 );
    const int      count = 2 + (int)( h % 3 );
    const Vector3  in = { (float)-DC[d], 0.0f, (float)-DR[d] };
    const Vector3  along = { DR[d] != 0 ? 1.0f : 0.0f, 0.0f, DC[d] != 0 ? 1.0f : 0.0f };
    const Cell    &k = cell( r, c );
    const float    base = k.m_kind == CellE::FLOOR ? k.m_height : 0.0f;
    const Vector3  foot = { ( c + 0.5f + DC[d] * 0.5f ) * CellSize, base, ( r + 0.5f + DR[d] * 0.5f ) * CellSize };
    const Color    keep = b.m_color;
    for ( int i = 0; i < count; ++i )
    {
        int     salt = r * 7 + i, at = c * 4 + d;
        float   u = ( hash01( salt, at, 822 ) - 0.5f ) * 1.8f;
        float   size = 0.06f + 0.14f * powf( hash01( salt, at, 824 ), 2.0f );
        float   out = 0.03f + 0.3f * powf( hash01( salt, at, 823 ), 2.0f ) + size * 0.5f;
        float   tone = 1.15f + 0.3f * hash01( salt, at, 825 );
        Vector3 p = Vector3Add( Vector3Add( foot, Vector3Scale( along, u ) ), Vector3Scale( in, out ) );
        p.y = base + size * 0.12f;
        b.m_color = { (unsigned char)fminf( colour.r * tone, 255.0f ), (unsigned char)fminf( colour.g * tone, 255.0f ),
                      (unsigned char)fminf( colour.b * tone, 255.0f ), 255 };
        b.blob( place( p, { ( hash01( salt, at, 826 ) - 0.5f ) * 0.5f, hash01( salt, at, 827 ) * 6.28f, 0.0f } ),
                { size, size * 0.45f, size * 0.8f }, 0.45f, h + (unsigned)i * 7u, 3 );
    }
    b.m_color = keep;
}

void Level::rockFoot( int r, int c, int d, MeshBuilder &b, Color colour )
{
    const int           rr = r + DR[d], rc = c + DC[d];   // the rock's cell
    const Cell         &k = cell( r, c );
    const float         base = k.m_kind == CellE::FLOOR ? k.m_height : 0.0f;
    const Vector3       out = { (float)-DC[d], 0.0f, (float)-DR[d] };   // from the rock into this cell
    const Vector3       along = { DR[d] != 0 ? 1.0f : 0.0f, 0.0f, DC[d] != 0 ? 1.0f : 0.0f };
    const Vector3       mid = { ( c + 0.5f + DC[d] * 0.5f ) * CellSize, base, ( r + 0.5f + DR[d] * 0.5f ) * CellSize };
    const unsigned char cut = m_cut[(size_t)rr * m_cols + rc];
    // A band from p0 to p1 along the foot, `n` its way out: a row hidden in the rock, a lumpy crest a little out and
    // up, a toe sunk into the ground further out; how far and how high wander along it by a noise of the place, the
    // same where two bands meet.
    auto band = [&]( Vector3 p0, Vector3 p1, Vector3 n, float hidden )
    {
        float                len = Vector3Distance( p0, p1 );
        int                  steps = std::max( 2, (int)ceilf( len / 0.2f ) );
        int                  nx = steps + 1;
        std::vector<Vector3> pos( (size_t)nx * 3 ), nrm( (size_t)nx * 3 );
        std::vector<Color>   col( (size_t)nx * 3 );
        Vector3              t = Vector3Scale( Vector3Subtract( p1, p0 ), 1.0f / len );
        for ( int i = 0; i < nx; ++i )
        {
            Vector3 p = Vector3Lerp( p0, p1, (float)i / steps );
            auto    wave = [&]( float f, unsigned seed )
            {
                return 0.5f +
                       0.5f * Clamp( 0.75f * smoothNoise( { p.x * f, 0.7f, p.z * f }, seed ) +
                                         0.25f * smoothNoise( { p.x * f * 3.1f, 1.9f, p.z * f * 3.1f }, seed + 1u ),
                                     -1.0f, 1.0f );
            };
            // (Out to 15 cm at its crest, the toe a hand further: less is lost under a pixel, and under Brannt's crust.)
            float crestOut = 0.03f + 0.12f * wave( 2.3f, 831u ), crestUp = 0.08f + 0.14f * wave( 1.9f, 833u );
            float toeOut = crestOut + 0.06f + 0.08f * wave( 2.7f, 835u );
            pos[(size_t)i] = Vector3Add( Vector3Subtract( p, Vector3Scale( n, 0.1f ) ), { 0.0f, hidden, 0.0f } );
            pos[(size_t)nx + i] = Vector3Add( Vector3Add( p, Vector3Scale( n, crestOut ) ), { 0.0f, crestUp, 0.0f } );
            pos[(size_t)2 * nx + i] = Vector3Add( Vector3Add( p, Vector3Scale( n, toeOut ) ), { 0.0f, -0.03f, 0.0f } );
        }
        for ( int j = 0; j < 3; ++j )
            for ( int i = 0; i < nx; ++i )
            {
                size_t  q = (size_t)j * nx + i;
                Vector3 across = Vector3Subtract( pos[(size_t)std::min( j + 1, 2 ) * nx + i],
                                                  pos[(size_t)std::max( j - 1, 0 ) * nx + i] );
                Vector3 m = Vector3Normalize( Vector3CrossProduct( t, across ) );
                if ( Vector3DotProduct( m, Vector3Add( n, { 0.0f, 0.7f, 0.0f } ) ) < 0.0f ) m = Vector3Negate( m );
                nrm[q] = m;
                float shade = j == 2 ? 0.88f : 1.0f;
                col[q] = { (unsigned char)( colour.r * shade ), (unsigned char)( colour.g * shade ),
                           (unsigned char)( colour.b * shade ), 255 };
            }
        b.sheet( pos, nrm, col, nx, 3 );
    };
    // Along the edge, as far as the rock's corners are not cut (there the foot runs across the cut, below).
    float from = -1.0f, to = 1.0f;
    for ( int q = 0; q < 4; ++q )
    {
        if ( !( cut & ( 1 << q ) ) ) continue;
        // Its corners on this side: towards this cell from the rock.
        bool here = DR[d] != 0 ? CR[q] == -DR[d] : CC[q] == -DC[d];
        if ( !here ) continue;
        float end = DR[d] != 0 ? (float)CC[q] : (float)CR[q];   // which end of the edge (along)
        if ( end < 0.0f )
            from = -1.0f + Natural / ( CellSize * 0.5f );
        else
            to = 1.0f - Natural / ( CellSize * 0.5f );
        // The cut's own foot, a diagonal: once (from the cell north or south of the rock).
        if ( DR[d] == 0 ) continue;
        Vector3 corner = { ( rc + ( CC[q] > 0 ? 1.0f : 0.0f ) ) * CellSize, base,
                           ( rr + ( CR[q] > 0 ? 1.0f : 0.0f ) ) * CellSize };
        Vector3 a = { corner.x - CC[q] * Natural, base, corner.z }, e = { corner.x, base, corner.z - CR[q] * Natural };
        band( a, e, Vector3Normalize( { (float)CC[q], 0.0f, (float)CR[q] } ), 0.12f );
    }
    if ( to - from > 0.05f )
        band( Vector3Add( mid, Vector3Scale( along, from * CellSize * 0.5f ) ),
              Vector3Add( mid, Vector3Scale( along, to * CellSize * 0.5f ) ), out, 0.26f );
}

void Level::screeApron( int r, int c, int d, MeshBuilder &b ) const
{
    const int           rr = r + DR[d], rc = c + DC[d];   // the rock's cell
    const Cell         &k = cell( r, c );
    const float         base = k.m_kind == CellE::FLOOR ? k.m_height : 0.0f;
    const Vector3       out = { (float)-DC[d], 0.0f, (float)-DR[d] };
    const Vector3       along = { DR[d] != 0 ? 1.0f : 0.0f, 0.0f, DC[d] != 0 ? 1.0f : 0.0f };
    const Vector3       mid = { ( c + 0.5f + DC[d] * 0.5f ) * CellSize, base, ( r + 0.5f + DR[d] * 0.5f ) * CellSize };
    const unsigned char cut = m_cut[(size_t)rr * m_cols + rc];
    // A slope of scree from inside the rock's face out over the ground, its outline wandering by a noise of the place
    // (20-46 cm out; the same where two aprons meet), heaped up the face in places (5-60 cm): where it meets the face
    // the line wanders too, and the rock meets the ground in a ragged line, not along the cells' edges. Three rows a
    // third of a metre apart: hidden in the face, the slope's shoulder, its toe sunk into the ground (some 25
    // triangles a cell's edge).
    auto apron = [&]( Vector3 p0, Vector3 p1, Vector3 n )
    {
        float                len = Vector3Distance( p0, p1 );
        int                  steps = std::max( 2, (int)ceilf( len / 0.34f ) );
        int                  nx = steps + 1;
        std::vector<Vector3> pos( (size_t)nx * 3 ), nrm( (size_t)nx * 3 );
        std::vector<Color>   col( (size_t)nx * 3, WHITE );
        Vector3              t = Vector3Scale( Vector3Subtract( p1, p0 ), 1.0f / len );
        for ( int i = 0; i < nx; ++i )
        {
            Vector3 p = Vector3Lerp( p0, p1, (float)i / steps );
            auto    wave = [&]( float f, unsigned seed )
            { return 0.5f + 0.5f * Clamp( smoothNoise( { p.x * f, 0.4f, p.z * f }, seed ), -1.0f, 1.0f ); };
            float toe = 0.2f + 0.2f * wave( 1.1f, 851u ) + 0.06f * wave( 2.3f, 853u );
            float heap = wave( 0.8f, 855u ), rise = 0.05f + 0.55f * heap * heap + 0.06f * wave( 2.9f, 857u );
            pos[(size_t)i] = Vector3Add( Vector3Subtract( p, Vector3Scale( n, 0.2f ) ), { 0.0f, rise + 0.45f, 0.0f } );
            pos[(size_t)nx + i] = Vector3Add( Vector3Add( p, Vector3Scale( n, toe * 0.3f ) ), { 0.0f, rise, 0.0f } );
            pos[(size_t)2 * nx + i] = Vector3Add( Vector3Add( p, Vector3Scale( n, toe ) ), { 0.0f, -0.015f, 0.0f } );
        }
        for ( int j = 0; j < 3; ++j )
            for ( int i = 0; i < nx; ++i )
            {
                Vector3 across = Vector3Subtract( pos[(size_t)std::min( j + 1, 2 ) * nx + i],
                                                  pos[(size_t)std::max( j - 1, 0 ) * nx + i] );
                Vector3 m = Vector3Normalize( Vector3CrossProduct( t, across ) );
                if ( m.y < 0.0f ) m = Vector3Negate( m );
                // Its toe as flat as the ground it runs into (no crease in the light where it meets it).
                nrm[(size_t)j * nx + i] = j == 2 ? Vector3{ 0.0f, 1.0f, 0.0f } : m;
            }
        b.sheet( pos, nrm, col, nx, 3 );
    };
    float from = -1.0f, to = 1.0f;
    for ( int q = 0; q < 4; ++q )
    {
        if ( !( cut & ( 1 << q ) ) ) continue;
        bool here = DR[d] != 0 ? CR[q] == -DR[d] : CC[q] == -DC[d];
        if ( !here ) continue;
        float end = DR[d] != 0 ? (float)CC[q] : (float)CR[q];
        if ( end < 0.0f )
            from = -1.0f + Natural / ( CellSize * 0.5f );
        else
            to = 1.0f - Natural / ( CellSize * 0.5f );
        if ( DR[d] == 0 ) continue;
        Vector3 corner = { ( rc + ( CC[q] > 0 ? 1.0f : 0.0f ) ) * CellSize, base,
                           ( rr + ( CR[q] > 0 ? 1.0f : 0.0f ) ) * CellSize };
        apron( { corner.x - CC[q] * Natural, base, corner.z }, { corner.x, base, corner.z - CR[q] * Natural },
               Vector3Normalize( { (float)CC[q], 0.0f, (float)CR[q] } ) );
    }
    if ( to - from > 0.05f )
        apron( Vector3Add( mid, Vector3Scale( along, from * CellSize * 0.5f ) ),
               Vector3Add( mid, Vector3Scale( along, to * CellSize * 0.5f ) ), out );
}

void Level::prepareOcclusion()
{
    size_t cells = (size_t)m_rows * m_cols;
    m_occProps.assign( cells, {} );
    m_occTop.assign( cells, 0.0f );
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            size_t      i = (size_t)r * m_cols + c;
            const Cell &k = m_cells[i];
            switch ( k.m_kind )
            {
            case CellE::ROCK:
                m_occTop[i] = m_slope[i] ? m_wallHeight + 0.8f : ( garden( r, c ) ? 1.6f : m_wallHeight );
                break;
            case CellE::VOID: m_occTop[i] = -1000.0f; break;
            case CellE::FIN: m_occTop[i] = FinHeight; break;
            case CellE::COUNTER: m_occTop[i] = CounterHeight; break;
            case CellE::FLOOR: m_occTop[i] = k.m_height; break;
            default: break;
            }
        }
    // What breaks is not baked in: it would leave its shadow behind.
    std::vector<bool> breaks( m_props.size(), false );
    for ( const BreakSpot &b : m_breaks )
        if ( b.m_prop >= 0 && b.m_prop < (int)breaks.size() ) breaks[(size_t)b.m_prop] = true;
    for ( size_t p = 0; p < m_props.size(); ++p )
    {
        if ( breaks[p] ) continue;
        const Prop &q = m_props[p];
        int         r0, c0, r1, c1;
        cellOf( { q.m_pos.x - q.m_radius, 0.0f, q.m_pos.z - q.m_radius }, r0, c0 );
        cellOf( { q.m_pos.x + q.m_radius, 0.0f, q.m_pos.z + q.m_radius }, r1, c1 );
        for ( int r = std::max( 0, r0 ); r <= std::min( m_rows - 1, r1 ); ++r )
            for ( int c = std::max( 0, c0 ); c <= std::min( m_cols - 1, c1 ); ++c )
            {
                size_t i = (size_t)r * m_cols + c;
                m_occProps[i].push_back( (int)p );
                m_occTop[i] = std::max( m_occTop[i], q.m_pos.y + PropTop );
            }
    }
}

float Level::standsAt( float x, float z ) const
{
    int r = (int)floorf( z / CellSize ), c = (int)floorf( x / CellSize );
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols ) return m_wallHeight;
    size_t      i = (size_t)r * m_cols + c;
    const Cell &k = m_cells[i];
    float       h = 0.0f;
    switch ( k.m_kind )
    {
    case CellE::ROCK:
        if ( m_slope[i] )
        {
            h = reliefLookup( x, z );
            break;
        }
        h = garden( r, c ) ? 1.6f : m_wallHeight;
        // A cut corner is open floor.
        for ( int q = 0; q < 4 && m_cut[i]; ++q )
        {
            float cs = natural( r, c ) ? Natural : CutSize;
            float cx = ( c + ( CC[q] > 0 ? 1 : 0 ) ) * CellSize, cz = ( r + ( CR[q] > 0 ? 1 : 0 ) ) * CellSize;
            if ( ( m_cut[i] & ( 1 << q ) ) && fabsf( x - cx ) + fabsf( z - cz ) < cs )
            {
                const Cell &f = cell( r + CR[q], c + CC[q] );
                h = f.m_kind == CellE::FLOOR ? f.m_height : 0.0f;
                break;
            }
        }
        break;
    case CellE::VOID: return -1000.0f;
    case CellE::FIN: h = FinHeight; break;
    case CellE::COUNTER: h = CounterHeight; break;
    case CellE::FLOOR: h = k.m_height; break;
    default: break;
    }
    for ( int p : m_occProps[i] )
    {
        const Prop &q = m_props[(size_t)p];
        float       dx = x - q.m_pos.x, dz = z - q.m_pos.z;
        if ( dx * dx + dz * dz < q.m_radius * q.m_radius ) h = std::max( h, q.m_pos.y + PropTop );
    }
    return h;
}

bool Level::floorFor( Vector3 p, Vector3 n, float &h ) const
{
    float l = sqrtf( n.x * n.x + n.z * n.z );
    for ( int k = 0; k < 2; ++k )
    {
        int r, c;
        cellOf( p, r, c );
        const Cell &cl = cell( r, c );
        if ( cl.m_kind == CellE::VOID ) return false;
        if ( cl.m_kind != CellE::ROCK )
        {
            h = cl.m_kind == CellE::FLOOR ? cl.m_height : 0.0f;
            return true;
        }
        // In rock (a wall's or a slope's face): the open cell it faces.
        if ( l < 0.2f ) return false;
        p = { p.x + n.x / l * 0.6f, p.y, p.z + n.z / l * 0.6f };
    }
    return false;
}

float Level::occlusion( Vector3 p, Vector3 n ) const
{
    if ( m_occTop.empty() ) return 1.0f;
    static const float Dir[8][2] = { { 0.9239f, 0.3827f },  { 0.3827f, 0.9239f },   { -0.3827f, 0.9239f },
                                     { -0.9239f, 0.3827f }, { -0.9239f, -0.3827f }, { -0.3827f, -0.9239f },
                                     { 0.3827f, -0.9239f }, { 0.9239f, -0.3827f } };
    // A little out along the normal: a vertex on a face's edge would otherwise see the wall it belongs to.
    Vector3 q = Vector3Add( p, Vector3Scale( n, 0.03f ) );
    float   side = 1.0f - fabsf( n.y ), hf = 0.0f;
    bool    floored = floorFor( q, n, hf );
    float   y = floored ? std::max( q.y, hf + 0.01f ) : q.y;   // (below the floor: as at the floor)
    // Near the floor a face that looks sideways or down sees less of the room: the corner it makes with the floor.
    float under = 0.0f;
    if ( floored && ( side > 0.3f || n.y < -0.5f ) )
    {
        float dy = std::max( q.y - hf, 0.0f );
        under = n.y < -0.5f ? 0.5f * ( 1.0f - smooth( 0.05f, 0.6f, dy ) )
                            : 0.45f * side * ( 1.0f - smooth( 0.0f, 0.9f, dy ) );
    }
    // What stands higher than the point within reach, in eight directions over the face's side: how high it stands
    // how near (the sine of its angle, less with distance) hides that much of the sky.
    float sky = 0.0f;
    if ( n.y > -0.5f )
    {
        int r, c;
        cellOf( q, r, c );
        float highest = -1e9f;
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
            {
                int rr = r + dr, cc = c + dc;
                highest = std::max( highest, rr < 0 || cc < 0 || rr >= m_rows || cc >= m_cols
                                                 ? m_wallHeight
                                                 : m_occTop[(size_t)rr * m_cols + cc] );
            }
        if ( highest > y + 0.02f )
        {
            float sum = 0.0f, weight = 0.0f;
            for ( const auto &d : Dir )
            {
                float w = fabsf( n.y ) + side * Clamp( ( d[0] * n.x + d[1] * n.z ) * 3.0f + 0.5f, 0.0f, 1.0f );
                if ( w < 0.01f ) continue;
                float o = 0.0f;
                for ( float t : Look )
                {
                    float rise = standsAt( q.x + d[0] * t, q.z + d[1] * t ) - y;
                    if ( rise > 0.0f )
                        o = std::max( o, rise / sqrtf( rise * rise + t * t ) * ( 1.0f - smooth( 0.4f, Reach, t ) ) );
                }
                sum += w * o;
                weight += w;
            }
            sky = weight > 0.0f ? sum / weight : 0.0f;
        }
    }
    return Clamp( ( 1.0f - sky ) * ( 1.0f - under ), 0.45f, 1.0f );
}

void Level::shadeOcclusion( MeshBuilder &b ) const
{
    b.shade( [this]( Vector3 p, Vector3 n ) { return occlusion( p, n ); }, 0.4f );
}

}   // namespace spaceopera
