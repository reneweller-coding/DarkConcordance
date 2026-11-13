/**
 * @file Gfx.cpp
 * @brief The renderer: targets, shadow passes (the sun's kept for what does not move), the depth pre-pass, light
 *        uniforms per draw, the fog pass, the post chain, the GPU's timers and dynamic resolution.
 * @author Rene Weller
 */
#include "Gfx.h"

#include "Shaders.h"

#include <raymath.h>
#include <rlgl.h>

// raylib's own GL loader (its functions are loaded by raylib at start): what rlgl does not wrap (timer queries, the
// depth test's function, framebuffer blits between two targets).
#include <external/glad.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace spaceopera
{

namespace
{

constexpr int    SunUnit = 12;        ///< texture unit of the sun's shadow map
constexpr int    SpotUnit = 13;       ///< texture unit of the lamp's shadow map
constexpr int    BloomLevels = 6;     ///< levels of the bloom chain
constexpr double Near = 0.5;          ///< the camera's near plane (m)
constexpr double Far = 30000.0;       ///< and its far one
constexpr float  SunExtent = 48.0f;   ///< the sun's map is this wide (m)
constexpr float  SunGrid = 1.5f;      ///< its middle moves in steps of this (m): 32, 64 or 128 of its texels
constexpr float  DynLowest = 0.55f;   ///< dynamic resolution: the scene at this share of the picture at least
/// The texture units of a blended surface's two more materials (Surface::m_layers): free of the material's maps
/// raylib binds (0 1 2 5), of its cube maps (7 8 9), of the shadow maps (12 13), and below 16 (OpenGL's least).
constexpr int LayerUnits[6] = { 3, 4, 6, 10, 14, 15 };
constexpr int EnvUnit = 7;         ///< the level's cube map (no material sets one: raylib's cube map units are free)
constexpr int WetUnit = 8;         ///< the wet map
constexpr int ConeDepthUnit = 9;   ///< the scene's depth while the cones are drawn
constexpr int MaskUnit = 11;       ///< the light masks' atlas
constexpr int EnvSize = 32;        ///< the cube map's faces (texels)
/// The material maps the lit surfaces use (albedo, ORM, normal, emission): bound on the unit of their own index.
constexpr int MapUnits[4] = { MATERIAL_MAP_ALBEDO, MATERIAL_MAP_METALNESS, MATERIAL_MAP_NORMAL, MATERIAL_MAP_EMISSION };

/** @brief Linear filtering and clamping for a target's texture. */
void linearClamp( unsigned int id )
{
    rlTextureParameters( id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR );
    rlTextureParameters( id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR );
    rlTextureParameters( id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP );
    rlTextureParameters( id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP );
}

/** @brief A stain: it multiplies what lies under it. */
bool stain( const Surface &s )
{
    return s.m_params.w > 3.5f && s.m_params.w < 4.5f;
}

/** @brief A decal: blended by its alpha over what lies under it. */
bool decal( const Surface &s )
{
    return s.m_params.w > 2.5f && s.m_params.w < 3.5f;
}

/** @brief Solid: drawn in the depth pre-pass (not a decal or a stain, blended over what is under them, nor a cut-out). */
bool solid( const Surface &s )
{
    float m = s.m_params.w;
    return !( m > 2.5f && m < 4.5f ) && !( m > 5.5f && m < 6.5f );
}

}   // namespace

bool Gfx::SunKey::operator==( const SunKey &o ) const
{
    return m_x == o.m_x && m_y == o.m_y && m_z == o.m_z && m_dir.x == o.m_dir.x && m_dir.y == o.m_dir.y &&
           m_dir.z == o.m_dir.z && m_casters == o.m_casters && m_size == o.m_size && m_on == o.m_on;
}

Shader Gfx::load( const char *vs, const char *fs )
{
    Shader s = LoadShaderFromMemory( vs, fs );
    if ( s.id == rlGetShaderIdDefault() ) TraceLog( LOG_ERROR, "GFX: a shader did not compile (see above)" );
    return s;
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
    l.m_clouds = GetShaderLocation( s, "clouds" );
    l.m_wind = GetShaderLocation( s, "wind" );
    l.m_lightMask = GetShaderLocation( s, "lightMask" );
    l.m_envOn = GetShaderLocation( s, "envOn" );
    l.m_wet = GetShaderLocation( s, "wetInfo" );
    int unit = SunUnit;
    SetShaderValue( s, l.m_sunShadow, &unit, SHADER_UNIFORM_INT );
    unit = SpotUnit;
    SetShaderValue( s, l.m_spotShadow, &unit, SHADER_UNIFORM_INT );
    // Every sampler a shader uses on a unit of its own (two of different kinds on one unit fail the draw on Mesa).
    for ( auto [name, at] :
          { std::pair<const char *, int>{ "lightMasks", MaskUnit }, { "envMap", EnvUnit }, { "wetMap", WetUnit } } )
    {
        int loc = GetShaderLocation( s, name );
        if ( loc >= 0 ) SetShaderValue( s, loc, &at, SHADER_UNIFORM_INT );
    }
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
    l.m_cloth = GetShaderLocation( s, "drawCloth" );
    l.m_cutCentre = GetShaderLocation( s, "cutCentre" );
    l.m_cutRadius = GetShaderLocation( s, "cutRadius" );
    l.m_splat = GetShaderLocation( s, "splat" );
    l.m_sway = GetShaderLocation( s, "sway" );
    // The blended materials' maps on their own units (set once; drawOne binds the textures).
    const char *layers[6] = {
        "layer1Albedo", "layer1Orm", "layer1Normal", "layer2Albedo", "layer2Orm", "layer2Normal"
    };
    for ( int i = 0; i < 6; ++i )
    {
        int loc = GetShaderLocation( s, layers[i] );
        if ( loc >= 0 ) SetShaderValue( s, loc, &LayerUnits[i], SHADER_UNIFORM_INT );
    }
    return l;
}

Gfx::DepthLocs Gfx::depthLocs( Shader s )
{
    DepthLocs l;
    l.m_viewProj = GetShaderLocation( s, "viewProj" );
    l.m_mvp = GetShaderLocation( s, "mvp" );
    l.m_model = GetShaderLocation( s, "matModel" );
    l.m_bones = GetShaderLocation( s, "boneMatrices" );
    l.m_cutaway = GetShaderLocation( s, "cutaway" );
    l.m_cutCentre = GetShaderLocation( s, "cutCentre" );
    l.m_cutRadius = GetShaderLocation( s, "cutRadius" );
    l.m_viewPos = GetShaderLocation( s, "viewPos" );
    return l;
}

void Gfx::upload( Shader s, const CommonLocs &l, bool points )
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
    int     count = points ? (int)std::min( L.m_points.size(), (size_t)MaxLights ) : 0;
    Vector4 pos[MaxLights];
    Vector4 col[MaxLights];
    Vector4 mask[MaxLights];
    for ( int i = 0; i < count; ++i )
    {
        const PointLight &p = L.m_points[i];
        pos[i] = { p.m_pos.x, p.m_pos.y, p.m_pos.z, p.m_radius };
        col[i] = { p.m_color.x, p.m_color.y, p.m_color.z, p.m_halo };
        mask[i] = maskOf( p );
    }
    SetShaderValue( s, l.m_lightCount, &count, SHADER_UNIFORM_INT );
    if ( count > 0 )
    {
        SetShaderValueV( s, l.m_lightPos, pos, SHADER_UNIFORM_VEC4, count );
        SetShaderValueV( s, l.m_lightColor, col, SHADER_UNIFORM_VEC4, count );
        if ( l.m_lightMask >= 0 ) SetShaderValueV( s, l.m_lightMask, mask, SHADER_UNIFORM_VEC4, count );
    }
    bool    env = !( m_off & OFF_ENV );
    float   envOn = m_envTex && env ? 1.0f : 0.0f;
    Vector4 wet = env ? m_wetInfo : Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
    if ( l.m_envOn >= 0 ) SetShaderValue( s, l.m_envOn, &envOn, SHADER_UNIFORM_FLOAT );
    if ( l.m_wet >= 0 ) SetShaderValue( s, l.m_wet, &wet, SHADER_UNIFORM_VEC4 );
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
    SetShaderValue( s, l.m_clouds, &L.m_clouds, SHADER_UNIFORM_VEC4 );
    SetShaderValue( s, l.m_wind, &L.m_wind, SHADER_UNIFORM_VEC4 );
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
        // A texture, not a renderbuffer: the fog pass reads it. 24 bits asked for by name (raylib leaves the size to
        // the driver; the shadow maps' 24 bits came that way on the RTX).
        GLuint id = 0;
        glGenTextures( 1, &id );
        glBindTexture( GL_TEXTURE_2D, id );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glBindTexture( GL_TEXTURE_2D, 0 );
        t.depth.id = id;
        t.depth.width = w;
        t.depth.height = h;
        t.depth.mipmaps = 1;
        rlFramebufferAttach( t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0 );
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
    std::string head = "#version 330\n";
    std::string few = head + "#define MAX_LIGHTS " + std::to_string( MaxDrawLights ) + "\n";
    std::string common = CommonFs;
    m_lit = load( LitVs, ( few + common + CutAwayFs + LitFs ).c_str() );
    m_litSkin = load( LitSkinVs, ( few + common + CutAwayFs + LitFs ).c_str() );
    m_glassShader = load( LitVs, ( head + common + GlassFs ).c_str() );
    m_waterShader = load( LitVs, ( head + common + WaterFs ).c_str() );
    m_pondShader = load( LitVs, ( head + common + PondFs ).c_str() );
    m_sky = load( FullscreenVs, ( head + common + SkyFs ).c_str() );
    m_planet = load( LitVs, ( head + common + PlanetFs ).c_str() );
    m_particle = load( ParticleVs, ( head + common + ParticleFs ).c_str() );
    m_fogShader = load( FullscreenVs, ( head + common + FogFs ).c_str() );
    m_fogUp = load( FullscreenVs, FogUpFs );
    m_depth = load( DepthVs, DepthFs );
    m_depthSkin = load( DepthSkinVs, DepthFs );
    m_prepassShader = load( LitVs, ( head + CutAwayFs + PrepassFs ).c_str() );
    m_prepassSkin = load( LitSkinVs, ( head + CutAwayFs + PrepassFs ).c_str() );
    m_coneShader = load( LitVs, ConeFs );
    m_prefilter = load( FullscreenVs, PrefilterFs );
    m_down = load( FullscreenVs, DownFs );
    m_up = load( FullscreenVs, UpFs );
    m_final = load( FullscreenVs, FinalFs );

    m_lit.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation( m_lit, "emissionMap" );
    m_litSkin.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation( m_litSkin, "emissionMap" );
    m_litLocs = locate( m_lit );
    m_litSkinLocs = locate( m_litSkin );
    m_glassLocs = locate( m_glassShader );
    m_waterLocs = locate( m_waterShader );
    m_pondLocs = locate( m_pondShader );
    m_rippleLoc = GetShaderLocation( m_pondShader, "ripples" );
    m_rippleCountLoc = GetShaderLocation( m_pondShader, "rippleCount" );
    m_skyLocs = locate( m_sky );
    m_planetLocs = locate( m_planet );
    m_particleLocs = locate( m_particle );
    m_fogLocs = locate( m_fogShader );
    m_litDraw = drawLocs( m_lit );
    m_skinDraw = drawLocs( m_litSkin );
    m_depthLocs = depthLocs( m_depth );
    m_depthSkinLocs = depthLocs( m_depthSkin );
    m_prepassLocs = depthLocs( m_prepassShader );
    m_prepassSkinLocs = depthLocs( m_prepassSkin );

    m_glassMat = LoadMaterialDefault();
    m_glassMat.shader = m_glassShader;
    m_waterMat = LoadMaterialDefault();
    m_waterMat.shader = m_waterShader;
    m_pondMat = LoadMaterialDefault();
    m_pondMat.shader = m_pondShader;
    m_planetMat = LoadMaterialDefault();
    m_planetMat.shader = m_planet;
    m_coneMat = LoadMaterialDefault();
    m_coneMat.shader = m_coneShader;
    int coneUnit = ConeDepthUnit;
    SetShaderValue( m_coneShader, GetShaderLocation( m_coneShader, "depthTex" ), &coneUnit, SHADER_UNIFORM_INT );
    // The lean path's maps on their units once (DrawMesh sets them anew for every draw).
    m_boneLoc = GetShaderLocation( m_litSkin, "boneMatrices" );
    for ( Shader s : { m_lit, m_litSkin } )
        for ( int unit : MapUnits )
            if ( s.locs[SHADER_LOC_MAP_DIFFUSE + unit] >= 0 )
                SetShaderValue( s, s.locs[SHADER_LOC_MAP_DIFFUSE + unit], &unit, SHADER_UNIFORM_INT );
    // The cube map's faces filtered across their edges (its blurred levels of detail would show seams).
    glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
    setQuality( quality, ss );
}

