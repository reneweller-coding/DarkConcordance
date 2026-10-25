/**
 * @file Figure.h
 * @brief The hero: a soldier in a segmented hardsuit, built from rounded boxes and moved procedurally (walk cycle,
 *        rifle held with two-bone IK, lean, sway, recoil). A lamp on the right shoulder lights the way.
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
    HEAD,      ///< helmet
    RIFLE,     ///< the rifle (held in front, independent of the chest's twist)
    UPPER_L,   ///< left upper arm
    FORE_L,    ///< left forearm and hand
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

/** @brief Which surfaces the figure uses (indices into the scene's list). */
struct FigureSurfaces
{
    int m_armor;   ///< the light armour plates
    int m_dark;    ///< dark plates
    int m_metal;   ///< joints, belts, the rifle's metal
    int m_glow;    ///< glowing parts (visor, lamp, status lights)
};

/** @brief The hero. */
class Figure
{
public:
    /**
     * @brief Builds the parts.
     * @param[in] s  the surfaces to use
     */
    void build( const FigureSurfaces &s );

    /**
     * @brief Moves the figure.
     * @param[in] dt      seconds since the last update
     * @param[in] wanted  wanted velocity on the ground (m/s, x and z)
     * @param[in] time    seconds since start (idle motion)
     */
    void update( float dt, Vector2 wanted, float time );

    /** @brief Kicks the rifle back (a shot). */
    void recoil() { m_recoil = 1.0f; }

    /**
     * @brief Adds the figure's parts in their current pose.
     * @param[in,out] out  the list to add to
     */
    void pose( std::vector<Drawable> &out );

    /** @return where the figure looks (on the ground) */
    Vector3 forward() const;
    /** @return the lamp's position (after pose()) */
    Vector3 lampPos() const { return m_lampPos; }
    /** @return the lamp's direction (after pose()) */
    Vector3 lampDir() const { return m_lampDir; }
    /** @return the rifle's muzzle (after pose()) */
    Vector3 muzzle() const { return m_muzzle; }

    Vector3 m_pos = { 0.0f, 0.0f, 0.0f };   ///< feet on the ground
    float   m_yaw = 0.0f;                   ///< heading (radians, 0 = +z)
    Vector2 m_vel = { 0.0f, 0.0f };         ///< velocity on the ground
    float   m_phase = 0.0f;                 ///< walk cycle (radians)
    float   m_time = 0.0f;                  ///< seconds since start

private:
    /** @brief A part: a mesh on a bone. */
    struct Part
    {
        Mesh    m_mesh;       ///< in the bone's frame
        int     m_surface;    ///< its surface
        BoneE   m_bone;       ///< the bone it sits on
        Vector4 m_emissive;   ///< its glow (x < 0: the surface's)
    };

    /** @brief Computes the bones' frames. */
    void solve();

    std::vector<Part> m_parts;
    Matrix            m_bones[(int)BoneE::COUNT];
    float             m_recoil = 0.0f;
    Vector3           m_lampPos = { 0.0f, 0.0f, 0.0f };
    Vector3           m_lampDir = { 0.0f, 0.0f, 1.0f };
    Vector3           m_muzzle = { 0.0f, 0.0f, 0.0f };
};

}   // namespace spaceopera
