/**
 * @file    LevelGarden.cpp
 * @brief   The ring's gardens: low clipped hedges along the paths with lumpy tops, a strip of garden behind them,
 *          forest beyond (and past the level's edge), gravel paths along the route, and the Concordance's bred
 *          plants: parasol trees with glowing pods, lantern willows, coiled topiary, coral tubes, fronds.
 * @author  Rene Weller
 */

#include "Level.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>

namespace spaceopera
{

namespace
{

const int DR[4] = { -1, 0, 1, 0 };   ///< north, east, south, west (as Level.cpp)
const int DC[4] = { 0, 1, 0, -1 };

constexpr int           GardenDepth = 3;    ///< wall cells this deep behind the edge are garden, deeper ones forest
constexpr int           Skirt = 12;         ///< cells of forest beyond the level's edge
constexpr unsigned char Far = 250;          ///< farther than anything counts

/** @brief Dice from a hash: the same plant every time the level is built. */
struct Dice
{
    unsigned m_s;
    float    operator()()
    {
        m_s = m_s * 1664525u + 1013904223u;
        return ( m_s >> 8 ) * ( 1.0f / 16777216.0f );
    }
    float in( float a, float b ) { return a + ( b - a ) * ( *this )(); }
    int   pick( int n ) { return std::min( n - 1, (int)( ( *this )() * n ) ); }
};

// The tints multiply the foliage's grey leaves (as linear colour): greens as dark as the hedges, the bred ones'
// violet, rust, bronze, silver-blue and magenta; the forest darker.
const Color Greens[] = { { 36, 95, 26, 255 }, { 44, 102, 60, 255 }, { 52, 110, 34, 255 }, { 30, 88, 52, 255 },
                         { 58, 104, 30, 255 } };
const Color Exotics[] = { { 87, 51, 116, 255 }, { 124, 51, 29, 255 }, { 109, 80, 22, 255 }, { 60, 96, 125, 255 },
                          { 140, 36, 84, 255 } };
const Color Darks[] = { { 26, 70, 26, 255 }, { 22, 64, 48, 255 }, { 34, 74, 30, 255 }, { 28, 60, 58, 255 } };
const Color DarkExotics[] = { { 58, 38, 82, 255 }, { 90, 40, 26, 255 }, { 40, 66, 92, 255 } };
// Coral tubes and blooms on the gravel's sand (its colour is orange: these bend it).
const Color Corals[] = { { 125, 72, 212, 255 }, { 105, 36, 255, 255 }, { 145, 105, 64, 255 }, { 40, 52, 255, 255 } };
const Vector4 Pods[] = { { 2.6f, 1.4f, 0.35f, 0.0f }, { 0.5f, 2.4f, 2.6f, 0.0f }, { 2.2f, 0.7f, 3.0f, 0.0f } };

Vector3 up( float y ) { return { 0.0f, y, 0.0f }; }

/**
 * @brief The lawn's colour at a point (it multiplies the grass): large patches of lush dark grass, plain grass and dry
 *        straw, one function for all cells, so that no seams and no squares show.
 */
Color meadow( float x, float z, float shade = 1.0f )
{
    float t = 1.6f * ( 0.65f * smoothNoise( { x * 0.11f, 0.5f, z * 0.11f }, 71u ) +
                       0.35f * smoothNoise( { x * 0.31f, 2.5f, z * 0.31f }, 73u ) );
    t = Clamp( t, -1.0f, 1.0f );
    Vector3 lush = { 0.6f, 0.7f, 0.58f }, plain = { 0.86f, 0.86f, 0.82f }, dry = { 1.0f, 0.88f, 0.58f };
    Vector3 c = Vector3Scale( t < 0.0f ? Vector3Lerp( plain, lush, -t ) : Vector3Lerp( plain, dry, t ), shade );
    return { (unsigned char)( c.x * 255.0f ), (unsigned char)( c.y * 255.0f ), (unsigned char)( c.z * 255.0f ), 255 };
}

/**
 * @brief Leaf clusters on cards round a crown or a bush (an ellipsoid), mostly on its top and sides, each half out of
 *        it: the crown ends in leaves, not in a smooth skin. Lit with the crown's own normal there.
 */
void leaves( MeshBuilder &cards, Vector3 centre, Vector3 radii, float turn, int count, Color tone, Dice &dice,
             float scale = 0.42f )
{
    cards.m_color = tone;
    float size = scale * ( radii.x + radii.z ) * 0.5f;
    for ( int k = 0; k < count; ++k )
    {
        float   th = acosf( dice.in( -0.5f, 1.0f ) ), ph = turn + dice.in( 0.0f, 6.28f ), depth = dice.in( 0.72f, 1.0f );
        Vector3 d = { sinf( th ) * cosf( ph ), cosf( th ), sinf( th ) * sinf( ph ) };
        Vector3 p = Vector3Add( centre, { d.x * radii.x * depth, d.y * radii.y * depth, d.z * radii.z * depth } );
        Vector3 n = Vector3Normalize( { d.x / radii.x, d.y / radii.y, d.z / radii.z } );
        Vector3 t1 = Vector3Normalize( Vector3CrossProduct( n, fabsf( n.y ) < 0.9f ? Vector3{ 0.0f, 1.0f, 0.0f }
                                                                                   : Vector3{ 1.0f, 0.0f, 0.0f } ) );
        Vector3 t2 = Vector3CrossProduct( n, t1 );
        float   roll = dice.in( 0.0f, 6.28f ), half = std::max( 0.25f, size * dice.in( 0.8f, 1.2f ) );
        Vector3 u = Vector3Scale( Vector3Add( Vector3Scale( t1, cosf( roll ) ), Vector3Scale( t2, sinf( roll ) ) ), half );
        Vector3 v = Vector3Scale( Vector3Add( Vector3Scale( t1, -sinf( roll ) ), Vector3Scale( t2, cosf( roll ) ) ), half );
        cards.card( Vector3Add( Vector3Subtract( p, u ), v ), Vector3Add( Vector3Add( p, u ), v ),
                    Vector3Subtract( Vector3Add( p, u ), v ), Vector3Subtract( Vector3Subtract( p, u ), v ), n );
    }
    cards.m_color = WHITE;
}

/** @brief A crown core: the same colour, darker (the leaf cards round it carry its outline and its light). */
Color shaded( Color c, float k )
{
    return { (unsigned char)( c.r * k ), (unsigned char)( c.g * k ), (unsigned char)( c.b * k ), c.a };
}

/** @brief A tuft of grass: two cards crossed, standing on the ground (lit as the ground is, so that it sits in it). */
void tuft( MeshBuilder &blades, Vector3 at, float width, float height, float turn, Color tone )
{
    blades.m_color = tone;
    for ( int k = 0; k < 2; ++k )
    {
        float   a = turn + k * PI * 0.5f;
        Vector3 t = { cosf( a ) * width * 0.5f, 0.0f, sinf( a ) * width * 0.5f };
        blades.card( Vector3Add( Vector3Subtract( at, t ), up( height ) ), Vector3Add( Vector3Add( at, t ), up( height ) ),
                     Vector3Add( at, t ), Vector3Subtract( at, t ), { 0.0f, 1.0f, 0.0f } );
    }
    blades.m_color = WHITE;
}

/** @brief A thin box from a to e (a strand, a stalk). */
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

}   // namespace

int Level::garden( int r, int c ) const
{
    if ( r < 0 || c < 0 || r >= m_rows || c >= m_cols || m_garden.empty() ) return 0;
    return m_garden[(size_t)r * m_cols + c];
}

void Level::planGarden( bool on )
{
    size_t cells = (size_t)m_rows * m_cols;
    m_garden.assign( cells, 0 );
    if ( !on ) return;
    // Each wall cell's distance to the nearest open cell (diagonals count: a hedge closes round a room's corners) and
    // that cell's place.
    std::vector<unsigned char> depth( cells, Far );
    std::vector<ThemeE>        owner( cells, ThemeE::SERVICE );
    std::deque<int>            todo;
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
            if ( cell( r, c ).m_kind != CellE::ROCK )
            {
                size_t i = (size_t)r * m_cols + c;
                depth[i] = 0;
                owner[i] = theme( r, c );
                todo.push_back( (int)i );
            }
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
                if ( depth[n] != Far || cell( nr, nc ).m_kind != CellE::ROCK ) continue;
                depth[n] = (unsigned char)std::min( depth[i] + 1, Far - 1 );
                owner[n] = owner[i];
                todo.push_back( (int)n );
            }
    }
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            size_t i = (size_t)r * m_cols + c;
            // Behind the walls of the park's service rooms the garden goes on too (they stand in it as small houses).
            if ( depth[i] == 0 || depth[i] == Far || ( owner[i] != ThemeE::PARK && owner[i] != ThemeE::SERVICE ) ||
                 ( owner[i] == ThemeE::SERVICE && depth[i] < 2 ) )
                continue;
            // A hedge only where every open cell round it is park: the walls of a clinic or an arena stay walls.
            bool park = true;
            for ( int dr = -1; dr <= 1; ++dr )
                for ( int dc = -1; dc <= 1; ++dc )
                {
                    const Cell &k = cell( r + dr, c + dc );
                    if ( k.m_kind != CellE::ROCK && ( theme( r + dr, c + dc ) != ThemeE::PARK || k.m_kind == CellE::VOID ) )
                        park = false;
                }
            if ( park ) m_garden[i] = depth[i];
        }

    // The paths along the route (the autopilot's way: through each corridor, across each room), kept to the middle of
    // the open floor; only in the park. Planned before the build: the lawn keeps its tufts off them.
    m_path.assign( cells, 0 );
    if ( m_route.size() < 2 ) return;
    std::vector<int> wall( cells, 0 );
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
        int a = m_route[w].first * m_cols + m_route[w].second, b = m_route[w + 1].first * m_cols + m_route[w + 1].second;
        if ( a < 0 || b < 0 || a >= (int)cells || b >= (int)cells ) continue;
        std::fill( cost.begin(), cost.end(), 1e9f );
        std::fill( from.begin(), from.end(), -1 );
        // Dijkstra on a small queue: a step costs more near the walls.
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
            int r = i / m_cols, c = i % m_cols;
            if ( theme( r, c ) == ThemeE::PARK && cell( r, c ).m_kind == CellE::FLOOR && cell( r, c ).m_height == 0.0f )
                m_path[(size_t)i] = 1;
            if ( i == a ) break;
        }
    }
}