void Gfx::unloadTargets()
{
    if ( m_hdr.id == 0 ) return;
    rlUnloadFramebuffer( m_hdrColor.id );   // its colour is m_hdr's
    UnloadRenderTexture( m_hdr );
    UnloadRenderTexture( m_fogTarget );
    for ( RenderTexture2D &t : m_bloomLevels ) UnloadRenderTexture( t );
    UnloadRenderTexture( m_sunMap );
    UnloadRenderTexture( m_sunKept );
    UnloadRenderTexture( m_spotMap );
    m_bloomLevels.clear();
    m_hdr = m_hdrColor = m_fogTarget = m_sunMap = m_sunKept = m_spotMap = {};
    m_sunKeyValid = false;
}

void Gfx::setQuality( int quality, float ss )
{
    struct Level
    {
        float m_ss;
        int   m_fogSteps, m_shadowTaps, m_sunSize, m_spotSize;
    };
    // Low, middle, high, and frugal for a Steam Deck on its battery (below low in the menu).
    static const Level Levels[4] = { { 0.75f, 8, 4, 2048, 1024 },
                                     { 1.0f, 20, 8, 4096, 2048 },
                                     { 1.5f, 40, 12, 4096, 2048 },
                                     { 0.67f, 5, 2, 1024, 512 } };
    m_quality = std::clamp( quality, 0, 3 );
    m_scaleAsked = ss;
    const Level &q = Levels[m_quality];
    m_scale = ss > 0.0f ? ss : q.m_ss;
    m_fogSteps = q.m_fogSteps;
    m_shadowTaps = q.m_shadowTaps;
    m_dynScale = 1.0f;
    m_dynWait = 0;
    unloadTargets();
    int w = std::max( (int)( m_width * m_scale ), 1 );
    int h = std::max( (int)( m_height * m_scale ), 1 );
    m_hdr = hdrTarget( w, h, true );
    // The same colour without the depth: the fog is laid over it while the depth is read.
    m_hdrColor.id = rlLoadFramebuffer();
    rlEnableFramebuffer( m_hdrColor.id );
    rlFramebufferAttach( m_hdrColor.id, m_hdr.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0 );
    if ( !rlFramebufferComplete( m_hdrColor.id ) ) TraceLog( LOG_ERROR, "GFX: colour target %dx%d incomplete", w, h );
    rlDisableFramebuffer();
    m_hdrColor.texture = m_hdr.texture;
    m_fogTarget = hdrTarget( ( w + m_fogStep - 1 ) / m_fogStep, ( h + m_fogStep - 1 ) / m_fogStep, false );
    for ( int i = 0; i < BloomLevels; ++i )
    {
        w = std::max( w / 2, 1 );
        h = std::max( h / 2, 1 );
        m_bloomLevels.push_back( hdrTarget( w, h, false ) );
    }
    m_sunSize = q.m_sunSize;
    m_spotSize = q.m_spotSize;
    m_sunMap = depthTarget( m_sunSize );
    m_sunKept = depthTarget( m_sunSize );
    m_spotMap = depthTarget( m_spotSize );
}

void Gfx::resize( int width, int height )
{
    if ( width == m_width && height == m_height ) return;
    m_width = std::max( width, 1 );
    m_height = std::max( height, 1 );
    setQuality( m_quality, m_scaleAsked );
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
    // The level's light masks, cube map and wet map with them.
    glActiveTexture( GL_TEXTURE0 + MaskUnit );
    glBindTexture( GL_TEXTURE_2D, on && !( m_off & OFF_MASKS ) ? m_maskTex : 0 );
    glActiveTexture( GL_TEXTURE0 + WetUnit );
    glBindTexture( GL_TEXTURE_2D, on && !( m_off & OFF_ENV ) ? m_wetTex : 0 );
    glActiveTexture( GL_TEXTURE0 + EnvUnit );
    glBindTexture( GL_TEXTURE_CUBE_MAP, on && !( m_off & OFF_ENV ) ? m_envTex : 0 );
    rlActiveTextureSlot( 0 );
}

Vector4 Gfx::maskOf( const PointLight &p ) const
{
    if ( p.m_mask < 0 || p.m_mask >= (int)m_maskPlace.size() || m_maskTex == 0 || ( m_off & OFF_MASKS ) )
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    return m_maskPlace[(size_t)p.m_mask];
}

void Gfx::lightMasks( const LightMasks &masks )
{
    if ( m_maskTex ) glDeleteTextures( 1, &m_maskTex );
    m_maskTex = 0;
    m_maskPlace.clear();
    m_maskReach.clear();
    if ( masks.m_atlas.empty() || masks.m_width <= 0 || masks.m_height <= 0 ) return;
    // One byte a texel, filtered (the shadows' edges soft between the texels), clamped.
    GLuint id = 0;
    glGenTextures( 1, &id );
    glBindTexture( GL_TEXTURE_2D, id );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_R8, masks.m_width, masks.m_height, 0, GL_RED, GL_UNSIGNED_BYTE,
                  masks.m_atlas.data() );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glBindTexture( GL_TEXTURE_2D, 0 );
    m_maskTex = id;
    m_maskPlace = masks.m_place;
    m_maskReach = masks.m_reach;
}

