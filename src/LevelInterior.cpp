/**
 * @file LevelInterior.cpp
 * @brief The interiors' light and floors (the graphics round's fourth phase): masks that keep each fixed light in its
 *        room, the wet map for puddles and polished floors, sunken channels under gratings in the floors, the wall
 *        lamps of each kind of place and their cones of light in the haze.
 * @author Rene Weller
 */
#include "Level.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace spaceopera
{

namespace
{

const int       DR[4] = { -1, 0, 1, 0 };   ///< north, east, south, west (as Level.cpp)
const int       DC[4] = { 0, 1, 0, -1 };
const int       CR[4] = { -1, -1, 1, 1 };   ///< a cell's corners: north-west, north-east, south-east, south-west
const int       CC[4] = { -1, 1, 1, -1 };
constexpr float CutSize = 0.75f;    ///< an outer corner cut off this far (as Level.cpp)
constexpr float FillSize = 0.55f;   ///< an inner corner filled this far (as Level.cpp)
constexpr float Natural = 0.98f;    ///< ... both in raw rock (as Level.cpp)

constexpr int   MaskTexels = 64;    ///< a light mask's square (texels a side)
constexpr int   MaskRays = 720;     ///< rays cast from each light (every half degree)
constexpr float WallLit = 0.6f;     ///< how deep into the wall its light still counts (its face, for the filtering)
constexpr int   Fine = 8;           ///< the walls' grid for the masks: this many points to a cell's edge
constexpr float WallReach = 0.7f;   ///< a light in a wall shines from the nearest open point this near (m)

/** @brief A thin bar from a to e. */
void rod( MeshBuilder &b, Vector3 a, Vector3 e, float half )
{
    Vector3 z = Vector3Subtract( e, a );
    float   len = Vector3Length( z );
    if ( len < 1e-4f ) return;
    z = Vector3Scale( z, 1.0f / len );
    Vector3 x = fabsf( z.y ) > 0.99f ? Vector3{ 1.0f, 0.0f, 0.0f }
                                     : Vector3Normalize( Vector3CrossProduct( { 0.0f, 1.0f, 0.0f }, z ) );
    b.box( frame( x, Vector3CrossProduct( z, x ), z, Vector3Lerp( a, e, 0.5f ) ), { half, half, len * 0.5f } );
}

/** @brief How wet a place's floors are (puddles) and how polished (0 .. 1 each). */
void wetness( ThemeE t, float &wet, float &gloss )
{
    wet = gloss = 0.0f;
    switch ( t )
    {
    case ThemeE::EXCHANGER: wet = 1.0f; break;
    case ThemeE::SHAFT: wet = 0.8f; break;
    case ThemeE::STREET: wet = 0.75f; break;
    case ThemeE::DECK: wet = 0.6f; break;
    case ThemeE::WORKS: wet = 0.45f; break;
    case ThemeE::SERVICE: wet = 0.35f; break;
    case ThemeE::PRISON: wet = 0.25f; break;
    case ThemeE::BAR: wet = 0.2f, gloss = 0.35f; break;
    case ThemeE::STATION: wet = 0.2f, gloss = 0.1f; break;
    case ThemeE::NET: wet = 0.15f, gloss = 0.2f; break;
    case ThemeE::CARGO:
    case ThemeE::HOLD: wet = 0.1f; break;
    case ThemeE::GANGWAY: gloss = 0.25f; break;
    case ThemeE::BRIDGE: gloss = 0.45f; break;
    case ThemeE::VAULT: gloss = 0.65f; break;
    case ThemeE::PODS: gloss = 0.45f; break;
    case ThemeE::CLINIC: gloss = 0.5f; break;
    case ThemeE::HALL: gloss = 0.6f; break;
    case ThemeE::SERVER: gloss = 0.4f; break;
    case ThemeE::CRYO: gloss = 0.3f; break;
    case ThemeE::COLDSTORE: gloss = 0.15f; break;
    case ThemeE::ALIEN: gloss = 0.5f; break;
    default: break;
    }
}

}   // namespace

bool Level::blocksLight( float x, float z ) const
{
    int r = (int)floorf( z / CellSize ), c = (int)floorf( x / CellSize );
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols ) return true;
    size_t      i = (size_t)r * m_cols + c;
    const Cell &k = m_cells[i];
    if ( k.m_kind == CellE::ROCK )
    {
        if ( !m_garden.empty() && m_garden[i] ) return false;   // a garden's hedges and trees let it through
        if ( sloped( r, c ) ) return false;   // the rock out in the open leans back below the lamps: it reaches over
        // A cut corner is open floor (as standsAt).
        for ( int q = 0; q < 4 && !m_cut.empty() && m_cut[i]; ++q )
        {
            float cs = natural( r, c ) ? Natural : CutSize;
            float cx = ( c + ( CC[q] > 0 ? 1 : 0 ) ) * CellSize, cz = ( r + ( CR[q] > 0 ? 1 : 0 ) ) * CellSize;
            if ( ( m_cut[i] & ( 1 << q ) ) && fabsf( x - cx ) + fabsf( z - cz ) < cs ) return false;
        }
        return true;
    }
    // A filled inner corner of a room stops it as its walls do.
    if ( ( k.m_kind == CellE::FLOOR || k.m_kind == CellE::GLASS ) && !m_fill.empty() && m_fill[i] )
        for ( int q = 0; q < 4; ++q )
        {
            if ( !( m_fill[i] & ( 1 << q ) ) ) continue;
            float fs = natural( r + CR[q], c + CC[q] ) ? Natural : FillSize;
            float cx = ( c + ( CC[q] > 0 ? 1 : 0 ) ) * CellSize, cz = ( r + ( CR[q] > 0 ? 1 : 0 ) ) * CellSize;
            if ( fabsf( x - cx ) + fabsf( z - cz ) < fs ) return true;
        }
    return false;
}

