/**
 * @file GameAct34.cpp
 * @brief The elements of Act III's and IV's places: the ring habitat seen from inside (its land rising into the sky,
 *        with a look up at it when one arrives); the dying gardens' rounds with changing rules; evidence on terminals
 *        that the Collegium's cleaners wipe one by one; the heat outside the turning shield's shadow; a moon's crust
 *        that the cutters slice (lines glow, then they cut) and the Mill's laser pulses; the suit's jets with their
 *        fuel; centrifuges that fling one outward; molten metal below; HAUSMEISTER's turrets on the hero's side; the
 *        catcher's grip as a clock; waterfalls drifting with the spin; a crowd that stays seated.
 *
 *        Each is switched on by its key in the level file (Level::m_keys):
 *          view ring                 the ring around the level (the planet shader's ring mode)
 *          vista s                   s seconds looking up at the sky when the level begins
 *          waves n                   n rounds before the boss shows (the level's exoskeleton waits)
 *          evidence n first last     n terminals ("thing evidence"), wiped between first and last seconds
 *          crust 1                   the moon's open ground is crust over glowing rock (Level makes it panes)
 *          cuts s / pulses s         every s seconds a line glows and fires (cuts break the crust)
 *          jetfuel n                 n bursts of the jets in the air (refilled on the ground)
 *          lava 1                    the shafts are channels of molten metal (a fall burns, one is pulled out)
 *          clock s                   during the machine's fight: s seconds before it grabs the hero
 *          waterfalls n              n falls beyond the north wall
 *          crowd n                   n people standing in the hall (they do not run: they come back anyway)
 *          pressureafter event s     the guard's squads only once the event has happened
 *        and things by name: "ally" (a turret of HAUSMEISTER's), "spin" (a centrifuge), "evidence".
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"
#include "Rig.h"
#include "Text.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace spaceopera
{

using namespace play;

namespace
{

// (Drawn at 2,400 m, the land was 200 m up a kilometre ahead and wider than the view: a ceiling, no rise out of the
// horizon. Now it runs on ahead, climbs and narrows into a thin arch.)
constexpr float RingRadius = 6000.0f;   ///< the ring as the game draws it (m; the story's is 1,000 km)
constexpr float RingWidth = 900.0f;     ///< its land from rim to rim (the widest level fits on it)
constexpr float RingRim = 140.0f;       ///< the rim walls' height (higher, they closed the sky at the sides)
constexpr float CutWarn = 2.4f;         ///< seconds a line glows before it fires
constexpr float CutBeam = 0.45f;        ///< seconds the beam stays
constexpr float SpinReach = 7.0f;       ///< a centrifuge's pull outward
constexpr float AllyReach = 18.0f;      ///< HAUSMEISTER's turrets
constexpr float AllyEvery = 0.7f;       ///< seconds between their shots
constexpr int   WaveRules = 4;          ///< low gravity, darkness, storm, quick champions

/** @brief Value noise that repeats every `period` cells along x. */
float noise( float x, float y, int period, unsigned seed )
{
    auto hash = [&]( int i, int j )
    {
        i = ( ( i % period ) + period ) % period;
        unsigned h = (unsigned)i * 374761393u + (unsigned)j * 668265263u + seed * 2246822519u;
        h = ( h ^ ( h >> 13 ) ) * 1274126177u;
        return ( ( h ^ ( h >> 16 ) ) & 0xffff ) / 65535.0f;
    };
    int   i = (int)floorf( x ), j = (int)floorf( y );
    float fx = x - i, fy = y - j;
    fx = fx * fx * ( 3.0f - 2.0f * fx ), fy = fy * fy * ( 3.0f - 2.0f * fy );
    float a = hash( i, j ), b = hash( i + 1, j ), c = hash( i, j + 1 ), d = hash( i + 1, j + 1 );
    return a + ( b - a ) * fx + ( c - a ) * fy + ( a - b - c + d ) * fx * fy;
}

/** @brief Four octaves of it. */
float fbm( float x, float y, int period, unsigned seed )
{
    float sum = 0.0f, amp = 0.5f;
    for ( int o = 0; o < 4; ++o, x *= 2.0f, y *= 2.0f, period *= 2, amp *= 0.5f )
        sum += amp * noise( x, y, period, seed + o );
    return sum / 0.9375f;
}

}   // namespace