void Level::gardenCell( int r, int c, GardenKit &kit )
{
    const int      depth = garden( r, c );
    const Vector3  mid = { ( c + 0.5f ) * CellSize, 0.0f, ( r + 0.5f ) * CellSize };
    const unsigned h = hashCell( r, c, 501 );
    Dice           dice{ h };
    if ( depth == 1 )
    {
        // The garden's edge by the paths, as out in the wild: stretches of rock (boulders heaped, now and then a crag)
        // and stretches of thicket (bushes, a tree among them), one or the other along the edge as a slow noise has
        // it. A dark core closes the gaps at the foot; the lumps of the cells beside run into each other.
        float turn = dice.in( 0.0f, 6.28f );
        lawn( r, c, kit );   // the ground under it (between the lumps it shows)
        if ( smoothNoise( { mid.x * 0.07f, 1.5f, mid.z * 0.07f }, 61u ) > -0.05f )
        {
            // Boulders wider than high, sunk into the ground, now and then one lying on the others or a crag.
            int g = 175 + (int)( h % 50 );
            for ( int i = 0; i < 2; ++i )
            {
                Vector3 rr = { dice.in( 0.95f, 1.3f ), dice.in( 0.7f, 1.25f ), dice.in( 0.8f, 1.1f ) };
                Vector3 p = { mid.x + dice.in( -0.2f, 0.2f ), rr.y * dice.in( 0.2f, 0.4f ), mid.z + dice.in( -0.2f, 0.2f ) };
                if ( i == 1 && dice() < 0.4f )
                {
                    rr = Vector3Scale( rr, 0.65f );   // lying on the first
                    p.y += 0.9f;
                }
                else if ( dice() < 0.1f )
                {
                    rr = { 1.2f, dice.in( 1.7f, 2.2f ), 1.15f };   // a crag
                    p.y = rr.y * 0.45f;
                }
                kit.m_stone.m_color = { (unsigned char)g, (unsigned char)( g + 2 ), (unsigned char)( g - 8 ), 255 };
                // Few facets and strong lumps: a stone's planes and edges, not a smooth egg.
                kit.m_stone.blob( place( p, { dice.in( -0.35f, 0.35f ), turn + i * 1.7f, dice.in( -0.35f, 0.35f ) } ), rr,
                                  0.3f, h + i, 4 );
                stand( p, rr, p.y + rr.y );
                // Moss and a bush in the clefts on top of some.
                if ( dice() < 0.35f )
                {
                    Vector3 top = Vector3Add( p, up( rr.y * 0.75f ) );
                    Vector3 bush = { 0.55f, 0.4f, 0.55f };
                    Color   tone = Greens[dice.pick( 5 )];
                    kit.m_foliage.m_color = tone;
                    kit.m_foliage.blob( place( top ), bush, 0.35f, h + 9u + i, 4 );
                    kit.m_foliage.m_color = WHITE;
                    leaves( kit.m_cards, top, bush, turn, 3, tone, dice );
                    stand( top, bush, top.y + bush.y );
                }
            }
            kit.m_stone.m_color = WHITE;
        }
        else
        {
            Color tone = dice() < 0.15f ? Exotics[dice.pick( 5 )] : Greens[dice.pick( 5 )];
            for ( int i = 0; i < 2; ++i )
            {
                Vector3 rr = { dice.in( 0.85f, 1.05f ), dice.in( 0.9f, 1.4f ), dice.in( 0.85f, 1.05f ) };
                Vector3 p = { mid.x + dice.in( -0.25f, 0.25f ), rr.y * 0.65f, mid.z + dice.in( -0.25f, 0.25f ) };
                kit.m_foliage.m_color = shaded( tone, 0.75f );
                kit.m_foliage.blob( place( p, { 0.0f, turn + i, 0.0f } ), Vector3Scale( rr, 0.85f ), 0.35f, h + i, 5 );
                kit.m_foliage.m_color = WHITE;
                leaves( kit.m_cards, p, rr, turn, 10, tone, dice, 0.32f );
                stand( p, rr, p.y + rr.y );
            }
            // Now and then a tree in the thicket, its crown over the path.
            if ( h % 3 == 0 )
                plant( dice() < 0.5f ? PlantE::CANOPY : PlantE::PARASOL, { mid.x, 0.0f, mid.z }, dice.in( 0.6f, 0.75f ),
                       h, kit );
        }
        kit.m_stone.m_color = WHITE;
        return;
    }
    // Behind the edge and in the forest: lawn (never walked on; only its top is ever seen; the crowns shade it).
    lawn( r, c, kit );
    if ( depth > GardenDepth )
    {
        // Forest: a tree to each block of two by two cells; deeper in, where only their tops are seen, plainer.
        if ( r % 2 == 0 && c % 2 == 0 )
        {
            Vector3 at = { mid.x + CellSize * 0.5f + dice.in( -0.7f, 0.7f ), 0.0f,
                           mid.z + CellSize * 0.5f + dice.in( -0.7f, 0.7f ) };
            plant( PlantE::CANOPY, at, dice.in( 0.9f, 1.2f ), h, kit, depth <= GardenDepth + 2 );
        }
        return;
    }
    // The garden: a tree in some blocks of three by three cells (at most one), else bushes, beds of blooms, fronds,
    // coral tubes, lawn.
    int      br = r - r % 3, bc = c - c % 3;
    unsigned bh = hashCell( br, bc, 503 );
    Vector3  at = { mid.x + dice.in( -0.45f, 0.45f ), 0.0f, mid.z + dice.in( -0.45f, 0.45f ) };
    if ( r == br + (int)( bh % 3 ) && c == bc + (int)( ( bh >> 4 ) % 3 ) && ( bh >> 8 ) % 10 < 7 )
    {
        static const PlantE Trees[] = { PlantE::PARASOL, PlantE::PARASOL, PlantE::LANTERN, PlantE::ORBS,
                                        PlantE::SPIRAL,  PlantE::CANOPY };
        plant( Trees[( bh >> 12 ) % 6], at, dice.in( 0.85f, 1.15f ), h, kit );
        return;
    }
    for ( int i = 0; i < 2; ++i )
        tuft( kit.m_blades, { mid.x + dice.in( -0.9f, 0.9f ), 0.0f, mid.z + dice.in( -0.9f, 0.9f ) },
              dice.in( 0.55f, 0.85f ), dice.in( 0.4f, 0.7f ), dice.in( 0.0f, 6.28f ), meadow( mid.x, mid.z ) );
    float roll = dice();
    if ( roll < 0.24f )
        plant( PlantE::BUSH, at, dice.in( 0.8f, 1.3f ), h, kit );
    else if ( roll < 0.34f )
        plant( PlantE::FRONDS, at, dice.in( 0.8f, 1.2f ), h, kit );
    else if ( roll < 0.42f )
        plant( PlantE::TUBES, at, dice.in( 0.8f, 1.2f ), h, kit );
    else if ( roll < 0.5f )
        plant( PlantE::BED, at, dice.in( 0.8f, 1.1f ), h, kit );
}