void Gfx::wetMap( const std::vector<unsigned char> &texels, int cols, int rows, float cell )
{
    if ( m_wetTex ) glDeleteTextures( 1, &m_wetTex );
    m_wetTex = 0;
    m_wetInfo = { 0.0f, 0.0f, 0.0f, 0.0f };
    if ( cols <= 0 || rows <= 0 || texels.size() < (size_t)cols * rows * 4 ) return;
    GLuint id = 0;
    glGenTextures( 1, &id );
    glBindTexture( GL_TEXTURE_2D, id );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data() );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glBindTexture( GL_TEXTURE_2D, 0 );
    m_wetTex = id;
    m_wetInfo = { 1.0f / ( cols * cell ), 1.0f / ( rows * cell ), 1.0f, 0.0f };
}

void Gfx::environment( Vector3 ambUp, Vector3 ambDown, Vector3 fog, const std::vector<Vector3> &lamps, unsigned seed )
{
    if ( m_envTex ) glDeleteTextures( 1, &m_envTex );
    m_envTex = 0;
    // The lamps that light the level most, as soft spots round the horizon (a room's lamps hang on its walls, a little
    // above what reflects them): their colours at the brightness of a lamp's glass, a wider glow round each.
    std::vector<Vector3> bright = lamps;
    auto                 luma = []( Vector3 c ) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; };
    std::sort( bright.begin(), bright.end(), [&]( Vector3 a, Vector3 b ) { return luma( a ) > luma( b ); } );
    if ( bright.size() > 10 ) bright.resize( 10 );
    struct Spot
    {
        Vector3 m_dir, m_colour;
        float   m_size;
    };
    std::vector<Spot> spots;
    unsigned          h = seed * 2654435761u + 12345u;
    auto              rnd = [&]()
    {
        h = h * 1664525u + 1013904223u;
        return ( h >> 8 ) / 16777216.0f;
    };
    Vector3 mean = { 0.0f, 0.0f, 0.0f };
    for ( size_t i = 0; i < bright.size(); ++i )
    {
        Vector3 c = bright[i];
        float   peak = std::max( { c.x, c.y, c.z, 1e-4f } );
        c = Vector3Scale( c, 2.5f / peak );
        float yaw = ( i + 0.3f + 0.4f * rnd() ) * 2.0f * PI / bright.size(), rise = 0.08f + 0.35f * rnd();
        float flat = sqrtf( 1.0f - rise * rise );
        spots.push_back( { { cosf( yaw ) * flat, rise, sinf( yaw ) * flat }, c, 0.05f + 0.04f * rnd() } );
        mean = Vector3Add( mean, Vector3Scale( c, 1.0f / bright.size() ) );
    }

    auto radiance = [&]( Vector3 d )
    {
        // The old fake environment underneath (a hemisphere), the ceiling darker, the haze's glow above.
        float   t = d.y * 0.5f + 0.5f;
        Vector3 c = Vector3Scale( Vector3Lerp( ambDown, ambUp, t ), 3.0f );
        float   high = std::clamp( ( d.y - 0.45f ) / 0.55f, 0.0f, 1.0f );
        c = Vector3Add( Vector3Scale( c, 1.0f - 0.75f * high ), Vector3Scale( fog, 8.0f * high ) );
        // The floor's pools of light below the horizon.
        float low = std::clamp( -d.y * 3.0f, 0.0f, 1.0f ) * std::clamp( ( d.y + 0.9f ) * 2.0f, 0.0f, 1.0f );
        c = Vector3Add( c, Vector3Scale( mean, 0.012f * low ) );
        for ( const Spot &s : spots )
        {
            float a = acosf( std::clamp( Vector3DotProduct( d, s.m_dir ), -1.0f, 1.0f ) );
            float core = expf( -( a * a ) / ( s.m_size * s.m_size ) );
            float glow = expf( -( a * a ) / 0.16f ) * 0.008f;
            c = Vector3Add( c, Vector3Scale( s.m_colour, core + glow ) );
        }
        return c;
    };
    std::vector<float> face( (size_t)EnvSize * EnvSize * 4 );
    GLuint             id = 0;
    glGenTextures( 1, &id );
    glBindTexture( GL_TEXTURE_CUBE_MAP, id );
    for ( int f = 0; f < 6; ++f )
    {
        for ( int j = 0; j < EnvSize; ++j )
            for ( int i = 0; i < EnvSize; ++i )
            {
                // OpenGL's faces: +x, -x, +y, -y, +z, -z (s right, t down on each).
                float   sc = ( i + 0.5f ) * 2.0f / EnvSize - 1.0f, tc = ( j + 0.5f ) * 2.0f / EnvSize - 1.0f;
                Vector3 d = f == 0   ? Vector3{ 1.0f, -tc, -sc }
                            : f == 1 ? Vector3{ -1.0f, -tc, sc }
                            : f == 2 ? Vector3{ sc, 1.0f, tc }
                            : f == 3 ? Vector3{ sc, -1.0f, -tc }
                            : f == 4 ? Vector3{ sc, -tc, 1.0f }
                                     : Vector3{ -sc, -tc, -1.0f };
                Vector3 c = radiance( Vector3Normalize( d ) );
                float  *px = &face[( (size_t)j * EnvSize + i ) * 4];
                px[0] = c.x, px[1] = c.y, px[2] = c.z, px[3] = 1.0f;
            }
        glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGBA16F, EnvSize, EnvSize, 0, GL_RGBA, GL_FLOAT,
                      face.data() );
    }
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
    glGenerateMipmap( GL_TEXTURE_CUBE_MAP );
    glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
    m_envTex = id;
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

bool Gfx::inside( const Frustum &f, const Drawable &d, Bounds &b )
{
    const Mesh &m = d.m_mesh;
    if ( d.m_sphereCount != m.vertexCount || d.m_sphereOf != m.vertices )
    {
        std::uint64_t key = (std::uint64_t)(std::uintptr_t)m.vertices * 31u + m.vaoId;
        Sphere       &s = m_spheres[key];
        Vector3       first = m.vertices && m.vertexCount > 0 ? Vector3{ m.vertices[0], m.vertices[1], m.vertices[2] }
                                                              : Vector3{ 0.0f, 0.0f, 0.0f };
        if ( s.m_count != m.vertexCount || s.m_sphere.w == 0.0f ||
             std::memcmp( &s.m_first, &first, sizeof( Vector3 ) ) != 0 )
        {
            // Measured once per mesh: the middle and half the diagonal of its box.
            s = { { 0.0f, 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 0.0f }, m.vertexCount, first };
            if ( m.vertices && m.vertexCount > 0 )
            {
                BoundingBox bb = GetMeshBoundingBox( m );
                Vector3     mid = Vector3Scale( Vector3Add( bb.min, bb.max ), 0.5f );
                s.m_sphere = { mid.x, mid.y, mid.z, std::max( Vector3Distance( bb.min, bb.max ) * 0.5f, 0.01f ) };
                s.m_half = Vector3Scale( Vector3Subtract( bb.max, bb.min ), 0.5f );
            }
        }
        d.m_sphere = s.m_sphere;
        d.m_half = s.m_half;
        d.m_sphereOf = m.vertices;
        d.m_sphereCount = m.vertexCount;
    }
    b.m_centre = { d.m_sphere.x, d.m_sphere.y, d.m_sphere.z };
    b.m_radius = d.m_sphere.w;
    b.m_half = d.m_half;
    b.m_box = false;
    if ( d.m_sphere.w < 0.0f ) return true;
    const Matrix &t = d.m_transform;
    // The level's tiles stand where they were built (thousands of them, three times a frame): no transform there.
    bool still = t.m0 == 1.0f && t.m5 == 1.0f && t.m10 == 1.0f && t.m12 == 0.0f && t.m13 == 0.0f && t.m14 == 0.0f &&
                 t.m1 == 0.0f && t.m2 == 0.0f && t.m4 == 0.0f && t.m6 == 0.0f && t.m8 == 0.0f && t.m9 == 0.0f;
    if ( still )
        b.m_box = true;
    else
    {
        b.m_centre = Vector3Transform( b.m_centre, t );
        float scale =
            sqrtf( std::max( { t.m0 * t.m0 + t.m1 * t.m1 + t.m2 * t.m2, t.m4 * t.m4 + t.m5 * t.m5 + t.m6 * t.m6,
                               t.m8 * t.m8 + t.m9 * t.m9 + t.m10 * t.m10 } ) );
        // A skinned figure's limbs reach past its rest pose.
        b.m_radius *= scale * ( d.m_bones ? 1.6f : 1.0f );
    }
    const Vector3 &c = b.m_centre;
    for ( const Vector4 &p : f.m_planes )
        if ( p.x * c.x + p.y * c.y + p.z * c.z + p.w < -b.m_radius ) return false;
    return true;
}

void Gfx::drawVao( const Mesh &mesh )
{
    if ( mesh.vaoId == 0 ) return;
    rlEnableVertexArray( mesh.vaoId );
    if ( mesh.indices )
        rlDrawVertexArrayElements( 0, mesh.triangleCount * 3, 0 );
    else
        rlDrawVertexArray( 0, mesh.vertexCount );
    ++m_stats.m_depthDraws;
    m_stats.m_depthTris += mesh.indices ? mesh.triangleCount : mesh.vertexCount / 3;
}

void Gfx::lap( int part )
{
    double now = GetTime();
    m_stats.m_partMs[part] += ( now - m_lapAt ) * 1000.0;
    m_lapAt = now;
    if ( m_stamps && m_timing )
    {
        glQueryCounter( m_stampIds[m_timerNext][part + 1], GL_TIMESTAMP );
        m_stampSet[m_timerNext] |= (unsigned char)( 1 << ( part + 1 ) );
    }
}

