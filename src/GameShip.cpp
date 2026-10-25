/**
 * @file GameShip.cpp
 * @brief Aboard a ship: the dodges that tilt gravity, the loose cargo they throw about, the refugees and Ilka, the view
 *        through the window in the floor (the red dwarf, the chain of braking ships, the patrol that does not come),
 *        and the last scene down to the clinic.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"
#include "Rig.h"

#include <raymath.h>
#include <rlgl.h>

#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float WarnTime = 1.3f;   ///< the klaxon before a dodge
constexpr float PushTime = 1.8f;   ///< the dodge itself
constexpr float CargoRadius = 0.55f;

}   // namespace

void Game::loadShip( Gfx &gfx, const std::string &assets )
{
    (void)gfx;
    const int DR[4] = { 1, 0, 0, -1 }, DC[4] = { 0, 1, -1, 0 };
    // People sit against a wall and face where there is most room; Ilka in the nook of her container.
    for ( const Marker &m : m_level.m_markers )
    {
        if ( m.m_kind == 'N' || m.m_kind == 'I' )
        {
            int best = 0, bestRoom = -1;
            for ( int d = 0; d < 4; ++d )
            {
                int room = 0;
                for ( int k = 1; k <= 3 && m_level.walkable( m.m_row + DR[d] * k, m.m_col + DC[d] * k ); ++k ) ++room;
                if ( room > bestRoom ) bestRoom = room, best = d;
            }
            Vector3 dir = { (float)DC[best], 0.0f, (float)DR[best] };
            Person  q;
            q.m_pos = Vector3Subtract( m.m_pos, Vector3Scale( dir, 0.4f ) );
            q.m_yaw = atan2f( dir.x, dir.z ) + ( rnd() - 0.5f ) * 0.6f;
            q.m_look = m.m_kind == 'I' ? 3 : ( m.m_row * 7 + m.m_col * 3 ) % 3;
            q.m_sit = 1.0f;
            q.m_down = 0.0f;
            if ( m.m_kind == 'I' )
            {
                m_ilka = (int)m_people.size();
                m_scanner = Vector3Add( q.m_pos, Vector3Scale( { dir.z, 0.0f, -dir.x }, 0.7f ) );
            }
            m_people.push_back( q );
            m_blocked.insert( m.m_row * m_level.cols() + m.m_col );
        }
        else if ( m.m_kind == 'Q' )
            m_cargo.push_back( { m.m_pos, { 0, 0, 0 }, rnd() * 6.28f, 0.0f } );
    }
    // A strapped crate; the scanner that sat round her head, a ring on the floor.
    MeshBuilder crate;
    crate.roundedBox( place( { 0.0f, 0.45f, 0.0f } ), { 0.5f, 0.45f, 0.5f }, 0.04f, 2 );
    m_cargoMesh = crate.build().front();
    MeshBuilder band;
    band.box( place( { 0.0f, 0.45f, 0.0f } ), { 0.52f, 0.46f, 0.05f } );
    band.box( place( { 0.0f, 0.45f, 0.0f } ), { 0.05f, 0.46f, 0.52f } );
    m_cargoBand = band.build().front();
    MeshBuilder scan;
    scan.lathe(
        place( { 0.0f, 0.02f, 0.0f } ),
        { { 0.12f, 0.0f }, { 0.16f, 0.0f }, { 0.16f, 0.0f }, { 0.16f, 0.06f }, { 0.16f, 0.06f }, { 0.12f, 0.06f } },
        20 );
    m_scannerMesh = scan.build().front();

    if ( m_level.m_sky.empty() ) return;
    m_skyTex = LoadTexture( ( assets + "/textures/" + m_level.m_sky + ".png" ).c_str() );
    m_starTex = LoadTexture( ( assets + "/textures/reddwarf.png" ).c_str() );
    m_cardTex = LoadTexture( ( assets + "/textures/clinic.png" ).c_str() );
    SetTextureFilter( m_cardTex, TEXTURE_FILTER_BILINEAR );
    m_sphere = GenMeshSphere( 1.0f, 96, 96 );
    // The window's panes, and where it is: the view below is laid out around it.
    MeshBuilder glass;
    Vector3     c = { m_level.cols() * CellSize * 0.5f, 0.0f, m_level.rows() * CellSize * 0.5f };
    if ( !m_level.m_glass.empty() )
    {
        c = { 0.0f, 0.0f, 0.0f };
        for ( auto rc : m_level.m_glass )
        {
            Vector3 p = { ( rc.second + 0.5f ) * CellSize, 0.0f, ( rc.first + 0.5f ) * CellSize };
            glass.box( place( { p.x, -0.02f, p.z } ), { CellSize * 0.5f, 0.02f, CellSize * 0.5f } );
            c = Vector3Add( c, p );
        }
        c = Vector3Scale( c, 1.0f / m_level.m_glass.size() );
        m_glassMesh = glass.build().front();
    }
    // Looking down through the window from the game's camera: the red dwarf's limb crosses the view, our own drive
    // plume reaches towards it, the other ships brake before it in a chain, and the patrol hangs off to one side.
    Vector3 view = Vector3Normalize( { 0.0f, -0.82f, -0.57f } );
    m_star = Vector3Add( c, { 3000.0f, -9000.0f, -9500.0f } );
    m_starRadius = 5000.0f;
    m_ownPlume = Vector3Add( c, { 70.0f, -160.0f, 30.0f } );
    for ( int i = 0; i < 12; ++i )
    {
        float d = 500.0f + 300.0f * i;
        float side = ( i % 2 ? 1.0f : -1.0f ) * ( 0.08f + 0.22f * rnd() ) * d;
        m_ships.push_back(
            { Vector3Add( Vector3Add( c, Vector3Scale( view, d ) ), { side, 0.0f, ( rnd() - 0.5f ) * 0.1f * d } ),
              d } );
    }
    m_patrol = Vector3Add( Vector3Add( c, Vector3Scale( view, 1200.0f ) ), { -230.0f, 40.0f, 0.0f } );
}

void Game::updateThrust( float dt )
{
    if ( !m_level.m_thrust || !m_fired.count( "alarm" ) || m_scene > 0 || m_finished )
    {
        m_thrustState = 0;
        m_thrustAmt = fmaxf( 0.0f, m_thrustAmt - dt * 8.0f );
        return;
    }
    m_thrustTimer -= dt;
    if ( m_thrustState == 0 && m_thrustTimer <= 0.0f )
    {
        // The pilot dodges: a klaxon, then the push sideways.
        const Vector3 dirs[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
        m_thrustDir = dirs[(int)( rnd() * 4.0f ) % 4];
        m_thrustState = 1;
        m_thrustTimer = WarnTime;
        ++m_dodges;
        m_audio.play( SfxE::KLAXON, m_pos, m_camTarget, 0.5f );
        if ( m_dodges <= 3 ) fire( TextFormat( "dodge%d", m_dodges ) );
    }
    else if ( m_thrustState == 1 && m_thrustTimer <= 0.0f )
    {
        m_thrustState = 2;
        m_thrustTimer = PushTime;
        m_audio.play( SfxE::THRUST, m_pos, m_camTarget, 0.9f );
        m_shake = fmaxf( m_shake, 0.3f );
    }
    else if ( m_thrustState == 2 )
    {
        float t = 1.0f - m_thrustTimer / PushTime;
        m_thrustAmt = ThrustPeak * sinf( PI * Clamp( t, 0.0f, 1.0f ) );
        // Dust and loose bits stream the way it pushes.
        if ( rnd() < dt * 40.0f )
        {
            Vector3 p =
                Vector3Add( m_pos, { ( rnd() - 0.5f ) * 10.0f, 0.2f + rnd() * 2.0f, ( rnd() - 0.5f ) * 10.0f } );
            m_particles.push_back(
                { p, Vector3Scale( m_thrustDir, 3.0f + rnd() * 2.0f ), 1.0f, 1.0f, 0.035f, { 200, 195, 185, 60 }, 3 } );
        }
        if ( m_thrustTimer <= 0.0f )
        {
            m_thrustState = 0;
            m_thrustAmt = 0.0f;
            m_thrustTimer = 6.5f + rnd() * 4.0f;
        }
    }
}

void Game::updateCargo( float dt )
{
    for ( size_t i = 0; i < m_cargo.size(); ++i )
    {
        Cargo &q = m_cargo[i];
        // A dodge sets it sliding; the deck's friction stops it again.
        q.m_vel = Vector3Add( q.m_vel, Vector3Scale( thrust(), dt * 1.1f ) );
        q.m_vel = Vector3Scale( q.m_vel, expf( -( m_thrustAmt > 0.5f ? 0.6f : 4.0f ) * dt ) );
        q.m_vel.y = 0.0f;
        Vector3 before = q.m_pos;
        q.m_pos = Vector3Add( q.m_pos, Vector3Scale( q.m_vel, dt ) );
        m_level.collide( q.m_pos, CargoRadius, 0.2f );
        for ( const Person &p : m_people ) pushOutCircle( q.m_pos, CargoRadius, p.m_pos.x, p.m_pos.z, 0.45f );
        for ( size_t j = 0; j < m_cargo.size(); ++j )
            if ( j != i ) pushOutCircle( q.m_pos, CargoRadius, m_cargo[j].m_pos.x, m_cargo[j].m_pos.z, CargoRadius );
        if ( dt > 0.0f ) q.m_vel = Vector3Scale( Vector3Subtract( q.m_pos, before ), 1.0f / dt );
        float speed = Vector3Length( q.m_vel );
        q.m_yaw += speed * 0.25f * dt;
        if ( speed < 2.5f ) continue;
        // Fast enough to hurt: whoever stands in its way.
        Vector3 dir = Vector3Scale( q.m_vel, 1.0f / speed );
        if ( Vector2Distance( { q.m_pos.x, q.m_pos.z }, { m_pos.x, m_pos.z } ) < CargoRadius + HeroRadius + 0.1f )
            hurtHero( 8.0f, Vector3Scale( dir, 2.0f ), q.m_pos );
        for ( Foe &f : m_foes )
            if ( f.m_alive && !f.m_hidden &&
                 Vector2Distance( { q.m_pos.x, q.m_pos.z }, { f.m_pos.x, f.m_pos.z } ) <
                     CargoRadius + foeRadius( f ) + 0.1f )
            {
                hurtFoe( f, 6.0f * speed, dir );
                q.m_vel = Vector3Scale( q.m_vel, 0.5f );
            }
    }
}

void Game::updateScene( float dt, GameInput &in )
{
    // The last scene, at Ilka's nook once the bridge is clear.
    if ( m_scene == 0 )
    {
        if ( m_ilka >= 0 && m_fired.count( "z" ) )
        {
            m_scene = 1;
            m_sceneTimer = 0.0f;
            m_hintQueue.clear();
            m_hint = nullptr;
        }
        return;
    }
    m_sceneTimer += dt;
    in = GameInput();
    const Person &ilka = m_people[m_ilka];
    if ( m_scene == 1 )
    {
        // Slowly to her side, the lines; then the shot from behind.
        Vector2 to = { ilka.m_pos.x - m_pos.x, ilka.m_pos.z - m_pos.z };
        float   dist = Vector2Length( to );
        if ( dist > 1.1f ) in.m_move = Vector2Scale( to, 0.35f / dist );
        float want = atan2f( to.x, to.y );
        m_yaw += Clamp( angleTo( m_yaw, want ), -4.0f * dt, 4.0f * dt );
        m_aimYaw = m_yaw;
        if ( m_sceneTimer > 1.5f && !m_line && m_lineQueue.empty() )
        {
            m_scene = 2;
            m_sceneTimer = 0.0f;
            Vector3 back = { -sinf( m_yaw ), 0.0f, -cosf( m_yaw ) };
            m_shooter = Vector3Add( m_pos, Vector3Scale( back, 3.2f ) );
            Vector3 muzzle = Vector3Add( m_shooter, { 0.0f, 1.35f, 0.0f } );
            m_flashes.push_back( { muzzle, { 40.0f, 18.0f, 6.0f }, 9.0f, 0.25f, 0.25f } );
            m_shotsInFlight.clear();
            m_audio.play( SfxE::GUNSHOT, muzzle, m_camTarget, 1.0f );
            m_hurtFlash = 1.0f;
            m_shake = 0.6f;
        }
    }
    else if ( m_scene == 2 )
    {
        m_sceneDown = fminf( 1.0f, m_sceneTimer / 0.6f );
        if ( m_sceneTimer > 0.5f && m_sceneTimer - dt <= 0.5f ) m_audio.play( SfxE::FALL, m_pos, m_camTarget, 0.8f );
        if ( m_sceneTimer > 2.4f ) m_scene = 3, m_sceneTimer = 0.0f;
    }
    else if ( m_scene == 3 )
    {
        if ( m_sceneTimer > 3.0f )
        {
            m_scene = 4;
            m_sceneTimer = 0.0f;
            fire( "clinic" );
        }
    }
    else if ( m_scene == 4 && m_sceneTimer > 2.0f && !m_line && m_lineQueue.empty() && !m_finished )
        m_finished = true;
}

void Game::drawView( Gfx &gfx )
{
    if ( m_skyTex.id == 0 ) return;
    Matrix star = MatrixMultiply(
        MatrixMultiply( MatrixScale( m_starRadius, m_starRadius, m_starRadius ), MatrixRotateX( 0.3f ) ),
        MatrixTranslate( m_star.x, m_star.y, m_star.z ) );
    gfx.planet( m_sphere, star, m_starTex, m_time * 0.0004f, 2.2f );
    drawPlumes( gfx );
}

void Game::drawPlumes( Gfx &gfx )
{
    Vector3 fwd = Vector3Normalize( Vector3Subtract( m_cam.target, m_cam.position ) );
    Vector3 right = Vector3Normalize( Vector3CrossProduct( fwd, { 0.0f, 1.0f, 0.0f } ) );
    Vector3 up = Vector3CrossProduct( right, fwd );
    auto    vertex = [&]( Vector3 p, float u, float v, Color c )
    {
        rlColor4ub( c.r, c.g, c.b, c.a );
        rlTexCoord2f( u, v );
        rlVertex3f( p.x, p.y, p.z );
    };
    // A tapered beam from tail to head, its colour running from one end to the other (the texture is soft across).
    auto beam = [&]( Vector3 tail, Vector3 head, float w0, float w1, Color c0, Color c1 )
    {
        Vector3 side = Vector3CrossProduct( Vector3Subtract( head, tail ), fwd );
        if ( Vector3Length( side ) < 1e-5f ) side = right;
        side = Vector3Normalize( side );
        vertex( Vector3Subtract( tail, Vector3Scale( side, w0 ) ), 0.0f, 0.0f, c0 );
        vertex( Vector3Add( tail, Vector3Scale( side, w0 ) ), 0.0f, 1.0f, c0 );
        vertex( Vector3Add( head, Vector3Scale( side, w1 ) ), 1.0f, 1.0f, c1 );
        vertex( Vector3Subtract( head, Vector3Scale( side, w1 ) ), 1.0f, 0.0f, c1 );
    };
    auto billboard = [&]( Vector3 p, float s, Color c )
    {
        Vector3 r = Vector3Scale( right, s ), u = Vector3Scale( up, s );
        vertex( Vector3Add( Vector3Subtract( p, r ), u ), 0.0f, 0.0f, c );
        vertex( Vector3Subtract( Vector3Subtract( p, r ), u ), 0.0f, 1.0f, c );
        vertex( Vector3Subtract( Vector3Add( p, r ), u ), 1.0f, 1.0f, c );
        vertex( Vector3Add( Vector3Add( p, r ), u ), 1.0f, 0.0f, c );
    };
    // A fusion plume: white-hot at the nozzle, widening and blue, fading out towards the star.
    auto plume = [&]( Vector3 at, float length, float width, float bright )
    {
        Vector3   dir = Vector3Normalize( Vector3Subtract( m_star, at ) );
        const int n = 8;
        for ( int k = 0; k < n; ++k )
        {
            float   f0 = (float)k / n, f1 = (float)( k + 1 ) / n;
            Vector3 a = Vector3Add( at, Vector3Scale( dir, length * f0 ) );
            Vector3 b = Vector3Add( at, Vector3Scale( dir, length * f1 ) );
            float   a0 = powf( 1.0f - f0, 1.8f ) * bright, a1 = powf( 1.0f - f1, 1.8f ) * bright;
            beam( a, b, width * ( 1.0f + 2.0f * f0 ), width * ( 1.0f + 2.0f * f1 ),
                  { 80, 140, 255, (unsigned char)( 140 * a0 ) }, { 80, 140, 255, (unsigned char)( 140 * a1 ) } );
            beam( a, b, width * 0.35f * ( 1.0f + f0 ), width * 0.35f * ( 1.0f + f1 ),
                  { 235, 242, 255, (unsigned char)( 255 * a0 ) }, { 235, 242, 255, (unsigned char)( 255 * a1 ) } );
        }
    };
    float flicker = 0.92f + 0.08f * sinf( m_time * 37.0f );
    float d = Vector3Distance( m_cam.position, m_patrol );

    gfx.beginParticles( 8.0f, false );
    rlSetTexture( m_streak.id );
    rlBegin( RL_QUADS );
    plume( m_ownPlume, 2200.0f, 10.0f + 5.0f * m_thrustAmt / ThrustPeak, flicker * 0.8f );
    for ( const Ship &s : m_ships ) plume( s.m_pos, s.m_size * 0.16f, s.m_size * 0.003f, 0.85f );
    // The patrol: a clean white hull catching the star's light.
    beam( Vector3Subtract( m_patrol, Vector3Scale( right, d * 0.01f ) ),
          Vector3Add( m_patrol, Vector3Scale( right, d * 0.01f ) ), d * 0.0008f, d * 0.0008f, { 220, 230, 255, 200 },
          { 220, 230, 255, 200 } );
    rlEnd();
    rlSetTexture( m_dot.id );
    rlBegin( RL_QUADS );
    // The nozzles' white points, the patrol's glint and its running lights.
    billboard( m_ownPlume, 18.0f, { 255, 250, 240, (unsigned char)( 230 * flicker ) } );
    for ( const Ship &s : m_ships ) billboard( s.m_pos, s.m_size * 0.005f, { 255, 250, 240, 220 } );
    billboard( m_patrol, d * 0.004f, { 255, 250, 245, 255 } );
    if ( fmodf( m_time, 1.2f ) < 0.15f )
    {
        billboard( Vector3Add( m_patrol, Vector3Scale( right, d * 0.009f ) ), d * 0.002f, { 60, 255, 90, 255 } );
        billboard( Vector3Subtract( m_patrol, Vector3Scale( right, d * 0.009f ) ), d * 0.002f, { 255, 50, 40, 255 } );
    }
    rlEnd();
    rlSetTexture( 0 );
    gfx.endParticles();
}

void Game::poseShip( std::vector<Drawable> &out )
{
    bool taken = m_fired.count( "bridgeclear" ) > 0;
    for ( size_t i = 0; i < m_people.size(); ++i )
    {
        const Person &q = m_people[i];
        FigurePose    p;
        p.m_pos = q.m_pos;
        p.m_yaw = q.m_yaw;
        p.m_time = m_time + (float)i * 1.7f;
        p.m_sit = q.m_sit;
        // Ilka, once they have been: on her back, empty.
        if ( (int)i == m_ilka && taken )
        {
            p.m_sit = 0.0f;
            p.m_down = 1.0f;
        }
        m_folk[q.m_look].pose( p, { -1.0f, 0.0f, 0.0f, 0.0f }, out );
    }
    for ( const Cargo &q : m_cargo )
    {
        Drawable d;
        d.m_transform = MatrixMultiply( MatrixRotateY( q.m_yaw ), MatrixTranslate( q.m_pos.x, q.m_pos.y, q.m_pos.z ) );
        d.m_mesh = m_cargoMesh;
        d.m_surface = S_CRATE;
        out.push_back( d );
        d.m_mesh = m_cargoBand;
        d.m_surface = S_DARK;
        out.push_back( d );
    }
    if ( m_ilka >= 0 && taken )
    {
        // The scanner, still warm.
        Drawable d;
        d.m_mesh = m_scannerMesh;
        d.m_surface = S_GLOW;
        d.m_transform = MatrixTranslate( m_scanner.x, m_scanner.y, m_scanner.z );
        float warm = 0.7f + 0.3f * sinf( m_time * 2.0f );
        d.m_emissive = { 3.0f * warm, 0.35f * warm, 0.15f * warm, 0.0f };
        out.push_back( d );
    }
    if ( m_scene == 2 )
    {
        // The one who shot, in the nook's mouth.
        FigurePose p;
        p.m_pos = m_shooter;
        p.m_yaw = atan2f( m_pos.x - m_shooter.x, m_pos.z - m_shooter.z );
        p.m_time = m_time;
        p.m_recoil = fmaxf( 0.0f, 1.0f - m_sceneTimer * 3.0f );
        m_boarderGun.pose( p, { 3.5f, 0.25f, 0.15f, 0.0f }, out );
    }
}

void Game::drawShipHud( int w, int h, float s )
{
    auto text = [&]( const char *t, float x, float y, float size, Color c )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        DrawTextEx( m_font, t, { x - m.x * 0.5f + 2 * s, y + 2 * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
        DrawTextEx( m_font, t, { x - m.x * 0.5f, y }, size * s, 1.0f, c );
    };
    // A dodge: an arrow from the hero the way it will push, and its name.
    if ( m_thrustState > 0 && m_scene == 0 )
    {
        Vector3 at = Vector3Add( m_pos, { 0.0f, 0.2f, 0.0f } );
        Vector2 a = GetWorldToScreenEx( at, m_cam, w, h );
        Vector2 b = GetWorldToScreenEx( Vector3Add( at, Vector3Scale( m_thrustDir, 3.5f ) ), m_cam, w, h );
        Vector2 d = Vector2Normalize( Vector2Subtract( b, a ) );
        Vector2 n = { -d.y, d.x };
        float   pulse = m_thrustState == 1 ? ( fmodf( m_time, 0.4f ) < 0.25f ? 1.0f : 0.35f ) : 1.0f;
        Color   c = { 255, 170, 40, (unsigned char)( 220 * pulse ) };
        for ( int k = 0; k < 3; ++k )
        {
            Vector2 tip = Vector2Add( a, Vector2Scale( d, ( 90.0f + 40.0f * k ) * s ) );
            Vector2 l =
                Vector2Subtract( Vector2Subtract( tip, Vector2Scale( d, 26.0f * s ) ), Vector2Scale( n, 22.0f * s ) );
            Vector2 r =
                Vector2Add( Vector2Subtract( tip, Vector2Scale( d, 26.0f * s ) ), Vector2Scale( n, 22.0f * s ) );
            DrawLineEx( l, tip, 6.0f * s, c );
            DrawLineEx( r, tip, 6.0f * s, c );
        }
        text( tr( "hud.thrust" ), w * 0.5f, h * 0.2f, 30, c );
    }
    // The patrol, marked while it is talked to.
    bool patrol = m_line && ( m_line->m_id == "p_call" || m_line->m_id.rfind( "p_patrol", 0 ) == 0 );
    if ( patrol && m_skyTex.id > 0 )
    {
        Vector2 p = GetWorldToScreenEx( m_patrol, m_cam, w, h );
        if ( p.x > 0 && p.y > 0 && p.x < w && p.y < h )
        {
            float r = 26.0f * s, l = 10.0f * s;
            Color c = { 200, 225, 255, 230 };
            for ( int k = 0; k < 4; ++k )
            {
                float   sx = k & 1 ? 1.0f : -1.0f, sy = k & 2 ? 1.0f : -1.0f;
                Vector2 corner = { p.x + sx * r, p.y + sy * r };
                DrawLineEx( corner, { corner.x - sx * l, corner.y }, 2.0f * s, c );
                DrawLineEx( corner, { corner.x, corner.y - sy * l }, 2.0f * s, c );
            }
            text( tr( "hud.patrol" ), p.x, p.y + r + 8 * s, 18, c );
        }
    }
    // The last scene: the shot's red, the dark, then the clinic.
    if ( m_scene == 2 )
        DrawRectangle( 0, 0, w, h, { 120, 0, 0, (unsigned char)( 160 * fmaxf( 0.0f, 1.0f - m_sceneTimer ) ) } );
    if ( m_scene == 2 && m_sceneTimer > 1.2f )
        DrawRectangle( 0, 0, w, h,
                       { 0, 0, 0, (unsigned char)( 255 * fminf( 1.0f, ( m_sceneTimer - 1.2f ) / 1.2f ) ) } );
    if ( m_scene >= 3 ) DrawRectangle( 0, 0, w, h, BLACK );
    if ( m_scene == 4 && m_cardTex.id > 0 )
    {
        // The picture fills the screen and drifts closer slowly.
        float         zoom = 1.04f + 0.03f * fminf( 1.0f, m_sceneTimer / 20.0f );
        float         k = fmaxf( (float)w / m_cardTex.width, (float)h / m_cardTex.height ) * zoom;
        float         dw = m_cardTex.width * k, dh = m_cardTex.height * k;
        unsigned char a = (unsigned char)( 255 * fminf( 1.0f, m_sceneTimer / 1.5f ) );
        DrawTexturePro( m_cardTex, { 0, 0, (float)m_cardTex.width, (float)m_cardTex.height },
                        { ( w - dw ) * 0.5f, ( h - dh ) * 0.5f, dw, dh }, { 0, 0 }, 0.0f, { 255, 255, 255, a } );
        DrawRectangleGradientV( 0, h * 2 / 3, w, h / 3, { 0, 0, 0, 0 }, { 0, 0, 0, 200 } );
    }
}

}   // namespace spaceopera
