/**
 * @file Figure.h
 * @brief A person built from rounded boxes: Corvin with his rifle, Dorra with hammer and shield, the boarders, and
 *        people without a suit (Ilka, the refugees) who sit or lie. A lamp on the right shoulder of a suit lights the
 *        way. Given a skinned body, the plates ride on its bones and the clips of the animation libraries move it
 *        (walking into a jog, the rifle aimed, the blows, the shield, jumps, death); else it moves procedurally (walk
 *        cycle, arms with two-bone IK, lean, sway).
 * @author Rene Weller
 */
#pragma once

#include "Gfx.h"
#include "Skin.h"

#include <raylib.h>

#include <functional>
#include <vector>

namespace spaceopera
{

/** @brief The bones of the figure. */
enum class BoneE : int
{
    PELVIS,    ///< hips (root of the legs)
    SPINE,     ///< chest and backpack
    HEAD,      ///< helmet or hood
    WEAPON,    ///< the rifle, the hammer or the baton (in the right hand, independent of the chest's twist)
    UPPER_L,   ///< left upper arm
    FORE_L,    ///< left forearm and hand (the shield)
    UPPER_R,   ///< right upper arm
    FORE_R,    ///< right forearm and hand
    THIGH_L,   ///< left thigh
    SHIN_L,    ///< left shin
    FOOT_L,    ///< left boot
    THIGH_R,   ///< right thigh
    SHIN_R,    ///< right shin
    FOOT_R,    ///< right boot
    COUNT      ///< number of bones
};

/** @brief What the figure carries. */
enum class WeaponE : int
{
    RIFLE,    ///< held in both hands (Corvin's Gauss rifle, the boarders' carbines)
    HAMMER,   ///< a ram hammer in the right hand, a shield on the left forearm (Dorra)
    BATON,    ///< a shock baton in the right hand (boarders)
    NONE      ///< empty hands
};

/** @brief Which surfaces the figure uses (indices into the scene's list) and how it is made. */
struct FigureSurfaces
{
    int     m_armor;                                 ///< the light armour plates (or the clothes)
    int     m_dark;                                  ///< dark plates
    int     m_metal;                                 ///< joints, belts, the weapon's metal
    int     m_glow;                                  ///< glowing parts (visor, lamp, status lights)
    Vector4 m_visor = { 0.35f, 2.4f, 3.2f, 0.0f };   ///< the visor's glow
    WeaponE m_weapon = WeaponE::RIFLE;               ///< what it carries
    bool    m_suit = true;                           ///< a hardsuit (false: clothes and a hood, no lamp)
    bool    m_helmet = true;                         ///< the suit's helmet on (false: the bare head)
    int     m_skin = -1;                             ///< the face (bare-headed)
    int     m_hair = -1;                             ///< the hair (bare-headed)
    Vector3 m_scale = { 1.0f, 1.0f, 1.0f };          ///< width, height, depth (Dorra is short and broad)
    int     m_shape = 0;                             ///< the weapon's family (rifle: gauss, carbine, scatter, rail,
                                                     ///< plasma, arc; hammer: ram, axe, pick, maul, breaker, drill)
    int     m_tier = 1;                              ///< the weapon's tier: more metal, more light
    int     m_heft = 1;                              ///< the suit: 0 light, 1 plated, 2 an exoskeleton
    Vector4 m_trim = { -1.0f, 0.0f, 0.0f, 0.0f };    ///< a glowing line on chest and weapon (the gear's rarity)
};

/** @brief A skinned body under the figure's plates (Quaternius' base body, CC0, dressed by the shader). */
struct FigureBody
{
    const SkinModel *m_model = nullptr;                   ///< the body (nullptr: the boxes alone)
    std::vector<int> m_surfaces;                          ///< per mesh of the body
    int              m_hair = -1;                         ///< the hair (the model's hair; -1: none)
    int              m_hairSurface = -1;                  ///< its surface
    int              m_brows = -1;                        ///< the brows (the model's hair)
    int              m_browSurface = -1;                  ///< their surface
    Vector3          m_cloth = { 0.12f, 0.12f, 0.14f };   ///< below the neck: the suit's or the clothes' colour
    bool             m_shown = true;                      ///< drawn (false: a frame's plates move with no man inside)
    bool             m_dressed = false;   ///< a whole person (suit, hair): only the weapon and the shield are boxes
    bool             m_ownSize = false;   ///< drawn at its own size (a hero), not as tall as the plates' figure
    bool m_helmet = false;   ///< the plates' helmet stays on (the men in suits: the visor shows the faction)
};

/** @brief Everything a pose depends on, so one built figure can draw many people. */
struct FigurePose
{
    Vector3 m_pos = { 0.0f, 0.0f, 0.0f };   ///< feet
    float   m_yaw = 0.0f;                   ///< heading (radians, 0 = +z)
    Vector2 m_vel = { 0.0f, 0.0f };         ///< velocity on the ground
    float   m_phase = 0.0f;                 ///< walk cycle (radians)
    float   m_time = 0.0f;                  ///< seconds since start (idle motion)
    bool    m_air = false;                  ///< in the air (jump pose)
    float   m_down = 0.0f;                  ///< 0 standing .. 1 fallen over backwards
    float   m_recoil = 0.0f;                ///< 1 just after a shot
    int     m_swing = -1;                   ///< the blow: 0 forehand, 1 backhand, 2 overhead (-1 none)
    float   m_swingT = 0.0f;                ///< 0..1 through the blow (the hit lands at 0.5)
    float   m_block = 0.0f;                 ///< 0..1 the shield raised
    float   m_sit = 0.0f;                   ///< 0..1 sitting on the floor, arms round the knees
    float   m_chair = 0.0f;                 ///< 0..1 sitting on a chair or stool, forearms on the table
    float   m_seat = 0.5f;                  ///< the seat's height (m)
    float   m_hit = 0.0f;                   ///< 1 just hit .. 0 (a flinch of the chest)
    float   m_knock = 0.0f;                 ///< 1 just thrown back by a heavy blow .. 0
    float   m_roll = 0.0f;                  ///< 0..1 through a dodge roll (0: none)
};

/** @brief A person. */
class Figure
{
public:
    /**
     * @brief Builds the parts.
     * @param[in] s  the surfaces to use and how the figure is made
     */
    void build( const FigureSurfaces &s );
    /** @return how the figure is made (its surfaces, weapon, size) */
    const FigureSurfaces &look() const { return m_look; }