void Game::buildRing()
{
    // The land: seas and coasts, fields in patches, forests, rivers, towns (alpha marks them: they glitter at night),
    // and along both edges the rim walls' grey. It repeats eight times round the ring.
    const int W = 2048, H = 512;
    Image     img = GenImageColor( W, H, BLANK );
    Color    *px = (Color *)img.data;
    for ( int y = 0; y < H; ++y )
        for ( int x = 0; x < W; ++x )
        {
            float u = x / (float)W * 16.0f, v = y / (float)H * 4.0f;
            Color c;
            if ( y > H * 0.94f )
            {
                float g = 46.0f + 22.0f * noise( u * 8.0f, v * 30.0f, 128, 9 );
                c = { (unsigned char)g, (unsigned char)( g * 1.06f ), (unsigned char)( g * 1.2f ), 0 };
                px[y * W + x] = c;
                continue;
            }
            float    sea = fbm( u, v, 16, 1 ), wood = fbm( u * 3.0f, v * 3.0f, 48, 2 ),
                     town = fbm( u * 2.0f, v * 2.0f, 32, 3 );
            float    river = fabsf( fbm( u * 1.5f, v * 1.5f, 24, 4 ) - 0.5f );
            unsigned field = (unsigned)( x / 14 ) * 2654435761u ^ (unsigned)( y / 10 ) * 40503u;
            field = ( field ^ ( field >> 15 ) ) * 2246822519u;
            if ( sea < 0.36f )
                c = Color{ 30, 62, 98, 0 };
            else if ( sea < 0.385f )
                c = Color{ 190, 176, 130, 0 };
            else if ( river < 0.012f )
                c = Color{ 50, 90, 120, 0 };
            else if ( town > 0.68f )
            {
                bool  block = ( ( x / 3 ) + ( y / 3 ) ) % 4 != 0;
                float g = block ? 150.0f + 40.0f * ( ( field >> 7 ) % 3 ) * 0.5f : 70.0f;
                c = { (unsigned char)g, (unsigned char)( g * 0.97f ), (unsigned char)( g * 0.92f ), 255 };
            }
            else if ( wood > 0.56f )
            {
                float g = 0.7f + 0.3f * noise( u * 40.0f, v * 40.0f, 640, 5 );
                c = { (unsigned char)( 38 * g ), (unsigned char)( 78 * g ), (unsigned char)( 40 * g ), 0 };
            }
            else
            {
                static const Color Fields[6] = { { 96, 132, 60, 0 },  { 120, 150, 70, 0 }, { 170, 160, 80, 0 },
                                                 { 140, 110, 70, 0 }, { 86, 120, 70, 0 },  { 150, 160, 96, 0 } };
                c = Fields[( field >> 9 ) % 6];
            }
            px[y * W + x] = c;
        }
    m_ringTex = LoadTextureFromImage( img );
    UnloadImage( img );
    GenTextureMipmaps( &m_ringTex );
    SetTextureFilter( m_ringTex, TEXTURE_FILTER_TRILINEAR );
    SetTextureWrap( m_ringTex, TEXTURE_WRAP_REPEAT );

    // The band round its axis (east to west through the level's middle), its bottom just under the level, and the
    // rim walls at both edges. It rises towards the north and the south, so it hangs as much lower as it rises over
    // half the level: a long level stands on it, not in it (the Sterbegaerten's 400 m stood up to 8 m deep in its
    // land, which filled the picture).
    const int                   N = 720, M = 4;
    float                       cx = m_level.cols() * CellSize * 0.5f, cz = m_level.rows() * CellSize * 0.5f;
    float                       cy = sqrtf( RingRadius * RingRadius - cz * cz ) - 0.8f;
    std::vector<float>          pos, nor, uv;
    std::vector<unsigned short> idx;
    auto                        vertex = [&]( Vector3 p, Vector3 n, float tu, float tv )
    {
        pos.insert( pos.end(), { p.x, p.y, p.z } );
        nor.insert( nor.end(), { n.x, n.y, n.z } );
        uv.insert( uv.end(), { tu, tv } );
        return (unsigned short)( pos.size() / 3 - 1 );
    };
    auto strip = [&]( auto point, auto normal, float v0, float v1, int across )
    {
        unsigned short first = (unsigned short)( pos.size() / 3 );
        for ( int i = 0; i <= N; ++i )
            for ( int j = 0; j <= across; ++j )
            {
                float a = 2.0f * PI * i / N, f = (float)j / across;
                vertex( point( a, f ), normal( a ), 8.0f * i / N, v0 + ( v1 - v0 ) * f );
            }
        for ( int i = 0; i < N; ++i )
            for ( int j = 0; j < across; ++j )
            {
                unsigned short a = first + i * ( across + 1 ) + j, b = a + across + 1;
                idx.insert( idx.end(), { a, b, (unsigned short)( a + 1 ), (unsigned short)( a + 1 ), b,
                                         (unsigned short)( b + 1 ) } );
            }
    };
    strip(
        [&]( float a, float f )
        {
            return Vector3{ cx - RingWidth * 0.5f + RingWidth * f, cy - RingRadius * cosf( a ),
                            cz - RingRadius * sinf( a ) };
        },
        []( float a ) { return Vector3{ 0.0f, cosf( a ), sinf( a ) }; }, 0.0f, 0.93f, M );
    for ( float side : { -1.0f, 1.0f } )
        strip(
            [&]( float a, float f )
            {
                float r = RingRadius - RingRim * f;
                return Vector3{ cx + side * RingWidth * 0.5f, cy - r * cosf( a ), cz - r * sinf( a ) };
            },
            [&]( float ) { return Vector3{ -side, 0.0f, 0.0f }; }, 0.95f, 1.0f, 1 );
    Mesh mesh = {};
    mesh.vertexCount = (int)pos.size() / 3;
    mesh.triangleCount = (int)idx.size() / 3;
    mesh.vertices = (float *)RL_MALLOC( pos.size() * sizeof( float ) );
    mesh.normals = (float *)RL_MALLOC( nor.size() * sizeof( float ) );
    mesh.texcoords = (float *)RL_MALLOC( uv.size() * sizeof( float ) );
    mesh.indices = (unsigned short *)RL_MALLOC( idx.size() * sizeof( unsigned short ) );
    memcpy( mesh.vertices, pos.data(), pos.size() * sizeof( float ) );
    memcpy( mesh.normals, nor.data(), nor.size() * sizeof( float ) );
    memcpy( mesh.texcoords, uv.data(), uv.size() * sizeof( float ) );
    memcpy( mesh.indices, idx.data(), idx.size() * sizeof( unsigned short ) );
    UploadMesh( &mesh, false );
    m_ringMesh = mesh;
}

