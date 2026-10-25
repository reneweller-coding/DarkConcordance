/**
 * @file Audio.cpp
 * @brief The synthesis of the effects and their playing.
 * @author Rene Weller
 */
#include "Audio.h"

#include <raymath.h>

#include <cmath>
#include <cstdlib>
#include <vector>

namespace spaceopera
{

namespace
{

constexpr int   Rate = 44100;   ///< sample rate
constexpr float Tau = 6.2831853f;

/** @brief A buffer of samples with a few tools to write into it. */
struct Buf
{
    std::vector<float> m_s;
    unsigned           m_seed = 777;

    explicit Buf( float seconds ) : m_s( (size_t)( seconds * Rate ), 0.0f ) {}
    float noise()
    {
        m_seed = m_seed * 1664525u + 1013904223u;
        return ( ( m_seed >> 8 ) / 8388608.0f ) - 1.0f;
    }
    /** @brief A sine sweep with an exponential decay. */
    void sweep( float start, float f0, float f1, float len, float amp, float decay )
    {
        double phase = 0.0;
        int    a = (int)( start * Rate );
        int    n = (int)( len * Rate );
        for ( int i = 0; i < n && a + i < (int)m_s.size(); ++i )
        {
            float t = (float)i / n;
            float f = f0 * powf( f1 / f0, t );
            phase += f / Rate;
            m_s[a + i] += amp * sinf( (float)( phase * Tau ) ) * expf( -decay * i / (float)Rate );
        }
    }
    /** @brief Filtered noise with an exponential decay (lowpass `lp` 0..1, highpass removes the low part). */
    void hiss( float start, float len, float amp, float decay, float lp, bool highpass = false )
    {
        int   a = (int)( start * Rate );
        int   n = (int)( len * Rate );
        float y = 0.0f;
        for ( int i = 0; i < n && a + i < (int)m_s.size(); ++i )
        {
            float x = noise();
            y += lp * ( x - y );
            float v = highpass ? x - y : y;
            float env = expf( -decay * i / (float)Rate ) * fminf( 1.0f, i / ( 0.003f * Rate ) );
            m_s[a + i] += amp * v * env;
        }
    }
    /** @brief A metallic ping: inharmonic partials. */
    void ping( float start, float f, float amp, float decay )
    {
        const float ratios[4] = { 1.0f, 1.56f, 2.21f, 2.93f };
        for ( int k = 0; k < 4; ++k )
            sweep( start, f * ratios[k], f * ratios[k], 2.0f / decay + 0.05f, amp / ( k + 1 ),
                   decay * ( 1.0f + k * 0.4f ) );
    }
    Wave wave()
    {
        float peak = 1e-6f;
        for ( float v : m_s ) peak = fmaxf( peak, fabsf( v ) );
        float  gain = peak > 0.95f ? 0.95f / peak : 1.0f;
        short *data = (short *)MemAlloc( (unsigned)( m_s.size() * sizeof( short ) ) );
        for ( size_t i = 0; i < m_s.size(); ++i ) data[i] = (short)( Clamp( m_s[i] * gain, -1.0f, 1.0f ) * 32767.0f );
        Wave w = {};
        w.frameCount = (unsigned)m_s.size();
        w.sampleRate = Rate;
        w.sampleSize = 16;
        w.channels = 1;
        w.data = data;
        return w;
    }
};

/** @brief Synthesises one effect. */
Wave make( SfxE s )
{
    switch ( s )
    {
    case SfxE::SHOT:
    {
        Buf b( 0.35f );
        b.sweep( 0.0f, 3200.0f, 380.0f, 0.12f, 0.45f, 18.0f );
        b.sweep( 0.0f, 140.0f, 60.0f, 0.12f, 0.6f, 25.0f );
        b.hiss( 0.0f, 0.08f, 0.5f, 60.0f, 0.6f, true );
        b.ping( 0.0f, 2400.0f, 0.12f, 30.0f );
        return b.wave();
    }
    case SfxE::HIT:
    {
        Buf b( 0.3f );
        b.ping( 0.0f, 1350.0f + rand() % 300, 0.35f, 22.0f );
        b.hiss( 0.0f, 0.03f, 0.5f, 120.0f, 0.8f, true );
        return b.wave();
    }
    case SfxE::CHITTER:
    {
        Buf b( 0.45f );
        for ( int i = 0; i < 9; ++i ) b.hiss( i * 0.045f + ( i % 3 ) * 0.008f, 0.012f, 0.6f, 200.0f, 0.5f, true );
        b.sweep( 0.0f, 900.0f, 1300.0f, 0.4f, 0.06f, 4.0f );
        return b.wave();
    }
    case SfxE::LEAP:
    {
        Buf b( 0.4f );
        b.sweep( 0.0f, 500.0f, 1700.0f, 0.3f, 0.25f, 6.0f );
        b.sweep( 0.0f, 760.0f, 2300.0f, 0.3f, 0.12f, 6.0f );
        b.hiss( 0.0f, 0.3f, 0.25f, 8.0f, 0.3f, true );
        return b.wave();
    }
    case SfxE::POP:
    {
        Buf b( 0.7f );
        b.hiss( 0.0f, 0.6f, 0.9f, 9.0f, 0.12f );
        b.sweep( 0.0f, 110.0f, 45.0f, 0.4f, 0.7f, 8.0f );
        for ( int i = 0; i < 5; ++i ) b.ping( 0.08f + i * 0.07f, 1800.0f + i * 230.0f, 0.06f, 40.0f );
        return b.wave();
    }
    case SfxE::SPIT:
    {
        Buf b( 0.5f );
        b.hiss( 0.0f, 0.45f, 0.6f, 6.0f, 0.5f, true );
        b.sweep( 0.0f, 220.0f, 90.0f, 0.3f, 0.3f, 10.0f );
        return b.wave();
    }
    case SfxE::SIZZLE:
    {
        Buf b( 0.7f );
        b.hiss( 0.0f, 0.65f, 0.5f, 5.0f, 0.7f, true );
        return b.wave();
    }
    case SfxE::SLAM:
    {
        Buf b( 1.6f );
        b.sweep( 0.0f, 70.0f, 28.0f, 1.0f, 1.0f, 3.5f );
        b.hiss( 0.0f, 0.8f, 0.9f, 6.0f, 0.06f );
        b.ping( 0.0f, 310.0f, 0.35f, 3.0f );
        b.ping( 0.02f, 520.0f, 0.2f, 4.0f );
        return b.wave();
    }
    case SfxE::KLAXON:
    {
        Buf b( 0.8f );
        for ( int i = 0; i < 4; ++i )
        {
            float f = i % 2 ? 660.0f : 520.0f;
            int   a = (int)( i * 0.19f * Rate );
            for ( int k = 0; k < (int)( 0.17f * Rate ); ++k )
            {
                float t = (float)k / Rate;
                float sq = sinf( Tau * f * t ) > 0.0f ? 1.0f : -1.0f;
                b.m_s[a + k] += 0.25f * sq * fminf( 1.0f, k / 200.0f ) * fminf( 1.0f, ( 0.17f * Rate - k ) / 400.0f );
            }
        }
        return b.wave();
    }
    case SfxE::BURST:
    {
        Buf b( 2.2f );
        b.hiss( 0.0f, 2.1f, 0.8f, 1.6f, 0.45f, true );
        b.sweep( 0.0f, 90.0f, 40.0f, 0.5f, 0.6f, 6.0f );
        b.ping( 0.0f, 420.0f, 0.3f, 5.0f );
        return b.wave();
    }
    case SfxE::BOOM:
    {
        Buf b( 2.0f );
        b.hiss( 0.0f, 1.9f, 1.0f, 2.2f, 0.05f );
        b.sweep( 0.0f, 60.0f, 25.0f, 1.2f, 0.9f, 2.5f );
        b.hiss( 0.0f, 0.2f, 0.6f, 20.0f, 0.5f, true );
        return b.wave();
    }
    case SfxE::PICKUP:
    {
        Buf b( 0.25f );
        b.sweep( 0.0f, 880.0f, 1320.0f, 0.08f, 0.3f, 20.0f );
        b.sweep( 0.07f, 1320.0f, 1760.0f, 0.12f, 0.25f, 20.0f );
        return b.wave();
    }
    case SfxE::CAPSULE:
    {
        Buf b( 1.6f );
        b.sweep( 0.0f, 660.0f, 660.0f, 1.5f, 0.25f, 2.5f );
        b.sweep( 0.12f, 990.0f, 990.0f, 1.4f, 0.2f, 2.5f );
        b.sweep( 0.24f, 1320.0f, 1320.0f, 1.3f, 0.15f, 2.5f );
        b.hiss( 0.0f, 1.0f, 0.08f, 3.0f, 0.3f, true );
        return b.wave();
    }
    case SfxE::JUMP:
    {
        Buf b( 0.4f );
        b.hiss( 0.0f, 0.35f, 0.4f, 7.0f, 0.35f, true );
        b.sweep( 0.0f, 300.0f, 700.0f, 0.2f, 0.12f, 12.0f );
        return b.wave();
    }
    case SfxE::LAND:
    {
        Buf b( 0.3f );
        b.sweep( 0.0f, 120.0f, 50.0f, 0.15f, 0.6f, 20.0f );
        b.hiss( 0.0f, 0.1f, 0.4f, 40.0f, 0.15f );
        b.ping( 0.0f, 600.0f, 0.08f, 25.0f );
        return b.wave();
    }
    case SfxE::HURT:
    {
        Buf b( 0.3f );
        b.sweep( 0.0f, 180.0f, 90.0f, 0.2f, 0.5f, 12.0f );
        b.hiss( 0.0f, 0.2f, 0.4f, 15.0f, 0.25f );
        return b.wave();
    }
    case SfxE::OVERHEAT:
    {
        Buf b( 0.9f );
        b.hiss( 0.0f, 0.85f, 0.5f, 3.0f, 0.6f, true );
        for ( int i = 0; i < (int)( 0.5f * Rate ); ++i )
            b.m_s[i] += 0.12f * ( fmodf( i * 110.0f / Rate, 1.0f ) - 0.5f ) * expf( -3.0f * i / Rate );
        return b.wave();
    }
    case SfxE::STIM:
    {
        Buf b( 0.6f );
        b.hiss( 0.0f, 0.15f, 0.4f, 25.0f, 0.6f, true );
        b.sweep( 0.12f, 440.0f, 880.0f, 0.4f, 0.2f, 5.0f );
        return b.wave();
    }
    default: break;
    }
    Buf b( 0.1f );
    return b.wave();
}

/** @brief The tunnels' hum: rumble, mains hum, ventilation, now and then a drop; loops seamlessly. */
Wave ambience()
{
    const float len = 12.0f;
    Buf         b( len );
    float       lo = 0.0f;
    float       vent = 0.0f;
    for ( size_t i = 0; i < b.m_s.size(); ++i )
    {
        float t = (float)i / Rate;
        float x = b.noise();
        lo += 0.004f * ( x - lo );
        vent += 0.05f * ( x - vent );
        float breath = 0.6f + 0.4f * sinf( Tau * t / len * 3.0f );
        b.m_s[i] = 2.5f * lo + 0.05f * vent * breath + 0.05f * sinf( Tau * 50.0f * t ) +
                   0.025f * sinf( Tau * 100.0f * t ) + 0.012f * sinf( Tau * 150.0f * t );
    }
    for ( float drop : { 1.3f, 4.1f, 6.8f, 9.9f, 11.0f } ) b.ping( drop, 1900.0f + drop * 37.0f, 0.04f, 18.0f );
    // Cross-fade the ends so the loop has no seam.
    int fade = Rate / 2;
    for ( int i = 0; i < fade; ++i )
    {
        float  w = (float)i / fade;
        size_t j = b.m_s.size() - fade + i;
        b.m_s[i] = b.m_s[i] * w + b.m_s[j] * ( 1.0f - w );
    }
    b.m_s.resize( b.m_s.size() - fade );
    return b.wave();
}

}   // namespace

void Audio::init( bool on )
{
    m_on = on;
    if ( !on ) return;
    InitAudioDevice();
    if ( !IsAudioDeviceReady() )
    {
        m_on = false;
        return;
    }
    SetMasterVolume( 0.8f );
    for ( int s = 0; s < (int)SfxE::COUNT; ++s )
    {
        Wave w = make( (SfxE)s );
        m_sounds[s][0] = LoadSoundFromWave( w );
        UnloadWave( w );
        for ( int v = 1; v < Voices; ++v ) m_sounds[s][v] = LoadSoundAlias( m_sounds[s][0] );
    }
    Wave a = ambience();
    m_ambience = LoadSoundFromWave( a );
    UnloadWave( a );
    SetSoundVolume( m_ambience, 0.55f );
}

void Audio::shutdown()
{
    if ( !m_on ) return;
    for ( int s = 0; s < (int)SfxE::COUNT; ++s )
    {
        for ( int v = 1; v < Voices; ++v ) UnloadSoundAlias( m_sounds[s][v] );
        UnloadSound( m_sounds[s][0] );
    }
    UnloadSound( m_ambience );
    if ( m_hasVoice ) UnloadSound( m_voice );
    CloseAudioDevice();
    m_on = false;
}

void Audio::play( SfxE s, Vector3 at, Vector3 listener, float volume )
{
    if ( !m_on ) return;
    Vector3 d = Vector3Subtract( at, listener );
    float   dist2 = d.x * d.x + d.z * d.z + 0.3f * d.y * d.y;
    float   v = volume / ( 1.0f + dist2 / 90.0f );
    if ( v < 0.01f ) return;
    int    i = (int)s;
    Sound &snd = m_sounds[i][m_next[i]];
    m_next[i] = ( m_next[i] + 1 ) % Voices;
    SetSoundVolume( snd, Clamp( v, 0.0f, 1.0f ) );
    SetSoundPitch( snd, 0.94f + 0.12f * ( rand() % 100 ) / 100.0f );
    SetSoundPan( snd, Clamp( 0.5f - d.x / 30.0f, 0.15f, 0.85f ) );
    PlaySound( snd );
}

void Audio::update()
{
    if ( m_on && !IsSoundPlaying( m_ambience ) ) PlaySound( m_ambience );
}

float Audio::voice( const std::string &path )
{
    if ( !m_on ) return 0.0f;
    for ( const char *ext : { ".ogg", ".wav", ".mp3" } )
    {
        std::string file = path + ext;
        if ( !FileExists( file.c_str() ) ) continue;
        if ( m_hasVoice ) UnloadSound( m_voice );
        Wave  w = LoadWave( file.c_str() );
        float len = w.sampleRate > 0 ? (float)w.frameCount / w.sampleRate : 0.0f;
        m_voice = LoadSoundFromWave( w );
        UnloadWave( w );
        m_hasVoice = true;
        PlaySound( m_voice );
        return len;
    }
    return 0.0f;
}

}   // namespace spaceopera
