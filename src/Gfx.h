/**
 * @file Gfx.h
 * @brief The renderer around raylib: an HDR target (supersampled), a shadow map for the sun and one for the suit lamp,
 *        the lit shaders with their shared light and fog uniforms, and the post chain (bloom, tone mapping, grade).
 * @author Rene Weller
 */
#pragma once

#include <raylib.h>

#include <functional>
#include <vector>

namespace spaceopera
{

constexpr int MaxLights = 32;   ///< point lights a frame (as in the shaders)

/** @brief A point light; it also glows in the fog. */
struct PointLight
{
    Vector3 m_pos;           ///< where
    float   m_radius;        ///< how far it reaches (m)
    Vector3 m_color;         ///< colour times intensity
    float   m_halo = 1.0f;   ///< how strongly it glows in the fog (0: not at all)
};

/** @brief Everything that lights a frame. */
struct Lighting
{
    Vector3                 m_sunDir;      ///< the direction the sunlight travels
    Vector3                 m_sunColor;    ///< its colour times intensity
    Vector3                 m_spotPos;     ///< the suit lamp
    Vector3                 m_spotDir;     ///< where it points
    Vector3                 m_spotColor;   ///< its colour times intensity
    float                   m_spotOuter;   ///< half its cone (degrees)
    float                   m_spotInner;   ///< half its full-strength core (degrees)
    float                   m_spotRange;   ///< how far it reaches (m)
    std::vector<PointLight> m_points;      ///< the point lights
    Vector3                 m_ambUp;       ///< ambient light from above
    Vector3                 m_ambDown;     ///< ambient light from below
    Vector4                 m_fog;         ///< base density, ground density, ground falloff (1/m), light shafts
    Vector3                 m_fogColor;    ///< the fog's own glow
    Vector3                 m_roomMin;     ///< the fog fills this box
    Vector3                 m_roomMax;     ///< up to here
};

/** @brief How a surface looks: its maps, tiling, roughness and metal scales, its glow. */
struct Surface
{
    Material m_material;                              ///< raylib material (lit shader, albedo, ORM, normal, emission)
    float    m_uvScale = 0.5f;                        ///< texture repeats per metre (triplanar) or per UV unit
    Vector4  m_params = { 1.0f, 1.0f, 1.0f, 0.0f };   ///< roughness scale, metallic scale, normal strength, UV mapping
    Vector4  m_emissive = { 0.0f, 0.0f, 0.0f, 0.0f };   ///< glow colour, mode (0 plain, 1 grooves, 2 emission map)
    Vector2  m_mask = { 0.3f, 0.7f };                   ///< where the grooves start and end glowing
    float    m_variation = 0.5f;                        ///< large-scale variation (breaks the tiling)
    Color    m_tint = WHITE;                            ///< albedo tint
};

/** @brief Something to draw: a mesh, its surface, where it is. */
struct Drawable
{
    Mesh    m_mesh;                         ///< the mesh
    int     m_surface = 0;                  ///< index of its surface
    Matrix  m_transform;                    ///< where it is
    Vector4 m_emissive = { -1, 0, 0, 0 };   ///< its own glow (x < 0: the surface's)
    bool    m_visible = true;               ///< drawn (false: it only casts shadows, like the ceiling)
    bool    m_sunShadow = true;             ///< casts a sun shadow
    bool    m_spotShadow = true;            ///< casts a lamp shadow
};

/** @brief The renderer. */
class Gfx
{
public:
    /**
     * @brief Loads the shaders and makes the targets.
     * @param[in] width   picture width (pixels)
     * @param[in] height  picture height
     * @param[in] ss      supersampling (the scene is drawn this much larger)
     */
    void init( int width, int height, float ss );

    /**
     * @brief Makes a surface with the lit shader.
     * @param[in] albedo    albedo map
     * @param[in] orm       occlusion, roughness, metallic map
     * @param[in] normal    normal map
     * @param[in] emission  emission map (id 0: none)
     * @return the surface
     */
    Surface surface( Texture2D albedo, Texture2D orm, Texture2D normal, Texture2D emission );

    /**
     * @brief Renders the two shadow maps.
     * @param[in] lights  this frame's lights (sun direction, lamp)
     * @param[in] centre  the middle of what the sun's map covers
     * @param[in] lists   what casts shadows
     */
    void shadows( const Lighting &lights, Vector3 centre, const std::vector<const std::vector<Drawable> *> &lists );