void Level::lawn( int r, int c, GardenKit &kit, float shade )
{
    static const std::vector<float> Steps = { 0.0f, 1.0f, 2.0f };
    kit.m_park( r, c, false )
        .field( c * CellSize, r * CellSize, Steps, []( float, float ) { return 0.0f; },
                [shade]( float x, float z ) { return meadow( x, z, shade ); } );
}

void Level::groundTop( int r, int c, float h, GardenKit &kit )
{
    // The lawn's top in its patches; by a path a little trodden, drier and darker.
    auto colour = [&]( float x, float z )
    {
        Color m = meadow( x, z );
        float near = 9.0f;
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
            {
                int rr = (int)floorf( z / CellSize ) + dr, cc = (int)floorf( x / CellSize ) + dc;
                if ( rr < 0 || cc < 0 || rr >= m_rows || cc >= m_cols || !m_path[(size_t)rr * m_cols + cc] ) continue;
                near = std::min( near, Vector2Distance( { x, z }, { ( cc + 0.5f ) * CellSize, ( rr + 0.5f ) * CellSize } ) );
            }
        float   t = Clamp( 1.0f - ( near - 1.0f ) / 1.4f, 0.0f, 1.0f ) * 0.45f;
        Vector3 trodden = { 0.8f, 0.68f, 0.48f };
        return Color{ (unsigned char)Lerp( m.r, trodden.x * 255.0f, t ), (unsigned char)Lerp( m.g, trodden.y * 255.0f, t ),
                      (unsigned char)Lerp( m.b, trodden.z * 255.0f, t ), 255 };
    };
    static const std::vector<float> Steps = { 0.0f, 1.0f, 2.0f };
    kit.m_park( r, c, false ).field( c * CellSize, r * CellSize, Steps, [h]( float, float ) { return h; }, colour );
    // Tufts of grass, more by the edges than in the middle, none on the path.
    if ( m_path[(size_t)r * m_cols + c] ) return;
    Dice dice{ hashCell( r, c, 513 ) };
    int  edge = 0;
    for ( int d = 0; d < 4; ++d ) edge += garden( r + DR[d], c + DC[d] ) == 1;
    int n = 1 + dice.pick( 3 ) + edge * 2;
    for ( int i = 0; i < n; ++i )
    {
        Vector3 p = { ( c + dice.in( 0.1f, 0.9f ) ) * CellSize, h, ( r + dice.in( 0.1f, 0.9f ) ) * CellSize };
        tuft( kit.m_blades, p, dice.in( 0.5f, 0.8f ), dice.in( 0.35f, 0.65f ), dice.in( 0.0f, 6.28f ), colour( p.x, p.z ) );
    }
}