    /**
     * @brief Moves the figure.
     * @param[in] dt      seconds since the last update
     * @param[in] wanted  wanted velocity on the ground (m/s, x and z)
     * @param[in] time    seconds since start (idle motion)
     */
    void update( float dt, Vector2 wanted, float time );

    /**
     * @brief Sets the pose's inputs from outside (the game moves the figure, the figure only animates).
     * @param[in] dt    seconds since the last call
     * @param[in] pos   feet
     * @param[in] vel   velocity on the ground
     * @param[in] yaw   heading (radians)
     * @param[in] air   in the air (jump pose)
     * @param[in] down  0 standing .. 1 fallen over
     * @param[in] time  seconds since start
     */
    void animate( float dt, Vector3 pos, Vector2 vel, float yaw, bool air, float down, float time );

    /**
     * @brief Puts the figure on a skinned body (kept when it is built again) and lines its plates up with the bones.
     * @param[in] body  the body and its looks (no model: the boxes alone again)
     */
    void body( const FigureBody &body );

    /** @return whether a skinned body moves the figure */
    bool skinned() const { return m_body.m_model != nullptr; }

    /**
     * @brief Adds only the plates and the weapon in a given pose (what flies apart when a suit bursts).
     * @param[in]     p    the pose
     * @param[in,out] out  the list to add to
     */
    void plates( const FigurePose &p, std::vector<Drawable> &out );

    /** @brief A new frame: the bone matrices handed out in the last one may be used again. */
    static void frame();

    /**
     * @brief Where the floor is, so the skinned figures put their feet on steps and edges.
     * @param[in] ground  the floor's height at x, z (below -100: none)
     */
    static void ground( std::function<float( float, float )> ground );

    /**
     * @brief Advances a walk cycle (for figures drawn from a FigurePose) as far as this figure's body steps.
     * @param[in,out] p   the pose (its phase moves)
     * @param[in]     dt  seconds
     */
    void step( FigurePose &p, float dt ) const;

    /** @brief Kicks the rifle back (a shot). */
    void recoil() { m_recoil = 1.0f; }

    /**
     * @brief Adds the figure's parts in its current pose.
     * @param[in,out] out  the list to add to
     */
    void pose( std::vector<Drawable> &out );

    /**
     * @brief Adds the figure's parts in a given pose (one built figure, many people).
     * @param[in]     p    the pose
     * @param[in]     glow the visor's glow for this one (x < 0: as built)
     * @param[in,out] out  the list to add to
     */
    void pose( const FigurePose &p, Vector4 glow, std::vector<Drawable> &out );

