/**
 * @file Film.cpp
 * @brief The films: IVF read frame by frame, AV1 decoded by dav1d, BT.709 YUV turned into the texture's RGB.
 * @author Rene Weller
 */
#include "Film.h"

#include <dav1d/dav1d.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace spaceopera
{

namespace
{

uint16_t le16( const uint8_t *p )
{
    return (uint16_t)( p[0] | p[1] << 8 );
}

uint32_t le32( const uint8_t *p )
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

uint8_t clamp8( int v )
{
    return (uint8_t)( v < 0 ? 0 : ( v > 255 ? 255 : v ) );
}

}   // namespace

Film::~Film()
{
    close();
}

bool Film::open( const std::string &path )
{
    close();
    m_file = fopen( ( path + ".ivf" ).c_str(), "rb" );
    if ( !m_file ) return false;
    // IVF: a 32-byte head ("DKIF", its size at 6, the codec, the size of the picture, the frame rate, the frame count).
    uint8_t head[32];
    if ( fread( head, 1, 32, m_file ) != 32 || memcmp( head, "DKIF", 4 ) != 0 || memcmp( head + 8, "AV01", 4 ) != 0 )
    {
        close();
        return false;
    }
    m_width = le16( head + 12 );
    m_height = le16( head + 14 );
    uint32_t rate = le32( head + 16 ), scale = le32( head + 20 );
    m_fps = rate > 0 && scale > 0 ? (float)rate / (float)scale : 24.0f;
    m_length = (float)le32( head + 24 ) / m_fps;
    fseek( m_file, le16( head + 6 ), SEEK_SET );

    Dav1dSettings settings;
    dav1d_default_settings( &settings );
#if DAV1D_API_VERSION_MAJOR >= 6
    settings.n_threads = 4;
    settings.max_frame_delay = 1;   // a picture for every frame sent: no frames held back
#else
    settings.n_frame_threads = 1;   // dav1d before 1.0 (Ubuntu 22.04): one frame at a time, tiles in parallel
    settings.n_tile_threads = 4;
#endif
    if ( dav1d_open( &m_ctx, &settings ) < 0 )
    {
        m_ctx = nullptr;
        close();
        return false;
    }
    m_rgb.assign( (size_t)m_width * m_height * 3, 0 );
    Image image = { m_rgb.data(), m_width, m_height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8 };
    m_tex = LoadTextureFromImage( image );
    SetTextureFilter( m_tex, TEXTURE_FILTER_BILINEAR );

    // The cues: "music <cue>", "length <s>", "line <s> <who> <key>".
    std::ifstream in( path + ".txt" );
    std::string   line;
    while ( std::getline( in, line ) )
    {
        if ( !line.empty() && line.back() == '\r' ) line.pop_back();
        if ( line.empty() || line[0] == '#' ) continue;
        std::stringstream ss( line );
        std::string       word;
        ss >> word;
        if ( word == "music" )
            ss >> m_music;
        else if ( word == "length" )
            ss >> m_length;
        else if ( word == "line" )
        {
            FilmCue c;
            ss >> c.m_at >> c.m_who >> c.m_key;
            m_cues.push_back( c );
        }
    }
    std::sort( m_cues.begin(), m_cues.end(), []( const FilmCue &a, const FilmCue &b ) { return a.m_at < b.m_at; } );

    m_time = 0.0f;
    if ( !nextPicture( true ) )
    {
        close();
        return false;
    }
    m_shown = 0;
    return true;
}

void Film::close()
{
    if ( m_pending.sz > 0 ) dav1d_data_unref( &m_pending );
    m_pending = {};
    if ( m_ctx ) dav1d_close( &m_ctx );
    m_ctx = nullptr;
    if ( m_tex.id ) UnloadTexture( m_tex );
    m_tex = {};
    if ( m_file ) fclose( m_file );
    m_file = nullptr;
    m_rgb.clear();
    m_cues.clear();
    m_music.clear();
    m_shown = -1;
    m_drained = false;
    m_ended = false;
}

bool Film::readFrame( std::vector<uint8_t> &data )
{
    // A frame: its size (4 bytes), its time (8), then the data.
    uint8_t head[12];
    if ( !m_file || fread( head, 1, 12, m_file ) != 12 ) return false;
    data.resize( le32( head ) );
    return !data.empty() && fread( data.data(), 1, data.size(), m_file ) == data.size();
}

bool Film::nextPicture( bool keep )
{
    // Data in until a picture comes out (dav1d may want a picture taken before it takes more data).
    Dav1dPicture picture = {};
    for ( ;; )
    {
        if ( m_pending.sz == 0 && !m_drained )
        {
            std::vector<uint8_t> frame;
            if ( readFrame( frame ) )
                memcpy( dav1d_data_create( &m_pending, frame.size() ), frame.data(), frame.size() );
            else
                m_drained = true;
        }
        if ( m_pending.sz > 0 )
        {
            int sent = dav1d_send_data( m_ctx, &m_pending );
            if ( sent < 0 && sent != DAV1D_ERR( EAGAIN ) ) return false;
        }
        int got = dav1d_get_picture( m_ctx, &picture );
        if ( got == 0 ) break;
        if ( got != DAV1D_ERR( EAGAIN ) ) return false;
        if ( m_drained && m_pending.sz == 0 ) return false;   // nothing held back any more: the end
    }
    if ( keep && picture.p.layout == DAV1D_PIXEL_LAYOUT_I420 && picture.p.bpc == 8 && picture.p.w == m_width &&
         picture.p.h == m_height )
    {
        // BT.709, video range, in fixed point (the factors times 256).
        const uint8_t *py = (const uint8_t *)picture.data[0];
        const uint8_t *pu = (const uint8_t *)picture.data[1];
        const uint8_t *pv = (const uint8_t *)picture.data[2];
        ptrdiff_t      sy = picture.stride[0], suv = picture.stride[1];
        uint8_t       *out = m_rgb.data();
        for ( int y = 0; y < m_height; ++y )
        {
            const uint8_t *ry = py + y * sy, *ru = pu + ( y >> 1 ) * suv, *rv = pv + ( y >> 1 ) * suv;
            for ( int x = 0; x < m_width; ++x )
            {
                int c = 298 * ( ry[x] - 16 ) + 128, u = ru[x >> 1] - 128, v = rv[x >> 1] - 128;
                *out++ = clamp8( ( c + 459 * v ) >> 8 );
                *out++ = clamp8( ( c - 55 * u - 136 * v ) >> 8 );
                *out++ = clamp8( ( c + 541 * u ) >> 8 );
            }
        }
        UpdateTexture( m_tex, m_rgb.data() );
    }
    dav1d_picture_unref( &picture );
    return true;
}

void Film::update( float dt )
{
    if ( !m_ctx ) return;
    m_time += dt;
    // The frames that came due; only the last of them is turned into the picture.
    int want = (int)( m_time * m_fps );
    while ( m_shown < want && !m_ended )
    {
        if ( !nextPicture( m_shown + 1 == want ) )
            m_ended = true;
        else
            ++m_shown;
    }
}

void Film::draw( int w, int h ) const
{
    DrawRectangle( 0, 0, w, h, BLACK );
    if ( !m_tex.id ) return;
    float s = fminf( (float)w / m_width, (float)h / m_height );
    float dw = m_width * s, dh = m_height * s;
    DrawTexturePro( m_tex, { 0, 0, (float)m_width, (float)m_height }, { ( w - dw ) * 0.5f, ( h - dh ) * 0.5f, dw, dh },
                    { 0, 0 }, 0.0f, WHITE );
}

}   // namespace spaceopera