void Level::hedgeFoot( int r, int c, int d, GardenKit &kit )
{
    // Low and flat: the hero's feet go through them like through tall grass.
    Dice dice{ hashCell( r, c * 4 + d, 511 ) };
    if ( dice() > 0.45f ) return;
    Vector3 n = { (float)-DC[d], 0.0f, (float)-DR[d] }, along = { n.z, 0.0f, -n.x };
    Vector3 face = { ( c + 0.5f + DC[d] * 0.5f ) * CellSize, 0.0f, ( r + 0.5f + DR[d] * 0.5f ) * CellSize };
    Color   bloom = dice() < 0.35f ? Exotics[dice.pick( 5 )] : Greens[dice.pick( 5 )];
    int     n2 = 2 + dice.pick( 3 );
    for ( int i = 0; i < n2; ++i )
    {
        float   rr = dice.in( 0.18f, 0.3f );
        Vector3 p = Vector3Add( face, Vector3Add( Vector3Scale( n, dice.in( 0.1f, 0.3f ) ),
                                                  Vector3Scale( along, dice.in( -0.8f, 0.8f ) ) ) );
        p.y = rr * 0.25f;
        kit.m_foliage.m_color = i % 2 ? bloom : Greens[dice.pick( 5 )];
        kit.m_foliage.blob( place( p, { 0.0f, dice.in( 0.0f, 6.28f ), 0.0f } ), { rr, rr * 0.6f, rr }, 0.3f,
                            dice.m_s, 3 );
    }
    kit.m_foliage.m_color = WHITE;
}

