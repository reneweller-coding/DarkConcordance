/**
 * @file GameWorld.cpp
 * @brief What a place does around the fight: the birds over a drum's market that never land; out on a station's skin
 *        the suit's air, the muffled sound, and the star's radiation storms; the gardens' ponds and the air out in
 *        the open (pollen, dust, snow, mist).
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace spaceopera
{

using namespace play;

void Game::loadWorld()
{
    m_flareTimer = m_level.m_flares * 0.6f;   // the first storm comes a little sooner
    // Balls of water where the level has them; under water one holds one's breath (shorter than a suit's air).
    for ( const Vector4 &w : m_level.m_water )
    {
        Vector3 c = m_level.centre( (int)w.x, (int)w.y );
        m_waters.push_back( { { c.x, c.y + w.z * 0.7f, c.z }, w.z, m_waters.size() % 2 ? -1.0f : 1.0f } );
    }
    buildPonds();
    if ( !m_waters.empty() || m_level.m_water.size() )
    {
        m_waterMesh = GenMeshSphere( 1.0f, 40, 40 );
        if ( !m_level.m_vacuum ) m_oxygenMax = m_oxygen = 35.0f;
    }
    m_oxygenMax *= 1.0f + stat( StatE::OXYGEN ) / 100.0f;   // a harness, a lung implant
    m_oxygen = m_oxygenMax;
    // Air posts out on the skin: a mast with a blue collar; near it the suit fills up.
    for ( const Marker &m : m_level.m_markers )
        if ( m.m_kind == ';' )
        {
            m_airPosts.push_back( m.m_pos );
            m_level.m_props.push_back( { m.m_pos, 0.35f } );
        }
    if ( !m_airPosts.empty() )
    {
        MeshBuilder post;
        post.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.25f, 0.3f, 12 );
        post.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.1f, 2.2f, 10 );
        post.roundedBox( place( { 0.0f, 2.3f, 0.0f } ), { 0.22f, 0.15f, 0.22f }, 0.05f, 2 );
        m_postMesh = post.build().front();
        MeshBuilder ring;
        ring.cylinder( place( { 0.0f, 1.5f, 0.0f } ), 0.18f, 0.25f, 14 );
        m_postRing = ring.build().front();
    }
    // Birds hatched in the drum: they circle above the roofs and never come down.
    if ( m_level.m_birds > 0 )
    {
        MeshBuilder body;
        body.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 0.06f, 0.05f, 0.16f }, 0.04f, 2 );
        body.box( place( { 0.0f, 0.0f, -0.2f } ), { 0.07f, 0.01f, 0.06f } );
        m_birdBody = body.build().front();
        MeshBuilder wing;
        wing.box( place( { 0.15f, 0.0f, 0.0f } ), { 0.15f, 0.008f, 0.07f } );
        m_birdWing = wing.build().front();
        m_flockAt = m_pos;
        for ( int i = 0; i < m_level.m_birds; ++i )
        {
            float a = rnd() * 6.283f, r = 6.0f + rnd() * 6.0f;
            Bird  b;
            b.m_pos = { m_pos.x + cosf( a ) * r, m_level.m_wallHeight + 1.0f + rnd() * 2.5f, m_pos.z + sinf( a ) * r };
            b.m_vel = { -sinf( a ) * 5.0f, 0.0f, cosf( a ) * 5.0f };
            b.m_phase = rnd() * 6.283f;
            m_birds.push_back( b );
        }
    }
}

void Game::updateWorld( float dt )
{
    // Under water: slow, floating, the current turns one round; the breath runs out.
    if ( !m_waters.empty() )
    {
        for ( Water &w : m_waters ) w.m_grow = fminf( 1.0f, w.m_grow + dt * 0.5f );
        int in = waterAt( Vector3Add( m_pos, { 0.0f, 1.0f, 0.0f } ) );
        if ( in >= 0 && !m_inWater ) m_audio.play( SfxE::SIZZLE, m_pos, m_camTarget, 0.4f ), fire( "water" );
        m_inWater = in >= 0;
        if ( m_inWater )
        {
            const Water &w = m_waters[in];
            Vector3      to = { m_pos.x - w.m_c.x, 0.0f, m_pos.z - w.m_c.z };
            m_vel = Vector3Add( m_vel, Vector3Scale( { -to.z * w.m_turn, 0.0f, to.x * w.m_turn }, 0.9f * dt ) );
            m_vel.x *= 1.0f - fminf( 1.0f, 1.5f * dt );
            m_vel.z *= 1.0f - fminf( 1.0f, 1.5f * dt );
            if ( rnd() < dt * 6.0f )
                m_particles.push_back(
                    { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.3f, 1.6f, ( rnd() - 0.5f ) * 0.3f } ),
                      { 0.0f, 1.2f, 0.0f },
                      1.2f,
                      1.2f,
                      0.04f,
                      { 200, 235, 255, 200 },
                      2 } );
        }
    }
    // Out in a vacuum or under water: the air runs out (refilled indoors, at air posts, from canisters; under water
    // above the surface), and one hears little.
    if ( m_level.m_vacuum || !m_waters.empty() )
    {
        int r, c;
        m_level.cellOf( m_pos, r, c );
        m_outside = ( m_level.m_vacuum && m_level.outdoor( r, c ) ) || m_inWater;
        bool post = false;
        for ( const Vector3 &p : m_airPosts )
            post = post || Vector2Distance( { p.x, p.z }, { m_pos.x, m_pos.z } ) < 2.4f;
        if ( m_dead )
            m_oxygen = m_oxygenMax;
        else if ( m_outside && !post && !m_finished && m_scene == 0 )
        {
            m_oxygen = fmaxf( 0.0f, m_oxygen - dt );
            if ( m_oxygen <= 0.0f ) m_hurtBy = "air", drainHero( 7.0f * dt );
            if ( m_oxygen < m_oxygenMax * 0.25f ) fire( "lowoxygen" );
        }
        else
            m_oxygen = fminf( m_oxygenMax, m_oxygen + 30.0f * dt );
        m_audio.m_muffle = m_outside ? ( m_inWater ? 0.45f : 0.35f ) : 1.0f;
    }
    // The star's storms: a warning, then six seconds in which the open is deadly (for the enemies too).
    if ( m_level.m_flares > 0.0f && !m_finished && m_scene == 0 )
    {
        m_flareT += dt;
        if ( m_flareState == 0 && m_flareT > m_flareTimer )
        {
            m_flareState = 1, m_flareT = 0.0f;
            fire( "flarewarn" );
            m_audio.m_muffle = 1.0f;
            m_audio.play( SfxE::KLAXON, m_pos, m_camTarget, 0.5f );
        }
        else if ( m_flareState == 1 && m_flareT > 8.0f )
        {
            m_flareState = 2, m_flareT = 0.0f;
            fire( "flare" );
        }
        else if ( m_flareState == 2 )
        {
            if ( exposed( m_pos ) ) m_hurtBy = "storm", drainHero( 9.0f * harm( m_difficulty ) * dt );
            for ( Foe &f : m_foes )
                if ( f.m_alive && !f.m_hidden && exposed( f.m_pos ) )
                {
                    f.m_hp -= f.m_hpMax * 0.04f * dt;
                    if ( f.m_hp <= 0.0f ) killFoe( f, true );
                }
            if ( m_flareT > 6.0f )
                m_flareState = 0, m_flareT = 0.0f, m_flareTimer = m_level.m_flares * ( 0.8f + 0.4f * rnd() );
        }
    }
    if ( m_birds.empty() ) return;
    // The flock's middle drifts after the hero; each bird keeps its own ring round it and its own height.
    m_flockAt = Vector3Lerp( m_flockAt, m_pos, fminf( 1.0f, dt * 0.3f ) );
    for ( size_t i = 0; i < m_birds.size(); ++i )
    {
        Bird   &b = m_birds[i];
        Vector3 to = { m_flockAt.x - b.m_pos.x, 0.0f, m_flockAt.z - b.m_pos.z };
        float   r = fmaxf( 0.5f, Vector3Length( to ) );
        float   ring = 7.0f + ( i % 7 ) * 1.1f, height = m_level.m_wallHeight + 0.8f + ( i % 5 ) * 0.55f;
        Vector3 want = Vector3Add( Vector3Scale( { -to.z / r, 0.0f, to.x / r }, 5.5f + ( i % 3 ) ),
                                   Vector3Scale( to, ( r - ring ) / r * 0.9f ) );
        want.y = ( height + 0.4f * sinf( m_time * 0.7f + i ) - b.m_pos.y ) * 1.5f;
        for ( size_t j = 0; j < m_birds.size(); ++j )
        {
            Vector3 d = Vector3Subtract( b.m_pos, m_birds[j].m_pos );
            float   len = Vector3Length( d );
            if ( j != i && len < 1.2f && len > 1e-3f )
                want = Vector3Add( want, Vector3Scale( d, ( 1.2f - len ) * 3.0f / len ) );
        }
        b.m_vel = Vector3Lerp( b.m_vel, want, fminf( 1.0f, dt * 1.5f ) );
        b.m_pos = Vector3Add( b.m_pos, Vector3Scale( b.m_vel, dt ) );
        // Flapping when climbing or slow, gliding otherwise.
        b.m_phase += dt * ( b.m_vel.y > 0.2f || Vector3Length( b.m_vel ) < 4.5f ? 16.0f : 4.0f );
    }
}

void Game::poseWorld( std::vector<Drawable> &out )
{
    for ( const Vector3 &p : m_airPosts )
    {
        Drawable d;
        d.m_mesh = m_postMesh;
        d.m_surface = S_METAL;
        d.m_transform = MatrixTranslate( p.x, p.y, p.z );
        out.push_back( d );
        float near = Vector2Distance( { p.x, p.z }, { m_pos.x, m_pos.z } ) < 2.4f ? 1.0f : 0.0f;
        float pulse = 0.7f + 0.3f * sinf( m_time * ( 3.0f + 6.0f * near ) );
        d.m_mesh = m_postRing;
        d.m_surface = S_GLOW;
        // Blue for air; a heater glows orange, a radiation shield green.
        d.m_emissive = m_level.m_meter == "warmth" || m_level.m_meter == "climate"
                           ? Vector4{ 4.0f * pulse, 1.4f * pulse, 0.3f * pulse, 0.0f }
                       : m_level.m_meter == "dose" ? Vector4{ 1.2f * pulse, 4.0f * pulse, 0.6f * pulse, 0.0f }
                                                   : Vector4{ 0.4f * pulse, 1.6f * pulse, 4.0f * pulse, 0.0f };
        d.m_spotShadow = d.m_sunShadow = false;
        out.push_back( d );
    }
    for ( size_t i = 0; i < m_birds.size(); ++i )
    {
        const Bird &b = m_birds[i];
        float       yaw = atan2f( b.m_vel.x, b.m_vel.z );
        float       bank = Clamp( ( b.m_vel.x * cosf( yaw ) - b.m_vel.z * sinf( yaw ) ) * 0.1f, -0.5f, 0.5f );
        Matrix      at = MatrixMultiply( MatrixMultiply( MatrixRotateZ( bank ), MatrixRotateY( yaw ) ),
                                         MatrixTranslate( b.m_pos.x, b.m_pos.y, b.m_pos.z ) );
        float       flap = 0.7f * sinf( b.m_phase );
        Drawable    d;
        d.m_surface = S_HAIR3;
        d.m_spotShadow = d.m_sunShadow = false;
        d.m_mesh = m_birdBody;
        d.m_transform = at;
        out.push_back( d );
        d.m_mesh = m_birdWing;
        d.m_transform =
            MatrixMultiply( MatrixMultiply( MatrixRotateZ( flap ), MatrixTranslate( 0.05f, 0.0f, 0.0f ) ), at );
        out.push_back( d );
        d.m_transform = MatrixMultiply( MatrixMultiply( MatrixMultiply( MatrixRotateZ( flap ), MatrixRotateY( PI ) ),
                                                        MatrixTranslate( -0.05f, 0.0f, 0.0f ) ),
                                        at );
        out.push_back( d );
    }
}

void Game::drainHero( float amount )
{
    // Health lost to no air or to radiation: no armour helps, and no flinch.
    if ( m_dead || m_finished || m_liftGoing || m_scene > 0 ) return;
    amount *= 1.0f - fminf( 0.6f, stat( StatE::RESIST ) / 100.0f );   // filters, treads, the harness
    m_hp -= amount;
    m_sinceHurt = 0.0f;
    m_hurtFlash = fmaxf( m_hurtFlash, 0.25f );
    if ( m_hp <= 0.0f )
    {
        m_hp = 0.01f;
        m_invuln = 0.0f;
        hurtHero( 50.0f, { 0.0f, 0.0f, 0.0f }, m_pos );
    }
}

bool Game::exposed( Vector3 p ) const
{
    // Out in the open and not in the lee of something tall (a housing, a radiator, a wall).
    int r, c;
    m_level.cellOf( p, r, c );
    if ( !m_level.outdoor( r, c ) ) return false;
    for ( int dr = -1; dr <= 1; ++dr )
        for ( int dc = -1; dc <= 1; ++dc )
        {
            CellE k = m_level.cell( r + dr, c + dc ).m_kind;
            if ( k == CellE::ROCK || k == CellE::FIN ) return false;
        }
    return true;
}

void Game::drawWorldHud( int w, int h, float s )
{
    auto text = [&]( const char *t, float x, float y, float size, Color col, bool centre )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        Vector2 p = { centre ? x - m.x * 0.5f : x, y };
        DrawTextEx( m_font, t, { p.x + 2.0f * s, p.y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( col.a * 0.8f ) } );
        DrawTextEx( m_font, t, p, size * s, 1.0f, col );
    };
    if ( m_level.m_vacuum || ( !m_waters.empty() && m_oxygen < m_oxygenMax ) )
    {
        // The suit's air (or the breath under water) over the hero's panel: blue, red and blinking when it runs low.
        float x = 34.0f * s, y = h - 214.0f * s, bw = 266.0f * s;
        float f = Clamp( m_oxygen / m_oxygenMax, 0.0f, 1.0f );
        bool  low = f < 0.25f;
        Color c = low ? ( fmodf( m_time, 0.6f ) < 0.3f ? Color{ 255, 70, 60, 255 } : Color{ 140, 30, 25, 255 } )
                      : Color{ 90, 170, 255, 255 };
        text( TextFormat( "%s  %s", tr( m_level.m_vacuum ? "hud.oxygen" : "hud.breath" ),
                          m_outside ? "" : tr( "hud.air" ) ),
              x, y - 24.0f * s, 18, { 190, 215, 240, 230 }, false );
        DrawRectangle( (int)x, (int)y, (int)bw, (int)( 9 * s ), { 10, 20, 35, 200 } );
        DrawRectangle( (int)x, (int)y, (int)( bw * f ), (int)( 9 * s ), c );
    }
    if ( m_flareState == 1 )
    {
        int left = (int)ceilf( 8.0f - m_flareT );
        text( TextFormat( tr( "hud.flarewarn" ), left ), w * 0.5f, h * 0.16f, 34, { 255, 170, 70, 255 }, true );
    }
    else if ( m_flareState == 2 )
    {
        // The storm: the light turns hard and red, and the screen with it where one stands in it.
        bool open = exposed( m_pos );
        DrawRectangle( 0, 0, w, h, { 255, 90, 40, (unsigned char)( open ? 50 + 30 * sinf( m_time * 19.0f ) : 18 ) } );
        text( tr( open ? "hud.flare" : "hud.sheltered" ), w * 0.5f, h * 0.16f, 34,
              open ? Color{ 255, 90, 60, 255 } : Color{ 200, 220, 255, 230 }, true );
    }
}

int Game::waterAt( Vector3 p ) const
{
    for ( size_t i = 0; i < m_waters.size(); ++i )
        if ( Vector3Distance( p, m_waters[i].m_c ) < m_waters[i].m_r * m_waters[i].m_grow ) return (int)i;
    return -1;
}

void Game::drawWater( Gfx &gfx )
{
    // Each ball wobbles a little, its axes breathing against each other.
    for ( size_t i = 0; i < m_waters.size(); ++i )
    {
        const Water &w = m_waters[i];
        float        r = w.m_r * w.m_grow, k = 0.035f * sinf( m_time * 1.3f + i * 1.7f );
        gfx.water( m_waterMesh,
                   MatrixMultiply( MatrixScale( r * ( 1.0f + k ), r * ( 1.0f - k ), r * ( 1.0f + k * 0.5f ) ),
                                   MatrixTranslate( w.m_c.x, w.m_c.y, w.m_c.z ) ) );
    }
    if ( m_pondMesh.vertexCount == 0 && m_shallowMesh.vertexCount == 0 ) return;
    // The ponds: rings round whatever wades in one (they lie off the paths, so it is rare), the sky they mirror. The
    // shallow water over the lawn the same, with a splash at the feet of what moves through it (w: how fast).
    std::vector<Vector4> wading, splashing;
    auto                 wade = [&]( Vector3 p, Vector3 v, float strength )
    {
        float fast = Clamp( sqrtf( v.x * v.x + v.z * v.z ) / 4.2f, 0.0f, 1.0f );
        for ( size_t i = 0; i < m_level.m_ponds.size() && wading.size() < 4; ++i )
            if ( m_level.pondReach( i, p.x, p.z ) < 1.0f )
            {
                wading.push_back( { p.x, p.z, strength, fast } );
                return;
            }
        for ( size_t i = 0; i < m_level.m_shallows.size() && splashing.size() < 4; ++i )
            if ( m_level.shallowReach( i, p.x, p.z ) < 1.0f )
            {
                splashing.push_back( { p.x, p.z, strength, fast } );
                return;
            }
    };
    wade( m_pos, m_vel, 1.0f );
    if ( m_partner ) wade( partnerPos(), { 0.0f, 0.0f, 0.0f }, 1.0f );
    for ( const Foe &f : m_foes )
        if ( f.m_alive ) wade( f.m_pos, f.m_vel, 0.8f );
    Vector3 sky = m_level.m_skyColor.x >= 0.0f
                      ? Vector3{ powf( m_level.m_skyColor.x, 2.2f ), powf( m_level.m_skyColor.y, 2.2f ),
                                 powf( m_level.m_skyColor.z, 2.2f ) }
                      : Vector3Scale( m_level.m_ambUp, 6.0f );
    gfx.pond( m_pondMesh, wading, sky, false );
    // The shallows only show (one walks on the lawn under them): not on frugal, as the lawn's tufts.
    if ( gfx.detail() > 0 ) gfx.pond( m_shallowMesh, splashing, sky, true );
}

void Game::buildPonds()
{
    // A disc each: rings out to a little past the shore (the bank rises through it there), round the wavering shore.
    // The shallow water over the lawn the same way, a hand over the lawn, out to its damp rim (Level::m_shallows).
    auto disc = [&]( const std::vector<Vector4> &list, size_t shape, float y, float past ) -> Mesh
    {
        const int                   Around = 28;
        const float                 Rings[] = { 0.0f, 0.3f, 0.55f, 0.75f, 0.88f, 0.96f, 1.02f, 1.1f };
        const int                   N = (int)( sizeof( Rings ) / sizeof( Rings[0] ) );
        std::vector<float>          pos, uv, nrm;
        std::vector<unsigned short> idx;
        for ( size_t i = 0; i < list.size(); ++i )
        {
            const Vector4 &p = list[i];
            int            first = (int)( pos.size() / 3 );
            for ( int j = 0; j < N; ++j )
                for ( int k = 0; k < Around; ++k )
                {
                    float a = k * 2.0f * PI / Around, t = j + 1 == N ? past : Rings[j];
                    float sh = m_level.pondShape( i + shape, a );
                    pos.insert( pos.end(), { p.x + cosf( a ) * p.z * t * sh, y, p.y + sinf( a ) * p.w * t * sh } );
                    uv.insert( uv.end(), { t * cosf( a ), t * sinf( a ) } );
                    nrm.insert( nrm.end(), { 0.0f, 1.0f, 0.0f } );
                }
            for ( int j = 0; j + 1 < N; ++j )
                for ( int k = 0; k < Around; ++k )
                {
                    int a = first + j * Around + k, b = first + j * Around + ( k + 1 ) % Around;
                    idx.insert( idx.end(),
                                { (unsigned short)a, (unsigned short)b, (unsigned short)( b + Around ),
                                  (unsigned short)a, (unsigned short)( b + Around ), (unsigned short)( a + Around ) } );
                }
        }
        Mesh m = {};
        if ( list.empty() ) return m;
        m.vertexCount = (int)( pos.size() / 3 );
        m.triangleCount = (int)( idx.size() / 3 );
        m.vertices = (float *)MemAlloc( (unsigned)( pos.size() * sizeof( float ) ) );
        m.texcoords = (float *)MemAlloc( (unsigned)( uv.size() * sizeof( float ) ) );
        m.normals = (float *)MemAlloc( (unsigned)( nrm.size() * sizeof( float ) ) );
        m.indices = (unsigned short *)MemAlloc( (unsigned)( idx.size() * sizeof( unsigned short ) ) );
        std::copy( pos.begin(), pos.end(), m.vertices );
        std::copy( uv.begin(), uv.end(), m.texcoords );
        std::copy( nrm.begin(), nrm.end(), m.normals );
        std::copy( idx.begin(), idx.end(), m.indices );
        UploadMesh( &m, false );
        return m;
    };
    m_pondMesh = disc( m_level.m_ponds, 0, PondLevel, 1.1f );
    m_shallowMesh = disc( m_level.m_shallows, 17, ShallowLevel, 1.35f );   // (its rim wanders out to 1.33)
}

void Game::drawAir( Gfx &gfx )
{
    if ( m_level.m_vacuum || m_level.m_outdoorThemes.empty() || m_dot.id == 0 ) return;
    int r, c;
    m_level.cellOf( m_camTarget, r, c );
    if ( !m_level.outdoor( r, c ) ) return;
    // What the air carries here: 1 pollen, 2 dust, 3 snow, 4 haze; with mist low over the ground in some.
    ThemeE t = m_level.theme( r, c );
    int    kind = t == ThemeE::PARK                                                ? 1
                  : t == ThemeE::DUSK || t == ThemeE::STREET || t == ThemeE::ARENA ? 2
                  : t == ThemeE::SNOW || t == ThemeE::ICE                          ? 3
                  : t == ThemeE::DECK                                              ? 4
                                                                                   : 0;
    if ( kind == 0 ) return;
    int detail = gfx.detail();   // 0 frugal, 1 low, 2 middle and high
    int motes = ( kind == 3 ? 2 : 1 ) * ( detail == 0 ? 50 : detail == 1 ? 120 : 260 );
    int mists = kind == 1 || kind == 4 || kind == 3 ? ( detail == 0 ? 0 : detail == 1 ? 10 : 22 ) : 0;
    if ( kind == 4 ) motes /= 3;
    // Unlit: their colour from the place's light (the sky's, the sun's).
    float   bright = Clamp( ( m_level.m_ambUp.x + m_level.m_ambUp.y + m_level.m_ambUp.z ) * 3.5f, 0.25f, 1.2f );
    Vector3 wind =
        Vector3Length( m_level.m_windDir ) > 0.1f ? Vector3Normalize( m_level.m_windDir ) : Vector3{ 0.8f, 0.0f, 0.6f };
    float         speed = ( Vector3Length( m_level.m_windDir ) > 0.1f ? fmaxf( m_level.m_windStrength, 1.0f ) : 1.0f ) *
                          ( 1.0f + 1.5f * m_gust );
    Vector3       fwd = Vector3Normalize( Vector3Subtract( m_cam.target, m_cam.position ) );
    Vector3       right = Vector3Normalize( Vector3CrossProduct( fwd, m_cam.up ) );
    Vector3       up = Vector3CrossProduct( right, fwd );
    const Vector3 Box = { 30.0f, kind == 3 ? 9.0f : 6.0f, 30.0f };
    float         ground = groundAt( m_camTarget );
    if ( ground < -100.0f ) ground = 0.0f;
    Vector3 lo = { m_camTarget.x - Box.x * 0.5f, ground, m_camTarget.z - Box.z * 0.5f };
    auto    hash = [&]( int i, int salt ) { return hash01( i, salt, 977 ); };
    // A fixed point carried by the wind, wrapped into the box round the camera's target; fading at the box's sides.
    auto place = [&]( int i, float drift, float fall, Vector3 &p ) -> float
    {
        Vector3 s = { hash( i, 1 ) * Box.x, hash( i, 2 ) * Box.y, hash( i, 3 ) * Box.z };
        float   go = m_time * drift * ( 0.6f + 0.8f * hash( i, 4 ) );
        float   ph = hash( i, 5 ) * 6.28f;
        Vector3 w = { s.x + wind.x * go + 0.4f * sinf( m_time * 0.7f + ph ),
                      s.y - m_time * fall * ( 0.7f + 0.6f * hash( i, 6 ) ) + 0.25f * sinf( m_time * 0.9f + ph * 1.3f ),
                      s.z + wind.z * go + 0.4f * cosf( m_time * 0.6f + ph ) };
        auto    wrap = []( float v, float o, float size ) { return o + v - size * floorf( ( v ) / size ); };
        p = { wrap( w.x - lo.x, lo.x, Box.x ), wrap( w.y, lo.y, Box.y ), wrap( w.z - lo.z, lo.z, Box.z ) };
        float ex = fminf( p.x - lo.x, lo.x + Box.x - p.x ), ez = fminf( p.z - lo.z, lo.z + Box.z - p.z );
        float ey = fminf( p.y - lo.y, lo.y + Box.y - p.y );
        return Clamp( fminf( ex, ez ) / 3.0f, 0.0f, 1.0f ) * Clamp( ey / 0.8f, 0.0f, 1.0f );
    };
    auto quad = [&]( Vector3 p, float s, Color col )
    {
        Vector3 rr = Vector3Scale( right, s ), uu = Vector3Scale( up, s );
        rlColor4ub( col.r, col.g, col.b, col.a );
        rlTexCoord2f( 0.0f, 0.0f );
        rlVertex3f( p.x - rr.x + uu.x, p.y - rr.y + uu.y, p.z - rr.z + uu.z );
        rlTexCoord2f( 0.0f, 1.0f );
        rlVertex3f( p.x - rr.x - uu.x, p.y - rr.y - uu.y, p.z - rr.z - uu.z );
        rlTexCoord2f( 1.0f, 1.0f );
        rlVertex3f( p.x + rr.x - uu.x, p.y + rr.y - uu.y, p.z + rr.z - uu.z );
        rlTexCoord2f( 1.0f, 0.0f );
        rlVertex3f( p.x + rr.x + uu.x, p.y + rr.y + uu.y, p.z + rr.z + uu.z );
    };
    // A puff of mist lies flat over the ground (phase F3: facing the camera, each stood in the ground and the ground cut
    // it off in a straight line across the picture: a pale box round the hero in the gardens, its legs ghostly in it).
    auto flat = [&]( Vector3 p, float s, Color col )
    {
        rlColor4ub( col.r, col.g, col.b, col.a );
        rlTexCoord2f( 0.0f, 0.0f );
        rlVertex3f( p.x - s, p.y, p.z - s );
        rlTexCoord2f( 0.0f, 1.0f );
        rlVertex3f( p.x - s, p.y, p.z + s );
        rlTexCoord2f( 1.0f, 1.0f );
        rlVertex3f( p.x + s, p.y, p.z + s );
        rlTexCoord2f( 1.0f, 0.0f );
        rlVertex3f( p.x + s, p.y, p.z - s );
    };
    gfx.beginParticles( 1.0f, false );
    rlSetTexture( m_dot.id );
    rlBegin( RL_QUADS );
    // Mist: wide faint puffs drifting low over the ground.
    for ( int i = 0; i < mists; ++i )
    {
        Vector3 p;
        float   fade = place( 1000 + i, speed * 0.25f, 0.0f, p );
        p.y = ground + 0.3f + 0.5f * hash( 1000 + i, 7 );
        float a = fade * ( 0.5f + 0.5f * sinf( m_time * 0.15f + i ) ) * ( kind == 4 ? 0.09f : 0.06f ) * bright;
        flat( p, 2.0f + 2.0f * hash( 1000 + i, 8 ),
              { (unsigned char)( kind == 4 ? 255 : 220 ), (unsigned char)( kind == 4 ? 210 : 235 ), 255,
                (unsigned char)( 255.0f * a ) } );
    }
    for ( int i = 0; i < motes; ++i )
    {
        Vector3 p;
        float   fade = 0.0f, size = 0.03f, a = 0.5f;
        Color   col = WHITE;
        if ( kind == 1 )   // pollen and seed fluff, slow, glinting
        {
            fade = place( i, speed * 0.35f, -0.02f, p );
            size = 0.02f + 0.025f * hash( i, 9 );
            a = 0.45f + 0.45f * sinf( m_time * 2.5f + i * 1.7f );
            col = hash( i, 10 ) < 0.6f ? Color{ 255, 240, 170, 255 } : Color{ 240, 255, 230, 255 };
        }
        else if ( kind == 2 )   // dust blown along
        {
            fade = place( i, speed * 1.2f, 0.05f, p );
            size = 0.025f + 0.05f * hash( i, 9 );
            a = 0.35f;
            col = { 255, 200, 140, 255 };
        }
        else if ( kind == 3 )   // snow falling, slanted by the wind
        {
            fade = place( i, speed * 0.6f, 1.1f, p );
            size = 0.025f + 0.025f * hash( i, 9 );
            a = 0.75f;
            col = { 235, 245, 255, 255 };
        }
        else   // drops of the clouds' haze
        {
            fade = place( i, speed * 0.8f, 0.2f, p );
            size = 0.03f;
            a = 0.3f;
            col = { 255, 220, 190, 255 };
        }
        col.a = (unsigned char)( 255.0f * Clamp( a * fade * bright, 0.0f, 1.0f ) );
        if ( col.a > 2 ) quad( p, size, col );
    }
    rlEnd();
    rlSetTexture( 0 );
    gfx.endParticles();
}

}   // namespace spaceopera
