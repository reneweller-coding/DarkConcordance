/**
 * @file GameAnomaly.cpp
 * @brief The Depths' anomalies: what each does to the fight and to the finds, the dead that burst, how they are shown.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"
#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float BurstFuse = 1.1f;     ///< seconds from the death to the blast
constexpr float BurstRadius = 2.3f;   ///< what it reaches (m)
constexpr float BurstHurt = 14.0f;    ///< what it does to the hero (before armour and the place's depth)

}   // namespace

void Game::applyAnomalies()
{
    // The place's anomalies multiply; each one also makes the finds better and the experience more.
    float reward = 0.0f;
    for ( int k : m_level.m_anomalies )
    {
        const AnomalyDef &a = anomalyDefs()[k];
        m_foeHp *= a.m_hp;
        m_foeTempo *= a.m_tempo;
        m_areaHit *= a.m_harm;
        m_stimScale *= a.m_stims;
        m_scrapScale *= a.m_scrap;
        m_burstDead = m_burstDead || a.m_burst;
        reward += a.m_reward;
    }
    // A deeper tier of the Depths: a tenth better finds and more experience per tier.
    int tier = std::max( 1, depthTier() );
    m_findScale = ( 1.0f + reward ) * ( 1.0f + 0.1f * ( tier - 1 ) );
    m_xpScale = ( 1.0f + reward * 0.67f ) * ( 1.0f + 0.06f * ( tier - 1 ) );
}

int Game::depthTier() const
{
    return m_levelFile == "tiefen.txt" ? std::clamp( m_progress.m_depthTier, 1, 10 ) : 0;
}

std::string Game::anomalyNames() const
{
    std::string names;
    for ( int k : m_level.m_anomalies )
        names +=
            ( names.empty() ? "" : "  ·  " ) + std::string( tr( std::string( "anomaly." ) + anomalyDefs()[k].m_key ) );
    return names;
}

void Game::anomalyDeath( Vector3 at )
{
    if ( m_burstDead ) m_bursts.push_back( { at, BurstFuse } );
}

void Game::updateAnomalies( float dt )
{
    // The dead that burst: a glow and sparks while the fuse burns, then the blast.
    for ( Burst &b : m_bursts )
    {
        b.m_t -= dt;
        if ( b.m_t > 0.0f )
        {
            if ( rnd() < dt * 14.0f )
                sparks( Vector3Add( b.m_pos, { 0.0f, 0.4f, 0.0f } ), { 0, 1, 0 }, 2, { 190, 120, 255, 255 }, 1.6f );
            continue;
        }
        explode( Vector3Add( b.m_pos, { 0.0f, 0.4f, 0.0f } ), 0.8f );
        Vector3 d = Vector3Subtract( m_pos, b.m_pos );
        d.y = 0.0f;
        float dist = Vector3Length( d );
        if ( !m_dead && dist < BurstRadius )
        {
            m_hurtBy = "burst";
            Vector3 push = dist > 1e-3f ? Vector3Scale( d, 3.0f / dist ) : Vector3{ 0.0f, 0.0f, 0.0f };
            hurtHero( BurstHurt * ( 1.0f - 0.4f * dist / BurstRadius ), push, b.m_pos );
        }
    }
    m_bursts.erase( std::remove_if( m_bursts.begin(), m_bursts.end(), []( const Burst &b ) { return b.m_t <= 0.0f; } ),
                    m_bursts.end() );
}

void Game::drawAnomalies( int w, int h, float y, unsigned char alpha )
{
    if ( m_level.m_anomalies.empty() ) return;
    float s = textScale( h );
    auto  line = [&]( const std::string &t, float at, float size, Color c )
    {
        Vector2 m = MeasureTextEx( m_font, t.c_str(), size * s, 1.0f );
        DrawTextEx( m_font, t.c_str(), { w * 0.5f - m.x * 0.5f + 2 * s, at + 2 * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( alpha * 0.8f ) } );
        DrawTextEx( m_font, t.c_str(), { w * 0.5f - m.x * 0.5f, at }, size * s, 1.0f, c );
    };
    std::string names = TextFormat( tr( "hud.anomalies" ), anomalyNames().c_str() );
    if ( depthTier() > 0 ) names = std::string( TextFormat( tr( "hud.tier" ), depthTier() ) ) + "  ·  " + names;
    line( names, y, 26, { 200, 160, 255, alpha } );
    line( TextFormat( tr( "hud.anomalyreward" ), (int)roundf( ( m_findScale - 1.0f ) * 100.0f ),
                      (int)roundf( ( m_xpScale - 1.0f ) * 100.0f ) ),
          y + 36 * s, 22, { 190, 185, 200, alpha } );
}

}   // namespace spaceopera