LightMasks Level::lightMasks() const
{
    LightMasks out;
    size_t     n = m_lights.size();
    out.m_place.assign( n, { 0.0f, 0.0f, 0.0f, 0.0f } );
    out.m_reach.assign( n, { -1e9f, -1e9f, 1e9f, 1e9f } );
    if ( n == 0 || m_rows == 0 ) return out;
    // The walls on a fine grid once (eight points to a cell's edge), so that each ray's step is a look-up.
    const int                  gw = m_cols * Fine, gh = m_rows * Fine;
    const float                gs = CellSize / Fine;
    std::vector<unsigned char> solid( (size_t)gw * gh );
    for ( int j = 0; j < gh; ++j )
        for ( int i = 0; i < gw; ++i )
            solid[(size_t)j * gw + i] = blocksLight( ( i + 0.5f ) * gs, ( j + 0.5f ) * gs ) ? 1 : 0;
    auto blocked = [&]( float x, float z )
    {
        int i = (int)floorf( x / gs ), j = (int)floorf( z / gs );
        return i < 0 || j < 0 || i >= gw || j >= gh || solid[(size_t)j * gw + i];
    };
    // Which lights get one: all, indoors and out in the open (a lamp on a container stack in a yard does not light the
    // far side of it either). A light standing in a wall (a fixture mounted on it, its point a little inside) casts its
    // rays from the nearest open point in front of it within WallReach: it lights the room it faces.
    std::vector<int>     masked;
    std::vector<Vector2> from( n, { 0.0f, 0.0f } );
    const int            around = (int)ceilf( WallReach / gs );
    for ( size_t l = 0; l < n; ++l )
    {
        const LevelLight &L = m_lights[l];
        if ( L.m_radius < 1.0f ) continue;
        Vector2 o = { L.m_pos.x, L.m_pos.z };
        if ( blocked( o.x, o.y ) )
        {
            int   ci = (int)floorf( o.x / gs ), cj = (int)floorf( o.y / gs );
            float best = WallReach * WallReach;
            bool  found = false;
            for ( int dj = -around; dj <= around; ++dj )
                for ( int di = -around; di <= around; ++di )
                {
                    float x = ( ci + di + 0.5f ) * gs, z = ( cj + dj + 0.5f ) * gs;
                    float d2 = ( x - o.x ) * ( x - o.x ) + ( z - o.y ) * ( z - o.y );
                    if ( d2 < best && !blocked( x, z ) ) best = d2, found = true, from[l] = { x, z };
                }
            if ( !found )
            {
                ++out.m_inRock;   // deep in the rock: it shines as before
                continue;
            }
        }
        else
            from[l] = o;
        int r = (int)floorf( from[l].y / CellSize ), c = (int)floorf( from[l].x / CellSize );
        if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols ) continue;
        masked.push_back( (int)l );
    }
    if ( masked.empty() ) return out;
    // A square atlas of squares.
    int per = (int)ceilf( sqrtf( (float)masked.size() ) );
    per = std::min( per, 4096 / MaskTexels );
    out.m_width = out.m_height = per * MaskTexels;
    out.m_atlas.assign( (size_t)out.m_width * out.m_height, 0 );
    std::vector<unsigned char> m( (size_t)MaskTexels * MaskTexels ), soft( m.size() );
    std::vector<float>         dirX( MaskRays ), dirZ( MaskRays );
    for ( int k = 0; k < MaskRays; ++k )
    {
        float a = k * 2.0f * PI / MaskRays;
        dirX[k] = cosf( a );
        dirZ[k] = sinf( a );
    }
    for ( size_t s = 0; s < masked.size() && s < (size_t)per * per; ++s )
    {
        const LevelLight &L = m_lights[(size_t)masked[s]];
        // Its square: a little more than its reach (a warm lamp's breathes 4 % further) round it.
        const float extent = 2.2f * L.m_radius, t = extent / MaskTexels;
        const float minX = L.m_pos.x - extent * 0.5f, minZ = L.m_pos.z - extent * 0.5f;
        std::fill( m.begin(), m.end(), 0 );
        const float   reach = extent * 0.5f * 1.415f, step = t * 0.5f;
        const Vector2 o = from[(size_t)masked[s]];
        for ( int k = 0; k < MaskRays; ++k )
        {
            // Out from the light (or the open point in front of its wall) to the square's edge: lit until a wall, the
            // wall's face a little way in, then dark.
            bool  inWall = false;
            float wallAt = 0.0f;
            for ( float d = 0.0f; d < reach; d += step )
            {
                float x = o.x + dirX[k] * d, z = o.y + dirZ[k] * d;
                int   ix = (int)floorf( ( x - minX ) / t ), iz = (int)floorf( ( z - minZ ) / t );
                if ( ix < 0 || iz < 0 || ix >= MaskTexels || iz >= MaskTexels ) break;
                if ( blocked( x, z ) )
                {
                    if ( !inWall ) inWall = true, wallAt = d;
                    if ( d - wallAt > WallLit ) break;
                }
                else if ( inWall )
                    break;
                m[(size_t)iz * MaskTexels + ix] = 255;
            }
        }
        // Softened: a tent over the neighbours (the shadows' edges), the square's rim dark (its neighbours in the
        // atlas must not bleed in; it lies beyond the light's reach).
        int x0 = MaskTexels, z0 = MaskTexels, x1 = -1, z1 = -1;
        for ( int j = 0; j < MaskTexels; ++j )
            for ( int i = 0; i < MaskTexels; ++i )
            {
                int sum = 0, weight = 0;
                for ( int dj = -1; dj <= 1; ++dj )
                    for ( int di = -1; di <= 1; ++di )
                    {
                        int ii = i + di, jj = j + dj;
                        if ( ii < 0 || jj < 0 || ii >= MaskTexels || jj >= MaskTexels ) continue;
                        int w = ( di == 0 ? 2 : 1 ) * ( dj == 0 ? 2 : 1 );
                        sum += m[(size_t)jj * MaskTexels + ii] * w;
                        weight += w;
                    }
                bool          rim = i == 0 || j == 0 || i == MaskTexels - 1 || j == MaskTexels - 1;
                unsigned char v = rim ? 0 : (unsigned char)( sum / std::max( weight, 1 ) );
                soft[(size_t)j * MaskTexels + i] = v;
                if ( v > 0 )
                    x0 = std::min( x0, i ), z0 = std::min( z0, j ), x1 = std::max( x1, i ), z1 = std::max( z1, j );
            }
        // Nothing stops it within its reach (a small lamp in the middle of a room): it needs none.
        bool open = true;
        for ( int j = 1; j < MaskTexels - 1 && open; ++j )
            for ( int i = 1; i < MaskTexels - 1 && open; ++i )
            {
                float dx = ( i + 0.5f ) * t + minX - L.m_pos.x, dz = ( j + 0.5f ) * t + minZ - L.m_pos.z;
                if ( dx * dx + dz * dz < L.m_radius * L.m_radius * 1.1f && soft[(size_t)j * MaskTexels + i] < 250 )
                    open = false;
            }
        if ( open )
        {
            ++out.m_open;
            continue;
        }
        out.m_moved += o.x != L.m_pos.x || o.y != L.m_pos.z ? 1 : 0;
        int tx = (int)( s % per ), tz = (int)( s / per );
        for ( int j = 0; j < MaskTexels; ++j )
            std::copy( soft.begin() + (size_t)j * MaskTexels, soft.begin() + (size_t)( j + 1 ) * MaskTexels,
                       out.m_atlas.begin() + ( (size_t)( tz * MaskTexels + j ) * out.m_width + tx * MaskTexels ) );
        // Texture coordinates = xy + world x, z times z (the texels' middles where the rays marked them).
        float scale = 1.0f / ( t * out.m_width );
        out.m_place[(size_t)masked[s]] = { ( tx * MaskTexels - minX / t ) / out.m_width,
                                           ( tz * MaskTexels - minZ / t ) / out.m_height, scale, 1.0f };
        out.m_reach[(size_t)masked[s]] =
            x1 < 0 ? Vector4{ 0.0f, 0.0f, -1.0f, -1.0f }
                   : Vector4{ minX + x0 * t, minZ + z0 * t, minX + ( x1 + 1 ) * t, minZ + ( z1 + 1 ) * t };
    }
    return out;
}