void Game::loadAct34()
{
    if ( m_level.m_view == "ring" ) buildRing();
    m_vistaT = m_vistaMax = m_level.num( "vista" );
    m_crust = m_level.m_keys.count( "crust" ) > 0;
    m_fuel = m_fuelMax = m_level.num( "jetfuel" );
    m_cutTimer = m_level.num( "cuts", m_level.num( "pulses" ) ) * 0.6f;
    m_grabClock = m_level.num( "clock" );
    m_lavaSafe = m_pos;
    MeshBuilder unit;
    unit.box( place( { 0.0f, 0.0f, 0.0f } ), { 0.5f, 0.5f, 0.5f } );
    m_cutMesh = unit.build().front();

    // The rounds: the boss waits out of sight until the last one is over.
    m_waves = (int)m_level.num( "waves" );
    if ( m_waves > 0 )
    {
        int b = boss();
        if ( b >= 0 )
        {
            m_arenaAt = m_foes[b].m_home;
            m_foes[b].m_hidden = true;
            m_foes[b].m_awake = false;
        }
        else
            m_waves = 0;
    }
    m_baseGravity = m_level.m_gravity;

    // Things by name.
    int   evidence = (int)m_level.num( "evidence" );
    float first = m_level.num( "evidence", 1, 300.0f ), last = m_level.num( "evidence", 2, 900.0f );
    for ( const Thing &t : m_level.m_things )
    {
        if ( t.m_name == "ally" ) m_allies.push_back( { t.m_pos, rnd() * 6.28f, rnd() } );
        if ( t.m_name == "spin" )
        {
            m_spinners.push_back( t.m_pos );
            m_level.m_props.push_back( { t.m_pos, 1.4f } );
        }
        if ( t.m_name == "evidence" && (int)m_evidence.size() < evidence )
        {
            // A terminal standing free: the freight papers on it, until the cleaners get there.
            Usable u;
            u.m_kind = 2;
            u.m_row = t.m_row;
            u.m_col = t.m_col;
            u.m_out = { 0.0f, 0.0f, 1.0f };
            u.m_at = t.m_pos;
            u.m_pos = Vector3Add( t.m_pos, { 0.0f, 0.0f, 0.95f } );
            u.m_key = "ev." + std::to_string( m_evidence.size() + 1 );
            m_usables.push_back( u );
            m_level.m_props.push_back( { t.m_pos, 0.42f } );
            m_evidence.push_back( { (int)m_usables.size() - 1, 0.0f, false, false } );
        }
    }
    for ( size_t i = 0; i < m_evidence.size(); ++i )
        m_evidence[i].m_deadline =
            first + ( last - first ) * ( m_evidence.size() > 1 ? (float)i / ( m_evidence.size() - 1 ) : 0.0f );
    if ( !m_spinners.empty() )
    {
        MeshBuilder spin;
        spin.lathe( place( { 0.0f, 0.0f, 0.0f } ),
                    { { 0.0f, 0.0f }, { 1.3f, 0.0f }, { 1.4f, 0.3f }, { 1.3f, 1.4f }, { 0.4f, 1.6f }, { 0.0f, 1.6f } },
                    24 );
        for ( int k = 0; k < 4; ++k )
            spin.box( place( { 0.0f, 0.9f, 0.0f }, { 0.0f, k * PI * 0.5f, 0.0f } ), { 1.8f, 0.12f, 0.12f } );
        m_spinMesh = spin.build().front();
    }

    // Molten metal below the channels: glowing, a little below the floor.
    if ( m_level.m_keys.count( "lava" ) )
    {
        MeshBuilder lava;
        for ( int r = 0; r < m_level.rows(); ++r )
            for ( int c = 0; c < m_level.cols(); ++c )
                if ( m_level.cell( r, c ).m_kind == CellE::VOID )
                {
                    Vector3 p = m_level.centre( r, c );
                    lava.box( place( { p.x, -1.3f, p.z } ), { CellSize * 0.5f, 0.1f, CellSize * 0.5f } );
                }
        auto meshes = lava.build();
        if ( !meshes.empty() ) m_lavaMesh = meshes.front();
    }
    // Waterfalls beyond the north wall, spread along it.
    for ( int i = 0; i < (int)m_level.num( "waterfalls" ); ++i )
        m_falls.push_back(
            { ( i + 0.5f ) / m_level.num( "waterfalls" ) * m_level.cols() * CellSize, 0.0f, -14.0f - rnd() * 10.0f } );
    // The crowd: people standing in the halls (skinned where the bodies are), facing the middle.
    int crowd = (int)m_level.num( "crowd" );
    for ( int t = 0; t < crowd * 30 && crowd > 0; ++t )
    {
        int         r = (int)( rnd() * m_level.rows() ), c = (int)( rnd() * m_level.cols() );
        std::string in = m_level.word( "crowd" );
        ThemeE      where =
            in.find( ' ' ) == std::string::npos ? ThemeE::HALL : themeNamed( in.substr( in.find( ' ' ) + 1 ) );
        if ( m_level.theme( r, c ) != where || !m_level.walkable( r, c ) ||
             m_level.cell( r, c ).m_kind != CellE::FLOOR || m_blocked.count( r * m_level.cols() + c ) )
            continue;
        // Only in the open, never in a doorway or a corridor: nobody's standing may close the way.
        bool open = true;
        for ( int dr = -1; dr <= 1 && open; ++dr )
            for ( int dc = -1; dc <= 1 && open; ++dc )
                open = m_level.walkable( r + dr, c + dc ) && !m_blocked.count( ( r + dr ) * m_level.cols() + c + dc );
        Vector3 p = m_level.centre( r, c );
        for ( const Prop &pr : m_level.m_props )
            if ( Vector2Distance( { p.x, p.z }, { pr.m_pos.x, pr.m_pos.z } ) < pr.m_radius + 2.2f ) open = false;
        if ( !open ) continue;
        Person q = {};
        q.m_pos = Vector3Add( p, { ( rnd() - 0.5f ) * 0.8f, 0.0f, ( rnd() - 0.5f ) * 0.8f } );
        q.m_yaw = atan2f( m_level.cols() * CellSize * 0.5f - p.x, m_level.rows() * CellSize * 0.5f - p.z );
        q.m_look = (int)( rnd() * 3.0f ) == 0 ? 6 : ( rnd() < 0.5f ? 7 : 0 );
        q.m_sit = 0.0f;
        q.m_down = 0.0f;
        q.m_fear = -3;   // they do not run, but they look, and flinch (updateRefugees)
        dressPerson( q, (unsigned)( t * 977u + 31u ) );
        m_people.push_back( q );
        m_blocked.insert( r * m_level.cols() + c );
        if ( (int)m_people.size() >= crowd ) break;
    }
}

