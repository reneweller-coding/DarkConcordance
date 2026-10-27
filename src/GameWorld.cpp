/**
 * @file GameWorld.cpp
 * @brief What a place does around the fight: the birds over a drum's market that never land; out on a station's skin
 *        the suit's air, the muffled sound, and the star's radiation storms.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <cmath>

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
}

}   // namespace spaceopera