    /** @return where the figure looks (on the ground) */
    Vector3 forward() const;
    /** @return the lamp's position (after pose()) */
    Vector3 lampPos() const { return m_lampPos; }
    /** @return the lamp's direction (after pose()) */
    Vector3 lampDir() const { return m_lampDir; }
    /** @return the rifle's muzzle, or the hammer's head (after pose()) */
    Vector3 muzzle() const { return m_muzzle; }

    Vector3 m_pos = { 0.0f, 0.0f, 0.0f };   ///< feet on the ground
    float   m_yaw = 0.0f;                   ///< heading (radians, 0 = +z)
    Vector2 m_vel = { 0.0f, 0.0f };         ///< velocity on the ground
    float   m_phase = 0.0f;                 ///< walk cycle (radians)
    float   m_time = 0.0f;                  ///< seconds since start
    int     m_swing = -1;                   ///< the blow under way (see FigurePose)
    float   m_swingT = 0.0f;                ///< how far through it
    float   m_block = 0.0f;                 ///< the shield raised
    float   m_hit = 0.0f;                   ///< the flinch of a hit (see FigurePose)
    float   m_knock = 0.0f;                 ///< thrown back
    float   m_roll = 0.0f;                  ///< through a dodge roll

private:
    /** @brief A part: a mesh on a bone. */
    struct Part
    {
        Mesh    m_mesh;           ///< in the bone's frame
        int     m_surface;        ///< its surface
        BoneE   m_bone;           ///< the bone it sits on
        Vector4 m_emissive;       ///< its glow (x < 0: the surface's)
        bool    m_visor;          ///< the visor (takes a per-figure glow)
        bool    m_gear = false;   ///< carried, not worn (the shield): shown on a dressed body too
    };

    /** @brief This figure's own pose. */
    FigurePose own() const;

    /** @brief Computes the bones' frames for a pose. */
    void solve( const FigurePose &p );

    /** @brief Computes the bones' frames from the skinned body's clips (and the body's pose in m_skinPose). */
    void solveSkin( const FigurePose &p );

    /** @brief The lamp and the muzzle from the bones. */
    void finish( const FigurePose &p );

    /** @brief Lines the plates up with the body's bones (its first idle frame against the boxes' rest pose). */
    void calibrate();

    /** @brief Adds the parts (and the body) in the solved pose. */
    void emit( Vector4 glow, bool body, std::vector<Drawable> &out );

    /** @brief The weapon's frame in the root's space: carried, or along the blow. */
    Matrix weaponFrame( const FigurePose &p, float amp, float bob, float ph ) const;

    FigureSurfaces    m_look;
    std::vector<Part> m_parts;
    Matrix            m_bones[(int)BoneE::COUNT];
    float             m_recoil = 0.0f;
    bool              m_air = false;
    float             m_down = 0.0f;
    Vector3           m_lampPos = { 0.0f, 0.0f, 0.0f };
    Vector3           m_lampDir = { 0.0f, 0.0f, 1.0f };
    Vector3           m_muzzle = { 0.0f, 0.0f, 0.0f };

    FigureBody             m_body;                          ///< the skinned body, if any
    float                  m_bodyScale = 1.0f;              ///< the body's size to the boxes' (hips at the same height)
    Matrix                 m_offset[(int)BoneE::COUNT];     ///< each part's bone against the body's bone
    int                    m_map[(int)BoneE::COUNT] = {};   ///< the body's bone for each of the figure's
    Matrix                 m_grip = {};                     ///< the weapon against the right forearm
    std::vector<int>       m_clips;                         ///< the clips the figure plays (see Figure.cpp)
    std::vector<Transform> m_skinPose;                      ///< the body's pose this time
    int                    m_handL = -1;                    ///< the body's left hand (it holds the rifle's fore end)
    int                    m_handR = -1;                    ///< the body's right hand (on the rifle's grip)
    int                    m_knuckleL = -1, m_knuckleR = -1;   ///< the middle fingers' roots (which way a hand points)
    int                    m_spine = -1, m_neck = -1;          ///< the chest turns about the first, the head the second
    float                  m_cycle[3] = { 2.0f, 2.0f, 2.0f };   ///< metres a walk, a jog, a sprint cycle covers
    Matrix                 m_place = {};                        ///< the body's place in the world
};

}   // namespace spaceopera
