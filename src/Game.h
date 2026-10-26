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
#include "Items.h"
#include "Level.h"
#include "Skills.h"
#include "Skin.h"

#include <raylib.h>
#include <raymath.h>

#include <map>
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
    bool    m_special = false;          ///< special pressed this frame (Dorra: throw a blast bolt; menus: back)
    bool    m_use = false;              ///< use pressed this frame (talk; A does it too where there is someone)
    bool    m_map = false;              ///< the map over the game toggled (M, Tab, the pad's up)
    bool    m_skill2 = false;           ///< the second skill (R, LB) pressed this frame
    bool    m_skill3 = false;           ///< the third skill (G, RB) pressed this frame
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

/** @brief What carries over from level to level, and into the save. */
struct Progress
{
    HeroE                 m_hero = HeroE::CORVIN;    ///< who is played
    std::string           m_level;                   ///< the level to play (next)
    int                   m_scrap = 0;               ///< scrap
    int                   m_stims = 1;               ///< stims
    int                   m_weapon = 0;              ///< the workshop's work on the weapon (0..3)
    int                   m_armor = 0;               ///< and on the armour (0..3)
    std::set<std::string> m_flags;                   ///< what has happened (job1, done.<level> ...)
    int                   m_xp = 0;                  ///< experience
    int                   m_heroLevel = 1;           ///< the hero's level
    int                   m_points = 1;              ///< skill points to spend
    std::vector<int>      m_ranks;                   ///< per skill (skillDefs()): its rank
    std::vector<Item>     m_bag;                     ///< what is carried
    Item                  m_gear[EquipSlots];        ///< what is worn: weapon, armour, helmet, gloves, boots, implants
    bool                  m_worn[EquipSlots] = {};   ///< whether a slot holds something
    int                   m_seed = 1;                ///< the random generator's state for items (and generated levels)
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

    /** @return the level that follows ("" none): the level's own, or where the hub's lift goes */
    const std::string &next() const { return m_exitTo.empty() ? m_level.m_next : m_exitTo; }

    /** @return what carries over now: scrap, stims, the workshop's work, what has happened (and this level done) */
    Progress progress() const;

    /** @return true if the end goes straight on (a hub's lift), without the summary */
    bool autoAdvance() const { return m_level.m_peaceful; }

    /** @return seconds since the level ended */
    float finishedFor() const { return m_endTimer; }

    /**
     * @brief Opens a conversation, or Ottla's (1) or Kiri's (2) menu (pictures).
     * @param[in] who   the person's name ("" none)
     * @param[in] menu  0 none, 1 stims, 2 workshop
     */
    void openForPicture( const std::string &who, int menu );

    /** @return a generated level's file ("" for a written one) */
    const std::string &generated() const { return m_level.m_generated; }

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

    /**
     * @brief Draws the inventory, the character page or the map over the frame (the game waits meanwhile).
     * @param[in] w    width (pixels)
     * @param[in] h    height (pixels)
     * @param[in] tab  0 inventory, 1 skills, 2 character, 3 map
     * @param[in] sel  the chosen row
     */
    void drawPanel( int w, int h, int tab, int sel );

    /**
     * @param[in] tab  the page
     * @return how many rows can be chosen on it
     */
    int panelRows( int tab ) const;

    /**
     * @brief Does what a page offers for its chosen row: put on or take off, salvage, learn a skill.
     * @param[in] tab     the page
     * @param[in] row     the chosen row
     * @param[in] action  0 the main one (A, Enter), 1 salvage (X, Delete)
     */
    void panelAction( int tab, int row, int action );

    /** @brief Pictures: a row of people in front of the hero, each in another animation. */
    void lineup();

    /**
     * @brief Tests and pictures: items scattered round the hero, experience given.
     * @param[in] items  how many items
     * @param[in] xp     how much experience
     */
    void testGifts( int items, int xp );