void Level::gardenEdges( GardenKit &kit )
{
    // Beyond the level's edge where a garden reaches it: more forest, so that the land of the ring far below does not
    // show at the edge.
    for ( int r = -Skirt; r < m_rows + Skirt; ++r )
        for ( int c = -Skirt; c < m_cols + Skirt; ++c )
        {
            if ( r >= 0 && c >= 0 && r < m_rows && c < m_cols ) continue;
            int er = std::clamp( r, 0, m_rows - 1 ), ec = std::clamp( c, 0, m_cols - 1 );
            if ( garden( er, ec ) == 0 ) continue;
            Vector3 mid = { ( c + 0.5f ) * CellSize, 0.0f, ( r + 0.5f ) * CellSize };
            lawn( r, c, kit );
            if ( ( r & 1 ) || ( c & 1 ) ) continue;
            Dice    dice{ hashCell( r, c, 501 ) };
            Vector3 at = { mid.x + CellSize * 0.5f + dice.in( -0.7f, 0.7f ), 0.0f,
                           mid.z + CellSize * 0.5f + dice.in( -0.7f, 0.7f ) };
            bool    near = garden( er, ec ) <= GardenDepth + 1 && std::max( abs( r - er ), abs( c - ec ) ) <= 2;
            plant( PlantE::CANOPY, at, dice.in( 0.9f, 1.25f ), dice.m_s, kit, near );
        }
}

void Level::gardenPaths( const LevelSurfaces &s )
{
    // Decals of trodden earth along the paths, one to a cell and wider than it, turned every which way and fading at
    // their rims (the next one's covers the gap); here and there a patch of earth in the grass.
    if ( !s.m_decals || m_path.empty() ) return;
    int earth[2] = { -1, -1 };
    for ( size_t i = 0; i < s.m_decals->size(); ++i )
        for ( int k = 0; k < 2; ++k )
            if ( ( *s.m_decals )[i].m_name == ( k ? "path_2" : "path_1" ) ) earth[k] = (int)i;
    if ( earth[0] < 0 || earth[1] < 0 ) return;
    auto lay = [&]( int r, int c, float size, Dice &dice, float dr, float dc )
    {
        static int layer = 0;
        Vector3    mid = { ( c + 0.5f + dc + dice.in( -0.12f, 0.12f ) ) * CellSize, 0.012f + 0.002f * ( layer++ % 4 ),
                           ( r + 0.5f + dr + dice.in( -0.12f, 0.12f ) ) * CellSize };
        float   yaw = dice.in( 0.0f, 6.28f ), half = size * 0.5f;
        Vector3 right = { cosf( yaw ) * half, 0.0f, sinf( yaw ) * half };
        Vector3 down = { -sinf( yaw ) * half, 0.0f, cosf( yaw ) * half };
        Decal   d;
        d.m_def = earth[dice.pick( 2 )];
        d.m_corner[0] = Vector3Subtract( Vector3Subtract( mid, right ), down );
        d.m_corner[1] = Vector3Subtract( Vector3Add( mid, right ), down );
        d.m_corner[2] = Vector3Add( Vector3Add( mid, right ), down );
        d.m_corner[3] = Vector3Add( Vector3Subtract( mid, right ), down );
        d.m_cut = false;
        m_decals.push_back( d );
    };
    auto onPath = [&]( int r, int c )
    { return r >= 0 && c >= 0 && r < m_rows && c < m_cols && m_path[(size_t)r * m_cols + c]; };
    for ( int r = 0; r < m_rows; ++r )
        for ( int c = 0; c < m_cols; ++c )
        {
            Dice dice{ hashCell( r, c, 517 ) };
            if ( onPath( r, c ) )
            {
                lay( r, c, dice.in( 2.4f, 2.8f ), dice, 0.0f, 0.0f );
                // and halfway to the next cell of the path, so that the way has no gaps
                for ( int d = 1; d <= 2; ++d )
                    if ( onPath( r + DR[d], c + DC[d] ) ) lay( r, c, dice.in( 2.2f, 2.6f ), dice, DR[d] * 0.5f, DC[d] * 0.5f );
            }
            else if ( theme( r, c ) == ThemeE::PARK && cell( r, c ).m_kind == CellE::FLOOR && dice() < 0.035f )
                lay( r, c, dice.in( 1.3f, 2.0f ), dice, 0.0f, 0.0f );
        }
}

