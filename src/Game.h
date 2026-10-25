/**
 * @file Game.h
 * @brief A playable level: Corvin with the Gauss rifle (auto-aim as in Brotherhood of Steel, mouse aim on the PC),
 *        a camera turning about him, low gravity with jumps and recoil, rust lice and welder lice on a flow field, the drone press as boss, rescue
 *        capsules (and the hidden death counter), scrap and stims, spoken lines and hints, the HUD, the camera, and
 *        an autopilot that plays the level for tests.
 * @author Rene Weller
 */
#pragma once

#include "Audio.h"
#include "Creatures.h"
#include "Figure.h"
#include "Gfx.h"
#include "Level.h"

#include <raylib.h>

#include <set>
#include <string>
#include <vector>

namespace spaceopera
{

/** @brief What the player does this frame. */
struct GameInput
{
    Vector2 m_move = { 0.0f, 0.0f };    ///< wanted direction on screen (x right, y down), length up to 1
    bool    m_fire = false;             ///< trigger held
    bool    m_jump = false;             ///< jump pressed this frame
    bool    m_stim = false;             ///< stim pressed this frame
    bool    m_confirm = false;          ///< confirm pressed this frame (restart after the end)
    bool    m_mouseAim = false;         ///< the mouse aims (it moved or a button is held)
    bool    m_strafe = false;           ///< keep facing where Corvin looks (left trigger, Shift)
    bool    m_cameraReset = false;      ///< camera back behind the start's view (right stick click)
    float   m_cameraTurn = 0.0f;        ///< camera turn this frame (radians; right stick, right mouse button)
    Vector2 m_mouse = { 0.0f, 0.0f };   ///< mouse position (window pixels)
    Vector2 m_stick = { 0.0f, 0.0f };   ///< aim direction in the world (the autopilot's; players aim automatically)
};

/** @brief How hard the game is. */
enum class DifficultyE : int
{
    EASY,     ///< less damage, weaker enemies, health returns slowly
    NORMAL,   ///< as designed
    HARD,     ///< more damage, tougher enemies
    COUNT     ///< number of levels
};

/**
 * @brief The name of a difficulty (German, for the menus).
 * @param[in] d  the difficulty
 * @return its name
 */
const char *difficultyName( DifficultyE d );

/** @brief The level being played. */
class Game
{
public:
    /**
     * @brief Loads textures, the level, the models and the sound.
     * @param[in] gfx     the renderer
     * @param[in] assets  the assets folder
     * @param[in] level   the level file (in assets/levels)
     * @param[in] sound   false: silent
     * @return false if the level cannot be read
     */
    bool load( Gfx &gfx, const std::string &assets, const std::string &level, bool sound );

    /**
     * @brief Advances the game.
     * @param[in] dt  seconds
     * @param[in] in  the player's input
     */
    void update( float dt, const GameInput &in );

    /**
     * @brief Renders the frame and the HUD.
     * @param[in] gfx  the renderer
     * @param[in] out  the target (nullptr: the screen, between BeginDrawing and EndDrawing)
     */
    void render( Gfx &gfx, const RenderTexture2D *out );

    /** @brief Releases the sound. */
    void shutdown();

    /**
     * @brief Puts Corvin somewhere (tests and pictures).
     * @param[in] x    east (m)
     * @param[in] z    south (m)
     * @param[in] yaw  heading (degrees, 0 = south)
     */
    void putHero( float x, float z, float yaw );

    /** @brief Wakes every enemy (pictures). */
    void wakeAll();

    /** @return true once the exit is reached */
    bool finished() const { return m_finished; }

    /** @return a line about the run (autopilot) */
    std::string report() const;

    /**
     * @brief Sets the difficulty (enemies already there are scaled too).
     * @param[in] d  the difficulty
     */
    void setDifficulty( DifficultyE d );

    /** @return the HUD's font (the menus use it too) */
    const Font &font() const { return m_font; }

    /** @brief Takes the language just loaded (loadTexts): the signs' pictures and the font's characters. */
    void setLanguage();

    /** @return the act and the level's name in the loaded language (for the title menu) */
    std::string title() const;

