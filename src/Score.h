/**
 * @file Score.h
 * @brief The music: the pieces in assets/music (made by tools/make_audio.py from tools/music.json), listed in
 *        assets/music/music.txt. A piece has up to three layers of rising intensity, recorded on one grid, that play
 *        in lockstep and are faded into one another as a fight grows; it alternates its takes at the loop's end.
 *        Pieces cross-fade when the place or the situation changes; stingers play over them. Pieces are decoded in the
 *        background and mixed in raylib's audio thread, so nothing stalls a frame.
 * @author Rene Weller
 */
#pragma once

#include <raylib.h>

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace spaceopera
{

/** @brief The music: what plays, how hot, faded and mixed. */
class Score
{
public:
    /**
     * @brief Reads the list of pieces and starts the mixer (the audio device must be open).
     * @param[in] dir     the music folder (assets/music)
     * @param[in] on      false: stay silent (tests, pictures)
     * @param[in] device  false: no audio stream; the caller mixes itself (--scoretest)
     */
    void init( const std::string &dir, bool on, bool device = true );

    /**
     * @brief The pieces to hold decoded (a level's own, its boss's, the stingers): asked for now, never let go while
     *        they are wanted, so a boss fight's music is there when the fight begins.
     * @param[in] ids  the pieces
     */
    void keep( const std::vector<std::string> &ids );

    /**
     * @param[in] id  a piece
     * @return it is decoded
     */
    bool has( const std::string &id );

    /** @brief Stops the mixer and the loader. */
    void shutdown();

    ~Score() { shutdown(); }

    /**
     * @brief Says what should play now; call every frame.
     * @param[in] dt    the frame's time (seconds)
     * @param[in] cue   the piece ("" silence); a new one fades in once it is decoded, the old one out
     * @param[in] heat  0 calm .. 1 fight: which of the piece's layers sounds
     */
    void update( float dt, const std::string &cue, float heat );

    /**
     * @brief Plays a stinger once over the music (which steps back meanwhile).
     * @param[in] id  the piece (victory, death)
     */
    void sting( const std::string &id );

    float m_volume = 0.7f;   ///< the player's music volume 0..1
    float m_duck = 1.0f;     ///< below 1 while a menu is open

    /** @brief One piece in memory. */
    struct Piece
    {
        bool                                         m_loop = true;   ///< loops (else it plays once)
        int                                          m_layers = 0;    ///< 1..3, in rising intensity
        std::vector<std::vector<std::vector<short>>> m_pcm;           ///< [take][layer]: stereo frames, 48 kHz
    };

    /** @brief A piece sounding: where it is, how loud, its layers' mix. */
    struct Voice
    {
        std::shared_ptr<const Piece> m_piece;
        std::string                  m_id;
        bool                         m_sting = false;              ///< over the music, once
        bool                         m_done = false;               ///< played out (or faded away)
        int                          m_take = 0, m_prevTake = 0;   ///< the take, and the one fading out
        size_t                       m_pos = 0, m_prevPos = 0;     ///< frames into them
        int                          m_xfade = 0;                  ///< frames left of the change of takes
        int                          m_loops = 0;                  ///< times round
        float                        m_gain = 0.0f, m_target = 1.0f, m_rate = 1.0f;   ///< fade (per second)
        float                        m_layer[3] = { 1.0f, 0.0f, 0.0f };               ///< the layers' gains now
        float                        m_layerTarget[3] = { 1.0f, 0.0f, 0.0f };         ///< ... and where they go
    };

    /**
     * @brief Mixes the next frames (raylib's audio thread).
     * @param[out] out     interleaved stereo floats
     * @param[in]  frames  how many
     */
    void mix( float *out, unsigned frames );

private:
    /** @brief Wants a piece decoded (the loader takes it next). */
    void request( const std::string &id );
    /** @brief The loader: decodes requested pieces one after another. */
    void loader();
    /** @brief Decodes one piece (null if a file is missing). */
    std::shared_ptr<Piece> decode( const std::string &id ) const;

    struct Entry
    {
        bool                     m_loop = true;
        int                      m_takes = 1;
        std::vector<std::string> m_layers;
    };
    bool                         m_on = false;
    bool                         m_device = false;   ///< the mixer feeds an audio stream
    std::string                  m_dir;
    std::map<std::string, Entry> m_list;   ///< what music.txt names

    // Shared with the loader (m_loadLock).
    std::mutex                                    m_loadLock;
    std::condition_variable                       m_wake;
    std::vector<std::string>                      m_queue;
    std::map<std::string, std::shared_ptr<Piece>> m_ready;    ///< decoded, by id
    std::map<std::string, float>                  m_unused;   ///< seconds since a piece last sounded
    std::vector<std::string>                      m_keep;     ///< never let go (see keep())
    bool                                          m_stop = false;
    std::thread                                   m_thread;

    // Shared with the mixer (m_mixLock); never held while calling raylib.
    std::mutex         m_mixLock;
    std::vector<Voice> m_voices;
    float              m_gainNow = 0.7f;   ///< volume x duck, as the mixer uses it

    std::string m_cue;           ///< what should play
    std::string m_playing;       ///< what does
    float       m_heat = 0.0f;   ///< heat, eased (quick up, slow down)
    AudioStream m_stream = {};
};

}   // namespace spaceopera
