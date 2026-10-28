/**
 * @file    LevelDress.cpp
 * @brief   The level's dressing after its walls and floors: decals, clutter by the walls, show pieces, their lights.
 * @author  Rene Weller
 */

#include "Level.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace spaceopera
{

namespace
{

const int DR[4] = { -1, 0, 1, 0 };   ///< north, east, south, west (as Level.cpp)
const int DC[4] = { 0, 1, 0, -1 };

/** @brief What kind of place a theme is, for its clutter and show pieces. */
enum DressE
{
    INDUSTRIAL,
    STREET,
    CLEAN,
    COLD,
    OUTDOOR,
    ALIEN,
    GARDEN
};

int dressOf( ThemeE t )
{
    switch ( t )
    {
    case ThemeE::STREET:
    case ThemeE::BAR: return STREET;
    case ThemeE::BRIDGE:
    case ThemeE::VAULT:
    case ThemeE::PODS:
    case ThemeE::CLINIC:
    case ThemeE::SERVER:
    case ThemeE::HALL:
    case ThemeE::NET: return CLEAN;
    case ThemeE::COLDSTORE:
    case ThemeE::ICE:
    case ThemeE::CRYO: return COLD;
    case ThemeE::DUSK:
    case ThemeE::ARENA:
    case ThemeE::MOON:
    case ThemeE::SNOW: return OUTDOOR;
    case ThemeE::ALIEN: return ALIEN;
    case ThemeE::PARK: return GARDEN;
    default: return INDUSTRIAL;
    }
}

constexpr float FloorShare = 0.3f;     ///< floor cells with a decal
constexpr float WallShare = 0.45f;     ///< free wall faces with a decal
constexpr float ClutterShare = 0.2f;   ///< wall-side cells of a wide room with clutter
constexpr float BreakShare = 0.16f;    ///< wall-side cells of a wide room with things that break

}   // namespace

void Level::dress( const LevelSurfaces &s, DressKit &kit )
{
    m_decals.clear();
    auto open = [&]( int r, int c ) { return cell( r, c ).m_kind != CellE::ROCK; };
    // Spawns, doors, people, things and their cells stay clear.
    auto busy = [&]( int r, int c, int reach )
    {
        for ( const Marker &m : m_markers )
            if ( m.m_kind != 'R' && m.m_kind != 'A' && abs( m.m_row - r ) <= reach && abs( m.m_col - c ) <= reach )
                return true;
        for ( const Npc &n : m_npcs )
            if ( abs( n.m_row - r ) <= reach + 1 && abs( n.m_col - c ) <= reach + 1 ) return true;
        for ( const Thing &t : m_things )
            if ( abs( t.m_row - r ) <= reach && abs( t.m_col - c ) <= reach ) return true;
        for ( const Native &n : m_natives )
            if ( abs( n.m_row - r ) <= reach && abs( n.m_col - c ) <= reach ) return true;
        for ( const auto &e : m_elites )
            if ( abs( e.first / 1000 - r ) <= reach && abs( e.first % 1000 - c ) <= reach ) return true;
        return false;
    };
    auto nearProp = [&]( Vector3 p, float reach )
    {
        for ( const Prop &q : m_props )
            if ( Vector2Distance( { p.x, p.z }, { q.m_pos.x, q.m_pos.z } ) < reach + q.m_radius ) return true;
        for ( const Seat &q : m_seats )
            if ( Vector2Distance( { p.x, p.z }, { q.m_pos.x, q.m_pos.z } ) < reach + 0.5f ) return true;
        return false;
    };
    auto nearVoid = [&]( int r, int c )
    {
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
                if ( cell( r + dr, c + dc ).m_kind == CellE::VOID ) return true;
        return false;
    };

    // Decals: stains, cracks, markings on the floors; posters, streaks, neon on the walls (as each place has them).
    if ( s.m_decals && !s.m_decals->empty() )
    {
        const std::vector<DecalDef> &defs = *s.m_decals;
        std::vector<int>             floors[64], walls[64];
        for ( size_t i = 0; i < defs.size(); ++i )
            for ( int t = 0; t < (int)ThemeE::COUNT; ++t )
                if ( ( defs[i].m_themes >> t ) & 1u ) ( defs[i].m_wall ? walls : floors )[t].push_back( (int)i );
        for ( int r = 0; r < m_rows; ++r )
            for ( int c = 0; c < m_cols; ++c )
            {
                const Cell &k = cell( r, c );
                int         t = (int)theme( r, c );
                if ( k.m_kind != CellE::FLOOR || floors[t].empty() || hash01( r, c, 201 ) > FloorShare ) continue;
                // It may reach into the neighbours where they are the same floor (under a wall it is hidden).
                bool even = true;
                for ( int d = 0; d < 4; ++d )
                {
                    const Cell &n = cell( r + DR[d], c + DC[d] );
                    if ( n.m_kind == CellE::ROCK ) continue;
                    if ( ( n.m_kind != CellE::FLOOR && n.m_kind != CellE::DOOR ) || n.m_height != k.m_height )
                        even = false;
                }
                const DecalDef &d = defs[floors[t][hashCell( r, c, 202 ) % floors[t].size()]];
                float           scale = 0.8f + 0.45f * hash01( r, c, 203 );
                float           w = d.m_width * scale, h = d.m_height * scale;
                float fit = std::min( 1.0f, ( even ? CellSize * 1.4f : CellSize * 0.95f ) / std::max( w, h ) );
                w *= fit;
                h *= fit;
                float   yaw = hash01( r, c, 204 ) * 2.0f * PI;
                float   slack = even ? 0.6f : std::max( 0.0f, CellSize - std::max( w, h ) ) * 0.8f;
                Vector3 mid = { ( c + 0.5f ) * CellSize + ( hash01( r, c, 205 ) - 0.5f ) * slack,
                                k.m_height + 0.012f + 0.002f * ( hashCell( r, c, 206 ) % 4 ),
                                ( r + 0.5f ) * CellSize + ( hash01( r, c, 207 ) - 0.5f ) * slack };
                Vector3 right = { cosf( yaw ) * w * 0.5f, 0.0f, sinf( yaw ) * w * 0.5f };
                Vector3 down = { -sinf( yaw ) * h * 0.5f, 0.0f, cosf( yaw ) * h * 0.5f };
                Decal   decal;
                decal.m_def = (int)( &d - defs.data() );
                decal.m_corner[0] = Vector3Subtract( Vector3Subtract( mid, right ), down );
                decal.m_corner[1] = Vector3Subtract( Vector3Add( mid, right ), down );
                decal.m_corner[2] = Vector3Add( Vector3Add( mid, right ), down );
                decal.m_corner[3] = Vector3Add( Vector3Subtract( mid, right ), down );
                decal.m_cut = false;
                m_decals.push_back( decal );
                if ( d.m_glow >= 1.5f && hashCell( r, c, 208 ) % 2 == 0 )
                    m_lights.push_back( { Vector3Add( mid, { 0.0f, 0.6f, 0.0f } ),
                                          Vector3Scale( d.m_color, d.m_glow * 1.6f ), 3.0f, 0.0f } );
            }
        for ( const FaceSlot &f : m_faces )
        {
            int t = (int)theme( f.m_r, f.m_c );
            // Only faces the camera sees (the ones looking away are cut away when they are in the way).
            if ( walls[t].empty() || DR[f.m_d] > 0 || hash01( f.m_r, f.m_c, 211 + f.m_d ) > WallShare ) continue;
            Vector3 n = { (float)-DC[f.m_d], 0.0f, (float)-DR[f.m_d] };
            Vector3 face = { ( f.m_c + 0.5f + DC[f.m_d] * 0.5f ) * CellSize, 0.0f,
                             ( f.m_r + 0.5f + DR[f.m_d] * 0.5f ) * CellSize };
            face = Vector3Add( face, Vector3Scale( n, f.m_front ) );
            bool byASign = false;
            for ( const Sign &sg : m_signs )
            {
                Vector3 sm = Vector3Scale( Vector3Add( sg.m_corner[0], sg.m_corner[2] ), 0.5f );
                if ( Vector2Distance( { sm.x, sm.z }, { face.x, face.z } ) < 1.8f ) byASign = true;
            }
            if ( byASign ) continue;
            const DecalDef &d = defs[walls[t][hashCell( f.m_r, f.m_c, 212 + f.m_d ) % walls[t].size()]];
            float           scale = 0.85f + 0.3f * hash01( f.m_r, f.m_c, 213 + f.m_d );
            float           w = std::min( d.m_width * scale, 1.7f ), h = d.m_height * w / d.m_width;
            Vector3         right = { n.z, 0.0f, -n.x };
            float           slide = ( hash01( f.m_r, f.m_c, 214 + f.m_d ) - 0.5f ) * std::max( 0.0f, 1.8f - w );
            float           y = f.m_base + d.m_y;
            y = std::min( y, f.m_base + m_wallHeight - 0.15f - h * 0.5f );
            y = std::max( y, f.m_base + 0.05f + h * 0.5f );
            Vector3 mid = Vector3Add( Vector3Add( face, Vector3Scale( right, slide ) ), { 0.0f, y, 0.0f } );
            Vector3 hr = Vector3Scale( right, w * 0.5f ), hu = { 0.0f, h * 0.5f, 0.0f };
            Decal   decal;
            decal.m_def = (int)( &d - defs.data() );
            decal.m_corner[0] = Vector3Add( Vector3Subtract( mid, hr ), hu );
            decal.m_corner[1] = Vector3Add( Vector3Add( mid, hr ), hu );
            decal.m_corner[2] = Vector3Subtract( Vector3Add( mid, hr ), hu );
            decal.m_corner[3] = Vector3Subtract( Vector3Subtract( mid, hr ), hu );
            decal.m_cut = true;
            m_decals.push_back( decal );
            if ( d.m_glow > 0.0f )
                m_lights.push_back( { Vector3Add( mid, Vector3Scale( n, 0.7f ) ),
                                      Vector3Scale( d.m_color, d.m_glow * 2.2f ), 3.5f, 0.0f } );
        }
    }
    m_faces.clear();

    // Clutter by the walls of the wider rooms (corridors stay clear), not where something stands already.
    int clutters = 0;
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            const Cell &k = cell( r, c );
            if ( k.m_kind != CellE::FLOOR || k.m_height != 0.0f || k.m_trigger || nearVoid( r, c ) || busy( r, c, 1 ) ||
                 hash01( r, c, 221 ) > ClutterShare )
                continue;
            int wall = -1, walk = 0;
            for ( int d = 0; d < 4; ++d )
            {
                if ( !open( r + DR[d], c + DC[d] ) && wall < 0 ) wall = d;
                if ( walkable( r + DR[d], c + DC[d] ) ) ++walk;
            }
            if ( wall < 0 || walk < 3 ) continue;
            if ( !walkable( r - DR[wall], c - DC[wall] ) || !walkable( r - 2 * DR[wall], c - 2 * DC[wall] ) ) continue;
            Vector3 n = { (float)-DC[wall], 0.0f, (float)-DR[wall] };
            Vector3 at = { ( c + 0.5f ) * CellSize - n.x * 0.45f, 0.0f, ( r + 0.5f ) * CellSize - n.z * 0.45f };
            if ( nearProp( at, 1.3f ) ) continue;
            clutter( dressOf( theme( r, c ) ), hashCell( r, c, 222 ), at, atan2f( n.x, n.z ), kit );
            ++clutters;
        }

    // Flat things in the floor, to walk over: hatches with their handles, cable bundles across the floor, guidance
    // lights along the corridors, vents, rime, pebbles, glowing nodes (as each place has them). A cell with a decal
    // keeps it alone.
    for ( int r = 1; r + 1 < m_rows; ++r )
        for ( int c = 1; c + 1 < m_cols; ++c )
        {
            const Cell &k = cell( r, c );
            if ( k.m_kind != CellE::FLOOR || k.m_trigger || hash01( r, c, 201 ) <= FloorShare || nearVoid( r, c ) )
                continue;
            int      group = dressOf( theme( r, c ) );
            unsigned h = hashCell( r, c, 241 );
            float    roll = ( h & 1023u ) / 1023.0f, y = k.m_height;
            Vector3  mid = { ( c + 0.5f ) * CellSize, y, ( r + 0.5f ) * CellSize };
            // A corridor: walls on two opposite sides.
            int  wallSide = -1;
            bool corridor = false;
            for ( int d = 0; d < 2; ++d )
                if ( !open( r + DR[d], c + DC[d] ) && !open( r + DR[d + 2], c + DC[d + 2] ) )
                    corridor = true, wallSide = d;
            auto glowAt = [&]( Vector3 p, Vector3 half, Vector4 colour )
            { kit.m_glows.push_back( { p, half, colour } ); };
            if ( corridor && ( r + c ) % 3 == 0 && ( group == INDUSTRIAL || group == CLEAN ) )
            {
                // Guidance lights at the foot of both walls.
                Vector4 lit = group == CLEAN ? Vector4{ 1.4f, 1.8f, 2.4f, 0.0f } : Vector4{ 2.6f, 1.4f, 0.3f, 0.0f };
                for ( int side : { wallSide, wallSide + 2 } )
                {
                    Vector3 p = { mid.x + DC[side] * ( CellSize * 0.5f - 0.18f ), y + 0.02f,
                                  mid.z + DR[side] * ( CellSize * 0.5f - 0.18f ) };
                    kit.m_metal.m_color = { 50, 52, 56, 255 };
                    kit.m_metal.box( place( { p.x, y + 0.01f, p.z } ), { 0.1f, 0.01f, 0.1f } );
                    glowAt( p, { DC[side] ? 0.02f : 0.07f, 0.008f, DR[side] ? 0.02f : 0.07f }, lit );
                }
                continue;
            }
            switch ( group )
            {
            case INDUSTRIAL:
            case STREET:
                if ( roll < 0.05f )
                {
                    // A hatch: its plate, its rim, a handle.
                    kit.m_metal.m_color = { 96, 100, 104, 255 };
                    kit.m_metal.box( place( { mid.x, y + 0.012f, mid.z } ), { 0.42f, 0.012f, 0.42f } );
                    kit.m_metal.m_color = { 50, 52, 56, 255 };
                    for ( int d = 0; d < 4; ++d )
                        kit.m_metal.box( place( { mid.x + DC[d] * 0.43f, y + 0.02f, mid.z + DR[d] * 0.43f } ),
                                         { DC[d] ? 0.02f : 0.45f, 0.02f, DR[d] ? 0.02f : 0.45f } );
                    kit.m_metal.m_color = { 200, 150, 40, 255 };
                    kit.m_metal.box( place( { mid.x + 0.25f, y + 0.035f, mid.z } ), { 0.02f, 0.012f, 0.1f } );
                }
                else if ( roll < 0.09f )
                {
                    // Cables across the floor, from wall to wall or across a room.
                    bool    alongX = ( h >> 10 ) & 1;
                    Matrix  axis = alongX ? MatrixRotateZ( -PI * 0.5f ) : MatrixRotateX( PI * 0.5f );
                    Vector3 start = { alongX ? mid.x - CellSize * 0.5f : mid.x, y,
                                      alongX ? mid.z : mid.z - CellSize * 0.5f };
                    for ( int i = 0; i < 3; ++i )
                    {
                        kit.m_rust.m_color = i == 1 ? Color{ 150, 45, 35, 255 } : Color{ 45, 47, 50, 255 };
                        Vector3 o = { alongX ? 0.0f : ( i - 1 ) * 0.07f, 0.03f, alongX ? ( i - 1 ) * 0.07f : 0.0f };
                        kit.m_rust.cylinder(
                            MatrixMultiply( axis, MatrixTranslate( start.x + o.x, start.y + o.y, start.z + o.z ) ),
                            0.03f, CellSize, 6 );
                    }
                }
                break;
            case CLEAN:
                if ( roll < 0.05f )
                {
                    // A floor vent: a frame, its slats.
                    kit.m_metal.m_color = { 170, 176, 182, 255 };
                    kit.m_metal.box( place( { mid.x, y + 0.008f, mid.z } ), { 0.36f, 0.008f, 0.22f } );
                    kit.m_metal.m_color = { 60, 64, 70, 255 };
                    for ( float z = -0.15f; z < 0.16f; z += 0.06f )
                        kit.m_metal.box( place( { mid.x, y + 0.018f, mid.z + z } ), { 0.32f, 0.006f, 0.015f } );
                }
                break;
            case COLD:
                if ( roll < 0.08f )
                {
                    kit.m_metal.m_color = { 236, 242, 248, 255 };
                    kit.m_metal.box( place( { mid.x + ( ( h >> 8 ) % 5 - 2 ) * 0.15f, y + 0.01f, mid.z },
                                            { 0.0f, ( h >> 12 ) % 6 * 0.5f, 0.0f } ),
                                     { 0.4f, 0.01f, 0.25f } );
                }
                break;
            case OUTDOOR:
                if ( roll < 0.12f )
                    for ( int i = 0; i < 3; ++i )
                    {
                        kit.m_rock.m_color = { 110, 98, 88, 255 };
                        unsigned g = hashCell( r, c * 3 + i, 242 );
                        kit.m_rock.box(
                            place( { mid.x + ( g % 9 - 4 ) * 0.18f, y + 0.05f, mid.z + ( ( g >> 4 ) % 9 - 4 ) * 0.18f },
                                   { 0.0f, ( g >> 8 ) % 6 * 0.5f, 0.0f } ),
                            { 0.08f + ( g >> 12 ) % 3 * 0.03f, 0.05f, 0.07f } );
                    }
                break;
            case ALIEN:
                if ( roll < 0.06f )
                    glowAt( { mid.x, y + 0.02f, mid.z }, { 0.05f, 0.02f, 0.05f }, { 1.6f, 0.6f, 2.8f, 0.0f } );
                break;
            default: break;
            }
        }
    for ( MeshBuilder *b : { &kit.m_metal, &kit.m_rust, &kit.m_rock } ) b->m_color = WHITE;

    // Things that break, in ones, twos and threes by the walls of the wider rooms (where clutter may stand: never in a
    // corridor). The game makes them breakable; here they are only places and circles that block.
    m_breaks.clear();
    for ( int r = 1; r + 1 < m_rows; ++r )
        for ( int c = 1; c + 1 < m_cols; ++c )
        {
            const Cell &k = cell( r, c );
            if ( k.m_kind != CellE::FLOOR || k.m_height != 0.0f || k.m_trigger || nearVoid( r, c ) || busy( r, c, 1 ) ||
                 hash01( r, c, 231 ) > BreakShare )
                continue;
            int wall = -1, walk = 0;
            for ( int d = 0; d < 4; ++d )
            {
                if ( !open( r + DR[d], c + DC[d] ) && wall < 0 ) wall = d;
                if ( walkable( r + DR[d], c + DC[d] ) ) ++walk;
            }
            if ( wall < 0 || walk < 3 ) continue;
            if ( !walkable( r - DR[wall], c - DC[wall] ) || !walkable( r - 2 * DR[wall], c - 2 * DC[wall] ) ) continue;
            Vector3 n = { (float)-DC[wall], 0.0f, (float)-DR[wall] }, along = { n.z, 0.0f, -n.x };
            Vector3 at = { ( c + 0.5f ) * CellSize - n.x * 0.35f, 0.0f, ( r + 0.5f ) * CellSize - n.z * 0.35f };
            if ( nearProp( at, 1.2f ) ) continue;
            using B = BreakE;
            static const BreakE Kinds[7][6] = {
                { B::CRATE, B::DRUM, B::CANISTERS, B::SPOOL, B::LOCKER, B::BATTERY },   // industrial
                { B::CRATE, B::DRUM, B::CASE, B::SACKS, B::CRATE, B::SACKS },           // street
                { B::CASE, B::TANK, B::CONSOLE, B::VIALS, B::POT, B::LOCKER },          // clean
                { B::CRATE, B::TANK, B::CASE, B::VIALS, B::BATTERY, B::LOCKER },        // cold
                { B::CRATE, B::DRUM, B::CASE, B::BATTERY, B::SACKS, B::DRUM },          // outdoor
                { B::CRYSTAL, B::CRYSTAL, B::CRYSTAL, B::CRYSTAL, B::CRYSTAL, B::CRYSTAL },
                { B::POT, B::CASE, B::POT, B::SACKS, B::CRATE, B::POT }
            };   // garden
            const float Radius[(int)BreakE::COUNT] = { 0.42f, 0.32f, 0.3f,  0.4f,  0.3f, 0.42f, 0.36f,
                                                       0.38f, 0.3f,  0.36f, 0.42f, 0.3f, 0.3f };
            unsigned    h = hashCell( r, c, 232 );
            int         count = 1 + (int)( h % 3 );
            for ( int i = 0; i < count; ++i )
            {
                BreakE  kind = Kinds[dressOf( theme( r, c ) )][( h >> ( 4 + i * 3 ) ) % 6];
                float   off = ( i - ( count - 1 ) * 0.5f ) * 0.78f + ( ( ( h >> ( 12 + i * 2 ) ) & 3 ) - 1.5f ) * 0.04f;
                Vector3 p = Vector3Add( at, Vector3Scale( along, off ) );
                m_breaks.push_back(
                    { p, atan2f( n.x, n.z ) + ( ( h >> ( 20 + i ) ) & 1 ) * 0.4f - 0.2f, kind, (int)m_props.size() } );
                m_props.push_back( { p, Radius[(int)kind] } );
            }
        }

    // A show piece in the middle of the larger rooms: a block of 3 x 3 open floor with nothing near it.
    int floorCells = 0;
    for ( const Cell &k : m_cells )
        if ( k.m_kind == CellE::FLOOR ) ++floorCells;
    int                              want = floorCells < 60 ? 0 : std::clamp( floorCells / 140, 1, 5 );
    std::vector<std::pair<int, int>> spots;
    for ( int r = 1; r + 1 < m_rows; ++r )
        for ( int c = 1; c + 1 < m_cols; ++c )
        {
            // Deep in a room: two cells of open floor all round (no wall, corridor mouth or door near), nothing may
            // close a way.
            bool fine = true;
            for ( int dr = -2; dr <= 2 && fine; ++dr )
                for ( int dc = -2; dc <= 2 && fine; ++dc )
                {
                    const Cell &k = cell( r + dr, c + dc );
                    if ( k.m_kind != CellE::FLOOR || k.m_height != 0.0f ||
                         ( abs( dr ) <= 1 && abs( dc ) <= 1 && k.m_trigger ) )
                        fine = false;
                }
            for ( int dr = -3; dr <= 3 && fine; ++dr )
                for ( int dc = -3; dc <= 3 && fine; ++dc )
                    if ( cell( r + dr, c + dc ).m_kind == CellE::DOOR ) fine = false;
            Vector3 mid = { ( c + 0.5f ) * CellSize, 0.0f, ( r + 0.5f ) * CellSize };
            if ( fine && !busy( r, c, 3 ) && !nearProp( mid, 2.6f ) ) spots.push_back( { r, c } );
        }
    std::sort( spots.begin(), spots.end(), [&]( const auto &a, const auto &b )
               { return hashCell( a.first, a.second, 231 ) < hashCell( b.first, b.second, 231 ); } );
    std::vector<std::pair<int, int>> shown;
    for ( const auto &sp : spots )
    {
        if ( (int)shown.size() >= want ) break;
        bool apart = true;
        for ( const auto &o : shown )
            if ( abs( o.first - sp.first ) + abs( o.second - sp.second ) < 10 ) apart = false;
        if ( !apart ) continue;
        shown.push_back( sp );
        showPiece( dressOf( theme( sp.first, sp.second ) ), hashCell( sp.first, sp.second, 232 ),
                   { ( sp.second + 0.5f ) * CellSize, 0.0f, ( sp.first + 0.5f ) * CellSize }, kit );
        TraceLog( LOG_WARNING, "DRESS: show piece at %.1f,%.1f", ( sp.second + 0.5f ) * CellSize,
                  ( sp.first + 0.5f ) * CellSize );
    }
    TraceLog( LOG_WARNING, "DRESS: %d decals, %d clutter, %d breakables, %d show pieces", (int)m_decals.size(),
              clutters, (int)m_breaks.size(), (int)shown.size() );
}

