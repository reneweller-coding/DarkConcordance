/**
 * @file Scene.cpp
 * @brief The observation deck: geometry, props, lights, particles, bolts, the hero's movement and the camera.
 * @author Rene Weller
 */
#include "Scene.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace spaceopera
{

namespace
{

constexpr float RoomW = 14.0f;                         ///< the room spans x -14..14
constexpr float RoomN = -11.0f;                        ///< north wall (inner face)
constexpr float RoomS = 12.0f;                         ///< south edge (no wall: the camera's side)
constexpr float WallH = 7.0f;                          ///< wall height
constexpr float GlassX = 7.0f;                         ///< the glass floor spans x -7..7
constexpr float GlassN = -6.0f;                        ///< and z -6..4
constexpr float GlassS = 4.0f;                         ///< south edge of the glass
constexpr float RunSpeed = 4.2f;                       ///< the hero's top speed (m/s)
constexpr float HeroRadius = 0.35f;                    ///< the hero's size on the ground
const Vector3   Conduit = { 7.62f, 4.7f, -10.75f };    ///< the broken pipe's end
const Vector3   MonolithAt = { 0.0f, 0.6f, -9.3f };    ///< the monolith stands here (on the platform)
const Vector3   ReactorAt = { -10.5f, 0.0f, -7.8f };   ///< the reactor
const float     CryoZ[3] = { -5.0f, -1.5f, 2.0f };     ///< the cryo pods along the east wall
const float     WindowZ[7] = { -9.0f, -6.0f, -3.0f, 0.0f, 3.0f, 6.0f, 9.0f };   ///< the west windows' middles
constexpr float WindowHalf = 0.5f;                                              ///< half a window's width
constexpr float WindowLow = 1.4f;                                               ///< sill height
constexpr float WindowHigh = 5.6f;                                              ///< lintel height

/** @brief Loads a texture with mipmaps and anisotropic filtering (a grey checker if it is missing). */
Texture2D loadTexture( const std::string &path, bool mipmaps = true )
{
    Texture2D t = FileExists( path.c_str() ) ? LoadTexture( path.c_str() ) : Texture2D{};
    if ( t.id == 0 )
    {
        Image i = GenImageChecked( 64, 64, 8, 8, GRAY, DARKGRAY );
        t = LoadTextureFromImage( i );
        UnloadImage( i );
    }
    if ( mipmaps )
    {
        GenTextureMipmaps( &t );
        SetTextureFilter( t, TEXTURE_FILTER_TRILINEAR );
        SetTextureFilter( t, TEXTURE_FILTER_ANISOTROPIC_16X );
    }
    else
        SetTextureFilter( t, TEXTURE_FILTER_BILINEAR );
    return t;
}

/** @brief Pulse between 0 and 1 with sharp peaks. */
float pulse( float t, float rate, float phase )
{
    return powf( 0.5f + 0.5f * sinf( t * rate + phase ), 4.0f );
}

/** @brief A cheap hash of an integer to 0..1 (flicker). */
float hash1( int n )
{
    n = ( n << 13 ) ^ n;
    return ( ( n * ( n * n * 15731 + 789221 ) + 1376312589 ) & 0x7fffffff ) / 2147483647.0f;
}

}   // namespace

float Scene::rnd()
{
    m_seed = m_seed * 1664525u + 1013904223u;
    return ( m_seed >> 8 ) * ( 1.0f / 16777216.0f );
}

void Scene::addStatic( MeshBuilder &b, SurfaceE s, bool visible, Vector4 glow )
{
    for ( Mesh m : b.build() )
    {
        Drawable d;
        d.m_mesh = m;
        d.m_surface = (int)s;
        d.m_transform = MatrixIdentity();
        d.m_emissive = glow;
        d.m_visible = visible;
        m_static.push_back( d );
    }
}

void Scene::load( Gfx &gfx, const std::string &assets )
{
    std::string dir = assets + "/textures/";
    m_surfaces.resize( (int)SurfaceE::COUNT );
    auto tile = [&]( SurfaceE s, const char *name, float uv, Vector4 params, float variation, Color tint = WHITE,
                     Texture2D emission = {} )
    {
        Texture2D a = loadTexture( dir + name + "_albedo.png" );
        Texture2D o = loadTexture( dir + name + "_orm.png" );
        Texture2D n = loadTexture( dir + name + "_normal.png" );
        m_textures.insert( m_textures.end(), { a, o, n } );
        Surface surf = gfx.surface( a, o, n, emission );
        surf.m_uvScale = uv;
        surf.m_params = params;
        surf.m_variation = variation;
        surf.m_tint = tint;
        m_surfaces[(int)s] = surf;
    };
    tile( SurfaceE::FLOOR, "floor", 0.25f, { 1.0f, 1.0f, 1.0f, 0.0f }, 0.6f );
    tile( SurfaceE::WALL, "wall", 0.28f, { 1.0f, 1.0f, 1.2f, 0.0f }, 0.5f );
    tile( SurfaceE::METAL, "metal", 0.5f, { 0.9f, 1.0f, 1.0f, 0.0f }, 0.4f );
    tile( SurfaceE::HAZARD, "hazard", 0.7f, { 1.0f, 1.0f, 1.0f, 0.0f }, 0.3f );
    tile( SurfaceE::PIPES, "pipes", 0.45f, { 1.0f, 1.0f, 1.5f, 0.0f }, 0.3f );
    tile( SurfaceE::CRATE, "crate", 0.6f, { 1.0f, 1.0f, 1.0f, 0.0f }, 0.4f );
    tile( SurfaceE::PAINT, "hazard", 1.1f, { 1.0f, 1.0f, 1.0f, 0.0f }, 0.3f );
    tile( SurfaceE::ARMOR, "armor", 1.6f, { 1.0f, 1.0f, 1.5f, 0.0f }, 0.15f, { 200, 196, 186, 255 } );
    tile( SurfaceE::DARK, "armor", 1.6f, { 0.9f, 1.0f, 1.5f, 0.0f }, 0.15f, { 62, 64, 70, 255 } );
    tile( SurfaceE::OBSIDIAN, "obsidian", 0.45f, { 1.0f, 1.0f, 1.5f, 0.0f }, 0.2f );
    m_surfaces[(int)SurfaceE::OBSIDIAN].m_emissive = { 1.5f, 0.5f, 3.6f, 1.0f };
    m_surfaces[(int)SurfaceE::OBSIDIAN].m_mask = { 0.12f, 0.42f };
    Texture2D screen = loadTexture( dir + "screen.png" );
    m_textures.push_back( screen );
    tile( SurfaceE::SCREEN, "metal", 1.0f, { 0.25f, 0.0f, 0.0f, 1.0f }, 0.0f, { 30, 30, 30, 255 }, screen );
    m_surfaces[(int)SurfaceE::SCREEN].m_emissive = { 2.4f, 2.4f, 2.4f, 2.0f };
    tile( SurfaceE::GLOW, "metal", 0.5f, { 0.5f, 0.0f, 0.0f, 0.0f }, 0.0f, { 25, 25, 25, 255 } );

    m_nebula = loadTexture( dir + "nebula.png", false );
    m_gasGiant = loadTexture( dir + "gasgiant.png", false );
    Image dot = GenImageGradientRadial( 64, 64, 0.0f, WHITE, BLANK );
    m_dot = LoadTextureFromImage( dot );
    UnloadImage( dot );
    m_sphere = GenMeshSphere( 1.0f, 128, 128 );

    buildRoom();
    buildProps();
    m_hero.build( { (int)SurfaceE::ARMOR, (int)SurfaceE::DARK, (int)SurfaceE::METAL, (int)SurfaceE::GLOW } );

    // Dust in the air.
    for ( int i = 0; i < 700; ++i )
    {
        Particle p;
        p.m_pos = { -13.5f + rnd() * 27.0f, 0.2f + rnd() * 6.5f, -10.5f + rnd() * 22.0f };
        p.m_vel = { rnd() * 6.28f, rnd() * 6.28f, rnd() * 6.28f };   // phases of the drift
        p.m_life = p.m_maxLife = 1.0f;
        p.m_size = 0.008f + rnd() * 0.014f;
        p.m_color = { 210, 215, 230, (unsigned char)( 90 + rnd() * 120 ) };
        p.m_kind = 3;
        m_dust.push_back( p );
    }
    putHero( -3.0f, 1.0f, 200.0f );
}

void Scene::buildRoom()
{
    MeshBuilder floor;
    floor.box( place( { 0.0f, -0.15f, ( RoomN + GlassN ) * 0.5f } ), { RoomW, 0.15f, ( GlassN - RoomN ) * 0.5f } );
    floor.box( place( { 0.0f, -0.15f, ( GlassS + RoomS ) * 0.5f } ), { RoomW, 0.15f, ( RoomS - GlassS ) * 0.5f } );
    floor.box( place( { -( RoomW + GlassX ) * 0.5f, -0.15f, ( GlassN + GlassS ) * 0.5f } ),
               { ( RoomW - GlassX ) * 0.5f, 0.15f, ( GlassS - GlassN ) * 0.5f } );
    floor.box( place( { ( RoomW + GlassX ) * 0.5f, -0.15f, ( GlassN + GlassS ) * 0.5f } ),
               { ( RoomW - GlassX ) * 0.5f, 0.15f, ( GlassS - GlassN ) * 0.5f } );
    // The platform of the monolith and its steps.
    floor.box( place( { 0.0f, 0.3f, -9.3f } ), { 4.5f, 0.3f, 1.7f } );
    addStatic( floor, SurfaceE::FLOOR );

    MeshBuilder hazard;
    hazard.box( place( { 0.0f, 0.006f, GlassN - 0.175f } ), { GlassX + 0.35f, 0.006f, 0.175f } );
    hazard.box( place( { 0.0f, 0.006f, GlassS + 0.175f } ), { GlassX + 0.35f, 0.006f, 0.175f } );
    hazard.box( place( { -GlassX - 0.175f, 0.006f, -1.0f } ), { 0.175f, 0.006f, 5.0f } );
    hazard.box( place( { GlassX + 0.175f, 0.006f, -1.0f } ), { 0.175f, 0.006f, 5.0f } );
    hazard.box( place( { 0.0f, 0.3f, -7.58f } ), { 4.5f, 0.3f, 0.02f } );
    hazard.box( place( { 0.0f, 0.402f, -7.05f } ), { 2.0f, 0.004f, 0.06f } );
    hazard.box( place( { 0.0f, 0.202f, -6.45f } ), { 2.0f, 0.004f, 0.06f } );
    // Around the reactor.
    hazard.lathe( place( { ReactorAt.x, 0.004f, ReactorAt.z } ),
                  { { 1.8f, 0.0f }, { 1.8f, 0.0f }, { 2.2f, 0.0f }, { 2.2f, 0.0f } }, 48 );
    addStatic( hazard, SurfaceE::HAZARD );

    MeshBuilder metal;
    // Steps up to the platform.
    metal.roundedBox( place( { 0.0f, 0.2f, -7.3f } ), { 2.0f, 0.2f, 0.3f }, 0.03f, 2 );
    metal.roundedBox( place( { 0.0f, 0.1f, -6.7f } ), { 2.0f, 0.1f, 0.3f }, 0.03f, 2 );
    // The glass floor's frame.
    for ( float x : { -7.0f, -3.5f, 0.0f, 3.5f, 7.0f } )
        metal.roundedBox( place( { x, -0.08f, -1.0f } ), { 0.14f, 0.12f, 5.0f }, 0.03f, 2 );
    for ( float z : { -6.0f, -1.0f, 4.0f } )
        metal.roundedBox( place( { 0.0f, -0.08f, z } ), { 7.0f, 0.12f, 0.14f }, 0.03f, 2 );
    // Ribs on the north and east walls, with hazard feet.
    for ( float x : { -12.0f, -8.0f, -4.0f, 4.0f, 8.0f, 12.0f } )
        metal.roundedBox( place( { x, WallH * 0.5f, RoomN + 0.2f } ), { 0.4f, WallH * 0.5f, 0.25f }, 0.06f, 2 );
    for ( float z : { -8.0f, -4.0f, 0.0f, 4.0f, 8.0f } )
        metal.roundedBox( place( { RoomW - 0.2f, WallH * 0.5f, z } ), { 0.25f, WallH * 0.5f, 0.4f }, 0.06f, 2 );
    // Window frames in the west wall: lintel, sill, jambs and two transoms (the light falls in bars).
    for ( float zc : WindowZ )
    {
        float x = -RoomW + 0.05f;
        metal.box( place( { x, WindowHigh + 0.08f, zc } ), { 0.1f, 0.08f, WindowHalf + 0.1f } );
        metal.box( place( { x, WindowLow - 0.06f, zc } ), { 0.14f, 0.06f, WindowHalf + 0.1f } );
        metal.box( place( { x, 3.5f, zc - WindowHalf } ), { 0.1f, 2.15f, 0.08f } );
        metal.box( place( { x, 3.5f, zc + WindowHalf } ), { 0.1f, 2.15f, 0.08f } );
        for ( float y : { 2.8f, 4.2f } ) metal.box( place( { x - 0.15f, y, zc } ), { 0.05f, 0.05f, WindowHalf } );
    }
    // Pipes along the north wall; the lowest is broken at x = 7.6 .. 8.4.
    Vector3 alongX = { 0.0f, 0.0f, -PI * 0.5f };
    metal.cylinder( place( { -RoomW, 5.0f, RoomN + 0.35f }, alongX ), 0.14f, 2.0f * RoomW, 20 );
    metal.cylinder( place( { -RoomW, 5.4f, RoomN + 0.3f }, alongX ), 0.09f, 2.0f * RoomW, 16 );
    metal.cylinder( place( { -RoomW, 4.7f, RoomN + 0.25f }, alongX ), 0.06f, RoomW + 7.6f, 12 );
    metal.cylinder( place( { 8.4f, 4.7f, RoomN + 0.25f }, alongX ), 0.06f, RoomW - 8.4f, 12 );
    for ( float x = -13.0f; x < RoomW; x += 2.0f )
        metal.box( place( { x, 5.05f, RoomN + 0.15f } ), { 0.05f, 0.5f, 0.15f } );
    // Wall caps (seen from above).
    metal.box( place( { 0.0f, WallH + 0.04f, RoomN - 0.3f } ), { RoomW + 0.65f, 0.04f, 0.34f } );
    metal.box( place( { RoomW + 0.3f, WallH + 0.04f, 0.2f } ), { 0.34f, 0.04f, 11.8f } );
    metal.box( place( { -RoomW - 0.3f, WallH + 0.04f, 0.2f } ), { 0.34f, 0.04f, 11.8f } );
    addStatic( metal, SurfaceE::METAL );

    MeshBuilder feet;
    for ( float x : { -12.0f, -8.0f, -4.0f, 4.0f, 8.0f, 12.0f } )
        feet.box( place( { x, 0.35f, RoomN + 0.22f } ), { 0.43f, 0.35f, 0.28f } );
    for ( float z : { -8.0f, -4.0f, 0.0f, 4.0f, 8.0f } )
        feet.box( place( { RoomW - 0.22f, 0.35f, z } ), { 0.28f, 0.35f, 0.43f } );
    addStatic( feet, SurfaceE::HAZARD );

    MeshBuilder wall;
    wall.box( place( { 0.0f, WallH * 0.5f, RoomN - 0.3f } ), { RoomW + 0.6f, WallH * 0.5f, 0.3f } );
    wall.box( place( { RoomW + 0.3f, WallH * 0.5f, 0.2f } ), { 0.3f, WallH * 0.5f, 11.8f } );
    // West wall around seven narrow windows.
    wall.box( place( { -RoomW - 0.3f, WindowLow * 0.5f, 0.2f } ), { 0.3f, WindowLow * 0.5f, 11.8f } );
    wall.box( place( { -RoomW - 0.3f, ( WindowHigh + WallH ) * 0.5f, 0.2f } ),
              { 0.3f, ( WallH - WindowHigh ) * 0.5f, 11.8f } );
    float from = -11.6f;
    for ( int i = 0; i <= 7; ++i )
    {
        float to = i < 7 ? WindowZ[i] - WindowHalf : 12.0f;
        wall.box( place( { -RoomW - 0.3f, ( WindowLow + WindowHigh ) * 0.5f, ( from + to ) * 0.5f } ),
                  { 0.3f, ( WindowHigh - WindowLow ) * 0.5f, ( to - from ) * 0.5f } );
        if ( i < 7 ) from = WindowZ[i] + WindowHalf;
    }
    addStatic( wall, SurfaceE::WALL );

    MeshBuilder pipes;
    pipes.box( place( { RoomW - 0.02f, 5.2f, 0.2f } ), { 0.02f, 0.6f, 11.6f } );
    pipes.box( place( { 0.0f, 2.2f, RoomN - 0.02f } ), { RoomW, 0.55f, 0.02f } );
    addStatic( pipes, SurfaceE::PIPES );

    // Only for the shadows: the ceiling and the south wall the camera looks past.
    MeshBuilder hidden;
    hidden.box( place( { 0.0f, WallH + 0.3f, 0.5f } ), { RoomW + 0.6f, 0.3f, 12.2f } );
    hidden.box( place( { 0.0f, WallH * 0.5f, RoomS + 0.3f } ), { RoomW + 0.6f, WallH * 0.5f, 0.3f } );
    addStatic( hidden, SurfaceE::WALL, false );

    // The station's frame below the glass, dark against the planet.
    MeshBuilder truss;
    for ( float x : { -6.0f, 0.0f, 6.0f } ) truss.box( place( { x, -3.0f, -1.0f } ), { 0.2f, 0.3f, 14.0f } );
    for ( float z : { -5.0f, 3.0f } ) truss.box( place( { 0.0f, -3.0f, z } ), { 12.0f, 0.25f, 0.2f } );
    size_t first = m_static.size();
    addStatic( truss, SurfaceE::METAL );
    for ( size_t i = first; i < m_static.size(); ++i ) m_static[i].m_sunShadow = m_static[i].m_spotShadow = false;

    MeshBuilder glass;
    glass.quad( { -GlassX, -0.02f, GlassN }, { GlassX, -0.02f, GlassN }, { GlassX, -0.02f, GlassS },
                { -GlassX, -0.02f, GlassS } );
    m_glass = glass.build().front();

    // Red emergency lamps on the ribs.
    for ( float x : { -12.0f, -4.0f, 4.0f, 12.0f } )
    {
        MeshBuilder lamp;
        lamp.roundedBox( place( { x, 6.2f, RoomN + 0.5f } ), { 0.12f, 0.12f, 0.08f }, 0.05f, 3 );
        m_beacons.push_back( (int)m_static.size() );
        addStatic( lamp, SurfaceE::GLOW, true, { 0.0f, 0.0f, 0.0f, 0.0f } );
    }
    for ( float z : { -8.0f, 8.0f } )
    {
        MeshBuilder lamp;
        lamp.roundedBox( place( { RoomW - 0.5f, 6.2f, z } ), { 0.08f, 0.12f, 0.12f }, 0.05f, 3 );
        m_beacons.push_back( (int)m_static.size() );
        addStatic( lamp, SurfaceE::GLOW, true, { 0.0f, 0.0f, 0.0f, 0.0f } );
    }
    // A cyan strip along the platform's front edge.
    MeshBuilder strip;
    strip.box( place( { 0.0f, 0.5f, -7.555f } ), { 4.3f, 0.015f, 0.01f } );
    addStatic( strip, SurfaceE::GLOW, true, { 0.3f, 1.8f, 3.0f, 0.0f } );
}

void Scene::buildProps()
{
    // The reactor: plinth, a glowing core behind cooling fins, containment rings, a cap, pipes into the ceiling.
    {
        Matrix      at = place( ReactorAt );
        MeshBuilder metal;
        metal.lathe( at,
                     { { 0.0f, 0.0f },
                       { 1.7f, 0.0f },
                       { 1.7f, 0.0f },
                       { 1.7f, 0.35f },
                       { 1.7f, 0.35f },
                       { 1.45f, 0.45f },
                       { 0.0f, 0.45f } },
                     48 );
        for ( float y : { 0.75f, 2.35f, 3.95f } )
            metal.lathe( MatrixMultiply( MatrixTranslate( 0.0f, y, 0.0f ), at ),
                         { { 0.92f, 0.0f },
                           { 1.3f, 0.0f },
                           { 1.3f, 0.0f },
                           { 1.36f, 0.06f },
                           { 1.36f, 0.24f },
                           { 1.3f, 0.3f },
                           { 1.3f, 0.3f },
                           { 0.92f, 0.3f } },
                         48 );
        for ( int i = 0; i < 18; ++i )
        {
            float a = i * 2.0f * PI / 18.0f;
            metal.box(
                MatrixMultiply( place( { cosf( a ) * 1.12f, 2.5f, sinf( a ) * 1.12f }, { 0.0f, -a, 0.0f } ), at ),
                { 0.22f, 1.75f, 0.022f } );
        }
        metal.lathe(
            MatrixMultiply( MatrixTranslate( 0.0f, 4.25f, 0.0f ), at ),
            { { 1.15f, 0.0f }, { 1.2f, 0.1f }, { 1.0f, 0.55f }, { 0.6f, 0.75f }, { 0.6f, 0.75f }, { 0.0f, 0.75f } },
            40 );
        for ( int i = 0; i < 3; ++i )
        {
            float a = i * 2.0f * PI / 3.0f + 0.4f;
            metal.cylinder( MatrixMultiply( MatrixTranslate( cosf( a ) * 0.4f, 4.9f, sinf( a ) * 0.4f ), at ), 0.16f,
                            WallH - 4.9f, 14 );
        }
        // A pipe from the north wall into the reactor.
        metal.cylinder( place( { ReactorAt.x + 0.9f, 1.0f, RoomN }, { PI * 0.5f, 0.0f, 0.0f } ), 0.18f,
                        -RoomN + ReactorAt.z - 1.2f, 14 );
        addStatic( metal, SurfaceE::METAL );
        MeshBuilder core;
        core.lathe( MatrixMultiply( MatrixTranslate( 0.0f, 0.45f, 0.0f ), at ),
                    { { 0.0f, 0.0f }, { 0.85f, 0.0f }, { 0.75f, 2.0f }, { 0.85f, 4.0f }, { 0.0f, 4.0f } }, 40 );
        addStatic( core, SurfaceE::GLOW, true, { 0.5f, 3.2f, 5.0f, 0.0f } );
        m_blockers.push_back( { { ReactorAt.x, ReactorAt.z }, 1.8f } );
    }

    // Cryo pods along the east wall, leaning back; the window glows cold.
    for ( int i = 0; i < 3; ++i )
    {
        Matrix      pod = MatrixMultiply( MatrixRotateZ( 0.1f ), MatrixTranslate( 12.6f, 0.3f, CryoZ[i] ) );
        MeshBuilder metal;
        metal.roundedBox( place( { 12.6f, 0.15f, CryoZ[i] } ), { 0.75f, 0.15f, 0.75f }, 0.05f, 2 );
        metal.lathe( pod,
                     { { 0.0f, 0.0f },
                       { 0.5f, 0.0f },
                       { 0.5f, 0.0f },
                       { 0.6f, 0.15f },
                       { 0.62f, 0.45f },
                       { 0.62f, 1.9f },
                       { 0.55f, 2.15f },
                       { 0.3f, 2.35f },
                       { 0.0f, 2.4f } },
                     32 );
        for ( float y : { 0.45f, 1.95f } )
            metal.lathe( MatrixMultiply( MatrixTranslate( 0.0f, y, 0.0f ), pod ),
                         { { 0.6f, -0.04f },
                           { 0.68f, -0.04f },
                           { 0.68f, -0.04f },
                           { 0.68f, 0.04f },
                           { 0.68f, 0.04f },
                           { 0.6f, 0.04f } },
                         32 );
        metal.cylinder( place( { 13.2f, 2.3f, CryoZ[i] - 0.25f }, { 0.0f, 0.0f, -PI * 0.5f } ), 0.07f, 0.8f, 10 );
        metal.cylinder( place( { 13.2f, 2.0f, CryoZ[i] + 0.25f }, { 0.0f, 0.0f, -PI * 0.5f } ), 0.05f, 0.8f, 10 );
        addStatic( metal, SurfaceE::METAL );
        MeshBuilder window;
        window.lathe( pod, { { 0.635f, 0.55f }, { 0.635f, 1.85f } }, 20, PI * 0.6f, PI * 1.4f );
        m_cryoGlass.push_back( (int)m_static.size() );
        addStatic( window, SurfaceE::GLOW, true, { 0.12f, 0.32f, 0.65f, 0.0f } );
        m_blockers.push_back( { { 12.6f, CryoZ[i] }, 0.8f } );
    }

    // Two consoles by the glass, each with three screens.
    auto console = [&]( Vector3 at, float yaw )
    {
        Matrix      base = MatrixMultiply( MatrixRotateY( yaw ), MatrixTranslate( at.x, at.y, at.z ) );
        auto        local = [&]( Vector3 p, Vector3 rot ) { return MatrixMultiply( place( p, rot ), base ); };
        MeshBuilder metal;
        metal.roundedBox( local( { 0.0f, 0.45f, 0.0f }, {} ), { 0.55f, 0.45f, 0.3f }, 0.04f, 3 );
        addStatic( metal, SurfaceE::METAL );
        MeshBuilder dark;
        dark.roundedBox( local( { 0.0f, 0.95f, 0.14f }, { 0.45f, 0.0f, 0.0f } ), { 0.6f, 0.06f, 0.32f }, 0.02f, 3 );
        Matrix      screens[3] = { local( { 0.0f, 1.48f, -0.12f }, { -0.2f, 0.0f, 0.0f } ),
                                   local( { -0.62f, 1.44f, -0.02f }, { -0.2f, 0.45f, 0.0f } ),
                                   local( { 0.62f, 1.44f, -0.02f }, { -0.2f, -0.45f, 0.0f } ) };
        MeshBuilder glass;
        for ( const Matrix &m : screens )
        {
            dark.roundedBox( m, { 0.32f, 0.22f, 0.03f }, 0.02f, 3 );
            glass.quad( Vector3Transform( { -0.29f, 0.2f, 0.032f }, m ), Vector3Transform( { 0.29f, 0.2f, 0.032f }, m ),
                        Vector3Transform( { 0.29f, -0.2f, 0.032f }, m ),
                        Vector3Transform( { -0.29f, -0.2f, 0.032f }, m ) );
        }
        dark.roundedBox( local( { 0.0f, 1.1f, -0.15f }, {} ), { 0.08f, 0.2f, 0.06f }, 0.02f, 2 );
        addStatic( dark, SurfaceE::DARK );
        addStatic( glass, SurfaceE::SCREEN );
        MeshBuilder buttons;
        for ( int i = 0; i < 6; ++i )
            buttons.box( local( { -0.4f + i * 0.16f, 1.0f, 0.26f }, { 0.45f, 0.0f, 0.0f } ),
                         { 0.05f, 0.012f, 0.025f } );
        addStatic( buttons, SurfaceE::GLOW, true, { 2.4f, 1.2f, 0.25f, 0.0f } );
        m_blockers.push_back( { { at.x, at.z }, 0.75f } );
    };
    console( { -9.5f, 0.0f, 2.2f }, PI * 0.5f );
    console( { 9.8f, 0.0f, -2.5f }, -PI * 0.5f );

    // Crates: rounded boxes in orange paint with steel bands.
    struct Crate
    {
        Vector3 m_at;
        Vector3 m_half;
        float   m_yaw;
    };
    const Crate crates[] = { { { 10.2f, 0.6f, 9.0f }, { 0.9f, 0.6f, 0.7f }, 0.1f },
                             { { 11.6f, 0.5f, 7.8f }, { 0.55f, 0.5f, 0.55f }, -0.3f },
                             { { 10.3f, 1.55f, 9.0f }, { 0.6f, 0.35f, 0.5f }, 0.35f },
                             { { 12.6f, 0.65f, 10.6f }, { 1.3f, 0.65f, 0.65f }, 0.0f },
                             { { -11.5f, 0.7f, 9.8f }, { 1.2f, 0.7f, 0.7f }, 0.05f },
                             { { -11.2f, 1.85f, 9.7f }, { 0.6f, 0.45f, 0.6f }, -0.2f },
                             { { -9.6f, 0.45f, 8.6f }, { 0.45f, 0.45f, 0.45f }, 0.5f },
                             { { -12.6f, 0.4f, 7.6f }, { 0.4f, 0.4f, 0.4f }, 0.2f },
                             { { 5.5f, 0.35f, 8.2f }, { 0.35f, 0.35f, 0.35f }, 0.7f },
                             { { 6.4f, 0.3f, 7.3f }, { 0.5f, 0.3f, 0.3f }, 1.2f } };
    MeshBuilder crate;
    MeshBuilder bands;
    for ( const Crate &c : crates )
    {
        Matrix m = place( c.m_at, { 0.0f, c.m_yaw, 0.0f } );
        crate.roundedBox( m, c.m_half, 0.05f, 2 );
        bands.roundedBox( m, { c.m_half.x + 0.02f, 0.06f, c.m_half.z + 0.02f }, 0.02f, 2 );
        bands.roundedBox( m, { c.m_half.x + 0.015f, c.m_half.y + 0.01f, 0.05f }, 0.02f, 2 );
        if ( c.m_at.y - c.m_half.y < 0.05f )
            m_blockers.push_back( { { c.m_at.x, c.m_at.z }, fmaxf( c.m_half.x, c.m_half.z ) * 0.9f } );
    }
    addStatic( crate, SurfaceE::CRATE );
    addStatic( bands, SurfaceE::METAL );

    // A wrecked maintenance robot, slumped forward, one eye still glowing faintly.
    {
        Matrix      at = MatrixMultiply( MatrixRotateY( 2.4f ), MatrixTranslate( 8.2f, 0.0f, 5.6f ) );
        auto        local = [&]( Vector3 p, Vector3 rot ) { return MatrixMultiply( place( p, rot ), at ); };
        MeshBuilder dark;
        for ( float x : { -0.36f, 0.36f } )
            dark.roundedBox( local( { x, 0.2f, 0.0f }, {} ), { 0.15f, 0.2f, 0.55f }, 0.1f, 3 );
        dark.roundedBox( local( { 0.0f, 0.3f, 0.0f }, {} ), { 0.25f, 0.12f, 0.4f }, 0.04f, 2 );
        addStatic( dark, SurfaceE::DARK );
        MeshBuilder yellow;
        yellow.roundedBox( local( { 0.0f, 0.72f, 0.05f }, { 0.55f, 0.0f, 0.1f } ), { 0.38f, 0.34f, 0.36f }, 0.16f, 4 );
        addStatic( yellow, SurfaceE::PAINT );
        MeshBuilder metal;
        metal.roundedBox( local( { 0.0f, 0.98f, 0.38f }, { 0.7f, 0.0f, 0.1f } ), { 0.16f, 0.1f, 0.13f }, 0.04f, 3 );
        metal.roundedBox( local( { 0.5f, 0.5f, 0.3f }, { 0.3f, 0.0f, -0.9f } ), { 0.05f, 0.35f, 0.05f }, 0.02f, 2 );
        metal.roundedBox( local( { 0.75f, 0.08f, 0.55f }, { 1.4f, 0.4f, 0.0f } ), { 0.045f, 0.3f, 0.045f }, 0.02f, 2 );
        metal.roundedBox( local( { -0.55f, 0.1f, 0.5f }, { 1.5f, -0.5f, 0.0f } ), { 0.05f, 0.4f, 0.05f }, 0.02f, 2 );
        addStatic( metal, SurfaceE::METAL );
        MeshBuilder eye;
        eye.roundedBox( local( { 0.0f, 1.0f, 0.52f }, { 0.7f, 0.0f, 0.1f } ), { 0.05f, 0.03f, 0.01f }, 0.01f, 2 );
        addStatic( eye, SurfaceE::GLOW, true, { 3.0f, 0.2f, 0.1f, 0.0f } );
        m_blockers.push_back( { { 8.2f, 5.6f }, 0.75f } );
    }

    // Debris: a fallen ceiling panel, scraps, cables on the floor.
    {
        MeshBuilder wall;
        wall.box( place( { 5.6f, 0.12f, -8.4f }, { 0.12f, 0.6f, 0.08f } ), { 0.9f, 0.04f, 0.6f } );
        wall.box( place( { -6.2f, 0.06f, -9.6f }, { 0.0f, 1.1f, 0.05f } ), { 0.5f, 0.03f, 0.35f } );
        addStatic( wall, SurfaceE::WALL );
        MeshBuilder metal;
        for ( int i = 0; i < 9; ++i )
            metal.box( place( { 4.0f + rnd() * 5.0f, 0.03f, -9.8f + rnd() * 2.5f }, { 0.0f, rnd() * 3.0f, 0.0f } ),
                       { 0.05f + rnd() * 0.15f, 0.025f, 0.04f + rnd() * 0.1f } );
        // Cables from the west console to the wall and from the reactor across the deck.
        Vector3 cable[] = {
            { -9.2f, 0.04f, 2.0f }, { -10.5f, 0.04f, 3.1f }, { -12.0f, 0.04f, 3.4f }, { -13.9f, 0.04f, 3.0f }
        };
        for ( size_t i = 0; i + 1 < sizeof( cable ) / sizeof( cable[0] ); ++i )
        {
            Vector3 d = Vector3Subtract( cable[i + 1], cable[i] );
            float   len = Vector3Length( d );
            float   yaw = atan2f( d.x, d.z );
            metal.cylinder( MatrixMultiply( MatrixMultiply( MatrixRotateX( PI * 0.5f ), MatrixRotateY( yaw ) ),
                                            MatrixTranslate( cable[i].x, cable[i].y, cable[i].z ) ),
                            0.04f, len, 8 );
        }
        addStatic( metal, SurfaceE::METAL );
    }

    // The monolith: an irregular four-sided spire, twisted and tapering; shards orbit it.
    {
        std::vector<Vector3> base = {
            { 0.75f, 0.0f, 0.1f }, { 0.05f, 0.0f, 0.55f }, { -0.7f, 0.0f, 0.05f }, { 0.0f, 0.0f, -0.6f }
        };
        auto ring = [&]( float y, float scale, float turn )
        {
            std::vector<Vector3> r;
            for ( Vector3 p : base )
                r.push_back( { ( p.x * cosf( turn ) - p.z * sinf( turn ) ) * scale, y,
                               ( p.x * sinf( turn ) + p.z * cosf( turn ) ) * scale } );
            return r;
        };
        MeshBuilder m;
        m.prism( MatrixIdentity(), { ring( 0.0f, 0.9f, 0.0f ), ring( 0.35f, 1.0f, 0.05f ), ring( 2.6f, 0.55f, 0.3f ),
                                     ring( 3.35f, 0.1f, 0.45f ) } );
        m_monolith = m.build();
        MeshBuilder s;
        Vector3     v[4] = {
            { 0.0f, 0.18f, 0.0f }, { 0.12f, -0.06f, 0.05f }, { -0.1f, -0.06f, 0.08f }, { 0.0f, -0.05f, -0.13f }
        };
        int     faces[4][3] = { { 0, 1, 2 }, { 0, 2, 3 }, { 0, 3, 1 }, { 1, 3, 2 } };
        Vector3 c = Vector3Scale( Vector3Add( Vector3Add( v[0], v[1] ), Vector3Add( v[2], v[3] ) ), 0.25f );
        for ( auto &f : faces )
        {
            Vector3 a = v[f[0]];
            Vector3 b = v[f[1]];
            Vector3 d = v[f[2]];
            Vector3 n = Vector3CrossProduct( Vector3Subtract( b, a ), Vector3Subtract( d, a ) );
            if ( Vector3DotProduct( n, Vector3Subtract( a, c ) ) < 0.0f ) std::swap( b, d );
            s.triangle( a, b, d );
        }
        m_shard = s.build();
        MeshBuilder pedestal;
        pedestal.lathe( place( MonolithAt ),
                        { { 1.1f, 0.0f }, { 1.25f, 0.06f }, { 1.4f, 0.06f }, { 1.4f, 0.06f }, { 1.4f, 0.0f } }, 40 );
        addStatic( pedestal, SurfaceE::METAL );
        MeshBuilder glowRing;
        glowRing.lathe( place( { MonolithAt.x, MonolithAt.y + 0.065f, MonolithAt.z } ),
                        { { 1.15f, 0.0f }, { 1.3f, 0.0f } }, 40 );
        addStatic( glowRing, SurfaceE::GLOW, true, { 1.6f, 0.5f, 4.0f, 0.0f } );
        m_blockers.push_back( { { MonolithAt.x, MonolithAt.z }, 1.2f } );
    }
}

float Scene::groundAt( float x, float z ) const
{
    if ( fabsf( x ) <= 4.5f && z >= RoomN && z <= -7.6f ) return 0.6f;
    if ( fabsf( x ) <= 2.0f && z > -7.6f && z <= -7.0f ) return 0.4f;
    if ( fabsf( x ) <= 2.0f && z > -7.0f && z <= -6.4f ) return 0.2f;
    return 0.0f;
}

void Scene::moveHero( float dt, const Controls &in )
{
    Vector2 move = in.m_move;
    if ( Vector2Length( move ) > 1.0f ) move = Vector2Normalize( move );
    m_hero.update( dt, Vector2Scale( move, RunSpeed ), m_time );

    Vector3 &p = m_hero.m_pos;
    float    here = groundAt( p.x, p.z );
    float    nx = p.x + m_hero.m_vel.x * dt;
    float    nz = p.z + m_hero.m_vel.y * dt;
    // Steps of at most 25 cm; otherwise slide along one axis.
    if ( groundAt( nx, nz ) - here <= 0.25f )
    {
        p.x = nx;
        p.z = nz;
    }
    else if ( groundAt( nx, p.z ) - here <= 0.25f )
        p.x = nx;
    else if ( groundAt( p.x, nz ) - here <= 0.25f )
        p.z = nz;
    for ( const Blocker &b : m_blockers )
    {
        Vector2 d = { p.x - b.m_centre.x, p.z - b.m_centre.y };
        float   dist = Vector2Length( d );
        float   min = b.m_radius + HeroRadius;
        if ( dist < min && dist > 1e-4f )
        {
            p.x = b.m_centre.x + d.x / dist * min;
            p.z = b.m_centre.y + d.y / dist * min;
        }
    }
    p.x = Clamp( p.x, -RoomW + 0.6f, RoomW - 0.6f );
    p.z = Clamp( p.z, RoomN + 0.6f, RoomS - 0.4f );
    float g = groundAt( p.x, p.z );
    p.y += ( g - p.y ) * fminf( 1.0f, dt * 15.0f );
}

void Scene::putHero( float x, float z, float yaw )
{
    m_hero.m_pos = { x, groundAt( x, z ), z };
    m_hero.m_yaw = yaw * DEG2RAD;
    m_hero.m_vel = { 0.0f, 0.0f };
    updateCamera( 0.0f, true );
}

void Scene::updateCamera( float dt, bool snap )
{
    Vector3 want =
        Vector3Add( Vector3Add( m_hero.m_pos, { 0.0f, 1.0f, 0.0f } ), Vector3Scale( m_hero.forward(), 1.0f ) );
    if ( snap )
        m_camTarget = want;
    else
        m_camTarget = Vector3Lerp( m_camTarget, want, 1.0f - expf( -dt * 4.0f ) );
    m_cam.target = m_camTarget;
    m_cam.position = Vector3Add( m_camTarget, Vector3Scale( { 0.0f, 11.5f, 8.1f }, m_zoom ) );
    m_cam.up = { 0.0f, 1.0f, 0.0f };
    m_cam.fovy = 38.0f;
    m_cam.projection = CAMERA_PERSPECTIVE;
}

void Scene::fire()
{
    std::vector<Drawable> pose;
    m_hero.pose( pose );   // the muzzle where the rifle is now
    Vector3 dir = m_hero.forward();
    Vector3 spread = { ( rnd() - 0.5f ) * 0.02f, 0.0f, ( rnd() - 0.5f ) * 0.02f };
    m_bolts.push_back(
        { m_hero.muzzle(), Vector3Scale( Vector3Normalize( Vector3Add( dir, spread ) ), 34.0f ), 1.2f } );
    m_flashes.push_back( { m_hero.muzzle(), { 14.0f, 24.0f, 34.0f }, 5.0f, 0.07f, 0.07f } );
    m_particles.push_back( { m_hero.muzzle(), { 0.0f, 0.0f, 0.0f }, 0.06f, 0.06f, 0.32f, { 190, 225, 255, 255 }, 2 } );
    m_hero.recoil();
}

bool Scene::boltHits( Vector3 p, Vector3 &normal ) const
{
    if ( p.x < -RoomW + 0.05f )
    {
        // Through the windows the bolt flies on into space.
        bool window = false;
        for ( float zc : WindowZ )
            window = window || ( p.y > WindowLow && p.y < WindowHigh && fabsf( p.z - zc ) < WindowHalf );
        if ( !window )
        {
            normal = { 1.0f, 0.0f, 0.0f };
            return true;
        }
    }
    if ( p.x > RoomW - 0.05f && p.y < WallH )
    {
        normal = { -1.0f, 0.0f, 0.0f };
        return true;
    }
    if ( p.z < RoomN + 0.05f && p.y < WallH )
    {
        normal = { 0.0f, 0.0f, 1.0f };
        return true;
    }
    if ( p.y < groundAt( p.x, p.z ) )
    {
        normal = { 0.0f, 1.0f, 0.0f };
        return true;
    }
    for ( const Blocker &b : m_blockers )
    {
        Vector2 d = { p.x - b.m_centre.x, p.z - b.m_centre.y };
        if ( p.y < 3.2f && Vector2Length( d ) < b.m_radius )
        {
            Vector2 n = Vector2Normalize( d );
            normal = { n.x, 0.0f, n.y };
            return true;
        }
    }
    return false;
}

void Scene::impact( Vector3 p, Vector3 normal )
{
    for ( int i = 0; i < 26; ++i )
    {
        Vector3 r = Vector3Normalize( { rnd() - 0.5f, rnd() - 0.5f, rnd() - 0.5f } );
        Vector3 v =
            Vector3Scale( Vector3Normalize( Vector3Add( normal, Vector3Scale( r, 0.9f ) ) ), 2.0f + rnd() * 5.0f );
        float life = 0.25f + rnd() * 0.5f;
        m_particles.push_back( { p, v, life, life, 0.012f + rnd() * 0.01f, { 160, 210, 255, 255 }, 0 } );
    }
    m_particles.push_back( { p, { 0.0f, 0.0f, 0.0f }, 0.1f, 0.1f, 0.5f, { 150, 200, 255, 255 }, 2 } );
    m_flashes.push_back(
        { Vector3Add( p, Vector3Scale( normal, 0.3f ) ), { 10.0f, 20.0f, 32.0f }, 6.0f, 0.15f, 0.15f } );
}

void Scene::sparkBurst()
{
    int count = 30 + (int)( rnd() * 30 );
    for ( int i = 0; i < count; ++i )
    {
        Vector3       v = { ( rnd() - 0.5f ) * 2.5f, -rnd() * 1.5f, 0.4f + rnd() * 1.8f };
        float         life = 0.6f + rnd() * 1.1f;
        unsigned char g = (unsigned char)( 150 + rnd() * 80 );
        m_particles.push_back( { Conduit, v, life, life, 0.01f + rnd() * 0.012f, { 255, g, 90, 255 }, 0 } );
    }
    m_sparkLight = 1.0f;
}

void Scene::update( float dt, const Controls &in )
{
    m_time += dt;
    moveHero( dt, in );
    updateCamera( dt, false );

    m_cooldown -= dt;
    if ( in.m_fire && m_cooldown <= 0.0f )
    {
        fire();
        m_cooldown = 0.16f;
    }

    for ( size_t i = 0; i < m_bolts.size(); )
    {
        Bolt   &b = m_bolts[i];
        Vector3 normal;
        b.m_pos = Vector3Add( b.m_pos, Vector3Scale( b.m_vel, dt ) );
        b.m_life -= dt;
        bool hit = boltHits( b.m_pos, normal );
        if ( hit ) impact( b.m_pos, normal );
        if ( hit || b.m_life <= 0.0f )
        {
            m_bolts[i] = m_bolts.back();
            m_bolts.pop_back();
        }
        else
            ++i;
    }

    m_sparkTimer -= dt;
    if ( m_sparkTimer <= 0.0f )
    {
        sparkBurst();
        m_sparkTimer = 1.2f + rnd() * 2.8f;
    }
    m_sparkLight = fmaxf( 0.0f, m_sparkLight - dt * 3.5f );

    // Motes rise around the monolith.
    if ( m_particles.size() < 900 && rnd() < dt * 25.0f )
    {
        float   a = rnd() * 2.0f * PI;
        float   r = 0.9f + rnd() * 0.7f;
        Vector3 p = { MonolithAt.x + cosf( a ) * r, MonolithAt.y + 0.2f + rnd() * 0.4f, MonolithAt.z + sinf( a ) * r };
        float   life = 3.0f + rnd() * 2.5f;
        m_particles.push_back( { p,
                                 { -sinf( a ) * 0.25f, 0.35f + rnd() * 0.35f, cosf( a ) * 0.25f },
                                 life,
                                 life,
                                 0.015f + rnd() * 0.02f,
                                 { 180, 110, 255, 255 },
                                 1 } );
    }

    for ( size_t i = 0; i < m_particles.size(); )
    {
        Particle &p = m_particles[i];
        p.m_life -= dt;
        if ( p.m_kind == 0 )
        {
            p.m_vel.y -= 9.8f * dt;
            p.m_pos = Vector3Add( p.m_pos, Vector3Scale( p.m_vel, dt ) );
            float g = groundAt( p.m_pos.x, p.m_pos.z );
            if ( p.m_pos.y < g )
            {
                p.m_pos.y = g;
                p.m_vel.y = -p.m_vel.y * 0.35f;
                p.m_vel.x *= 0.6f;
                p.m_vel.z *= 0.6f;
            }
        }
        else if ( p.m_kind == 1 )
        {
            Vector3 d = Vector3Subtract( p.m_pos, MonolithAt );
            Vector3 swirl = Vector3Scale( Vector3Normalize( { -d.z, 0.0f, d.x } ), 0.3f * dt );
            p.m_pos = Vector3Add( Vector3Add( p.m_pos, Vector3Scale( p.m_vel, dt ) ), swirl );
        }
        if ( p.m_life <= 0.0f )
        {
            m_particles[i] = m_particles.back();
            m_particles.pop_back();
        }
        else
            ++i;
    }

    for ( Particle &d : m_dust )
    {
        d.m_pos.x += sinf( m_time * 0.31f + d.m_vel.x ) * 0.05f * dt;
        d.m_pos.y += ( sinf( m_time * 0.23f + d.m_vel.y ) * 0.03f + 0.004f ) * dt;
        d.m_pos.z += cosf( m_time * 0.27f + d.m_vel.z ) * 0.05f * dt;
        if ( d.m_pos.y > 6.8f ) d.m_pos.y = 0.2f;
    }

    for ( size_t i = 0; i < m_flashes.size(); )
    {
        m_flashes[i].m_life -= dt;
        if ( m_flashes[i].m_life <= 0.0f )
        {
            m_flashes[i] = m_flashes.back();
            m_flashes.pop_back();
        }
        else
            ++i;
    }

    // The red lamps pulse, the broken cryo pod flickers.
    for ( size_t i = 0; i < m_beacons.size(); ++i )
    {
        float p = 0.15f + 0.85f * pulse( m_time, 2.4f, i * 1.3f );
        m_static[m_beacons[i]].m_emissive = { 9.0f * p, 0.5f * p, 0.25f * p, 0.0f };
    }
    float flicker = hash1( (int)( m_time * 13.0f ) ) > 0.7f ? 0.15f : 1.0f;
    m_static[m_cryoGlass[2]].m_emissive = { 0.12f * flicker, 0.32f * flicker, 0.65f * flicker, 0.0f };
}

Lighting Scene::lighting() const
{
    Lighting L;
    L.m_sunDir = Vector3Normalize( { 0.86f, -0.5f, -0.12f } );
    L.m_sunColor = { 5.0f, 4.6f, 4.0f };
    L.m_spotPos = m_hero.lampPos();
    L.m_spotDir = m_hero.lampDir();
    L.m_spotColor = { 40.0f, 36.0f, 30.0f };
    L.m_spotOuter = 24.0f;
    L.m_spotInner = 11.0f;
    L.m_spotRange = 24.0f;
    L.m_ambUp = { 0.010f, 0.012f, 0.018f };
    L.m_ambDown = { 0.028f, 0.036f, 0.055f };
    L.m_fog = { 0.007f, 0.04f, 0.55f, 2.5f };
    L.m_fogColor = { 0.0015f, 0.002f, 0.003f };
    L.m_roomMin = { -RoomW, 0.0f, RoomN };
    L.m_roomMax = { RoomW, WallH, RoomS };

    auto &P = L.m_points;
    float t = m_time;
    // The hero's light, as in the action RPGs of old: warm, from above, so the figure reads in the dark.
    P.push_back( { Vector3Add( m_hero.m_pos, { 0.4f, 3.2f, 1.2f } ), 7.5f, { 9.0f, 7.6f, 6.2f }, 0.0f } );
    const float beaconX[4] = { -12.0f, -4.0f, 4.0f, 12.0f };
    for ( int i = 0; i < 4; ++i )
    {
        float p = 0.15f + 0.85f * pulse( t, 2.4f, i * 1.3f );
        P.push_back(
            { { beaconX[i], 6.2f, RoomN + 0.7f }, 9.0f, Vector3Scale( { 1.0f, 0.07f, 0.03f }, 28.0f * p ), 1.0f } );
    }
    for ( int i = 0; i < 2; ++i )
    {
        float p = 0.15f + 0.85f * pulse( t, 2.4f, ( i + 4 ) * 1.3f );
        P.push_back( { { RoomW - 0.7f, 6.2f, i ? 8.0f : -8.0f },
                       9.0f,
                       Vector3Scale( { 1.0f, 0.07f, 0.03f }, 28.0f * p ),
                       1.0f } );
    }
    float flicker = hash1( (int)( t * 17.0f ) + 99 ) > 0.75f ? 0.1f : 1.0f;
    P.push_back( { { -6.0f, 6.6f, -4.0f }, 11.0f, { 4.5f, 5.0f, 6.0f }, 0.5f } );
    P.push_back( { { 6.0f, 6.6f, -4.0f }, 11.0f, { 4.5f, 5.0f, 6.0f }, 0.5f } );
    P.push_back( { { -6.0f, 6.6f, 6.0f }, 11.0f, { 4.5f, 5.0f, 6.0f }, 0.5f } );
    P.push_back( { { 6.0f, 6.6f, 6.0f }, 11.0f, Vector3Scale( { 4.5f, 5.0f, 6.0f }, flicker ), 0.5f } );
    float hum = 0.9f + 0.06f * sinf( t * 9.0f ) + 0.04f * sinf( t * 23.0f );
    P.push_back(
        { { ReactorAt.x, 2.5f, ReactorAt.z }, 11.0f, Vector3Scale( { 0.25f, 0.85f, 1.0f }, 70.0f * hum ), 0.8f } );
    float glow = 0.75f + 0.25f * sinf( t * 0.9f );
    P.push_back(
        { { MonolithAt.x, 2.4f, MonolithAt.z }, 12.0f, Vector3Scale( { 0.55f, 0.2f, 1.0f }, 45.0f * glow ), 1.2f } );
    P.push_back( { { -9.15f, 1.5f, 2.2f }, 3.5f, { 2.6f, 4.5f, 6.0f }, 0.3f } );
    P.push_back( { { 9.45f, 1.5f, -2.5f }, 3.5f, { 2.6f, 4.5f, 6.0f }, 0.3f } );
    for ( int i = 0; i < 3; ++i )
    {
        float f = i == 2 ? ( hash1( (int)( t * 13.0f ) ) > 0.7f ? 0.15f : 1.0f ) : 1.0f;
        P.push_back( { { 11.1f, 1.3f, CryoZ[i] }, 4.5f, Vector3Scale( { 0.45f, 0.75f, 1.0f }, 8.0f * f ), 0.6f } );
    }
    if ( m_sparkLight > 0.0f )
        P.push_back( { Vector3Add( Conduit, { 0.0f, -0.2f, 0.3f } ), 7.0f,
                       Vector3Scale( { 1.0f, 0.55f, 0.2f },
                                     40.0f * m_sparkLight * ( 0.6f + 0.4f * hash1( (int)( t * 40.0f ) ) ) ),
                       1.0f } );
    for ( const Bolt &b : m_bolts ) P.push_back( { b.m_pos, 6.0f, { 10.0f, 20.0f, 32.0f }, 1.0f } );
    for ( const Flash &f : m_flashes )
        P.push_back( { f.m_pos, f.m_radius, Vector3Scale( f.m_color, f.m_life / f.m_max ), 1.0f } );
    if ( P.size() > (size_t)MaxLights ) P.resize( MaxLights );
    return L;
}

void Scene::drawParticles( Gfx &gfx )
{
    Vector3 fwd = Vector3Normalize( Vector3Subtract( m_cam.target, m_cam.position ) );
    Vector3 right = Vector3Normalize( Vector3CrossProduct( fwd, m_cam.up ) );
    Vector3 up = Vector3CrossProduct( right, fwd );
    auto    quad = [&]( Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col )
    {
        rlColor4ub( col.r, col.g, col.b, col.a );
        rlTexCoord2f( 0.0f, 0.0f );
        rlVertex3f( a.x, a.y, a.z );
        rlTexCoord2f( 0.0f, 1.0f );
        rlVertex3f( b.x, b.y, b.z );
        rlTexCoord2f( 1.0f, 1.0f );
        rlVertex3f( c.x, c.y, c.z );
        rlTexCoord2f( 1.0f, 0.0f );
        rlVertex3f( d.x, d.y, d.z );
    };
    auto billboard = [&]( Vector3 p, float s, Color col )
    {
        Vector3 r = Vector3Scale( right, s );
        Vector3 u = Vector3Scale( up, s );
        quad( Vector3Add( Vector3Subtract( p, r ), u ), Vector3Subtract( Vector3Subtract( p, r ), u ),
              Vector3Subtract( Vector3Add( p, r ), u ), Vector3Add( Vector3Add( p, r ), u ), col );
    };
    auto streak = [&]( Vector3 head, Vector3 tail, float w, Color col )
    {
        Vector3 axis = Vector3Subtract( head, tail );
        Vector3 side = Vector3CrossProduct( axis, fwd );
        if ( Vector3Length( side ) < 1e-5f ) side = right;
        side = Vector3Scale( Vector3Normalize( side ), w );
        Vector3 ext = Vector3Scale( Vector3Normalize( axis ), w );
        head = Vector3Add( head, ext );
        tail = Vector3Subtract( tail, ext );
        quad( Vector3Subtract( tail, side ), Vector3Add( tail, side ), Vector3Add( head, side ),
              Vector3Subtract( head, side ), col );
    };

    gfx.beginParticles( 1.2f, true );
    rlSetTexture( m_dot.id );
    rlBegin( RL_QUADS );
    for ( const Particle &d : m_dust ) billboard( d.m_pos, d.m_size, d.m_color );
    rlEnd();
    rlSetTexture( 0 );
    gfx.endParticles();

    gfx.beginParticles( 6.0f, false );
    rlSetTexture( m_dot.id );
    rlBegin( RL_QUADS );
    for ( const Particle &p : m_particles )
    {
        float f = p.m_life / p.m_maxLife;
        Color c = p.m_color;
        if ( p.m_kind == 0 )
        {
            c.a = (unsigned char)( 255 * fminf( 1.0f, f * 2.0f ) );
            Vector3 tail = Vector3Subtract( p.m_pos, Vector3Scale( p.m_vel, 0.035f ) );
            streak( p.m_pos, tail, p.m_size, c );
        }
        else if ( p.m_kind == 1 )
        {
            c.a = (unsigned char)( 140 * sinf( f * PI ) );
            billboard( p.m_pos, p.m_size, c );
        }
        else
        {
            c.a = (unsigned char)( 255 * f );
            billboard( p.m_pos, p.m_size * ( 0.6f + 0.4f * f ), c );
        }
    }
    for ( const Bolt &b : m_bolts )
    {
        Vector3 dir = Vector3Normalize( b.m_vel );
        streak( b.m_pos, Vector3Subtract( b.m_pos, Vector3Scale( dir, 0.9f ) ), 0.045f, { 230, 245, 255, 255 } );
        streak( b.m_pos, Vector3Subtract( b.m_pos, Vector3Scale( dir, 1.4f ) ), 0.16f, { 60, 120, 255, 160 } );
        billboard( b.m_pos, 0.35f, { 70, 140, 255, 110 } );
    }
    rlEnd();
    rlSetTexture( 0 );
    gfx.endParticles();
}

void Scene::render( Gfx &gfx, const RenderTexture2D *out )
{
    Lighting L = lighting();
    m_dynamic.clear();
    m_hero.pose( m_dynamic );
    // Lamp and muzzle come from the pose just solved.
    L.m_spotPos = m_hero.lampPos();
    L.m_spotDir = m_hero.lampDir();
    float   bobbing = 0.06f * sinf( m_time * 0.8f );
    Matrix  mono = MatrixMultiply( MatrixRotateY( m_time * 0.12f ),
                                   MatrixTranslate( MonolithAt.x, MonolithAt.y + 0.28f + bobbing, MonolithAt.z ) );
    float   glow = 0.7f + 0.3f * sinf( m_time * 0.9f );
    Vector4 em = { 1.5f * glow, 0.5f * glow, 3.6f * glow, 1.0f };
    for ( Mesh m : m_monolith )
    {
        Drawable d;
        d.m_mesh = m;
        d.m_surface = (int)SurfaceE::OBSIDIAN;
        d.m_transform = mono;
        d.m_emissive = em;
        m_dynamic.push_back( d );
    }
    for ( int i = 0; i < 6; ++i )
    {
        float  a = m_time * 0.35f + i * 2.0f * PI / 6.0f;
        float  r = 1.6f + 0.2f * sinf( m_time * 0.5f + i );
        Matrix s = MatrixMultiply( MatrixRotateXYZ( { m_time * ( 0.4f + i * 0.1f ), m_time * 0.7f + i, 0.3f * i } ),
                                   MatrixTranslate( MonolithAt.x + cosf( a ) * r,
                                                    MonolithAt.y + 1.2f + i * 0.33f + 0.15f * sinf( m_time + i ),
                                                    MonolithAt.z + sinf( a ) * r ) );
        for ( Mesh m : m_shard )
        {
            Drawable d;
            d.m_mesh = m;
            d.m_surface = (int)SurfaceE::OBSIDIAN;
            d.m_transform = s;
            d.m_emissive = { em.x * 1.5f, em.y * 1.5f, em.z * 1.5f, 1.0f };
            m_dynamic.push_back( d );
        }
    }

    gfx.shadows( L, { 0.0f, 2.0f, 0.5f }, { &m_static, &m_dynamic } );
    gfx.beginScene( m_cam, L, m_time, m_nebula );
    // 20 km below, 18 degrees east of the view: its lit western limb crosses the glass.
    Matrix planet = MatrixMultiply( MatrixMultiply( MatrixScale( 8000.0f, 8000.0f, 8000.0f ), MatrixRotateX( 0.45f ) ),
                                    MatrixTranslate( 6180.0f, -15660.0f, -10800.0f ) );
    gfx.planet( m_sphere, planet, m_gasGiant, m_time * 0.0015f );
    gfx.draw( m_static, m_surfaces );
    gfx.draw( m_dynamic, m_surfaces );
    // A soft blob of shadow under the hero (the point lights cast none).
    rlDrawRenderBatchActive();
    BeginBlendMode( BLEND_ALPHA );
    rlDisableDepthMask();
    rlSetTexture( m_dot.id );
    rlBegin( RL_QUADS );
    rlColor4ub( 0, 0, 0, 170 );
    Vector3 h = m_hero.m_pos;
    float   r = 0.75f;
    float   y = h.y + 0.012f;
    rlTexCoord2f( 0.0f, 0.0f );
    rlVertex3f( h.x - r, y, h.z - r );
    rlTexCoord2f( 0.0f, 1.0f );
    rlVertex3f( h.x - r, y, h.z + r );
    rlTexCoord2f( 1.0f, 1.0f );
    rlVertex3f( h.x + r, y, h.z + r );
    rlTexCoord2f( 1.0f, 0.0f );
    rlVertex3f( h.x + r, y, h.z - r );
    rlEnd();
    rlSetTexture( 0 );
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
    gfx.glass( m_glass, MatrixIdentity() );
    drawParticles( gfx );
    gfx.endScene();
    gfx.post( out );
}

}   // namespace spaceopera
