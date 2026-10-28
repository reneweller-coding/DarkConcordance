/**
 * @file GameVault.cpp
 * @brief The Core Bank (level 5): racks of stolen cores blinking like a night sky (they break when hit, and every core
 *        is someone); shells, emptied bodies a stolen core drives, shambling and scratching (skinned figures with the
 *        animation library's zombie clips); and Mother Sevenfold, one mind in seven bodies, linked by radio through
 *        three relays: while one stands, her bodies barely bleed and mend.
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

namespace
{

constexpr float ShellScratch = 8.0f;     ///< a shell's blow
constexpr float MotherScratch = 10.0f;   ///< a body of Mother Sevenfold's
constexpr float RelayHp = 900.0f;
constexpr float MotherHp = 1100.0f;

}   // namespace

void Game::loadVault()
{
    // The racks: tall cabinets, a grid of tiny lights on their fronts; they stand in the way and stop shots.
    for ( const Marker &m : m_level.m_markers )
        if ( m.m_kind == '|' )
        {
            m_racks.push_back( { m.m_pos, true, ( m.m_row * 7 + m.m_col * 13 ) % 17 * 0.37f } );
            m_blocked.insert( m.m_row * m_level.cols() + m.m_col );
            m_level.m_props.push_back( { m.m_pos, 0.55f } );
        }
    if ( !m_racks.empty() )
    {
        MeshBuilder rack;
        rack.box( place( { 0.0f, 1.15f, 0.0f } ), { 0.5f, 1.15f, 0.45f } );
        rack.box( place( { 0.0f, 2.32f, 0.0f } ), { 0.55f, 0.03f, 0.5f } );
        m_rackMesh = rack.build().front();
        MeshBuilder lights;
        for ( int side : { -1, 1 } )
            for ( int row = 0; row < 9; ++row )
                for ( int col = 0; col < 4; ++col )
                    lights.box( place( { -0.33f + col * 0.22f, 0.35f + row * 0.21f, side * 0.46f } ),
                                { 0.035f, 0.02f, 0.01f } );
        m_rackLights = lights.build().front();
    }
    // Mother Sevenfold's hall: three relays (boss parts) and seven bodies.
    if ( !m_level.m_relays.empty() )
    {
        float tough = toughness( m_difficulty ) * ( 1.0f + 0.25f * ( m_level.m_area - 1 ) );
        for ( auto rc : m_level.m_relays )
        {
            Vector3 p = m_level.centre( rc.first, rc.second );
            m_parts.push_back(
                { "mother.relay", p, 1.1f, RelayHp * tough, RelayHp * tough, Vector3Add( p, { 0.0f, 3.1f, 0.0f } ) } );
            m_blocked.insert( rc.first * m_level.cols() + rc.second );
            m_level.m_props.push_back( { p, 0.5f } );
        }
        for ( auto rc : m_level.m_mothers )
        {
            spawnFoe( FoeE::SHELL, m_level.centre( rc.first, rc.second ), false );
            Foe &f = m_foes.back();
            f.m_variant = 2;
            f.m_elite = 2;
            f.m_hp = f.m_hpMax = MotherHp * tough;
        }
        m_motherOn = true;
        MeshBuilder relay;
        relay.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.45f, 0.4f, 14 );
        relay.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.12f, 3.0f, 10 );
        for ( int i = 0; i < 3; ++i )
            relay.box( place( { 0.0f, 2.2f + i * 0.3f, 0.0f }, { 0.0f, i * 1.05f, 0.0f } ),
                       { 0.5f - i * 0.1f, 0.02f, 0.02f } );
        m_relayMesh = relay.build().front();
        MeshBuilder top;
        top.roundedBox( place( { 0.0f, 3.15f, 0.0f } ), { 0.22f, 0.22f, 0.22f }, 0.1f, 2 );
        m_relayTop = top.build().front();
    }
    // The shells' bodies: Quaternius' figures in rags, pale, a core glowing at the back of the head.
    for ( Foe &f : m_foes )
    {
        if ( f.m_kind != FoeE::SHELL || !m_skins ) continue;
        Person   q = {};
        unsigned seed = (unsigned)f.m_id * 2654435761u;
        q.m_pos = f.m_pos;
        q.m_yaw = f.m_yaw;
        q.m_look = 0;
        q.m_body = (int)( seed % 2 );
        q.m_hair = f.m_variant == 2 ? -1 : (int)( ( seed >> 3 ) % 4 ) - 1;
        q.m_tone = 1;
        q.m_hairColour = 4;
        q.m_cloth = f.m_variant == 2 ? Vector3{ 0.32f, 0.05f, 0.06f } : Vector3{ 0.16f, 0.17f, 0.19f };
        q.m_scale = f.m_variant == 2 ? 1.08f : 0.97f + ( seed % 5 ) * 0.01f;
        q.m_skin = SkinState();
        q.m_skin.m_clip = m_clip["Idle_Loop"];
        q.m_skin.m_time = ( seed % 97 ) * 0.11f;
        f.m_body = (int)m_shellBodies.size();
        m_shellBodies.push_back( q );
    }
    MeshBuilder core;
    core.roundedBox( place( { 0.0f, 0.0f, 0.0f } ), { 0.06f, 0.06f, 0.04f }, 0.02f, 2 );
    m_coreGlow = core.build().front();
}

bool Game::motherLinked() const
{
    if ( !m_motherOn ) return false;
    for ( const BossPart &p : m_parts )
        if ( std::string( p.m_key ) == "mother.relay" && p.m_hp > 0.0f ) return true;
    return false;
}

bool Game::rackInWay( Vector3 a, Vector3 b ) const
{
    if ( m_racks.empty() ) return false;
    Vector3 d = Vector3Subtract( b, a );
    float   len = Vector3Length( d );
    if ( len < 1e-3f ) return false;
    d = Vector3Scale( d, 1.0f / len );
    for ( const Rack &r : m_racks )
    {
        if ( !r.m_alive ) continue;
        float t = rayCylinder( a, d, r.m_pos, 0.5f, 2.3f );
        if ( t >= 0.0f && t < len - 0.5f ) return true;
    }
    return false;
}

void Game::breakRack( int i )
{
    // A rack broken: its lights go out in a spray of sparks; every one of those cores was someone.
    Rack &r = m_racks[i];
    if ( !r.m_alive ) return;
    r.m_alive = false;
    ++m_racksBroken;
    Vector3 at = Vector3Add( r.m_pos, { 0.0f, 1.3f, 0.0f } );
    sparks( at, { 0.0f, 1.0f, 0.0f }, 40, { 170, 215, 255, 255 }, 4.0f );
    m_flashes.push_back( { at, { 10.0f, 16.0f, 26.0f }, 5.0f, 0.3f, 0.3f } );
    m_audio.play( SfxE::CRACKLE, at, m_camTarget, 0.9f );
    fire( "rack" );
}

void Game::updateShell( Foe &f, float dt, Vector3 dir, float dist, Vector3 path, Vector3 &want, float &faceYaw )
{
    // It shambles at the hero and scratches; Mother's bodies are quicker and hit harder (slower while the masts still
    // pull at them).
    bool mother = f.m_variant == 2;
    if ( f.m_swingT >= 0.0f )
    {
        float before = f.m_swingT;
        f.m_swingT += dt / 1.0f;
        faceYaw = atan2f( dir.x, dir.z );
        if ( before < 0.45f && f.m_swingT >= 0.45f && dist < 1.9f && fabsf( m_pos.y - f.m_pos.y ) < 1.5f )
            hurtHero( mother ? MotherScratch : ShellScratch, { dir.x * 1.5f, 0.2f, dir.z * 1.5f }, f.m_pos );
        if ( f.m_swingT >= 1.0f ) f.m_swingT = -1.0f, f.m_timer = mother ? 0.5f : 0.6f;
        return;
    }
    want = Vector3Scale( path, mother ? ( motherLinked() ? 1.8f : 2.2f ) : 1.7f );
    faceYaw = dist < 4.0f ? atan2f( dir.x, dir.z ) : atan2f( path.x, path.z );
    if ( dist < 1.6f && f.m_los && f.m_timer <= 0.0f ) f.m_swingT = 0.0f;
}

void Game::updateVault( float dt )
{
    // The shells' bodies follow their foes: walking, scratching, falling; the dead lie a while, then sink away.
    if ( m_skins )
        for ( Foe &f : m_foes )
        {
            if ( f.m_kind != FoeE::SHELL || f.m_body < 0 ) continue;
            Person          &q = m_shellBodies[f.m_body];
            const SkinModel &m = m_body[q.m_body];
            q.m_pos = f.m_pos;
            q.m_yaw = f.m_yaw;
            float speed = sqrtf( f.m_vel.x * f.m_vel.x + f.m_vel.z * f.m_vel.z );
            if ( !f.m_alive )
            {
                f.m_down += dt;
                int death = m_clip["Death01"];
                m.play( q.m_skin, death, 0.2f );
                if ( q.m_skin.m_time < m.length( death ) - 0.05f ) m.advance( q.m_skin, dt );
                if ( f.m_down > 8.0f ) q.m_pos.y -= ( f.m_down - 8.0f ) * 0.6f;
                continue;
            }
            int   clip = m_clip["Idle_Loop"];
            float rate = 1.0f;
            if ( f.m_swingT >= 0.0f )
                clip = m_clip["Zombie_Scratch"];
            else if ( speed > 0.3f )
                clip = m_clip["Zombie_Walk_Fwd_Loop"], rate = speed / ( f.m_variant == 2 ? 1.6f : 1.1f );
            if ( clip < 0 ) clip = m_clip["Walk_Loop"];
            m.play( q.m_skin, clip, 0.25f );
            m.advance( q.m_skin, dt, rate );
        }
    if ( !m_motherOn || m_motherDone ) return;
    // Mother Sevenfold: awake once the hero comes near; while a relay stands her bodies mend.
    bool linked = motherLinked();
    int  alive = 0;
    for ( Foe &f : m_foes )
    {
        if ( f.m_kind != FoeE::SHELL || f.m_variant != 2 || !f.m_alive ) continue;
        ++alive;
        if ( !m_motherAwake && Vector3Distance( f.m_pos, m_pos ) < 18.0f )
        {
            m_motherAwake = true;
            fire( "bossstart" );
            for ( Foe &o : m_foes )
                if ( o.m_kind == FoeE::SHELL && o.m_variant == 2 ) o.m_awake = true;
        }
        if ( linked ) f.m_hp = fminf( f.m_hpMax, f.m_hp + f.m_hpMax * 0.025f * dt );
        // The link: a thin line from the nearest relay to the body, flickering.
        if ( linked && m_motherAwake && rnd() < 0.5f )
        {
            int   near = -1;
            float best = 1e9f;
            for ( int i = 0; i < (int)m_parts.size(); ++i )
                if ( m_parts[i].m_hp > 0.0f && Vector3Distance( m_parts[i].m_pos, f.m_pos ) < best )
                    best = Vector3Distance( m_parts[i].m_pos, f.m_pos ), near = i;
            if ( near >= 0 )
                m_tracers.push_back( { m_parts[near].m_pos, Vector3Add( f.m_pos, { 0.0f, 1.7f, 0.0f } ), 0.05f } );
        }
    }
    if ( m_motherAwake && !linked && !m_fired.count( "unlinked" ) )
    {
        // The last relay down: every body staggers.
        fire( "unlinked" );
        for ( Foe &f : m_foes )
            if ( f.m_kind == FoeE::SHELL && f.m_variant == 2 && f.m_alive ) f.m_stun = 2.0f, f.m_swingT = -1.0f;
    }
    if ( alive == 0 && m_motherAwake )
    {
        m_motherDone = true;
        m_bossAt = m_pos;
        fire( "bossdead" );
        fire( m_racksBroken == 0 ? "clean" : "broken" );
        if ( m_racksBroken == 0 ) m_progress.m_flags.insert( "cores.spared" );
        for ( int i = 0; i < 5; ++i ) dropItem( Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), 5.0f );
        m_pickups.push_back( { 0, 90, Vector3Add( m_pos, { 0.0f, 1.0f, 0.0f } ), { 0.5f, 3.0f, 0.5f } } );
        gainXp( (int)( 900 * ( 1.0f + 0.15f * ( areaLevel() - 1 ) ) ) );
        m_shake = 0.8f;
    }
}

void Game::poseVault( std::vector<Drawable> &out )
{
    for ( size_t i = 0; i < m_racks.size(); ++i )
    {
        const Rack &r = m_racks[i];
        Drawable    d;
        d.m_mesh = m_rackMesh;
        d.m_surface = r.m_alive ? S_HULL : S_RUST;
        d.m_transform = MatrixTranslate( r.m_pos.x, r.m_pos.y, r.m_pos.z );
        out.push_back( d );
        if ( !r.m_alive ) continue;
        // Each rack's lights breathe on their own: a field of them twinkles like stars.
        float tw = 0.55f + 0.45f * sinf( m_time * ( 0.7f + 0.13f * ( i % 7 ) ) + r.m_phase ) *
                               sinf( m_time * 2.3f + r.m_phase * 3.0f );
        d.m_mesh = m_rackLights;
        d.m_surface = S_GLOW;
        d.m_emissive = { 1.2f * tw, 2.6f * tw, 4.0f * tw, 0.0f };
        d.m_spotShadow = d.m_sunShadow = false;
        out.push_back( d );
    }
    for ( const BossPart &p : m_parts )
    {
        if ( std::string( p.m_key ) != "mother.relay" ) continue;
        Vector3  base = { p.m_pos.x, p.m_pos.y - 3.1f, p.m_pos.z };
        Drawable d;
        d.m_mesh = m_relayMesh;
        d.m_surface = p.m_hp > 0.0f ? S_METAL : S_RUST;
        d.m_transform = MatrixTranslate( base.x, base.y, base.z );
        out.push_back( d );
        d.m_mesh = m_relayTop;
        d.m_surface = S_GLOW;
        float pulse = 0.6f + 0.4f * sinf( m_time * 5.0f + base.x );
        d.m_emissive = p.m_hp > 0.0f ? Vector4{ 1.0f * pulse, 2.8f * pulse, 5.0f * pulse, 0.0f }
                                     : Vector4{ 0.05f, 0.05f, 0.05f, 0.0f };
        out.push_back( d );
    }
    if ( !m_skins ) return;
    for ( Foe &f : m_foes )
    {
        if ( f.m_kind != FoeE::SHELL || f.m_body < 0 || f.m_hidden || ( !f.m_alive && f.m_down > 11.0f ) ) continue;
        Person &q = m_shellBodies[f.m_body];
        poseSkin( q, out );
        if ( !f.m_alive ) continue;
        // The core at the back of the head: blue in a shell, red in Mother's bodies, white when hit.
        Vector3  back = { -sinf( f.m_yaw ) * 0.13f, 1.66f * q.m_scale, -cosf( f.m_yaw ) * 0.13f };
        Drawable d;
        d.m_mesh = m_coreGlow;
        d.m_surface = S_GLOW;
        d.m_transform = MatrixMultiply( MatrixRotateY( f.m_yaw ),
                                        MatrixTranslate( f.m_pos.x + back.x, f.m_pos.y + back.y, f.m_pos.z + back.z ) );
        float w = 4.0f * f.m_flash;
        d.m_emissive = f.m_variant == 2 ? Vector4{ 3.0f + w, 0.3f + w, 0.2f + w, 0.0f }
                                        : Vector4{ 0.5f + w, 1.3f + w, 2.5f + w, 0.0f };
        d.m_spotShadow = d.m_sunShadow = false;
        out.push_back( d );
    }
}

void Game::drawVaultHud( int w, int h, float s )
{
    if ( !motherFighting() ) return;
    auto text = [&]( const char *t, float x, float y, float size, Color c )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        DrawTextEx( m_font, t, { x - m.x * 0.5f + 2.0f * s, y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
        DrawTextEx( m_font, t, { x - m.x * 0.5f, y }, size * s, 1.0f, c );
    };
    // Seven segments for her seven bodies; below them the relays that hold the link, a marker over each.
    float pw = 640.0f * s, px = w * 0.5f - pw * 0.5f, py = 40.0f * s, gap = 6.0f * s;
    float part = ( pw - 6.0f * gap ) / 7.0f;
    text( bossTitle( "boss.mother" ), w * 0.5f, py - 4.0f * s, 24, { 255, 210, 150, 240 } );
    int k = 0;
    for ( const Foe &f : m_foes )
    {
        if ( f.m_kind != FoeE::SHELL || f.m_variant != 2 || k >= 7 ) continue;
        float bx = px + k++ * ( part + gap );
        DrawRectangle( (int)bx, (int)( py + 26 * s ), (int)part, (int)( 12 * s ), { 30, 10, 0, 200 } );
        if ( f.m_alive )
            DrawRectangle( (int)bx, (int)( py + 26 * s ), (int)( part * Clamp( f.m_hp / f.m_hpMax, 0.0f, 1.0f ) ),
                           (int)( 12 * s ), motherLinked() ? Color{ 120, 170, 255, 255 } : Color{ 255, 120, 60, 255 } );
    }
    int relays = 0;
    for ( const BossPart &p : m_parts )
    {
        if ( std::string( p.m_key ) != "mother.relay" || p.m_hp <= 0.0f ) continue;
        ++relays;
        float   pulse = 0.6f + 0.4f * sinf( m_time * 6.0f );
        Vector2 q = GetWorldToScreenEx( Vector3Add( p.m_pos, { 0.0f, 0.9f, 0.0f } ), m_cam, w, h );
        DrawPolyLinesEx( q, 4, ( 16.0f + 6.0f * pulse ) * s, 45.0f, 3.0f * s,
                         { 120, 180, 255, (unsigned char)( 255 * pulse ) } );
    }
    text( relays > 0 ? TextFormat( tr( "hud.linked" ), relays ) : tr( "hud.unlinked" ), w * 0.5f, py + 42 * s, 16,
          relays > 0 ? Color{ 150, 190, 255, 230 } : Color{ 255, 170, 110, 230 } );
}

}   // namespace spaceopera
