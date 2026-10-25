/**
 * @file Game.h
 * @brief A playable level: Corvin with the Gauss rifle (auto-aim as in Brotherhood of Steel, mouse aim on the PC) or
 *        Dorra with ram hammer, shield and blast bolts; a camera turning about the hero; gravity from spin or thrust,
 *        with jumps and recoil, and on a ship the tilts of its dodges; rust lice, welder lice and boarders; the drone
 *        press as boss; rescue capsules (and the hidden death counter); refugees, Ilka, the view through a window in
 *        the floor; scrap and stims; spoken lines, hints and scenes; the HUD; and an autopilot that plays the level
 *        for tests.
 *
 *        Game.cpp holds the level and Corvin, GameCrew.cpp Dorra and the boarders, GameShip.cpp the ship (dodges,
 *        loose cargo, the people, the view, the last scene).
 * @author Rene Weller
 */
#pragma once

#include "Audio.h"
#include "Creatures.h"
#include "Figure.h"
#include "Gfx.h"
#include "Level.h"

#include <raylib.h>
#include <raymath.h>

#include <set>
#include <string>
#include <vector>

namespace spaceopera
{

/** @brief What the player does this frame. */
struct GameInput
{
    Vector2 m_move = { 0.0f, 0.0f };    ///< wanted direction on screen (x right, y down), length up to 1
    bool    m_fire = false;             ///< trigger held (Dorra: strike)
    bool    m_jump = false;             ///< jump pressed this frame
    bool    m_stim = false;             ///< stim pressed this frame
    bool    m_special = false;          ///< special pressed this frame (Dorra: throw a blast bolt)
    bool    m_confirm = false;          ///< confirm pressed this frame (restart after the end)
    bool    m_mouseAim = false;         ///< the mouse aims (it moved or a button is held)
    bool    m_strafe = false;           ///< keep facing where the hero looks (left trigger, Shift; Dorra: shield up)
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

/** @brief Who is played. */
enum class HeroE : int
{
    CORVIN,   ///< ranged: the Gauss rifle
    DORRA,    ///< melee: ram hammer, shield, blast bolts
    IONE,     ///< drones (not playable yet)
    COUNT     ///< number of heroes
};

/**
 * @brief The name of a difficulty (for the menus).
 * @param[in] d  the difficulty
 * @return its name in the loaded language
 */
const char *difficultyName( DifficultyE d );

/**
 * @brief A hero's key in the texts (corvin, dorra, ione).
 * @param[in] h  the hero
 * @return the key
 */
const char *heroKey( HeroE h );

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
     * @brief Puts the hero somewhere (tests and pictures).
     * @param[in] x    east (m)
     * @param[in] z    south (m)
     * @param[in] yaw  heading (degrees, 0 = south)
     */
    void putHero( float x, float z, float yaw );

    /** @brief Wakes every enemy (pictures). */
    void wakeAll();

    /** @return true once the level is over */
    bool finished() const { return m_finished; }

    /** @return the level that follows ("" none) */
    const std::string &next() const { return m_level.m_next; }

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
    float       m_camYaw = 0.0f;                      ///< camera turned about the hero (radians, 0 = looking north)
    DifficultyE m_difficulty = DifficultyE::NORMAL;   ///< how hard
    HeroE       m_heroKind = HeroE::CORVIN;           ///< who is played (set before load)

private:
    enum class FoeE : uint8_t
    {
        LOUSE,
        WELDER,
        BATON,   ///< a boarder with a shock baton
        GUNNER   ///< a boarder with a carbine
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
        // Boarders.
        float m_phase = 0.0f;     ///< walk cycle
        float m_swingT = -1.0f;   ///< the baton's blow (-1 none)
        float m_aim = 0.0f;       ///< the carbine's aim (its laser shows it)
        int   m_burst = 0;        ///< shots left in the burst
        float m_down = 0.0f;      ///< fallen (the body stays)
        int   m_group = 0;        ///< 1 the bridge's, 2 coming aboard later
        bool  m_hidden = false;   ///< not aboard yet
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
    /** @brief Dorra's blast bolt in flight. */
    struct Bolt
    {
        Vector3 m_pos, m_vel;
        float   m_life;
    };
    /** @brief A carbine's bolt in flight. */
    struct Shot
    {
        Vector3 m_pos, m_vel;
        float   m_life;
    };
    /** @brief A crate that slides when the ship dodges. */
    struct Cargo
    {
        Vector3 m_pos, m_vel;
        float   m_yaw, m_spin;
    };
    /** @brief Someone who does not fight: a refugee, Ilka. */
    struct Person
    {
        Vector3 m_pos;
        float   m_yaw;
        int     m_look;   ///< which clothes (0..2), 3 Ilka
        float   m_sit;    ///< sitting
        float   m_down;   ///< lying
    };
    /** @brief A ship braking far below (the view through the window). */
    struct Ship
    {
        Vector3 m_pos;
        float   m_size;
    };