    bool        m_autoplay = false;                   ///< the autopilot plays
    float       m_zoom = 1.0f;                        ///< camera distance factor
    float       m_time = 0.0f;                        ///< seconds played
    float       m_brightness = 1.0f;                  ///< exposure factor (the menus' brightness)
    float       m_camYaw = 0.0f;                      ///< camera turned about Corvin (radians, 0 = looking north)
    DifficultyE m_difficulty = DifficultyE::NORMAL;   ///< how hard

private:
    enum class FoeE : uint8_t
    {
        LOUSE,
        WELDER
    };
    enum class FoeStateE : uint8_t
    {
        CHASE,
        WINDUP,
        LEAP,
        RECOVER,
        SPIT
    };
    struct Foe
    {
        FoeE      m_kind = FoeE::LOUSE;
        Vector3   m_pos = { 0, 0, 0 };
        Vector3   m_vel = { 0, 0, 0 };
        float     m_yaw = 0.0f;
        float     m_hp = 1.0f;
        FoeStateE m_state = FoeStateE::CHASE;
        float     m_timer = 0.0f;
        bool      m_awake = false;
        bool      m_alive = true;
        bool      m_ground = true;
        float     m_flash = 0.0f;
        float     m_losTimer = 0.0f;
        bool      m_los = false;
        float     m_spit = 2.0f;
        float     m_crouch = 0.0f;
        float     m_torch = 0.0f;
        int       m_id = 0;
        LouseLegs m_legs;
    };
    struct Blob
    {
        Vector3 m_pos, m_vel;
        float   m_life;
    };
    struct Tracer
    {
        Vector3 m_from, m_to;
        float   m_life;
    };
    struct Particle
    {
        Vector3 m_pos, m_vel;
        float   m_life, m_max, m_size;
        Color   m_color;
        int     m_kind;   ///< 0 spark, 1 steam (lit), 2 flash, 3 ember
    };
    struct Debris
    {
        Vector3 m_pos, m_vel, m_rot, m_spin;
        float   m_life;
    };
    struct Flash
    {
        Vector3 m_pos, m_color;
        float   m_radius, m_life, m_max;
    };
    struct Pickup
    {
        int     m_kind;   ///< 0 scrap, 1 stim
        int     m_amount;
        Vector3 m_pos, m_vel;
        bool    m_alive = true;
        float   m_t = 0.0f;
    };
    struct Capsule
    {
        Vector3 m_pos, m_front;
        float   m_yaw;
        bool    m_active = false;
    };
    struct Drop
    {
        Vector3 m_centre, m_off;
        float   m_speed, m_phase, m_size;
    };
    struct Vent
    {
        Vector3 m_pos;
        float   m_timer, m_burst;
    };
    struct Target
    {
        int     m_kind = -1;   ///< -1 none, 0 foe, 1 tank 0, 2 tank 1, 3 core
        int     m_index = -1;
        Vector3 m_point = { 0, 0, 0 };
    };

    void loadSurfaces( Gfx &gfx, const std::string &assets );
    void loadFont();
    void spawnFoe( FoeE kind, Vector3 pos, bool awake );
    void updateHero( float dt, const GameInput &in );
    void updateFoes( float dt );
    void updatePress( float dt );
    void updateEffects( float dt );
    void shoot();
    Target findTarget( Vector3 from, Vector3 dir, float cone ) const;
    Vector3 targetPoint( const Target &t ) const;
    bool los( Vector3 a, Vector3 b ) const;
    void hurtHero( float amount, Vector3 push );
    void killFoe( Foe &f, bool drops );
    void hurtFoe( Foe &f, float amount, Vector3 dir );
    void sparks( Vector3 p, Vector3 n, int count, Color c, float speed );
    void explode( Vector3 p, float size );
    void flow( int r, int c );
    Vector3 flowDir( Vector3 from ) const;
    void fire( const std::string &trigger );
    void blockHero( Vector3 &p ) const;
    float groundAt( Vector3 p ) const;
    Lighting lighting() const;
    void drawHud( int w, int h );
    void drawParticles( Gfx &gfx );
    void autopilot( float dt, GameInput &in );
    void plan();
    float rnd();