std::vector<unsigned char> Level::wetMap() const
{
    std::vector<unsigned char> out( (size_t)m_rows * m_cols * 4, 0 );
    bool                       any = false;
    auto height = []( float h ) { return (unsigned char)Clamp( ( h + 4.0f ) / 16.0f * 255.0f + 0.5f, 0.0f, 255.0f ); };
    auto wall = [&]( int r, int c )
    {
        CellE k = cell( r, c ).m_kind;
        return k == CellE::ROCK || k == CellE::FIN || k == CellE::COUNTER;
    };
    // A drain: a sunken channel's grating, or a plate of grating laid on the floor (Level.cpp lays one in each cell
    // whose hash says so, as here).
    auto drain = [&]( int r, int c )
    {
        if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols || cell( r, c ).m_kind != CellE::FLOOR ) return false;
        return ( !m_pit.empty() && m_pit[(size_t)r * m_cols + c] != PIT_NONE ) || hashCell( r, c, 3 ) % 9 == 0;
    };
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            const Cell &k = cell( r, c );
            if ( k.m_kind != CellE::FLOOR ) continue;
            float wet, gloss;
            wetness( theme( r, c ), wet, gloss );
            // Water boils off in a vacuum, dries out in the open air and where molten metal runs.
            if ( m_vacuum || outdoor( r, c ) || m_keys.count( "lava" ) ) wet = 0.0f;
            if ( outdoor( r, c ) ) gloss = 0.0f;
            // Where water gathers (phase F2; a slow noise over the whole floor read as a camouflage pattern): by the
            // walls, round the drains, in a cell lower than the one beside it; rarely out in the middle of a room.
            float gather = 0.08f;
            for ( int d = 0; d < 4; ++d )
            {
                const Cell &n = cell( r + DR[d], c + DC[d] );
                if ( wall( r + DR[d], c + DC[d] ) ) gather = std::max( gather, 0.55f );
                if ( drain( r + DR[d], c + DC[d] ) ) gather = std::max( gather, 0.45f );
                if ( n.m_kind == CellE::FLOOR && n.m_height > k.m_height + 0.01f ) gather = std::max( gather, 0.5f );
                if ( wall( r + CR[d], c + CC[d] ) ) gather = std::max( gather, 0.3f );
            }
            if ( drain( r, c ) ) gather = 0.85f;
            unsigned char *px = &out[( (size_t)r * m_cols + c ) * 4];
            px[0] = (unsigned char)( wet * 255.0f );
            px[1] = height( k.m_height );
            px[2] = (unsigned char)( gloss * 255.0f );
            px[3] = (unsigned char)( gather * 255.0f );
            any = any || wet > 0.0f || gloss > 0.0f;
        }
    if ( !any ) return {};
    // The walls (and what stands in the room) take a neighbouring floor's values: the filtering then keeps the puddles
    // and the polish up to their feet, and their tops lie far above that floor. At a wall the water gathers most.
    std::vector<unsigned char> base = out;
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            CellE kind = cell( r, c ).m_kind;
            if ( kind != CellE::ROCK && kind != CellE::DOOR && kind != CellE::FIN && kind != CellE::COUNTER ) continue;
            for ( int d = 0; d < 4; ++d )
            {
                int rr = r + DR[d], cc = c + DC[d];
                if ( rr < 0 || cc < 0 || rr >= m_rows || cc >= m_cols || cell( rr, cc ).m_kind != CellE::FLOOR )
                    continue;
                std::copy_n( &base[( (size_t)rr * m_cols + cc ) * 4], 4, &out[( (size_t)r * m_cols + c ) * 4] );
                out[( (size_t)r * m_cols + c ) * 4 + 3] =
                    kind == CellE::ROCK ? 255 : base[( (size_t)rr * m_cols + cc ) * 4 + 3];
                break;
            }
        }
    return out;
}

