/**
 * @file Audio.cpp
 * @brief The synthesis of the effects and their playing.
 * @author Rene Weller
 */
#include "Audio.h"

#include <raymath.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
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
    case SfxE::SWING:
    {
        // Air cut by something heavy: noise swelling and falling.
        Buf   b( 0.4f );
        float y = 0.0f;
        int   n = (int)b.m_s.size();
        for ( int i = 0; i < n; ++i )
        {
            float t = (float)i / n;
            y += 0.25f * ( b.noise() - y );
            b.m_s[i] = 0.7f * y * sinf( PI * t ) * sinf( PI * t );
        }
        return b.wave();
    }
    case SfxE::SMASH:
    {
        Buf b( 0.6f );
        b.sweep( 0.0f, 110.0f, 45.0f, 0.3f, 0.9f, 12.0f );
        b.hiss( 0.0f, 0.15f, 0.8f, 30.0f, 0.2f );
        b.ping( 0.0f, 640.0f, 0.25f, 14.0f );
        return b.wave();
    }
    case SfxE::BLOCK:
    {
        Buf b( 0.5f );
        b.ping( 0.0f, 880.0f, 0.4f, 9.0f );
        b.ping( 0.0f, 1310.0f, 0.2f, 12.0f );
        b.hiss( 0.0f, 0.04f, 0.5f, 90.0f, 0.7f, true );
        return b.wave();
    }
    case SfxE::ZAP:
    {
        Buf b( 0.25f );
        b.sweep( 0.0f, 1800.0f, 600.0f, 0.1f, 0.35f, 20.0f );
        b.hiss( 0.0f, 0.05f, 0.4f, 70.0f, 0.5f, true );
        b.sweep( 0.0f, 220.0f, 120.0f, 0.08f, 0.3f, 30.0f );
        return b.wave();
    }
    case SfxE::THRUST:
    {
        // The drive swings round: a deep roar, and the hull groaning under it.
        Buf b( 2.2f );
        b.hiss( 0.0f, 2.1f, 1.0f, 1.2f, 0.02f );
        b.sweep( 0.0f, 48.0f, 34.0f, 2.0f, 0.6f, 1.2f );
        b.sweep( 0.3f, 210.0f, 150.0f, 1.2f, 0.12f, 2.0f );
        b.sweep( 0.9f, 260.0f, 190.0f, 0.8f, 0.08f, 3.0f );
        return b.wave();
    }
    case SfxE::GUNSHOT:
    {
        Buf b( 1.4f );
        b.hiss( 0.0f, 0.12f, 1.0f, 40.0f, 0.7f, true );
        b.sweep( 0.0f, 160.0f, 40.0f, 0.5f, 1.0f, 7.0f );
        b.hiss( 0.0f, 1.3f, 0.35f, 2.5f, 0.05f );
        return b.wave();
    }
    case SfxE::FALL:
    {
        Buf b( 0.5f );
        b.sweep( 0.0f, 90.0f, 50.0f, 0.2f, 0.7f, 15.0f );
        b.hiss( 0.0f, 0.15f, 0.4f, 25.0f, 0.12f );
        b.sweep( 0.12f, 80.0f, 45.0f, 0.2f, 0.4f, 18.0f );
        return b.wave();
    }
    case SfxE::THROW:
    {
        Buf b( 0.35f );
        b.hiss( 0.0f, 0.3f, 0.4f, 9.0f, 0.3f, true );
        b.sweep( 0.0f, 600.0f, 900.0f, 0.15f, 0.1f, 15.0f );
        return b.wave();
    }
    case SfxE::DOOR:
    {
        Buf b( 0.8f );
        b.hiss( 0.0f, 0.45f, 0.45f, 5.0f, 0.4f, true );
        b.sweep( 0.02f, 160.0f, 240.0f, 0.5f, 0.22f, 4.0f );
        b.sweep( 0.5f, 95.0f, 50.0f, 0.15f, 0.6f, 25.0f );
        b.ping( 0.5f, 700.0f, 0.12f, 30.0f );
        return b.wave();
    }
    case SfxE::CRACKLE:
    {
        Buf b( 0.7f );
        for ( int i = 0; i < 12; ++i )
        {
            float t = 0.6f * powf( b.noise() * 0.5f + 0.5f, 1.6f );
            b.hiss( t, 0.04f, 0.5f, 90.0f, 0.7f, true );
            b.ping( t, 2600.0f + 1400.0f * ( b.noise() * 0.5f + 0.5f ), 0.08f, 70.0f );
        }
        return b.wave();
    }
    default: break;
    }
    Buf b( 0.1f );
    return b.wave();
}