    bool        m_autoplay = false;                   ///< the autopilot plays
    float       m_zoom = 1.0f;                        ///< camera distance factor
    float       m_time = 0.0f;                        ///< seconds played
    float       m_brightness = 1.0f;                  ///< exposure factor (the menus' brightness)
    float       m_camYaw = 0.0f;                      ///< camera turned about the hero (radians, 0 = looking north)
    DifficultyE m_difficulty = DifficultyE::NORMAL;   ///< how hard
    HeroE       m_heroKind = HeroE::CORVIN;           ///< who is played (set before load)
    Progress    m_progress;                           ///< what the level starts with (set before load)
    unsigned    m_genSeed = 0;                        ///< a generated level's seed (0: from the progress and the level)

private:
    enum class FoeE : uint8_t
    {
        LOUSE,
        WELDER,
        BATON,     ///< a boarder with a shock baton
        GUNNER,    ///< a boarder with a carbine
        SHIELD,    ///< a market guard behind a riot shield, with a shock maul
        HEAVY,     ///< a heavy boarder with a rotary gun: long bursts, much armour
        DRONE,     ///< a hovering drone that keeps its distance and shoots
        SAW,       ///< a small saw bot: fast, in swarms, cuts on contact
        BOMBER,    ///< a crawler with a charge on its back: runs at the hero and goes off
        TAXATOR,   ///< the Taxator in his exoskeleton (the Knochenmarkt's boss)
        SHELL      ///< an emptied body a stolen core drives (a skinned figure that shambles and scratches)
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
        // What hits did to it.
        float m_burn = 0.0f;       ///< damage per second while burning
        float m_burnTime = 0.0f;   ///< seconds it still burns
        float m_stun = 0.0f;       ///< seconds it still stands stunned
        // Champions and the later kinds.
        int   m_elite = 0;      ///< 0 an ordinary one, 1 a champion, 2 a guardian (the level's end)
        int   m_affix = -1;     ///< a champion's gift: 0 swift, 1 armoured, 2 volatile, 3 berserk
        float m_hpMax = 1.0f;   ///< full health (its bar)
        float m_fuse = -1.0f;   ///< a bomber's fuse burning down (-1 not lit)
        float m_spin = 0.0f;    ///< a saw's blade, a drone's rotors
        int   m_stage = 0;      ///< the Taxator: 0 .. 2 as he weakens
        int   m_body = -1;      ///< a shell's figure (m_shellBodies)
        int m_variant = 0;   ///< a kind's other shape: the Taxator's kind as the Hydrauliker, a gunner with a shock gun
        Vector3 m_rush = { 0, 0, 0 };   ///< the Taxator's charge (its direction)
        Vector3 m_home = { 0, 0, 0 };   ///< where it was put (a boss keeps to its hall)
        Vector3 m_safe = { 0, 0, 0 };   ///< where it last stood in the open (never left in rock)
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
    /** @brief A plate of a beaten boarder's suit, flung off when its power cell bursts. */
    struct Piece
    {
        Mesh    m_mesh;
        int     m_surface;
        Vector4 m_emissive;
        Matrix  m_turn;   ///< how it lay when it came off
        Vector3 m_pos, m_vel, m_axis;
        float   m_angle, m_spin, m_life;
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
        float   m_damage = 0.0f;   ///< at the middle of the blast (0: a blast bolt's own)
        float   m_radius = 0.0f;   ///< the blast's reach (0: a blast bolt's own)
    };
    /** @brief A carbine's bolt in flight. */
    struct Shot
    {
        Vector3 m_pos, m_vel;
        float   m_life;
        float   m_hurt = 0.0f;   ///< what it does (0: a carbine's)
    };
    /** @brief A crate that slides when the ship dodges. */
    struct Cargo
    {
        Vector3 m_pos, m_vel;
        float   m_yaw, m_spin;
    };
    /** @brief Someone who does not fight: a refugee, Ilka, people in a hub's alleys. */
    struct Person
    {
        Vector3 m_pos;
        float   m_yaw;
        int     m_look;                        ///< which figure of m_folk (3 Ilka, 8 a child)
        float   m_sit;                         ///< sitting
        float   m_down;                        ///< lying
        bool    m_walks = false;               ///< strolls about
        Vector3 m_to = { 0.0f, 0.0f, 0.0f };   ///< where to
        float   m_wait = 0.0f;                 ///< standing about before strolling on
        Vector2 m_vel = { 0.0f, 0.0f };        ///< on the ground
        float   m_phase = 0.0f;                ///< walk cycle
        float   m_said = -100.0f;              ///< when they last said something (m_time)
        float   m_chair = 0.0f;                ///< on a stool
        float   m_seat = 0.5f;                 ///< the stool's height
        // Drawn as a skinned figure (Quaternius' bodies) when m_body >= 0.
        int                 m_body = -1;                      ///< 0 a man, 1 a woman
        int                 m_hair = -1;                      ///< which hair of that body (-1 none)
        int                 m_tone = 0;                       ///< 0 dark, 1 light skin
        int                 m_hairColour = 0;                 ///< black, brown, blond, red, grey
        Vector3             m_cloth = { 0.5f, 0.5f, 0.5f };   ///< the clothes' colour
        float               m_scale = 1.0f;                   ///< a child is smaller
        int                 m_force = -1;                     ///< a clip played regardless (pictures)
        SkinState           m_skin;
        std::vector<Matrix> m_bones;   ///< this frame's skin matrices (alive until drawn)
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
    bool gapBetween( Vector3 a, Vector3 b ) const;
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
    bool human( const Foe &f ) const
    {
        return f.m_kind == FoeE::BATON || f.m_kind == FoeE::GUNNER || f.m_kind == FoeE::SHIELD ||
               f.m_kind == FoeE::HEAVY || f.m_kind == FoeE::TAXATOR;
    }
    float foeRadius( const Foe &f ) const;
    float foeHeight( const Foe &f ) const;
    bool shielded( Vector3 from ) const;