void Level::planPits()
{
    m_pit.assign( (size_t)m_rows * m_cols, PIT_NONE );
    if ( s_plainInterior ) return;
    for ( int r = 1; r + 1 < m_rows; ++r )
        for ( int c = 1; c + 1 < m_cols; ++c )
        {
            const Cell &k = cell( r, c );
            size_t      i = (size_t)r * m_cols + c;
            if ( k.m_kind != CellE::FLOOR || outdoor( r, c ) ) continue;
            ThemeE t = theme( r, c );
            int    group = dressGroup( t );
            // Not in natural ground, a garden, the alien object or out on a station's skin over open space.
            if ( group >= 4 || t == ThemeE::HALL || t == ThemeE::YARD || t == ThemeE::CONTAINER || t == ThemeE::HULL ||
                 t == ThemeE::BAR )
                continue;
            if ( ( !m_fill.empty() && m_fill[i] ) || ( !m_gallery.empty() && m_gallery[i] ) ) continue;
            // Level all round: walls or floors of its height (no step's side, shaft, door, window, counter).
            int  walls = 0;
            bool ok = true;
            for ( int dr = -1; dr <= 1 && ok; ++dr )
                for ( int dc = -1; dc <= 1; ++dc )
                    if ( cell( r + dr, c + dc ).m_kind == CellE::VOID ) ok = false;
            for ( int d = 0; d < 4 && ok; ++d )
            {
                const Cell &nb = cell( r + DR[d], c + DC[d] );
                if ( nb.m_kind == CellE::ROCK )
                    walls |= 1 << d;
                else if ( nb.m_kind != CellE::FLOOR || nb.m_height != k.m_height )
                    ok = false;
            }
            if ( !ok ) continue;
            bool alongX = walls == ( 1 | 4 ), alongZ = walls == ( 2 | 8 );
            if ( t == ThemeE::STREET )
            {
                // A street's gutters run along its walls (the first of them: north, west, east, south).
                for ( int d : { 0, 3, 1, 2 } )
                    if ( walls & ( 1 << d ) )
                    {
                        m_pit[i] = (unsigned char)( PIT_GUTTER_N + d );
                        break;
                    }
            }
            else if ( alongX )
                m_pit[i] = PIT_DUCT_X;
            else if ( alongZ )
                m_pit[i] = PIT_DUCT_Z;
            if ( m_pit[i] == PIT_NONE && hashCell( r, c, 3 ) % 9 == 0 ) m_pit[i] = PIT_SUMP;
        }
}

bool Level::pitRect( int r, int c, int &i0, int &i1, int &j0, int &j1 ) const
{
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols || m_pit.empty() ) return false;
    switch ( m_pit[(size_t)r * m_cols + c] )
    {
    case PIT_SUMP: i0 = 2, i1 = 6, j0 = 2, j1 = 6; return true;
    case PIT_DUCT_X: i0 = 0, i1 = 8, j0 = 3, j1 = 5; return true;
    case PIT_DUCT_Z: i0 = 3, i1 = 5, j0 = 0, j1 = 8; return true;
    case PIT_GUTTER_N: i0 = 0, i1 = 8, j0 = 1, j1 = 3; return true;
    case PIT_GUTTER_S: i0 = 0, i1 = 8, j0 = 5, j1 = 7; return true;
    case PIT_GUTTER_W: i0 = 1, i1 = 3, j0 = 0, j1 = 8; return true;
    case PIT_GUTTER_E: i0 = 5, i1 = 7, j0 = 0, j1 = 8; return true;
    default: return false;
    }
}

