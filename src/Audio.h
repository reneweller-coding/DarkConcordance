/**
 * @file Audio.h
 * @brief Sound effects synthesised at start (no sample files): the Gauss rifle, hits, the lice, the press, steam,
 *        explosions, pickups; played with distance and panning. The background of a place is a recorded loop
 *        (assets/sound/amb_<name>.ogg, made by tools/make_audio.py) or, without one, synthesised. Spoken lines are
 *        played from assets/voice when they exist. The device is opened once by the program (the music lives on
 *        across levels).
 * @author Rene Weller
 */
#pragma once

#include <raylib.h>

#include <string>

namespace spaceopera
{

/** @brief The sound effects. */
enum class SfxE : int
{
    SHOT,       ///< the Gauss rifle
    HIT,        ///< a slug on metal
    CHITTER,    ///< a louse noticing you
    LEAP,       ///< a louse leaping
    POP,        ///< a louse bursting
    SPIT,       ///< the welder spitting
    SIZZLE,     ///< molten metal landing
    SLAM,       ///< the press's ram
    KLAXON,     ///< the press's warning
    BURST,      ///< a coolant tank bursting
    BOOM,       ///< an explosion
    PICKUP,     ///< scrap or a stim taken
    CAPSULE,    ///< a rescue capsule wakes
    JUMP,       ///< the suit's jump servos
    LAND,       ///< landing
    HURT,       ///< taking damage
    OVERHEAT,   ///< the rifle overheats
    STIM,       ///< a stim injected
    SWING,      ///< a hammer or baton cutting the air
    SMASH,      ///< the hammer landing
    BLOCK,      ///< something on Dorra's shield
    ZAP,        ///< a boarder's carbine
    THRUST,     ///< the ship's dodge: the hull groans
    GUNSHOT,    ///< one heavy shot (the last scene)
    FALL,       ///< a body falling
    THROW,      ///< a blast bolt thrown
    DOOR,       ///< a door sliding open: a hiss, the servo, a clunk
    CRACKLE,    ///< a suit's power cell shorting out: sparks
    SWARM,      ///< Ione's drones cutting: a whirr of many small rotors
    HACK,       ///< a machine taken over: a chirp down the maintenance line
    COUNT       ///< number of effects
};

/** @brief The sound. */
class Audio
{
public:
    /**
     * @brief Synthesises the effects and starts the background (the device must be open).
     * @param[in] on        false: stay silent (tests, pictures)
     * @param[in] ambience  the background: assets/sound/amb_<ambience>.ogg, else synthesised ("ship", "suit",
     *                      "water", otherwise Velm's hum)
     * @param[in] assets    the assets folder
     */
    void init( bool on, const std::string &ambience = "tunnels", const std::string &assets = "" );

    /** @brief Lets go of the effects and the background. */
    void shutdown();

    /**
     * @brief Writes every synthesised effect and background as a WAV (to level recorded sounds against them).
     * @param[in] dir  the folder
     */
    static void dump( const std::string &dir );

    /**
     * @brief Plays an effect at a place.
     * @param[in] s         the effect
     * @param[in] at        where it happens
     * @param[in] listener  where the ears are (the camera's target)
     * @param[in] volume    loudness 0..1
     */
    void play( SfxE s, Vector3 at, Vector3 listener, float volume = 1.0f );

    /** @brief Keeps the ambience running (call every frame). */
    void update();

    /**
     * @brief Plays a spoken line if its file exists.
     * @param[in] path  the file without extension (.ogg, .wav and .mp3 are tried)
     * @return its length in seconds, 0 if there is none
     */
    float voice( const std::string &path );

    /** @brief Stops the spoken line (a conversation closed or moved on). */
    void stopVoice();

    bool  m_on = false;      ///< sound is running
    float m_muffle = 1.0f;   ///< what reaches the ears (out in a vacuum only what the suit carries)
    float m_yaw = 0.0f;      ///< the camera's turn (radians), so left and right follow the screen

private:
    static constexpr int Voices = 6;   ///< copies of each effect that can play at once
    Sound                m_sounds[(int)SfxE::COUNT][Voices] = {};
    int                  m_next[(int)SfxE::COUNT] = {};
    Sound                m_ambience = {};   ///< the synthesised background
    Music                m_bed = {};        ///< ... or the recorded one
    bool                 m_hasBed = false;
    Sound                m_voice = {};
    bool                 m_hasVoice = false;
};

}   // namespace spaceopera
