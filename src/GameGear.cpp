/**
 * @file GameGear.cpp
 * @brief What the gear does beyond its numbers: the energy shield of the emitter (it takes hits before the health and
 *        fills again after a pause), the sockets' modules, the sets (more for each piece worn together, a power with
 *        all four) and the uniques' powers - what happens when the hero hits, kills or is hit, a drone that flies
 *        along, loot drawn from afar, a ring of fire when the gun runs hot.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"
#include "Journal.h"

#include "Items.h"
#include "Skills.h"
#include "Text.h"

#include <raymath.h>

#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float ShieldPause = 3.0f;       ///< seconds without a hit before the shield fills again
constexpr float ShieldFill = 0.22f;       ///< of the whole shield per second
constexpr float CompanionEvery = 1.2f;    ///< the drone's shots
constexpr float CompanionReach = 11.0f;   ///< ... and how far it reaches
constexpr float NovaChance = 0.15f;       ///< a ring when hit
constexpr float GhostChance = 0.1f;       ///< a hit that goes through nothing
constexpr float DoubleChance = 0.15f;     ///< a hit that lands twice
constexpr float CullBelow = 0.12f;        ///< what is this nearly dead dies
constexpr int   BlastEvery = 10;          ///< every tenth hit a blast
constexpr int   RageMost = 10;            ///< rage stacks this high
constexpr float LastStandBelow = 0.3f;    ///< badly hurt below this

}   // namespace

int Game::gearIndex( const Item &it ) const
{
    // Where an item goes on the hero: the slot of its kind, an implant in a free socket of the two first.
    switch ( itemSlot( it ) )
    {
    case SlotE::WEAPON: return 0;
    case SlotE::ARMOR: return 1;
    case SlotE::HELMET: return 2;
    case SlotE::GLOVES: return 3;
    case SlotE::BOOTS: return 4;
    case SlotE::IMPLANT: return !m_progress.m_worn[5] ? 5 : ( !m_progress.m_worn[6] ? 6 : 5 );
    case SlotE::HARNESS: return 7;
    case SlotE::EMITTER: return 8;
    case SlotE::CELL: return 9;
    default: return -1;
    }
}

void Game::gearStats( HeroStats &s, int &skillBonus )
{
    // The modules in the sockets, the sets by how many of their pieces are worn, the powers of what is worn.
    for ( bool &p : m_power ) p = false;
    int pieces[16] = {};
    for ( int i = 0; i < EquipSlots; ++i )
    {
        if ( !m_progress.m_worn[i] ) continue;
        const Item &it = m_progress.m_gear[i];
        for ( const Affix &a : it.m_mods ) s.m_add[(int)a.m_stat] += a.m_value;
        int set = itemSet( it );
        if ( set >= 0 && set < 16 ) ++pieces[set];
        m_power[(int)itemPower( it )] = true;
        if ( itemSlot( it ) == SlotE::EMITTER ) s.m_shield += itemShield( it );
    }
    const std::vector<SetDef> &sets = itemSets();
    for ( size_t k = 0; k < sets.size() && k < 16; ++k )
    {
        m_setPieces[k] = pieces[k];
        if ( pieces[k] >= 4 ) journal::win( "set_full" );
        if ( pieces[k] >= 2 )
            for ( const Affix &a : sets[k].m_two ) s.m_add[(int)a.m_stat] += a.m_value;
        if ( pieces[k] >= 3 )
            for ( const Affix &a : sets[k].m_three ) s.m_add[(int)a.m_stat] += a.m_value;
        if ( pieces[k] >= 4 ) m_power[(int)sets[k].m_four] = true;
    }
    m_power[(int)PowerE::NONE] = false;
    skillBonus = (int)s.m_add[(int)StatE::SKILLS];
    s.m_shield += s.m_add[(int)StatE::SHIELD];
    if ( m_power[(int)PowerE::PIERCE] ) s.m_pierce += 2;
    s.m_splash *= 1.0f + s.m_add[(int)StatE::AREA] / 100.0f;
}

float Game::areaScale() const
{
    return 1.0f + stat( StatE::AREA ) / 100.0f;
}

float Game::skillPower() const
{
    return 1.0f + ( stat( StatE::DAMAGE ) + stat( StatE::SKILL_DAMAGE ) ) / 100.0f;
}

int Game::maxStims() const
{
    return 9 + (int)stat( StatE::STIM_SLOTS );
}

float Game::hitScale( const Foe &f ) const
{
    // Against machines or people, badly hurt, raging.
    float k = 1.0f + ( human( f ) ? stat( StatE::VS_PEOPLE ) : stat( StatE::VS_MACHINES ) ) / 100.0f;
    if ( m_power[(int)PowerE::LAST_STAND] && m_hp < hpMax() * LastStandBelow ) k *= 1.4f;
    if ( m_power[(int)PowerE::RAGE] ) k *= 1.0f + 0.04f * m_rage;
    return k;
}

void Game::onHit( Foe &f, float damage, Vector3 dir, Vector3 at, bool crit )
{
    if ( m_inPower ) return;
    m_inPower = true;
    // A crit arcs on to two more.
    if ( crit && m_power[(int)PowerE::CHAIN_CRIT] )
    {
        int     arcs = 0;
        Vector3 from = at;
        for ( Foe &o : m_foes )
        {
            if ( &o == &f || !o.m_alive || o.m_hidden || o.m_hacked > 0.0f || arcs >= 2 ) continue;
            Vector3 to = Vector3Add( o.m_pos, { 0.0f, foeHeight( o ), 0.0f } );
            if ( Vector3Distance( to, at ) > 6.0f || !los( from, to ) ) continue;
            m_tracers.push_back( { from, to, 0.12f } );
            sparks( to, { 0.0f, 1.0f, 0.0f }, 8, { 255, 240, 160, 255 }, 2.5f );
            hurtFoe( o, damage * 0.5f, Vector3Normalize( Vector3Subtract( to, from ) ) );
            ++arcs;
        }
    }
    if ( m_power[(int)PowerE::DOUBLE] && f.m_alive && rnd() < DoubleChance )
    {
        sparks( at, Vector3Negate( dir ), 8, { 255, 255, 255, 255 }, 3.0f );
        hurtFoe( f, damage, dir );
    }
    if ( m_power[(int)PowerE::CULL] && f.m_alive && f.m_elite != 2 && f.m_kind != FoeE::TAXATOR &&
         f.m_hp < f.m_hpMax * CullBelow )
    {
        sparks( at, { 0.0f, 1.0f, 0.0f }, 20, { 255, 80, 60, 255 }, 4.0f );
        hurtFoe( f, f.m_hp + 1.0f, dir );
    }
    if ( m_power[(int)PowerE::BLAST] && ++m_blastCount % BlastEvery == 0 ) blast( at, damage * 1.5f, 2.4f );
    if ( m_power[(int)PowerE::RAGE] )
    {
        m_rage = std::min( RageMost, m_rage + 1 );
        m_rageT = 4.0f;
    }
    m_inPower = false;
}

void Game::onKill( const Foe &f )
{
    if ( m_dead || m_level.m_peaceful ) return;
    // Health for the kill (the stat), and what the powers make of it.
    if ( stat( StatE::LIFE_KILL ) > 0.0f ) m_hp = fminf( hpMax(), m_hp + stat( StatE::LIFE_KILL ) );
    if ( m_power[(int)PowerE::HEAL_KILL] ) m_hp = fminf( hpMax(), m_hp + hpMax() * 0.04f );
    if ( m_power[(int)PowerE::SHIELD_KILL] ) m_shield = fminf( m_stats.m_shield, m_shield + m_stats.m_shield * 0.12f );
    if ( m_power[(int)PowerE::HASTE_KILL] ) m_haste = 3.0f;
    if ( m_power[(int)PowerE::COOLDOWN_KILL] )
    {
        for ( float &c : m_cool ) c = fmaxf( 0.0f, c - 0.6f );
        m_boltTimer -= 0.6f;
    }
    if ( m_power[(int)PowerE::EXPLODE_KILL] && !m_inPower )
    {
        m_inPower = true;
        Vector3 at = Vector3Add( f.m_pos, { 0.0f, 0.6f, 0.0f } );
        blast( at, m_stats.m_damage * 1.2f, 2.5f );
        m_inPower = false;
    }
}

bool Game::onHurt( float &amount, Vector3 from )
{
    // A hit that goes through nothing; the shield before the health; rings and thorns for whoever is near.
    if ( m_power[(int)PowerE::GHOST] && rnd() < GhostChance )
    {
        for ( int i = 0; i < 12; ++i )
            m_particles.push_back(
                { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.8f, 0.4f + rnd() * 1.4f, ( rnd() - 0.5f ) * 0.8f } ),
                  { 0.0f, 0.5f, 0.0f },
                  0.5f,
                  0.5f,
                  0.03f,
                  { 200, 210, 255, 160 },
                  3 } );
        return false;
    }
    m_shieldWait = ShieldPause;
    if ( m_shield > 0.0f )
    {
        float take = fminf( m_shield, amount );
        m_shield -= take;
        amount -= take;
        Vector3 at = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
        Vector3 d = Vector3Subtract( from, at );
        if ( Vector3Length( d ) > 1e-3f ) at = Vector3Add( at, Vector3Scale( Vector3Normalize( d ), 0.7f ) );
        sparks( at, { 0.0f, 1.0f, 0.0f }, 10, { 120, 200, 255, 255 }, 2.0f );
        m_shieldFlash = 1.0f;
    }
    if ( m_power[(int)PowerE::NOVA_HURT] && rnd() < NovaChance ) nova( 4.0f, m_stats.m_damage * 0.8f, false );
    if ( m_power[(int)PowerE::THORN_NOVA] && stat( StatE::THORNS ) > 0.0f )
        for ( Foe &f : m_foes )
        {
            Vector3 d = Vector3Subtract( f.m_pos, m_pos );
            float   len = Vector3Length( d );
            if ( f.m_alive && !f.m_hidden && f.m_hacked <= 0.0f && len < 3.2f )
                hurtFoe( f, stat( StatE::THORNS ) * 2.0f,
                         len > 1e-3f ? Vector3Scale( d, 1.0f / len ) : Vector3{ 0, 0, 1 } );
        }
    return true;
}

void Game::nova( float radius, float damage, bool fireRing )
{
    // A ring from the hero: shock stuns, fire burns.
    radius *= areaScale();
    Color c = fireRing ? Color{ 255, 150, 60, 255 } : Color{ 150, 210, 255, 255 };
    for ( int i = 0; i < 40; ++i )
    {
        float a = i * PI * 2.0f / 40.0f;
        m_particles.push_back( { Vector3Add( m_pos, { 0.0f, 1.0f, 0.0f } ),
                                 { cosf( a ) * radius * 1.8f, 0.2f, sinf( a ) * radius * 1.8f },
                                 0.5f,
                                 0.5f,
                                 0.03f,
                                 c,
                                 3 } );
    }
    m_audio.play( fireRing ? SfxE::BOOM : SfxE::ZAP, m_pos, m_camTarget, 0.8f );
    for ( Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden || f.m_hacked > 0.0f ) continue;
        Vector3 d = { f.m_pos.x - m_pos.x, 0.0f, f.m_pos.z - m_pos.z };
        float   len = Vector3Length( d );
        if ( len > radius + foeRadius( f ) ) continue;
        Vector3 dir = len > 1e-3f ? Vector3Scale( d, 1.0f / len ) : Vector3{ 0.0f, 0.0f, 1.0f };
        hurtFoe( f, damage, dir );
        if ( fireRing )
        {
            f.m_burn = fmaxf( f.m_burn, damage * 0.3f );
            f.m_burnTime = 3.0f;
        }
        else
            f.m_stun = fmaxf( f.m_stun, 1.2f );
    }
}

void Game::overheated()
{
    if ( m_power[(int)PowerE::OVERHEAT_NOVA] ) nova( 4.0f, m_stats.m_damage * 2.0f, true );
}

bool Game::secondLife()
{
    // Once a level, what would have killed leaves one standing: a flash, the shield full, a moment untouchable.
    if ( !m_power[(int)PowerE::SECOND_LIFE] || m_secondUsed ) return false;
    m_secondUsed = true;
    m_hp = 1.0f;
    m_shield = m_stats.m_shield;
    m_invuln = 2.0f;
    m_flashes.push_back( { Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), { 20.0f, 24.0f, 30.0f }, 8.0f, 0.4f, 0.4f } );
    sparks( Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 40, { 230, 240, 255, 255 }, 4.0f );
    fire( "secondlife" );
    return true;
}

void Game::updateGear( float dt )
{
    // The shield fills after a pause; haste and rage run out; the drone shoots.
    if ( m_shieldWait > 0.0f )
        m_shieldWait -= dt;
    else if ( m_shield < m_stats.m_shield )
        m_shield =
            fminf( m_stats.m_shield, m_shield + m_stats.m_shield * ShieldFill *
                                                    fmaxf( 0.2f, 1.0f + stat( StatE::SHIELD_RATE ) / 100.0f ) * dt );
    m_shieldFlash = fmaxf( 0.0f, m_shieldFlash - dt * 3.0f );
    m_haste = fmaxf( 0.0f, m_haste - dt );
    m_rageT -= dt;
    if ( m_rageT <= 0.0f ) m_rage = 0;
    // What lies about comes from far.
    if ( m_power[(int)PowerE::MAGNET] && !m_dead )
        for ( Loot &l : m_loot )
        {
            Vector3 d = Vector3Subtract( Vector3Add( m_pos, { 0.0f, 0.4f, 0.0f } ), l.m_pos );
            float   len = Vector3Length( d );
            if ( l.m_alive && l.m_t > 0.6f && len < 9.0f && len > 0.5f )
                l.m_pos = Vector3Add( l.m_pos, Vector3Scale( d, fminf( 1.0f, dt * 4.0f / len ) ) );
        }
    // The drone: round the hero's shoulder, a shot at the nearest every so often.
    if ( !m_power[(int)PowerE::COMPANION] || m_dead )
    {
        m_compPos = Vector3Add( m_pos, { 0.0f, 2.2f, 0.0f } );
        return;
    }
    float   a = m_time * 1.7f;
    Vector3 want = Vector3Add( m_pos, { cosf( a ) * 1.1f, 2.1f + 0.15f * sinf( m_time * 3.0f ), sinf( a ) * 1.1f } );
    m_compPos = Vector3Lerp( m_compPos, want, fminf( 1.0f, dt * 6.0f ) );
    m_compT -= dt;
    if ( m_compT > 0.0f || m_level.m_peaceful ) return;
    m_compT = CompanionEvery;
    Foe  *best = nullptr;
    float bestDist = CompanionReach;
    for ( Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden || !f.m_awake || f.m_hacked > 0.0f ) continue;
        Vector3 at = Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } );
        float   d = Vector3Distance( at, m_compPos );
        if ( d < bestDist && los( m_compPos, at ) ) bestDist = d, best = &f;
    }
    if ( !best ) return;
    Vector3 at = Vector3Add( best->m_pos, { 0.0f, foeHeight( *best ), 0.0f } );
    m_tracers.push_back( { m_compPos, at, 0.1f } );
    sparks( at, { 0.0f, 1.0f, 0.0f }, 6, { 255, 210, 120, 255 }, 2.0f );
    m_audio.play( SfxE::ZAP, m_compPos, m_camTarget, 0.35f );
    hurtFoe( *best, m_stats.m_damage * 0.35f * weaponBoost(), Vector3Normalize( Vector3Subtract( at, m_compPos ) ) );
}

void Game::poseGear( std::vector<Drawable> &out )
{
    // The companion drone, if a power brings one; the shield's shimmer when it takes a hit.
    if ( m_power[(int)PowerE::COMPANION] && !m_dead && m_moteMesh.vertexCount > 0 )
    {
        Drawable d;
        Matrix   m = MatrixMultiply( MatrixScale( 3.0f, 3.0f, 3.0f ),
                                     MatrixMultiply( MatrixRotateY( m_time * 2.0f ),
                                                     MatrixTranslate( m_compPos.x, m_compPos.y, m_compPos.z ) ) );
        d.m_transform = m;
        d.m_mesh = m_moteMesh;
        d.m_surface = S_METAL;
        d.m_sunShadow = d.m_spotShadow = false;
        out.push_back( d );
        d.m_mesh = m_moteLight;
        d.m_surface = S_GLOW;
        d.m_emissive = { 3.0f, 1.8f, 0.4f, 0.0f };
        out.push_back( d );
    }
}

bool Game::insertModule( int bagIndex )
{
    // A module goes into the first free socket that takes it: attack modules into the weapon, the rest into the suit,
    // the helmet or the emitter.
    if ( bagIndex < 0 || bagIndex >= (int)m_progress.m_bag.size() ) return false;
    const Item &mod = m_progress.m_bag[bagIndex];
    if ( itemSlot( mod ) != SlotE::MODULE || mod.m_affixes.empty() ) return false;
    StatE st = mod.m_affixes[0].m_stat;
    bool  attack =
        st == StatE::DAMAGE || st == StatE::CRIT || st == StatE::RATE || st == StatE::BURN || st == StatE::SHOCK;
    const int  attackSlots[1] = { 0 }, defenceSlots[3] = { 1, 2, 8 };
    const int *slots = attack ? attackSlots : defenceSlots;
    int        count = attack ? 1 : 3;
    for ( int k = 0; k < count; ++k )
    {
        int slot = slots[k];
        if ( !m_progress.m_worn[slot] ) continue;
        Item &it = m_progress.m_gear[slot];
        if ( (int)it.m_mods.size() >= it.m_sockets ) continue;
        it.m_mods.push_back( mod.m_affixes[0] );
        journal::win( "module" );
        m_toast = TextFormat( tr( "module.set" ), itemName( mod ).c_str(), itemName( it ).c_str() );
        m_toastColor = { 140, 230, 255, 255 };
        m_toastTimer = 2.5f;
        m_progress.m_bag.erase( m_progress.m_bag.begin() + bagIndex );
        m_audio.play( SfxE::HACK, m_pos, m_camTarget, 0.6f );
        float part = m_hp / fmaxf( 1.0f, hpMax() );
        refreshStats();
        m_hp = fminf( hpMax(), part * hpMax() );
        return true;
    }
    m_toast = tr( attack ? "module.noweapon" : "module.noroom" );
    m_toastColor = { 255, 140, 110, 255 };
    m_toastTimer = 2.0f;
    return false;
}

void Game::dropModule( Vector3 at )
{
    if ( m_tale ) return;
    unsigned seed = (unsigned)m_progress.m_seed;
    Loot     l;
    l.m_item = makeModule( std::max( 1, areaLevel() + (int)( rnd() * 3.0f ) - 1 ), seed );
    m_progress.m_seed = (int)( seed & 0x7fffffff );
    l.m_pos = at;
    l.m_vel = { ( rnd() - 0.5f ) * 2.5f, 3.0f, ( rnd() - 0.5f ) * 2.5f };
    m_loot.push_back( l );
}

void Game::testGear( const std::string &list )
{
    size_t at = 0;
    while ( at < list.size() )
    {
        size_t      comma = list.find( ',', at );
        std::string key = list.substr( at, comma == std::string::npos ? std::string::npos : comma - at );
        at = comma == std::string::npos ? list.size() : comma + 1;
        Item it;
        int  level = std::max( 1, m_progress.m_heroLevel );
        if ( key.size() > 1 && ( key[0] == 'u' || key[0] == 's' ) && isdigit( (unsigned char)key[1] ) )
            it = namedItem( atoi( key.c_str() + 1 ), key[0] == 's', level );
        else
        {
            if ( baseOf( key.c_str() ) < 0 ) continue;
            it.m_base = baseOf( key.c_str() );
            it.m_level = level;
            it.m_rarity = RarityE::RARE;
            it.m_name = 0;
        }
        m_progress.m_bag.push_back( it );
        equip( (int)m_progress.m_bag.size() - 1 );
    }
    m_hp = hpMax();
    m_shield = m_stats.m_shield;
}

const Texture2D *Game::pictureOf( const Item &it )
{
    std::string name = itemPicture( it );
    auto        known = m_itemPics.find( name );
    if ( known == m_itemPics.end() )
    {
        std::string path = m_assets + "/textures/items/" + name + ".png";
        Texture2D   t = {};
        if ( FileExists( path.c_str() ) )
        {
            t = LoadTexture( path.c_str() );
            GenTextureMipmaps( &t );
            SetTextureFilter( t, TEXTURE_FILTER_TRILINEAR );
        }
        known = m_itemPics.emplace( name, t ).first;
    }
    return known->second.id ? &known->second : nullptr;
}

void Game::drawShieldHud( float x, float y, float bw, float s )
{
    // The shield over the health bar: a thin cyan band, brighter when it has just taken a hit.
    if ( m_stats.m_shield <= 0.0f ) return;
    float         part = Clamp( m_shield / m_stats.m_shield, 0.0f, 1.0f );
    unsigned char g = (unsigned char)( 200 + 55 * m_shieldFlash );
    DrawRectangle( (int)x, (int)( y + 25 * s ), (int)bw, (int)( 5 * s ), { 10, 30, 45, 200 } );
    DrawRectangle( (int)x, (int)( y + 25 * s ), (int)( bw * part ), (int)( 5 * s ), { 90, g, 255, 255 } );
}

}   // namespace spaceopera