void Level::pitCell( int r, int c, float h, MeshBuilder &sunk, MeshBuilder &rust, MeshBuilder &steel,
                     std::vector<GlowBox> &glows )
{
    int i0, i1, j0, j1;
    if ( !pitRect( r, c, i0, i1, j0, j1 ) ) return;
    const unsigned char kind = m_pit[(size_t)r * m_cols + c];
    const ThemeE        t = theme( r, c );
    const int           group = dressGroup( t );
    const float         e = CellSize / 8.0f;
    const float         ax = c * CellSize + i0 * e, bx = c * CellSize + i1 * e, az = r * CellSize + j0 * e,
                        bz = r * CellSize + j1 * e;
    const bool          gutter = kind >= PIT_GUTTER_N, sump = kind == PIT_SUMP;
    const float         depth = sump ? 0.5f : ( gutter ? 0.2f : 0.32f ), floorY = h - depth;
    // Its sides, darker downwards (no sky reaches in there), open where the next cell's channel runs on.
    auto side = [&]( Vector3 a, Vector3 b, Vector3 nrm )
    {
        std::vector<Vector3> pos = { a, b, { a.x, floorY, a.z }, { b.x, floorY, b.z } };
        std::vector<Vector3> nrms( 4, nrm );
        const Color          top = { 100, 98, 94, 220 }, bottom = { 70, 68, 66, 110 };
        sunk.sheet( pos, nrms, { top, top, bottom, bottom }, 2, 2 );
    };
    auto runsOn = [&]( int d )
    {
        int rr = r + DR[d], cc = c + DC[d];
        return rr >= 0 && cc >= 0 && rr < m_rows && cc < m_cols && m_pit[(size_t)rr * m_cols + cc] == kind && !sump;
    };
    // (A side at the cell's edge is open only where the channel runs on into the next cell.)
    if ( !( j0 == 0 && runsOn( 0 ) ) ) side( { ax, h, az }, { bx, h, az }, { 0.0f, 0.0f, 1.0f } );
    if ( !( j1 == 8 && runsOn( 2 ) ) ) side( { bx, h, bz }, { ax, h, bz }, { 0.0f, 0.0f, -1.0f } );
    if ( !( i0 == 0 && runsOn( 3 ) ) ) side( { ax, h, bz }, { ax, h, az }, { 1.0f, 0.0f, 0.0f } );
    if ( !( i1 == 8 && runsOn( 1 ) ) ) side( { bx, h, az }, { bx, h, bz }, { -1.0f, 0.0f, 0.0f } );
    sunk.m_color = { 64, 62, 60, 110 };
    sunk.card( { ax, floorY, az }, { bx, floorY, az }, { bx, floorY, bz }, { ax, floorY, bz }, { 0.0f, 1.0f, 0.0f } );
    const bool    alongX = bx - ax >= bz - az;
    const Vector3 mid = { ( ax + bx ) * 0.5f, floorY, ( az + bz ) * 0.5f };
    const float   len = alongX ? bx - ax : bz - az, wide = alongX ? bz - az : bx - ax;
    auto          along = [&]( float u, float y, float v )
    { return alongX ? Vector3{ mid.x + u, y, mid.z + v } : Vector3{ mid.x + v, y, mid.z + u }; };
    unsigned hsh = hashCell( r, c, 611 );
    // What runs in it, as the place has it.
    if ( t == ThemeE::FURNACE )
    {
        // Slag glowing at its bottom: the light shows between the bars.
        Vector3 g = along( 0.0f, floorY + 0.03f, 0.0f );
        glows.push_back( { g,
                           { alongX ? len * 0.5f : wide * 0.42f, 0.02f, alongX ? wide * 0.42f : len * 0.5f },
                           { 4.5f, 1.3f, 0.22f, 0.0f } } );
    }
    else if ( gutter )
    {
        // A street's water runs in it (the wet map makes its bottom shine); now and then a drain's mouth.
        if ( hsh % 3 == 0 )
        {
            sunk.m_color = { 20, 20, 22, 90 };
            sunk.cylinder( place( along( 0.0f, floorY + 0.002f, 0.0f ) ), wide * 0.3f, 0.004f, 12 );
        }
    }
    else if ( sump )
    {
        // Pipes across it, one with a valve's wheel; water standing in it where the place is wet.
        for ( int p = 0; p < 2; ++p )
        {
            float   y = floorY + 0.14f + p * 0.17f, v = ( p ? 0.22f : -0.18f ) * wide;
            Color   paint = p ? Color{ 128, 84, 60, 140 } : Color{ 96, 104, 112, 140 };
            Vector3 a = along( -len * 0.5f, y, v ), b = along( len * 0.5f, y, v );
            sunk.m_color = paint;
            rod( sunk, a, b, 0.055f - p * 0.012f );
        }
        if ( hsh % 2 )
        {
            sunk.m_color = { 150, 40, 30, 150 };
            Vector3 w = along( len * 0.15f, floorY + 0.31f + 0.07f, 0.22f * wide );
            sunk.cylinder( place( w ), 0.09f, 0.02f, 10 );
        }
    }
    else
    {
        // Cables in a duct, in the place's colours; in the clean and cold places a line of light along its bottom.
        const Color cables[4] = {
            { 40, 40, 44, 150 }, { 150, 60, 40, 150 }, { 50, 90, 140, 150 }, { 160, 140, 60, 150 }
        };
        int count = 3 + (int)( hsh % 2 );
        for ( int k = 0; k < count; ++k )
        {
            float v = ( k + 0.5f ) / count * wide * 0.8f - wide * 0.4f, y = floorY + 0.04f + 0.03f * ( k % 2 );
            sunk.m_color = cables[( k + hsh ) % 4];
            rod( sunk, along( -len * 0.5f, y, v ), along( len * 0.5f, y, v ), 0.022f + 0.008f * ( k % 2 ) );
        }
        if ( group == 2 || group == 3 )
        {
            Vector4 lit = group == 3 ? Vector4{ 0.5f, 1.4f, 2.6f, 0.0f } : Vector4{ 0.3f, 1.6f, 2.2f, 0.0f };
            if ( t == ThemeE::SERVER ) lit = { 0.3f, 2.4f, 0.9f, 0.0f };
            Vector3 g = along( 0.0f, floorY + 0.012f, 0.0f );
            glows.push_back(
                { g, { alongX ? len * 0.5f : 0.025f, 0.008f, alongX ? 0.025f : len * 0.5f }, lit, false } );
        }
        else if ( hsh % 3 == 0 )
        {
            sunk.m_color = { 110, 112, 116, 150 };
            rod( sunk, along( -len * 0.5f, floorY + 0.2f, wide * 0.25f ),
                 along( len * 0.5f, floorY + 0.2f, wide * 0.25f ), 0.045f );
        }
    }
    // The grating over it, flush with the floor: bars along it on a frame, two across beneath them; rusty in the works,
    // pale steel in the clean places. (Bars of 5 cm on a pitch of some 15: thinner ones vanish at the Deck's size, the
    // dark between them shows the depth.)
    MeshBuilder &grid = group == 2 || group == 3 ? steel : rust;
    const Color  paint = group == 2 || group == 3 ? Color{ 205, 208, 212, 255 } : Color{ 235, 225, 210, 255 };
    grid.m_color = paint;
    int bars = std::max( 3, (int)( wide / 0.15f ) );
    for ( int k = 0; k < bars; ++k )
    {
        float   v = ( k + 0.5f ) / bars * wide - wide * 0.5f;
        Vector3 a = along( -len * 0.5f, h - 0.02f, v ), b = along( len * 0.5f, h - 0.02f, v );
        grid.box( place( Vector3Lerp( a, b, 0.5f ) ),
                  { alongX ? len * 0.5f : 0.025f, 0.02f, alongX ? 0.025f : len * 0.5f } );
    }
    for ( float u : { -0.25f, 0.25f } )
    {
        Vector3 a = along( u * len, h - 0.05f, -wide * 0.5f ), b = along( u * len, h - 0.05f, wide * 0.5f );
        grid.box( place( Vector3Lerp( a, b, 0.5f ) ),
                  { alongX ? 0.015f : wide * 0.5f, 0.02f, alongX ? wide * 0.5f : 0.015f } );
    }
    // The frame round the opening, a hair over the floor.
    grid.m_color = { (unsigned char)( paint.r * 0.75f ), (unsigned char)( paint.g * 0.75f ),
                     (unsigned char)( paint.b * 0.75f ), 255 };
    const float rim = 0.025f;
    if ( !( j0 == 0 && runsOn( 0 ) ) )
        grid.box( place( { ( ax + bx ) * 0.5f, h - 0.012f, az + rim } ), { ( bx - ax ) * 0.5f, 0.016f, rim } );
    if ( !( j1 == 8 && runsOn( 2 ) ) )
        grid.box( place( { ( ax + bx ) * 0.5f, h - 0.012f, bz - rim } ), { ( bx - ax ) * 0.5f, 0.016f, rim } );
    if ( !( i0 == 0 && runsOn( 3 ) ) )
        grid.box( place( { ax + rim, h - 0.012f, ( az + bz ) * 0.5f } ), { rim, 0.016f, ( bz - az ) * 0.5f } );
    if ( !( i1 == 8 && runsOn( 1 ) ) )
        grid.box( place( { bx - rim, h - 0.012f, ( az + bz ) * 0.5f } ), { rim, 0.016f, ( bz - az ) * 0.5f } );
    grid.m_color = WHITE;
    sunk.m_color = WHITE;
}

