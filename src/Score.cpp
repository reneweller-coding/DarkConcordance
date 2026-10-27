/**
 * @file Score.cpp
 * @brief The music: decoding in the background, cross-fades between pieces, layers and takes, the mixer.
 * @author Rene Weller
 */
#include "Score.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace spaceopera
{

namespace
{

constexpr int   Rate = 48000;              ///< the pieces' sample rate (ACE-Step's)
constexpr float PieceFade = 2.5f;          ///< seconds a piece takes to give way to the next
constexpr float FirstFade = 1.0f;          ///< ... to come up out of silence
constexpr float LayerFade = 1.5f;          ///< seconds a layer takes to come up or go
constexpr int   TakeFade = Rate * 3 / 2;   ///< frames over which one take hands over to the next
constexpr float StingDuck = 0.45f;         ///< the music under a stinger
constexpr float Forget = 120.0f;           ///< seconds a decoded piece is kept after it last sounded
constexpr float HalfPi = 1.5707963f;

Score *s_score = nullptr;   ///< the mixer's way back from raylib's callback

void mixCallback( void *buffer, unsigned int frames )
{
    if ( s_score ) s_score->mix( (float *)buffer, frames );
}

/** @brief A value moved toward a target by at most a step. */
float approach( float v, float target, float step )
{
    return v < target ? fminf( target, v + step ) : fmaxf( target, v - step );
}

/** @brief The layers' gains for a heat: neighbouring layers cross-fade at equal power. */
void layersFor( float heat, int layers, float *gain )
{
    gain[0] = gain[1] = gain[2] = 0.0f;
    if ( layers <= 1 )
    {
        gain[0] = 1.0f;
        return;
    }
    float p = fminf( 1.0f, fmaxf( 0.0f, heat ) ) * ( layers - 1 );
    int   i = std::min( (int)p, layers - 2 );
    float f = p - i;
    gain[i] = cosf( f * HalfPi );
    gain[i + 1] = sinf( f * HalfPi );
}

}   // namespace

void Score::init( const std::string &dir, bool on, bool device )
{
    m_dir = dir;
    m_device = device;
    m_on = on && ( !device || IsAudioDeviceReady() );
    // music.txt: "<piece> <loop|stinger|oneshot> <takes> <layer> ..." with the layers in rising intensity.
    char *text =
        FileExists( ( dir + "/music.txt" ).c_str() ) ? LoadFileText( ( dir + "/music.txt" ).c_str() ) : nullptr;
    if ( text )
    {
        std::istringstream in( text );
        std::string        line;
        while ( std::getline( in, line ) )
        {
            if ( line.empty() || line[0] == '#' ) continue;
            std::istringstream words( line );
            std::string        id, kind, layer;
            Entry              e;
            if ( !( words >> id >> kind >> e.m_takes ) ) continue;
            e.m_loop = kind == "loop";
            while ( words >> layer ) e.m_layers.push_back( layer );
            if ( !e.m_layers.empty() ) m_list[id] = e;
        }
        UnloadFileText( text );
    }
    if ( !m_on || m_list.empty() )
    {
        m_on = false;
        return;
    }
    if ( m_device )
    {
        SetAudioStreamBufferSizeDefault( 4096 );
        m_stream = LoadAudioStream( Rate, 32, 2 );
        SetAudioStreamBufferSizeDefault( 0 );
        s_score = this;
        SetAudioStreamCallback( m_stream, mixCallback );
        PlayAudioStream( m_stream );
    }
    m_thread = std::thread( &Score::loader, this );
}

void Score::shutdown()
{
    if ( !m_on ) return;
    {
        std::lock_guard<std::mutex> l( m_loadLock );
        m_stop = true;
    }
    m_wake.notify_all();
    if ( m_thread.joinable() ) m_thread.join();
    if ( m_device )
    {
        StopAudioStream( m_stream );
        UnloadAudioStream( m_stream );
        s_score = nullptr;
    }
    m_voices.clear();
    m_ready.clear();
    m_on = false;
}

void Score::keep( const std::vector<std::string> &ids )
{
    if ( !m_on ) return;
    {
        std::lock_guard<std::mutex> l( m_loadLock );
        m_keep = ids;
    }
    for ( const std::string &id : ids ) request( id );
}

bool Score::has( const std::string &id )
{
    std::lock_guard<std::mutex> l( m_loadLock );
    return m_ready.count( id ) > 0;
}

void Score::request( const std::string &id )
{
    if ( !m_list.count( id ) ) return;
    {
        std::lock_guard<std::mutex> l( m_loadLock );
        if ( m_ready.count( id ) || std::find( m_queue.begin(), m_queue.end(), id ) != m_queue.end() ) return;
        m_queue.push_back( id );
    }
    m_wake.notify_one();
}

void Score::loader()
{
    for ( ;; )
    {
        std::string id;
        {
            std::unique_lock<std::mutex> l( m_loadLock );
            m_wake.wait( l, [this]() { return m_stop || !m_queue.empty(); } );
            if ( m_stop ) return;
            id = m_queue.front();
        }
        std::shared_ptr<Piece>      piece = decode( id );
        std::lock_guard<std::mutex> l( m_loadLock );
        m_queue.erase( m_queue.begin() );
        if ( piece ) m_ready[id] = piece, m_unused[id] = 0.0f;
    }
}

std::shared_ptr<Score::Piece> Score::decode( const std::string &id ) const
{
    auto it = m_list.find( id );
    if ( it == m_list.end() ) return nullptr;
    const Entry &e = it->second;
    auto         piece = std::make_shared<Piece>();
    piece->m_loop = e.m_loop;
    piece->m_layers = std::min( 3, (int)e.m_layers.size() );
    for ( int t = 1; t <= e.m_takes; ++t )
    {
        std::vector<std::vector<short>> take;
        size_t                          frames = 0;
        for ( int l = 0; l < piece->m_layers; ++l )
        {
            std::string path = m_dir + "/" + id + "_" + e.m_layers[l] + "_t" + std::to_string( t ) + ".ogg";
            if ( !FileExists( path.c_str() ) ) break;
            Wave w = LoadWave( path.c_str() );
            if ( w.frameCount == 0 ) break;
            WaveFormat( &w, Rate, 16, 2 );
            const short *s = (const short *)w.data;
            // The layers of a take were cut on one grid; should one be a frame off, it follows the first.
            if ( l == 0 ) frames = w.frameCount;
            std::vector<short> pcm( frames * 2, 0 );
            memcpy( pcm.data(), s, std::min<size_t>( frames, w.frameCount ) * 2 * sizeof( short ) );
            UnloadWave( w );
            take.push_back( std::move( pcm ) );
        }
        if ( (int)take.size() < piece->m_layers ) break;
        piece->m_pcm.push_back( std::move( take ) );
    }
    return piece->m_pcm.empty() ? nullptr : piece;
}

void Score::update( float dt, const std::string &cue, float heat )
{
    if ( !m_on ) return;
    // The heat comes up within a second or two and sinks slowly: the music does not drop at the last shot.
    m_heat += ( heat - m_heat ) * fminf( 1.0f, ( heat > m_heat ? 1.2f : 0.15f ) * dt );
    if ( cue != m_cue )
    {
        m_cue = cue;
        request( cue );
    }
    std::shared_ptr<Piece>   ready;
    std::vector<std::string> sounding;
    {
        std::lock_guard<std::mutex> l( m_mixLock );
        for ( const Voice &v : m_voices ) sounding.push_back( v.m_id );
    }
    {
        std::lock_guard<std::mutex> l( m_loadLock );
        if ( !m_cue.empty() && m_cue != m_playing )
        {
            auto it = m_ready.find( m_cue );
            if ( it != m_ready.end() ) ready = it->second;
        }
        // What has not sounded for a while is let go (the stingers stay).
        for ( auto it = m_unused.begin(); it != m_unused.end(); )
        {
            bool keep = it->first == m_cue || std::find( m_keep.begin(), m_keep.end(), it->first ) != m_keep.end();
            it->second =
                std::find( sounding.begin(), sounding.end(), it->first ) != sounding.end() ? 0.0f : it->second + dt;
            if ( !keep && it->second > Forget )
            {
                m_ready.erase( it->first );
                it = m_unused.erase( it );
            }
            else
                ++it;
        }
    }
    std::vector<Voice> dropped;   // let go outside the lock (their memory may be large)
    {
        std::lock_guard<std::mutex> l( m_mixLock );
        m_gainNow = m_volume * m_duck;
        if ( m_cue != m_playing && ( ready || m_cue.empty() || !m_list.count( m_cue ) ) )
        {
            // The new piece: what played gives way, the new one comes up (at once out of silence).
            bool any = false;
            for ( Voice &v : m_voices )
                if ( !v.m_sting && !v.m_done ) v.m_target = 0.0f, v.m_rate = 1.0f / PieceFade, any = true;
            if ( ready )
            {
                Voice v;
                v.m_piece = ready;
                v.m_id = m_cue;
                v.m_take = rand() % (int)ready->m_pcm.size();
                v.m_rate = 1.0f / ( any ? PieceFade : FirstFade );
                layersFor( m_heat, ready->m_layers, v.m_layer );
                m_voices.push_back( v );
            }
            m_playing = m_cue;
        }
        for ( Voice &v : m_voices )
            if ( !v.m_sting && v.m_id == m_playing ) layersFor( m_heat, v.m_piece->m_layers, v.m_layerTarget );
        for ( auto it = m_voices.begin(); it != m_voices.end(); )
            if ( it->m_done )
            {
                dropped.push_back( std::move( *it ) );
                it = m_voices.erase( it );
            }
            else
                ++it;
    }
}

void Score::sting( const std::string &id )
{
    if ( !m_on ) return;
    std::shared_ptr<Piece> piece;
    {
        std::lock_guard<std::mutex> l( m_loadLock );
        auto                        it = m_ready.find( id );
        if ( it != m_ready.end() ) piece = it->second;
    }
    if ( !piece )
    {
        request( id );
        return;
    }
    Voice v;
    v.m_piece = piece;
    v.m_id = id;
    v.m_sting = true;
    v.m_gain = 1.0f;
    v.m_take = rand() % (int)piece->m_pcm.size();
    std::lock_guard<std::mutex> l( m_mixLock );
    m_voices.push_back( v );
}

void Score::mix( float *out, unsigned frames )
{
    memset( out, 0, frames * 2 * sizeof( float ) );
    std::lock_guard<std::mutex> guard( m_mixLock );
    bool                        stinging = false;
    for ( const Voice &v : m_voices ) stinging |= v.m_sting && !v.m_done;
    const float layerStep = 1.0f / ( LayerFade * Rate );
    for ( Voice &v : m_voices )
    {
        if ( v.m_done ) continue;
        const Piece &p = *v.m_piece;
        const float  gainStep = v.m_rate / Rate;
        const float  level = m_gainNow * ( !v.m_sting && stinging ? StingDuck : 1.0f );
        for ( unsigned f = 0; f < frames; ++f )
        {
            v.m_gain = approach( v.m_gain, v.m_target, gainStep );
            float l = 0.0f, r = 0.0f;
            for ( int k = 0; k < p.m_layers; ++k )
            {
                v.m_layer[k] = approach( v.m_layer[k], v.m_layerTarget[k], layerStep );
                if ( v.m_layer[k] <= 0.0f ) continue;
                const short *s = p.m_pcm[v.m_take][k].data() + v.m_pos * 2;
                l += v.m_layer[k] * s[0];
                r += v.m_layer[k] * s[1];
                if ( v.m_xfade > 0 )
                {
                    // The take before, running on into its own start, fades out under the new one.
                    float        a = 1.0f - (float)v.m_xfade / TakeFade;
                    const short *q = p.m_pcm[v.m_prevTake][k].data() + v.m_prevPos * 2;
                    float        in = sinf( a * HalfPi ), outGain = cosf( a * HalfPi );
                    l = l - v.m_layer[k] * s[0] * ( 1.0f - in ) + v.m_layer[k] * q[0] * outGain;
                    r = r - v.m_layer[k] * s[1] * ( 1.0f - in ) + v.m_layer[k] * q[1] * outGain;
                }
            }
            if ( v.m_xfade > 0 ) --v.m_xfade, ++v.m_prevPos;
            float g = v.m_gain * level / 32768.0f;
            out[f * 2] += l * g;
            out[f * 2 + 1] += r * g;
            if ( ++v.m_pos * 2 >= p.m_pcm[v.m_take][0].size() )
            {
                if ( !p.m_loop || v.m_sting )
                {
                    v.m_done = true;
                    break;
                }
                // Round again; every second time the next take takes over (both start on the grid's first beat).
                v.m_pos = 0;
                ++v.m_loops;
                int takes = (int)p.m_pcm.size();
                if ( takes > 1 && v.m_loops % 2 == 0 && v.m_xfade == 0 &&
                     p.m_pcm[v.m_take][0].size() >= (size_t)TakeFade * 2 )
                {
                    v.m_prevTake = v.m_take;
                    v.m_prevPos = 0;
                    v.m_take = ( v.m_take + 1 ) % takes;
                    v.m_xfade = std::min<int>( TakeFade, (int)( p.m_pcm[v.m_take][0].size() / 2 ) );
                }
            }
            if ( v.m_xfade > 0 && ( v.m_prevPos + 1 ) * 2 >= p.m_pcm[v.m_prevTake][0].size() ) v.m_xfade = 0;
        }
        if ( v.m_target <= 0.0f && v.m_gain <= 0.0f ) v.m_done = true;
    }
    for ( unsigned i = 0; i < frames * 2; ++i ) out[i] = fminf( 1.0f, fmaxf( -1.0f, out[i] ) );
}

}   // namespace spaceopera