/** @brief The tunnels' hum: rumble, mains hum, ventilation, now and then a drop (made a loop by loop()). */
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
    return b.wave();
}

/** @brief A hull under thrust: the drive's deep roar, its beat, creaking frames, air in the ducts. */
Wave shipAmbience()
{
    const float len = 12.0f;
    Buf         b( len );
    float       lo = 0.0f, mid = 0.0f;
    for ( size_t i = 0; i < b.m_s.size(); ++i )
    {
        float t = (float)i / Rate;
        float x = b.noise();
        lo += 0.0025f * ( x - lo );
        mid += 0.03f * ( x - mid );
        float beat = 0.75f + 0.25f * sinf( Tau * 0.5f * t ) * sinf( Tau * 3.0f / len * t );
        b.m_s[i] =
            3.2f * lo * beat + 0.03f * mid + 0.06f * sinf( Tau * 41.0f * t ) * beat + 0.03f * sinf( Tau * 61.5f * t );
    }
    for ( float creak : { 2.2f, 7.5f, 10.3f } ) b.sweep( creak, 180.0f, 140.0f, 0.6f, 0.05f, 3.0f );
    return b.wave();
}

/** @brief Inside a suit: slow breathing in the helmet, the fans, now and then the radio's crackle. */
Wave suitAmbience()
{
    const float len = 12.0f;
    Buf         b( len );
    float       lo = 0.0f, air = 0.0f;
    for ( size_t i = 0; i < b.m_s.size(); ++i )
    {
        float t = (float)i / Rate;
        float x = b.noise();
        lo += 0.003f * ( x - lo );
        air += 0.12f * ( x - air );
        // A breath every four seconds: in (rising), a pause, out (longer, lower).
        float ph = fmodf( t, 4.0f ), breath = 0.0f;
        if ( ph < 1.3f )
            breath = sinf( PI * ph / 1.3f ) * 0.9f;
        else if ( ph > 1.6f && ph < 3.6f )
            breath = sinf( PI * ( ph - 1.6f ) / 2.0f ) * 0.6f;
        b.m_s[i] = 1.2f * lo + 0.08f * air * breath +
                   0.02f * sinf( Tau * 180.0f * t ) * ( 0.6f + 0.4f * sinf( Tau * 0.3f * t ) );
    }
    for ( float c : { 2.7f, 8.3f } ) b.hiss( c, 0.25f, 0.2f, 60.0f, 0.5f, true );
    return b.wave();
}

/** @brief A flooded quarter: deep slow water moving, drips, now and then a gurgle in a pipe. */
Wave waterAmbience()
{
    const float len = 12.0f;
    Buf         b( len );
    float       lo = 0.0f, mid = 0.0f;
    for ( size_t i = 0; i < b.m_s.size(); ++i )
    {
        float t = (float)i / Rate;
        float x = b.noise();
        lo += 0.002f * ( x - lo );
        mid += 0.02f * ( x - mid );
        float swell = 0.7f + 0.3f * sinf( Tau * t / len * 2.0f );
        b.m_s[i] = 2.6f * lo * swell + 0.04f * mid * ( 0.5f + 0.5f * sinf( Tau * 0.21f * t ) );
    }
    for ( float drop : { 0.7f, 2.3f, 3.1f, 5.6f, 6.2f, 8.8f, 10.4f, 11.3f } )
        b.ping( drop, 1100.0f + drop * 113.0f, 0.06f, 22.0f );
    for ( float g : { 4.0f, 9.3f } ) b.sweep( g, 140.0f, 260.0f, 0.5f, 0.15f, 8.0f );
    return b.wave();
}

/** @brief Makes a sound loop: the ends cross-faded so it has no seam. */
Wave loop( Wave w )
{
    short *s = (short *)w.data;
    int    n = (int)w.frameCount, fade = Rate / 2;
    for ( int i = 0; i < fade; ++i )
    {
        float wgt = (float)i / fade;
        s[i] = (short)( s[i] * wgt + s[n - fade + i] * ( 1.0f - wgt ) );
    }
    w.frameCount = (unsigned)( n - fade );
    return w;
}

}   // namespace