bool Level::lamp( Vector3 at, Vector3 n, ThemeE t, Vector4 glow, Vector3 colour, bool cone, DressKit &kit, float reach )
{
    const int group = dressGroup( t );
    if ( group == 4 || group == 6 || s_plainInterior ) return false;   // out in the open, the gardens: the old housing
    const Vector3 up = { 0.0f, 1.0f, 0.0f }, wall = Vector3Subtract( at, Vector3Scale( n, 0.16f ) );
    const bool    alongX = fabsf( n.z ) > 0.5f;
    auto          flat = [&]( float a, float y, float b )
    { return Vector3{ alongX ? a : b, y, alongX ? b : a }; };   // a along the wall, b out of it (world half sizes)
    float floorY = floorAt( at.x + n.x * 0.4f, at.z + n.z * 0.4f );
    if ( floorY < -100.0f || floorY > 100.0f ) floorY = at.y - 2.6f;
    float peak = std::max( { colour.x, colour.y, colour.z, 1e-4f } );
    Color tint = { (unsigned char)( 255.0f * colour.x / peak ), (unsigned char)( 255.0f * colour.y / peak ),
                   (unsigned char)( 255.0f * colour.z / peak ), 255 };
    auto  addCone = [&]( Vector3 apex, Vector3 axis, float radius, float spread, unsigned char strength )
    {
        if ( !cone ) return;
        float drop = std::max( apex.y - floorY, 0.8f );
        float length = Clamp( drop / std::max( -axis.y, 0.3f ), 1.0f, 3.6f );
        tint.a = strength;
        m_coneSpecs.push_back( { apex, Vector3Normalize( axis ), length, radius, spread, tint } );
    };
    MeshBuilder &metal = kit.m_metal;
    switch ( group )
    {
    case 1:   // a street's lantern on a bracket
    {
        metal.m_color = { 38, 36, 34, 255 };
        Vector3 arm0 = Vector3Add( wall, Vector3Scale( up, 0.3f ) ),
                arm1 = Vector3Add( at, Vector3Scale( n, 0.18f + reach ) );
        arm1.y = arm0.y;
        rod( metal, arm0, arm1, 0.014f );
        rod( metal, Vector3Subtract( arm0, Vector3Scale( up, 0.22f ) ), Vector3Lerp( arm0, arm1, 0.6f ), 0.01f );
        metal.box( place( Vector3Subtract( arm0, Vector3Scale( n, 0.01f ) ) ), flat( 0.05f, 0.09f, 0.015f ) );
        Vector3 body = Vector3Subtract( arm1, Vector3Scale( up, 0.2f ) );
        rod( metal, arm1, Vector3Add( body, Vector3Scale( up, 0.14f ) ), 0.008f );
        // Its roof, its foot, four posts round the glass.
        metal.lathe( place( Vector3Add( body, Vector3Scale( up, 0.11f ) ), { 0.0f, PI * 0.25f, 0.0f } ),
                     { { 0.0f, 0.0f }, { 0.11f, 0.0f }, { 0.11f, 0.01f }, { 0.0f, 0.07f } }, 4 );
        metal.box( place( Vector3Subtract( body, Vector3Scale( up, 0.115f ) ) ), { 0.075f, 0.012f, 0.075f } );
        for ( int k = 0; k < 4; ++k )
        {
            Vector3 post = Vector3Add( body, { ( k & 1 ) ? 0.065f : -0.065f, 0.0f, ( k & 2 ) ? 0.065f : -0.065f } );
            metal.box( place( post ), { 0.008f, 0.105f, 0.008f } );
        }
        kit.m_glows.push_back( { body, { 0.066f, 0.11f, 0.066f }, glow } );
        addCone( Vector3Subtract( body, Vector3Scale( up, 0.1f ) ), { 0.0f, -1.0f, 0.0f }, 0.1f, 0.55f, 160 );
        break;
    }
    case 2:   // a clean light bar under a hood
    case 3:
    {
        metal.m_color = group == 3 ? Color{ 196, 208, 222, 255 } : Color{ 168, 172, 178, 255 };
        Vector3 hood = Vector3Add( wall, Vector3Add( Vector3Scale( n, 0.12f + reach ), Vector3Scale( up, 0.12f ) ) );
        metal.box( place( hood ), flat( 0.42f, 0.03f, 0.12f ) );
        for ( float s : { -0.3f, 0.3f } )
            if ( reach > 0.0f )
            {
                Vector3 side = alongX ? Vector3{ s, 0.0f, 0.0f } : Vector3{ 0.0f, 0.0f, s };
                rod( metal, Vector3Add( Vector3Add( wall, Vector3Scale( up, 0.12f ) ), side ),
                     Vector3Add( Vector3Subtract( hood, Vector3Scale( n, 0.1f ) ), side ), 0.012f );
            }
        metal.box( place( Vector3Add( wall, Vector3Scale( up, 0.12f ) ) ), flat( 0.42f, 0.06f, 0.012f ) );
        for ( float s : { -1.0f, 1.0f } )
        {
            Vector3 cap =
                Vector3Add( hood, alongX ? Vector3{ s * 0.41f, -0.04f, 0.0f } : Vector3{ 0.0f, -0.04f, s * 0.41f } );
            metal.box( place( cap ), flat( 0.012f, 0.04f, 0.13f ) );
        }
        // The strip of light under it, a little further out than the hood (its edge glows towards the camera).
        Vector3 strip = Vector3Add( Vector3Subtract( hood, Vector3Scale( up, 0.05f ) ), Vector3Scale( n, 0.02f ) );
        kit.m_glows.push_back( { strip, flat( 0.38f, 0.03f, 0.13f ), glow } );
        addCone( Vector3Subtract( strip, Vector3Scale( up, 0.02f ) ),
                 Vector3Add( Vector3Scale( n, 0.22f ), { 0.0f, -1.0f, 0.0f } ), 0.26f, 0.38f, 200 );
        break;
    }
    case 5:   // a cluster of crystal glowing in the black
    {
        kit.m_glows.push_back( { Vector3Add( at, Vector3Scale( up, -0.05f ) ), flat( 0.05f, 0.22f, 0.05f ), glow } );
        kit.m_glows.push_back( { Vector3Add( Vector3Add( at, Vector3Scale( n, 0.06f ) ), flat( 0.09f, -0.12f, 0.0f ) ),
                                 flat( 0.035f, 0.13f, 0.035f ), glow } );
        kit.m_glows.push_back( { Vector3Add( Vector3Add( at, Vector3Scale( n, 0.04f ) ), flat( -0.08f, -0.16f, 0.0f ) ),
                                 flat( 0.03f, 0.09f, 0.03f ), glow } );
        break;
    }
    default:   // a caged bulb on an arm under a small cap, seen from every side (and from above round its cap)
    {
        bool sooty = t == ThemeE::FURNACE || t == ThemeE::STATION || t == ThemeE::PRISON;
        metal.m_color = sooty ? Color{ 44, 40, 38, 255 } : Color{ 66, 72, 66, 255 };
        Vector3 arm = Vector3Add( wall, Vector3Scale( up, 0.06f ) ),
                bulb = Vector3Add( at, Vector3Scale( n, 0.13f + reach ) );
        bulb.y = at.y - 0.06f;
        Vector3 hub = { bulb.x, arm.y, bulb.z };
        metal.box( place( Vector3Subtract( arm, Vector3Scale( n, 0.005f ) ) ), flat( 0.07f, 0.1f, 0.015f ) );
        rod( metal, arm, hub, 0.018f );
        rod( metal, Vector3Subtract( arm, Vector3Scale( up, 0.14f ) ), Vector3Lerp( arm, hub, 0.6f ), 0.011f );
        // The cap, the cage's posts and its foot ring round the glowing bulb.
        metal.cylinder( place( Vector3Add( bulb, Vector3Scale( up, 0.1f ) ) ), 0.06f, 0.04f, 12 );
        for ( int k = 0; k < 4; ++k )
        {
            float a = k * PI * 0.5f + PI * 0.25f;
            metal.box( place( Vector3Add( bulb, { cosf( a ) * 0.085f, 0.0f, sinf( a ) * 0.085f } ) ),
                       { 0.007f, 0.1f, 0.007f } );
        }
        metal.box( place( Vector3Subtract( bulb, Vector3Scale( up, 0.1f ) ) ), { 0.07f, 0.008f, 0.07f } );
        kit.m_glows.push_back( { bulb, { 0.078f, 0.1f, 0.078f }, glow } );
        addCone( Vector3Subtract( bulb, Vector3Scale( up, 0.09f ) ),
                 Vector3Add( Vector3Scale( n, 0.25f ), { 0.0f, -1.0f, 0.0f } ), 0.12f, 0.5f, 255 );
        break;
    }
    }
    metal.m_color = WHITE;
    return true;
}