void Game::startWave()
{
    // A new rule each round (the arena's voice announces it), the gravity, the light, the weather set by it.
    m_level.m_gravity = m_baseGravity;
    m_level.m_dark = false;
    m_level.m_windStrength = 0.0f;
    m_level.m_gusts = 0.0f;
    m_waveRule = m_wave % WaveRules;
    if ( m_waveRule == 0 ) m_level.m_gravity = m_baseGravity * 0.35f;
    if ( m_waveRule == 1 ) m_level.m_dark = true;
    if ( m_waveRule == 2 )
    {
        m_level.m_windDir = { 1.0f, 0.0f, 0.0f };
        m_level.m_windStrength = 5.0f;
        m_level.m_gusts = 6.0f;
        m_gustTimer = 2.0f, m_gustT = 0.0f, m_gustState = 0;
    }
    m_waveT = 0.0f;
    m_waveFoes.clear();
    fire( "wave" + std::to_string( m_wave + 1 ) );
    // Citizens who die for fun and the arena's beasts, from its edges.
    int n = 3 + m_wave;
    for ( int i = 0; i < n; ++i )
        for ( int t = 0; t < 20; ++t )
        {
            float   a = rnd() * 6.283f, d = 7.0f + rnd() * 5.0f;
            Vector3 p = { m_arenaAt.x + cosf( a ) * d, 0.0f, m_arenaAt.z + sinf( a ) * d };
            float   floor = groundAt( p );
            if ( floor < -100.0f || floor > 100.0f ) continue;
            p.y = floor;
            FoeE kind = i % 3 == 2 ? FoeE::SAW : ( i % 2 ? FoeE::GUNNER : FoeE::BATON );
            spawnFoe( kind, p, true );
            Foe &f = m_foes.back();
            if ( kind == FoeE::SAW ) f.m_variant = 1;   // the beasts: hounds
            if ( m_waveRule == 3 || ( i == 0 && m_wave > 0 ) )
            {
                makeElite( f, 1 );
                if ( m_waveRule == 3 ) f.m_affix = 0;   // quick
            }
            m_waveFoes.push_back( f.m_id );
            explode( Vector3Add( p, { 0.0f, 0.3f, 0.0f } ), 0.4f );
            break;
        }
    m_audio.play( SfxE::KLAXON, m_arenaAt, m_camTarget, 0.9f );
}