void Audio::init( bool on, const std::string &ambienceName, const std::string &assets )
{
    m_on = on && IsAudioDeviceReady();
    if ( !m_on ) return;
    SetMasterVolume( 0.8f );
    for ( int s = 0; s < (int)SfxE::COUNT; ++s )
    {
        Wave w = make( (SfxE)s );
        m_sounds[s][0] = LoadSoundFromWave( w );
        UnloadWave( w );
        for ( int v = 1; v < Voices; ++v ) m_sounds[s][v] = LoadSoundAlias( m_sounds[s][0] );
    }
    // The recorded background, streamed in a loop; levelled by make_audio.py to sit where the synthesised one did.
    std::string bed = assets + "/sound/amb_" + ambienceName + ".ogg";
    if ( !assets.empty() && FileExists( bed.c_str() ) )
    {
        m_bed = LoadMusicStream( bed.c_str() );
        m_hasBed = m_bed.frameCount > 0;
    }
    if ( m_hasBed )
    {
        m_bed.looping = true;
        SetMusicVolume( m_bed, 0.55f );
        PlayMusicStream( m_bed );
        return;
    }
    Wave a = loop( ambienceName == "ship"    ? shipAmbience()
                   : ambienceName == "suit"  ? suitAmbience()
                   : ambienceName == "water" ? waterAmbience()
                                             : ambience() );
    m_ambience = LoadSoundFromWave( a );
    UnloadWave( a );
    SetSoundVolume( m_ambience, 0.55f );
}

void Audio::dump( const std::string &dir )
{
    for ( int s = 0; s < (int)SfxE::COUNT; ++s )
    {
        Wave w = make( (SfxE)s );
        ExportWave( w, TextFormat( "%s/sfx_%02d.wav", dir.c_str(), s ) );
        UnloadWave( w );
    }
    const char *names[] = { "tunnels", "ship", "suit", "water" };
    for ( const char *name : names )
    {
        Wave w = loop( !strcmp( name, "ship" )    ? shipAmbience()
                       : !strcmp( name, "suit" )  ? suitAmbience()
                       : !strcmp( name, "water" ) ? waterAmbience()
                                                  : ambience() );
        ExportWave( w, TextFormat( "%s/amb_%s.wav", dir.c_str(), name ) );
        UnloadWave( w );
    }
}

void Audio::shutdown()
{
    if ( !m_on ) return;
    for ( int s = 0; s < (int)SfxE::COUNT; ++s )
    {
        for ( int v = 1; v < Voices; ++v ) UnloadSoundAlias( m_sounds[s][v] );
        UnloadSound( m_sounds[s][0] );
    }
    if ( m_hasBed )
        UnloadMusicStream( m_bed ), m_hasBed = false;
    else
        UnloadSound( m_ambience );
    if ( m_hasVoice ) UnloadSound( m_voice );
    m_on = false;
}

void Audio::play( SfxE s, Vector3 at, Vector3 listener, float volume )
{
    if ( !m_on ) return;
    Vector3 d = Vector3Subtract( at, listener );
    float   dist2 = d.x * d.x + d.z * d.z + 0.3f * d.y * d.y;
    float   v = volume * m_muffle / ( 1.0f + dist2 / 90.0f );
    if ( v < 0.01f ) return;
    int    i = (int)s;
    Sound &snd = m_sounds[i][m_next[i]];
    m_next[i] = ( m_next[i] + 1 ) % Voices;
    SetSoundVolume( snd, Clamp( v, 0.0f, 1.0f ) );
    SetSoundPitch( snd, 0.94f + 0.12f * ( rand() % 100 ) / 100.0f );
    float right = d.x * cosf( m_yaw ) - d.z * sinf( m_yaw );
    SetSoundPan( snd, Clamp( 0.5f - right / 30.0f, 0.15f, 0.85f ) );
    PlaySound( snd );
}

void Audio::update()
{
    if ( !m_on ) return;
    if ( m_hasBed )
        UpdateMusicStream( m_bed );
    else if ( !IsSoundPlaying( m_ambience ) )
        PlaySound( m_ambience );
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
