/**
 * @file GameTactics.cpp
 * @brief How the enemies fight together: the ones that close in come from the sides and do not run in alone while
 *        others are on their way; the ones that shoot step behind cover when they are hit and keep to a ledge while
 *        they see the hero from up there.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include <raymath.h>

#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float WaitMost = 2.5f;    ///< how long one waits for the others, at most
constexpr float CoverTime = 3.2f;   ///< in cover: getting there and staying a while
constexpr int   CoverReach = 2;     ///< cells around it where cover is looked for

/** @brief What a foe is doing besides its own ways (Foe::m_tactic). */
enum TacticE
{
    NONE,
    WAITING,    ///< circles at a distance until the others are there
    TO_COVER,   ///< on its way behind cover
    IN_COVER,   ///< behind it
    HOLDING     ///< up on a ledge: stays there
};

}   // namespace

bool Game::findCover( const Foe &f, Vector3 &out ) const
{
    // The nearest floor cell close by from which the hero's chest cannot be seen, not taken by another.
    int r0, c0;
    m_level.cellOf( f.m_pos, r0, c0 );
    Vector3 chest = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
    float   best = 1e9f;
    for ( int dr = -CoverReach; dr <= CoverReach; ++dr )
        for ( int dc = -CoverReach; dc <= CoverReach; ++dc )
        {
            int r = r0 + dr, c = c0 + dc;
            if ( ( dr == 0 && dc == 0 ) || !m_level.walkable( r, c ) ) continue;
            Vector3 p = m_level.centre( r, c );
            p.y = m_level.floorAt( p.x, p.z );
            if ( fabsf( p.y - f.m_pos.y ) > 0.5f ) continue;
            float far = Vector2Distance( { p.x, p.z }, { m_pos.x, m_pos.z } );
            if ( far < 4.0f || far > 22.0f || los( Vector3Add( p, { 0.0f, 1.2f, 0.0f } ), chest ) ) continue;
            bool taken = false;
            for ( const Foe &o : m_foes )
                if ( &o != &f && o.m_alive &&
                     ( Vector2Distance( { o.m_cover.x, o.m_cover.z }, { p.x, p.z } ) < 0.9f ||
                       Vector2Distance( { o.m_pos.x, o.m_pos.z }, { p.x, p.z } ) < 0.8f ) )
                    taken = true;
            float d = Vector3Distance( p, f.m_pos );
            if ( !taken && d < best ) best = d, out = p;
        }
    return best < 1e8f;
}

void Game::tactics( Foe &f, float dt, Vector3 dir, float dist, Vector3 &path )
{
    f.m_tactic = NONE;
    int role = f.m_hacked > 0.0f ? 0 : foeRole( f );
    if ( role == 0 ) return;
    f.m_coverCool -= dt;
    if ( role == 2 )
    {
        // Hit: behind cover for a while (now and then), then out again to shoot.
        if ( f.m_coverT > 0.0f )
        {
            f.m_coverT -= dt;
            f.m_tactic =
                Vector2Distance( { f.m_pos.x, f.m_pos.z }, { f.m_cover.x, f.m_cover.z } ) > 0.45f ? TO_COVER : IN_COVER;
            if ( f.m_coverT <= 0.0f ) f.m_coverCool = 5.0f + rnd() * 3.0f, f.m_cover = { 0.0f, -1000.0f, 0.0f };
            return;
        }
        if ( f.m_flash > 0.0f && f.m_coverCool <= 0.0f && dist < 18.0f && f.m_elite != 2 )
        {
            if ( findCover( f, f.m_cover ) )
            {
                f.m_coverT = CoverTime;
                f.m_tactic = TO_COVER;
                return;
            }
            f.m_coverCool = 2.0f;
        }
        // Up on a ledge, seeing the hero: it stays up there and shoots.
        if ( f.m_los && f.m_ground && dist < 20.0f && f.m_pos.y > m_pos.y + 0.9f ) f.m_tactic = HOLDING;
        return;
    }
    // One that closes in: alone near the hero while others are still coming, it waits for them a little.
    if ( f.m_elite == 0 && f.m_kind != FoeE::BOMBER && f.m_waitT < WaitMost && f.m_los && dist > 4.5f && dist < 12.0f )
    {
        int near = 0, coming = 0;
        for ( const Foe &o : m_foes )
        {
            if ( &o == &f || !o.m_alive || o.m_hidden || !o.m_awake || foeRole( o ) == 0 ) continue;
            float d = Vector3Distance( o.m_pos, f.m_pos );
            float far = Vector2Distance( { o.m_pos.x, o.m_pos.z }, { m_pos.x, m_pos.z } );
            if ( d < 5.0f )
                ++near;
            else if ( far > dist + 2.0f && far < 26.0f )
                ++coming;
        }
        if ( near == 0 && coming >= 2 )
        {
            f.m_waitT += dt;
            f.m_tactic = WAITING;
            float side = ( f.m_id % 2 ) ? 1.0f : -1.0f;
            path = { -dir.z * side, 0.0f, dir.x * side };
            return;
        }
    }
    // Close and in sight: two of three come from a side (the others straight on), so they arrive round the hero.
    int k = f.m_id % 3;
    if ( k != 0 && f.m_los && dist > 2.5f && dist < 10.0f )
    {
        float   a = ( k == 1 ? 0.9f : -0.9f ) * Clamp( ( dist - 2.5f ) / 5.0f, 0.0f, 1.0f );
        Vector3 turned = { dir.x * cosf( a ) - dir.z * sinf( a ), 0.0f, dir.x * sinf( a ) + dir.z * cosf( a ) };
        int     r, c;
        m_level.cellOf( Vector3Add( f.m_pos, Vector3Scale( turned, 1.4f ) ), r, c );
        if ( m_level.walkable( r, c ) ) path = turned, ++m_tacticFrames[5];
    }
}

void Game::tacticsAfter( Foe &f, Vector3 dir, Vector3 &want, float &faceYaw )
{
    ++m_tacticFrames[f.m_tactic];
    switch ( f.m_tactic )
    {
    case WAITING:
        want = Vector3Scale( want, 0.35f );
        faceYaw = atan2f( dir.x, dir.z );
        break;
    case TO_COVER:
    {
        Vector3 to = Vector3Subtract( f.m_cover, f.m_pos );
        to.y = 0.0f;
        float len = Vector3Length( to );
        want = len > 1e-3f ? Vector3Scale( to, 3.4f / len ) : Vector3{ 0.0f, 0.0f, 0.0f };
        if ( len > 1e-3f ) faceYaw = atan2f( to.x, to.z );
        break;
    }
    case IN_COVER:
        want = { 0.0f, 0.0f, 0.0f };
        faceYaw = atan2f( dir.x, dir.z );
        break;
    case HOLDING: want = { 0.0f, 0.0f, 0.0f }; break;
    default: break;
    }
}

}   // namespace spaceopera
