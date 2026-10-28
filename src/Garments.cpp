/**
 * @file Garments.cpp
 * @brief Clothes over the skinned bodies, fitted to each body's outline.
 * @author Rene Weller
 */
#include "Garments.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace spaceopera
{

namespace
{

constexpr int   Angles = 32;   ///< a section's samples round its centre
constexpr float Turn = 2.0f * PI;

/** @brief The body's outline at a height: how far out its skin is at each angle round a centre. */
struct Section
{
    Vector2            m_centre = { 0.0f, 0.0f };   ///< x, z
    float              m_y = 0.0f;                  ///< the height
    std::vector<float> m_r;                         ///< per angle (from +z, the front, towards +x, the left)

    /** @return the point at an angle (radians), `out` beyond the skin */
    Vector3 at( float angle, float out ) const
    {
        float f = angle / Turn * Angles;
        f -= floorf( f / Angles ) * Angles;
        int   i = (int)f % Angles;
        float t = f - floorf( f );
        float r = m_r[i] * ( 1.0f - t ) + m_r[( i + 1 ) % Angles] * t + out;
        return { m_centre.x + sinf( angle ) * r, m_y, m_centre.y + cosf( angle ) * r };
    }

    /** @return the same outline at another height */
    Section moved( float y ) const
    {
        Section s = *this;
        s.m_y = y;
        return s;
    }
};

/** @brief Measures the body's skin in its rest pose. */
class Measure
{
public:
    explicit Measure( const SkinModel &m ) : m_model( m )
    {
        // The body is its biggest skinned mesh (the eyes and brows are small and rigid).
        const Mesh *body = nullptr;
        for ( int i = 0; i < m.m_body.meshCount; ++i )
            if ( m.m_skinned[i] && ( !body || m.m_body.meshes[i].vertexCount > body->vertexCount ) )
                body = &m.m_body.meshes[i];
        if ( !body || !body->vertices ) return;
        for ( int i = 0; i < body->vertexCount; ++i )
        {
            Vector3 v = { body->vertices[i * 3], body->vertices[i * 3 + 1], body->vertices[i * 3 + 2] };
            m_verts.push_back( v );
            m_top = std::max( m_top, v.y );
        }
    }

    /** @return a bone's place in the rest pose */
    Vector3 bone( const char *name ) const
    {
        int i = m_model.bone( name );
        return i >= 0 ? m_model.m_body.bindPose[i].translation : Vector3{ 0.0f, 0.0f, 0.0f };
    }

    /** @return the top of the head */
    float top() const { return m_top; }

    /**
     * @brief The outline at a height of what lies between two x (a leg, the torso without the arms).
     * @param[in] y     the height
     * @param[in] cx    the centre's x (its z is the middle of what is there)
     * @param[in] xMin  from
     * @param[in] xMax  to
     * @return the outline
     */
    Section section( float y, float cx, float xMin, float xMax ) const
    {
        Section s;
        s.m_y = y;
        float zMin = 1e9f, zMax = -1e9f;
        for ( Vector3 v : m_verts )
            if ( fabsf( v.y - y ) < Band && v.x >= xMin && v.x <= xMax )
            {
                zMin = std::min( zMin, v.z );
                zMax = std::max( zMax, v.z );
            }
        s.m_centre = { cx, zMin < zMax ? ( zMin + zMax ) * 0.5f : 0.0f };
        s.m_r.assign( Angles, -1.0f );
        for ( Vector3 v : m_verts )
        {
            if ( fabsf( v.y - y ) >= Band || v.x < xMin || v.x > xMax ) continue;
            float dx = v.x - s.m_centre.x, dz = v.z - s.m_centre.y;
            float a = atan2f( dx, dz );
            int   i = ( (int)lroundf( ( a < 0.0f ? a + Turn : a ) / Turn * Angles ) ) % Angles;
            s.m_r[i] = std::max( s.m_r[i], sqrtf( dx * dx + dz * dz ) );
        }
        // Angles with nothing from their neighbours; dents filled (cloth bridges them).
        std::vector<float> r = s.m_r;
        for ( int i = 0; i < Angles; ++i )
        {
            if ( r[i] >= 0.0f ) continue;
            int back = 1, ahead = 1;
            while ( back < Angles && r[( i - back + Angles ) % Angles] < 0.0f ) ++back;
            while ( ahead < Angles && r[( i + ahead ) % Angles] < 0.0f ) ++ahead;
            if ( back >= Angles )
            {
                s.m_r.assign( Angles, 0.1f );
                return s;
            }
            float a = r[( i - back + Angles ) % Angles], b = r[( i + ahead ) % Angles];
            s.m_r[i] = a + ( b - a ) * back / (float)( back + ahead );
        }
        for ( int pass = 0; pass < 4; ++pass )
            for ( int i = 0; i < Angles; ++i )
                s.m_r[i] =
                    std::max( s.m_r[i], 0.5f * ( s.m_r[( i + Angles - 1 ) % Angles] + s.m_r[( i + 1 ) % Angles] ) );
        return s;
    }

    /** @return the highest skin near a point (x, z) below the head (the shoulders' top) */
    float topAt( float x, float z, float below ) const
    {
        float y = 0.0f;
        for ( Vector3 v : m_verts )
            if ( v.y < below && fabsf( v.x - x ) < 0.02f && fabsf( v.z - z ) < 0.04f ) y = std::max( y, v.y );
        return y;
    }

private:
    static constexpr float Band = 0.02f;   ///< how far above and below a height counts

    const SkinModel     &m_model;
    std::vector<Vector3> m_verts;
    float                m_top = 0.0f;
};

/** @brief An arc of an outline, `steps` + 1 points from `from` to `to` (radians), `out` beyond the skin. */
std::vector<Vector3> arc( const Section &s, float out, float from, float to, int steps )
{
    std::vector<Vector3> ring;
    for ( int i = 0; i <= steps; ++i ) ring.push_back( s.at( from + ( to - from ) * i / steps, out ) );
    return ring;
}

/** @brief A whole outline, `steps` points (the loft closes it). */
std::vector<Vector3> whole( const Section &s, float out, int steps )
{
    std::vector<Vector3> ring;
    for ( int i = 0; i < steps; ++i ) ring.push_back( s.at( Turn * i / steps, out ) );
    return ring;
}

/** @brief A strap along points: flat boxes from one to the next, lying on the body (outward from `centre`). */
void strap( MeshBuilder &b, const std::vector<Vector3> &points, Vector2 centre, float width, float thick )
{
    for ( size_t i = 0; i + 1 < points.size(); ++i )
    {
        Vector3 a = points[i], e = points[i + 1];
        float   len = Vector3Distance( a, e );
        if ( len < 1e-4f ) continue;
        Vector3 z = Vector3Scale( Vector3Subtract( e, a ), 1.0f / len );
        Vector3 mid = Vector3Lerp( a, e, 0.5f );
        Vector3 out = { mid.x - centre.x, 0.0f, mid.z - centre.y };
        out = Vector3Subtract( out, Vector3Scale( z, Vector3DotProduct( out, z ) ) );
        if ( Vector3Length( out ) < 1e-4f ) out = Vector3Perpendicular( z );
        Vector3 x = Vector3Normalize( out );
        b.box( frame( x, Vector3CrossProduct( z, x ), z, mid ), { thick, width * 0.5f, len * 0.5f + 0.008f } );
    }
}

}   // namespace

bool hidesHair( GarmentE g )
{
    return g == GarmentE::CAP || g == GarmentE::HOOD_UP;
}

std::vector<std::vector<GarmentPiece>> buildGarments( const SkinModel &body )
{
    std::vector<std::vector<GarmentPiece>> out( (int)GarmentE::COUNT );
    Measure                                m( body );
    auto                                   add = [&]( GarmentE g, const char *bone, GarmentColourE colour, auto shape )
    {
        MeshBuilder b;
        shape( b );
        for ( Mesh mesh : b.build() ) out[(int)g].push_back( { mesh, body.bone( bone ), colour } );
    };
    const Vector3 neck = m.bone( "neck_01" ), head = m.bone( "Head" ), spine = m.bone( "spine_03" );
    const Vector3 arm = m.bone( "upperarm_l" ), thigh = m.bone( "thigh_l" ), knee = m.bone( "calf_l" );
    const float   top = m.top(), belt = 0.95f, arms = std::max( arm.x, 0.15f );
    // The outlines: hips, torso (without the arms of the T-pose), neck, head, a leg.
    auto torso = [&]( float y, float w ) { return m.section( y, 0.0f, -w, w ); };
    auto neckAt = [&]( float y ) { return m.section( y, 0.0f, -0.09f, 0.09f ); };
    auto headAt = [&]( float y ) { return m.section( y, 0.0f, -0.15f, 0.15f ); };
    auto legAt = [&]( float side, float y )
    { return m.section( y, side * thigh.x, side > 0.0f ? 0.0f : -0.35f, side > 0.0f ? 0.35f : 0.0f ); };
    const Section hip = torso( belt, 0.3f );
    const float   hem = knee.y - 0.1f;

    // The coat: a skirt behind and at the sides that flares out (the legs swing inside it), a flap before each thigh
    // that goes with it, and a stand-up collar.
    add( GarmentE::COAT, "pelvis", GarmentColourE::UPPER,
         [&]( MeshBuilder &b )
         {
             const float ys[4] = { belt + 0.02f, 0.8f, 0.62f, hem }, flare[4] = { 0.022f, 0.05f, 0.09f, 0.125f };
             std::vector<std::vector<Vector3>> rings;
             for ( int i = 0; i < 4; ++i )
                 rings.push_back( arc( hip.moved( ys[i] ), flare[i], 1.25f, Turn - 1.25f, 16 ) );
             b.loft( MatrixIdentity(), rings, false );
         } );
    for ( float side : { 1.0f, -1.0f } )
        add( GarmentE::COAT, side > 0.0f ? "thigh_l" : "thigh_r", GarmentColourE::UPPER,
             [&]( MeshBuilder &b )
             {
                 const Section leg = legAt( side, 0.75f );
                 const float   ys[4] = { belt + 0.02f, 0.8f, 0.62f, hem }, flare[4] = { 0.045f, 0.055f, 0.075f, 0.1f };
                 std::vector<std::vector<Vector3>> rings;
                 for ( int i = 0; i < 4; ++i )
                     rings.push_back( side > 0.0f ? arc( leg.moved( ys[i] ), flare[i], -0.35f, 1.75f, 10 )
                                                  : arc( leg.moved( ys[i] ), flare[i], -1.75f, 0.35f, 10 ) );
                 b.loft( MatrixIdentity(), rings, false );
             } );
    auto collar = [&]( MeshBuilder &b )
    {
        std::vector<std::vector<Vector3>> rings = {
            arc( torso( neck.y - 0.06f, 0.12f ), 0.015f, 0.45f, Turn - 0.45f, 16 ),
            arc( neckAt( neck.y - 0.02f ), 0.04f, 0.45f, Turn - 0.45f, 16 ),
            arc( neckAt( neck.y + 0.04f ).moved( neck.y + 0.045f ), 0.055f, 0.45f, Turn - 0.45f, 16 )
        };
        b.loft( MatrixIdentity(), rings, false );
    };
    add( GarmentE::COAT, "spine_03", GarmentColourE::UPPER, collar );
    add( GarmentE::COLLAR, "spine_03", GarmentColourE::UPPER, collar );

    // A hood down: a soft roll behind the neck, its fold over the shoulder blades.
    add( GarmentE::HOOD, "spine_03", GarmentColourE::UPPER,
         [&]( MeshBuilder &b )
         {
             std::vector<std::vector<Vector3>> rings = {
                 arc( torso( neck.y - 0.14f, arms ), 0.012f, 1.95f, Turn - 1.95f, 12 ),
                 arc( torso( neck.y - 0.07f, arms ), 0.05f, 1.85f, Turn - 1.85f, 12 ),
                 arc( neckAt( neck.y ), 0.08f, 1.75f, Turn - 1.75f, 12 ),
                 arc( neckAt( neck.y + 0.05f ), 0.065f, 1.8f, Turn - 1.8f, 12 ),
                 arc( neckAt( neck.y + 0.08f ), 0.025f, 1.9f, Turn - 1.9f, 12 )
             };
             b.loft( MatrixIdentity(), rings, false );
         } );
    // A hood up: over the head and onto the shoulders, open round the face.
    add( GarmentE::HOOD_UP, "Head", GarmentColourE::UPPER,
         [&]( MeshBuilder &b )
         {
             const float                       ys[6] = { neck.y - 0.03f, neck.y + 0.05f, head.y + 0.05f,
                                                         head.y + 0.11f, top - 0.06f,    top - 0.02f };
             const float                       outs[6] = { 0.065f, 0.05f, 0.042f, 0.042f, 0.04f, 0.035f };
             const float                       open[6] = { 1.05f, 1.0f, 0.95f, 0.8f, 0.55f, 0.25f };
             std::vector<std::vector<Vector3>> rings;
             for ( int i = 0; i < 6; ++i )
             {
                 Section s = i == 0 ? neckAt( ys[i] ) : headAt( ys[i] );
                 rings.push_back( arc( s, outs[i], open[i], Turn - open[i], 16 ) );
             }
             // The crown: rings drawn in towards the middle until it closes.
             Section s = headAt( top - 0.02f );
             for ( auto [scale, y] : { std::pair{ 0.6f, top + 0.022f }, std::pair{ 0.25f, top + 0.034f },
                                       std::pair{ 0.02f, top + 0.037f } } )
             {
                 std::vector<Vector3> crown = rings[5];
                 for ( Vector3 &p : crown )
                     p = { s.m_centre.x + ( p.x - s.m_centre.x ) * scale, y,
                           s.m_centre.y + ( p.z - s.m_centre.y ) * scale };
                 rings.push_back( crown );
             }
             b.loft( MatrixIdentity(), rings, false );
         } );

    // A scarf: wound thick round the neck, an end hanging on the chest.
    add( GarmentE::SCARF, "neck_01", GarmentColourE::ACCENT,
         [&]( MeshBuilder &b )
         {
             std::vector<std::vector<Vector3>> rings = { whole( torso( neck.y - 0.055f, 0.12f ), 0.015f, 18 ),
                                                         whole( neckAt( neck.y - 0.025f ), 0.055f, 18 ),
                                                         whole( neckAt( neck.y + 0.015f ), 0.055f, 18 ),
                                                         whole( neckAt( neck.y + 0.045f ), 0.02f, 18 ) };
             b.loft( MatrixIdentity(), rings, true );
         } );
    add( GarmentE::SCARF, "spine_03", GarmentColourE::ACCENT,
         [&]( MeshBuilder &b )
         {
             Vector3 p = torso( neck.y - 0.16f, arms ).at( 0.28f, 0.022f );
             b.roundedBox( place( p, { 0.12f, 0.28f, 0.06f } ), { 0.045f, 0.11f, 0.012f }, 0.01f, 2 );
         } );

    // A backpack: the pack, a pocket on it, a bedroll on top; straps over the shoulders.
    const Section chest = torso( spine.y - 0.03f, arms );
    const Vector3 back = chest.at( PI, 0.0f );
    add( GarmentE::BACKPACK, "spine_03", GarmentColourE::ACCENT,
         [&]( MeshBuilder &b )
         {
             b.roundedBox( place( { 0.0f, spine.y - 0.04f, back.z - 0.085f } ), { 0.15f, 0.17f, 0.075f }, 0.05f, 3 );
             b.roundedBox( place( { 0.0f, spine.y - 0.1f, back.z - 0.165f } ), { 0.1f, 0.07f, 0.022f }, 0.015f, 2 );
         } );
    add( GarmentE::BACKPACK, "spine_03", GarmentColourE::LOWER,
         [&]( MeshBuilder &b )
         {
             b.cylinder( MatrixMultiply( MatrixRotateZ( -PI * 0.5f ),
                                         MatrixTranslate( -0.18f, spine.y + 0.19f, back.z - 0.08f ) ),
                         0.055f, 0.36f, 14 );
         } );
    add( GarmentE::BACKPACK, "spine_03", GarmentColourE::DARK,
         [&]( MeshBuilder &b )
         {
             for ( float side : { 1.0f, -1.0f } )
             {
                 float   x = side * 0.095f;
                 Vector3 low = torso( spine.y - 0.1f, arms ).at( side * 0.5f, 0.008f );
                 Vector3 high = torso( neck.y - 0.08f, arms ).at( side * 0.45f, 0.01f );
                 float   z = neckAt( neck.y - 0.03f ).m_centre.y;
                 Vector3 over = { x, m.topAt( x, z, head.y - 0.05f ) + 0.012f, z };
                 Vector3 behind = { x, spine.y + 0.1f, back.z - 0.04f };
                 strap( b, { low, high, over, behind }, { 0.0f, z }, 0.045f, 0.007f );
             }
         } );

    // A satchel on the left hip; its strap from the right shoulder across the chest and the back.
    const Vector3 bag = hip.at( PI * 0.5f + 0.15f, 0.045f );
    add( GarmentE::SATCHEL, "pelvis", GarmentColourE::LEATHER,
         [&]( MeshBuilder &b )
         {
             Vector3 at = { bag.x, belt - 0.1f, bag.z };
             b.roundedBox( place( at, { 0.0f, PI * 0.5f + 0.15f, 0.0f } ), { 0.12f, 0.1f, 0.035f }, 0.02f, 2 );
             Vector3 flap = Vector3Add( at, { 0.037f, 0.04f, 0.0f } );
             b.roundedBox( place( flap, { 0.0f, PI * 0.5f + 0.15f, 0.0f } ), { 0.118f, 0.06f, 0.006f }, 0.005f, 1 );
         } );
    add( GarmentE::SATCHEL, "spine_02", GarmentColourE::LEATHER,
         [&]( MeshBuilder &b )
         {
             float   z = neckAt( neck.y - 0.03f ).m_centre.y;
             Vector3 over = { -0.1f, m.topAt( -0.1f, z, head.y - 0.05f ) + 0.012f, z };
             Vector3 end = { bag.x, belt - 0.02f, bag.z };
             strap( b,
                    { over, torso( spine.y, arms ).at( -0.25f, 0.012f ),
                      torso( spine.y - 0.2f, 0.3f ).at( 0.45f, 0.014f ),
                      hip.moved( belt + 0.02f ).at( PI * 0.5f - 0.1f, 0.03f ), end },
                    { 0.0f, hip.m_centre.y }, 0.04f, 0.005f );
             strap( b,
                    { over, torso( spine.y, arms ).at( PI + 0.25f, 0.012f ),
                      torso( spine.y - 0.2f, 0.3f ).at( PI - 0.45f, 0.014f ),
                      hip.moved( belt + 0.02f ).at( PI * 0.5f + 0.45f, 0.03f ), end },
                    { 0.0f, hip.m_centre.y }, 0.04f, 0.005f );
         } );

    // A tool belt: broad, with pouches and a tool hanging from it.
    add( GarmentE::TOOLBELT, "pelvis", GarmentColourE::LEATHER,
         [&]( MeshBuilder &b )
         {
             std::vector<std::vector<Vector3>> rings = { whole( hip.moved( belt - 0.025f ), 0.012f, 24 ),
                                                         whole( hip.moved( belt ), 0.02f, 24 ),
                                                         whole( hip.moved( belt + 0.025f ), 0.012f, 24 ) };
             b.loft( MatrixIdentity(), rings, true );
         } );
    add( GarmentE::TOOLBELT, "pelvis", GarmentColourE::DARK,
         [&]( MeshBuilder &b )
         {
             for ( float a : { 0.55f, 1.25f, 2.05f, -0.65f, -1.35f } )
             {
                 Vector3 p = hip.moved( belt - 0.035f ).at( a, 0.042f );
                 b.roundedBox( place( p, { 0.0f, a, 0.0f } ), { 0.04f, 0.05f, 0.022f }, 0.012f, 2 );
             }
             Vector3 tool = hip.moved( 0.76f ).at( -2.1f, 0.035f );
             b.cylinder( place( tool ), 0.012f, belt - 0.78f, 8 );
         } );

    // A work vest, open in front, with four pockets.
    add( GarmentE::VEST, "spine_02", GarmentColourE::ACCENT,
         [&]( MeshBuilder &b )
         {
             const float ys[6] = { belt + 0.04f, 1.08f, 1.18f, 1.28f, arm.y - 0.06f, arm.y + 0.02f };
             const float outs[6] = { 0.02f, 0.022f, 0.024f, 0.026f, 0.026f, 0.02f };
             std::vector<std::vector<Vector3>> rings;
             for ( int i = 0; i < 6; ++i )
                 rings.push_back( arc( torso( ys[i], i == 5 ? 0.12f : ( ys[i] > 1.25f ? arms : 0.3f ) ), outs[i], 0.25f,
                                       Turn - 0.25f, 18 ) );
             b.loft( MatrixIdentity(), rings, false );
             for ( float side : { 1.0f, -1.0f } )
             {
                 Vector3 upper = torso( 1.3f, arms ).at( side * 0.45f, 0.045f );
                 b.roundedBox( place( upper, { 0.0f, side * 0.45f, 0.0f } ), { 0.045f, 0.05f, 0.014f }, 0.01f, 2 );
                 Vector3 lower = torso( 1.08f, 0.3f ).at( side * 0.55f, 0.047f );
                 b.roundedBox( place( lower, { 0.0f, side * 0.55f, 0.0f } ), { 0.05f, 0.045f, 0.016f }, 0.01f, 2 );
             }
         } );

    // A knitted cap, lower at the back than over the brow.
    add( GarmentE::CAP, "Head", GarmentColourE::ACCENT,
         [&]( MeshBuilder &b )
         {
             const float                       drop[6] = { 0.1f, 0.085f, 0.055f, 0.025f, 0.0f, -0.004f };
             const float                       outs[6] = { 0.032f, 0.032f, 0.024f, 0.015f, 0.0f, 0.0f };
             std::vector<std::vector<Vector3>> rings;
             for ( int i = 0; i < 6; ++i )
             {
                 Section              s = headAt( top - drop[i] - 0.01f );
                 std::vector<Vector3> ring = whole( s, outs[i], 20 );
                 for ( size_t k = 0; k < ring.size(); ++k )
                 {
                     float a = Turn * k / ring.size();
                     ring[k].y = top - drop[i] + 0.02f * ( i < 3 ? cosf( a ) : 0.0f ) + ( i >= 4 ? 0.015f : 0.0f );
                     // The crown closes.
                     float in = i == 4 ? 0.5f : i == 5 ? 0.02f : 1.0f;
                     ring[k] = { s.m_centre.x + ( ring[k].x - s.m_centre.x ) * in, ring[k].y,
                                 s.m_centre.y + ( ring[k].z - s.m_centre.y ) * in };
                 }
                 rings.push_back( ring );
             }
             b.loft( MatrixIdentity(), rings, true );
         } );

    // An apron over the thighs, its bib on the chest, a strap round the neck.
    add( GarmentE::APRON, "pelvis", GarmentColourE::ACCENT,
         [&]( MeshBuilder &b )
         {
             const float                       ys[4] = { belt + 0.025f, 0.82f, 0.64f, knee.y - 0.04f },
                                               outs[4] = { 0.022f, 0.05f, 0.075f, 0.09f };
             std::vector<std::vector<Vector3>> rings;
             for ( int i = 0; i < 4; ++i ) rings.push_back( arc( hip.moved( ys[i] ), outs[i], -1.15f, 1.15f, 12 ) );
             b.loft( MatrixIdentity(), rings, false );
         } );
    add(
        GarmentE::APRON, "spine_02", GarmentColourE::ACCENT,
        [&]( MeshBuilder &b )
        {
            std::vector<std::vector<Vector3>> rings = { arc( torso( belt + 0.07f, 0.3f ), 0.022f, -0.6f, 0.6f, 8 ),
                                                        arc( torso( spine.y - 0.05f, arms ), 0.022f, -0.55f, 0.55f, 8 ),
                                                        arc( torso( spine.y + 0.08f, arms ), 0.02f, -0.5f, 0.5f, 8 ) };
            b.loft( MatrixIdentity(), rings, false );
        } );
    add( GarmentE::APRON, "spine_03", GarmentColourE::DARK,
         [&]( MeshBuilder &b )
         {
             Section top3 = torso( spine.y + 0.08f, arms );
             strap( b,
                    { top3.at( 0.5f, 0.02f ), neckAt( neck.y - 0.02f ).at( 1.3f, 0.02f ),
                      neckAt( neck.y ).at( PI, 0.02f ), neckAt( neck.y - 0.02f ).at( -1.3f, 0.02f ),
                      top3.at( -0.5f, 0.02f ) },
                    { 0.0f, top3.m_centre.y }, 0.025f, 0.004f );
         } );

    // Pads on the knees; pockets on the thighs' outsides; boots round the feet up to the ankles.
    for ( float side : { 1.0f, -1.0f } )
    {
        add( GarmentE::BOOTS, side > 0.0f ? "foot_l" : "foot_r", GarmentColourE::DARK,
             [&]( MeshBuilder &b )
             {
                 const float                       ys[6] = { 0.004f, 0.03f, 0.07f, 0.11f, 0.16f, 0.21f };
                 const float                       outs[6] = { 0.006f, 0.014f, 0.016f, 0.016f, 0.014f, 0.012f };
                 std::vector<std::vector<Vector3>> rings;
                 for ( int i = 0; i < 6; ++i ) rings.push_back( whole( legAt( side, ys[i] ), outs[i], 24 ) );
                 // The sole closed underneath.
                 std::vector<Vector3> sole = rings.front();
                 Vector3              mid = { 0.0f, 0.0f, 0.0f };
                 for ( Vector3 p : sole ) mid = Vector3Add( mid, Vector3Scale( p, 1.0f / sole.size() ) );
                 for ( Vector3 &p : sole )
                     p = { mid.x + ( p.x - mid.x ) * 0.05f, 0.0f, mid.z + ( p.z - mid.z ) * 0.05f };
                 rings.insert( rings.begin(), sole );
                 b.loft( MatrixIdentity(), rings, true );
             } );
        add( GarmentE::KNEEPADS, side > 0.0f ? "calf_l" : "calf_r", GarmentColourE::DARK,
             [&]( MeshBuilder &b )
             {
                 Vector3 p = legAt( side, knee.y - 0.02f ).at( 0.0f, 0.018f );
                 b.roundedBox( place( p, { -0.1f, 0.0f, 0.0f } ), { 0.05f, 0.065f, 0.02f }, 0.015f, 2 );
             } );
        add( GarmentE::POCKETS, side > 0.0f ? "thigh_l" : "thigh_r", GarmentColourE::LOWER,
             [&]( MeshBuilder &b )
             {
                 float   a = side * PI * 0.5f;
                 Vector3 p = legAt( side, 0.7f ).at( a, 0.02f );
                 b.roundedBox( place( p, { 0.0f, a, 0.0f } ), { 0.06f, 0.07f, 0.02f }, 0.012f, 2 );
             } );
    }
    return out;
}

}   // namespace spaceopera
