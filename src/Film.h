/**
 * @file Film.h
 * @brief The films for the big moments (tools/make_films.py): AV1 in an IVF file (assets/films/<name>.ivf), decoded
 *        with dav1d frame by frame as the time goes, and a cue list (<name>.txt) with the music under it and when each
 *        line is said. The film itself is silent and serves every language; voices and subtitles come from the game.
 * @author Rene Weller
 */
#pragma once

#include <dav1d/dav1d.h>
#include <raylib.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace spaceopera
{

/** @brief A line said in a film. */
struct FilmCue
{
    float       m_at = 0.0f;   ///< when (seconds from the film's start)
    std::string m_who;         ///< who says it (as in a level's lines; hero: the hero played)
    std::string m_key;         ///< its text key (assets/text/<language>/films.txt) and voice
};

/** @brief A film playing: its frames, its cues. */
class Film
{
public:
    Film() = default;
    ~Film();
    Film( const Film & ) = delete;
    Film &operator=( const Film & ) = delete;

    /**
     * @brief Opens a film and shows its first frame.
     * @param[in] path  the film without extension (<path>.ivf and <path>.txt)
     * @return false if it is not there or cannot be decoded
     */
    bool open( const std::string &path );

    /** @brief Stops and lets go of everything. */
    void close();

    /**
     * @brief Moves the film on, decoding the frames that came due.
     * @param[in] dt  seconds since the last call
     */
    void update( float dt );

    /**
     * @brief Draws the current frame over the screen, black bars where the shapes differ.
     * @param[in] w  screen width
     * @param[in] h  screen height
     */
    void draw( int w, int h ) const;

    /** @return true while a film is open */
    bool playing() const { return m_ctx != nullptr; }

    /** @return seconds since the start */
    float time() const { return m_time; }

    /** @return true once its time is up or the pictures have run out */
    bool over() const { return m_time >= m_length || m_ended; }

    std::string          m_music;   ///< the music cue the game plays under it
    std::vector<FilmCue> m_cues;    ///< the lines, by time

private:
    /**
     * @brief Decodes the next picture.
     * @param[in] keep  turn it into the texture (else it is skipped: the film is catching up)
     * @return false at the end or on an error
     */
    bool nextPicture( bool keep );

    /**
     * @brief Reads the next frame of the IVF file.
     * @param[out] data  its bytes
     * @return false at the end
     */
    bool readFrame( std::vector<uint8_t> &data );

    Dav1dContext        *m_ctx = nullptr;
    Dav1dData            m_pending = {};   ///< read but not yet taken by the decoder
    FILE                *m_file = nullptr;
    Texture2D            m_tex = {};
    std::vector<uint8_t> m_rgb;   ///< the frame converted, for the texture
    int                  m_width = 0, m_height = 0;
    float                m_fps = 24.0f;
    float                m_time = 0.0f;
    float                m_length = 0.0f;
    int                  m_shown = -1;        ///< the frame on the texture
    bool                 m_drained = false;   ///< the file is read to its end
    bool                 m_ended = false;     ///< the last picture is shown
};

}   // namespace spaceopera
