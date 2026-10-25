/**
 * @file Creatures.h
 * @brief The machines of level 1, built from shapes and moved procedurally: rust lice (six legs in a tripod gait with
 *        IK, feet planted until they must step), the larger welder louse with its torch, and the drone press with its
 *        ram, hatches, coolant tanks and core.
 * @author Rene Weller
 */
#pragma once

#include "Gfx.h"
#include "MeshBuilder.h"

#include <raylib.h>

#include <vector>

namespace spaceopera
{

/** @brief Where a louse's six feet are (world), and which are stepping. */
struct LouseLegs
{
    Vector3 m_foot[6];           ///< planted or moving feet
    Vector3 m_from[6];           ///< a step's start
    Vector3 m_to[6];             ///< a step's goal
    float   m_step[6] = { 0 };   ///< 0: planted, else progress of the step (0..1]
    bool    m_init = false;      ///< feet placed once
};

/** @brief What a louse's pose depends on. */
struct LousePose
{
    Vector3 m_pos;             ///< on the ground under the body
    float   m_yaw = 0.0f;      ///< heading
    float   m_crouch = 0.0f;   ///< 0..1 wind-up before a leap or a spit
    float   m_pitch = 0.0f;    ///< nose up (+) or down (-)
    bool    m_air = false;     ///< leaping
    float   m_flash = 0.0f;    ///< hit flash 0..1
    float   m_torch = 0.0f;    ///< the welder's torch glow 0..1
    float   m_time = 0.0f;     ///< seconds (idle twitches)
};

/** @brief The meshes of one kind of louse. */
class LouseModel
{
public:
    /**
     * @brief Builds the parts.
     * @param[in] rust    surface of the shell
     * @param[in] metal   surface of joints and legs
     * @param[in] glow    surface of the eye and torch
     * @param[in] welder  the larger kind with a torch and a tank
     */
    void build( int rust, int metal, int glow, bool welder );

    /**
     * @brief Moves the feet: a foot steps when it lags too far behind, three at a time.
     * @param[in,out] legs    the feet
     * @param[in]     pose    the body
     * @param[in]     vel     the body's velocity
     * @param[in]     ground  floor height under the body
     * @param[in]     dt      seconds
     */
    void stepLegs( LouseLegs &legs, const LousePose &pose, Vector3 vel, float ground, float dt ) const;

    /**
     * @brief Adds the louse's parts.
     * @param[in]     pose  the body
     * @param[in]     legs  the feet
     * @param[in,out] out   the list
     */
    void draw( const LousePose &pose, const LouseLegs &legs, std::vector<Drawable> &out ) const;

    float m_scale = 1.0f;     ///< size
    float m_height = 0.42f;   ///< body above the ground

private:
    /** @brief The body frame. */
    Matrix body( const LousePose &pose ) const;
    /** @brief A hip in body coordinates. */
    Vector3 hip( int i ) const;
    /** @brief A foot's rest place in body coordinates (on the ground). */
    Vector3 rest( int i ) const;

    struct Part
    {
        Mesh m_mesh;
        int  m_surface;
        bool m_glow;    ///< takes the eye's or torch's glow
        bool m_torch;   ///< the torch's nozzle
    };
    std::vector<Part> m_parts;
    Mesh              m_upper = {};
    Mesh              m_lower = {};
    int               m_metal = 0;
    float             m_thigh = 0.42f;
    float             m_shin = 0.55f;
    bool              m_welder = false;
};

/** @brief What the press's pose depends on. */
struct PressPose
{
    float m_ram = 5.0f;                 ///< height of the ram's middle
    float m_shutter = 0.0f;             ///< 0 closed .. 1 open (the core behind)
    float m_hatch[2] = { 0, 0 };        ///< glow of the hatches 0..1
    bool  m_tank[2] = { true, true };   ///< tanks intact
    float m_tankFlash[2] = { 0, 0 };    ///< hit flash
    float m_coreFlash = 0.0f;           ///< hit flash of the core
    float m_warn = 0.0f;                ///< warning lights 0..1
    bool  m_dead = false;               ///< all dark
    float m_time = 0.0f;                ///< seconds
};

/** @brief The drone press: a maintenance factory that builds lice from scrap. */
class PressModel
{
public:
    /**
     * @brief Builds the parts.
     * @param[in] origin  front middle of the housing on the floor; the arena lies towards +z
     * @param[in] metal   steel
     * @param[in] hazard  hazard stripes
     * @param[in] rust    rusty steel
     * @param[in] glow    lamps
     */
    void build( Vector3 origin, int metal, int hazard, int rust, int glow );

    /**
     * @brief Adds the press's parts.
     * @param[in]     pose  its state
     * @param[in,out] out   the list
     */
    void draw( const PressPose &pose, std::vector<Drawable> &out ) const;

    Vector3 m_origin = { 0.0f, 0.0f, 0.0f };   ///< front middle of the housing

private:
    struct Part
    {
        Mesh m_mesh;
        int  m_surface;
        int  m_role;   ///< 0 static, 1 ram, 2 left shutter, 3 right shutter, 4 tank 0, 5 tank 1, 6 core, 7 warning,
                       ///< 8 hatch 0, 9 hatch 1, 10 broken tank 0, 11 broken tank 1, 12 coolant 0, 13 coolant 1
    };
    void add( MeshBuilder &b, int surface, int role );

    std::vector<Part> m_parts;
};

}   // namespace spaceopera