    // GameCrew.cpp: Dorra, the boarders, their bolts and shots.
    void buildCrew( const std::string &assets );
    void updateMelee( float dt, const GameInput &in, Vector2 move );
    void strike( int swing );
    void shootPellet( bool first );
    /** @return the exit lift takes the hero: the press destroyed, else the level's gate event has happened */
    /** @return what a drum's spin does to something flying at velocity v (an acceleration, sideways) */
    Vector3 coriolis( Vector3 v ) const
    {
        float w = 2.0f * m_level.m_coriolis;
        return { -w * v.z, 0.0f, w * v.x };
    }
    bool liftOpen() const
    {
        return m_pressModel.m_origin.x != 0.0f ? m_pressDead : m_fired.count( m_level.m_gate ) > 0;
    }
    void throwBolt( float damage = 0.0f, float radius = 0.0f );
    void blast( Vector3 p, float damage = 0.0f, float radius = 0.0f );
    void updateBoarder( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    void updateShots( float dt );
    void poseCrew( std::vector<Drawable> &out );
    void burst( const Foe &f );
    void updatePieces( float dt );
    bool autopilotMelee( GameInput &in, const Target &best, float bestDist );
    void autoSkills( GameInput &in, const Target &best, float bestDist );

    // GameFoes.cpp: the later enemies (guards, heavies, drones, saws, bombers, the Taxator), champions and guardians,
    // their bars and names.
    void buildFoes();
    void makeElite( Foe &f, int elite );
    float foeTall( const Foe &f ) const;
    Figure &foeFigure( const Foe &f );
    void updateGuard( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    void updateHeavy( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    void updateTaxator( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    void updateCritter( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    void detonate( Foe &f, float damage, float radius );
    void fireShot( Vector3 muzzle, Vector3 at, float speed, float hurt, float spread );
    void bossDown( Foe &f );
    void poseFoes( std::vector<Drawable> &out );
    void drawFoeHud( int w, int h, float s );
    int boss() const;
    Figure     m_guardFigure, m_heavyFigure, m_taxatorFigure;   ///< market guards, heavies, the Taxator
    LouseModel m_saw, m_bomber;                                 ///< the saw bots, the bombers
    Mesh       m_droneBody = {}, m_droneRotor = {}, m_droneEye = {}, m_blade = {}, m_sac = {}, m_ring = {};
    float      m_hitScale = 1.0f;                      ///< what the foe acting now does more (champions)
    float      m_areaHit = 1.0f;                       ///< what enemies do more in a deeper area
    std::vector<std::pair<FoeE, Vector3>> m_summons;   ///< enemies called this frame (they come in after the update)

    // GameTug.cpp: the tug at the hull's edge (level 3's boss) and the boss parts one hits (its gun, its engines).
    struct BossPart
    {
        const char *m_key;      ///< its name in the texts
        Vector3     m_local;    ///< where on the tug (its frame)
        float       m_radius;   ///< how big a target
        float       m_hp, m_max;
        Vector3     m_pos;   ///< where it is this frame
    };
    struct Shell
    {
        Vector3 m_at;   ///< where it lands
        float   m_t;    ///< until it does
    };
    void loadTug();
    void updateTug( float dt );
    void poseTug( std::vector<Drawable> &out );
    void drawTugHud( int w, int h, float s );
    void hitPart( int i, float damage, Vector3 at );
    Matrix tugFrame() const;
    bool tugFighting() const { return m_tugOn && m_tugAwake && !m_tugDead; }
    bool                  m_tugOn = false, m_tugAwake = false, m_tugDead = false;
    Vector3               m_tugHome = { 0, 0, 0 }, m_tugPos = { 0, 0, 0 }, m_tugStand = { 0, 0, 0 };
    float                 m_tugYaw = 0.0f, m_tugT = 0.0f, m_tugGun = 0.0f, m_tugPd = 0.0f, m_tugDrone = 0.0f;
    float                 m_tugDying = 0.0f;
    int                   m_tugVolley = 0, m_tugStage = 0;
    std::vector<BossPart> m_parts;
    std::vector<Shell>    m_shells;
    std::vector<Mesh>     m_tugMeshes;
    std::vector<int>      m_tugSurfaces;
    Mesh                  m_tugGunMesh = {}, m_tugEngineMesh = {}, m_tugNozzle = {}, m_tugPodMesh = {};
    Vector3               m_podPos = { 0, 0, 0 }, m_podVel = { 0, 0, 0 };
    float                 m_podSpin = 0.0f;

    // GameVault.cpp: the Core Bank: racks of cores that break, shells, Mother Sevenfold and her relays.
    struct Rack
    {
        Vector3 m_pos;
        bool    m_alive;
        float   m_phase;   ///< its lights' own rhythm
    };
    void loadVault();
    void updateVault( float dt );
    void poseVault( std::vector<Drawable> &out );
    void drawVaultHud( int w, int h, float s );
    bool rackInWay( Vector3 a, Vector3 b ) const;
    void breakRack( int i );
    void updateShell( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    bool motherLinked() const;
    bool motherFighting() const { return m_motherOn && m_motherAwake && !m_motherDone; }
    /** @return boss parts can be hit now (the tug's, Mother Sevenfold's relays) */
    bool partsLive() const { return tugFighting() || motherFighting(); }
    std::vector<Rack>   m_racks;
    int                 m_racksBroken = 0;
    std::vector<Person> m_shellBodies;
    Mesh                m_rackMesh = {}, m_rackLights = {}, m_relayMesh = {}, m_relayTop = {}, m_coreGlow = {};
    bool                m_motherOn = false, m_motherAwake = false, m_motherDone = false;

    // GameWorld.cpp: what a place does around the fight (birds over a drum).
    struct Bird
    {
        Vector3 m_pos, m_vel;
        float   m_phase;
    };
    void loadWorld();
    void updateWorld( float dt );
    void poseWorld( std::vector<Drawable> &out );
    void drawWorldHud( int w, int h, float s );
    void drainHero( float amount );
    bool exposed( Vector3 p ) const;
    float m_oxygen = 100.0f, m_oxygenMax = 100.0f;   ///< the suit's air (seconds out in a vacuum)
    /** @brief A ball of water floating in low gravity (one can dive through it; a current turns in it). */
    struct Water
    {
        Vector3 m_c;             ///< its middle
        float   m_r;             ///< its radius
        float   m_turn;          ///< which way its current turns (+1, -1)
        float   m_grow = 1.0f;   ///< a new one swells into being
    };
    std::vector<Water> m_waters;
    Mesh               m_waterMesh = {};
    bool               m_inWater = false;   ///< the hero's chest is under water
    int waterAt( Vector3 p ) const;
    void drawWater( Gfx &gfx );
    void updateShocker( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    void updateHydraulic( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw );
    Figure               m_hydraFigure;
    std::vector<Vector3> m_airPosts;   ///< posts that refill the suit's air near them
    Mesh                 m_postMesh = {}, m_postRing = {};
    bool                 m_seekAir = false;                      ///< the autopilot goes for air
    const char          *m_hurtBy = "";                          ///< what hurt the hero last (the autopilot's log)
    bool                 m_outside = false;                      ///< the hero is out in the open (a vacuum)
    int                  m_flareState = 0;                       ///< the star: 0 calm, 1 warning, 2 a storm
    float                m_flareTimer = 0.0f, m_flareT = 0.0f;   ///< until the next warning; time in the state
    std::vector<Bird>    m_birds;
    Vector3              m_flockAt = { 0, 0, 0 };
    Mesh                 m_birdBody = {}, m_birdWing = {};

    // GameLoot.cpp: the hero's stats from gear, level and skills; hits with crits, burning, stuns and leech; items on
    // the floor and picking them up; experience and levels; the active skills.
    /** @brief The hero's numbers as gear, level, skills and the workshop make them. */
    struct HeroStats
    {
        float m_add[(int)StatE::COUNT] = {};   ///< the sums of affixes, the kinds' bonuses and the skills
        float m_health = 100.0f;               ///< maximum health
        float m_armor = 0.0f;                  ///< armour
        float m_damage = 20.0f;                ///< per hit (a pellet, a blow)
        float m_interval = 0.16f;              ///< Corvin: seconds between shots; Dorra: how long a blow takes
        float m_heat = 7.5f;                   ///< heat per shot
        float m_spread = 0.012f, m_splash = 0.0f, m_range = 30.0f;
        int   m_pellets = 1, m_pierce = 0, m_chain = 0;
    };
    /** @brief An item on the floor. */
    struct Loot
    {
        Item    m_item;
        Vector3 m_pos, m_vel;
        float   m_t = 0.0f;
        bool    m_alive = true;
    };
    void refreshStats();
    float stat( StatE s ) const { return m_stats.m_add[(int)s]; }
    float armorCut() const;
    int areaLevel() const { return m_level.m_area; }
    void dealHit( Foe &f, float damage, Vector3 dir, Vector3 at );
    void gainXp( int xp );
    int xpFor( const Foe &f ) const;
    static int xpNeed( int level );
    void dropItem( Vector3 at, float luck, int slot = -1 );
    void updateLoot( float dt );
    void poseLoot( std::vector<Drawable> &out );
    void useSkills( float dt, const GameInput &in );
    void quake( int r );
    void drawLootHud( int w, int h, float s );
    void equip( int bagIndex );
    void unequip( int slot );
    void salvage( int bagIndex );
    int rank( int skill ) const;
    HeroStats         m_stats;
    std::vector<Loot> m_loot;
    Mesh              m_lootMesh[(int)SlotE::COUNT] = {};
    Mesh              m_beamMesh = {};
    float             m_cool[4] = {};        ///< the active skills' cooldowns, by button
    float             m_overcharge = 0.0f;   ///< Corvin's overcharge still running
    float             m_dash = 0.0f;         ///< a roll or a charge still running
    Vector3           m_dashDir = { 0, 0, 1 };
    std::set<int>     m_dashHit;        ///< who the charge has struck already
    int               m_dashKind = 0;   ///< 0 a roll, 1 a charge
    float             m_dashDamage = 0.0f;
    float             m_levelUp = 0.0f;   ///< the level-up banner still showing
    Color             m_toastColor = { 240, 235, 225, 255 };
    float             m_bagFull = 0.0f;
    float             m_regenStore = 0.0f;

    // GamePanel.cpp: the map (what the hero has seen), the inventory, the character page.
    struct PanelRow
    {
        std::string m_title, m_value, m_detail;
        bool        m_header = false;
        int         m_slot = -1;                        ///< a worn item's slot
        int         m_bag = -1;                         ///< an item in the bag
        int         m_skill = -1;                       ///< a skill
        Color       m_color = { 210, 205, 195, 230 };   ///< the value's colour
    };
    std::vector<PanelRow> skillRows() const;
    std::string itemDetail( const Item &it ) const;
    void loadPanel();
    void updateMap( float dt );
    void drawMap( Rectangle area, float alpha, float s );
    std::vector<PanelRow> inventory() const;
    std::vector<uint8_t> m_seen;   ///< cells the hero has seen
    float                m_seenTimer = 0.0f;
    bool                 m_showMap = false;   ///< the map over the game (M, Tab, the pad's up)
    int                  m_mapR0 = 0, m_mapR1 = 0, m_mapC0 = 0, m_mapC1 = 0;   ///< the map's open part

    // GameUse.cpp: what the hero works with A / E: doors (some locked until something has happened), lockers to
    // search, terminals to read.
    void loadUsables();
    bool updateUse( float dt, GameInput &in );
    void poseUsables( std::vector<Drawable> &out );
    void drawUseHud( int w, int h, float s );

    // GameHub.cpp: people to talk to, the conversations, Ottla's stims, Kiri's workshop, the harvest board, the drums
    // turning past beneath the floor, the lift out.
    void loadHub( Gfx &gfx, const std::string &assets );
    void renderBoard();
    bool updateHub( float dt, GameInput &in );
    int nearNpc() const;
    void openTalk( int npc );
    void enterNode( const TalkNode *node );
    void runEvent( const std::string &event );
    void buy( int item );
    void updateCrowd( float dt );
    Vector3 strollTo( const Person &q );
    void poseHub( std::vector<Drawable> &out );
    void drawHubHud( int w, int h, float s );
    /** @return where the hub's lift may go now (the flag set, not done yet unless it may be done again) */
    std::vector<std::string> openExits( bool &promised ) const;
    float weaponBoost() const { return 1.0f + 0.2f * m_progress.m_weapon; }

    // GameShip.cpp: the dodges, the cargo, the people, the view, the last scene.
    void loadShip( Gfx &gfx, const std::string &assets );
    void updateThrust( float dt );
    Vector3 thrust() const { return Vector3Scale( m_thrustDir, m_thrustAmt ); }
    void updateCargo( float dt );
    void updateIlka( float dt );
    void updateScene( float dt, GameInput &in );
    void drawView( Gfx &gfx );
    void drawPlumes( Gfx &gfx );
    void poseShip( std::vector<Drawable> &out );

    // GameSkin.cpp: people as skinned figures with real animations (Quaternius' bodies and animation libraries).
    void loadSkins( Gfx &gfx );
    void dressPerson( Person &q, unsigned seed );
    void updateSkins( float dt );
    void poseSkin( Person &q, std::vector<Drawable> &out );
    bool                       m_skins = false;       ///< the bodies are there
    SkinModel                  m_body[2];             ///< a man, a woman
    std::vector<int>           m_bodySurface[2][2];   ///< per mesh of each body: its surface, per skin tone
    std::vector<int>           m_hairSurface[2][5];   ///< per hair of each body, per colour
    std::map<std::string, int> m_clip;                ///< the clips by name (the same in both bodies)
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
    std::vector<Piece>    m_pieces;
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
    static constexpr int  FolkCount = 9;
    Figure                m_folk[FolkCount];   ///< worn clothes 0-2, Ilka, Ysolde, Kiri, more clothes 6-7, a child
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
    int               m_ilka = -1;      ///< Ilka among the people (in her nook)
    int               m_ilkaRun = -1;   ///< Ilka at the start, who runs back to her container
    size_t            m_ilkaStep = 0;   ///< the point of her way she runs to
    bool              m_ilkaGone = false;
    Vector3           m_scanner = { 0, 0, 0 };
    int               m_scene = 0;   ///< the last scene: 0 none, 1 Ilka, 2 the shot, 3 dark, 4 the clinic
    float             m_sceneTimer = 0.0f, m_sceneDown = 0.0f, m_patrolMark = 0.0f;
    Vector3           m_shooter = { 0, 0, 0 };

    // The hub.
    struct NpcState
    {
        int     m_def;      ///< its line in the level
        Vector3 m_pos;      ///< where it stands
        float   m_yaw;      ///< where it looks
        int     m_look;     ///< which figure (-1: the terminal)
        Person  m_figure;   ///< drawn as a skinned figure when its m_body >= 0
    };
    std::vector<NpcState>            m_npcs;
    const TalkNode                  *m_talk = nullptr;   ///< the conversation's step on screen
    int                              m_talkNpc = -1, m_choice = 0;
    int                              m_menu = 0, m_menuChoice = 0;   ///< 1 Ottla's stims, 2 Kiri's workshop, 3 the lift
    float                            m_navY = 0.0f;                  ///< the stick's last up and down (menus)
    std::map<std::string, Texture2D> m_portraits;
    Texture2D                        m_boardTex = {};
    int                              m_boardSurface = -1;
    std::vector<Drawable>            m_drums;   ///< the cavern and the other drums (turned each frame)
    std::vector<Vector3>             m_cavernLights;
    Mesh                             m_terminalMesh = {}, m_screenMesh = {}, m_padMesh = {};
    Vector3                          m_axis = { 0, 0, 0 };   ///< a point on our drum's axis (it runs east-west)
    Vector2                          m_rawMove = { 0, 0 };   ///< the stick before the camera's turn (menus)
    Vector3                          m_exit = { 0, 0, 0 };
    bool                             m_hasExit = false;
    bool                             m_padArmed = true;   ///< the lift's plate reacts (again once one has stepped off)
    std::vector<std::string>         m_liftChoices;       ///< where the lift may go (its menu)
    std::string                      m_exitTo, m_levelFile;
    float                            m_spin = 0.0f;                 ///< how far the drum has turned (radians)
    int                              m_barks = 0, m_barkNext = 0;   ///< overheard lines ("bark.1".."bark.n"), next one
    int                              m_barkWho = -1;                ///< who says one now (in m_people)
    std::string                      m_barkKey;
    float                            m_barkShow = 0.0f, m_barkPause = 3.0f;

    /** @brief Something to work with A / E. */
    struct Usable
    {
        int         m_kind = 0;                     ///< 0 a door, 1 a locker, 2 a terminal
        int         m_row = 0, m_col = 0;           ///< its cell
        Vector3     m_pos = { 0.0f, 0.0f, 0.0f };   ///< a door's middle; where one stands before a locker or terminal
        Vector3     m_at = { 0.0f, 0.0f, 0.0f };    ///< a locker or a terminal, against the wall
        Vector3     m_out = { 0.0f, 0.0f, 1.0f };   ///< out of the wall
        bool        m_alongX = false;               ///< a door's leaves run along x
        bool        m_used = false;                 ///< opened, searched
        float       m_open = 0.0f;                  ///< 0 shut .. 1 open
        std::string m_key;                          ///< a door: the event that unlocks it; a terminal: its text
    };
    std::vector<Usable> m_usables;
    int                 m_near = -1;   ///< the one within reach
    std::string         m_reading;     ///< a terminal's text on screen
    std::string         m_toast;       ///< a short word (what a locker held, "locked")
    float               m_toastTimer = 0.0f;
    Mesh                m_leafMesh = {}, m_leafBand = {}, m_lockerMesh = {}, m_lockerDoor = {}, m_doorLamp = {};

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
    static constexpr float    IntroTime = 14.0f;   ///< the opening card, unless read sooner
    float                     m_intro = 0.0f;      ///< how long the opening card still shows

    // Camera.
    Camera3D m_cam = {};
    Vector3  m_camTarget = { 0, 0, 0 };
    float    m_shake = 0.0f;
    float    m_camPull = 1.0f;   ///< extra distance in the arena

    // Autopilot.
    std::vector<int>  m_path;
    float             m_planTimer = 0.0f, m_stuckTimer = 0.0f, m_runup = 0.0f;
    float             m_unstick = 0.0f;   ///< the autopilot frees itself: seconds left
    Vector2           m_unstickDir = { 0, 0 };
    std::vector<char> m_propCells;   ///< cells a crate or a stall stands in (the autopilot goes round)
    Vector3           m_lastPos = { 0, 0, 0 };
    size_t            m_waypoint = 0;   ///< the next of the level's route
    unsigned          m_seed = 4242;
};

}   // namespace spaceopera
