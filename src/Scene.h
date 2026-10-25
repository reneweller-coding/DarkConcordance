/**
 * @file Scene.h
 * @brief The look test's room: an observation deck of an abandoned station with a glass floor over a gas giant, sun
 *        shafts through the west windows, a reactor, cryo pods, consoles, crates, a wrecked robot and an alien monolith
 *        on a platform. The hero walks and shoots plasma bolts; sparks rain from a broken conduit, dust glitters in the
 *        light.
 * @author Rene Weller
 */
#pragma once

#include "Figure.h"
#include "Gfx.h"
#include "MeshBuilder.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace spaceopera
{

/** @brief What the player does this frame. */
struct Controls
{
    Vector2 m_move = { 0.0f, 0.0f };   ///< wanted direction on the ground (length up to 1; x east, y south)
    bool    m_fire = false;            ///< the trigger is held
};

/** @brief The surfaces of the scene. */
enum class SurfaceE : int
{
    FLOOR,      ///< deck plating
    WALL,       ///< bulkhead panels
    METAL,      ///< brushed steel
    HAZARD,     ///< hazard stripes
    PIPES,      ///< a band of pipes
    CRATE,      ///< orange paint
    PAINT,      ///< the robot's yellow paint
    ARMOR,      ///< the hardsuit's plates
    DARK,       ///< the hardsuit's dark plates
    OBSIDIAN,   ///< the monolith
    SCREEN,     ///< console screens
    GLOW,       ///< lamps (their glow is per drawable)
    COUNT       ///< number of surfaces
};

/** @brief The room and everything in it. */
class Scene
{
public:
    /**
     * @brief Loads the textures and builds the room.
     * @param[in] gfx     the renderer (makes the surfaces)
     * @param[in] assets  the assets folder
     */
    void load( Gfx &gfx, const std::string &assets );

    /**
     * @brief Advances the world.
     * @param[in] dt  seconds
     * @param[in] in  the player's controls
     */
    void update( float dt, const Controls &in );

    /**
     * @brief Renders a frame.
     * @param[in] gfx  the renderer
     * @param[in] out  where the picture goes (nullptr: the screen, between BeginDrawing and EndDrawing)
     */
    void render( Gfx &gfx, const RenderTexture2D *out );

    /**
     * @brief Puts the hero somewhere (and the camera behind it).
     * @param[in] x    east (m)
     * @param[in] z    south (m)
     * @param[in] yaw  heading (degrees, 0 = south, towards the camera)
     */
    void putHero( float x, float z, float yaw );

    /** @brief Fires one bolt at once (for pictures). */
    void fire();

    Figure m_hero;          ///< the player
    float  m_time = 0.0f;   ///< seconds since start
    float  m_zoom = 1.0f;   ///< camera distance factor (mouse wheel, shoulder buttons)

private:
    /** @brief A short-lived glowing thing. */
    struct Particle
    {
        Vector3 m_pos;       ///< where
        Vector3 m_vel;       ///< how fast
        float   m_life;      ///< seconds left
        float   m_maxLife;   ///< seconds it had
        float   m_size;      ///< size (m)
        Color   m_color;     ///< colour
        int     m_kind;      ///< 0 spark, 1 mote, 2 flash
    };

    /** @brief A plasma bolt. */
    struct Bolt
    {
        Vector3 m_pos;    ///< where
        Vector3 m_vel;    ///< how fast
        float   m_life;   ///< seconds left
    };

    /** @brief A short flash of light (muzzle, impact, sparks). */
    struct Flash
    {
        Vector3 m_pos;      ///< where
        Vector3 m_color;    ///< colour times intensity
        float   m_radius;   ///< reach
        float   m_life;     ///< seconds left
        float   m_max;      ///< seconds it had
    };

    /** @brief Something the hero cannot walk through. */
    struct Blocker
    {
        Vector2 m_centre;   ///< on the ground
        float   m_radius;   ///< size
    };

    void buildRoom();
    void buildProps();
    void addStatic( MeshBuilder &b, SurfaceE s, bool visible = true, Vector4 glow = { -1.0f, 0.0f, 0.0f, 0.0f } );
    float groundAt( float x, float z ) const;
    void moveHero( float dt, const Controls &in );
    void sparkBurst();
    void impact( Vector3 p, Vector3 normal );
    bool boltHits( Vector3 p, Vector3 &normal ) const;
    Lighting lighting() const;
    void drawParticles( Gfx &gfx );
    void updateCamera( float dt, bool snap );
    float rnd();

    std::vector<Surface>   m_surfaces;
    std::vector<Texture2D> m_textures;
    std::vector<Drawable>  m_static;
    std::vector<Drawable>  m_dynamic;
    std::vector<int>       m_beacons;     ///< indices into m_static of the red lamps
    std::vector<int>       m_cryoGlass;   ///< indices into m_static of the cryo windows
    std::vector<Mesh>      m_monolith;
    std::vector<Mesh>      m_shard;
    Mesh                   m_glass = {};
    Mesh                   m_sphere = {};
    Texture2D              m_nebula = {};
    Texture2D              m_gasGiant = {};
    Texture2D              m_dot = {};
    std::vector<Particle>  m_particles;
    std::vector<Particle>  m_dust;
    std::vector<Bolt>      m_bolts;
    std::vector<Flash>     m_flashes;
    std::vector<Blocker>   m_blockers;
    Camera3D               m_cam = {};
    Vector3                m_camTarget = { 0.0f, 0.0f, 0.0f };
    float                  m_cooldown = 0.0f;
    float                  m_sparkTimer = 2.0f;
    float                  m_sparkLight = 0.0f;
    unsigned int           m_seed = 12345;
};

}   // namespace spaceopera