void Gfx::frameBegin()
{
    if ( m_timing ) frameEnd();   // a frame that never reached post()
    if ( m_timers[0] == 0 )
    {
        glGenQueries( TimerRing, m_timers );
        glGenQueries( TimerRing * 6, &m_stampIds[0][0] );
    }
    collectTimers( false );
    int slot = m_timerNext;
    if ( m_timerOut[slot] ) collectTimers( true );   // all four still out: wait for the oldest
    rlDrawRenderBatchActive();
    glBeginQuery( GL_TIME_ELAPSED, m_timers[slot] );
    m_stampSet[slot] = 0;
    if ( m_stamps )
    {
        glQueryCounter( m_stampIds[slot][0], GL_TIMESTAMP );
        m_stampSet[slot] = 1;
    }
    m_timing = true;
    m_cpuStart = m_lapAt = GetTime();
}

void Gfx::frameEnd()
{
    if ( !m_timing ) return;
    rlDrawRenderBatchActive();
    glEndQuery( GL_TIME_ELAPSED );
    lap( 4 );
    m_timerOut[m_timerNext] = true;
    m_timerNext = ( m_timerNext + 1 ) % TimerRing;
    m_timing = false;
    float cpu = (float)( ( GetTime() - m_cpuStart ) * 1000.0 );
    m_cpuMs = m_cpuMs > 0.0f ? m_cpuMs + ( cpu - m_cpuMs ) * 0.15f : cpu;
    m_stats.m_cpuMs += cpu;
    ++m_stats.m_frames;
}

void Gfx::collectTimers( bool wait )
{
    // Oldest first, so a frame's time is read before a later one's.
    for ( int k = 0; k < TimerRing; ++k )
    {
        int slot = ( m_timerNext + k ) % TimerRing;
        if ( !m_timerOut[slot] ) continue;
        GLint ready = 0;
        if ( !wait ) glGetQueryObjectiv( m_timers[slot], GL_QUERY_RESULT_AVAILABLE, &ready );
        if ( !wait && !ready ) break;
        GLuint64 ns = 0;
        glGetQueryObjectui64v( m_timers[slot], GL_QUERY_RESULT, &ns );
        m_timerOut[slot] = false;
        float ms = (float)( ns / 1.0e6 );
        m_gpuMs = m_gpuMs > 0.0f ? m_gpuMs + ( ms - m_gpuMs ) * 0.15f : ms;
        m_stats.m_gpuMs += ms;
        ++m_stats.m_gpuFrames;
        // The parts' timestamps (the bench): each part's end after the last one written.
        if ( m_stampSet[slot] == 0x3f )
        {
            GLuint64 t[6];
            for ( int j = 0; j < 6; ++j ) glGetQueryObjectui64v( m_stampIds[slot][j], GL_QUERY_RESULT, &t[j] );
            for ( int j = 0; j < 5; ++j ) m_stats.m_gpuPartMs[j] += ( t[j + 1] - t[j] ) / 1.0e6;
            ++m_stats.m_stampFrames;
        }
        m_stampSet[slot] = 0;
    }
}

void Gfx::newPlace()
{
    m_spheres.clear();
    m_sunKeyValid = false;
    m_lightSet[0].m_count = m_lightSet[1].m_count = 0;
}

void Gfx::resetStats()
{
    collectTimers( true );
    m_stats = {};
}

Gfx::Stats Gfx::stats()
{
    collectTimers( true );
    return m_stats;
}

void Gfx::pickScale()
{
    if ( !m_dynamicRes || ( m_quality != 0 && m_quality != 3 ) )
    {
        m_dynScale = 1.0f;
        return;
    }
    // The timers lag by a few frames, and a change needs some to show: wait between steps.
    if ( ++m_dynWait < 10 || m_gpuMs <= 0.0f ) return;
    float lowest = std::min( DynLowest / m_scale, 1.0f );
    float s = m_dynScale;
    // The GPU's timer runs while it waits for the processor too: only a GPU well behind it is too slow by itself.
    bool gpuBound = m_gpuMs > m_cpuMs * 1.1f;
    if ( gpuBound && m_gpuMs > m_frameBudget * 0.92f )
        s *= std::clamp( sqrtf( m_frameBudget * 0.85f / m_gpuMs ), 0.8f, 0.97f );   // the pixels' cost goes as s²
    else if ( m_gpuMs < m_frameBudget * 0.75f )
        s *= 1.04f;
    s = std::clamp( roundf( s * 50.0f ) / 50.0f, lowest, 1.0f );
    if ( s != m_dynScale )
    {
        m_dynScale = s;
        m_dynWait = 0;
    }
}

void Gfx::casters( const Frustum &cull, const Matrix &vp, bool sun,
                   const std::vector<const std::vector<Drawable> *> &lists, size_t from, size_t to )
{
    rlDrawRenderBatchActive();
    SetShaderValueMatrix( m_depth, m_depthLocs.m_viewProj, vp );
    SetShaderValueMatrix( m_depthSkin, m_depthSkinLocs.m_viewProj, vp );
    // Straight to GL (DrawMesh binds every map of a material and sets five matrices for each of thousands of tiles):
    // the model matrix, a figure's bones, its vertex array.
    unsigned int bound = 0;
    const int    level = detail();
    for ( size_t i = from; i < to; ++i )
        for ( const Drawable &d : *lists[i] )
        {
            Bounds b;
            if ( !( sun ? d.m_sunShadow : d.m_spotShadow ) || d.m_detail > level || !inside( cull, d, b ) ) continue;
            const DepthLocs &l = d.m_bones ? m_depthSkinLocs : m_depthLocs;
            unsigned int     id = d.m_bones ? m_depthSkin.id : m_depth.id;
            if ( id != bound ) rlEnableShader( bound = id );
            rlSetUniformMatrix( l.m_model, d.m_transform );
            if ( d.m_bones ) rlSetUniformMatrices( l.m_bones, d.m_bones, d.m_boneCount );
            drawVao( d.m_mesh );
        }
    rlDisableVertexArray();
    rlDisableShader();
}