void Level::clutter( int group, unsigned h, Vector3 at, float yaw, DressKit &kit )
{
    // Its own frame: x along the wall, z out into the room.
    Vector3 F = { sinf( yaw ), 0.0f, cosf( yaw ) }, R = { F.z, 0.0f, -F.x }, U = { 0.0f, 1.0f, 0.0f };
    auto    P = [&]( float x, float y, float z )
    {
        return Vector3Add(
            at, Vector3Add( Vector3Scale( R, x ), Vector3Add( Vector3Scale( U, y ), Vector3Scale( F, z ) ) ) );
    };
    auto M = [&]( float x, float y, float z, float spin = 0.0f )
    {
        Vector3 f = { sinf( yaw + spin ), 0.0f, cosf( yaw + spin ) };
        return frame( { f.z, 0.0f, -f.x }, U, f, P( x, y, z ) );
    };
    auto alongX = [&]( float x, float y, float z ) { return frame( Vector3Negate( U ), R, F, P( x, y, z ) ); };
    auto glow = [&]( float x, float y, float z, Vector3 half, Vector4 colour )
    {
        Vector3 world = { fabsf( R.x ) * half.x + fabsf( F.x ) * half.z, half.y,
                          fabsf( R.z ) * half.x + fabsf( F.z ) * half.z };
        kit.m_glows.push_back( { P( x, y, z ), world, colour } );
    };
    auto light = [&]( float x, float y, float z, Vector3 colour, float radius, float flicker = 0.0f )
    { m_lights.push_back( { P( x, y, z ), colour, radius, flicker } ); };
    auto        block = [&]( float z, float radius ) { m_props.push_back( { P( 0.0f, 0.0f, z ), radius } ); };
    auto        rnd = [&]( int i ) { return ( ( h >> ( i % 24 ) ) & 255u ) / 255.0f; };
    const Color paints[6] = { { 170, 60, 40, 255 }, { 60, 110, 70, 255 },   { 200, 150, 50, 255 },
                              { 70, 90, 130, 255 }, { 150, 150, 150, 255 }, { 120, 70, 110, 255 } };
    unsigned    pick = h >> 4;

    switch ( group )
    {
    case INDUSTRIAL:
        switch ( pick % 12 )
        {
        case 0:   // gas bottles chained to the wall
            for ( int i = 0; i < 3; ++i )
            {
                kit.m_rust.m_color = paints[( pick / 12 + i ) % 6];
                kit.m_rust.cylinder( M( -0.3f + i * 0.3f, 0.0f, 0.0f ), 0.12f, 1.25f, 12 );
                kit.m_metal.cylinder( M( -0.3f + i * 0.3f, 1.25f, 0.0f ), 0.05f, 0.12f, 8 );
            }
            kit.m_metal.box( M( 0.0f, 0.95f, -0.1f ), { 0.48f, 0.015f, 0.015f } );
            block( 0.0f, 0.42f );
            break;
        case 1:   // a cable drum on its side
            for ( float x : { -0.36f, 0.3f } ) kit.m_crate.cylinder( alongX( x, 0.52f, 0.0f ), 0.52f, 0.06f, 16 );
            kit.m_cloth.m_color = { 30, 30, 32, 255 };
            kit.m_cloth.cylinder( alongX( -0.3f, 0.52f, 0.0f ), 0.4f, 0.6f, 14 );
            kit.m_cloth.m_color = { 200, 90, 30, 255 };
            kit.m_cloth.cylinder( alongX( -0.3f, 0.52f, 0.0f ), 0.43f, 0.12f, 14 );
            block( 0.0f, 0.55f );
            break;
        case 2:   // a pallet of sacks
            kit.m_crate.m_color = { 150, 120, 85, 255 };
            kit.m_crate.box( M( 0.0f, 0.07f, 0.0f ), { 0.6f, 0.07f, 0.5f } );
            kit.m_cloth.m_color = { 160, 145, 115, 255 };
            for ( int i = 0; i < 5; ++i )
                kit.m_cloth.roundedBox( M( i < 4 ? -0.3f + ( i % 2 ) * 0.6f : 0.0f, i < 4 ? 0.3f : 0.6f,
                                           i < 4 ? -0.22f + ( i / 2 ) * 0.44f : 0.0f, ( rnd( i * 3 ) - 0.5f ) * 0.4f ),
                                        { 0.28f, 0.15f, 0.2f }, 0.1f, 3 );
            block( 0.0f, 0.6f );
            break;
        case 3:   // barrels, one fallen
            for ( int i = 0; i < 2; ++i )
            {
                kit.m_rust.m_color = paints[( pick / 12 + i * 2 ) % 6];
                kit.m_rust.cylinder( M( -0.32f + i * 0.6f, 0.0f, -0.1f ), 0.28f, 0.9f, 14 );
                kit.m_metal.cylinder( M( -0.32f + i * 0.6f, 0.9f, -0.1f ), 0.26f, 0.02f, 14 );
            }
            kit.m_rust.m_color = paints[( pick / 12 + 5 ) % 6];
            kit.m_rust.cylinder( alongX( -0.45f, 0.28f, 0.5f ), 0.28f, 0.9f, 14 );
            block( 0.1f, 0.62f );
            break;
        case 4:   // a fuse cabinet with its lamps
            kit.m_metal.m_color = { 120, 124, 128, 255 };
            kit.m_metal.box( M( 0.0f, 1.3f, -0.48f ), { 0.32f, 0.55f, 0.12f } );
            kit.m_hazard.box( M( 0.0f, 0.68f, -0.47f ), { 0.32f, 0.04f, 0.11f } );
            kit.m_rust.cylinder( M( 0.15f, 1.85f, -0.5f ), 0.05f, 2.4f, 8 );
            glow( -0.18f, 1.62f, -0.35f, { 0.035f, 0.035f, 0.01f }, { 0.4f, 4.0f, 0.6f, 0.0f } );
            glow( -0.08f, 1.62f, -0.35f, { 0.035f, 0.035f, 0.01f },
                  pick % 3 ? Vector4{ 0.4f, 4.0f, 0.6f, 0.0f } : Vector4{ 5.0f, 0.4f, 0.2f, 0.0f } );
            light( 0.0f, 1.6f, 0.1f, { 0.6f, 2.0f, 0.8f }, 2.5f, pick % 5 == 0 ? 0.5f : 0.0f );
            break;
        case 5:   // a workbench with a vice and a toolbox
            kit.m_crate.m_color = { 120, 95, 70, 255 };
            kit.m_crate.box( M( 0.0f, 0.88f, 0.0f ), { 0.75f, 0.04f, 0.38f } );
            for ( float x : { -0.68f, 0.68f } )
                for ( float z : { -0.3f, 0.3f } ) kit.m_metal.box( M( x, 0.43f, z ), { 0.03f, 0.43f, 0.03f } );
            kit.m_metal.box( M( 0.0f, 0.25f, 0.0f ), { 0.7f, 0.02f, 0.34f } );
            kit.m_metal.box( M( 0.5f, 1.0f, 0.2f ), { 0.08f, 0.08f, 0.12f } );
            kit.m_crate.m_color = { 180, 40, 30, 255 };
            kit.m_crate.box( M( -0.35f, 1.0f, -0.05f, 0.2f ), { 0.22f, 0.08f, 0.1f } );
            glow( 0.0f, 1.9f, -0.35f, { 0.4f, 0.02f, 0.05f }, { 6.0f, 5.5f, 4.5f, 0.0f } );
            kit.m_metal.box( M( 0.0f, 1.94f, -0.35f ), { 0.45f, 0.03f, 0.08f } );
            light( 0.0f, 1.6f, 0.2f, { 3.5f, 3.2f, 2.6f }, 3.0f );
            block( 0.0f, 0.62f );
            break;
        case 6:   // a heap of scrap
            for ( int i = 0; i < 9; ++i )
            {
                MeshBuilder &b = i % 3 ? kit.m_rust : kit.m_metal;
                b.box( place( P( ( rnd( i + 2 ) - 0.5f ) * 0.8f, 0.08f + rnd( i + 7 ) * 0.25f,
                                 ( rnd( i + 4 ) - 0.5f ) * 0.6f ),
                              { rnd( i ) - 0.5f, rnd( i + 5 ) * 6.0f, rnd( i + 9 ) - 0.5f } ),
                       { 0.1f + rnd( i + 1 ) * 0.2f, 0.04f + rnd( i + 3 ) * 0.06f, 0.08f + rnd( i + 6 ) * 0.15f } );
            }
            block( 0.0f, 0.55f );
            break;
        case 7:   // crates stacked
            kit.m_crate.box( M( -0.2f, 0.42f, 0.0f, 0.1f ), { 0.45f, 0.42f, 0.42f } );
            kit.m_crate.box( M( 0.35f, 0.3f, 0.05f, -0.2f ), { 0.3f, 0.3f, 0.3f } );
            kit.m_crate.box( M( -0.15f, 1.07f, 0.0f, 0.35f ), { 0.3f, 0.23f, 0.28f } );
            block( 0.0f, 0.6f );
            break;
        case 8:   // a toolbox and jerrycans
            kit.m_crate.m_color = { 190, 40, 30, 255 };
            kit.m_crate.box( M( -0.3f, 0.14f, 0.1f, 0.3f ), { 0.28f, 0.14f, 0.14f } );
            kit.m_rust.m_color = paints[( pick / 12 ) % 6];
            for ( int i = 0; i < 2; ++i )
                kit.m_rust.roundedBox( M( 0.2f + i * 0.28f, 0.24f, -0.2f, 0.1f * i ), { 0.12f, 0.24f, 0.2f }, 0.03f,
                                       2 );
            block( 0.0f, 0.3f );
            break;
        case 9:   // a fan unit with its duct up the wall
            kit.m_metal.m_color = { 140, 140, 136, 255 };
            kit.m_metal.box( M( 0.0f, 0.35f, -0.2f ), { 0.5f, 0.35f, 0.35f } );
            kit.m_grate.box( M( 0.0f, 0.35f, 0.16f ), { 0.38f, 0.28f, 0.02f } );
            kit.m_rust.box( M( 0.0f, 2.0f, -0.42f ), { 0.25f, 1.3f, 0.12f } );
            block( 0.0f, 0.5f );
            break;
        case 10:   // a trolley with crates
            kit.m_metal.box( M( 0.0f, 0.3f, 0.0f ), { 0.55f, 0.03f, 0.35f } );
            kit.m_metal.box( M( -0.55f, 0.65f, 0.0f ), { 0.02f, 0.35f, 0.33f } );
            for ( float x : { -0.45f, 0.45f } )
                for ( float z : { -0.28f, 0.28f } )
                    kit.m_rust.cylinder( frame( Vector3Negate( U ), R, F, P( x - 0.03f, 0.1f, z ) ), 0.1f, 0.06f, 10 );
            kit.m_crate.box( M( -0.15f, 0.55f, 0.0f, 0.1f ), { 0.3f, 0.22f, 0.28f } );
            kit.m_crate.box( M( 0.3f, 0.47f, 0.05f, -0.15f ), { 0.2f, 0.14f, 0.2f } );
            block( 0.0f, 0.55f );
            break;
        default:   // bollards with a chain
            for ( float x : { -0.55f, 0.55f } ) kit.m_hazard.cylinder( M( x, 0.0f, 0.2f ), 0.1f, 0.85f, 10 );
            kit.m_metal.box( M( 0.0f, 0.7f, 0.2f ), { 0.55f, 0.012f, 0.012f } );
            break;
        }
        break;

    case STREET:
        switch ( pick % 10 )
        {
        case 8:   // a noodle stand: counter, awning, a lamp in the steam
            kit.m_crate.m_color = { 120, 60, 40, 255 };
            kit.m_crate.box( M( 0.0f, 0.5f, -0.1f ), { 0.75f, 0.5f, 0.3f } );
            kit.m_metal.box( M( 0.0f, 1.02f, -0.1f ), { 0.78f, 0.02f, 0.33f } );
            for ( float x : { -0.72f, 0.72f } ) kit.m_metal.box( M( x, 1.5f, -0.35f ), { 0.02f, 0.5f, 0.02f } );
            kit.m_cloth.m_color = paints[( pick / 10 ) % 6];
            kit.m_cloth.box( frame( R, Vector3Normalize( { F.x * 0.4f, 1.0f, F.z * 0.4f } ),
                                    Vector3CrossProduct( R, Vector3Normalize( { F.x * 0.4f, 1.0f, F.z * 0.4f } ) ),
                                    P( 0.0f, 2.0f, -0.05f ) ),
                             { 0.85f, 0.015f, 0.45f } );
            kit.m_metal.cylinder( M( -0.35f, 1.04f, -0.1f ), 0.16f, 0.2f, 12 );
            glow( 0.4f, 1.75f, -0.1f, { 0.09f, 0.12f, 0.09f }, { 6.0f, 3.0f, 0.8f, 0.0f } );
            light( 0.4f, 1.6f, 0.3f, { 4.0f, 2.2f, 0.8f }, 3.5f );
            block( -0.1f, 0.65f );
            break;
        case 9:   // a scooter on its stand
            kit.m_hull.m_color = paints[( pick / 10 + 2 ) % 6];
            kit.m_hull.roundedBox( M( 0.0f, 0.35f, 0.0f ), { 0.55f, 0.12f, 0.16f }, 0.06f, 2 );
            kit.m_hull.roundedBox( M( -0.15f, 0.55f, 0.0f ), { 0.22f, 0.06f, 0.14f }, 0.04f, 2 );
            for ( float x : { -0.45f, 0.45f } ) kit.m_rust.cylinder( alongX( x - 0.04f, 0.2f, 0.0f ), 0.2f, 0.08f, 14 );
            kit.m_metal.box( M( 0.45f, 0.75f, 0.0f ), { 0.02f, 0.35f, 0.02f } );
            kit.m_metal.box( M( 0.45f, 1.1f, 0.0f ), { 0.02f, 0.02f, 0.25f } );
            glow( 0.58f, 0.45f, 0.0f, { 0.02f, 0.04f, 0.06f }, { 3.0f, 2.8f, 2.2f, 0.0f } );
            block( 0.0f, 0.45f );
            break;
        case 0:   // a bin and bags
            kit.m_metal.m_color = { 60, 100, 70, 255 };
            kit.m_metal.cylinder( M( -0.35f, 0.0f, -0.1f ), 0.3f, 0.9f, 14 );
            kit.m_cloth.m_color = { 25, 25, 28, 255 };
            for ( int i = 0; i < 3; ++i )
                kit.m_cloth.roundedBox( M( 0.15f + i * 0.25f, 0.2f, 0.05f + ( i % 2 ) * 0.2f, rnd( i ) ),
                                        { 0.2f, 0.2f, 0.18f }, 0.12f, 3 );
            block( 0.0f, 0.55f );
            break;
        case 1:   // a vending machine, lit
        {
            kit.m_hull.m_color = paints[( pick / 8 ) % 6];
            kit.m_hull.box( M( 0.0f, 0.95f, -0.25f ), { 0.45f, 0.95f, 0.32f } );
            Vector4 lit[3] = { { 2.5f, 1.2f, 3.5f, 0.0f }, { 1.0f, 3.0f, 3.5f, 0.0f }, { 3.5f, 2.5f, 1.0f, 0.0f } };
            glow( -0.08f, 1.2f, 0.08f, { 0.3f, 0.6f, 0.01f }, lit[pick % 3] );
            glow( 0.3f, 1.0f, 0.08f, { 0.06f, 0.2f, 0.01f }, { 3.0f, 3.0f, 3.0f, 0.0f } );
            light( 0.0f, 1.2f, 0.7f, { lit[pick % 3].x, lit[pick % 3].y, lit[pick % 3].z }, 3.5f );
            block( -0.1f, 0.55f );
            break;
        }
        case 2:   // a bench
        case 3:
            kit.m_crate.m_color = { 130, 90, 60, 255 };
            for ( int i = 0; i < 3; ++i )
                kit.m_crate.box( M( 0.0f, 0.45f, -0.12f + i * 0.12f ), { 0.8f, 0.025f, 0.05f } );
            kit.m_crate.box( M( 0.0f, 0.75f, -0.24f ), { 0.8f, 0.08f, 0.025f } );
            for ( float x : { -0.7f, 0.7f } ) kit.m_metal.box( M( x, 0.22f, 0.0f ), { 0.03f, 0.22f, 0.18f } );
            block( 0.0f, 0.5f );
            break;
        case 4:   // a planter
            kit.m_crate.m_color = { 110, 100, 95, 255 };
            kit.m_crate.box( M( 0.0f, 0.22f, -0.1f ), { 0.7f, 0.22f, 0.3f } );
            kit.m_cloth.m_color = { 50, 110, 45, 255 };
            for ( int i = 0; i < 5; ++i )
                kit.m_cloth.roundedBox( M( -0.5f + i * 0.25f, 0.5f + rnd( i ) * 0.2f, -0.1f, rnd( i + 3 ) ),
                                        { 0.16f, 0.16f + rnd( i + 1 ) * 0.1f, 0.14f }, 0.1f, 3 );
            block( 0.0f, 0.55f );
            break;
        case 5:   // a fruit stall
            kit.m_crate.box( M( -0.3f, 0.25f, 0.0f, 0.05f ), { 0.3f, 0.25f, 0.28f } );
            kit.m_crate.box( M( 0.35f, 0.2f, 0.05f, -0.1f ), { 0.28f, 0.2f, 0.26f } );
            for ( int i = 0; i < 8; ++i )
            {
                kit.m_cloth.m_color = paints[( i + pick ) % 6];
                kit.m_cloth.roundedBox(
                    M( i < 4 ? -0.3f + ( rnd( i ) - 0.5f ) * 0.3f : 0.35f + ( rnd( i ) - 0.5f ) * 0.3f,
                       i < 4 ? 0.55f : 0.45f, ( rnd( i + 4 ) - 0.5f ) * 0.3f ),
                    { 0.07f, 0.07f, 0.07f }, 0.05f, 2 );
            }
            block( 0.0f, 0.6f );
            break;
        case 6:   // a table with two chairs
            kit.m_metal.cylinder( M( 0.0f, 0.0f, 0.15f ), 0.05f, 0.72f, 8 );
            kit.m_crate.cylinder( M( 0.0f, 0.72f, 0.15f ), 0.42f, 0.04f, 16 );
            for ( float x : { -0.6f, 0.6f } )
            {
                kit.m_metal.box( M( x, 0.45f, 0.15f ), { 0.2f, 0.03f, 0.2f } );
                kit.m_metal.box( M( x + ( x < 0 ? -0.18f : 0.18f ), 0.7f, 0.15f ), { 0.02f, 0.25f, 0.18f } );
                kit.m_metal.box( M( x, 0.22f, 0.15f ), { 0.02f, 0.22f, 0.02f } );
            }
            block( 0.15f, 0.65f );
            break;
        default:   // a small dish on a pole
            kit.m_metal.cylinder( M( 0.0f, 0.0f, -0.3f ), 0.04f, 1.9f, 8 );
            kit.m_metal.lathe( frame( R, Vector3Normalize( { F.x, 1.0f, F.z } ),
                                      Vector3CrossProduct( R, Vector3Normalize( { F.x, 1.0f, F.z } ) ),
                                      P( 0.0f, 1.9f, -0.25f ) ),
                               { { 0.0f, 0.0f }, { 0.25f, 0.08f }, { 0.4f, 0.2f } }, 16 );
            glow( 0.0f, 1.88f, -0.3f, { 0.03f, 0.03f, 0.03f }, { 6.0f, 0.5f, 0.3f, 0.0f } );
            break;
        }
        break;

    case CLEAN:
        switch ( pick % 9 )
        {
        case 6:   // a cleaning robot, its lamp blinking
            kit.m_hull.m_color = { 230, 230, 225, 255 };
            kit.m_hull.roundedBox( M( 0.2f, 0.18f, 0.2f, 0.4f ), { 0.28f, 0.16f, 0.28f }, 0.14f, 3 );
            kit.m_metal.cylinder( M( 0.2f, 0.34f, 0.2f ), 0.08f, 0.06f, 10 );
            glow( 0.2f, 0.43f, 0.2f, { 0.03f, 0.03f, 0.03f }, { 4.0f, 2.5f, 0.3f, 0.0f } );
            light( 0.2f, 0.6f, 0.2f, { 2.0f, 1.2f, 0.2f }, 2.0f, 0.5f );
            block( 0.2f, 0.32f );
            break;
        case 7:   // a row of seats
            for ( int i = 0; i < 3; ++i )
            {
                kit.m_hull.m_color = paints[( pick / 9 + 3 ) % 6];
                kit.m_hull.roundedBox( M( -0.55f + i * 0.55f, 0.45f, 0.0f ), { 0.24f, 0.04f, 0.22f }, 0.03f, 2 );
                kit.m_hull.roundedBox( M( -0.55f + i * 0.55f, 0.75f, -0.2f ), { 0.24f, 0.24f, 0.03f }, 0.03f, 2 );
            }
            kit.m_metal.box( M( 0.0f, 0.38f, 0.0f ), { 0.85f, 0.03f, 0.05f } );
            for ( float x : { -0.7f, 0.7f } ) kit.m_metal.box( M( x, 0.19f, 0.0f ), { 0.03f, 0.19f, 0.18f } );
            block( 0.0f, 0.55f );
            break;
        case 8:   // a slim info pillar
            kit.m_metal.m_color = { 70, 74, 80, 255 };
            kit.m_metal.box( M( 0.0f, 1.0f, -0.1f ), { 0.2f, 1.0f, 0.08f } );
            glow( 0.0f, 1.3f, -0.01f, { 0.16f, 0.5f, 0.01f }, { 0.8f, 1.8f, 2.6f, 0.0f } );
            glow( 0.0f, 0.4f, -0.01f, { 0.16f, 0.03f, 0.01f }, { 2.6f, 1.6f, 0.4f, 0.0f } );
            light( 0.0f, 1.3f, 0.5f, { 0.8f, 1.5f, 2.2f }, 2.8f );
            block( -0.1f, 0.25f );
            break;
        case 0:   // a potted plant
            kit.m_hull.m_color = { 225, 225, 220, 255 };
            kit.m_hull.lathe( M( 0.0f, 0.0f, 0.0f ),
                              { { 0.0f, 0.0f }, { 0.2f, 0.0f }, { 0.28f, 0.5f }, { 0.25f, 0.5f } }, 16 );
            kit.m_cloth.m_color = { 45, 115, 55, 255 };
            for ( int i = 0; i < 6; ++i )
                kit.m_cloth.roundedBox( M( ( rnd( i ) - 0.5f ) * 0.4f, 0.65f + rnd( i + 2 ) * 0.6f,
                                           ( rnd( i + 5 ) - 0.5f ) * 0.4f, rnd( i + 7 ) * 3.0f ),
                                        { 0.18f, 0.12f, 0.1f }, 0.08f, 2 );
            block( 0.0f, 0.35f );
            break;
        case 1:   // a terminal on a column
            kit.m_metal.m_color = { 190, 195, 200, 255 };
            kit.m_metal.box( M( 0.0f, 0.55f, 0.0f ), { 0.16f, 0.55f, 0.12f } );
            kit.m_metal.box( M( 0.0f, 1.2f, 0.05f ), { 0.3f, 0.2f, 0.06f } );
            glow( 0.0f, 1.22f, 0.12f, { 0.25f, 0.15f, 0.01f }, { 0.6f, 2.0f, 3.2f, 0.0f } );
            light( 0.0f, 1.3f, 0.6f, { 0.6f, 1.6f, 2.6f }, 2.8f );
            block( 0.0f, 0.32f );
            break;
        case 2:   // a bench
            kit.m_hull.m_color = { 205, 208, 212, 255 };
            kit.m_hull.roundedBox( M( 0.0f, 0.42f, 0.0f ), { 0.8f, 0.05f, 0.22f }, 0.03f, 2 );
            for ( float x : { -0.65f, 0.65f } ) kit.m_metal.box( M( x, 0.2f, 0.0f ), { 0.04f, 0.2f, 0.18f } );
            block( 0.0f, 0.5f );
            break;
        case 3:   // a care trolley
            kit.m_metal.m_color = { 200, 200, 205, 255 };
            for ( float y : { 0.3f, 0.75f } ) kit.m_metal.box( M( 0.0f, y, 0.0f ), { 0.4f, 0.02f, 0.28f } );
            for ( float x : { -0.38f, 0.38f } )
                for ( float z : { -0.26f, 0.26f } ) kit.m_metal.box( M( x, 0.4f, z ), { 0.015f, 0.4f, 0.015f } );
            kit.m_cloth.m_color = { 235, 235, 235, 255 };
            kit.m_cloth.box( M( -0.15f, 0.84f, 0.0f, 0.2f ), { 0.15f, 0.07f, 0.12f } );
            kit.m_crate.m_color = { 200, 60, 60, 255 };
            kit.m_crate.box( M( 0.2f, 0.4f, 0.0f ), { 0.15f, 0.08f, 0.15f } );
            block( 0.0f, 0.42f );
            break;
        case 4:   // a water dispenser
            kit.m_hull.m_color = { 215, 218, 222, 255 };
            kit.m_hull.box( M( 0.0f, 0.5f, -0.2f ), { 0.2f, 0.5f, 0.2f } );
            glow( 0.0f, 1.25f, -0.2f, { 0.15f, 0.25f, 0.15f }, { 0.3f, 0.9f, 1.6f, 0.0f } );
            block( -0.2f, 0.3f );
            break;
        default:   // a glass case with a relic
            kit.m_hull.m_color = { 60, 60, 66, 255 };
            kit.m_hull.box( M( 0.0f, 0.45f, 0.0f ), { 0.35f, 0.45f, 0.35f } );
            kit.m_metal.m_color = { 210, 170, 90, 255 };
            kit.m_metal.lathe( M( 0.0f, 0.9f, 0.0f ),
                               { { 0.0f, 0.0f }, { 0.12f, 0.0f }, { 0.05f, 0.2f }, { 0.14f, 0.38f }, { 0.0f, 0.42f } },
                               12 );
            glow( 0.0f, 1.25f, 0.0f, { 0.33f, 0.006f, 0.33f }, { 1.5f, 1.4f, 1.2f, 0.0f } );
            light( 0.0f, 1.3f, 0.3f, { 1.4f, 1.3f, 1.0f }, 2.5f );
            block( 0.0f, 0.45f );
            break;
        }
        break;

    case COLD:
        switch ( pick % 7 )
        {
        case 4:   // a heater glowing orange, the only warm thing here
            kit.m_metal.m_color = { 90, 90, 92, 255 };
            kit.m_metal.box( M( 0.0f, 0.45f, -0.15f ), { 0.4f, 0.45f, 0.25f } );
            for ( int i = 0; i < 4; ++i )
                glow( -0.27f + i * 0.18f, 0.5f, 0.11f, { 0.04f, 0.3f, 0.01f }, { 6.0f, 2.0f, 0.3f, 0.0f } );
            kit.m_rust.cylinder( M( 0.3f, 0.9f, -0.3f ), 0.06f, 2.0f, 8 );
            light( 0.0f, 0.6f, 0.6f, { 5.0f, 1.8f, 0.4f }, 3.5f );
            block( -0.1f, 0.48f );
            break;
        case 5:   // a rack of frozen sample tubes
            kit.m_metal.m_color = { 180, 190, 200, 255 };
            kit.m_metal.box( M( 0.0f, 0.95f, -0.25f ), { 0.55f, 0.95f, 0.2f } );
            for ( int y = 0; y < 4; ++y )
                for ( int x = 0; x < 6; ++x )
                    if ( ( h >> ( ( x + y * 6 ) % 20 ) ) & 1u )
                        glow( -0.42f + x * 0.17f, 0.4f + y * 0.42f, -0.04f, { 0.03f, 0.1f, 0.01f },
                              { 0.4f, 1.6f, 2.6f, 0.0f } );
            light( 0.0f, 1.0f, 0.5f, { 0.5f, 1.4f, 2.4f }, 3.0f );
            block( -0.15f, 0.5f );
            break;
        case 6:   // barrels crusted with ice
            for ( int i = 0; i < 3; ++i )
            {
                kit.m_rust.m_color = { 175, 190, 205, 255 };
                kit.m_rust.cylinder( M( -0.35f + i * 0.35f, 0.0f, -0.05f + ( i % 2 ) * 0.25f ), 0.25f, 0.85f, 14 );
                glow( -0.35f + i * 0.35f, 0.87f, -0.05f + ( i % 2 ) * 0.25f, { 0.2f, 0.015f, 0.2f },
                      { 0.6f, 0.7f, 0.8f, 0.0f } );
            }
            block( 0.05f, 0.6f );
            break;
        case 0:   // cryo canisters
            for ( int i = 0; i < 3; ++i )
            {
                kit.m_metal.m_color = { 200, 210, 220, 255 };
                kit.m_metal.cylinder( M( -0.4f + i * 0.4f, 0.0f, -0.1f ), 0.18f, 1.2f, 14 );
                glow( -0.4f + i * 0.4f, 0.8f, 0.09f, { 0.1f, 0.03f, 0.01f }, { 0.4f, 1.8f, 4.0f, 0.0f } );
            }
            light( 0.0f, 1.0f, 0.5f, { 0.6f, 1.5f, 3.0f }, 3.0f );
            block( 0.0f, 0.55f );
            break;
        case 1:   // frosted crates
            kit.m_crate.m_color = { 200, 215, 230, 255 };
            kit.m_crate.box( M( -0.15f, 0.4f, 0.0f, 0.15f ), { 0.45f, 0.4f, 0.4f } );
            kit.m_crate.box( M( 0.1f, 1.0f, 0.0f, -0.1f ), { 0.3f, 0.2f, 0.28f } );
            block( 0.0f, 0.58f );
            break;
        case 2:   // a frozen valve
            kit.m_rust.m_color = { 170, 185, 200, 255 };
            kit.m_rust.cylinder( M( 0.0f, 0.0f, -0.35f ), 0.12f, 1.1f, 12 );
            kit.m_metal.cylinder( frame( U, F, R, P( 0.0f, 0.85f, -0.35f ) ), 0.25f, 0.04f, 16 );
            for ( int i = 0; i < 4; ++i )
                glow( -0.18f + i * 0.12f, 0.6f, -0.2f, { 0.015f, 0.12f + rnd( i ) * 0.1f, 0.015f },
                      { 0.5f, 0.7f, 0.9f, 0.0f } );
            block( -0.2f, 0.32f );
            break;
        default:   // a sledge with a tank
            kit.m_metal.box( M( 0.0f, 0.12f, 0.0f ), { 0.6f, 0.03f, 0.3f } );
            for ( float z : { -0.3f, 0.3f } ) kit.m_metal.box( M( 0.0f, 0.05f, z ), { 0.65f, 0.05f, 0.02f } );
            kit.m_rust.m_color = { 220, 120, 40, 255 };
            kit.m_rust.cylinder( alongX( -0.5f, 0.42f, 0.0f ), 0.27f, 1.0f, 14 );
            block( 0.0f, 0.6f );
            break;
        }
        break;

    case OUTDOOR:
        switch ( pick % 8 )
        {
        case 6:   // a supply crate half sunk, its chute still on it
            kit.m_crate.m_color = { 90, 110, 70, 255 };
            kit.m_crate.box( place( P( 0.0f, 0.25f, 0.0f ), { 0.25f, yaw + 0.4f, 0.15f } ), { 0.5f, 0.35f, 0.4f } );
            kit.m_cloth.m_color = { 200, 120, 60, 255 };
            kit.m_cloth.roundedBox( place( P( 0.45f, 0.1f, 0.35f ), { 0.1f, yaw + 1.0f, 0.0f } ), { 0.5f, 0.08f, 0.4f },
                                    0.07f, 2 );
            block( 0.0f, 0.55f );
            break;
        case 7:   // a marker flag
            kit.m_metal.cylinder( M( 0.0f, 0.0f, 0.0f ), 0.03f, 2.4f, 8 );
            kit.m_cloth.m_color = paints[( pick / 8 ) % 6];
            kit.m_cloth.box( M( 0.3f, 2.15f, 0.0f, 0.15f ), { 0.3f, 0.2f, 0.01f } );
            block( 0.0f, 0.15f );
            break;
        case 0:   // boulders
        case 1:
            for ( int i = 0; i < 3; ++i )
            {
                float size = 0.25f + rnd( i ) * 0.3f;
                kit.m_rock.roundedBox( place( P( -0.4f + i * 0.4f, size * 0.7f, ( rnd( i + 9 ) - 0.5f ) * 0.4f ),
                                              { rnd( i + 3 ) - 0.5f, rnd( i + 6 ) * 6.0f, rnd( i + 1 ) - 0.5f } ),
                                       { size, size * 0.8f, size * 0.9f }, size * 0.4f, 2 );
            }
            block( 0.0f, 0.62f );
            break;
        case 2:   // a sensor mast, blinking
            kit.m_metal.cylinder( M( 0.0f, 0.0f, 0.0f ), 0.04f, 2.6f, 8 );
            kit.m_metal.box( M( 0.0f, 1.2f, 0.06f ), { 0.12f, 0.16f, 0.06f } );
            glow( 0.0f, 2.62f, 0.0f, { 0.05f, 0.05f, 0.05f }, { 8.0f, 0.6f, 0.3f, 0.0f } );
            light( 0.0f, 2.6f, 0.0f, { 4.0f, 0.3f, 0.2f }, 3.0f, 0.6f );
            block( 0.0f, 0.2f );
            break;
        case 3:   // a solar panel on its stand
            kit.m_metal.box( M( 0.0f, 0.4f, 0.0f ), { 0.04f, 0.4f, 0.04f } );
            kit.m_hull.m_color = { 40, 60, 110, 255 };
            kit.m_hull.box( frame( R, Vector3Normalize( { F.x * 0.6f, 1.0f, F.z * 0.6f } ),
                                   Vector3CrossProduct( R, Vector3Normalize( { F.x * 0.6f, 1.0f, F.z * 0.6f } ) ),
                                   P( 0.0f, 0.85f, 0.0f ) ),
                            { 0.6f, 0.02f, 0.4f } );
            block( 0.0f, 0.5f );
            break;
        case 4:   // a dead drone
            kit.m_metal.m_color = { 110, 110, 105, 255 };
            kit.m_metal.roundedBox( M( 0.0f, 0.2f, 0.0f, 0.5f ), { 0.35f, 0.15f, 0.25f }, 0.08f, 2 );
            for ( int i = 0; i < 3; ++i )
                kit.m_rust.box( place( P( ( rnd( i + 1 ) - 0.5f ) * 0.9f, 0.05f, ( rnd( i + 2 ) - 0.5f ) * 0.7f ),
                                       { ( rnd( i ) - 0.5f ) * 0.5f, rnd( i + 4 ) * 6.0f, 0.0f } ),
                                { 0.2f, 0.02f, 0.06f } );
            glow( 0.0f, 0.25f, 0.24f, { 0.05f, 0.05f, 0.02f }, { 0.4f, 0.05f, 0.03f, 0.0f } );
            block( 0.0f, 0.45f );
            break;
        default:   // a cairn of flat stones
            for ( int i = 0; i < 4; ++i )
                kit.m_rock.roundedBox( M( ( rnd( i ) - 0.5f ) * 0.1f, 0.1f + i * 0.2f, 0.0f, rnd( i + 3 ) * 3.0f ),
                                       { 0.3f - i * 0.05f, 0.09f, 0.25f - i * 0.04f }, 0.05f, 2 );
            block( 0.0f, 0.38f );
            break;
        }
        break;

    case ALIEN:
        if ( pick % 5 == 2 )
        {
            // Stones hanging in a ring around a point of light, as if something held them.
            for ( int i = 0; i < 7; ++i )
            {
                float a = i * 0.9f + rnd( i );
                kit.m_rock.roundedBox( place( P( cosf( a ) * 0.55f, 1.0f + sinf( a * 2.0f ) * 0.15f, sinf( a ) * 0.4f ),
                                              { rnd( i + 1 ), a, rnd( i + 2 ) } ),
                                       { 0.11f, 0.08f, 0.09f }, 0.04f, 2 );
            }
            glow( 0.0f, 1.0f, 0.0f, { 0.07f, 0.07f, 0.07f }, { 1.5f, 3.5f, 5.0f, 0.0f } );
            light( 0.0f, 1.0f, 0.3f, { 1.0f, 2.4f, 3.6f }, 3.0f );
            block( 0.0f, 0.3f );
        }
        else if ( pick % 5 == 3 )
        {
            // A low pedestal with a glyph lit on its top.
            kit.m_hull.m_color = { 30, 30, 38, 255 };
            kit.m_hull.box( M( 0.0f, 0.4f, 0.0f, 0.785f ), { 0.3f, 0.4f, 0.3f } );
            glow( 0.0f, 0.81f, 0.0f, { 0.18f, 0.005f, 0.03f }, { 0.5f, 3.0f, 3.6f, 0.0f } );
            glow( 0.0f, 0.81f, 0.0f, { 0.03f, 0.005f, 0.18f }, { 0.5f, 3.0f, 3.6f, 0.0f } );
            light( 0.0f, 1.0f, 0.2f, { 0.4f, 2.0f, 2.4f }, 2.5f );
            block( 0.0f, 0.4f );
        }
        else if ( pick % 5 == 4 )
        {
            // A ribbed pod, a seam of light along it.
            kit.m_hull.m_color = { 36, 32, 44, 255 };
            kit.m_hull.lathe(
                M( 0.0f, 0.0f, 0.0f ),
                { { 0.0f, 0.0f }, { 0.3f, 0.05f }, { 0.42f, 0.4f }, { 0.36f, 0.9f }, { 0.15f, 1.2f }, { 0.0f, 1.25f } },
                10 );
            glow( 0.0f, 0.6f, 0.4f, { 0.015f, 0.45f, 0.015f }, { 3.0f, 0.8f, 4.0f, 0.0f } );
            light( 0.0f, 0.7f, 0.7f, { 1.8f, 0.6f, 2.6f }, 2.5f );
            block( 0.0f, 0.45f );
        }
        else if ( pick % 5 == 0 )
        {
            // A cluster of crystals with glowing tips.
            for ( int i = 0; i < 4; ++i )
            {
                float   tall = 0.5f + rnd( i ) * 0.9f;
                Vector3 lean =
                    Vector3Normalize( { ( rnd( i + 2 ) - 0.5f ) * 0.7f, 1.0f, ( rnd( i + 5 ) - 0.5f ) * 0.7f } );
                Vector3 side = Vector3Normalize( Vector3CrossProduct( lean, F ) );
                Vector3 base = P( ( rnd( i + 7 ) - 0.5f ) * 0.6f, 0.0f, ( rnd( i + 9 ) - 0.5f ) * 0.4f );
                kit.m_hull.m_color = { 40, 36, 60, 255 };
                kit.m_hull.box( frame( side, lean, Vector3CrossProduct( side, lean ),
                                       Vector3Add( base, Vector3Scale( lean, tall * 0.5f ) ) ),
                                { 0.07f, tall * 0.5f, 0.07f } );
                Vector3 tip = Vector3Add( base, Vector3Scale( lean, tall ) );
                kit.m_glows.push_back( { tip, { 0.05f, 0.05f, 0.05f }, { 3.0f, 1.2f, 5.0f, 0.0f } } );
            }
            light( 0.0f, 1.2f, 0.3f, { 2.0f, 0.8f, 3.5f }, 3.2f );
            block( 0.0f, 0.42f );
        }
        else
        {
            // A dark shard with a line of light.
            kit.m_hull.m_color = { 26, 26, 32, 255 };
            kit.m_hull.box( M( 0.0f, 0.9f, 0.0f, 0.3f ), { 0.22f, 0.9f, 0.14f } );
            glow( 0.0f, 0.9f, 0.15f, { 0.012f, 0.75f, 0.01f }, { 0.4f, 3.5f, 4.0f, 0.0f } );
            light( 0.0f, 1.0f, 0.5f, { 0.3f, 2.0f, 2.4f }, 2.5f );
            block( 0.0f, 0.32f );
        }
        break;

    default:   // GARDEN
        switch ( pick % 7 )
        {
        case 4:   // a bird bath
            kit.m_rock.m_color = { 175, 170, 160, 255 };
            kit.m_rock.lathe( M( 0.0f, 0.0f, 0.0f ),
                              { { 0.0f, 0.0f },
                                { 0.25f, 0.0f },
                                { 0.1f, 0.15f },
                                { 0.08f, 0.7f },
                                { 0.4f, 0.8f },
                                { 0.4f, 0.88f },
                                { 0.0f, 0.84f } },
                              16 );
            glow( 0.0f, 0.86f, 0.0f, { 0.25f, 0.005f, 0.25f }, { 0.3f, 0.6f, 0.8f, 0.0f } );
            block( 0.0f, 0.38f );
            break;
        case 5:   // a wheelbarrow of soil
            kit.m_rust.m_color = { 60, 110, 90, 255 };
            kit.m_rust.box( M( 0.0f, 0.42f, 0.0f, 0.3f ), { 0.4f, 0.15f, 0.28f } );
            kit.m_cloth.m_color = { 70, 50, 35, 255 };
            kit.m_cloth.roundedBox( M( 0.0f, 0.55f, 0.0f, 0.3f ), { 0.36f, 0.06f, 0.24f }, 0.05f, 2 );
            kit.m_metal.cylinder( alongX( 0.38f, 0.16f, 0.15f ), 0.16f, 0.06f, 12 );
            block( 0.0f, 0.45f );
            break;
        case 6:   // a small statue on a plinth
            kit.m_rock.m_color = { 190, 185, 175, 255 };
            kit.m_rock.box( M( 0.0f, 0.3f, 0.0f ), { 0.25f, 0.3f, 0.25f } );
            kit.m_hull.m_color = { 210, 205, 195, 255 };
            kit.m_hull.lathe( M( 0.0f, 0.6f, 0.0f ),
                              { { 0.0f, 0.0f },
                                { 0.14f, 0.0f },
                                { 0.08f, 0.35f },
                                { 0.16f, 0.6f },
                                { 0.06f, 0.85f },
                                { 0.0f, 0.9f } },
                              12 );
            block( 0.0f, 0.32f );
            break;
        case 0:   // a bush
            kit.m_cloth.m_color = { 55, 105, 50, 255 };
            for ( int i = 0; i < 6; ++i )
                kit.m_cloth.roundedBox( M( ( rnd( i ) - 0.5f ) * 0.7f, 0.3f + rnd( i + 3 ) * 0.4f,
                                           ( rnd( i + 6 ) - 0.5f ) * 0.5f, rnd( i + 1 ) * 3.0f ),
                                        { 0.3f, 0.25f, 0.28f }, 0.2f, 3 );
            block( 0.0f, 0.5f );
            break;
        case 1:   // a lamp post
            kit.m_metal.m_color = { 50, 55, 50, 255 };
            kit.m_metal.cylinder( M( 0.0f, 0.0f, 0.0f ), 0.05f, 2.6f, 8 );
            kit.m_metal.box( M( 0.0f, 2.7f, 0.0f ), { 0.16f, 0.06f, 0.16f } );
            glow( 0.0f, 2.55f, 0.0f, { 0.12f, 0.1f, 0.12f }, { 6.0f, 5.0f, 3.0f, 0.0f } );
            light( 0.0f, 2.3f, 0.2f, { 5.0f, 4.0f, 2.4f }, 5.0f );
            block( 0.0f, 0.2f );
            break;
        case 2:   // a bench
            kit.m_crate.m_color = { 130, 90, 60, 255 };
            for ( int i = 0; i < 3; ++i )
                kit.m_crate.box( M( 0.0f, 0.45f, -0.12f + i * 0.12f ), { 0.8f, 0.025f, 0.05f } );
            for ( float x : { -0.7f, 0.7f } ) kit.m_metal.box( M( x, 0.22f, 0.0f ), { 0.03f, 0.22f, 0.18f } );
            block( 0.0f, 0.5f );
            break;
        default:   // a flower bed
            kit.m_crate.m_color = { 100, 80, 60, 255 };
            kit.m_crate.box( M( 0.0f, 0.12f, 0.0f ), { 0.7f, 0.12f, 0.35f } );
            for ( int i = 0; i < 10; ++i )
            {
                kit.m_cloth.m_color = i % 3 ? Color{ 60, 120, 50, 255 } : paints[( i + pick ) % 6];
                kit.m_cloth.roundedBox(
                    M( -0.6f + ( i % 5 ) * 0.3f, 0.3f + rnd( i ) * 0.1f, -0.15f + ( i / 5 ) * 0.3f ),
                    { 0.09f, 0.09f, 0.09f }, 0.06f, 2 );
            }
            block( 0.0f, 0.55f );
            break;
        }
        break;
    }
    for ( MeshBuilder *b : { &kit.m_metal, &kit.m_rust, &kit.m_crate, &kit.m_box, &kit.m_cloth, &kit.m_hazard,
                             &kit.m_grate, &kit.m_rock, &kit.m_hull } )
        b->m_color = WHITE;
}

