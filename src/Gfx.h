/**
 * @file Gfx.h
 * @brief The renderer around raylib: an HDR target (supersampled, or smaller on a weak GPU), a shadow map for the sun
 *        (what does not move kept from frame to frame) and one for the suit lamp, a depth pre-pass, the lit shaders
 *        with the lights that reach each draw, the volumetric fog as its own pass at half size, and the post chain
 *        (bloom, tone mapping, grade, FXAA).
 * @author Rene Weller
 */
#pragma once

#include <raylib.h>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace spaceopera
{

constexpr int MaxLights = 32;   ///< point lights a frame (the fog's halos, glass, water, particles)
/// Point lights a lit draw at most: all of the frame's that reach it. A cap of eight (or sixteen) left visible seams
/// at the tiles' edges and darker bushes beside lamps where a fight's flashes and a machine's or a garden's lamps crowd
/// (16 m and 32 m tiles reach many; a fifth to a half of the draws in the Mill, Velm and the gardens).
constexpr int MaxDrawLights = MaxLights;

/** @brief A point light; it also glows in the fog. */
struct PointLight
{
    Vector3 m_pos;           ///< where
    float   m_radius;        ///< how far it reaches (m)
    Vector3 m_color;         ///< colour times intensity
    float   m_halo = 1.0f;   ///< how strongly it glows in the fog (0: not at all)
    int     m_mask = -1;     ///< its mask among the level's (Gfx::lightMasks): the walls stop it (-1: none, as a flash)
};

/**
 * @brief Where each of a level's fixed lights reaches past the walls (Level::lightMasks): one square of an atlas per
 *        light, seen from above, 1 where its light arrives and 0 behind the walls, soft at the shadows' edges.
 */
struct LightMasks
{
    std::vector<unsigned char> m_atlas;                     ///< the squares, row by row (one byte a texel)
    int                        m_width = 0, m_height = 0;   ///< the atlas's size (texels)
    /// Per light: where its square lies (texture coordinates = xy + world x, z times z; w 1: masked, 0: not).
    std::vector<Vector4> m_place;
    std::vector<Vector4> m_reach;        ///< per light: the rectangle its light reaches (west, north, east, south; m)
    int                  m_open = 0;     ///< lights nothing stops within their reach (they need none)
    int                  m_inRock = 0;   ///< lights deep in a wall, no open point near them (none)
    int                  m_moved = 0;    ///< lights in a wall that shine from the open point in front of it (masked)
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
    std::vector<Vector4>    m_water;       ///< balls of water (middle, radius): caustics near them (8 at most)
    Vector4 m_clouds = { 0, 0, 0,
                         0 };   ///< clouds' shadows: how much they darken the sun (0 none), size (1/m), drift (m/s)
    /// The wind the plants sway in: where it blows (x, z of a unit vector), its speed (m/s; 0: still air, a vacuum),
    /// a gust blowing now (0 .. 1, on top of the gusts that travel over the place by themselves).
    Vector4 m_wind = { 0, 0, 0, 0 };
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
    /// Two more materials blended in by a slow noise of the place (albedo, ORM, normal each; id 0: none): the ground
    /// and the rock out in the open, of their variants, with no patches and no tiles repeating (triplanar only).
    Texture2D m_layers[6] = {};
    /// How the layers are laid: 1 blended by the noise (above); 2 a patchwork, each vertex naming its material by its
    /// u's whole fours (MeshBuilder::tag: a wall's or floor's variants in one draw instead of three, not turned); 3
    /// the ground out in the open with the second material along the ways (the vertex's u) and the third at the rock's
    /// foot (its v), both also in a few patches of the noise.
    float m_layerMode = 1.0f;
    /// Plants in the wind (the lit vertex shader): 0 still; 1 grass (its cards' tops sway); 2 leaf cards (the whole
    /// card, more the higher it hangs); 3 fir branches (as leaves, their tips more); 4 leaves that do not move. From 2
    /// on lit softly, as the volume they stand for (wrapped light, the sun's shadow looked up outside the crown). Only
    /// for what is drawn after the depth pre-pass and casts no shadow (the cut-outs): the depth passes do not sway.
    float m_sway = 0.0f;
};

/** @brief Something to draw: a mesh, its surface, where it is. */
struct Drawable
{
    Mesh          m_mesh;                         ///< the mesh
    int           m_surface = 0;                  ///< index of its surface
    Matrix        m_transform;                    ///< where it is
    Vector4       m_emissive = { -1, 0, 0, 0 };   ///< its own glow (x < 0: the surface's)
    bool          m_visible = true;               ///< drawn (false: it only casts shadows, like the ceiling)
    bool          m_sunShadow = true;             ///< casts a sun shadow
    bool          m_spotShadow = true;            ///< casts a lamp shadow
    bool          m_cutaway = false;              ///< dissolves where it stands between the camera and the hero
    bool          m_nearCut = false;              ///< with m_cutaway: right at the camera too (leaves it comes into)
    const Matrix *m_bones = nullptr;              ///< a skinned figure's bone matrices (they must live until drawn)
    int           m_boneCount = 0;                ///< how many
    Vector4       m_tint = { 1, 1, 1, 0 };   ///< clothed surfaces: the clothes' colour, the height the skin shows above
    Vector4 m_cloth = { -1, 0, 0, 0 };   ///< clothed: the trousers' colour (x < 0: as m_tint); w the cut (Shaders.h)
    /// Drawn only from this much detail on (Gfx::detail): 0 always, 1 not on frugal, 2 middle and high only.
    int m_detail = 0;
    // The mesh's bounds, kept by Gfx in what stays put (the level's tiles), so the frustum needs no look-up there.
    mutable Vector4      m_sphere = { 0, 0, 0, -1 };   ///< middle and radius in the mesh's space
    mutable Vector3      m_half = { 0, 0, 0 };         ///< half the size of its box in the mesh's space
    mutable const float *m_sphereOf = nullptr;         ///< the vertices it was measured for
    mutable int          m_sphereCount = -1;           ///< and their count (-1: not measured)
};

/** @brief The renderer. */
class Gfx
{
public:
    /**
     * @brief Loads the shaders and makes the targets.
     * @param[in] width    picture width (pixels)
     * @param[in] height   picture height
     * @param[in] quality  0 low (the Steam Deck), 1 middle, 2 high
     * @param[in] ss       supersampling: the scene is drawn this much larger (negative: the quality's)
     */
    void init( int width, int height, int quality, float ss = -1.0f );

    /**
     * @brief Sets how much the renderer does and makes its targets anew. Low (the Steam Deck): the scene at 3/4 of the
     *        picture, 8 fog steps, 4 shadow samples, shadow maps of 2048 and 1024; middle: the scene as large as the
     *        picture, 20 steps, 8 samples; high: supersampled 1.5 times, 40 steps, 12 samples, maps of 4096 and 2048.
     * @param[in] quality  0..2
     * @param[in] ss       supersampling (negative: the quality's)
     */
    void setQuality( int quality, float ss = -1.0f );

    /**
     * @brief A new picture size (the window changed): the targets anew, the quality as it is.
     * @param[in] width   the picture's width
     * @param[in] height  its height
     */
    void resize( int width, int height );

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
     * @brief A level's light masks (Level::lightMasks): the lit surfaces, the fog's halos, the glass and the water
     *        take a fixed light only where its mask lets it through, and a draw gets no light whose reach misses it.
     *        Replaces the last level's; an empty atlas clears them.
     * @param[in] masks  the masks
     */
    void lightMasks( const LightMasks &masks );

    /**
     * @brief A level's surroundings for what reflects (a small cube map, made once): its ambient light below and
     *        above, its haze, and its lamps' colours as soft spots round the horizon (where a room's lamps hang),
     *        blurrier in the smaller levels of detail for the rougher surfaces.
     * @param[in] ambUp    ambient light from above
     * @param[in] ambDown  ambient light from below
     * @param[in] fog      the haze's own glow
     * @param[in] lamps    the level's lamps' colours times intensity (the brightest are used)
     * @param[in] seed     where round the horizon they hang
     */
    void environment( Vector3 ambUp, Vector3 ambDown, Vector3 fog, const std::vector<Vector3> &lamps, unsigned seed );

    /**
     * @brief Forgets what belonged to the last place (a new one is being built): the meshes' measured bounds (a new
     *        mesh can get a freed one's vertex array and vertex array object, and its bounds would be taken for its own)
     *        and the sun's kept map.
     */
    void newPlace();

    /**
     * @brief Where a level's floors are wet or polished (one texel a cell, filtered): red how wet (puddles), green its
     *        floor's height ((h + 4) / 16), blue how polished, alpha where its water gathers. Empty: none (dry).
     * @param[in] texels  the cells' texels, row by row (RGBA, four bytes each)
     * @param[in] cols    the map's columns
     * @param[in] rows    its rows
     * @param[in] cell    a cell's size (m)
     */
    void wetMap( const std::vector<unsigned char> &texels, int cols, int rows, float cell );

    /**
     * @brief Soft cones of light in the haze under the lamps: added to the picture by the fog pass right after the fog
     *        (in the same target, no switch of its own), fading along them and towards their rims, and where they meet
     *        what stands in them (the scene's depth). Not on frugal. Call before fog(); the list must live until then.
     * @param[in] list  the cones (colour: the light's, alpha its strength; texture v: 0 at the lamp, 1 at the end)
     * @param[in] gain  how strongly the haze shows them
     */
    void cones( const std::vector<Drawable> &list, float gain );

    /**
     * @brief Renders the two shadow maps; the frame begins here. The sun's map stands on a grid of 1.5 m in the light's
     *        own axes: while its middle and the sun stay, what does not move (the first `still` lists) is kept in a
     *        copy and only what moves is drawn over it.
     * @param[in] lights  this frame's lights (sun direction, lamp)
     * @param[in] centre  the middle of what the sun's map covers
     * @param[in] lists   what casts shadows
     * @param[in] still   how many of the lists, from the first, do not move (their shadows are kept)
     */
    void shadows( const Lighting &lights, Vector3 centre, const std::vector<const std::vector<Drawable> *> &lists,
                  size_t still = 1 );

    /**
     * @brief Starts the frame in the HDR target: clears it, draws the sky, begins 3D, sets the light uniforms.
     * @param[in] cam     the camera
     * @param[in] lights  this frame's lights
     * @param[in] time    seconds since start
     * @param[in] sky     the nebula picture
     */
    void beginScene( const Camera3D &cam, const Lighting &lights, float time, Texture2D sky );

    /**
     * @brief The depth pre-pass: the solid surfaces of these lists (not decals, stains or cut-outs) into the depth
     *        only, so draw() shades each of their pixels once. After beginScene(), before draw().
     * @param[in] lists     what will be drawn
     * @param[in] surfaces  the surfaces the lists refer to
     */
    void prepass( const std::vector<const std::vector<Drawable> *> &lists, const std::vector<Surface> &surfaces );

    /**
     * @brief Draws a list with the lit shader (a list given to prepass() tests its depth EQUAL and writes none; decals
     *        and stains test LEQUAL and write as before, in the list's order).
     * @param[in] list      what to draw
     * @param[in] surfaces  the surfaces the list refers to
     */
    void draw( const std::vector<Drawable> &list, const std::vector<Surface> &surfaces );

    /**
     * @brief The volumetric fog over the opaque scene: raymarched at half size from the depth, laid over the picture
     *        with a depth-aware upsampling (where a stain lies, its darkening in the target's alpha darkens the fog's
     *        light too). After the opaque draws and the decals, before what is blended over them
     *        (glass, water, particles, and what glows by itself with no fog: the star, the ring). endScene() does it if
     *        it has not been done.
     */
    void fog();

    /**
     * @brief Draws a planet lit by the sun, or a star glowing by itself.
     * @param[in] mesh       a unit sphere
     * @param[in] transform  where and how large
     * @param[in] map        its cloud (or surface) map
     * @param[in] spin       how far it has turned (texture u)
     * @param[in] glow       0: a planet; above: a star this bright (limb darkened)
     */
    void planet( const Mesh &mesh, const Matrix &transform, Texture2D map, float spin, float glow = 0.0f );

    /**
     * @brief Draws the glass (blended over what is behind it).
     * @param[in] mesh       the panes
     * @param[in] transform  where
     * @param[in] ice        1: ice instead of glass
     */
    void glass( const Mesh &mesh, const Matrix &transform, float ice = 0.0f );

    /**
     * @brief Draws a ring habitat's band from inside (the planet shader in its ring mode).
     * @param[in] mesh    the band
     * @param[in] map     its land (alpha: towns)
     * @param[in] radius  the ring's radius (m)
     * @param[in] sky     the sky's colour (linear): the land fades into it with distance
     */
    void ring( const Mesh &mesh, Texture2D map, float radius, Vector3 sky );

    /**
     * @brief Draws a ball of water (after the opaque scene; it does not write depth).
     * @param[in] mesh       the ball
     * @param[in] transform  where it is
     */
    void water( const Mesh &mesh, const Matrix &transform );

    /**
     * @brief Draws ponds (after the opaque scene and the fog; blended, no depth written): the wind's small waves, rings
     *        where drops fall and round what wades in them, the sky and the sun mirrored, the shore fading out.
     * @param[in] mesh     the ponds' discs, in the world (texture coordinates -1 .. 1 across each: its shore at 1)
     * @param[in] ripples  what wades (x, z, strength, how fast 0 .. 1); four at most are used
     * @param[in] sky      the sky's colour it mirrors (linear)
     * @param[in] shallow  shallow water over the lawn (the ground shows through, darker; a damp rim past the shore;
     *                     splashes round what moves through it)
     */
    void pond( const Mesh &mesh, const std::vector<Vector4> &ripples, Vector3 sky, bool shallow );

    /**
     * @return how much detail the quality draws (Drawable::m_detail): 0 frugal, 1 low, 2 middle and high
     */
    int detail() const { return m_quality == 3 ? 0 : ( m_quality == 0 ? 1 : 2 ); }

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
     * @brief Bloom, tone mapping, grade and FXAA, into a target or onto the screen; the frame ends here.
     * @param[in] out  the target (nullptr: the screen)
     */
    void post( const RenderTexture2D *out );

    float   m_exposure = 1.0f;                        ///< exposure before tone mapping
    Vector4 m_grade = { 1.0f, 1.0f, 0.0f, 0.0f };     ///< colour, contrast, extra grain, extra vignette
    Vector3 m_gradeTint = { 1.0f, 1.0f, 1.0f };       ///< the picture's tint after the grade
    float   m_bloom = 0.08f;                          ///< how much bloom is mixed in
    int     m_colourVision = 0;                       ///< 1 red-green, 2 blue-yellow (the colours moved apart)
    bool    m_sunShadows = true;                      ///< render the sun's shadow map (off where there is no sun)
    float   m_fill = 0.3f;                            ///< the figures' fill light (a share of the sun's); 0: none
    Vector3 m_cutCentre = { 0.0f, -1000.0f, 0.0f };   ///< the hero, whom walls in front of dissolve around
    float   m_cutRadius = 2.2f;                       ///< radius of that hole at the hero's distance (m)
    int     m_width = 0;                              ///< picture width
    int     m_height = 0;                             ///< picture height
    int     m_quality = 2;                            ///< 0 low, 1 middle, 2 high, 3 frugal
    float   m_scale = 1.5f;                           ///< the scene's size against the picture's
    float   m_scaleAsked = -1.0f;                     ///< the scale asked for (negative: the quality's)
    float   m_gpuMs = 0.0f;                           ///< the GPU's time per frame, smoothed (ms)
    float   m_cpuMs = 0.0f;                           ///< the processor's time per frame in the renderer, smoothed (ms)
    bool    m_dynamicRes = true;     ///< low and frugal: the scene drawn smaller while the GPU misses the budget
    float   m_frameBudget = 25.0f;   ///< the GPU's time a frame may take (ms; 40 frames a second)
    float   m_dynScale = 1.0f;       ///< the share of the scene's target drawn this frame (dynamic resolution)
    bool    m_cacheShadows = true;   ///< keep the sun's shadow of what does not move (off: drawn every frame; tests)
    /// The lit draws straight to GL with their state kept from draw to draw (program, maps, matrices, uniforms set
    /// only where they change); off: each through raylib's DrawMesh, as before (the bench compares both).
    bool m_lean = true;
    /// The bench: GPU timestamps at the parts of the frame (Stats::m_gpuPartMs; reading them may wait for the GPU).
    bool m_stamps = false;
    /// The interiors' features switched off, for the bench's breakdown (--benchparts): bits of OffE.
    int m_off = 0;
    /** @brief What m_off switches off. */
    enum OffE
    {
        OFF_MASKS = 1,   ///< the fixed lights' masks (they shine through the walls again)
        OFF_CONES = 2,   ///< the lamps' cones in the haze
        OFF_ENV = 4      ///< the level's cube map and the wet map (the old fake environment, dry floors)
    };

    /** @brief The six planes of a frustum (a point is inside where all of them are >= 0). */
    struct Frustum
    {
        Vector4 m_planes[6];
    };
    /** @brief The frustum of a view-projection matrix (raylib's order: the view, then the projection). */
    static Frustum frustum( const Matrix &vp );

    /** @brief What the renderer measured since resetStats(). */
    struct Stats
    {
        int    m_frames = 0;        ///< frames drawn (shadows() to post())
        double m_cpuMs = 0.0;       ///< the processor's time for them, summed (from shadows() to the end of post())
        int    m_gpuFrames = 0;     ///< frames the GPU's timer has reported
        double m_gpuMs = 0.0;       ///< the GPU's time for those, summed (GL_TIME_ELAPSED over the same span)
        long   m_litDraws = 0;      ///< lit draws
        long   m_depthDraws = 0;    ///< depth-only draws (shadow maps, pre-pass)
        long   m_lightsOver8 = 0;   ///< lit draws more than eight lights reached
        long   m_lightSum = 0;      ///< the point lights the lit draws got, summed
        long   m_lightCut = 0;      ///< lit draws more than MaxDrawLights lights reached (the weakest left out)
        int    m_sunRenders = 0;    ///< frames the sun's map of what does not move was drawn anew
        double m_litTris = 0.0;     ///< triangles of the lit draws
        double m_depthTris = 0.0;   ///< triangles of the depth-only draws
        /// The processor's time by part of the frame, summed (ms): the shadow maps, the sky and the pre-pass, the lit
        /// draws, the fog and the cones, the rest (what glows, glass, water, particles, the post chain).
        double m_partMs[5] = {};
        double m_gpuPartMs[5] = {};   ///< the GPU's time by the same parts (timestamps; with m_stamps on)
        int    m_stampFrames = 0;     ///< frames whose timestamps were read
    };

    /** @brief Starts counting anew. */
    void resetStats();

    /**
     * @brief What was counted (waits for the GPU's timers still out).
     * @return the sums since resetStats()
     */
    Stats stats();

private:
    /** @brief Uniform locations every lit shader shares. */
    struct CommonLocs
    {
        int m_viewPos, m_time, m_frame, m_sunDir, m_sunColor, m_sunVP, m_sunShadow, m_sunTexel;
        int m_spotPos, m_spotDir, m_spotColor, m_spotCos, m_spotRange, m_spotVP, m_spotShadow, m_spotNF;
        int m_lightCount, m_lightPos, m_lightColor, m_ambUp, m_ambDown, m_fogParams, m_fogColor, m_roomMin, m_roomMax;
        int m_waterCount, m_waterBalls, m_fogSteps, m_shadowTaps, m_clouds, m_wind;
        int m_lightMask;   ///< per light: its mask's place (lightMasks)
        int m_envOn;       ///< the level's cube map is there (environment)
        int m_wet;         ///< the wet map's scale (x, z), on (wetMap)
    };

    /** @brief Loads a shader from vertex and fragment source. */
    Shader load( const char *vs, const char *fs );
    /** @brief Finds the shared locations of a shader. */
    CommonLocs locate( Shader s );
    /**
     * @brief Sets the shared uniforms of a shader.
     * @param[in] s       the shader
     * @param[in] l       its locations
     * @param[in] points  true: all the frame's point lights (false: none yet; each draw sets its own)
     */
    void upload( Shader s, const CommonLocs &l, bool points = true );
    /** @brief Draws a texture over the whole current viewport through a shader (and binds a second one to extraLoc). */
    void fullscreen( Shader s, Texture2D src, int w, int h, Texture2D extra = {}, int extraLoc = -1 );
    /** @brief An HDR (half float) target, with or without a depth texture. */
    RenderTexture2D hdrTarget( int w, int h, bool depth );
    /** @brief A depth-only target for a shadow map. */
    RenderTexture2D depthTarget( int size );
    /** @brief Frees the targets. */
    void unloadTargets();
    /** @brief Binds the shadow maps to their units (12, 13) or unbinds them. */
    void bindShadows( bool on );
    /**
     * @brief Begins 3D in the current target with the projection made for the scene's viewport (raylib's would take
     *        the target's whole size).
     * @param[in] cam  the camera
     * @param[in] w    the viewport's width
     * @param[in] h    and height
     */
    void begin3D( const Camera3D &cam, int w, int h );
    /** @brief Binds the HDR target with the scene's viewport and begins 3D (the frame's camera) in it. */
    void enterScene();

    /** @brief Where the per-draw uniforms of a lit shader are. */
    struct DrawLocs
    {
        int m_uv, m_params, m_emissive, m_mask, m_variation, m_cutaway, m_tint, m_cloth, m_cutCentre, m_cutRadius;
        int m_splat;   ///< the blended materials on (Surface::m_layers)
        int m_sway;    ///< how it moves in the wind (Surface::m_sway)
    };
    /** @brief Finds them. */
    DrawLocs drawLocs( Shader s );

    /** @brief The uniforms of a depth-only shader (the shadow maps, the pre-pass). */
    struct DepthLocs
    {
        int m_viewProj, m_mvp, m_model, m_bones, m_cutaway, m_cutCentre, m_cutRadius, m_viewPos;
    };
    /** @brief Finds them. */
    DepthLocs depthLocs( Shader s );

    /** @brief Where a drawable stands in the world. */
    struct Bounds
    {
        Vector3 m_centre;   ///< its middle
        float   m_radius;   ///< its sphere's radius (negative: unknown, never culled)
        Vector3 m_half;     ///< as built (no transform): half its box, which is tighter than the sphere
        bool    m_box;      ///< m_half holds
    };

    /**
     * @brief Whether a drawable can show in a frustum (its mesh's sphere, moved by its transform).
     * @param[in]  f  the frustum
     * @param[in]  d  the drawable
     * @param[out] b  where it stands
     * @return true: it may show
     */
    bool inside( const Frustum &f, const Drawable &d, Bounds &b );

    /**
     * @brief Draws a mesh's triangles through the shader bound (its vertex array as raylib uploaded it).
     * @param[in] mesh  the mesh
     */
    void drawVao( const Mesh &mesh );

    /**
     * @brief Draws the shadow casters of some of the lists into the map bound.
     * @param[in] cull   the light's frustum
     * @param[in] vp     its view-projection
     * @param[in] sun    true: the sun's casters, false: the lamp's
     * @param[in] lists  the lists
     * @param[in] from   the first list
     * @param[in] to     one past the last
     */
    void casters( const Frustum &cull, const Matrix &vp, bool sun,
                  const std::vector<const std::vector<Drawable> *> &lists, size_t from, size_t to );

    /**
     * @brief Copies a shadow map's depth into another of the same size.
     * @param[in] from  the source
     * @param[in] to    the target
     */
    void copyDepth( const RenderTexture2D &from, const RenderTexture2D &to );

    /**
     * @brief Sets a lit shader's point lights for a draw: those whose reach touches it (the strongest
     *        MaxDrawLights where more do).
     * @param[in] skin  0 the plain lit shader, 1 the skinned one
     * @param[in] b     where the draw stands
     */
    void lightsFor( int skin, const Bounds &b );

    /**
     * @brief Draws one lit drawable.
     * @param[in] d      the drawable
     * @param[in] s      its surface
     * @param[in] b      where it stands
     * @param[in] early  its depth is the pre-pass's (no cut-away of its own: the pre-pass made the holes)
     */
    void drawOne( const Drawable &d, const Surface &s, const Bounds &b, bool early );

    /**
     * @brief The depth test of the pre-passed draws (EQUAL, no writes) or of the others (LEQUAL, written).
     * @param[in] early  true: the pre-passed
     */
    void depthEqual( bool early );

    /**
     * @brief The lean lit path's state (m_lean): what is bound and set, so a draw sets only what differs from the last.
     *        Valid within one draw() call (other code binds and sets between them).
     */
    struct Lean
    {
        Matrix   m_view, m_proj, m_stack;   ///< the camera's matrices and rlgl's transform for this list
        unsigned m_program = 0;             ///< the program bound (0: none yet)
        unsigned m_units[16];               ///< the texture bound per unit (~0u: not known)
        /** @brief What was last set on one of the two lit programs. */
        struct Program
        {
            const Surface *m_surface = nullptr;   ///< the surface whose uniforms are set (nullptr: none)
            bool           m_placed = false;      ///< m_transform holds
            Matrix         m_transform = {};      ///< the transform the matrices were set for
            Vector4        m_emissive = {}, m_tint = {}, m_cloth = {};   ///< the drawable's uniforms as set
            float          m_cut = -1.0f;   ///< the cut-away as set (negative: none of the drawable's set yet)
        } m_programs[2];
    };
    Lean m_leanState;

    /** @brief Starts the lean path for a list (the camera's matrices; nothing known bound). */
    void leanBegin();
    /** @brief Ends it: the maps unbound as DrawMesh leaves them, no vertex array, no program. */
    void leanEnd();
    /**
     * @brief Binds a texture to a unit unless it is bound there already.
     * @param[in] unit  the unit
     * @param[in] id    the texture (0: none)
     */
    void leanBind( int unit, unsigned id );
    /**
     * @brief Draws one lit drawable straight to GL (as DrawMesh would, with the state kept from the last draw).
     * @param[in] d         the drawable
     * @param[in] s         its surface
     * @param[in] skin      a skinned figure
     * @param[in] multiply  a stain (not lit; multiplies what is under it)
     * @param[in] b         where it stands (its lights)
     * @param[in] early     its depth is the pre-pass's
     */
    void leanDraw( const Drawable &d, const Surface &s, bool skin, bool multiply, const Bounds &b, bool early );

    /**
     * @brief Adds the processor's time since the last mark to a part of the frame (Stats::m_partMs).
     * @param[in] part  the part (0 shadows .. 4 the rest)
     */
    void lap( int part );
    double m_lapAt = 0.0;   ///< the last mark (s)

    const std::vector<Drawable> *m_coneList = nullptr;   ///< this frame's cones (cones(); drawn by fog())
    float                        m_coneGain = 0.0f;      ///< ... how strongly
    /** @brief Draws the cones into the colour target bound (fog()), the scene's depth read. */
    void drawCones();
    int m_boneLoc = -1;   ///< the skinned lit shader's bone matrices

    /** @brief Dynamic resolution: the share of the scene's target to draw, from the last frames' timers. */
    void pickScale();

    /** @brief A frame begins (shadows()): the GPU's timer and the processor's clock start. */
    void frameBegin();
    /** @brief The frame ends (post()): both stop. */
    void frameEnd();
    /**
     * @brief Reads the GPU timers that have finished.
     * @param[in] wait  true: waits for those still out
     */
    void collectTimers( bool wait );

    /** @brief A mesh's bounds in its own space. */
    struct Sphere
    {
        Vector4 m_sphere;   ///< middle, radius (radius < 0: unknown, never culled)
        Vector3 m_half;     ///< half the box's size
        int     m_count;    ///< the vertex count it was measured with
        Vector3 m_first;    ///< ... and its first vertex (another mesh in the same memory is told apart by it)
    };
    std::unordered_map<std::uint64_t, Sphere> m_spheres;   ///< by the meshes' vertex arrays
    Frustum                                   m_view;      ///< this frame's camera

    /** @brief What the sun's kept map was drawn for. */
    struct SunKey
    {
        int           m_x = 0, m_y = 0, m_z = 0;   ///< its middle on the grid (the light's axes)
        Vector3       m_dir = { 0, 0, 0 };         ///< the sun's direction
        std::uint64_t m_casters = 0;               ///< what does not move (a hash of meshes and places)
        int           m_size = 0;                  ///< the map's size
        bool          m_on = false;                ///< sun shadows at all
        bool operator==( const SunKey &o ) const;
    };

    Shader     m_lit, m_glassShader, m_sky, m_planet, m_particle, m_depth, m_prefilter, m_down, m_up, m_final;
    Shader     m_litSkin, m_depthSkin;           ///< the same for skinned figures
    Shader     m_prepassShader, m_prepassSkin;   ///< the depth pre-pass (with the cut-away's holes)
    Shader     m_fogShader, m_fogUp;             ///< the fog at half size, and laid over the picture
    CommonLocs m_litLocs, m_glassLocs, m_skyLocs, m_planetLocs, m_particleLocs, m_litSkinLocs, m_fogLocs;
    DrawLocs   m_litDraw, m_skinDraw;
    DepthLocs  m_depthLocs, m_depthSkinLocs, m_prepassLocs, m_prepassSkinLocs;
    Material   m_glassMat;
    Material   m_waterMat;
    Shader     m_waterShader;
    CommonLocs m_waterLocs;
    Material   m_pondMat;   ///< the ponds (their shader)
    Shader     m_pondShader;
    CommonLocs m_pondLocs;
    int        m_rippleLoc = -1, m_rippleCountLoc = -1;   ///< what wades in them
    Material   m_planetMat;

    // The interiors (lightMasks, environment, wetMap, cones).
    Shader               m_coneShader;                 ///< the lamps' cones in the haze
    Material             m_coneMat;                    ///< ... with it
    unsigned             m_maskTex = 0;                ///< the light masks' atlas (GL id; 0 none)
    unsigned             m_envTex = 0;                 ///< the level's cube map (GL id; 0 none)
    unsigned             m_wetTex = 0;                 ///< the wet map (GL id; 0 none)
    std::vector<Vector4> m_maskPlace;                  ///< per fixed light: its square in the atlas (LightMasks)
    std::vector<Vector4> m_maskReach;                  ///< per fixed light: the rectangle it reaches
    Vector4              m_wetInfo = { 0, 0, 0, 0 };   ///< the wet map: 1 / its width and depth (m), on

    /**
     * @param[in] p  a point light
     * @return its mask's place for the shaders (all zero: none)
     */
    Vector4 maskOf( const PointLight &p ) const;

    RenderTexture2D m_hdr = {};         ///< the scene: colour and a depth texture
    RenderTexture2D m_hdrColor = {};    ///< the same colour alone (the fog is laid over it from the depth)
    RenderTexture2D m_fogTarget = {};   ///< the fog at half the scene's size: in-scattered light, transmittance
    std::vector<RenderTexture2D> m_bloomLevels;
    RenderTexture2D              m_sunMap = {};
    RenderTexture2D              m_sunKept = {};   ///< the sun's map of what does not move
    RenderTexture2D              m_spotMap = {};
    int                          m_sunSize = 4096;
    int                          m_spotSize = 2048;
    int                          m_fogSteps = 40;     ///< the fog's steps along a ray
    int                          m_fogStep = 2;       ///< the fog pass's size: 2 half the scene's, 1 the scene's
    int                          m_shadowTaps = 12;   ///< a soft shadow's samples
    SunKey                       m_sunKey;            ///< what m_sunKept holds
    bool                         m_sunKeyValid = false;
    Matrix                       m_sunVP;
    Matrix                       m_spotVP;
    Vector2                      m_spotNF = { 0.1f, 32.0f };
    Lighting                     m_lights;
    float                        m_time = 0.0f;
    float                        m_frame = 0.0f;
    Camera3D                     m_cam;
    Matrix                       m_viewProj;    ///< this frame's camera (its frustum, the fog's rays)
    int                          m_viewW = 1;   ///< the scene's viewport this frame (dynamic resolution)
    int                          m_viewH = 1;
    int                          m_dynWait = 0;       ///< frames since the scale changed
    bool                         m_fogDone = false;   ///< fog() has run this frame
    std::vector<const void *>    m_prepassed;         ///< the lists in this frame's pre-pass
    /** @brief The point lights last set on a lit shader (so a draw with the same ones sets nothing). */
    struct LightSet
    {
        int           m_count = -1;
        unsigned char m_index[MaxDrawLights] = {};
    };
    LightSet m_lightSet[2];   ///< plain, skinned

    static constexpr int TimerRing = 4;              ///< frames a timer may be out before it is read
    unsigned int         m_timers[TimerRing] = {};   ///< GL query objects
    unsigned int  m_stampIds[TimerRing][6] = {};     ///< GL timestamp queries: the frame's start and its parts' ends
    unsigned char m_stampSet[TimerRing] = {};        ///< which of them this slot's frame wrote (bits)
    bool          m_timerOut[TimerRing] = {};        ///< begun and not read yet
    int           m_timerNext = 0;                   ///< the one used next
    bool          m_timing = false;                  ///< a frame's timer is running
    double        m_cpuStart = 0.0;                  ///< when this frame began (s)
    Stats         m_stats;                           ///< the sums since resetStats()
};

}   // namespace spaceopera