    void loadSurfaces( Gfx &gfx, const std::string &assets );
    void loadFont();
    void spawnFoe( FoeE kind, Vector3 pos, bool awake );
    void updateHero( float dt, const GameInput &in );
    void updateGunner( float dt, const GameInput &in, Vector2 move );
    void moveHero( float dt );
    void updateFoes( float dt );
    void updatePress( float dt );
    void updateEffects( float dt );
    void shoot();
    Target findTarget( Vector3 from, Vector3 dir, float cone ) const;
    Vector3 targetPoint( const Target &t ) const;
    bool los( Vector3 a, Vector3 b ) const;
    void hurtHero( float amount, Vector3 push, Vector3 from );
    void killFoe( Foe &f, bool drops );
    void hurtFoe( Foe &f, float amount, Vector3 dir );
    void hitTank( int side, float amount, Vector3 at, Vector3 n );
    void hitCore( float amount, Vector3 at, Vector3 n );
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
    float hpMax() const;
    float runSpeed() const;
    bool human( const Foe &f ) const { return f.m_kind == FoeE::BATON || f.m_kind == FoeE::GUNNER; }
    float foeRadius( const Foe &f ) const;
    float foeHeight( const Foe &f ) const;
    bool shielded( Vector3 from ) const;

    // GameCrew.cpp: Dorra, the boarders, their bolts and shots.
    void buildCrew( const std::string &assets );
    void updateMelee( float dt, const GameInput &in, Vector2 move );
    void strike( int swing );
    void throwBolt();
    void blast( Vector3 p );
    void updateBoarder( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    void updateShots( float dt );
    void poseCrew( std::vector<Drawable> &out );
    bool autopilotMelee( GameInput &in, const Target &best, float bestDist );

    // GameShip.cpp: the dodges, the cargo, the people, the view, the last scene.
    void loadShip( Gfx &gfx, const std::string &assets );
    void updateThrust( float dt );
    Vector3 thrust() const { return Vector3Scale( m_thrustDir, m_thrustAmt ); }
    void updateCargo( float dt );
    void updateScene( float dt, GameInput &in );
    void drawView( Gfx &gfx );
    void drawPlumes( Gfx &gfx );
    void poseShip( std::vector<Drawable> &out );
    void drawShipHud( int w, int h, float s );

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
    Texture2D             m_streak = {};   ///< soft across, even along (beams, plumes)
    Font                  m_font = {};
    bool                  m_ownFont = false;
    std::string           m_assets;

    // The hero.
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
    // Dorra.
    int   m_swing = -1;          ///< the blow under way (0 forehand, 1 backhand, 2 overhead)
    float m_swingT = 0.0f;       ///< how far through it
    bool  m_comboNext = false;   ///< the next blow follows
    float m_block = 0.0f;        ///< the shield raised
    float m_boltTimer = 0.0f;    ///< until the next blast bolt
    bool  m_fireWas = false;     ///< the trigger was held last frame

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
    std::vector<Bolt>     m_bolts;
    std::vector<Shot>     m_shotsInFlight;
    std::vector<Cargo>    m_cargo;
    std::vector<Person>   m_people;
    std::vector<int>      m_flowDist;
    std::set<int>         m_blocked;   ///< cells taken by capsules, people and the bodies (for paths)
    int                   m_flowR = -1, m_flowC = -1;
    float                 m_flowTimer = 0.0f;
    Figure                m_boarderBaton, m_boarderGun;   ///< the boarders (one figure draws them all)
    Figure                m_folk[4];                      ///< refugees in three kinds of clothes, and Ilka
    Mesh                  m_cargoMesh = {}, m_cargoBand = {}, m_scannerMesh = {};

    // The ship.
    int               m_thrustState = 0;   ///< 0 calm, 1 warning, 2 pushing
    float             m_thrustTimer = 8.0f, m_thrustAmt = 0.0f, m_roll = 0.0f;
    Vector3           m_thrustDir = { 1, 0, 0 };
    int               m_dodges = 0;
    Texture2D         m_skyTex = {}, m_starTex = {}, m_cardTex = {};
    Mesh              m_sphere = {}, m_glassMesh = {};
    std::vector<Ship> m_ships;
    Vector3           m_star = { 0, 0, 0 }, m_ownPlume = { 0, 0, 0 }, m_patrol = { 0, 0, 0 };
    float             m_starRadius = 0.0f;
    int               m_ilka = -1;   ///< Ilka among the people
    Vector3           m_scanner = { 0, 0, 0 };
    int               m_scene = 0;   ///< the last scene: 0 none, 1 Ilka, 2 the shot, 3 dark, 4 the clinic
    float             m_sceneTimer = 0.0f, m_sceneDown = 0.0f, m_patrolMark = 0.0f;
    Vector3           m_shooter = { 0, 0, 0 };

    // The press.
    bool      m_pressActive = false, m_pressDead = false, m_pressDying = false;
    float     m_tank[2] = { 450.0f, 450.0f }, m_core = 700.0f;
    int       m_pressState = 0;   ///< 0 rest, 1 raise, 2 hold, 3 slam
    float     m_pressTimer = 2.0f, m_spawnTimer = 3.0f, m_deathTimer = 0.0f;
    int       m_spawns = 0, m_spawnHatch = -1;
    PressPose m_pressPose;
    Vector3   m_door = { 0, 0, 0 };
    float     m_doorOpen = 0.0f, m_doorHalf = 0.0f;
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
    size_t           m_waypoint = 0;   ///< the next of the level's route
    unsigned         m_seed = 4242;
};

}   // namespace spaceopera