void Game::updateAct34( float dt, GameInput &in )
{
    int r, c;
    m_level.cellOf( m_pos, r, c );
    m_vistaT = fmaxf( 0.0f, m_vistaT - dt );

    // The shield turns: its shadow sweeps across the station and back.
    if ( m_level.m_meter == "shadow" )
    {
        float period = m_level.num( "shadow", 0, 80.0f ), width = m_level.num( "shadow", 1, 14.0f );
        float centre =
            m_level.cols() * 0.5f + ( m_level.cols() * 0.5f - width * 0.5f ) * sinf( m_time * 2.0f * PI / period );
        m_level.m_bandHot = (int)( centre - width * 0.5f );
        m_level.m_bandCold = (int)( centre + width * 0.5f );
        if ( m_metering && !m_dead && rnd() < dt * 30.0f )
            m_particles.push_back(
                { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 4.0f, rnd() * 2.0f, ( rnd() - 0.5f ) * 4.0f } ),
                  { 0.0f, 1.5f + rnd(), 0.0f },
                  0.8f,
                  0.8f,
                  0.03f,
                  { 255, 170, 70, 255 },
                  3 } );
        if ( m_metering )
            m_flashes.push_back(
                { Vector3Add( m_pos, { 0.0f, 6.0f, 0.0f } ), { 40.0f, 18.0f, 6.0f }, 14.0f, dt * 1.5f, dt * 1.5f } );
    }

    // The jets: in the air a burst up and forward, as long as there is fuel; on the ground they fill again.
    if ( m_fuelMax > 0.0f && !m_dead )
    {
        if ( !m_ground && in.m_jump && m_fuel >= 1.0f )
        {
            m_fuel -= 1.0f;
            m_vel.y = fmaxf( m_vel.y, 5.2f );
            m_vel.x += in.m_move.x * 3.5f;
            m_vel.z += in.m_move.y * 3.5f;
            for ( int i = 0; i < 16; ++i )
                m_particles.push_back(
                    { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.4f, 0.6f, ( rnd() - 0.5f ) * 0.4f } ),
                      { ( rnd() - 0.5f ) * 2.0f, -4.0f - rnd() * 3.0f, ( rnd() - 0.5f ) * 2.0f },
                      0.4f,
                      0.4f,
                      0.05f,
                      { 160, 200, 255, 230 },
                      2 } );
            m_audio.play( SfxE::JUMP, m_pos, m_camTarget, 0.8f );
            in.m_jump = false;
            fire( "jet" );
        }
        if ( m_ground ) m_fuel = fminf( m_fuelMax, m_fuel + dt * 0.6f );
    }

    // The rounds.
    if ( m_waves > 0 && m_wave < m_waves )
    {
        if ( !m_wavesOn && Vector3Distance( m_pos, m_arenaAt ) < 15.0f )
        {
            m_wavesOn = true;
            startWave();
        }
        else if ( m_wavesOn )
        {
            m_waveT += dt;
            bool left = false;
            for ( int id : m_waveFoes ) left = left || ( id < (int)m_foes.size() && m_foes[id].m_alive );
            if ( !left && m_waveT > 2.0f )
            {
                if ( ++m_wave < m_waves )
                    startWave();
                else
                {
                    // The last round is won: the rules lifted, and he steps into the sand.
                    m_level.m_gravity = m_baseGravity;
                    m_level.m_dark = false;
                    m_level.m_windStrength = 0.0f;
                    m_level.m_gusts = 0.0f;
                    m_waveRule = -1;
                    int b = -1;
                    for ( size_t i = 0; i < m_foes.size(); ++i )
                        if ( m_foes[i].m_kind == FoeE::TAXATOR && m_foes[i].m_alive ) b = (int)i;
                    if ( b >= 0 )
                    {
                        m_foes[b].m_hidden = false;
                        m_foes[b].m_awake = true;
                        explode( Vector3Add( m_foes[b].m_pos, { 0.0f, 1.0f, 0.0f } ), 1.0f );
                    }
                    fire( "wavesdone" );
                }
            }
        }
    }

    // Evidence: what was read is kept; what the cleaners reach first is wiped.
    if ( !m_evidence.empty() )
    {
        for ( size_t i = 0; i < m_evidence.size(); ++i )
        {
            Evidence &e = m_evidence[i];
            Usable   &u = m_usables[e.m_usable];
            if ( !e.m_read && m_reading == u.m_key && u.m_key != "ev.wiped" )
            {
                e.m_read = true;
                fire( "evread" );
            }
            if ( !e.m_read && !e.m_wiped && m_time > e.m_deadline )
            {
                e.m_wiped = true;
                u.m_key = "ev.wiped";
                m_toast = tr( "hud.evwiped" );
                m_toastColor = { 255, 120, 90, 255 };
                m_toastTimer = 3.0f;
                fire( "evwiped" );
            }
        }
        if ( m_fired.count( "bossdead" ) && !m_fired.count( "evcounted" ) )
        {
            int read = 0;
            for ( const Evidence &e : m_evidence ) read += e.m_read ? 1 : 0;
            fire( "evcounted" );
            runEvent( read == (int)m_evidence.size()
                          ? "evidence.full"
                          : ( read * 2 >= (int)m_evidence.size() ? "evidence.most" : "evidence.few" ) );
        }
    }

    // Lines that glow, then fire: the cutters slice the crust (whoever stands on it falls), the Mill's pulses only burn.
    float every = m_level.num( "cuts", m_level.num( "pulses" ) );
    if ( every > 0.0f && !m_finished && !m_dead && m_scene == 0 )
    {
        m_cutTimer -= dt;
        if ( m_cutTimer <= 0.0f )
        {
            m_cutTimer = every * ( 0.8f + 0.4f * rnd() );
            Cut  cut;
            bool alongX = rnd() < 0.5f;
            int  off = ( rnd() < 0.5f ? -1 : 1 ) * ( 1 + (int)( rnd() * 4.0f ) );
            cut.m_dr = alongX ? 0 : 1;
            cut.m_dc = alongX ? 1 : 0;
            cut.m_len = 14;
            cut.m_r0 = alongX ? r + off : r - 7;
            cut.m_c0 = alongX ? c - 7 : c + off;
            m_cuts.push_back( cut );
            fire( "cutwarn" );
        }
    }
    for ( Cut &k : m_cuts )
    {
        k.m_t += dt;
        if ( k.m_t >= CutWarn && !k.m_fired )
        {
            k.m_fired = true;
            m_audio.play( SfxE::BURST, m_level.centre( k.m_r0 + k.m_dr * 7, k.m_c0 + k.m_dc * 7 ), m_camTarget, 1.0f );
            m_shake = fmaxf( m_shake, 0.35f );
            for ( int i = 0; i < k.m_len; ++i )
            {
                int     cr = k.m_r0 + k.m_dr * i, cc = k.m_c0 + k.m_dc * i;
                Vector3 p = m_level.centre( cr, cc );
                if ( m_crust && m_level.m_keys.count( "cuts" ) ) crackIce( p, 1.0f, 99.0f );
                sparks( Vector3Add( p, { 0.0f, 0.2f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 6, { 255, 140, 60, 255 }, 5.0f );
                if ( cr == r && cc == c && m_pos.y < 2.5f && !m_dead )
                {
                    m_hurtBy = "laser";
                    // A share of the hero's health: the place's depth must not multiply it again.
                    hurtHero( hpMax() * 0.24f / m_areaHit, { 0.0f, 3.0f, 0.0f }, p );
                }
                for ( Foe &f : m_foes )
                {
                    if ( !f.m_alive || f.m_hidden || f.m_kind == FoeE::TAXATOR ) continue;
                    int fr, fc;
                    m_level.cellOf( f.m_pos, fr, fc );
                    if ( fr == cr && fc == cc ) hurtFoe( f, 400.0f, { 0.0f, 1.0f, 0.0f } );
                }
            }
        }
    }
    m_cuts.erase(
        std::remove_if( m_cuts.begin(), m_cuts.end(), []( const Cut &k ) { return k.m_t > CutWarn + CutBeam; } ),
        m_cuts.end() );

    // Cut crust cools and closes again (not under anyone's feet).
    bool grown = false;
    for ( auto it = m_regrow.begin(); it != m_regrow.end(); )
    {
        it->m_t -= dt;
        Vector3 p = m_level.centre( it->m_r, it->m_c );
        if ( it->m_t > 0.0f || Vector2Distance( { p.x, p.z }, { m_pos.x, m_pos.z } ) < 1.6f )
        {
            ++it;
            continue;
        }
        m_level.setGlass( it->m_r, it->m_c );
        m_level.m_glass.push_back( { it->m_r, it->m_c } );
        m_iceHp[it->m_r * 1000 + it->m_c] = 2.0f;
        it = m_regrow.erase( it );
        grown = true;
    }
    if ( grown )
    {
        rebuildGlass();
        m_planTimer = 0.0f;
    }

    // Centrifuges fling outward what comes near; their blades cut.
    for ( const Vector3 &sp : m_spinners )
    {
        Vector3 out = { m_pos.x - sp.x, 0.0f, m_pos.z - sp.z };
        float   d = Vector3Length( out );
        if ( d < SpinReach && d > 0.1f )
        {
            m_vel = Vector3Add( m_vel, Vector3Scale( out, 7.0f * dt / d * ( 1.0f - d / SpinReach ) * 3.0f ) );
            if ( d < 2.1f && !m_dead )
            {
                m_hurtBy = "centrifuge";
                drainHero( 20.0f * harm( m_difficulty ) * dt );
                fire( "spun" );
            }
        }
        for ( Foe &f : m_foes )
        {
            if ( !f.m_alive || f.m_hidden || f.m_kind == FoeE::TAXATOR || f.m_kind == FoeE::DRONE ) continue;
            Vector3 fo = { f.m_pos.x - sp.x, 0.0f, f.m_pos.z - sp.z };
            float   fd = Vector3Length( fo );
            if ( fd < SpinReach && fd > 0.1f ) f.m_pos = Vector3Add( f.m_pos, Vector3Scale( fo, 2.5f * dt / fd ) );
        }
    }

    // HAUSMEISTER's turrets: a dumb machine shooting at machines with people in them.
    for ( Ally &a : m_allies )
    {
        a.m_timer -= dt;
        int   best = -1;
        float bestD = AllyReach;
        for ( size_t i = 0; i < m_foes.size(); ++i )
        {
            const Foe &f = m_foes[i];
            if ( !f.m_alive || f.m_hidden || !f.m_awake ) continue;
            float d = Vector3Distance( f.m_pos, a.m_pos );
            if ( d < bestD &&
                 los( Vector3Add( a.m_pos, { 0.0f, 1.2f, 0.0f } ), Vector3Add( f.m_pos, { 0.0f, 1.0f, 0.0f } ) ) )
                bestD = d, best = (int)i;
        }
        if ( best < 0 ) continue;
        Foe  &f = m_foes[best];
        float want = atan2f( f.m_pos.x - a.m_pos.x, f.m_pos.z - a.m_pos.z );
        a.m_yaw += Clamp( angleTo( a.m_yaw, want ), -4.0f * dt, 4.0f * dt );
        if ( a.m_timer > 0.0f ) continue;
        a.m_timer = AllyEvery;
        Vector3 from = Vector3Add( a.m_pos, { sinf( a.m_yaw ) * 0.9f, 1.25f, cosf( a.m_yaw ) * 0.9f } );
        Vector3 to = Vector3Add( f.m_pos, { 0.0f, 1.0f, 0.0f } );
        m_tracers.push_back( { from, to, 0.08f } );
        m_flashes.push_back( { from, { 3.0f, 12.0f, 5.0f }, 3.0f, 0.06f, 0.06f } );
        m_audio.play( SfxE::ZAP, from, m_camTarget, 0.5f );
        hurtFoe( f, 30.0f * m_areaHit, Vector3Normalize( Vector3Subtract( to, from ) ) );
    }

    // Molten metal: a fall burns, and one is pulled out where one last stood.
    if ( m_lavaMesh.vertexCount > 0 && !m_dead )
    {
        bool solid = m_ground;
        for ( int dr = -1; dr <= 1 && solid; ++dr )
            for ( int dc = -1; dc <= 1 && solid; ++dc ) solid = m_level.cell( r + dr, c + dc ).m_kind != CellE::VOID;
        if ( solid ) m_lavaSafe = m_pos;
        if ( m_pos.y < -0.8f )
        {
            for ( int k = 0; k < 24; ++k )
                m_particles.push_back(
                    { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 1.2f, 0.8f, ( rnd() - 0.5f ) * 1.2f } ),
                      { ( rnd() - 0.5f ) * 3.0f, 3.0f + rnd() * 4.0f, ( rnd() - 0.5f ) * 3.0f },
                      0.9f,
                      0.9f,
                      0.05f,
                      { 255, 150, 50, 255 },
                      3 } );
            m_audio.play( SfxE::SIZZLE, m_pos, m_camTarget, 1.0f );
            m_hurtBy = "lava";
            // The autopilot went in soon after a jump: no more jumps from where it took off (as after a fall; it
            // jumped short over the same lava again and again, Kasimir eight times in Lohe).
            if ( m_autoplay && m_jumpFrom >= 0 && m_time - m_jumpTime < 8.0f ) m_noJump.insert( m_jumpFrom );
            drainHero( hpMax() * 0.32f );
            m_pos = m_lavaSafe;
            m_vel = { 0.0f, 0.0f, 0.0f };
            m_invuln = fmaxf( m_invuln, 1.2f );
            m_path.clear();
            m_planTimer = 0.0f;
            fire( "burnt" );
        }
    }

    // The catcher's grip: while it fights, its clock runs; at nought it has the container.
    if ( m_grabClock > 0.0f )
    {
        if ( tugFighting() && !m_dead )
        {
            if ( m_grabT <= 0.0f ) m_grabT = m_grabClock;
            m_grabT -= dt;
            if ( m_grabT <= 0.0f )
            {
                m_hurtBy = "grabbed";
                fire( "grabbed" );
                hurtHero( hpMax() * 5.0f, { 0.0f, 4.0f, 0.0f }, m_tugPos );
                m_grabT = m_grabClock;
            }
        }
    }

    // Waterfalls drifting sideways with the spin as they fall.
    for ( const Vector3 &f : m_falls )
        if ( fabsf( f.x - m_pos.x ) < 70.0f && rnd() < dt * 50.0f )
            m_particles.push_back( { { f.x + ( rnd() - 0.5f ) * 6.0f, 48.0f, f.z + ( rnd() - 0.5f ) * 3.0f },
                                     { 3.5f, -16.0f - rnd() * 4.0f, 0.0f },
                                     3.0f,
                                     3.0f,
                                     0.5f + rnd() * 0.4f,
                                     { 225, 235, 245, 110 },
                                     1 } );

    // The guard's squads, once their moment has come.
    std::string after = m_level.word( "pressureafter" );
    if ( !after.empty() && m_level.m_pressure <= 0.0f )
    {
        std::string event = after.substr( 0, after.find( ' ' ) );
        if ( m_fired.count( event ) )
        {
            m_level.m_pressure = m_level.num( "pressureafter", 1, 45.0f );
            m_pressureT = m_level.m_pressure * 0.8f;
        }
    }
}