void Level::plant( PlantE kind, Vector3 at, float s, unsigned h, GardenKit &kit, bool full )
{
    Dice         dice{ h * 2654435761u + 977u };
    MeshBuilder &leaf = kit.m_foliage, &bark = kit.m_bark;
    int          r = (int)floorf( at.z / CellSize ), c = (int)floorf( at.x / CellSize );
    auto         glow = [&]( Vector3 p, float half, Vector4 colour )
    { kit.m_glows.push_back( { p, { half, half * 1.3f, half }, colour, true } ); };
    // A trunk from the foot to `top`, bending through `bend` (sideways at its middle), thinner upwards.
    auto trunk = [&]( Vector3 top, Vector3 bend, float r0, float r1, int sides )
    {
        std::vector<Vector3> p;
        std::vector<float>   rad;
        for ( int i = 0; i <= 4; ++i )
        {
            float t = i / 4.0f;
            p.push_back( Vector3Add( Vector3Lerp( at, top, t ), Vector3Scale( bend, sinf( t * PI ) ) ) );
            rad.push_back( Lerp( r0, r1, t ) );
        }
        p.front().y -= 0.2f;   // into the ground
        bark.tube( p, rad, sides );
    };
    bool odd = dice() < 0.22f;   // one of the bred colours rather than green
    switch ( kind )
    {
    case PlantE::PARASOL:
    {
        // A slender trunk, a few short branches spreading at its top, a wide flat crown over them, pods glowing under
        // its rim.
        Vector3 top = Vector3Add( at, { dice.in( -0.5f, 0.5f ) * s, dice.in( 3.3f, 4.2f ) * s, dice.in( -0.5f, 0.5f ) * s } );
        trunk( top, { dice.in( -0.3f, 0.3f ) * s, 0.0f, dice.in( -0.3f, 0.3f ) * s }, 0.22f * s, 0.13f * s, 7 );
        leaf.m_color = odd ? Exotics[dice.pick( 5 )] : Greens[1 + dice.pick( 3 )];
        float turn = dice.in( 0.0f, 6.28f );
        int   n = 3 + dice.pick( 3 );
        for ( int k = 0; k < n; ++k )
        {
            float   a = turn + k * 2.0f * PI / n + dice.in( -0.3f, 0.3f ), reach = dice.in( 1.1f, 1.6f ) * s;
            Vector3 tip = Vector3Add( top, { cosf( a ) * reach, dice.in( 0.3f, 0.6f ) * s, sinf( a ) * reach } );
            bark.tube( { Vector3Subtract( top, up( 0.5f * s ) ), Vector3Lerp( top, tip, 0.5f ), tip },
                       { 0.09f * s, 0.06f * s, 0.035f * s }, 5 );
            Color tone = leaf.m_color;
            leaf.m_color = shaded( tone, 0.72f );
            leaf.blob( place( Vector3Add( tip, up( 0.2f * s ) ), { 0.0f, a, 0.0f } ),
                       { 0.85f * s, 0.34f * s, 0.8f * s }, 0.35f, h + k, 5 );
            leaf.m_color = tone;
            leaves( kit.m_cards, Vector3Add( tip, up( 0.2f * s ) ), { 1.1f * s, 0.42f * s, 1.0f * s }, a, 9, tone, dice,
                    0.3f );
            hang( tip, { 1.1f * s, 0.42f * s, 1.0f * s }, tip.y - 0.2f * s, tip.y + 0.65f * s );
        }
        Color tone = leaf.m_color;
        leaf.m_color = shaded( tone, 0.72f );
        leaf.blob( place( Vector3Add( top, up( 0.45f * s ) ), { 0.0f, turn, 0.0f } ), { 2.1f * s, 0.45f * s, 2.0f * s },
                   0.3f, h, 7 );
        leaf.m_color = tone;
        leaves( kit.m_cards, Vector3Add( top, up( 0.45f * s ) ), { 2.5f * s, 0.6f * s, 2.4f * s }, turn, 30, tone, dice,
                0.24f );
        hang( top, { 2.5f * s, 0.55f * s, 2.4f * s }, top.y + 0.1f * s, top.y + 1.0f * s );
        leaf.m_color = WHITE;
        Vector4 pod = Pods[dice.pick( 3 )];
        for ( int k = 0; k < 6; ++k )
        {
            float a = dice.in( 0.0f, 6.28f ), d = dice.in( 1.0f, 2.1f ) * s;
            glow( Vector3Add( top, { cosf( a ) * d, dice.in( -0.6f, -0.1f ) * s, sinf( a ) * d } ), 0.07f * s, pod );
        }
        break;
    }
    case PlantE::LANTERN:
    {
        // A bent trunk under a dome of silvery leaves; strands hang from its rim, a light at each end.
        Vector3 top = Vector3Add( at, { dice.in( -0.3f, 0.3f ) * s, dice.in( 2.7f, 3.3f ) * s, dice.in( -0.3f, 0.3f ) * s } );
        trunk( top, { dice.in( -0.5f, 0.5f ) * s, 0.0f, dice.in( -0.5f, 0.5f ) * s }, 0.2f * s, 0.12f * s, 7 );
        Color tone = odd ? Exotics[dice.pick( 5 )] : Color{ 74, 82, 98, 255 };
        leaf.m_color = shaded( tone, 0.72f );
        leaf.blob( place( Vector3Add( top, up( 0.3f * s ) ), { 0.0f, dice.in( 0.0f, 6.28f ), 0.0f } ),
                   { 1.5f * s, 0.85f * s, 1.5f * s }, 0.3f, h, 6 );
        leaf.m_color = tone;
        leaves( kit.m_cards, Vector3Add( top, up( 0.3f * s ) ), { 1.75f * s, 1.0f * s, 1.75f * s }, 0.0f, 22, tone,
                dice, 0.26f );
        hang( top, { 1.75f * s, 1.0f * s, 1.75f * s }, top.y - 0.5f * s, top.y + 1.3f * s );
        Vector4 light = Pods[1 + dice.pick( 2 )];
        int     n = 10 + dice.pick( 5 );
        for ( int k = 0; k < n; ++k )
        {
            float   a = k * 2.0f * PI / n + dice.in( -0.15f, 0.15f ), d = dice.in( 1.3f, 1.65f ) * s;
            Vector3 root = Vector3Add( top, { cosf( a ) * d, -0.2f * s, sinf( a ) * d } );
            Vector3 end = Vector3Add( root, { cosf( a ) * 0.2f * s, -dice.in( 1.0f, 1.9f ) * s, sinf( a ) * 0.2f * s } );
            rod( leaf, root, end, 0.022f * s );
            glow( end, 0.055f * s, light );
        }
        leaf.m_color = WHITE;
        break;
    }
    case PlantE::SPIRAL:
    {
        // A coil of clipped leaves wound up round a stalk, a ball on top.
        MeshBuilder &hedge = kit.m_park( r, c, true );
        float        hgt = dice.in( 2.4f, 3.2f ) * s, phase = dice.in( 0.0f, 6.28f ), turns = dice.in( 2.3f, 3.0f );
        bark.tube( { at, Vector3Add( at, up( hgt ) ) }, { 0.07f * s, 0.05f * s }, 6 );
        std::vector<Vector3> coil;
        std::vector<float>   rad;
        for ( int i = 0; i <= 40; ++i )
        {
            float t = i / 40.0f, a = phase + t * turns * 2.0f * PI, R = ( 0.62f - 0.45f * t ) * s;
            coil.push_back( Vector3Add( at, { cosf( a ) * R, 0.3f * s + t * ( hgt - 0.5f * s ), sinf( a ) * R } ) );
            rad.push_back( ( 0.3f - 0.17f * t ) * s );
        }
        hedge.m_color = { 220, 235, 215, 255 };
        hedge.tube( coil, rad, 7 );
        hedge.blob( place( Vector3Add( at, up( hgt ) ) ), { 0.26f * s, 0.26f * s, 0.26f * s }, 0.1f, h, 5 );
        stand( at, { 0.62f * s, 0.0f, 0.62f * s }, hgt + 0.26f * s );
        hedge.m_color = WHITE;
        break;
    }
    case PlantE::ORBS:
    {
        // Balls of clipped leaves stacked on a straight trunk.
        MeshBuilder &hedge = kit.m_park( r, c, true );
        bark.tube( { Vector3Subtract( at, up( 0.2f ) ), Vector3Add( at, up( 2.7f * s ) ) }, { 0.12f * s, 0.07f * s }, 7 );
        const float ys[3] = { 1.05f, 1.95f, 2.7f }, rs[3] = { 0.8f, 0.6f, 0.4f };
        hedge.m_color = { 215, 230, 210, 255 };
        for ( int i = 0; i < 3; ++i )
            hedge.blob( place( Vector3Add( at, up( ys[i] * s ) ), { 0.0f, i * 1.3f, 0.0f } ),
                        { rs[i] * s, rs[i] * 0.92f * s, rs[i] * s }, 0.1f, h + i, 7 );
        stand( at, { 0.8f * s, 0.0f, 0.8f * s }, 3.1f * s );
        hedge.m_color = WHITE;
        break;
    }
    case PlantE::TUBES:
    {
        // Open tubes of a coral's colour, flaring at their mouths; some lit inside.
        Color tone = Corals[dice.pick( 4 )];
        int   n = 5 + dice.pick( 5 );
        kit.m_gravel.m_color = tone;
        for ( int k = 0; k < n; ++k )
        {
            float   a = dice.in( 0.0f, 6.28f ), d = dice.in( 0.0f, 0.45f ) * s;
            Vector3 foot = Vector3Add( at, { cosf( a ) * d, 0.0f, sinf( a ) * d } );
            float   tall = dice.in( 0.45f, 1.5f ) * s, r0 = dice.in( 0.06f, 0.11f ) * s, r1 = r0 * dice.in( 1.5f, 2.2f );
            kit.m_gravel.lathe( place( foot, { cosf( a ) * 0.15f, 0.0f, sinf( a ) * 0.15f } ),
                                { { 0.0f, -0.1f },
                                  { r0, -0.1f },
                                  { r0 * 0.85f, tall * 0.55f },
                                  { r1, tall },
                                  { r1 * 0.8f, tall },
                                  { r1 * 0.55f, tall - 0.12f * s } },
                                7 );
            if ( dice() < 0.35f )
                glow( Vector3Add( foot, { cosf( a ) * 0.15f * tall, tall - 0.06f * s, sinf( a ) * 0.15f * tall } ),
                      r1 * 0.45f, { 2.4f, 0.5f, 1.2f, 0.0f } );
        }
        kit.m_gravel.m_color = WHITE;
        stand( at, { 0.5f * s, 0.0f, 0.5f * s }, 1.5f * s );
        break;
    }
    case PlantE::FRONDS:
    {
        // Long blades out of the ground, rising and drooping at their tips; a ridge down each.
        leaf.m_color = odd ? Exotics[dice.pick( 3 )] : Greens[dice.pick( 5 )];
        int   n = 7 + dice.pick( 6 );
        float turn = dice.in( 0.0f, 6.28f );
        for ( int k = 0; k < n; ++k )
        {
            float   a = turn + k * 2.0f * PI / n + dice.in( -0.2f, 0.2f ), len = dice.in( 1.0f, 1.7f ) * s,
                  rise = dice.in( 0.5f, 0.9f ) * len;
            Vector3 dir = { cosf( a ), 0.0f, sinf( a ) }, side = { -sinf( a ), 0.0f, cosf( a ) };
            std::vector<std::vector<Vector3>> rings;
            for ( int i = 0; i <= 6; ++i )
            {
                float   t = i / 6.0f, y = rise * ( 1.0f - ( 1.0f - t ) * ( 1.0f - t ) ) - 0.7f * rise * t * t * t;
                float   w = 0.17f * s * powf( sinf( PI * ( 0.12f + 0.88f * t ) ), 0.7f ) * ( 1.0f - 0.3f * t ) + 0.005f;
                Vector3 p = Vector3Add( at, Vector3Add( Vector3Scale( dir, len * t ), up( y ) ) );
                rings.push_back(
                    { Vector3Subtract( p, Vector3Scale( side, w ) ), Vector3Add( p, up( w * 0.3f ) ), Vector3Add( p, Vector3Scale( side, w ) ) } );
            }
            leaf.loft( MatrixIdentity(), rings, false );
        }
        leaf.m_color = WHITE;
        break;
    }
    case PlantE::BUSH:
    {
        int n = 2 + dice.pick( 3 );
        leaf.m_color = odd ? Exotics[dice.pick( 5 )] : Greens[dice.pick( 5 )];
        for ( int k = 0; k < n; ++k )
        {
            Vector3 rr = { dice.in( 0.45f, 0.8f ) * s, dice.in( 0.4f, 0.65f ) * s, dice.in( 0.45f, 0.8f ) * s };
            Vector3 p = Vector3Add( at, { dice.in( -0.5f, 0.5f ) * s, rr.y * 0.7f, dice.in( -0.5f, 0.5f ) * s } );
            Color tone = leaf.m_color;
            leaf.m_color = shaded( tone, 0.75f );
            leaf.blob( place( p, { 0.0f, dice.in( 0.0f, 6.28f ), 0.0f } ), Vector3Scale( rr, 0.85f ), 0.35f, h + k, 5 );
            leaf.m_color = tone;
            leaves( kit.m_cards, p, rr, 0.0f, 6, tone, dice, 0.34f );
            stand( p, rr, p.y + rr.y );
        }
        leaf.m_color = WHITE;
        break;
    }
    case PlantE::BED:
    {
        // A low patch of blooms in two colours among their leaves.
        Color bloom = Exotics[dice.pick( 5 )], green = Greens[dice.pick( 5 )];
        bloom = { (unsigned char)std::min( 255, bloom.r * 2 ), (unsigned char)std::min( 255, bloom.g * 2 ),
                  (unsigned char)std::min( 255, bloom.b * 2 ), 255 };
        for ( int k = 0; k < 6; ++k )
        {
            leaf.m_color = k % 3 ? bloom : green;
            float rr = dice.in( 0.22f, 0.34f ) * s;
            leaf.blob( place( Vector3Add( at, { dice.in( -0.65f, 0.65f ) * s, rr * 0.5f, dice.in( -0.65f, 0.65f ) * s } ) ),
                       { rr, rr * 0.7f, rr }, 0.3f, h + k, 3 );
        }
        leaf.m_color = WHITE;
        break;
    }
    default:   // CANOPY
    {
        // A forest tree: a short trunk into a big dark crown and a clump beside it, low and wide, so that the crowns
        // close over the forest; deep in it (`full` false) only the crown, a little wider. Their tops stay below the
        // game's camera, eight metres at most.
        float   hgt = dice.in( 3.2f, 4.2f ) * s;
        Vector3 top = Vector3Add( at, { dice.in( -0.4f, 0.4f ), hgt, dice.in( -0.4f, 0.4f ) } );
        if ( full )
            bark.tube( { Vector3Subtract( at, up( 0.2f ) ), Vector3Lerp( at, top, 0.5f ), top },
                       { 0.34f * s, 0.27f * s, 0.2f * s }, 6 );
        leaf.m_color = dice() < 0.12f ? DarkExotics[dice.pick( 3 )] : Darks[dice.pick( 4 )];
        Vector3 crown = { dice.in( 2.6f, 3.3f ) * s, dice.in( 1.3f, 1.7f ) * s, dice.in( 2.6f, 3.3f ) * s };
        float   turn = dice.in( 0.0f, 6.28f );
        if ( !full ) crown = Vector3Scale( crown, 1.15f );
        MeshBuilder &crowns = full ? leaf : kit.m_canopy;   // deep in the forest they throw no shadows (none is seen)
        crowns.m_color = shaded( leaf.m_color, 0.75f );
        crowns.blob( place( Vector3Add( top, up( 0.8f * s ) ), { 0.0f, turn, 0.0f } ), Vector3Scale( crown, 0.85f ), 0.3f,
                     h, full ? 5 : 4 );
        kit.m_canopy.m_color = WHITE;
        leaves( kit.m_cards, Vector3Add( top, up( 0.8f * s ) ), crown, turn, full ? 22 : 9, leaf.m_color, dice,
                full ? 0.26f : 0.34f );
        hang( top, crown, top.y + 0.8f * s - crown.y * 0.6f, top.y + 0.8f * s + crown.y );
        if ( full )
        {
            float   a = turn + dice.in( -0.4f, 0.4f ), d = crown.x * dice.in( 0.55f, 0.8f );
            Vector3 p = Vector3Add( top, { cosf( a ) * d, dice.in( 0.0f, 0.9f ) * s, sinf( a ) * d } );
            Vector3 clump = Vector3Scale( crown, dice.in( 0.5f, 0.65f ) );
            Color   tone = leaf.m_color;
            leaf.m_color = shaded( tone, 0.75f );
            leaf.blob( place( p, { 0.0f, a, 0.0f } ), Vector3Scale( clump, 0.85f ), 0.35f, h + 7u, 4 );
            leaf.m_color = tone;
            leaves( kit.m_cards, p, clump, a, 8, tone, dice, 0.3f );
        }
        leaf.m_color = WHITE;
        break;
    }
    }
}

}   // namespace spaceopera