    /**
     * @brief Starts the frame in the HDR target: clears it, draws the sky, begins 3D, sets the light uniforms.
     * @param[in] cam     the camera
     * @param[in] lights  this frame's lights
     * @param[in] time    seconds since start
     * @param[in] sky     the nebula picture
     */
    void beginScene( const Camera3D &cam, const Lighting &lights, float time, Texture2D sky );

    /**
     * @brief Draws a list with the lit shader.
     * @param[in] list      what to draw
     * @param[in] surfaces  the surfaces the list refers to
     */
    void draw( const std::vector<Drawable> &list, const std::vector<Surface> &surfaces );

    /**
     * @brief Draws the gas giant.
     * @param[in] mesh       a unit sphere
     * @param[in] transform  where and how large
     * @param[in] map        its cloud map
     * @param[in] spin       how far it has turned (texture u)
     */
    void planet( const Mesh &mesh, const Matrix &transform, Texture2D map, float spin );

    /**
     * @brief Draws the glass (blended over what is behind it).
     * @param[in] mesh       the panes
     * @param[in] transform  where
     */
    void glass( const Mesh &mesh, const Matrix &transform );

    /**
     * @brief Begins additive particles.
     * @param[in] boost  intensity factor (the vertex colours stop at 1)
     * @param[in] lit    true: lit by the sun and the lamp (dust)
     */
    void beginParticles( float boost, bool lit );

    /** @brief Ends the particles. */
    void endParticles();

    /** @brief Ends 3D and the HDR target. */
    void endScene();

    /**
     * @brief Bloom, tone mapping and grade, into a target or onto the screen.
     * @param[in] out  the target (nullptr: the screen)
     */
    void post( const RenderTexture2D *out );

    float m_exposure = 1.0f;   ///< exposure before tone mapping
    float m_bloom = 0.08f;     ///< how much bloom is mixed in
    int   m_width = 0;         ///< picture width
    int   m_height = 0;        ///< picture height

private:
    /** @brief Uniform locations every lit shader shares. */
    struct CommonLocs
    {
        int m_viewPos, m_time, m_frame, m_sunDir, m_sunColor, m_sunVP, m_sunShadow, m_sunTexel;
        int m_spotPos, m_spotDir, m_spotColor, m_spotCos, m_spotRange, m_spotVP, m_spotShadow, m_spotNF;
        int m_lightCount, m_lightPos, m_lightColor, m_ambUp, m_ambDown, m_fogParams, m_fogColor, m_roomMin, m_roomMax;
    };

    /** @brief Loads a shader from vertex and fragment source. */
    Shader load( const char *vs, const char *fs );
    /** @brief Finds the shared locations of a shader. */
    CommonLocs locate( Shader s );
    /** @brief Sets the shared uniforms of a shader. */
    void upload( Shader s, const CommonLocs &l );
    /** @brief Draws a texture over the whole current target through a shader (and binds a second one to extraLoc). */
    void fullscreen( Shader s, Texture2D src, int w, int h, Texture2D extra = {}, int extraLoc = -1 );
    /** @brief An HDR (half float) target, with or without a depth buffer. */
    RenderTexture2D hdrTarget( int w, int h, bool depth );
    /** @brief A depth-only target for a shadow map. */
    RenderTexture2D depthTarget( int size );
    /** @brief Binds the shadow maps to their units (12, 13) or unbinds them. */
    void bindShadows( bool on );

    Shader     m_lit, m_glassShader, m_sky, m_planet, m_particle, m_depth, m_prefilter, m_down, m_up, m_final;
    CommonLocs m_litLocs, m_glassLocs, m_skyLocs, m_planetLocs, m_particleLocs;
    int        m_litUv, m_litParams, m_litEmissive, m_litMask, m_litVariation;
    Material   m_depthMat;
    Material   m_glassMat;
    Material   m_planetMat;

    RenderTexture2D              m_hdr;
    std::vector<RenderTexture2D> m_bloomLevels;
    RenderTexture2D              m_sunMap;
    RenderTexture2D              m_spotMap;
    int                          m_sunSize = 4096;
    int                          m_spotSize = 2048;
    float                        m_sunExtent = 46.0f;
    Matrix                       m_sunVP;
    Matrix                       m_spotVP;
    Vector2                      m_spotNF = { 0.1f, 32.0f };
    Lighting                     m_lights;
    float                        m_time = 0.0f;
    float                        m_frame = 0.0f;
    Camera3D                     m_cam;
};

}   // namespace spaceopera
