/**
 * @file GamePhoto.cpp
 * @brief The photo mode: the game stands still, the camera moves freely round a point on the ground (panned, turned,
 *        tilted, pulled in or out), and the picture is taken without the HUD.
 * @author Rene Weller
 */
#include "Game.h"

#include <raymath.h>

#include <cmath>

namespace spaceopera
{

namespace
{

constexpr float NearMost = 2.0f;    ///< the camera's distance: nearest
constexpr float FarMost = 40.0f;    ///< farthest
constexpr float Roam = 30.0f;       ///< how far from the hero its point may go
constexpr float LowMost = 0.06f;    ///< the tilt: lowest (nearly level)
constexpr float HighMost = 1.45f;   ///< highest (nearly from above)

}   // namespace

void Game::startPhoto()
{
    // From the camera as it is: its point, its turn, its tilt, its distance.
    m_photo = true;
    m_photoTarget = m_cam.target;
    Vector3 off = Vector3Subtract( m_cam.position, m_cam.target );
    m_photoDist = Clamp( Vector3Length( off ), NearMost, FarMost );
    m_photoPitch = Clamp( asinf( off.y / fmaxf( 1e-3f, Vector3Length( off ) ) ), LowMost, HighMost );
    m_photoYaw = atan2f( off.x, off.z );
    photo( 0.0f, PhotoInput() );
}

void Game::photo( float dt, const PhotoInput &in )
{
    if ( !m_photo ) return;
    m_photoYaw += in.m_turn;
    m_photoPitch = Clamp( m_photoPitch + in.m_tilt, LowMost, HighMost );
    m_photoDist = Clamp( m_photoDist * ( 1.0f + in.m_zoom ), NearMost, FarMost );
    // Panned on the ground as the picture is turned (up on the screen is away from the camera).
    float   speed = ( 2.0f + m_photoDist * 0.5f ) * dt;
    Vector3 away = { -sinf( m_photoYaw ), 0.0f, -cosf( m_photoYaw ) };
    Vector3 right = { -away.z, 0.0f, away.x };
    m_photoTarget = Vector3Add( m_photoTarget, Vector3Scale( right, in.m_move.x * speed ) );
    m_photoTarget = Vector3Add( m_photoTarget, Vector3Scale( away, -in.m_move.y * speed ) );
    Vector3 from = Vector3Subtract( m_photoTarget, m_pos );
    from.y = 0.0f;
    if ( Vector3Length( from ) > Roam )
        m_photoTarget = Vector3Add( m_pos, Vector3Scale( Vector3Normalize( from ), Roam ) );
    float ground = groundAt( m_photoTarget );
    m_photoTarget.y = ground > -100.0f ? ground + 1.0f : m_pos.y + 1.0f;
    m_cam.target = m_photoTarget;
    m_cam.position =
        Vector3Add( m_photoTarget, Vector3Scale( { sinf( m_photoYaw ) * cosf( m_photoPitch ), sinf( m_photoPitch ),
                                                   cosf( m_photoYaw ) * cosf( m_photoPitch ) },
                                                 m_photoDist ) );
    m_cam.up = { 0.0f, 1.0f, 0.0f };
    m_cam.fovy = 38.0f;
    m_cam.projection = CAMERA_PERSPECTIVE;
}

void Game::endPhoto()
{
    // The game's camera takes over again with its next update.
    m_photo = false;
}

}   // namespace spaceopera