void Game::applyVista( float )
{
    // Arriving: the camera stands by the hero and looks up along the land into the sky, then sinks to the game's.
    if ( m_vistaT <= 0.0f || m_vistaMax <= 0.0f ) return;
    float w = Clamp( m_vistaT / 3.0f, 0.0f, 1.0f );
    w = w * w * ( 3.0f - 2.0f * w );
    Vector3 eye = Vector3Add( m_pos, { 0.0f, 4.6f, 6.5f } );   // over the roofs and the lamps
    Vector3 look = Vector3Add( eye, { 0.0f, sinf( 0.42f ) * 100.0f, -cosf( 0.42f ) * 100.0f } );
    m_cam.position = Vector3Lerp( m_cam.position, eye, w );
    m_cam.target = Vector3Lerp( m_cam.target, look, w );
    m_cam.up = { 0.0f, 1.0f, 0.0f };
    m_cam.fovy = 38.0f + 22.0f * w;
}

void Game::poseAct34( std::vector<Drawable> &out )
{
    Drawable d;
    d.m_spotShadow = d.m_sunShadow = false;
    // The crust: slabs of rock over the glow.
    if ( m_crust && m_glassMesh.vertexCount > 0 )
    {
        Drawable s;
        s.m_mesh = m_glassMesh;
        s.m_surface = m_ownSurfaces.count( "regolith" ) ? m_ownSurfaces.at( "regolith" ) : S_ROCK;
        s.m_transform = MatrixIdentity();
        out.push_back( s );
    }
    // The lines: a red glow that grows, then a white-hot beam.
    for ( const Cut &k : m_cuts )
    {
        bool  beam = k.m_fired;
        float pulse = beam ? 1.0f : 0.4f + 0.6f * fabsf( sinf( k.m_t * ( 4.0f + 6.0f * k.m_t / CutWarn ) ) );
        for ( int i = 0; i < k.m_len; ++i )
        {
            Vector3 p = m_level.centre( k.m_r0 + k.m_dr * i, k.m_c0 + k.m_dc * i );
            float   sx = k.m_dc ? CellSize : 0.12f, sz = k.m_dr ? CellSize : 0.12f;
            d.m_mesh = m_cutMesh;
            d.m_surface = S_GLOW;
            d.m_transform = MatrixMultiply( MatrixScale( sx * ( beam ? 1.0f : 1.0f ), beam ? 3.0f : 0.05f, sz ),
                                            MatrixTranslate( p.x, beam ? 1.5f : 0.06f, p.z ) );
            d.m_emissive =
                beam ? Vector4{ 30.0f, 18.0f, 10.0f, 0.0f } : Vector4{ 8.0f * pulse, 0.6f * pulse, 0.3f * pulse, 0.0f };
            out.push_back( d );
        }
    }
    // Centrifuges turning.
    for ( const Vector3 &sp : m_spinners )
    {
        Drawable s;
        s.m_mesh = m_spinMesh;
        s.m_surface = S_RUST;
        s.m_transform = MatrixMultiply( MatrixRotateY( m_time * 3.0f ), MatrixTranslate( sp.x, 0.0f, sp.z ) );
        out.push_back( s );
    }
    // HAUSMEISTER's turrets: a green lamp on each.
    for ( const Ally &a : m_allies )
    {
        Drawable t;
        t.m_mesh = m_turretBase;
        t.m_surface = S_HAZARD;
        t.m_transform = MatrixTranslate( a.m_pos.x, a.m_pos.y, a.m_pos.z );
        out.push_back( t );
        t.m_mesh = m_turretHead;
        t.m_surface = S_METAL;
        t.m_transform =
            MatrixMultiply( MatrixRotateY( a.m_yaw ), MatrixTranslate( a.m_pos.x, a.m_pos.y + 1.25f, a.m_pos.z ) );
        out.push_back( t );
        d.m_mesh = m_cutMesh;
        d.m_surface = S_GLOW;
        d.m_transform = MatrixMultiply( MatrixScale( 0.16f, 0.16f, 0.16f ),
                                        MatrixTranslate( a.m_pos.x, a.m_pos.y + 1.7f, a.m_pos.z ) );
        d.m_emissive = { 0.4f, 6.0f, 1.2f, 0.0f };
        out.push_back( d );
    }
    // Molten metal.
    if ( m_lavaMesh.vertexCount > 0 )
    {
        Drawable l;
        l.m_mesh = m_lavaMesh;
        l.m_surface = S_GLOW;
        l.m_transform = MatrixIdentity();
        float pulse = 0.85f + 0.15f * sinf( m_time * 1.3f );
        l.m_emissive = { 9.0f * pulse, 2.6f * pulse, 0.45f * pulse, 0.0f };
        l.m_spotShadow = l.m_sunShadow = false;
        out.push_back( l );
    }
}