void Level::showPiece( int group, unsigned h, Vector3 at, DressKit &kit )
{
    auto P = [&]( float x, float y, float z ) { return Vector3Add( at, { x, y, z } ); };
    auto M = [&]( float x, float y, float z, float spin = 0.0f )
    { return place( P( x, y, z ), { 0.0f, spin, 0.0f } ); };
    auto glow = [&]( float x, float y, float z, Vector3 half, Vector4 colour )
    { kit.m_glows.push_back( { P( x, y, z ), half, colour } ); };
    auto light = [&]( float x, float y, float z, Vector3 colour, float radius, float flicker = 0.0f )
    { m_lights.push_back( { P( x, y, z ), colour, radius, flicker } ); };
    auto ring = [&]( float y, float radius, int count, Vector4 colour )
    {
        for ( int i = 0; i < count; ++i )
        {
            float a = i * 2.0f * PI / count;
            glow( cosf( a ) * radius, y, sinf( a ) * radius, { 0.06f, 0.05f, 0.06f }, colour );
        }
    };
    unsigned pick = h >> 5;

    switch ( group )
    {
    case INDUSTRIAL:
        if ( pick % 3 == 0 )
        {
            // A reactor column: a heavy base, the core with rings of light, pipes into the dark above.
            kit.m_metal.lathe(
                M( 0, 0, 0 ), { { 0.0f, 0.0f }, { 1.3f, 0.0f }, { 1.3f, 0.35f }, { 1.0f, 0.5f }, { 0.0f, 0.5f } }, 24 );
            kit.m_rust.m_color = { 120, 116, 110, 255 };
            kit.m_rust.cylinder( M( 0, 0.5f, 0 ), 0.75f, 3.6f, 24 );
            for ( float y : { 1.2f, 2.4f, 3.6f } ) kit.m_metal.cylinder( M( 0, y - 0.08f, 0 ), 0.8f, 0.16f, 24 );
            Vector4 core = pick % 2 ? Vector4{ 0.6f, 3.0f, 5.0f, 0.0f } : Vector4{ 5.0f, 2.2f, 0.5f, 0.0f };
            for ( float y : { 1.2f, 2.4f, 3.6f } ) ring( y, 0.82f, 12, core );
            for ( int i = 0; i < 4; ++i )
            {
                float a = i * PI * 0.5f + 0.4f;
                kit.m_rust.cylinder( M( cosf( a ) * 0.55f, 4.1f, sinf( a ) * 0.55f ), 0.1f, 2.0f, 8 );
            }
            light( 0.0f, 2.4f, 1.3f, { core.x * 1.6f, core.y * 1.6f, core.z * 1.6f }, 7.0f );
            m_props.push_back( { at, 1.35f } );
        }
        else if ( pick % 3 == 1 )
        {
            // A heap of ore under a crane arm with a hook.
            kit.m_rust.m_color = { 110, 80, 60, 255 };
            for ( int i = 0; i < 9; ++i )
            {
                float a = i * 0.7f, d = ( i % 3 ) * 0.35f;
                float s = 0.5f - ( i % 3 ) * 0.1f;
                kit.m_rust.roundedBox( place( P( cosf( a ) * d, s * 0.6f + ( i < 3 ? 0.3f : 0.0f ), sinf( a ) * d ),
                                              { 0.3f * i, a, 0.2f } ),
                                       { s, s * 0.6f, s * 0.8f }, s * 0.3f, 2 );
            }
            kit.m_metal.m_color = { 200, 160, 40, 255 };
            kit.m_metal.box( M( -1.3f, 2.2f, -0.6f ), { 0.15f, 2.2f, 0.15f } );
            kit.m_metal.box( M( -0.4f, 4.3f, -0.6f ), { 1.05f, 0.12f, 0.12f } );
            kit.m_metal.box( M( 0.5f, 3.4f, -0.6f ), { 0.01f, 0.85f, 0.01f } );
            kit.m_rust.m_color = WHITE;
            kit.m_rust.box( M( 0.5f, 2.5f, -0.6f ), { 0.15f, 0.08f, 0.15f } );
            glow( -1.3f, 4.5f, -0.6f, { 0.08f, 0.04f, 0.08f }, { 7.0f, 4.0f, 0.5f, 0.0f } );
            light( -1.0f, 4.0f, 0.0f, { 5.0f, 3.2f, 1.2f }, 6.0f, 0.3f );
            m_props.push_back( { at, 1.2f } );
        }
        else
        {
            // A worn drill head on its stand, lying.
            kit.m_metal.m_color = { 150, 140, 125, 255 };
            kit.m_metal.lathe(
                frame( { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, P( -1.3f, 0.75f, 0.0f ) ),
                { { 0.0f, 0.0f }, { 0.7f, 0.0f }, { 0.7f, 0.6f }, { 0.55f, 1.6f }, { 0.25f, 2.4f }, { 0.0f, 2.7f } },
                16 );
            for ( int i = 0; i < 6; ++i )
                kit.m_rust.box( place( P( -0.6f + i * 0.35f, 0.75f, 0.0f ), { i * 0.9f, 0.0f, 0.0f } ),
                                { 0.05f, 0.72f - i * 0.08f, 0.06f } );
            kit.m_crate.box( M( -0.8f, 0.2f, 0.0f ), { 0.2f, 0.2f, 0.7f } );
            kit.m_crate.box( M( 0.6f, 0.2f, 0.0f ), { 0.2f, 0.2f, 0.6f } );
            m_props.push_back( { at, 1.35f } );
        }
        break;

    case STREET:
    case GARDEN:
        if ( group == GARDEN && pick % 2 == 0 )
        {
            // A tree in a ring of stone with a bench around it.
            kit.m_crate.m_color = { 90, 65, 45, 255 };
            kit.m_crate.cylinder( M( 0, 0, 0 ), 0.22f, 2.6f, 10 );
            kit.m_cloth.m_color = { 55, 110, 50, 255 };
            for ( int i = 0; i < 9; ++i )
            {
                float a = i * 0.9f, d = 0.3f + ( i % 3 ) * 0.35f;
                kit.m_cloth.roundedBox(
                    place( P( cosf( a ) * d, 2.6f + ( i % 4 ) * 0.35f, sinf( a ) * d ), { 0.0f, a, 0.0f } ),
                    { 0.6f, 0.45f, 0.55f }, 0.4f, 3 );
            }
            kit.m_rock.lathe( M( 0, 0, 0 ), { { 1.1f, 0.0f }, { 1.1f, 0.45f }, { 1.3f, 0.45f }, { 1.3f, 0.0f } }, 24 );
            m_props.push_back( { at, 1.35f } );
        }
        else if ( pick % 3 == 0 || group == GARDEN )
        {
            // A fountain: a stone basin, a column, water lit from below.
            kit.m_rock.m_color = { 170, 165, 155, 255 };
            kit.m_rock.lathe( M( 0, 0, 0 ),
                              { { 1.35f, 0.0f }, { 1.35f, 0.55f }, { 1.2f, 0.55f }, { 1.2f, 0.2f }, { 0.0f, 0.2f } },
                              28 );
            kit.m_rock.lathe( M( 0, 0.2f, 0 ),
                              { { 0.25f, 0.0f }, { 0.15f, 1.2f }, { 0.5f, 1.3f }, { 0.45f, 1.4f }, { 0.0f, 1.4f } },
                              16 );
            glow( 0.0f, 0.45f, 0.0f, { 0.85f, 0.01f, 0.85f }, { 0.25f, 0.7f, 1.0f, 0.0f } );
            for ( int i = 0; i < 6; ++i )
            {
                float a = i * PI / 3.0f;
                glow( cosf( a ) * 0.45f, 1.1f, sinf( a ) * 0.45f, { 0.015f, 0.45f, 0.015f },
                      { 0.5f, 1.2f, 1.8f, 0.0f } );
            }
            light( 0.0f, 1.0f, 0.0f, { 1.2f, 2.6f, 3.6f }, 5.0f );
            m_props.push_back( { at, 1.4f } );
        }
        else if ( pick % 3 == 1 )
        {
            // A founder on a plinth, raising a pick.
            kit.m_rock.m_color = { 150, 145, 140, 255 };
            kit.m_rock.box( M( 0, 0.6f, 0 ), { 0.7f, 0.6f, 0.7f } );
            kit.m_hull.m_color = { 150, 110, 60, 255 };
            kit.m_hull.box( M( -0.13f, 1.65f, 0 ), { 0.09f, 0.45f, 0.1f } );
            kit.m_hull.box( M( 0.13f, 1.65f, 0 ), { 0.09f, 0.45f, 0.1f } );
            kit.m_hull.roundedBox( M( 0, 2.45f, 0 ), { 0.26f, 0.38f, 0.16f }, 0.08f, 2 );
            kit.m_hull.roundedBox( M( 0, 3.0f, 0 ), { 0.12f, 0.14f, 0.13f }, 0.08f, 2 );
            kit.m_hull.box( M( 0.34f, 3.0f, 0 ), { 0.06f, 0.4f, 0.07f } );
            kit.m_hull.box( M( -0.32f, 2.3f, 0.05f ), { 0.06f, 0.35f, 0.07f } );
            kit.m_hull.box( M( 0.34f, 3.45f, 0 ), { 0.35f, 0.05f, 0.05f } );
            glow( 0.0f, 0.15f, 0.72f, { 0.5f, 0.02f, 0.01f }, { 5.0f, 3.5f, 1.5f, 0.0f } );
            light( 0.0f, 0.4f, 1.4f, { 3.5f, 2.6f, 1.4f }, 4.5f );
            m_props.push_back( { at, 1.05f } );
        }
        else
        {
            // A column of advertising light.
            kit.m_metal.cylinder( M( 0, 0, 0 ), 0.55f, 0.3f, 20 );
            kit.m_metal.cylinder( M( 0, 3.6f, 0 ), 0.55f, 0.15f, 20 );
            Vector4 cols[3] = { { 1.6f, 0.25f, 1.0f, 0.0f }, { 0.2f, 1.0f, 1.6f, 0.0f }, { 1.6f, 1.0f, 0.2f, 0.0f } };
            for ( int i = 0; i < 3; ++i )
            {
                glow( 0.0f, 0.75f + i * 1.0f, 0.0f, { 0.42f, 0.4f, 0.42f }, cols[( i + pick ) % 3] );
                kit.m_metal.cylinder( M( 0, 1.2f + i * 1.0f, 0 ), 0.5f, 0.1f, 20 );
            }
            light( 0.0f, 1.8f, 0.0f, { 3.5f, 2.0f, 3.0f }, 6.0f );
            m_props.push_back( { at, 0.75f } );
        }
        break;

    case CLEAN:
        if ( pick % 2 == 0 )
        {
            // A model of the station on a plinth, its windows lit, a ring of light around it.
            kit.m_hull.m_color = { 210, 212, 215, 255 };
            kit.m_hull.lathe( M( 0, 0, 0 ), { { 0.0f, 0.0f }, { 0.9f, 0.0f }, { 0.9f, 0.8f }, { 0.0f, 0.8f } }, 24 );
            kit.m_metal.lathe( M( 0, 0.8f, 0 ),
                               { { 0.0f, 0.0f },
                                 { 0.12f, 0.0f },
                                 { 0.12f, 0.5f },
                                 { 0.6f, 0.65f },
                                 { 0.6f, 0.8f },
                                 { 0.15f, 0.95f },
                                 { 0.15f, 1.5f },
                                 { 0.0f, 1.6f } },
                               20 );
            ring( 1.52f, 0.62f, 14, { 0.8f, 2.4f, 3.6f, 0.0f } );
            ring( 1.95f, 0.95f, 18, { 0.4f, 1.2f, 2.0f, 0.0f } );
            light( 0.0f, 1.8f, 0.0f, { 1.2f, 2.4f, 3.4f }, 5.0f );
            m_props.push_back( { at, 1.0f } );
        }
        else
        {
            // A planter island with a young tree and benches.
            kit.m_hull.m_color = { 220, 220, 215, 255 };
            kit.m_hull.box( M( 0, 0.3f, 0 ), { 1.0f, 0.3f, 1.0f } );
            kit.m_cloth.m_color = { 70, 60, 45, 255 };
            kit.m_cloth.box( M( 0, 0.6f, 0 ), { 0.9f, 0.02f, 0.9f } );
            kit.m_crate.m_color = { 110, 85, 60, 255 };
            kit.m_crate.cylinder( M( 0, 0.6f, 0 ), 0.12f, 1.8f, 8 );
            kit.m_cloth.m_color = { 70, 130, 60, 255 };
            for ( int i = 0; i < 6; ++i )
            {
                float a = i * 1.05f;
                kit.m_cloth.roundedBox(
                    place( P( cosf( a ) * 0.35f, 2.4f + ( i % 2 ) * 0.3f, sinf( a ) * 0.35f ), { 0.0f, a, 0.0f } ),
                    { 0.45f, 0.35f, 0.4f }, 0.3f, 3 );
            }
            m_props.push_back( { at, 1.3f } );
        }
        break;

    case COLD:
        // Cryo tanks back to back, their windows frosted blue.
        for ( int i = 0; i < 4; ++i )
        {
            float a = i * PI * 0.5f + 0.785f;
            kit.m_metal.m_color = { 200, 210, 222, 255 };
            kit.m_metal.cylinder( M( cosf( a ) * 0.55f, 0.0f, sinf( a ) * 0.55f ), 0.42f, 2.4f, 16 );
            glow( cosf( a ) * 0.98f, 1.4f, sinf( a ) * 0.98f, { 0.08f, 0.45f, 0.08f }, { 0.6f, 2.2f, 4.5f, 0.0f } );
        }
        kit.m_metal.cylinder( M( 0, 2.4f, 0 ), 1.1f, 0.15f, 20 );
        light( 0.0f, 1.6f, 1.3f, { 1.0f, 2.4f, 4.4f }, 5.5f );
        m_props.push_back( { at, 1.15f } );
        break;

    case OUTDOOR:
        if ( pick % 2 == 0 )
        {
            // The nose of a crashed shuttle, half buried, a lamp still blinking.
            Vector3 fwd = Vector3Normalize( { 1.0f, -0.35f, 0.3f } );
            Vector3 side = Vector3Normalize( Vector3CrossProduct( { 0.0f, 1.0f, 0.0f }, fwd ) );
            Vector3 up = Vector3CrossProduct( fwd, side );
            kit.m_hull.m_color = { 170, 168, 160, 255 };
            kit.m_hull.lathe( frame( side, fwd, Vector3CrossProduct( side, fwd ), P( -1.0f, 0.9f, -0.2f ) ),
                              { { 0.0f, 0.0f }, { 0.85f, 0.0f }, { 0.85f, 1.2f }, { 0.6f, 2.0f }, { 0.0f, 2.4f } },
                              12 );
            kit.m_rust.box( frame( side, up, fwd, P( -0.4f, 1.2f, 0.4f ) ), { 0.05f, 0.4f, 0.9f } );
            glow( 0.6f, 0.5f, 0.1f, { 0.06f, 0.06f, 0.06f }, { 6.0f, 0.8f, 0.3f, 0.0f } );
            light( 0.6f, 0.9f, 0.6f, { 4.0f, 0.6f, 0.3f }, 4.0f, 0.7f );
            m_props.push_back( { at, 1.35f } );
        }
        else
        {
            // A dish on its mount, a lamp at the feed.
            kit.m_metal.box( M( 0, 0.5f, 0 ), { 0.4f, 0.5f, 0.4f } );
            kit.m_metal.cylinder( M( 0, 1.0f, 0 ), 0.12f, 0.9f, 10 );
            Vector3 aim = Vector3Normalize( { 0.3f, 1.0f, 0.5f } );
            Vector3 side = Vector3Normalize( Vector3CrossProduct( aim, { 0.0f, 0.0f, 1.0f } ) );
            kit.m_hull.m_color = { 210, 210, 205, 255 };
            kit.m_hull.lathe( frame( side, aim, Vector3CrossProduct( side, aim ), P( 0.0f, 1.9f, 0.0f ) ),
                              { { 0.0f, 0.0f }, { 0.6f, 0.15f }, { 1.1f, 0.45f }, { 1.12f, 0.5f } }, 24 );
            glow( aim.x * 0.9f, 1.9f + aim.y * 0.9f, aim.z * 0.9f, { 0.05f, 0.05f, 0.05f },
                  { 7.0f, 1.0f, 0.4f, 0.0f } );
            light( 0.0f, 3.0f, 0.4f, { 3.0f, 0.5f, 0.3f }, 3.5f, 0.6f );
            m_props.push_back( { at, 1.0f } );
        }
        break;

    default:   // ALIEN
        // A monolith with lines of light, crystals at its foot.
        kit.m_hull.m_color = { 22, 22, 28, 255 };
        kit.m_hull.box( M( 0, 1.9f, 0, 0.35f ), { 0.45f, 1.9f, 0.3f } );
        for ( int i = 0; i < 5; ++i )
            glow( -0.3f + i * 0.15f, 1.0f + ( i % 3 ) * 0.7f, 0.33f, { 0.01f, 0.35f + ( i % 2 ) * 0.2f, 0.01f },
                  { 0.4f, 3.2f, 4.2f, 0.0f } );
        for ( int i = 0; i < 6; ++i )
        {
            float a = i * 1.05f;
            glow( cosf( a ) * 0.8f, 0.15f, sinf( a ) * 0.8f, { 0.06f, 0.15f, 0.06f }, { 2.5f, 1.0f, 4.5f, 0.0f } );
        }
        light( 0.0f, 2.0f, 1.0f, { 0.6f, 2.6f, 3.6f }, 6.0f );
        m_props.push_back( { at, 0.85f } );
        break;
    }
    for ( MeshBuilder *b : { &kit.m_metal, &kit.m_rust, &kit.m_crate, &kit.m_box, &kit.m_cloth, &kit.m_hazard,
                             &kit.m_grate, &kit.m_rock, &kit.m_hull } )
        b->m_color = WHITE;
}

int Level::dressGroup( ThemeE t )
{
    return dressOf( t );
}

void Level::wallKit( int r, int c, int d, Vector3 face, Vector3 n, float base, int walls, bool decal, DressKit &kit )
{
    // Its own frame: x along the face, y up, z out of the wall into the cell.
    const ThemeE t = theme( r, c );
    const int    group = dressOf( t );
    if ( t == ThemeE::STREET || t == ThemeE::HULL || t == ThemeE::YARD || t == ThemeE::CONTAINER ||
         t == ThemeE::ARENA || walls == 3 )
        return;   // house fronts, open space and the arena's tiers have their own
    Vector3 F = n, R = { F.z, 0.0f, -F.x }, U = { 0.0f, 1.0f, 0.0f };
    auto    P = [&]( float x, float y, float z )
    {
        return Vector3Add(
            face, Vector3Add( Vector3Scale( R, x ), Vector3Add( Vector3Scale( U, base + y ), Vector3Scale( F, z ) ) ) );
    };
    auto M = [&]( float x, float y, float z ) { return frame( R, U, F, P( x, y, z ) ); };
    auto along = [&]( float x, float y, float z ) { return frame( Vector3Negate( U ), R, F, P( x, y, z ) ); };
    auto outward = [&]( float x, float y, float z ) { return frame( R, F, Vector3Negate( U ), P( x, y, z ) ); };
    auto tilted = [&]( float x, float y, float z, float angle )
    { return MatrixMultiply( MatrixRotateZ( angle ), frame( R, U, F, P( x, y, z ) ) ); };
    auto glow = [&]( float x, float y, float z, Vector3 half, Vector4 colour )
    {
        Vector3 world = { fabsf( R.x ) * half.x + fabsf( F.x ) * half.z, half.y,
                          fabsf( R.z ) * half.x + fabsf( F.z ) * half.z };
        kit.m_glows.push_back( { P( x, y, z ), world, colour, true } );   // in the wall: dissolves with it
    };
    auto light = [&]( float x, float y, float z, Vector3 colour, float radius, float flicker = 0.0f )
    { m_lights.push_back( { P( x, y, z ), colour, radius, flicker } ); };
    auto paint = [&]( MeshBuilder &b, Color col ) -> MeshBuilder &
    {
        b.m_color = col;
        return b;
    };
    const unsigned h = hashCell( r, c * 4 + d, 301 );
    auto           rnd = [&]( int i ) { return ( ( h >> ( i % 24 ) ) & 255u ) / 255.0f; };
    const float    top = m_wallHeight;
    const Color    steel = { 120, 124, 130, 255 }, dark = { 62, 66, 72, 255 }, pale = { 212, 216, 220, 255 };

    if ( walls == 1 )
    {
        // Stacked containers: now and then a tie-down strap down the stack, ratchet at the bottom.
        if ( rnd( 3 ) < 0.35f )
        {
            float x = -0.6f + 1.2f * rnd( 7 );
            paint( kit.m_drape, { 220, 120, 40, 255 } )
                .box( M( x, top * 0.5f, 0.06f ), { 0.035f, top * 0.5f, 0.012f } );
            paint( kit.m_metal, dark ).box( M( x, 0.45f, 0.09f ), { 0.06f, 0.08f, 0.04f } );
        }
        kit.m_drape.m_color = WHITE;
        kit.m_metal.m_color = WHITE;
        return;
    }

    // Two places on a face: left and right of its middle (where every other face has its rib). A face that carries a
    // decal keeps its middle free and gets only slim things at its edges, low and high.
    const float sides[2] = { -0.6f, 0.6f };
    switch ( group )
    {
    case INDUSTRIAL:
    {
        // A kick plate along the floor and a girder along the top: the frame of a ship's or a works' wall.
        paint( kit.m_metal, rnd( 1 ) < 0.25f ? Color{ 196, 150, 40, 255 } : dark )
            .box( M( 0.0f, 0.11f, 0.1f ), { 1.0f, 0.11f, 0.035f } );
        paint( kit.m_metal, steel ).box( M( 0.0f, top - 0.2f, 0.13f ), { 1.0f, 0.1f, 0.13f } );
        paint( kit.m_metal, dark ).box( M( 0.0f, top - 0.33f, 0.24f ), { 1.0f, 0.025f, 0.02f } );
        // Cables along the foot of the wall, running on from cell to cell (flat: nothing to stumble over).
        unsigned run = hashCell( DR[d] != 0 ? r : c / 5, DR[d] != 0 ? c / 5 : c, 304 + d );
        if ( run % 3 == 0 )
            for ( int k = 0; k < 2 + (int)( run >> 4 ) % 2; ++k )
                paint( kit.m_rust, k == 1 ? Color{ 150, 45, 35, 255 } : dark )
                    .cylinder( along( -1.0f, 0.03f + k * 0.01f, 0.2f + k * 0.07f ), 0.028f, 2.0f, 6 );
        if ( !decal )
            for ( float s : { -1.0f, 1.0f } )   // knee braces from the rib up to the girder
                paint( kit.m_metal, steel )
                    .box( tilted( s * 0.32f, top - 0.55f, 0.14f, s * 0.75f ), { 0.035f, 0.32f, 0.07f } );
        for ( int i = 0; i < 2; ++i )
        {
            float x = sides[i];
            float roll = rnd( 5 + i * 7 );
            if ( decal )
            {
                // Slim things at the edge only: a bundle of cables down the wall, or a caged lamp high up.
                float ex = i ? 0.86f : -0.86f;
                if ( roll < 0.35f )
                {
                    for ( int k = 0; k < 3; ++k )
                        paint( kit.m_rust, k == 1 ? Color{ 150, 40, 30, 255 } : dark )
                            .cylinder( M( ex + ( k - 1 ) * 0.05f, 0.0f, 0.07f + 0.015f * k ), 0.022f, top - 0.3f, 6 );
                    for ( float y = 0.8f; y < top - 0.5f; y += 0.9f )
                        paint( kit.m_metal, steel ).box( M( ex, y, 0.09f ), { 0.1f, 0.025f, 0.04f } );
                }
                else if ( roll < 0.55f )
                {
                    paint( kit.m_metal, dark ).box( M( ex, 2.75f, 0.08f ), { 0.08f, 0.06f, 0.06f } );
                    glow( ex, 2.75f, 0.15f, { 0.05f, 0.035f, 0.02f },
                          rnd( 9 ) < 0.5f ? Vector4{ 6.0f, 2.2f, 0.4f, 0.0f } : Vector4{ 5.0f, 0.5f, 0.3f, 0.0f } );
                    if ( hash01( r, c * 4 + d, 302 ) < 0.18f )
                        light( ex, 2.7f, 0.5f, { 3.0f, 1.2f, 0.4f }, 3.5f, rnd( 11 ) < 0.3f ? 0.6f : 0.0f );
                }
                continue;
            }
            if ( roll < 0.12f )
            {
                // A junction box, its conduit up into the girder, a status light.
                paint( kit.m_metal, steel ).box( M( x, 1.45f, 0.08f ), { 0.22f, 0.3f, 0.08f } );
                paint( kit.m_metal, dark ).box( M( x, 1.45f, 0.165f ), { 0.2f, 0.005f, 0.005f } );
                paint( kit.m_rust, dark ).cylinder( M( x + 0.1f, 1.75f, 0.06f ), 0.03f, top - 2.05f, 6 );
                glow( x - 0.14f, 1.68f, 0.17f, { 0.025f, 0.025f, 0.01f },
                      rnd( 13 ) < 0.6f ? Vector4{ 0.3f, 4.0f, 0.6f, 0.0f } : Vector4{ 5.0f, 2.5f, 0.3f, 0.0f } );
            }
            else if ( roll < 0.24f )
            {
                // A pipe up from the floor with a valve wheel.
                paint( kit.m_rust, { 150, 120, 95, 255 } ).cylinder( M( x, 0.0f, 0.12f ), 0.07f, 2.3f, 10 );
                paint( kit.m_rust, { 150, 120, 95, 255 } ).cylinder( outward( x, 1.25f, 0.12f ), 0.03f, 0.16f, 6 );
                paint( kit.m_metal, { 170, 40, 35, 255 } ).cylinder( outward( x, 1.25f, 0.27f ), 0.17f, 0.03f, 14 );
                paint( kit.m_metal, { 170, 40, 35, 255 } ).box( M( x, 1.25f, 0.29f ), { 0.15f, 0.015f, 0.012f } );
                paint( kit.m_metal, { 170, 40, 35, 255 } ).box( M( x, 1.25f, 0.29f ), { 0.015f, 0.15f, 0.012f } );
            }
            else if ( roll < 0.36f )
            {
                // A vent: a grille in a frame, low or high.
                float y = rnd( 15 ) < 0.5f ? 0.55f : 3.0f;
                paint( kit.m_metal, dark ).box( M( x, y, 0.03f ), { 0.3f, 0.22f, 0.01f } );
                for ( float s = -0.16f; s < 0.2f; s += 0.08f )
                    paint( kit.m_metal, steel ).box( M( x, y + s, 0.05f ), { 0.29f, 0.012f, 0.02f } );
                paint( kit.m_metal, dark ).box( M( x, y + 0.24f, 0.07f ), { 0.33f, 0.025f, 0.03f } );
                paint( kit.m_metal, dark ).box( M( x, y - 0.24f, 0.07f ), { 0.33f, 0.025f, 0.03f } );
                for ( float s : { -1.0f, 1.0f } )
                    paint( kit.m_metal, dark ).box( M( x + s * 0.31f, y, 0.07f ), { 0.025f, 0.22f, 0.03f } );
            }
            else if ( roll < 0.46f )
            {
                // A ladder up to the girder.
                for ( float s : { -1.0f, 1.0f } )
                    paint( kit.m_metal, steel )
                        .box( M( x + s * 0.2f, ( top - 0.3f ) * 0.5f, 0.14f ),
                              { 0.02f, ( top - 0.3f ) * 0.5f, 0.02f } );
                for ( float y = 0.35f; y < top - 0.4f; y += 0.32f )
                    paint( kit.m_metal, steel ).box( M( x, y, 0.14f ), { 0.2f, 0.014f, 0.014f } );
                for ( float y : { 0.4f, top - 0.6f } )
                    paint( kit.m_metal, dark ).box( M( x, y, 0.07f ), { 0.24f, 0.02f, 0.07f } );
            }
            else if ( roll < 0.58f )
            {
                // A breaker cabinet: its door, a handle, a strip of status lights.
                paint( kit.m_metal, { 96, 104, 98, 255 } ).box( M( x, 1.3f, 0.1f ), { 0.3f, 0.48f, 0.1f } );
                paint( kit.m_metal, dark ).box( M( x, 1.3f, 0.205f ), { 0.004f, 0.46f, 0.004f } );
                paint( kit.m_metal, pale ).box( M( x + 0.2f, 1.3f, 0.21f ), { 0.015f, 0.08f, 0.015f } );
                for ( int k = 0; k < 3; ++k )
                    glow( x - 0.2f + k * 0.06f, 1.68f, 0.205f, { 0.018f, 0.018f, 0.006f },
                          ( h >> ( k * 3 ) ) % 4 == 0 ? Vector4{ 5.0f, 0.5f, 0.3f, 0.0f }
                                                      : Vector4{ 0.3f, 4.0f, 0.6f, 0.0f } );
            }
            else if ( roll < 0.68f )
            {
                // Sealant canisters in a bracket (the ship's fire extinguishers).
                paint( kit.m_metal, dark ).box( M( x, 0.95f, 0.06f ), { 0.2f, 0.03f, 0.06f } );
                for ( float s : { -1.0f, 1.0f } )
                    paint( kit.m_metal, { 190, 50, 35, 255 } )
                        .cylinder( M( x + s * 0.09f, 0.45f, 0.12f ), 0.065f, 0.5f, 10 );
            }
            else if ( roll < 0.78f )
            {
                // Gauges on a plate, a pipe stub into them.
                paint( kit.m_metal, steel ).box( M( x, 1.65f, 0.04f ), { 0.26f, 0.18f, 0.02f } );
                for ( int k = 0; k < 2; ++k )
                    paint( kit.m_metal, pale )
                        .cylinder( outward( x - 0.11f + k * 0.22f, 1.67f, 0.06f ), 0.07f, 0.03f, 12 );
                paint( kit.m_rust, dark ).cylinder( M( x, 0.0f, 0.06f ), 0.025f, 1.47f, 6 );
            }
            else if ( roll < 0.86f )
            {
                // A data terminal: a dark box, its screen lit.
                paint( kit.m_metal, dark ).box( M( x, 1.5f, 0.07f ), { 0.24f, 0.18f, 0.07f } );
                glow( x, 1.52f, 0.145f, { 0.19f, 0.12f, 0.005f },
                      rnd( 17 ) < 0.5f ? Vector4{ 0.4f, 2.6f, 3.2f, 0.0f } : Vector4{ 3.4f, 2.4f, 0.5f, 0.0f } );
            }
            // else: bare, so the walls breathe
        }
        break;
    }
    case CLEAN:
    case STREET:   // the bar: warm panelling below, sconces above
    {
        bool  bar = group == STREET;
        Color strip = bar ? Color{ 90, 60, 42, 255 } : pale;
        paint( bar ? kit.m_box : kit.m_metal, strip )
            .box( M( 0.0f, bar ? 0.55f : 0.08f, 0.07f ), { 1.0f, bar ? 0.55f : 0.08f, 0.03f } );
        if ( bar )
            paint( kit.m_box, { 70, 46, 32, 255 } ).box( M( 0.0f, 1.12f, 0.1f ), { 1.0f, 0.03f, 0.04f } );
        else
        {
            // Seams that cut the wall into panels, a lit cove under the top.
            paint( kit.m_metal, dark ).box( M( 0.0f, 2.7f, 0.06f ), { 1.0f, 0.012f, 0.012f } );
            if ( !decal )
                for ( float s : { -0.5f, 0.5f } )
                    paint( kit.m_metal, dark ).box( M( s, 1.4f, 0.06f ), { 0.012f, 1.3f, 0.012f } );
            if ( t == ThemeE::PODS || t == ThemeE::CLINIC || t == ThemeE::VAULT || t == ThemeE::BRIDGE ||
                 t == ThemeE::NET )
                glow( 0.0f, top - 0.4f, 0.08f, { 1.0f, 0.015f, 0.02f }, { 2.2f, 2.5f, 2.8f, 0.0f } );
        }
        for ( int i = 0; i < 2; ++i )
        {
            float x = decal ? ( i ? 0.85f : -0.85f ) : sides[i];
            float roll = rnd( 5 + i * 7 );
            if ( bar )
            {
                if ( roll < 0.4f )
                {
                    // A sconce: a little shade, warm light.
                    paint( kit.m_metal, { 150, 110, 60, 255 } ).box( M( x, 2.2f, 0.08f ), { 0.08f, 0.1f, 0.06f } );
                    glow( x, 2.32f, 0.1f, { 0.06f, 0.02f, 0.04f }, { 7.0f, 4.2f, 1.6f, 0.0f } );
                }
                continue;
            }
            if ( decal ) continue;
            if ( roll < 0.25f )
            {
                // A wall display in a slim frame.
                paint( kit.m_metal, dark ).box( M( x, 1.75f, 0.04f ), { 0.3f, 0.2f, 0.025f } );
                glow( x, 1.75f, 0.07f, { 0.27f, 0.17f, 0.005f },
                      t == ThemeE::SERVER || t == ThemeE::CRYO ? Vector4{ 0.4f, 3.0f, 1.6f, 0.0f }
                                                               : Vector4{ 0.5f, 2.2f, 3.4f, 0.0f } );
            }
            else if ( roll < 0.45f )
            {
                // A handrail on two brackets.
                paint( kit.m_metal, pale ).cylinder( along( x - 0.35f, 1.0f, 0.14f ), 0.025f, 0.7f, 8 );
                for ( float s : { -0.25f, 0.25f } )
                    paint( kit.m_metal, steel ).box( M( x + s, 1.0f, 0.08f ), { 0.015f, 0.015f, 0.06f } );
            }
            else if ( roll < 0.6f )
            {
                // A data port: a small plate, two lights.
                paint( kit.m_metal, steel ).box( M( x, 1.2f, 0.03f ), { 0.1f, 0.14f, 0.02f } );
                for ( int k = 0; k < 2; ++k )
                    glow( x, 1.25f - k * 0.08f, 0.055f, { 0.02f, 0.012f, 0.004f }, { 0.4f, 3.0f, 3.4f, 0.0f } );
            }
            else if ( roll < 0.72f )
            {
                // A white cabinet set into the wall, a green light on it.
                paint( kit.m_metal, { 228, 230, 232, 255 } ).box( M( x, 1.1f, 0.06f ), { 0.26f, 0.38f, 0.06f } );
                glow( x + 0.18f, 1.4f, 0.125f, { 0.02f, 0.02f, 0.004f }, { 0.4f, 4.0f, 0.8f, 0.0f } );
            }
        }
        break;
    }
    case COLD:
    {
        // Heater pipes low along the wall, rimed; quilted insulation pads; a thermostat with a blue light.
        for ( int k = 0; k < 2; ++k )
            paint( kit.m_rust, { 170, 105, 70, 255 } )
                .cylinder( along( -1.0f, 0.3f + k * 0.16f, 0.12f ), 0.05f, 2.0f, 8 );
        paint( kit.m_metal, { 236, 242, 248, 255 } )
            .box( M( -0.3f + 0.6f * rnd( 3 ), 0.4f, 0.17f ), { 0.3f, 0.04f, 0.03f } );
        for ( int i = 0; i < 2; ++i )
        {
            float x = sides[i], roll = rnd( 5 + i * 7 );
            if ( decal ) continue;
            if ( roll < 0.45f )
            {
                paint( kit.m_drape, { 190, 196, 200, 255 } ).box( M( x, 1.7f, 0.07f ), { 0.32f, 0.6f, 0.05f } );
                for ( float y : { 1.4f, 2.0f } )
                    paint( kit.m_drape, { 150, 156, 160, 255 } ).box( M( x, y, 0.12f ), { 0.32f, 0.012f, 0.008f } );
            }
            else if ( roll < 0.65f )
            {
                paint( kit.m_metal, steel ).box( M( x, 1.5f, 0.05f ), { 0.12f, 0.16f, 0.05f } );
                glow( x, 1.58f, 0.105f, { 0.06f, 0.03f, 0.004f }, { 0.6f, 1.8f, 4.0f, 0.0f } );
            }
        }
        break;
    }
    case OUTDOOR:
    {
        // Rock held by bolts and mesh, a steel arch set against it, a cable on hooks, now and then a floodlight.
        for ( int k = 0; k < 4; ++k )
        {
            float x = -0.8f + 1.6f * rnd( k * 5 + 1 ), y = 0.8f + 2.6f * rnd( k * 5 + 3 );
            if ( decal && fabsf( x ) < 0.6f && y < 2.5f ) continue;
            paint( kit.m_metal, steel ).box( M( x, y, 0.03f ), { 0.06f, 0.06f, 0.012f } );
            paint( kit.m_metal, dark ).cylinder( outward( x, y, 0.04f ), 0.018f, 0.05f, 6 );
        }
        if ( !decal && rnd( 2 ) < 0.4f )   // a patch of mesh, its edge wires
            for ( float s = -0.6f; s <= 0.61f; s += 0.2f )
                paint( kit.m_metal, { 120, 120, 118, 255 } )
                    .box( M( sides[h & 1], 2.2f + s, 0.04f ), { 0.38f, 0.008f, 0.008f } );
        if ( !decal && rnd( 4 ) < 0.5f )
        {
            // A mine arch's leg: an H-beam and its flanges, a foot plate.
            float x = sides[( h >> 2 ) & 1] * 1.4f;
            paint( kit.m_rust, { 110, 92, 70, 255 } ).box( M( x, top * 0.5f, 0.1f ), { 0.025f, top * 0.5f, 0.08f } );
            for ( float s : { -1.0f, 1.0f } )
                paint( kit.m_rust, { 110, 92, 70, 255 } )
                    .box( M( x + s * 0.07f, top * 0.5f, 0.1f ), { 0.012f, top * 0.5f, 0.08f } );
            paint( kit.m_metal, dark ).box( M( x, 0.03f, 0.12f ), { 0.12f, 0.03f, 0.12f } );
        }
        paint( kit.m_metal, dark ).cylinder( along( -1.0f, 2.45f, 0.1f ), 0.02f, 2.0f, 6 );
        paint( kit.m_metal, steel ).box( M( 0.0f, 2.5f, 0.06f ), { 0.015f, 0.06f, 0.05f } );
        if ( hash01( r, c * 4 + d, 303 ) < 0.08f )
        {
            paint( kit.m_metal, dark ).box( M( 0.0f, 3.2f, 0.18f ), { 0.16f, 0.1f, 0.12f } );
            glow( 0.0f, 3.15f, 0.31f, { 0.13f, 0.07f, 0.01f }, { 7.0f, 6.5f, 5.0f, 0.0f } );
            light( 0.0f, 3.0f, 0.8f, { 3.5f, 3.3f, 2.8f }, 6.0f );
        }
        break;
    }
    case ALIEN:
    {
        // Lines of light cut into the black in steps; stepped relief; now and then a crystal node.
        Vector4 lit = rnd( 1 ) < 0.5f ? Vector4{ 0.3f, 2.4f, 3.4f, 0.0f } : Vector4{ 2.2f, 0.6f, 3.2f, 0.0f };
        float   y = 0.6f + 2.4f * rnd( 3 );
        glow( -0.5f, y, 0.02f, { 0.5f, 0.01f, 0.006f }, lit );
        glow( 0.0f, y + 0.25f, 0.02f, { 0.01f, 0.25f, 0.006f }, lit );
        glow( 0.5f, y + 0.5f, 0.02f, { 0.5f, 0.01f, 0.006f }, lit );
        if ( !decal )
            for ( int k = 0; k < 3; ++k )
                paint( kit.m_rock, { 40, 40, 48, 255 } )
                    .box( M( sides[h & 1], 0.6f + k * 0.5f, 0.04f + k * 0.03f ), { 0.3f - k * 0.08f, 0.24f, 0.04f } );
        if ( rnd( 9 ) < 0.25f )
            for ( int k = 0; k < 3; ++k )
                glow( sides[( h >> 3 ) & 1] + ( k - 1 ) * 0.07f, 0.25f + k * 0.06f, 0.12f,
                      { 0.03f, 0.12f + k * 0.04f, 0.03f }, lit );
        break;
    }
    case GARDEN:
    {
        // Planters with greenery along the wall, a trellis above some.
        for ( int i = 0; i < 2; ++i )
        {
            float x = decal ? ( i ? 0.8f : -0.8f ) : sides[i];
            if ( rnd( 5 + i * 7 ) < 0.5f )
            {
                paint( kit.m_box, { 120, 110, 96, 255 } )
                    .box( M( x, 0.22f, 0.2f ), { decal ? 0.15f : 0.3f, 0.22f, 0.16f } );
                paint( kit.m_drape, { 70, 120, 60, 255 } )
                    .box( M( x, 0.52f, 0.2f ), { decal ? 0.14f : 0.28f, 0.12f, 0.15f } );
                if ( !decal && rnd( 11 + i ) < 0.5f )
                {
                    for ( float s : { -0.2f, 0.0f, 0.2f } )
                        paint( kit.m_metal, steel ).box( M( x + s, 1.5f, 0.06f ), { 0.01f, 0.9f, 0.01f } );
                    paint( kit.m_drape, { 60, 110, 55, 255 } ).box( M( x, 1.3f, 0.07f ), { 0.24f, 0.5f, 0.03f } );
                }
            }
        }
        break;
    }
    default: break;
    }
    for ( MeshBuilder *b : { &kit.m_metal, &kit.m_rust, &kit.m_box, &kit.m_drape, &kit.m_rock } ) b->m_color = WHITE;
}

void Level::crownKit( int r, int c, int walls, Vector3 mid, DressKit &kit )
{
    // What lies on a wall's top by an open cell (the camera looks down on it): a coping along the edge, runs of pipes
    // or cable trays that line up from cell to cell, now and then a machine. All of it dissolves with the wall.
    const ThemeE t = theme( r, c );
    const int    group = dressOf( t );
    if ( t == ThemeE::STREET || t == ThemeE::ARENA || t == ThemeE::PARK || walls == 1 || walls == 3 ) return;
    const float    top = m_wallHeight + 0.02f;
    const unsigned h = hashCell( r, c, 311 );
    auto           rnd = [&]( int i ) { return ( ( h >> ( i % 24 ) ) & 255u ) / 255.0f; };
    const Color    steel = { 120, 124, 130, 255 }, dark = { 58, 62, 68, 255 }, pale = { 205, 210, 214, 255 };
    const bool     metalTop = walls == 2;
    Vector3        U = { 0.0f, 1.0f, 0.0f };
    auto           paint = [&]( MeshBuilder &b, Color col ) -> MeshBuilder &
    {
        b.m_color = col;
        return b;
    };
    int openSides = 0;
    for ( int d = 0; d < 4; ++d )
    {
        if ( cell( r + DR[d], c + DC[d] ).m_kind == CellE::ROCK ) continue;
        ++openSides;
        // Its frame: z towards the open side (the rim at 1), x along the rim.
        Vector3 F = { (float)DC[d], 0.0f, (float)DR[d] }, R = { F.z, 0.0f, -F.x };
        auto    P = [&]( float x, float y, float z )
        {
            return Vector3Add( mid, Vector3Add( Vector3Scale( R, x ),
                                                Vector3Add( Vector3Scale( U, top + y ), Vector3Scale( F, z ) ) ) );
        };
        auto M = [&]( float x, float y, float z ) { return frame( R, U, F, P( x, y, z ) ); };
        auto along = [&]( float x, float y, float z ) { return frame( Vector3Negate( U ), R, F, P( x, y, z ) ); };
        auto glow = [&]( float x, float y, float z, Vector3 half, Vector4 colour )
        {
            Vector3 world = { fabsf( R.x ) * half.x + fabsf( F.x ) * half.z, half.y,
                              fabsf( R.z ) * half.x + fabsf( F.z ) * half.z };
            kit.m_glows.push_back( { P( x, y, z ), world, colour, true } );
        };
        // Runs keep their kind along a wall: the hash of the wall's line, not of the cell.
        unsigned run = hashCell( DR[d] != 0 ? r : c / 6, DR[d] != 0 ? c / 6 : c, 312 + d );
        switch ( group )
        {
        case INDUSTRIAL:
            paint( kit.m_metal, run % 4 == 0 ? Color{ 196, 150, 40, 255 } : steel )
                .box( M( 0.0f, 0.05f, 0.9f ), { 1.0f, 0.05f, 0.1f } );
            if ( run % 3 == 0 )
            {
                // Two pipes on saddles.
                paint( kit.m_rust, { 150, 120, 95, 255 } ).cylinder( along( -1.0f, 0.1f, 0.62f ), 0.08f, 2.0f, 10 );
                paint( kit.m_rust, dark ).cylinder( along( -1.0f, 0.08f, 0.42f ), 0.06f, 2.0f, 8 );
                paint( kit.m_metal, dark ).box( M( 0.0f, 0.04f, 0.52f ), { 0.05f, 0.04f, 0.2f } );
            }
            else if ( run % 3 == 1 )
            {
                // A cable tray: its floor, its sides, the cables in it.
                paint( kit.m_metal, steel ).box( M( 0.0f, 0.02f, 0.55f ), { 1.0f, 0.02f, 0.18f } );
                for ( float s : { -0.18f, 0.18f } )
                    paint( kit.m_metal, steel ).box( M( 0.0f, 0.08f, 0.55f + s ), { 1.0f, 0.06f, 0.012f } );
                for ( int k = 0; k < 3; ++k )
                    paint( kit.m_rust, k == 1 ? Color{ 140, 40, 30, 255 } : dark )
                        .cylinder( along( -1.0f, 0.07f, 0.47f + k * 0.08f ), 0.03f, 2.0f, 6 );
            }
            else if ( metalTop )
            {
                // A duct with a flange.
                paint( kit.m_metal, { 140, 144, 148, 255 } ).box( M( 0.0f, 0.2f, 0.5f ), { 1.0f, 0.18f, 0.24f } );
                paint( kit.m_metal, dark ).box( M( -0.98f, 0.2f, 0.5f ), { 0.03f, 0.21f, 0.27f } );
            }
            break;
        case CLEAN:
        case COLD:
            paint( kit.m_metal, group == COLD ? Color{ 225, 232, 238, 255 } : pale )
                .box( M( 0.0f, 0.04f, 0.9f ), { 1.0f, 0.04f, 0.1f } );
            if ( group == CLEAN && run % 2 == 0 )
                glow( 0.0f, 0.085f, 0.9f, { 1.0f, 0.006f, 0.02f }, { 1.6f, 2.0f, 2.5f, 0.0f } );
            if ( group == COLD && rnd( d * 3 ) < 0.4f )   // frost along the edge
                paint( kit.m_metal, { 240, 246, 252, 255 } )
                    .box( M( -0.5f + rnd( d * 3 + 1 ), 0.06f, 0.75f ), { 0.3f, 0.03f, 0.12f } );
            break;
        case ALIEN:
            if ( run % 2 == 0 ) glow( 0.0f, 0.02f, 0.88f, { 1.0f, 0.006f, 0.015f }, { 0.3f, 2.0f, 3.0f, 0.0f } );
            break;
        case OUTDOOR:
            if ( rnd( d * 3 ) < 0.3f )   // loose rock on the edge
                paint( kit.m_rock, { 90, 80, 72, 255 } )
                    .box( frame( R, U, F, P( -0.6f + 1.2f * rnd( d * 3 + 1 ), 0.1f, 0.7f ) ), { 0.18f, 0.1f, 0.14f } );
            break;
        default: break;
        }
    }

    // Now and then a machine on the top, away from the edge: an air unit with its fan, a tank lying on saddles, a
    // cabinet, a mast with a red light.
    if ( openSides > 0 && ( group == INDUSTRIAL || ( group == CLEAN && metalTop ) || group == COLD ) &&
         rnd( 7 ) < 0.14f )
    {
        Vector3 F = { 0.0f, 0.0f, 1.0f }, R = { 1.0f, 0.0f, 0.0f };
        if ( h & 1 ) F = { 1.0f, 0.0f, 0.0f }, R = { 0.0f, 0.0f, -1.0f };
        auto P = [&]( float x, float y, float z )
        {
            return Vector3Add( mid, Vector3Add( Vector3Scale( R, x ),
                                                Vector3Add( Vector3Scale( U, top + y ), Vector3Scale( F, z ) ) ) );
        };
        auto M = [&]( float x, float y, float z ) { return frame( R, U, F, P( x, y, z ) ); };
        switch ( ( h >> 9 ) % 4 )
        {
        case 0:
            paint( kit.m_metal, { 150, 155, 150, 255 } ).box( M( 0.0f, 0.3f, 0.0f ), { 0.45f, 0.3f, 0.4f } );
            paint( kit.m_metal, dark ).cylinder( M( 0.0f, 0.6f, 0.0f ), 0.3f, 0.04f, 16 );
            for ( float s : { -0.12f, 0.0f, 0.12f } )
                paint( kit.m_metal, steel ).box( M( 0.0f, 0.65f, s ), { 0.3f, 0.01f, 0.012f } );
            break;
        case 1:
            paint( kit.m_rust, { 160, 165, 170, 255 } )
                .cylinder( frame( Vector3Negate( U ), R, F, P( -0.6f, 0.35f, 0.0f ) ), 0.3f, 1.2f, 16 );
            for ( float s : { -0.35f, 0.35f } )
                paint( kit.m_metal, dark ).box( M( s, 0.1f, 0.0f ), { 0.06f, 0.1f, 0.25f } );
            break;
        case 2:
            paint( kit.m_metal, { 98, 106, 100, 255 } ).box( M( 0.0f, 0.45f, 0.0f ), { 0.35f, 0.45f, 0.25f } );
            kit.m_glows.push_back(
                { P( 0.2f, 0.75f, 0.26f ), { 0.03f, 0.03f, 0.01f }, { 0.4f, 4.0f, 0.6f, 0.0f }, true } );
            break;
        default:
            paint( kit.m_metal, steel ).cylinder( M( 0.0f, 0.0f, 0.0f ), 0.04f, 1.6f, 6 );
            for ( float s : { -1.0f, 1.0f } )
                paint( kit.m_metal, steel )
                    .box( MatrixMultiply( MatrixRotateZ( s * 0.5f ), M( s * 0.18f, 0.35f, 0.0f ) ),
                          { 0.015f, 0.38f, 0.015f } );
            kit.m_glows.push_back(
                { P( 0.0f, 1.65f, 0.0f ), { 0.05f, 0.05f, 0.05f }, { 6.0f, 0.4f, 0.3f, 0.0f }, true } );
            break;
        }
    }
    for ( MeshBuilder *b : { &kit.m_metal, &kit.m_rust, &kit.m_rock } ) b->m_color = WHITE;
}

}   // namespace spaceopera