void Gfx::copyDepth( const RenderTexture2D &from, const RenderTexture2D &to )
{
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    glBindFramebuffer( GL_READ_FRAMEBUFFER, from.id );
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, to.id );
    glBlitFramebuffer( 0, 0, from.depth.width, from.depth.height, 0, 0, to.depth.width, to.depth.height,
                       GL_DEPTH_BUFFER_BIT, GL_NEAREST );
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void Gfx::shadows( const Lighting &lights, Vector3 centre, const std::vector<const std::vector<Drawable> *> &lists,
                   size_t still )
{
    frameBegin();
    pickScale();
    bindShadows( false );
    m_lights = lights;
    m_fogDone = false;
    m_prepassed.clear();
    m_coneList = nullptr;
    still = std::min( still, lists.size() );

    // The sun's map, its middle on a grid in the light's own axes (as the look-at builds them): its texels stay put
    // while the hero walks, and what does not move need not be drawn again until the middle steps on.
    Vector3 up = { 0.0f, 1.0f, 0.0f };
    Vector3 z = Vector3Negate( Vector3Normalize( lights.m_sunDir ) );
    if ( fabsf( z.y ) > 0.999f ) up = { 0.0f, 0.0f, 1.0f };
    Vector3 x = Vector3Normalize( Vector3CrossProduct( up, z ) );
    Vector3 y = Vector3CrossProduct( z, x );
    SunKey  key;
    key.m_x = (int)lroundf( Vector3DotProduct( centre, x ) / SunGrid );
    key.m_y = (int)lroundf( Vector3DotProduct( centre, y ) / SunGrid );
    key.m_z = (int)lroundf( Vector3DotProduct( centre, z ) / SunGrid );
    key.m_dir = lights.m_sunDir;
    key.m_size = m_sunSize;
    key.m_on = m_sunShadows;
    // What does not move, as a hash of its lists, meshes and places (cheap beside drawing it).
    std::uint64_t hash = 1469598103934665603ull;
    auto          mix = [&]( std::uint64_t v ) { hash = ( hash ^ v ) * 1099511628211ull; };
    for ( size_t i = 0; i < still; ++i )
    {
        mix( (std::uint64_t)(std::uintptr_t)lists[i]->data() );
        mix( lists[i]->size() );
        for ( const Drawable &d : *lists[i] )
        {
            if ( !d.m_sunShadow ) continue;
            std::uint32_t p[4];
            std::memcpy( &p[0], &d.m_transform.m12, 4 );
            std::memcpy( &p[1], &d.m_transform.m13, 4 );
            std::memcpy( &p[2], &d.m_transform.m14, 4 );
            std::memcpy( &p[3], &d.m_transform.m0, 4 );
            mix( ( (std::uint64_t)d.m_mesh.vaoId << 32 ) | (std::uint32_t)d.m_mesh.vertexCount );
            mix( ( (std::uint64_t)p[0] << 32 ) | p[1] );
            mix( ( (std::uint64_t)p[2] << 32 ) | p[3] );
        }
    }
    key.m_casters = hash;
    Vector3 mid = Vector3Add( Vector3Add( Vector3Scale( x, key.m_x * SunGrid ), Vector3Scale( y, key.m_y * SunGrid ) ),
                              Vector3Scale( z, key.m_z * SunGrid ) );

    Camera3D sun = {};
    sun.target = mid;
    sun.position = Vector3Subtract( mid, Vector3Scale( lights.m_sunDir, 70.0f ) );
    sun.up = up;
    sun.fovy = SunExtent;
    sun.projection = CAMERA_ORTHOGRAPHIC;
    // A skinned figure casts the shadow of its pose, not of its rest pose. What lies outside the map is left out.
    auto sunPass = [&]( const RenderTexture2D &target, bool clear, size_t from, size_t to )
    {
        BeginTextureMode( target );
        if ( clear ) rlClearScreenBuffers();
        rlSetClipPlanes( 1.0, 160.0 );
        BeginMode3D( sun );
        m_sunVP = MatrixMultiply( rlGetMatrixModelview(), rlGetMatrixProjection() );
        if ( m_sunShadows && from < to ) casters( frustum( m_sunVP ), m_sunVP, true, lists, from, to );
        EndMode3D();
        EndTextureMode();
    };
    if ( !m_cacheShadows || still == 0 || !m_sunShadows )
    {
        sunPass( m_sunMap, true, 0, lists.size() );
        m_sunKeyValid = false;
    }
    else
    {
        if ( !m_sunKeyValid || !( key == m_sunKey ) )
        {
            sunPass( m_sunKept, true, 0, still );
            m_sunKey = key;
            m_sunKeyValid = true;
            ++m_stats.m_sunRenders;
        }
        copyDepth( m_sunKept, m_sunMap );
        sunPass( m_sunMap, false, still, lists.size() );
    }

    // The lamp's map moves with the hero's shoulder (it breathes even standing): drawn anew each frame.
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
    casters( frustum( m_spotVP ), m_spotVP, false, lists, 0, lists.size() );
    EndMode3D();
    EndTextureMode();
    lap( 0 );
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

void Gfx::begin3D( const Camera3D &cam, int w, int h )
{
    // As BeginMode3D, with the viewport's aspect instead of the target's.
    rlDrawRenderBatchActive();
    rlMatrixMode( RL_PROJECTION );
    rlPushMatrix();
    rlLoadIdentity();
    float aspect = (float)w / (float)h;
    if ( cam.projection == CAMERA_PERSPECTIVE )
    {
        double top = rlGetCullDistanceNear() * tan( cam.fovy * 0.5 * DEG2RAD );
        double right = top * aspect;
        rlFrustum( -right, right, -top, top, rlGetCullDistanceNear(), rlGetCullDistanceFar() );
    }
    else
    {
        double top = cam.fovy / 2.0;
        double right = top * aspect;
        rlOrtho( -right, right, -top, top, rlGetCullDistanceNear(), rlGetCullDistanceFar() );
    }
    rlMatrixMode( RL_MODELVIEW );
    rlLoadIdentity();
    Matrix view = MatrixLookAt( cam.position, cam.target, cam.up );
    rlMultMatrixf( MatrixToFloat( view ) );
    rlEnableDepthTest();
}

void Gfx::enterScene()
{
    BeginTextureMode( m_hdr );
    rlViewport( 0, 0, m_viewW, m_viewH );
    rlSetClipPlanes( Near, Far );
    begin3D( m_cam, m_viewW, m_viewH );
}

void Gfx::beginScene( const Camera3D &cam, const Lighting &lights, float time, Texture2D sky )
{
    m_cam = cam;
    m_lights = lights;
    m_time = time;
    m_frame += 1.0f;
    m_fogDone = false;
    m_prepassed.clear();
    int tw = m_hdr.texture.width;
    int th = m_hdr.texture.height;
    m_viewW = m_dynScale >= 1.0f ? tw : std::clamp( (int)lroundf( tw * m_dynScale ), 1, tw );
    m_viewH = m_dynScale >= 1.0f ? th : std::clamp( (int)lroundf( th * m_dynScale ), 1, th );
    BeginTextureMode( m_hdr );
    rlViewport( 0, 0, m_viewW, m_viewH );
    ClearBackground( BLACK );
    // The lit surfaces get their point lights draw by draw (lightsFor); the rest all of them.
    upload( m_lit, m_litLocs, false );
    upload( m_litSkin, m_litSkinLocs, false );
    m_lightSet[0].m_count = m_lightSet[1].m_count = 0;
    upload( m_glassShader, m_glassLocs );
    upload( m_waterShader, m_waterLocs );
    upload( m_pondShader, m_pondLocs );
    upload( m_sky, m_skyLocs );
    upload( m_planet, m_planetLocs );
    upload( m_particle, m_particleLocs );
    upload( m_fogShader, m_fogLocs );
    bindShadows( true );
    for ( auto [shader, locs] : { std::pair<Shader, DrawLocs>{ m_lit, m_litDraw }, { m_litSkin, m_skinDraw } } )
    {
        SetShaderValue( shader, locs.m_cutCentre, &m_cutCentre, SHADER_UNIFORM_VEC3 );
        SetShaderValue( shader, locs.m_cutRadius, &m_cutRadius, SHADER_UNIFORM_FLOAT );
        // The fill light only for the figures (the skinned ones).
        float fill = shader.id == m_litSkin.id ? m_fill : 0.0f;
        SetShaderValue( shader, GetShaderLocation( shader, "fill" ), &fill, SHADER_UNIFORM_FLOAT );
    }
    for ( auto [shader, locs] :
          { std::pair<Shader, DepthLocs>{ m_prepassShader, m_prepassLocs }, { m_prepassSkin, m_prepassSkinLocs } } )
    {
        SetShaderValue( shader, locs.m_cutCentre, &m_cutCentre, SHADER_UNIFORM_VEC3 );
        SetShaderValue( shader, locs.m_cutRadius, &m_cutRadius, SHADER_UNIFORM_FLOAT );
        SetShaderValue( shader, locs.m_viewPos, &m_cam.position, SHADER_UNIFORM_VEC3 );
    }

    Matrix view = MatrixLookAt( cam.position, cam.target, cam.up );
    Matrix proj = MatrixPerspective( cam.fovy * DEG2RAD, (double)m_viewW / m_viewH, Near, Far );
    Matrix inv = MatrixInvert( MatrixMultiply( view, proj ) );
    SetShaderValueMatrix( m_sky, GetShaderLocation( m_sky, "invViewProj" ), inv );
    float nebula = 0.8f;
    SetShaderValue( m_sky, GetShaderLocation( m_sky, "nebula" ), &nebula, SHADER_UNIFORM_FLOAT );
    rlDisableDepthMask();
    rlDisableDepthTest();
    if ( !( m_off & OFF_VIEW ) ) fullscreen( m_sky, sky, m_viewW, m_viewH );
    rlEnableDepthTest();
    rlEnableDepthMask();

    rlSetClipPlanes( Near, Far );
    begin3D( cam, m_viewW, m_viewH );
    m_viewProj = MatrixMultiply( rlGetMatrixModelview(), rlGetMatrixProjection() );
    m_view = frustum( m_viewProj );
}

void Gfx::prepass( const std::vector<const std::vector<Drawable> *> &lists, const std::vector<Surface> &surfaces )
{
    rlDrawRenderBatchActive();
    rlColorMask( false, false, false, false );
    // The model matrix and the mvp as DrawMesh makes them (the transform after rlgl's own, then the view, then the
    // projection), so the lit pass meets this depth exactly.
    Matrix       stack = rlGetMatrixTransform();
    Matrix       view = rlGetMatrixModelview();
    Matrix       proj = rlGetMatrixProjection();
    unsigned int bound = 0;
    float        cutNow[2] = { -1.0f, -1.0f };
    const int    level = detail();
    for ( const auto *list : lists )
    {
        m_prepassed.push_back( list );
        for ( const Drawable &d : *list )
        {
            Bounds b;
            if ( !d.m_visible || d.m_detail > level || !solid( surfaces[d.m_surface] ) || !inside( m_view, d, b ) )
                continue;
            int              k = d.m_bones ? 1 : 0;
            const DepthLocs &l = k ? m_prepassSkinLocs : m_prepassLocs;
            unsigned int     id = k ? m_prepassSkin.id : m_prepassShader.id;
            if ( id != bound ) rlEnableShader( bound = id );
            float cut = d.m_cutaway ? ( d.m_nearCut ? 2.0f : 1.0f ) : 0.0f;
            if ( cut != cutNow[k] )
            {
                cutNow[k] = cut;
                rlSetUniform( l.m_cutaway, &cut, RL_SHADER_UNIFORM_FLOAT, 1 );
            }
            Matrix model = MatrixMultiply( d.m_transform, stack );
            rlSetUniformMatrix( l.m_model, model );
            rlSetUniformMatrix( l.m_mvp, MatrixMultiply( MatrixMultiply( model, view ), proj ) );
            if ( d.m_bones ) rlSetUniformMatrices( l.m_bones, d.m_bones, d.m_boneCount );
            drawVao( d.m_mesh );
        }
    }
    rlDisableVertexArray();
    rlDisableShader();
    rlColorMask( true, true, true, true );
    lap( 1 );
}

void Gfx::lightsFor( int skin, const Bounds &b )
{
    // Those whose reach touches the draw's box (as built) or sphere; where more do, the strongest at its nearest.
    const std::vector<PointLight> &all = m_lights.m_points;
    int                            n = (int)std::min( all.size(), (size_t)MaxLights );
    int                            pick[MaxLights];
    float                          weight[MaxLights] = {};
    int                            count = 0;
    for ( int i = 0; i < n; ++i )
    {
        const PointLight &p = all[i];
        float             dist = 0.0f;
        if ( b.m_radius >= 0.0f )
        {
            if ( b.m_box )
            {
                float dx = std::max( fabsf( p.m_pos.x - b.m_centre.x ) - b.m_half.x, 0.0f );
                float dy = std::max( fabsf( p.m_pos.y - b.m_centre.y ) - b.m_half.y, 0.0f );
                float dz = std::max( fabsf( p.m_pos.z - b.m_centre.z ) - b.m_half.z, 0.0f );
                dist = sqrtf( dx * dx + dy * dy + dz * dz );
            }
            else
                dist = std::max( Vector3Distance( p.m_pos, b.m_centre ) - b.m_radius, 0.0f );
        }
        if ( dist >= p.m_radius ) continue;
        // A fixed light whose mask keeps it from the draw's box (behind the walls) is left out.
        if ( p.m_mask >= 0 && p.m_mask < (int)m_maskReach.size() && m_maskTex && b.m_radius >= 0.0f &&
             !( m_off & OFF_MASKS ) )
        {
            const Vector4 &q = m_maskReach[(size_t)p.m_mask];
            float          hx = b.m_box ? b.m_half.x : b.m_radius, hz = b.m_box ? b.m_half.z : b.m_radius;
            if ( b.m_centre.x + hx < q.x || b.m_centre.x - hx > q.z || b.m_centre.z + hz < q.y ||
                 b.m_centre.z - hz > q.w )
                continue;
        }
        float x = dist / p.m_radius, fall = 1.0f - x * x * x * x;
        weight[i] = ( 0.2126f * p.m_color.x + 0.7152f * p.m_color.y + 0.0722f * p.m_color.z ) * fall * fall /
                    ( dist * dist + 1.0f );
        pick[count++] = i;
    }
    if ( count > 8 ) ++m_stats.m_lightsOver8;
    m_stats.m_lightSum += std::min( count, MaxDrawLights );
    if ( count > MaxDrawLights )
    {
        ++m_stats.m_lightCut;
        std::partial_sort( pick, pick + MaxDrawLights, pick + count,
                           [&]( int a, int c ) { return weight[a] > weight[c]; } );
        count = MaxDrawLights;
        std::sort( pick, pick + count );
    }
    LightSet &last = m_lightSet[skin];
    if ( last.m_count == count && std::equal( pick, pick + count, last.m_index ) ) return;
    last.m_count = count;
    Vector4 pos[MaxDrawLights];
    Vector4 col[MaxDrawLights];
    Vector4 mask[MaxDrawLights];
    for ( int k = 0; k < count; ++k )
    {
        const PointLight &p = all[pick[k]];
        last.m_index[k] = (unsigned char)pick[k];
        pos[k] = { p.m_pos.x, p.m_pos.y, p.m_pos.z, p.m_radius };
        col[k] = { p.m_color.x, p.m_color.y, p.m_color.z, p.m_halo };
        mask[k] = maskOf( p );
    }
    // The shader is bound (drawOne).
    const CommonLocs &l = skin ? m_litSkinLocs : m_litLocs;
    rlSetUniform( l.m_lightCount, &count, RL_SHADER_UNIFORM_INT, 1 );
    if ( count > 0 )
    {
        rlSetUniform( l.m_lightPos, pos, RL_SHADER_UNIFORM_VEC4, count );
        rlSetUniform( l.m_lightColor, col, RL_SHADER_UNIFORM_VEC4, count );
        if ( l.m_lightMask >= 0 ) rlSetUniform( l.m_lightMask, mask, RL_SHADER_UNIFORM_VEC4, count );
    }
}

void Gfx::depthEqual( bool early )
{
    glDepthFunc( early ? GL_EQUAL : GL_LEQUAL );
    if ( early )
        rlDisableDepthMask();
    else
        rlEnableDepthMask();
}

void Gfx::leanBegin()
{
    Lean &n = m_leanState;
    n.m_view = rlGetMatrixModelview();
    n.m_proj = rlGetMatrixProjection();
    n.m_stack = rlGetMatrixTransform();
    n.m_program = 0;
    std::fill( std::begin( n.m_units ), std::end( n.m_units ), ~0u );
    for ( Lean::Program &p : n.m_programs ) p = Lean::Program();
}

void Gfx::leanEnd()
{
    rlDisableVertexArray();
    // As DrawMesh leaves them: the maps' units empty, the first unit active, no program.
    for ( int unit : MapUnits ) leanBind( unit, 0 );
    rlActiveTextureSlot( 0 );
    rlDisableShader();
    m_leanState.m_program = 0;
}

void Gfx::leanBind( int unit, unsigned id )
{
    if ( m_leanState.m_units[unit] == id ) return;
    m_leanState.m_units[unit] = id;
    glActiveTexture( GL_TEXTURE0 + unit );
    glBindTexture( GL_TEXTURE_2D, id );
}

void Gfx::leanDraw( const Drawable &d, const Surface &s, bool skin, bool multiply, const Bounds &b, bool early )
{
    Lean           &n = m_leanState;
    const Shader   &sh = skin ? m_litSkin : m_lit;
    const DrawLocs &l = skin ? m_skinDraw : m_litDraw;
    Lean::Program  &p = n.m_programs[skin ? 1 : 0];
    // A stain multiplies what is drawn under it (and keeps how much in the target's alpha), a decal is blended by its
    // alpha. First: a change of blending flushes rlgl's batch, which unbinds the program.
    bool over = !skin && decal( s ), mul = !skin && multiply;
    if ( mul )
        rlSetBlendFactorsSeparate( RL_DST_COLOR, RL_ZERO, RL_DST_ALPHA, RL_ZERO, RL_FUNC_ADD, RL_FUNC_ADD );
    else if ( over )
        rlSetBlendFactorsSeparate( RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD );
    if ( mul || over )
    {
        rlSetBlendMode( RL_BLEND_CUSTOM_SEPARATE );
        n.m_program = 0;
    }
    if ( n.m_program != sh.id )
    {
        rlEnableShader( sh.id );
        n.m_program = sh.id;
        // (The lit shaders have no view or projection of their own; DrawMesh would set them where they had.)
        if ( sh.locs[SHADER_LOC_MATRIX_VIEW] >= 0 ) rlSetUniformMatrix( sh.locs[SHADER_LOC_MATRIX_VIEW], n.m_view );
        if ( sh.locs[SHADER_LOC_MATRIX_PROJECTION] >= 0 )
            rlSetUniformMatrix( sh.locs[SHADER_LOC_MATRIX_PROJECTION], n.m_proj );
    }
    if ( !multiply ) lightsFor( skin ? 1 : 0, b );   // a stain is not lit
    // The surface's uniforms where it changes (the level's tiles come builder by builder: mostly the same one).
    if ( p.m_surface != &s )
    {
        p.m_surface = &s;
        rlSetUniform( l.m_uv, &s.m_uvScale, RL_SHADER_UNIFORM_FLOAT, 1 );
        rlSetUniform( l.m_params, &s.m_params, RL_SHADER_UNIFORM_VEC4, 1 );
        rlSetUniform( l.m_mask, &s.m_mask, RL_SHADER_UNIFORM_VEC2, 1 );
        rlSetUniform( l.m_variation, &s.m_variation, RL_SHADER_UNIFORM_FLOAT, 1 );
        rlSetUniform( l.m_sway, &s.m_sway, RL_SHADER_UNIFORM_FLOAT, 1 );
        float tint[4] = { s.m_tint.r / 255.0f, s.m_tint.g / 255.0f, s.m_tint.b / 255.0f, s.m_tint.a / 255.0f };
        if ( sh.locs[SHADER_LOC_COLOR_DIFFUSE] >= 0 )
            rlSetUniform( sh.locs[SHADER_LOC_COLOR_DIFFUSE], tint, RL_SHADER_UNIFORM_VEC4, 1 );
        if ( !skin )
        {
            float splat = s.m_layers[0].id > 0 ? s.m_layerMode : 0.0f;
            rlSetUniform( l.m_splat, &splat, RL_SHADER_UNIFORM_FLOAT, 1 );
        }
    }
    // The drawable's own (as cheap to compare as to set, but each set is a call into the driver).
    const Vector4  none = { 0.0f, 0.0f, 0.0f, 0.0f };
    const Vector4 &em = ( m_off & OFF_EMISSIVE ) ? none : d.m_emissive.x >= 0.0f ? d.m_emissive : s.m_emissive;
    float          cut = d.m_cutaway && !early ? ( d.m_nearCut ? 2.0f : 1.0f ) : 0.0f;
    bool           fresh = p.m_cut < 0.0f;
    auto           same = []( const Vector4 &a, const Vector4 &c ) { return std::memcmp( &a, &c, sizeof a ) == 0; };
    if ( fresh || !same( em, p.m_emissive ) ) rlSetUniform( l.m_emissive, &em, RL_SHADER_UNIFORM_VEC4, 1 );
    if ( fresh || cut != p.m_cut ) rlSetUniform( l.m_cutaway, &cut, RL_SHADER_UNIFORM_FLOAT, 1 );
    if ( fresh || !same( d.m_tint, p.m_tint ) ) rlSetUniform( l.m_tint, &d.m_tint, RL_SHADER_UNIFORM_VEC4, 1 );
    if ( fresh || !same( d.m_cloth, p.m_cloth ) ) rlSetUniform( l.m_cloth, &d.m_cloth, RL_SHADER_UNIFORM_VEC4, 1 );
    p.m_emissive = em, p.m_cut = cut, p.m_tint = d.m_tint, p.m_cloth = d.m_cloth;
    // The matrices as DrawMesh makes them (the pre-pass's depth is met to the bit), only where the transform changes:
    // the level's tiles all stand where they were built.
    if ( !p.m_placed || std::memcmp( &p.m_transform, &d.m_transform, sizeof( Matrix ) ) != 0 )
    {
        p.m_placed = true;
        p.m_transform = d.m_transform;
        Matrix model = MatrixMultiply( d.m_transform, n.m_stack );
        if ( sh.locs[SHADER_LOC_MATRIX_MODEL] >= 0 ) rlSetUniformMatrix( sh.locs[SHADER_LOC_MATRIX_MODEL], model );
        if ( sh.locs[SHADER_LOC_MATRIX_NORMAL] >= 0 )
            rlSetUniformMatrix( sh.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose( MatrixInvert( model ) ) );
        rlSetUniformMatrix( sh.locs[SHADER_LOC_MATRIX_MVP],
                            MatrixMultiply( MatrixMultiply( model, n.m_view ), n.m_proj ) );
    }
    if ( skin && m_boneLoc >= 0 ) rlSetUniformMatrices( m_boneLoc, d.m_bones, d.m_boneCount );
    // The maps: each unit only when another texture belongs there (the units are shared by both programs).
    for ( int unit : MapUnits ) leanBind( unit, s.m_material.maps[unit].texture.id );
    if ( !skin && s.m_layers[0].id > 0 )
        for ( int i = 0; i < 6; ++i ) leanBind( LayerUnits[i], s.m_layers[i].id );
    if ( d.m_mesh.vaoId > 0 )
    {
        rlEnableVertexArray( d.m_mesh.vaoId );
        if ( d.m_mesh.indices )
            rlDrawVertexArrayElements( 0, d.m_mesh.triangleCount * 3, 0 );
        else
            rlDrawVertexArray( 0, d.m_mesh.vertexCount );
    }
    if ( mul || over )
    {
        rlSetBlendMode( RL_BLEND_ALPHA );
        n.m_program = 0;   // (the flush unbound it)
    }
}

void Gfx::drawOne( const Drawable &d, const Surface &s, const Bounds &b, bool early )
{
    ++m_stats.m_litDraws;
    m_stats.m_litTris += d.m_mesh.indices ? d.m_mesh.triangleCount : d.m_mesh.vertexCount / 3;
    bool            skin = d.m_bones != nullptr;
    Shader          shader = skin ? m_litSkin : m_lit;
    const DrawLocs &l = skin ? m_skinDraw : m_litDraw;
    bool            multiply = stain( s );
    if ( m_lean )
    {
        leanDraw( d, s, skin, multiply, b, early );
        return;
    }
    // Bound once for its uniforms (SetShaderValue binds the program anew for each).
    rlEnableShader( shader.id );
    if ( !multiply ) lightsFor( skin ? 1 : 0, b );   // a stain is not lit
    rlSetUniform( l.m_uv, &s.m_uvScale, RL_SHADER_UNIFORM_FLOAT, 1 );
    rlSetUniform( l.m_params, &s.m_params, RL_SHADER_UNIFORM_VEC4, 1 );
    const Vector4 &em = d.m_emissive.x >= 0.0f ? d.m_emissive : s.m_emissive;
    rlSetUniform( l.m_emissive, &em, RL_SHADER_UNIFORM_VEC4, 1 );
    rlSetUniform( l.m_mask, &s.m_mask, RL_SHADER_UNIFORM_VEC2, 1 );
    rlSetUniform( l.m_variation, &s.m_variation, RL_SHADER_UNIFORM_FLOAT, 1 );
    rlSetUniform( l.m_sway, &s.m_sway, RL_SHADER_UNIFORM_FLOAT, 1 );
    // What the pre-pass drew has its holes from there (its depth is EQUAL only where the pre-pass kept it).
    float cut = d.m_cutaway && !early ? ( d.m_nearCut ? 2.0f : 1.0f ) : 0.0f;
    rlSetUniform( l.m_cutaway, &cut, RL_SHADER_UNIFORM_FLOAT, 1 );
    rlSetUniform( l.m_tint, &d.m_tint, RL_SHADER_UNIFORM_VEC4, 1 );
    rlSetUniform( l.m_cloth, &d.m_cloth, RL_SHADER_UNIFORM_VEC4, 1 );
    s.m_material.maps[MATERIAL_MAP_ALBEDO].color = s.m_tint;
    if ( !skin )
    {
        // Two more materials blended in (the ground and the rock out in the open): their maps on their units.
        float splat = s.m_layers[0].id > 0 ? s.m_layerMode : 0.0f;
        rlSetUniform( l.m_splat, &splat, RL_SHADER_UNIFORM_FLOAT, 1 );
        if ( splat > 0.0f )
        {
            for ( int i = 0; i < 6; ++i )
            {
                rlActiveTextureSlot( LayerUnits[i] );
                rlEnableTexture( s.m_layers[i].id );
            }
            rlActiveTextureSlot( 0 );
        }
        // A stain multiplies what is drawn under it, and keeps how much in the target's alpha: the fog pass darkens its
        // in-scattered light there as much (before, the fog was in the colour the stain multiplied). A decal leaves the
        // alpha as it is.
        bool over = decal( s );
        if ( multiply )
            rlSetBlendFactorsSeparate( RL_DST_COLOR, RL_ZERO, RL_DST_ALPHA, RL_ZERO, RL_FUNC_ADD, RL_FUNC_ADD );
        else if ( over )
            rlSetBlendFactorsSeparate( RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                                       RL_FUNC_ADD );
        if ( multiply || over ) rlSetBlendMode( RL_BLEND_CUSTOM_SEPARATE );
        DrawMesh( d.m_mesh, s.m_material, d.m_transform );
        if ( multiply || over ) rlSetBlendMode( RL_BLEND_ALPHA );
        return;
    }
    // A skinned figure: its own bones on the shared mesh, the skinning shader with the surface's maps.
    Mesh     mesh = d.m_mesh;
    Material mat = s.m_material;
    mesh.boneMatrices = const_cast<Matrix *>( d.m_bones );
    mesh.boneCount = d.m_boneCount;
    mat.shader = m_litSkin;
    DrawMesh( mesh, mat, d.m_transform );
}

void Gfx::draw( const std::vector<Drawable> &list, const std::vector<Surface> &surfaces )
{
    bool      prepassed = std::find( m_prepassed.begin(), m_prepassed.end(), (const void *)&list ) != m_prepassed.end();
    bool      equal = false;
    const int level = detail();
    if ( m_lean ) leanBegin();
    for ( const Drawable &d : list )
    {
        Bounds b;
        if ( !d.m_visible || d.m_detail > level || !inside( m_view, d, b ) ) continue;
        const Surface &s = surfaces[d.m_surface];
        if ( ( m_off & OFF_DECALS ) && ( decal( s ) || stain( s ) ) ) continue;
        bool early = prepassed && solid( s );
        if ( early != equal ) depthEqual( equal = early );
        drawOne( d, s, b, early );
    }
    if ( m_lean ) leanEnd();
    if ( equal ) depthEqual( false );
}

void Gfx::fog()
{
    if ( m_fogDone ) return;
    m_fogDone = true;
    lap( 2 );
    EndMode3D();
    EndTextureMode();
    Texture2D depth = { m_hdr.depth.id, m_hdr.texture.width, m_hdr.texture.height, 1, 0 };

    // The fog at half size: every second pixel's depth, its ray through the room, written as it is (no blending).
    int fw = ( m_viewW + m_fogStep - 1 ) / m_fogStep, fh = ( m_viewH + m_fogStep - 1 ) / m_fogStep;
    SetShaderValue( m_fogShader, GetShaderLocation( m_fogShader, "fogStep" ), &m_fogStep, SHADER_UNIFORM_INT );
    SetShaderValue( m_fogUp, GetShaderLocation( m_fogUp, "fogStep" ), &m_fogStep, SHADER_UNIFORM_INT );
    BeginTextureMode( m_fogTarget );
    rlViewport( 0, 0, fw, fh );
    Matrix  inv = MatrixInvert( m_viewProj );
    Vector2 size = { (float)m_viewW, (float)m_viewH };
    SetShaderValueMatrix( m_fogShader, GetShaderLocation( m_fogShader, "invViewProj" ), inv );
    SetShaderValue( m_fogShader, GetShaderLocation( m_fogShader, "sceneSize" ), &size, SHADER_UNIFORM_VEC2 );
    rlDisableColorBlend();
    if ( !( m_off & OFF_FOG ) ) fullscreen( m_fogShader, depth, fw, fh );
    rlEnableColorBlend();
    EndTextureMode();

    // Over the picture at its own size, weighed by depth: colour times transmittance plus the in-scattered light, that
    // times the target's alpha (1, or less under a stain).
    BeginTextureMode( m_hdrColor );
    rlViewport( 0, 0, m_viewW, m_viewH );
    Vector2 nearFar = { (float)Near, (float)Far };
    int     last[2] = { fw - 1, fh - 1 };
    SetShaderValue( m_fogUp, GetShaderLocation( m_fogUp, "nearFar" ), &nearFar, SHADER_UNIFORM_VEC2 );
    SetShaderValue( m_fogUp, GetShaderLocation( m_fogUp, "fogLast" ), last, SHADER_UNIFORM_IVEC2 );
    rlSetBlendFactorsSeparate( RL_DST_ALPHA, RL_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD );
    rlSetBlendMode( RL_BLEND_CUSTOM_SEPARATE );
    if ( !( m_off & OFF_FOG ) )
        fullscreen( m_fogUp, m_fogTarget.texture, m_viewW, m_viewH, depth, GetShaderLocation( m_fogUp, "depthTex" ) );
    rlSetBlendMode( RL_BLEND_ALPHA );
    // The lamps' cones while this target (the colour alone) is bound: no switch of their own.
    drawCones();
    EndTextureMode();

    enterScene();
    lap( 3 );
}

void Gfx::cones( const std::vector<Drawable> &list, float gain )
{
    m_coneList = &list;
    m_coneGain = gain;
}

void Gfx::drawCones()
{
    const std::vector<Drawable> *list = m_coneList;
    m_coneList = nullptr;
    if ( !list || list->empty() || m_quality == 3 || m_coneGain <= 0.0f || ( m_off & OFF_CONES ) ) return;
    // Into the colour alone, the scene's depth read (no feedback through the bound depth), added to the picture; the
    // cones' tiles straight to GL (one program, their matrices set once: they stand where they were built).
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlSetBlendMode( RL_BLEND_ADDITIVE );   // (before the program: its flush of rlgl's batch unbinds the program)
    const Shader &sh = m_coneShader;
    rlEnableShader( sh.id );
    Vector2 nearFar = { (float)Near, (float)Far };
    rlSetUniform( GetShaderLocation( sh, "nearFar" ), &nearFar, RL_SHADER_UNIFORM_VEC2, 1 );
    rlSetUniform( GetShaderLocation( sh, "viewPos" ), &m_cam.position, RL_SHADER_UNIFORM_VEC3, 1 );
    rlSetUniform( GetShaderLocation( sh, "coneGain" ), &m_coneGain, RL_SHADER_UNIFORM_FLOAT, 1 );
    glActiveTexture( GL_TEXTURE0 + ConeDepthUnit );
    glBindTexture( GL_TEXTURE_2D, m_hdr.depth.id );
    glActiveTexture( GL_TEXTURE0 );
    bool   placed = false;
    Matrix at = {};
    for ( const Drawable &d : *list )
    {
        Bounds b;
        if ( d.m_mesh.vaoId == 0 || !inside( m_view, d, b ) ) continue;
        if ( !placed || std::memcmp( &at, &d.m_transform, sizeof( Matrix ) ) != 0 )
        {
            placed = true;
            at = d.m_transform;
            rlSetUniformMatrix( sh.locs[SHADER_LOC_MATRIX_MODEL], at );
            rlSetUniformMatrix( sh.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose( MatrixInvert( at ) ) );
            rlSetUniformMatrix( sh.locs[SHADER_LOC_MATRIX_MVP], MatrixMultiply( at, m_viewProj ) );
        }
        rlEnableVertexArray( d.m_mesh.vaoId );
        if ( d.m_mesh.indices )
            rlDrawVertexArrayElements( 0, d.m_mesh.triangleCount * 3, 0 );
        else
            rlDrawVertexArray( 0, d.m_mesh.vertexCount );
    }
    rlDisableVertexArray();
    rlSetBlendMode( RL_BLEND_ALPHA );
    glActiveTexture( GL_TEXTURE0 + ConeDepthUnit );
    glBindTexture( GL_TEXTURE_2D, 0 );
    glActiveTexture( GL_TEXTURE0 );
    rlDisableShader();
    rlEnableDepthMask();
    rlEnableDepthTest();
}

void Gfx::planet( const Mesh &mesh, const Matrix &transform, Texture2D map, float spin, float glow )
{
    m_planetMat.maps[MATERIAL_MAP_ALBEDO].texture = map;
    SetShaderValue( m_planet, GetShaderLocation( m_planet, "spin" ), &spin, SHADER_UNIFORM_FLOAT );
    SetShaderValue( m_planet, GetShaderLocation( m_planet, "glow" ), &glow, SHADER_UNIFORM_FLOAT );
    DrawMesh( mesh, m_planetMat, transform );
}

void Gfx::ring( const Mesh &mesh, Texture2D map, float radius, Vector3 sky )
{
    static int loc = -2, skyLoc = -2;
    if ( loc == -2 ) loc = GetShaderLocation( m_planet, "ringMode" );
    if ( skyLoc == -2 ) skyLoc = GetShaderLocation( m_planet, "ringSky" );
    m_planetMat.maps[MATERIAL_MAP_ALBEDO].texture = map;
    if ( loc >= 0 ) SetShaderValue( m_planet, loc, &radius, SHADER_UNIFORM_FLOAT );
    if ( skyLoc >= 0 ) SetShaderValue( m_planet, skyLoc, &sky, SHADER_UNIFORM_VEC3 );
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

void Gfx::pond( const Mesh &mesh, const std::vector<Vector4> &ripples, Vector3 sky, bool shallow )
{
    if ( mesh.vertexCount == 0 ) return;
    static int skyLoc = -2, shallowLoc = -2;
    if ( skyLoc == -2 ) skyLoc = GetShaderLocation( m_pondShader, "pondSky" );
    if ( shallowLoc == -2 ) shallowLoc = GetShaderLocation( m_pondShader, "pondShallow" );
    SetShaderValue( m_pondShader, skyLoc, &sky, SHADER_UNIFORM_VEC3 );
    float flag = shallow ? 1.0f : 0.0f;
    if ( shallowLoc >= 0 ) SetShaderValue( m_pondShader, shallowLoc, &flag, SHADER_UNIFORM_FLOAT );
    int count = (int)std::min( ripples.size(), (size_t)4 );
    SetShaderValue( m_pondShader, m_rippleCountLoc, &count, SHADER_UNIFORM_INT );
    if ( count > 0 ) SetShaderValueV( m_pondShader, m_rippleLoc, ripples.data(), SHADER_UNIFORM_VEC4, count );
    BeginBlendMode( BLEND_ALPHA_PREMULTIPLY );
    rlDisableDepthMask();
    DrawMesh( mesh, m_pondMat, MatrixIdentity() );
    rlEnableDepthMask();
    EndBlendMode();
}

void Gfx::beginParticles( float boost, bool lit, bool smoke )
{
    rlDrawRenderBatchActive();
    BeginShaderMode( m_particle );
    int litFlag = smoke ? 2 : lit ? 1 : 0;
    SetShaderValue( m_particle, GetShaderLocation( m_particle, "boost" ), &boost, SHADER_UNIFORM_FLOAT );
    SetShaderValue( m_particle, GetShaderLocation( m_particle, "lit" ), &litFlag, SHADER_UNIFORM_INT );
    BeginBlendMode( smoke ? BLEND_ALPHA_PREMULTIPLY : BLEND_ADDITIVE );
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
    if ( !m_fogDone ) fog();
    EndMode3D();
    EndTextureMode();
}

void Gfx::post( const RenderTexture2D *out )
{
    Texture2D src = m_hdr.texture;
    // The part of the target the scene filled this frame (dynamic resolution).
    Vector2 part = { (float)m_viewW / src.width, (float)m_viewH / src.height };
    float   threshold = 1.2f;
    BeginTextureMode( m_bloomLevels[0] );
    Vector2 texel = { 1.0f / src.width, 1.0f / src.height };
    SetShaderValue( m_prefilter, GetShaderLocation( m_prefilter, "texel" ), &texel, SHADER_UNIFORM_VEC2 );
    SetShaderValue( m_prefilter, GetShaderLocation( m_prefilter, "threshold" ), &threshold, SHADER_UNIFORM_FLOAT );
    SetShaderValue( m_prefilter, GetShaderLocation( m_prefilter, "sceneScale" ), &part, SHADER_UNIFORM_VEC2 );
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
    SetShaderValue( m_final, GetShaderLocation( m_final, "colourVision" ), &m_colourVision, SHADER_UNIFORM_INT );
    SetShaderValue( m_final, GetShaderLocation( m_final, "sceneScale" ), &part, SHADER_UNIFORM_VEC2 );
    fullscreen( m_final, src, w, h, m_bloomLevels[0].texture, GetShaderLocation( m_final, "bloomTex" ) );
    if ( out ) EndTextureMode();
    frameEnd();
}

}   // namespace spaceopera
