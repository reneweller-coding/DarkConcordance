/**
 * @file MeshBuilder.cpp
 * @brief The shapes of MeshBuilder.
 * @author Rene Weller
 */
#include "MeshBuilder.h"

#include <raymath.h>

#include <cmath>
#include <cstring>

namespace spaceopera
{

namespace
{

constexpr size_t ChunkVertices = 65000;   ///< raylib's meshes index with 16 bits

/** @brief Rotates a direction by a matrix (no translation). */
Vector3 rotate( const Matrix &m, Vector3 v )
{
    return { m.m0 * v.x + m.m4 * v.y + m.m8 * v.z, m.m1 * v.x + m.m5 * v.y + m.m9 * v.z,
             m.m2 * v.x + m.m6 * v.y + m.m10 * v.z };
}

/** @brief The coordinates of a rounded box's grid along one axis: dense in the rounds, one step across the flat. */
std::vector<float> roundCoords( float half, float radius, int seg )
{
    float              inner = half - radius;
    std::vector<float> c;
    for ( int k = 0; k <= seg; ++k ) c.push_back( -inner - radius * cosf( k * PI * 0.5f / seg ) );
    for ( int k = 0; k <= seg; ++k ) c.push_back( inner + radius * sinf( k * PI * 0.5f / seg ) );
    return c;
}

}   // namespace

Matrix frame( Vector3 x, Vector3 y, Vector3 z, Vector3 t )
{
    Matrix m = MatrixIdentity();
    m.m0 = x.x;
    m.m1 = x.y;
    m.m2 = x.z;
    m.m4 = y.x;
    m.m5 = y.y;
    m.m6 = y.z;
    m.m8 = z.x;
    m.m9 = z.y;
    m.m10 = z.z;
    m.m12 = t.x;
    m.m13 = t.y;
    m.m14 = t.z;
    return m;
}

Matrix place( Vector3 t, Vector3 rot )
{
    return MatrixMultiply( MatrixRotateXYZ( rot ), MatrixTranslate( t.x, t.y, t.z ) );
}

void MeshBuilder::reserve( size_t count )
{
    if ( m_pos.size() + count <= ChunkVertices ) return;
    m_done.push_back(
        { std::move( m_pos ), std::move( m_nrm ), std::move( m_uv ), std::move( m_col ), std::move( m_idx ) } );
    m_pos.clear();
    m_nrm.clear();
    m_uv.clear();
    m_col.clear();
    m_idx.clear();
}

unsigned short MeshBuilder::vertex( Vector3 p, Vector3 n, Vector2 uv )
{
    m_pos.push_back( p );
    m_nrm.push_back( n );
    m_uv.push_back( uv );
    m_col.push_back( m_color );
    return (unsigned short)( m_pos.size() - 1 );
}

void MeshBuilder::box( const Matrix &m, Vector3 half )
{
    reserve( 24 );
    const float h[3] = { half.x, half.y, half.z };
    for ( int axis = 0; axis < 3; ++axis )
        for ( int sign = -1; sign <= 1; sign += 2 )
        {
            int   b = ( axis + 1 ) % 3;
            int   c = ( axis + 2 ) % 3;
            float n[3] = { 0, 0, 0 };
            n[axis] = (float)sign;
            Vector3        normal = rotate( m, { n[0], n[1], n[2] } );
            unsigned short first = 0;
            for ( int k = 0; k < 4; ++k )
            {
                float p[3];
                p[axis] = sign * h[axis];
                p[b] = ( k == 1 || k == 2 ) ? h[b] : -h[b];
                p[c] = ( k >= 2 ) ? h[c] : -h[c];
                unsigned short i = vertex( Vector3Transform( { p[0], p[1], p[2] }, m ), normal,
                                           { ( k == 1 || k == 2 ) ? 1.0f : 0.0f, k >= 2 ? 1.0f : 0.0f } );
                if ( k == 0 ) first = i;
            }
            m_idx.insert( m_idx.end(), { first, (unsigned short)( first + 1 ), (unsigned short)( first + 2 ), first,
                                         (unsigned short)( first + 2 ), (unsigned short)( first + 3 ) } );
        }
}

void MeshBuilder::roundedBox( const Matrix &m, Vector3 half, float radius, int seg )
{
    const float h[3] = { half.x, half.y, half.z };
    radius = fminf( radius, fminf( half.x, fminf( half.y, half.z ) ) );
    float inner[3] = { half.x - radius, half.y - radius, half.z - radius };
    for ( int axis = 0; axis < 3; ++axis )
        for ( int sign = -1; sign <= 1; sign += 2 )
        {
            int                b = ( axis + 1 ) % 3;
            int                c = ( axis + 2 ) % 3;
            std::vector<float> cb = roundCoords( h[b], radius, seg );
            std::vector<float> cc = roundCoords( h[c], radius, seg );
            reserve( cb.size() * cc.size() );
            unsigned short first = (unsigned short)m_pos.size();
            for ( float vc : cc )
                for ( float vb : cb )
                {
                    float q[3];
                    q[axis] = sign * h[axis];
                    q[b] = vb;
                    q[c] = vc;
                    float cl[3];
                    for ( int i = 0; i < 3; ++i ) cl[i] = Clamp( q[i], -inner[i], inner[i] );
                    Vector3 d = { q[0] - cl[0], q[1] - cl[1], q[2] - cl[2] };
                    Vector3 n = Vector3Normalize( d );
                    Vector3 p = { cl[0] + n.x * radius, cl[1] + n.y * radius, cl[2] + n.z * radius };
                    vertex( Vector3Transform( p, m ), rotate( m, n ), { 0.0f, 0.0f } );
                }
            unsigned short w = (unsigned short)cb.size();
            for ( unsigned short j = 0; j + 1 < cc.size(); ++j )
                for ( unsigned short i = 0; i + 1 < w; ++i )
                {
                    unsigned short a = first + j * w + i;
                    m_idx.insert( m_idx.end(), { a, (unsigned short)( a + 1 ), (unsigned short)( a + w + 1 ), a,
                                                 (unsigned short)( a + w + 1 ), (unsigned short)( a + w ) } );
                }
        }
}

void MeshBuilder::lathe( const Matrix &m, const std::vector<Vector2> &profile, int sides, float from, float to )
{
    size_t count = profile.size();
    reserve( count * ( sides + 1 ) );
    std::vector<Vector2> normals( count );
    for ( size_t i = 0; i < count; ++i )
    {
        // A repeated point is a hard edge: the first copy looks back, the second ahead.
        bool    dupNext = i + 1 < count && Vector2Equals( profile[i], profile[i + 1] );
        bool    dupPrev = i > 0 && Vector2Equals( profile[i], profile[i - 1] );
        Vector2 prev = profile[i > 0 && !dupPrev ? i - 1 : i];
        Vector2 next = profile[i + 1 < count && !dupNext ? i + 1 : i];
        if ( dupNext && i > 0 ) next = profile[i];
        if ( dupPrev && i + 1 < count ) prev = profile[i];
        Vector2 t = Vector2Subtract( next, prev );
        if ( Vector2Length( t ) < 1e-6f ) t = { 0.0f, 1.0f };
        normals[i] = Vector2Normalize( { t.y, -t.x } );
    }
    unsigned short first = (unsigned short)m_pos.size();
    for ( int s = 0; s <= sides; ++s )
    {
        float a = from + ( to - from ) * s / sides;
        float ca = cosf( a );
        float sa = sinf( a );
        for ( size_t i = 0; i < count; ++i )
        {
            Vector3 p = { profile[i].x * ca, profile[i].y, profile[i].x * sa };
            Vector3 n = { normals[i].x * ca, normals[i].y, normals[i].x * sa };
            vertex( Vector3Transform( p, m ), rotate( m, n ), { (float)s / sides, (float)i / ( count - 1 ) } );
        }
    }
    for ( int s = 0; s < sides; ++s )
        for ( size_t i = 0; i + 1 < count; ++i )
        {
            if ( Vector2Equals( profile[i], profile[i + 1] ) ) continue;
            unsigned short a = (unsigned short)( first + s * count + i );
            unsigned short b = (unsigned short)( a + count );
            m_idx.insert( m_idx.end(), { a, b, (unsigned short)( b + 1 ), a, (unsigned short)( b + 1 ),
                                         (unsigned short)( a + 1 ) } );
        }
}

void MeshBuilder::loft( const Matrix &m, const std::vector<std::vector<Vector3>> &rings, bool closed )
{
    size_t count = rings.size(), k = count ? rings.front().size() : 0;
    if ( count < 2 || k < 2 ) return;
    // A closed ring repeats its first point at its end (the seam gets its own UVs).
    size_t around = closed ? k + 1 : k;
    reserve( count * around );
    unsigned short first = (unsigned short)m_pos.size();
    for ( size_t r = 0; r < count; ++r )
    {
        Vector3 mid = { 0.0f, 0.0f, 0.0f };
        for ( Vector3 p : rings[r] ) mid = Vector3Add( mid, p );
        mid = Vector3Scale( mid, 1.0f / k );
        for ( size_t j = 0; j < around; ++j )
        {
            size_t  i = j % k;
            Vector3 p = rings[r][i];
            Vector3 prev = rings[r][closed ? ( i + k - 1 ) % k : ( i > 0 ? i - 1 : i )];
            Vector3 next = rings[r][closed ? ( i + 1 ) % k : ( i + 1 < k ? i + 1 : i )];
            Vector3 down = rings[r > 0 ? r - 1 : r][i], up = rings[r + 1 < count ? r + 1 : r][i];
            Vector3 n = Vector3CrossProduct( Vector3Subtract( next, prev ), Vector3Subtract( up, down ) );
            n = Vector3Length( n ) > 1e-9f ? Vector3Normalize( n ) : Vector3{ 0.0f, 1.0f, 0.0f };
            if ( Vector3DotProduct( n, Vector3Subtract( p, mid ) ) < 0.0f ) n = Vector3Negate( n );
            vertex( Vector3Transform( p, m ), rotate( m, n ), { (float)j / ( around - 1 ), (float)r / ( count - 1 ) } );
        }
    }
    for ( size_t r = 0; r + 1 < count; ++r )
        for ( size_t j = 0; j + 1 < around; ++j )
        {
            unsigned short a = (unsigned short)( first + r * around + j );
            unsigned short b = (unsigned short)( a + around );
            m_idx.insert( m_idx.end(), { a, b, (unsigned short)( b + 1 ), a, (unsigned short)( b + 1 ),
                                         (unsigned short)( a + 1 ) } );
        }
}

void MeshBuilder::cylinder( const Matrix &m, float radius, float height, int sides )
{
    lathe( m,
           { { 0.0f, 0.0f },
             { radius, 0.0f },
             { radius, 0.0f },
             { radius, height },
             { radius, height },
             { 0.0f, height } },
           sides );
}

void MeshBuilder::quad( Vector3 a, Vector3 b, Vector3 c, Vector3 d )
{
    reserve( 4 );
    Vector3        n = Vector3Normalize( Vector3CrossProduct( Vector3Subtract( d, a ), Vector3Subtract( b, a ) ) );
    unsigned short i = vertex( a, n, { 0.0f, 0.0f } );
    vertex( b, n, { 1.0f, 0.0f } );
    vertex( c, n, { 1.0f, 1.0f } );
    vertex( d, n, { 0.0f, 1.0f } );
    m_idx.insert( m_idx.end(), { i, (unsigned short)( i + 1 ), (unsigned short)( i + 2 ), i, (unsigned short)( i + 2 ),
                                 (unsigned short)( i + 3 ) } );
}

void MeshBuilder::triangle( Vector3 a, Vector3 b, Vector3 c )
{
    reserve( 3 );
    Vector3        n = Vector3Normalize( Vector3CrossProduct( Vector3Subtract( b, a ), Vector3Subtract( c, a ) ) );
    unsigned short i = vertex( a, n, { 0.0f, 0.0f } );
    vertex( b, n, { 1.0f, 0.0f } );
    vertex( c, n, { 0.0f, 1.0f } );
    m_idx.insert( m_idx.end(), { i, (unsigned short)( i + 1 ), (unsigned short)( i + 2 ) } );
}

void MeshBuilder::prism( const Matrix &m, const std::vector<std::vector<Vector3>> &rings )
{
    // Flat faces whose normals point away from the axis; the caps away from the middle.
    auto face = [&]( Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 inside )
    {
        reserve( 4 );
        Vector3 n = Vector3Normalize( Vector3CrossProduct( Vector3Subtract( b, a ), Vector3Subtract( d, a ) ) );
        Vector3 centre = Vector3Scale( Vector3Add( Vector3Add( a, b ), Vector3Add( c, d ) ), 0.25f );
        if ( Vector3DotProduct( n, Vector3Subtract( centre, inside ) ) < 0.0f ) n = Vector3Negate( n );
        unsigned short i = vertex( a, n, { 0.0f, 0.0f } );
        vertex( b, n, { 1.0f, 0.0f } );
        vertex( c, n, { 1.0f, 1.0f } );
        vertex( d, n, { 0.0f, 1.0f } );
        m_idx.insert( m_idx.end(), { i, (unsigned short)( i + 1 ), (unsigned short)( i + 2 ), i,
                                     (unsigned short)( i + 2 ), (unsigned short)( i + 3 ) } );
    };
    std::vector<std::vector<Vector3>> w( rings.size() );
    for ( size_t r = 0; r < rings.size(); ++r )
        for ( Vector3 p : rings[r] ) w[r].push_back( Vector3Transform( p, m ) );
    Vector3 middle = { 0.0f, 0.0f, 0.0f };
    size_t  corners = 0;
    for ( auto &ring : w )
        for ( Vector3 p : ring )
        {
            middle = Vector3Add( middle, p );
            ++corners;
        }
    middle = Vector3Scale( middle, 1.0f / corners );
    for ( size_t r = 0; r + 1 < w.size(); ++r )
    {
        size_t  k = w[r].size();
        Vector3 axis = { 0.0f, 0.0f, 0.0f };
        for ( size_t i = 0; i < k; ++i ) axis = Vector3Add( axis, Vector3Add( w[r][i], w[r + 1][i] ) );
        axis = Vector3Scale( axis, 0.5f / k );
        for ( size_t i = 0; i < k; ++i )
            face( w[r][i], w[r][( i + 1 ) % k], w[r + 1][( i + 1 ) % k], w[r + 1][i], axis );
    }
    for ( size_t end : { (size_t)0, w.size() - 1 } )
    {
        size_t  k = w[end].size();
        Vector3 c = { 0.0f, 0.0f, 0.0f };
        for ( Vector3 p : w[end] ) c = Vector3Add( c, p );
        c = Vector3Scale( c, 1.0f / k );
        for ( size_t i = 0; i < k; ++i )
        {
            Vector3 a = w[end][i];
            Vector3 b = w[end][( i + 1 ) % k];
            Vector3 n = Vector3Normalize( Vector3CrossProduct( Vector3Subtract( b, a ), Vector3Subtract( c, a ) ) );
            if ( Vector3DotProduct( n, Vector3Subtract( c, middle ) ) < 0.0f ) n = Vector3Negate( n );
            reserve( 3 );
            unsigned short j = vertex( a, n, { 0.0f, 0.0f } );
            vertex( b, n, { 1.0f, 0.0f } );
            vertex( c, n, { 0.5f, 1.0f } );
            m_idx.insert( m_idx.end(), { j, (unsigned short)( j + 1 ), (unsigned short)( j + 2 ) } );
        }
    }
}

std::vector<Mesh> MeshBuilder::build()
{
    reserve( ChunkVertices + 1 );   // closes the open chunk
    std::vector<Mesh> meshes;
    for ( Chunk &c : m_done )
    {
        if ( c.m_pos.empty() ) continue;
        Mesh mesh = {};
        mesh.vertexCount = (int)c.m_pos.size();
        mesh.triangleCount = (int)( c.m_idx.size() / 3 );
        mesh.vertices = (float *)MemAlloc( (unsigned)( mesh.vertexCount * 3 * sizeof( float ) ) );
        mesh.normals = (float *)MemAlloc( (unsigned)( mesh.vertexCount * 3 * sizeof( float ) ) );
        mesh.texcoords = (float *)MemAlloc( (unsigned)( mesh.vertexCount * 2 * sizeof( float ) ) );
        mesh.colors = (unsigned char *)MemAlloc( (unsigned)( mesh.vertexCount * 4 ) );
        mesh.indices = (unsigned short *)MemAlloc( (unsigned)( c.m_idx.size() * sizeof( unsigned short ) ) );
        memcpy( mesh.vertices, c.m_pos.data(), mesh.vertexCount * 3 * sizeof( float ) );
        memcpy( mesh.normals, c.m_nrm.data(), mesh.vertexCount * 3 * sizeof( float ) );
        memcpy( mesh.texcoords, c.m_uv.data(), mesh.vertexCount * 2 * sizeof( float ) );
        memcpy( mesh.colors, c.m_col.data(), mesh.vertexCount * 4 );
        memcpy( mesh.indices, c.m_idx.data(), c.m_idx.size() * sizeof( unsigned short ) );
        UploadMesh( &mesh, false );
        meshes.push_back( mesh );
    }
    m_done.clear();
    return meshes;
}

}   // namespace spaceopera