void Game::drawAct34( Gfx &gfx )
{
    if ( m_ringMesh.vertexCount > 0 )
    {
        // The sky's colour as the sky shader shows it (linear, 0.8 of it), for the haze on the land: a picture's mean
        // colour where the level has one (a free port's night, a habitat's own sky), else its colour.
        if ( m_ringSky.x < 0.0f )
        {
            Vector3 sky = m_level.m_skyColor.x >= 0.0f ? m_level.m_skyColor : Vector3{ 0.42f, 0.58f, 0.85f };
            sky = { powf( sky.x, 2.2f ), powf( sky.y, 2.2f ), powf( sky.z, 2.2f ) };
            if ( m_skyTex.id > 0 )
            {
                Image img = LoadImageFromTexture( m_skyTex );
                ImageFormat( &img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 );
                ImageResize( &img, 32, 16 );
                Color  *px = (Color *)img.data;
                Vector3 sum = { 0.0f, 0.0f, 0.0f };
                for ( int i = 0; i < 32 * 16; ++i )
                    sum = Vector3Add( sum, { powf( px[i].r / 255.0f, 2.2f ), powf( px[i].g / 255.0f, 2.2f ),
                                             powf( px[i].b / 255.0f, 2.2f ) } );
                sky = Vector3Scale( sum, 1.0f / ( 32 * 16 ) );
                UnloadImage( img );
            }
            m_ringSky = Vector3Scale( sky, 0.8f );
        }
        gfx.ring( m_ringMesh, m_ringTex, RingRadius, m_ringSky );
    }
}

