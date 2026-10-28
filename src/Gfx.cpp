/**
 * @file Gfx.cpp
 * @brief The renderer: targets, shadow passes, light uniforms, post chain.
 * @author Rene Weller
 */
#include "Gfx.h"

#include "Shaders.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace spaceopera
{

namespace
{

constexpr int SunUnit = 12;      ///< texture unit of the sun's shadow map
constexpr int SpotUnit = 13;     ///< texture unit of the lamp's shadow map
constexpr int BloomLevels = 6;   ///< levels of the bloom chain

/** @brief Linear filtering and clamping for a target's texture. */
void linearClamp( unsigned int id )
{
    rlTextureParameters( id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR );
    rlTextureParameters( id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR );
    rlTextureParameters( id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP );
    rlTextureParameters( id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP );
}

}   // namespace

Shader Gfx::load( const char *vs, const char *fs )
{
    return LoadShaderFromMemory( vs, fs );
}

Gfx::CommonLocs Gfx::locate( Shader s )
{
    CommonLocs l;
    l.m_viewPos = GetShaderLocation( s, "viewPos" );
    l.m_time = GetShaderLocation( s, "time" );
    l.m_frame = GetShaderLocation( s, "frame" );
    l.m_sunDir = GetShaderLocation( s, "sunDir" );
    l.m_sunColor = GetShaderLocation( s, "sunColor" );
    l.m_sunVP = GetShaderLocation( s, "sunVP" );
    l.m_sunShadow = GetShaderLocation( s, "sunShadow" );
    l.m_sunTexel = GetShaderLocation( s, "sunTexel" );
    l.m_spotPos = GetShaderLocation( s, "spotPos" );
    l.m_spotDir = GetShaderLocation( s, "spotDir" );
    l.m_spotColor = GetShaderLocation( s, "spotColor" );
    l.m_spotCos = GetShaderLocation( s, "spotCos" );
    l.m_spotRange = GetShaderLocation( s, "spotRange" );
    l.m_spotVP = GetShaderLocation( s, "spotVP" );
    l.m_spotShadow = GetShaderLocation( s, "spotShadow" );
    l.m_spotNF = GetShaderLocation( s, "spotNF" );
    l.m_lightCount = GetShaderLocation( s, "lightCount" );
    l.m_lightPos = GetShaderLocation( s, "lightPos" );
    l.m_lightColor = GetShaderLocation( s, "lightColor" );
    l.m_ambUp = GetShaderLocation( s, "ambUp" );
    l.m_ambDown = GetShaderLocation( s, "ambDown" );
    l.m_fogParams = GetShaderLocation( s, "fogParams" );
    l.m_fogColor = GetShaderLocation( s, "fogColor" );
    l.m_roomMin = GetShaderLocation( s, "roomMin" );
    l.m_roomMax = GetShaderLocation( s, "roomMax" );
    l.m_waterCount = GetShaderLocation( s, "waterCount" );
    l.m_waterBalls = GetShaderLocation( s, "waterBalls" );
    l.m_fogSteps = GetShaderLocation( s, "fogSteps" );
    l.m_shadowTaps = GetShaderLocation( s, "shadowTaps" );
    int unit = SunUnit;
    SetShaderValue( s, l.m_sunShadow, &unit, SHADER_UNIFORM_INT );
    unit = SpotUnit;
    SetShaderValue( s, l.m_spotShadow, &unit, SHADER_UNIFORM_INT );
    return l;
}

Gfx::DrawLocs Gfx::drawLocs( Shader s )
{
    DrawLocs l;
    l.m_uv = GetShaderLocation( s, "uvScale" );
    l.m_params = GetShaderLocation( s, "matParams" );
    l.m_emissive = GetShaderLocation( s, "emissive" );
    l.m_mask = GetShaderLocation( s, "emissiveMask" );
    l.m_variation = GetShaderLocation( s, "variation" );
    l.m_cutaway = GetShaderLocation( s, "cutaway" );
    l.m_tint = GetShaderLocation( s, "drawTint" );
    l.m_cutCentre = GetShaderLocation( s, "cutCentre" );
    l.m_cutRadius = GetShaderLocation( s, "cutRadius" );
    return l;
}

void Gfx::upload( Shader s, const CommonLocs &l )
{
    const Lighting &L = m_lights;
    SetShaderValue( s, l.m_viewPos, &m_cam.position, SHADER_UNIFORM_VEC3 );
    SetShaderValue( s, l.m_time, &m_time, SHADER_UNIFORM_FLOAT );
    SetShaderValue( s, l.m_frame, &m_frame, SHADER_UNIFORM_FLOAT );
    SetShaderValue( s, l.m_sunDir, &L.m_sunDir, SHADER_UNIFORM_VEC3 );
    SetShaderValue( s, l.m_sunColor, &L.m_sunColor, SHADER_UNIFORM_VEC3 );
    SetShaderValueMatrix( s, l.m_sunVP, m_sunVP );
    float texel = 1.0f / m_sunSize;
    SetShaderValue( s, l.m_sunTexel, &texel, SHADER_UNIFORM_FLOAT );
    SetShaderValue( s, l.m_spotPos, &L.m_spotPos, SHADER_UNIFORM_VEC3 );
    SetShaderValue( s, l.m_spotDir, &L.m_spotDir, SHADER_UNIFORM_VEC3 );
    SetShaderValue( s, l.m_spotColor, &L.m_spotColor, SHADER_UNIFORM_VEC3 );
    Vector2 cosines = { cosf( L.m_spotOuter * DEG2RAD ), cosf( L.m_spotInner * DEG2RAD ) };
    SetShaderValue( s, l.m_spotCos, &cosines, SHADER_UNIFORM_VEC2 );
    SetShaderValue( s, l.m_spotRange, &L.m_spotRange, SHADER_UNIFORM_FLOAT );
    SetShaderValueMatrix( s, l.m_spotVP, m_spotVP );
    SetShaderValue( s, l.m_spotNF, &m_spotNF, SHADER_UNIFORM_VEC2 );
    int     count = (int)std::min( L.m_points.size(), (size_t)MaxLights );
    Vector4 pos[MaxLights];
    Vector4 col[MaxLights];
    for ( int i = 0; i < count; ++i )
    {
        const PointLight &p = L.m_points[i];
        pos[i] = { p.m_pos.x, p.m_pos.y, p.m_pos.z, p.m_radius };
        col[i] = { p.m_color.x, p.m_color.y, p.m_color.z, p.m_halo };
    }
    SetShaderValue( s, l.m_lightCount, &count, SHADER_UNIFORM_INT );
    if ( count > 0 )
    {
        SetShaderValueV( s, l.m_lightPos, pos, SHADER_UNIFORM_VEC4, count );
        SetShaderValueV( s, l.m_lightColor, col, SHADER_UNIFORM_VEC4, count );
    }
    SetShaderValue( s, l.m_ambUp, &L.m_ambUp, SHADER_UNIFORM_VEC3 );
    SetShaderValue( s, l.m_ambDown, &L.m_ambDown, SHADER_UNIFORM_VEC3 );
    SetShaderValue( s, l.m_fogParams, &L.m_fog, SHADER_UNIFORM_VEC4 );
    SetShaderValue( s, l.m_fogColor, &L.m_fogColor, SHADER_UNIFORM_VEC3 );
    SetShaderValue( s, l.m_roomMin, &L.m_roomMin, SHADER_UNIFORM_VEC3 );
    SetShaderValue( s, l.m_roomMax, &L.m_roomMax, SHADER_UNIFORM_VEC3 );
    int water = (int)std::min( L.m_water.size(), (size_t)8 );
    SetShaderValue( s, l.m_waterCount, &water, SHADER_UNIFORM_INT );
    if ( water > 0 ) SetShaderValueV( s, l.m_waterBalls, L.m_water.data(), SHADER_UNIFORM_VEC4, water );
    SetShaderValue( s, l.m_fogSteps, &m_fogSteps, SHADER_UNIFORM_INT );
    SetShaderValue( s, l.m_shadowTaps, &m_shadowTaps, SHADER_UNIFORM_INT );
}

RenderTexture2D Gfx::hdrTarget( int w, int h, bool depth )
{
    RenderTexture2D t = {};
    t.id = rlLoadFramebuffer();
    rlEnableFramebuffer( t.id );
    t.texture.id = rlLoadTexture( nullptr, w, h, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1 );
    t.texture.width = w;
    t.texture.height = h;
    t.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    t.texture.mipmaps = 1;
    rlFramebufferAttach( t.id, t.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0 );
    if ( depth )
    {
        t.depth.id = rlLoadTextureDepth( w, h, true );
        t.depth.width = w;
        t.depth.height = h;
        rlFramebufferAttach( t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0 );
    }
    if ( !rlFramebufferComplete( t.id ) ) TraceLog( LOG_ERROR, "GFX: HDR target %dx%d incomplete", w, h );
    rlDisableFramebuffer();
    linearClamp( t.texture.id );
    return t;
}

RenderTexture2D Gfx::depthTarget( int size )
{
    RenderTexture2D t = {};
    t.id = rlLoadFramebuffer();
    rlEnableFramebuffer( t.id );
    t.texture.width = size;
    t.texture.height = size;
    t.depth.id = rlLoadTextureDepth( size, size, false );
    t.depth.width = size;
    t.depth.height = size;
    t.depth.format = 19;   // DEPTH_COMPONENT_24BIT
    t.depth.mipmaps = 1;
    rlFramebufferAttach( t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0 );
    if ( !rlFramebufferComplete( t.id ) ) TraceLog( LOG_ERROR, "GFX: shadow map %d incomplete", size );
    rlDisableFramebuffer();
    rlTextureParameters( t.depth.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST );
    rlTextureParameters( t.depth.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST );
    rlTextureParameters( t.depth.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP );
    rlTextureParameters( t.depth.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP );
    return t;
}

void Gfx::init( int width, int height, int quality, float ss )
{
    m_width = width;
    m_height = height;
    // Both sides everywhere: the fullscreen quads are wound backwards, and shadows must not leak through a face.
    rlDisableBackfaceCulling();
    std::string common = CommonFs;
    m_lit = load( LitVs, ( common + LitFs ).c_str() );
    m_glassShader = load( LitVs, ( common + GlassFs ).c_str() );
    m_waterShader = load( LitVs, ( common + WaterFs ).c_str() );
    m_sky = load( FullscreenVs, ( common + SkyFs ).c_str() );
    m_planet = load( LitVs, ( common + PlanetFs ).c_str() );
    m_particle = load( ParticleVs, ( common + ParticleFs ).c_str() );
    m_depth = load( DepthVs, DepthFs );
    m_prefilter = load( FullscreenVs, PrefilterFs );
    m_down = load( FullscreenVs, DownFs );
    m_up = load( FullscreenVs, UpFs );
    m_final = load( FullscreenVs, FinalFs );

    m_litSkin = load( LitSkinVs, ( common + LitFs ).c_str() );
    m_depthSkin = load( DepthSkinVs, DepthFs );

    m_lit.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation( m_lit, "emissionMap" );
    m_litSkin.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation( m_litSkin, "emissionMap" );
    m_litLocs = locate( m_lit );
    m_litSkinLocs = locate( m_litSkin );
    m_glassLocs = locate( m_glassShader );
    m_waterLocs = locate( m_waterShader );
    m_skyLocs = locate( m_sky );
    m_planetLocs = locate( m_planet );
    m_particleLocs = locate( m_particle );
    m_litDraw = drawLocs( m_lit );
    m_skinDraw = drawLocs( m_litSkin );

    m_depthMat = LoadMaterialDefault();
    m_depthMat.shader = m_depth;
    m_depthSkinMat = LoadMaterialDefault();
    m_depthSkinMat.shader = m_depthSkin;
    m_glassMat = LoadMaterialDefault();
    m_glassMat.shader = m_glassShader;
    m_waterMat = LoadMaterialDefault();
    m_waterMat.shader = m_waterShader;
    m_planetMat = LoadMaterialDefault();
    m_planetMat.shader = m_planet;
    setQuality( quality, ss );
}

void Gfx::setQuality( int quality, float ss )
{
    struct Level
    {
        float m_ss;
        int   m_fogSteps, m_shadowTaps, m_sunSize, m_spotSize;
    };
    static const Level Levels[3] = { { 0.75f, 8, 4, 2048, 1024 },
                                     { 1.0f, 20, 8, 4096, 2048 },
                                     { 1.5f, 40, 12, 4096, 2048 } };
    m_quality = std::clamp( quality, 0, 2 );
    const Level &q = Levels[m_quality];
    m_scale = ss > 0.0f ? ss : q.m_ss;
    m_fogSteps = q.m_fogSteps;
    m_shadowTaps = q.m_shadowTaps;
    if ( m_hdr.id > 0 )
    {
        UnloadRenderTexture( m_hdr );
        for ( RenderTexture2D &t : m_bloomLevels ) UnloadRenderTexture( t );
        UnloadRenderTexture( m_sunMap );
        UnloadRenderTexture( m_spotMap );
        m_bloomLevels.clear();
    }
    int w = std::max( (int)( m_width * m_scale ), 1 );
    int h = std::max( (int)( m_height * m_scale ), 1 );
    m_hdr = hdrTarget( w, h, true );
    for ( int i = 0; i < BloomLevels; ++i )
    {
        w = std::max( w / 2, 1 );
        h = std::max( h / 2, 1 );
        m_bloomLevels.push_back( hdrTarget( w, h, false ) );
    }
    m_sunSize = q.m_sunSize;
    m_spotSize = q.m_spotSize;
    m_sunMap = depthTarget( m_sunSize );
    m_spotMap = depthTarget( m_spotSize );
}

Surface Gfx::surface( Texture2D albedo, Texture2D orm, Texture2D normal, Texture2D emission )
{
    Surface s;
    s.m_material = LoadMaterialDefault();
    s.m_material.shader = m_lit;
    s.m_material.maps[MATERIAL_MAP_ALBEDO].texture = albedo;
    s.m_material.maps[MATERIAL_MAP_METALNESS].texture = orm;
    s.m_material.maps[MATERIAL_MAP_NORMAL].texture = normal;
    if ( emission.id > 0 ) s.m_material.maps[MATERIAL_MAP_EMISSION].texture = emission;
    return s;
}

void Gfx::bindShadows( bool on )
{
    rlActiveTextureSlot( SunUnit );
    if ( on )
        rlEnableTexture( m_sunMap.depth.id );
    else
        rlDisableTexture();
    rlActiveTextureSlot( SpotUnit );
    if ( on )
        rlEnableTexture( m_spotMap.depth.id );
    else
        rlDisableTexture();
    rlActiveTextureSlot( 0 );
}

Gfx::Frustum Gfx::frustum( const Matrix &vp )
{
    // Gribb and Hartmann: the planes are sums and differences of the matrix's rows.
    Vector4 r0 = { vp.m0, vp.m4, vp.m8, vp.m12 }, r1 = { vp.m1, vp.m5, vp.m9, vp.m13 };
    Vector4 r2 = { vp.m2, vp.m6, vp.m10, vp.m14 }, r3 = { vp.m3, vp.m7, vp.m11, vp.m15 };
    Frustum f = { { Vector4Add( r3, r0 ), Vector4Subtract( r3, r0 ), Vector4Add( r3, r1 ), Vector4Subtract( r3, r1 ),
                    Vector4Add( r3, r2 ), Vector4Subtract( r3, r2 ) } };
    for ( Vector4 &p : f.m_planes )
        p = Vector4Scale( p, 1.0f / std::max( sqrtf( p.x * p.x + p.y * p.y + p.z * p.z ), 1e-6f ) );
    return f;
}

bool Gfx::inside( const Frustum &f, const Drawable &d )
{
    const Mesh   &m = d.m_mesh;
    std::uint64_t key = (std::uint64_t)(std::uintptr_t)m.vertices * 31u + m.vaoId;
    Sphere       &s = m_spheres[key];
    if ( s.m_count != m.vertexCount || s.m_sphere.w == 0.0f )
    {
        // Measured once per mesh: the middle and half the diagonal of its box.
        s = { { 0.0f, 0.0f, 0.0f, -1.0f }, m.vertexCount };
        if ( m.vertices && m.vertexCount > 0 )
        {
            BoundingBox b = GetMeshBoundingBox( m );
            Vector3     mid = Vector3Scale( Vector3Add( b.min, b.max ), 0.5f );
            s.m_sphere = { mid.x, mid.y, mid.z, std::max( Vector3Distance( b.min, b.max ) * 0.5f, 0.01f ) };
        }
    }
    if ( s.m_sphere.w < 0.0f ) return true;
    const Matrix &t = d.m_transform;
    Vector3       c = Vector3Transform( { s.m_sphere.x, s.m_sphere.y, s.m_sphere.z }, t );
    float scale = sqrtf( std::max( { t.m0 * t.m0 + t.m1 * t.m1 + t.m2 * t.m2, t.m4 * t.m4 + t.m5 * t.m5 + t.m6 * t.m6,
                                     t.m8 * t.m8 + t.m9 * t.m9 + t.m10 * t.m10 } ) );
    // A skinned figure's limbs reach past its rest pose.
    float r = s.m_sphere.w * scale * ( d.m_bones ? 1.6f : 1.0f );
    for ( const Vector4 &p : f.m_planes )
        if ( p.x * c.x + p.y * c.y + p.z * c.z + p.w < -r ) return false;
    return true;
}

void Gfx::shadows( const Lighting &lights, Vector3 centre, const std::vector<const std::vector<Drawable> *> &lists )
{
    bindShadows( false );
    m_lights = lights;

    Camera3D sun = {};
    sun.target = centre;
    sun.position = Vector3Subtract( centre, Vector3Scale( lights.m_sunDir, 70.0f ) );
    sun.up = { 0.0f, 1.0f, 0.0f };
    sun.fovy = m_sunExtent;
    sun.projection = CAMERA_ORTHOGRAPHIC;
    BeginTextureMode( m_sunMap );
    rlClearScreenBuffers();
    rlSetClipPlanes( 1.0, 160.0 );
    BeginMode3D( sun );
    m_sunVP = MatrixMultiply( rlGetMatrixModelview(), rlGetMatrixProjection() );
    Frustum cull = frustum( m_sunVP );
    // A skinned figure casts the shadow of its pose, not of its rest pose. What lies outside the map is left out.
    auto depth = [&]( const Drawable &d )
    {
        if ( !inside( cull, d ) ) return;
        if ( !d.m_bones )
        {
            DrawMesh( d.m_mesh, m_depthMat, d.m_transform );
            return;
        }
        Mesh mesh = d.m_mesh;
        mesh.boneMatrices = const_cast<Matrix *>( d.m_bones );
        mesh.boneCount = d.m_boneCount;
        DrawMesh( mesh, m_depthSkinMat, d.m_transform );
    };
    if ( m_sunShadows )
        for ( const auto *list : lists )
            for ( const Drawable &d : *list )
                if ( d.m_sunShadow ) depth( d );
    EndMode3D();
    EndTextureMode();

    Camera3D spot = {};
    spot.position = lights.m_spotPos;
    spot.target = Vector3Add( lights.m_spotPos, lights.m_spotDir );
    spot.up = fabsf( lights.m_spotDir.y ) > 0.95f ? Vector3{ 0.0f, 0.0f, 1.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
    spot.fovy = lights.m_spotOuter * 2.0f + 6.0f;
    spot.projection = CAMERA_PERSPECTIVE;
    m_spotNF = { 0.1f, lights.m_spotRange };
    BeginTextureMode( m_spotMap );
    rlClearScreenBuffers();
    rlSetClipPlanes( m_spotNF.x, m_spotNF.y );
    BeginMode3D( spot );
    m_spotVP = MatrixMultiply( rlGetMatrixModelview(), rlGetMatrixProjection() );
    cull = frustum( m_spotVP );
    for ( const auto *list : lists )
        for ( const Drawable &d : *list )
            if ( d.m_spotShadow ) depth( d );
    EndMode3D();
    EndTextureMode();
}

void Gfx::fullscreen( Shader s, Texture2D src, int w, int h, Texture2D extra, int extraLoc )
{
    BeginShaderMode( s );
    // After BeginShaderMode: its batch flush forgets the extra texture slots.
    if ( extraLoc >= 0 ) SetShaderValueTexture( s, extraLoc, extra );
    DrawTexturePro( src, { 0.0f, 0.0f, (float)src.width, (float)src.height }, { 0.0f, 0.0f, (float)w, (float)h },
                    { 0.0f, 0.0f }, 0.0f, WHITE );
    EndShaderMode();
}

void Gfx::beginScene( const Camera3D &cam, const Lighting &lights, float time, Texture2D sky )
{
    m_cam = cam;
    m_lights = lights;
    m_time = time;
    m_frame += 1.0f;
    int w = m_hdr.texture.width;
    int h = m_hdr.texture.height;
    BeginTextureMode( m_hdr );
    ClearBackground( BLACK );
    upload( m_lit, m_litLocs );
    upload( m_litSkin, m_litSkinLocs );
    upload( m_glassShader, m_glassLocs );
    upload( m_waterShader, m_waterLocs );
    upload( m_sky, m_skyLocs );
    upload( m_planet, m_planetLocs );
    upload( m_particle, m_particleLocs );
    bindShadows( true );
    for ( auto [shader, locs] : { std::pair<Shader, DrawLocs>{ m_lit, m_litDraw }, { m_litSkin, m_skinDraw } } )
    {
        SetShaderValue( shader, locs.m_cutCentre, &m_cutCentre, SHADER_UNIFORM_VEC3 );
        SetShaderValue( shader, locs.m_cutRadius, &m_cutRadius, SHADER_UNIFORM_FLOAT );
    }

    Matrix view = MatrixLookAt( cam.position, cam.target, cam.up );
    Matrix proj = MatrixPerspective( cam.fovy * DEG2RAD, (double)w / h, 0.5, 30000.0 );
    Matrix inv = MatrixInvert( MatrixMultiply( view, proj ) );
    m_view = frustum( MatrixMultiply( view, proj ) );
    SetShaderValueMatrix( m_sky, GetShaderLocation( m_sky, "invViewProj" ), inv );
    float nebula = 0.8f;
    SetShaderValue( m_sky, GetShaderLocation( m_sky, "nebula" ), &nebula, SHADER_UNIFORM_FLOAT );
    rlDisableDepthMask();
    rlDisableDepthTest();
    fullscreen( m_sky, sky, w, h );
    rlEnableDepthTest();
    rlEnableDepthMask();

    rlSetClipPlanes( 0.5, 30000.0 );
    BeginMode3D( cam );
}

void Gfx::draw( const std::vector<Drawable> &list, const std::vector<Surface> &surfaces )
{
    for ( const Drawable &d : list )
    {
        if ( !d.m_visible || !inside( m_view, d ) ) continue;
        const Surface  &s = surfaces[d.m_surface];
        bool            skin = d.m_bones != nullptr;
        Shader          shader = skin ? m_litSkin : m_lit;
        const DrawLocs &l = skin ? m_skinDraw : m_litDraw;
        SetShaderValue( shader, l.m_uv, &s.m_uvScale, SHADER_UNIFORM_FLOAT );
        SetShaderValue( shader, l.m_params, &s.m_params, SHADER_UNIFORM_VEC4 );
        const Vector4 &em = d.m_emissive.x >= 0.0f ? d.m_emissive : s.m_emissive;
        SetShaderValue( shader, l.m_emissive, &em, SHADER_UNIFORM_VEC4 );
        SetShaderValue( shader, l.m_mask, &s.m_mask, SHADER_UNIFORM_VEC2 );
        SetShaderValue( shader, l.m_variation, &s.m_variation, SHADER_UNIFORM_FLOAT );
        float cut = d.m_cutaway ? 1.0f : 0.0f;
        SetShaderValue( shader, l.m_cutaway, &cut, SHADER_UNIFORM_FLOAT );
        SetShaderValue( shader, l.m_tint, &d.m_tint, SHADER_UNIFORM_VEC4 );
        s.m_material.maps[MATERIAL_MAP_ALBEDO].color = s.m_tint;
        if ( !skin )
        {
            // A stain multiplies what is drawn under it.
            bool multiply = s.m_params.w > 3.5f;
            if ( multiply )
            {
                rlSetBlendFactors( RL_DST_COLOR, RL_ZERO, RL_FUNC_ADD );
                rlSetBlendMode( RL_BLEND_CUSTOM );
            }
            DrawMesh( d.m_mesh, s.m_material, d.m_transform );
            if ( multiply ) rlSetBlendMode( RL_BLEND_ALPHA );
            continue;
        }
        // A skinned figure: its own bones on the shared mesh, the skinning shader with the surface's maps.
        Mesh     mesh = d.m_mesh;
        Material mat = s.m_material;
        mesh.boneMatrices = const_cast<Matrix *>( d.m_bones );
        mesh.boneCount = d.m_boneCount;
        mat.shader = m_litSkin;
        DrawMesh( mesh, mat, d.m_transform );
    }
}

void Gfx::planet( const Mesh &mesh, const Matrix &transform, Texture2D map, float spin, float glow )
{
    m_planetMat.maps[MATERIAL_MAP_ALBEDO].texture = map;
    SetShaderValue( m_planet, GetShaderLocation( m_planet, "spin" ), &spin, SHADER_UNIFORM_FLOAT );
    SetShaderValue( m_planet, GetShaderLocation( m_planet, "glow" ), &glow, SHADER_UNIFORM_FLOAT );
    DrawMesh( mesh, m_planetMat, transform );
}

void Gfx::ring( const Mesh &mesh, Texture2D map, float radius )
{
    static int loc = -2;
    if ( loc == -2 ) loc = GetShaderLocation( m_planet, "ringMode" );
    m_planetMat.maps[MATERIAL_MAP_ALBEDO].texture = map;
    if ( loc >= 0 ) SetShaderValue( m_planet, loc, &radius, SHADER_UNIFORM_FLOAT );
    DrawMesh( mesh, m_planetMat, MatrixIdentity() );
    float off = 0.0f;
    if ( loc >= 0 ) SetShaderValue( m_planet, loc, &off, SHADER_UNIFORM_FLOAT );
}

void Gfx::glass( const Mesh &mesh, const Matrix &transform, float ice )
{
    static int iceLoc = -2;
    if ( iceLoc == -2 ) iceLoc = GetShaderLocation( m_glassShader, "iceMode" );
    if ( iceLoc >= 0 ) SetShaderValue( m_glassShader, iceLoc, &ice, SHADER_UNIFORM_FLOAT );
    BeginBlendMode( BLEND_ALPHA_PREMULTIPLY );
    rlDisableDepthMask();
    DrawMesh( mesh, m_glassMat, transform );
    rlEnableDepthMask();
    EndBlendMode();
}

void Gfx::water( const Mesh &mesh, const Matrix &transform )
{
    BeginBlendMode( BLEND_ALPHA_PREMULTIPLY );
    rlDisableDepthMask();
    DrawMesh( mesh, m_waterMat, transform );
    rlEnableDepthMask();
    EndBlendMode();
}

void Gfx::beginParticles( float boost, bool lit )
{
    rlDrawRenderBatchActive();
    BeginShaderMode( m_particle );
    int litFlag = lit ? 1 : 0;
    SetShaderValue( m_particle, GetShaderLocation( m_particle, "boost" ), &boost, SHADER_UNIFORM_FLOAT );
    SetShaderValue( m_particle, GetShaderLocation( m_particle, "lit" ), &litFlag, SHADER_UNIFORM_INT );
    BeginBlendMode( BLEND_ADDITIVE );
    rlDisableDepthMask();
}

void Gfx::endParticles()
{
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
    EndShaderMode();
}

void Gfx::endScene()
{
    EndMode3D();
    EndTextureMode();
}

void Gfx::post( const RenderTexture2D *out )
{
    Texture2D src = m_hdr.texture;
    float     threshold = 1.2f;
    BeginTextureMode( m_bloomLevels[0] );
    Vector2 texel = { 1.0f / src.width, 1.0f / src.height };
    SetShaderValue( m_prefilter, GetShaderLocation( m_prefilter, "texel" ), &texel, SHADER_UNIFORM_VEC2 );
    SetShaderValue( m_prefilter, GetShaderLocation( m_prefilter, "threshold" ), &threshold, SHADER_UNIFORM_FLOAT );
    fullscreen( m_prefilter, src, m_bloomLevels[0].texture.width, m_bloomLevels[0].texture.height );
    EndTextureMode();
    for ( size_t i = 1; i < m_bloomLevels.size(); ++i )
    {
        Texture2D from = m_bloomLevels[i - 1].texture;
        texel = { 1.0f / from.width, 1.0f / from.height };
        BeginTextureMode( m_bloomLevels[i] );
        SetShaderValue( m_down, GetShaderLocation( m_down, "texel" ), &texel, SHADER_UNIFORM_VEC2 );
        fullscreen( m_down, from, m_bloomLevels[i].texture.width, m_bloomLevels[i].texture.height );
        EndTextureMode();
    }
    for ( size_t i = m_bloomLevels.size() - 1; i > 0; --i )
    {
        Texture2D from = m_bloomLevels[i].texture;
        texel = { 1.0f / from.width, 1.0f / from.height };
        BeginTextureMode( m_bloomLevels[i - 1] );
        BeginBlendMode( BLEND_ADDITIVE );
        SetShaderValue( m_up, GetShaderLocation( m_up, "texel" ), &texel, SHADER_UNIFORM_VEC2 );
        fullscreen( m_up, from, m_bloomLevels[i - 1].texture.width, m_bloomLevels[i - 1].texture.height );
        EndBlendMode();
        EndTextureMode();
    }

    int w = out ? out->texture.width : GetScreenWidth();
    int h = out ? out->texture.height : GetScreenHeight();
    if ( out ) BeginTextureMode( *out );
    Vector2 res = { (float)w, (float)h };
    SetShaderValue( m_final, GetShaderLocation( m_final, "exposure" ), &m_exposure, SHADER_UNIFORM_FLOAT );
    SetShaderValue( m_final, GetShaderLocation( m_final, "grade" ), &m_grade, SHADER_UNIFORM_VEC4 );
    SetShaderValue( m_final, GetShaderLocation( m_final, "gradeTint" ), &m_gradeTint, SHADER_UNIFORM_VEC3 );
    SetShaderValue( m_final, GetShaderLocation( m_final, "bloom" ), &m_bloom, SHADER_UNIFORM_FLOAT );
    SetShaderValue( m_final, GetShaderLocation( m_final, "time" ), &m_time, SHADER_UNIFORM_FLOAT );
    SetShaderValue( m_final, GetShaderLocation( m_final, "resolution" ), &res, SHADER_UNIFORM_VEC2 );
    fullscreen( m_final, src, w, h, m_bloomLevels[0].texture, GetShaderLocation( m_final, "bloomTex" ) );
    if ( out ) EndTextureMode();
}

}   // namespace spaceopera