    Level                 m_level;
    std::vector<Surface>  m_surfaces;
    std::vector<int>      m_signSurfaces;   ///< each sign's surface (its picture follows the language)
    std::vector<Drawable> m_static;
    std::vector<Drawable> m_dynamic;
    Figure                m_hero;
    LouseModel            m_louse;
    LouseModel            m_welder;
    PressModel            m_pressModel;
    Audio                 m_audio;
    Mesh                  m_chunk = {}, m_scrapMesh = {}, m_stimMesh = {}, m_stimCap = {};
    std::vector<Mesh>     m_capsuleMeshes;
    std::vector<int>      m_capsuleSurfaces;
    std::vector<Mesh>     m_doorFrame, m_doorPanel;
    std::vector<Mesh>     m_liftDeck, m_liftRim, m_liftBeacons;
    Vector3               m_lift = { 0, 0, 0 }, m_liftMin = { 0, 0, 0 }, m_liftMax = { 0, 0, 0 };
    float                 m_liftY = 0.0f, m_liftWait = 0.0f;
    bool                  m_liftGoing = false;
    float                 m_sinceHurt = 0.0f;
    float                 m_armourHint = 0.0f;   ///< shots wasted on the press's armour (they bring up a hint)
    Mesh                  m_anvilGlow = {};      ///< the red warning on the anvil
    Texture2D             m_dot = {};
    Font                  m_font = {};
    bool                  m_ownFont = false;
    std::string           m_assets;

    // Corvin.
    Vector3 m_pos = { 0, 0, 0 }, m_vel = { 0, 0, 0 };
    float   m_yaw = 0.0f, m_aimYaw = 0.0f, m_aimTimer = 0.0f;
    bool    m_ground = true;
    float   m_hp = 100.0f, m_heat = 0.0f, m_coolDelay = 0.0f, m_fireTimer = 0.0f, m_invuln = 0.0f, m_hurtFlash = 0.0f;
    bool    m_overheat = false, m_firing = false;
    int     m_stims = 1, m_scrap = 0, m_deaths = 0, m_kills = 0, m_shots = 0;
    bool    m_dead = false;
    float   m_deadTimer = 0.0f;
    Vector3 m_respawn = { 0, 0, 0 };
    Target  m_target;
    Vector3 m_aimDir = { 0, 0, 1 };

    // The world.
    std::vector<Foe>      m_foes;
    std::vector<Blob>     m_blobs;
    std::vector<Tracer>   m_tracers;
    std::vector<Particle> m_particles;
    std::vector<Debris>   m_debris;
    std::vector<Flash>    m_flashes;
    std::vector<Pickup>   m_pickups;
    std::vector<Capsule>  m_capsules;
    std::vector<Drop>     m_drops;
    std::vector<Vent>     m_vents;
    std::vector<int>      m_flowDist;
    std::set<int>         m_blocked;   ///< cells taken by capsules and the bodies (for paths)
    int                   m_flowR = -1, m_flowC = -1;
    float                 m_flowTimer = 0.0f;

    // The press.
    bool      m_pressActive = false, m_pressDead = false, m_pressDying = false;
    float     m_tank[2] = { 450.0f, 450.0f }, m_core = 700.0f;
    int       m_pressState = 0;   ///< 0 rest, 1 raise, 2 hold, 3 slam
    float     m_pressTimer = 2.0f, m_spawnTimer = 3.0f, m_deathTimer = 0.0f;
    int       m_spawns = 0, m_spawnHatch = -1;
    PressPose m_pressPose;
    Vector3   m_door = { 0, 0, 0 };
    float     m_doorOpen = 0.0f, m_doorHalf = 1.0f;
    bool      m_finished = false;

    // Story.
    std::set<std::string>     m_fired;
    std::vector<const Line *> m_lineQueue;
    std::vector<const Hint *> m_hintQueue;
    const Line               *m_line = nullptr;
    const Hint               *m_hint = nullptr;
    float                     m_lineTimer = 0.0f, m_hintTimer = 0.0f, m_titleTimer = 6.0f, m_endTimer = 0.0f;

    // Camera.
    Camera3D m_cam = {};
    Vector3  m_camTarget = { 0, 0, 0 };
    float    m_shake = 0.0f;
    float    m_camPull = 1.0f;   ///< extra distance in the arena

    // Autopilot.
    std::vector<int> m_path;
    float            m_planTimer = 0.0f, m_stuckTimer = 0.0f, m_runup = 0.0f;
    Vector3          m_lastPos = { 0, 0, 0 };
    unsigned         m_seed = 4242;
};

}   // namespace spaceopera