void Level::buildCones()
{
    m_cones.clear();
    if ( m_coneSpecs.empty() ) return;
    MeshBuilder b;
    const int   Sides = 14;
    for ( const Cone &k : m_coneSpecs )
    {
        Vector3 z = k.m_axis;
        Vector3 x = fabsf( z.y ) > 0.99f ? Vector3{ 1.0f, 0.0f, 0.0f }
                                         : Vector3Normalize( Vector3CrossProduct( { 0.0f, 1.0f, 0.0f }, z ) );
        Vector3 y = Vector3CrossProduct( z, x );
        float   r0 = k.m_radius, r1 = k.m_radius + k.m_spread * k.m_length;
        float   slope = ( r1 - r0 ) / k.m_length;
        b.m_color = k.m_colour;
        for ( int s = 0; s < Sides; ++s )
        {
            Vector3 corner[4], normal[4];
            for ( int e = 0; e < 2; ++e )
            {
                float   a = ( s + e ) * 2.0f * PI / Sides;
                Vector3 radial = Vector3Add( Vector3Scale( x, cosf( a ) ), Vector3Scale( y, sinf( a ) ) );
                Vector3 nrm = Vector3Normalize( Vector3Subtract( radial, Vector3Scale( z, slope ) ) );
                corner[e] = Vector3Add( k.m_apex, Vector3Scale( radial, r0 ) );
                corner[3 - e] =
                    Vector3Add( Vector3Add( k.m_apex, Vector3Scale( z, k.m_length ) ), Vector3Scale( radial, r1 ) );
                normal[e] = normal[3 - e] = nrm;
            }
            // (card's v: 0 along a to b at the lamp, 1 along d to c at the cone's end.)
            b.card( corner, normal );
        }
    }
    for ( Mesh mesh : b.build( 16.0f * CellSize ) )
    {
        Drawable d;
        d.m_mesh = mesh;
        d.m_transform = MatrixIdentity();
        d.m_sunShadow = d.m_spotShadow = false;
        m_cones.push_back( d );
    }
    m_coneSpecs.clear();
}

}   // namespace spaceopera