void Game::drawAct34Hud( int w, int h, float s )
{
    auto text = [&]( const char *t, float x, float y, float size, Color col, bool centre )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        Vector2 p = { centre ? x - m.x * 0.5f : x, y };
        DrawTextEx( m_font, t, { p.x + 2.0f * s, p.y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( col.a * 0.8f ) } );
        DrawTextEx( m_font, t, p, size * s, 1.0f, col );
    };
    // The round and its rule: big when it begins, then small at the top.
    if ( m_wavesOn && m_wave < m_waves && m_waveRule >= 0 )
    {
        float       big = Clamp( 1.0f - ( m_waveT - 2.5f ), 0.0f, 1.0f );
        std::string round = TextFormat( tr( "hud.round" ), m_wave + 1, m_waves );
        std::string rule = tr( "rule." + std::to_string( m_waveRule ) );
        if ( big > 0.0f )
        {
            text( round.c_str(), w * 0.5f, h * 0.3f, 30, { 255, 200, 130, (unsigned char)( 255 * big ) }, true );
            text( rule.c_str(), w * 0.5f, h * 0.3f + 40 * s, 52, { 245, 240, 230, (unsigned char)( 255 * big ) },
                  true );
        }
        text( ( round + "  ·  " + rule ).c_str(), w * 0.5f, 92.0f * s, 22, { 255, 210, 160, 220 }, true );
    }
    // The evidence: kept, and how long until the next one is wiped.
    if ( !m_evidence.empty() && !m_finished )
    {
        int   read = 0;
        float next = -1.0f;
        for ( const Evidence &e : m_evidence )
        {
            read += e.m_read ? 1 : 0;
            if ( !e.m_read && !e.m_wiped && ( next < 0.0f || e.m_deadline < next ) ) next = e.m_deadline;
        }
        std::string line = TextFormat( tr( "hud.evidence" ), read, (int)m_evidence.size() );
        if ( next > 0.0f )
        {
            int left = (int)fmaxf( 0.0f, next - m_time );
            line += TextFormat( tr( "hud.evnext" ), left / 60, left % 60 );
        }
        text( line.c_str(), w - 40.0f * s - MeasureTextEx( m_font, line.c_str(), 20 * s, 1.0f ).x, 40.0f * s, 20,
              { 220, 230, 245, 230 }, false );
    }
    // The shield's shadow: the station from west to east, the shade dark, where the hero is.
    if ( m_level.m_meter == "shadow" && !m_finished )
    {
        float bw = 420.0f * s, bx = w * 0.5f - bw * 0.5f, by = 132.0f * s, cols = (float)m_level.cols();
        DrawRectangle( (int)bx, (int)by, (int)bw, (int)( 12 * s ), { 255, 140, 50, 170 } );
        float a = Clamp( m_level.m_bandHot / cols, 0.0f, 1.0f ), b = Clamp( m_level.m_bandCold / cols, 0.0f, 1.0f );
        DrawRectangle( (int)( bx + bw * a ), (int)by, (int)( bw * ( b - a ) ), (int)( 12 * s ), { 20, 24, 40, 230 } );
        float hx = bx + bw * Clamp( m_pos.x / ( cols * CellSize ), 0.0f, 1.0f );
        DrawTriangle( { hx, by + 14 * s }, { hx - 7 * s, by + 26 * s }, { hx + 7 * s, by + 26 * s },
                      { 240, 240, 240, 240 } );
        text( tr( "hud.shadow" ), w * 0.5f, by - 26.0f * s, 18, { 230, 220, 210, 220 }, true );
    }
    // The jets' fuel.
    if ( m_fuelMax > 0.0f )
    {
        float x = 34.0f * s, y = h - 236.0f * s;
        text( tr( "hud.jets" ), x, y - 22.0f * s, 18, { 200, 220, 245, 220 }, false );
        for ( int i = 0; i < (int)m_fuelMax; ++i )
        {
            float fill = Clamp( m_fuel - i, 0.0f, 1.0f );
            DrawRectangle( (int)( x + i * 46 * s ), (int)y, (int)( 40 * s ), (int)( 9 * s ), { 20, 30, 45, 200 } );
            DrawRectangle( (int)( x + i * 46 * s ), (int)y, (int)( 40 * s * fill ), (int)( 9 * s ),
                           fill >= 1.0f ? Color{ 140, 200, 255, 255 } : Color{ 80, 110, 150, 255 } );
        }
    }
    // The catcher's clock.
    if ( m_grabClock > 0.0f && tugFighting() )
    {
        int left = (int)fmaxf( 0.0f, m_grabT );
        text( TextFormat( tr( "hud.grab" ), left / 60, left % 60 ), w * 0.5f, 100.0f * s, 24,
              left < 20 && fmodf( m_time, 0.6f ) < 0.3f ? Color{ 255, 80, 60, 255 } : Color{ 255, 200, 150, 235 },
              true );
    }
    // A line about to fire near the hero.
    for ( const Cut &k : m_cuts )
        if ( !k.m_fired )
        {
            text( tr( m_level.m_keys.count( "cuts" ) ? "hud.cutwarn" : "hud.pulsewarn" ), w * 0.5f, h * 0.2f, 30,
                  { 255, 120, 80, (unsigned char)( 180 + 75 * sinf( m_time * 12.0f ) ) }, true );
            break;
        }
}

}   // namespace spaceopera
