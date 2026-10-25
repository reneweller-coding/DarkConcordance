/**
 * @file Figure.h
 * @brief A person built from rounded boxes and moved procedurally (walk cycle, arms with two-bone IK, lean, sway):
 *        Corvin with his rifle, Dorra with hammer and shield, the boarders, and people without a suit (Ilka, the
 *        refugees) who sit or lie. A lamp on the right shoulder of a suit lights the way.
 * @author Rene Weller
 */
#pragma once

#include "Gfx.h"

#include <raylib.h>

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
    Vector3 m_scale = { 1.0f, 1.0f, 1.0f };          ///< width, height, depth (Dorra is short and broad)
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
     * @brief Advances a walk cycle (for figures drawn from a FigurePose).
     * @param[in,out] p   the pose (its phase moves)
     * @param[in]     dt  seconds
     */
    static void step( FigurePose &p, float dt );

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

private:
    /** @brief A part: a mesh on a bone. */
    struct Part
    {
        Mesh    m_mesh;       ///< in the bone's frame
        int     m_surface;    ///< its surface
        BoneE   m_bone;       ///< the bone it sits on
        Vector4 m_emissive;   ///< its glow (x < 0: the surface's)
        bool    m_visor;      ///< the visor (takes a per-figure glow)
    };

    /** @brief This figure's own pose. */
    FigurePose own() const;

    /** @brief Computes the bones' frames for a pose. */
    void solve( const FigurePose &p );

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
};

}   // namespace spaceopera
