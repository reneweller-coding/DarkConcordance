/**
 * @file Skin.h
 * @brief Skinned figures from glTF (Quaternius' base characters, CC0): a body and its skeleton, animations from other
 *        files put onto it by bone name (both rigs share a T-pose to measure from), played with crossfades; hair
 *        and the eyes ride on the head bone.
 * @author Rene Weller
 */
#pragma once

#include "Gfx.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace spaceopera
{

/** @brief An animation on the body's skeleton: poses in the model's space, 1000/17 frames a second (raylib's). */
struct SkinClip
{
    std::string            m_name;
    int                    m_frames = 0;
    bool                   m_loop = false;   ///< loops ("..._Loop"), else holds its last frame
    std::vector<Transform> m_poses;          ///< frame * bones + bone
};

/** @brief Where one figure is in its animations. */
struct SkinState
{
    int   m_clip = 0;             ///< playing
    int   m_prev = -1;            ///< fading out (-1 none)
    float m_time = 0.0f;          ///< seconds into the clip
    float m_prevTime = 0.0f;      ///< seconds into the one fading out
    float m_fade = 1.0f;          ///< 0..1 through the crossfade
    float m_fadeLength = 0.25f;   ///< seconds the crossfade takes
};

/** @brief A body with its animations. */
class SkinModel
{
public:
    /**
     * @brief Loads the body.
     * @param[in] body   the body's glTF
     * @param[in] tpose  a file with an "A_TPose" animation on the same rig (the reference for retargeting)
     * @return false if the body cannot be read
     */
    bool load( const std::string &body, const std::string &tpose );

    /**
     * @brief Adds animations from a file, put onto the body by bone name (the file's own "A_TPose" is its reference).
     * @param[in] file   a glTF / GLB with animations
     * @param[in] names  only these (empty: all)
     * @return how many were added
     */
    int addClips( const std::string &file, const std::vector<std::string> &names = {} );

    /**
     * @brief A clip by its name.
     * @param[in] name  the name
     * @return its index, or -1
     */
    int clip( const std::string &name ) const;

    /**
     * @brief Hair (or anything else) modelled on the body in its rest pose: it rides on the head bone.
     * @param[in] file  the glTF
     * @return its index
     */
    int addHair( const std::string &file );

    /**
     * @brief Switches to a clip with a crossfade (nothing if it already plays).
     * @param[in,out] s     the figure
     * @param[in]     clip  the clip
     * @param[in]     fade  seconds
     */
    void play( SkinState &s, int clip, float fade = 0.25f ) const;

    /**
     * @brief Moves a figure on through its clip.
     * @param[in,out] s      the figure
     * @param[in]     dt     seconds
     * @param[in]     speed  how fast the clip runs
     */
    void advance( SkinState &s, float dt, float speed = 1.0f ) const;

    /**
     * @brief The skin matrices of a pose.
     * @param[in]  s      the figure
     * @param[out] bones  one per bone (rest pose to posed, in the model's space)
     */
    void pose( const SkinState &s, std::vector<Matrix> &bones ) const;

    /** @return the head bone's index */
    int head() const { return m_head; }
    /** @return how many bones */
    int bones() const { return (int)m_names.size(); }
    /** @return the clip's length in seconds */
    float length( int clip ) const;
    /** @return the pelvis's height in a clip's first frame (to set a sitting figure on its seat) */
    float hipHeight( int clip ) const;

    Model              m_body = {};   ///< the meshes (skinned ones carry bone ids and weights)
    std::vector<Model> m_hair;        ///< rigid on the head
    std::vector<bool>  m_skinned;     ///< per mesh of the body: skinned (else rigid on the head)

private:
    /** @brief A clip's pose at a time (interpolated between frames). */
    void sample( int clip, float time, std::vector<Transform> &out ) const;

    std::vector<std::string> m_names;
    std::vector<int>         m_parent;
    std::vector<Transform>   m_bind;   ///< rest pose, model space
    std::vector<Transform>   m_ref;    ///< the T-pose, model space
    std::vector<SkinClip>    m_clips;
    int                      m_head = 0;
};

}   // namespace spaceopera
