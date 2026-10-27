/**
 * @file GameLoot.cpp
 * @brief The hero's numbers from gear, level and skills; hits with crits, burning, stuns and leech; experience and
 *        levels; items dropped on the floor and picked up by walking over them; equipping and salvaging; the active
 *        skills (Corvin's overcharge, grenade and roll, Dorra's quake and charge; her bolts are her own).
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include "MeshBuilder.h"
#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float PickupReach = 1.3f;   ///< walking this near picks an item up
constexpr int   MaxLevel = 40;
constexpr float BaseCrit = 5.0f;   ///< % without anything

}   // namespace

void Game::testGifts( int items, int xp )
{
    // Half of them straight into the bag, the others on the floor round the hero (none in a tale: it is a memory).
    if ( m_tale ) return;
    for ( int i = 0; i < items; ++i )
    {
        float a = i * 2.39996f, r = 1.8f + 0.2f * i;
        dropItem( Vector3Add( m_pos, { cosf( a ) * r, 1.0f, sinf( a ) * r } ), 3.0f );
        m_loot.back().m_vel = { 0.0f, 0.0f, 0.0f };
        if ( i % 2 == 0 && (int)m_progress.m_bag.size() < BagSize )
        {
            m_progress.m_bag.push_back( m_loot.back().m_item );
            m_loot.pop_back();
        }
    }
    if ( xp > 0 ) gainXp( xp );
    m_levelUp = 0.0f;
}

int Game::rank( int skill ) const
{
    // A skill learnt gains the ranks the gear adds to all.
    int r = skill >= 0 && skill < (int)m_progress.m_ranks.size() ? m_progress.m_ranks[skill] : 0;
    return r > 0 ? r + (int)stat( StatE::SKILLS ) : 0;
}

void Game::refreshStats()
{
    HeroStats s;
    int       hero = heroIndex();
    // What is worn: the kinds' own bonuses, the affixes, the armour.
    for ( int i = 0; i < EquipSlots; ++i )
    {
        if ( !m_progress.m_worn[i] ) continue;
        const Item     &it = m_progress.m_gear[i];
        const ItemBase &b = itemBases()[it.m_base];
        if ( b.m_bonus != StatE::COUNT ) s.m_add[(int)b.m_bonus] += b.m_bonusValue;
        for ( const Affix &a : it.m_affixes ) s.m_add[(int)a.m_stat] += a.m_value;
        s.m_armor += itemArmor( it );
    }
    // The sockets, the sets, the powers, the shield.
    int skillBonus = 0;
    gearStats( s, skillBonus );
    // The passive skills (with the ranks the gear adds to all that are learnt).
    const std::vector<SkillDef> &defs = skillDefs();
    for ( size_t k = 0; k < defs.size(); ++k )
    {
        int r = k < m_progress.m_ranks.size() ? m_progress.m_ranks[k] : 0;
        if ( defs[k].m_hero == hero && defs[k].m_slot == 0 && defs[k].m_stat != StatE::COUNT && r > 0 )
            s.m_add[(int)defs[k].m_stat] += defs[k].m_perRank * ( r + skillBonus );
    }
    // The weapon, as its kind fights, made stronger by the workshop, the affixes and the hero's level.
    Item            weapon = m_progress.m_worn[0] ? m_progress.m_gear[0] : startItem( hero, SlotE::WEAPON );
    const ItemBase &b = itemBases()[weapon.m_base];
    float           level = 1.0f + 0.02f * ( m_progress.m_heroLevel - 1 );
    s.m_damage = itemDamage( weapon ) * weaponBoost() * ( 1.0f + s.m_add[(int)StatE::DAMAGE] / 100.0f ) * level;
    s.m_interval = b.m_interval / ( 1.0f + s.m_add[(int)StatE::RATE] / 100.0f );
    s.m_heat = b.m_heat * ( 1.0f - fminf( 0.7f, s.m_add[(int)StatE::HEAT] / 100.0f ) ) *
               ( 1.0f - 0.12f * m_progress.m_weapon );
    s.m_pellets = b.m_pellets;
    s.m_spread = b.m_spread;
    s.m_pierce = b.m_pierce;
    s.m_splash = b.m_splash;
    s.m_range = b.m_range;
    s.m_chain = b.m_chain;
    if ( m_power[(int)PowerE::PIERCE] ) s.m_pierce += 2;
    s.m_splash *= 1.0f + s.m_add[(int)StatE::AREA] / 100.0f;
    s.m_armor += s.m_add[(int)StatE::ARMOR] + 5.0f * m_progress.m_armor;
    s.m_health = ( hero == 1 ? DorraHp : ( hero == 2 ? 95.0f : ( hero == 3 ? 120.0f : 100.0f ) ) ) +
                 20.0f * m_progress.m_armor + ( m_progress.m_heroLevel - 1 ) * ( hero == 1 ? 10.0f : 8.0f ) +
                 s.m_add[(int)StatE::HEALTH];
    m_stats = s;
}

float Game::armorCut() const
{
    float a = m_stats.m_armor;
    if ( m_power[(int)PowerE::LAST_STAND] && m_hp < hpMax() * 0.3f ) a += 25.0f;   // badly hurt: braced
    float cut = fminf( 0.75f, a / ( a + 50.0f + 6.0f * areaLevel() ) );
    return m_corrode > 0.0f ? cut * 0.6f : cut;   // acid eats at the plates
}

void Game::dealHit( Foe &f, float damage, Vector3 dir, Vector3 at )
{
    if ( !f.m_alive ) return;
    // A critical hit: more, and a bright burst; then burning, a stun, and some of it back as health.
    damage *= hitScale( f );
    bool crit = rnd() * 100.0f < BaseCrit + stat( StatE::CRIT );
    if ( crit )
    {
        damage *= 1.5f + stat( StatE::CRIT_DAMAGE ) / 100.0f;
        sparks( at, Vector3Negate( dir ), 16, { 255, 250, 200, 255 }, 5.0f );
    }
    if ( rnd() * 100.0f < stat( StatE::BURN ) && resist( f, DamageE::HEAT ) < 100 )
    {
        f.m_burn = fmaxf( f.m_burn, damage * 0.3f );
        f.m_burnTime = 3.0f;
    }
    if ( rnd() * 100.0f < stat( StatE::SHOCK ) && resist( f, DamageE::SHOCK ) < 100 )
    {
        f.m_stun = fmaxf( f.m_stun, 0.9f );
        sparks( at, { 0.0f, 1.0f, 0.0f }, 12, { 150, 200, 255, 255 }, 3.0f );
    }
    if ( stat( StatE::LEECH ) > 0.0f ) m_hp = fminf( hpMax(), m_hp + damage * stat( StatE::LEECH ) / 100.0f );
    hurtFoe( f, damage, dir, heroDamage() );
    onHit( f, damage, dir, at, crit );
}

int Game::xpNeed( int level )
{
    return (int)( 60.0f * powf( (float)level, 1.6f ) );
}

int Game::xpFor( const Foe &f ) const
{
    int base = 10;
    switch ( f.m_kind )
    {
    case FoeE::LOUSE: base = 6; break;
    case FoeE::WELDER: base = 16; break;
    case FoeE::BATON: base = 12; break;
    case FoeE::GUNNER: base = 14; break;
    case FoeE::SHIELD: base = 20; break;
    case FoeE::HEAVY: base = 34; break;
    case FoeE::DRONE: base = 11; break;
    case FoeE::SAW: base = 5; break;
    case FoeE::BOMBER: base = 8; break;
    case FoeE::TAXATOR: base = f.m_variant ? 900 : 700; break;
    case FoeE::SHELL: base = 9; break;
    case FoeE::CREATURE: base = creatureXp( f ); break;
    default: break;
    }
    // A champion is worth four, the guardian of a level's end ten.
    if ( f.m_elite ) base *= f.m_elite == 2 ? 10 : ( f.m_elite == 3 ? 7 : 4 );
    return (int)( base * ( 1.0f + 0.15f * ( areaLevel() - 1 ) ) );
}

void Game::gainXp( int xp )
{
    if ( m_progress.m_heroLevel >= MaxLevel ) return;
    if ( m_tale ) m_taleXp += xp;
    m_progress.m_xp += (int)( xp * ( 1.0f + stat( StatE::XP ) / 100.0f ) );
    while ( m_progress.m_heroLevel < MaxLevel && m_progress.m_xp >= xpNeed( m_progress.m_heroLevel ) )
    {
        // A level: a skill point, more health, healed whole; a ring of light.
        m_progress.m_xp -= xpNeed( m_progress.m_heroLevel );
        ++m_progress.m_heroLevel;
        ++m_progress.m_points;
        m_levelUp = 3.5f;
        refreshStats();
        m_hp = hpMax();
        for ( int i = 0; i < 40; ++i )
        {
            float a = i * PI * 2.0f / 40.0f;
            m_particles.push_back( { Vector3Add( m_pos, { cosf( a ) * 0.6f, 0.2f, sinf( a ) * 0.6f } ),
                                     { cosf( a ) * 0.4f, 1.6f + rnd(), sinf( a ) * 0.4f },
                                     1.2f,
                                     1.2f,
                                     0.025f,
                                     { 255, 210, 120, 255 },
                                     3 } );
        }
        m_audio.play( SfxE::CAPSULE, m_pos, m_camTarget, 0.9f );
    }
}

void Game::dropItem( Vector3 at, float luck, int slot )
{
    if ( m_tale ) return;   // a memory leaves nothing to carry
    unsigned seed = (unsigned)m_progress.m_seed;
    int      level = std::max( 1, areaLevel() + (int)( rnd() * 3.0f ) - 1 );
    Item     it = makeItem( level, heroIndex(), seed, luck * ( 1.0f + stat( StatE::FIND ) / 100.0f ), slot );
    m_progress.m_seed = (int)( seed & 0x7fffffff );
    Loot l;
    l.m_item = it;
    l.m_pos = at;
    l.m_vel = { ( rnd() - 0.5f ) * 2.5f, 3.0f, ( rnd() - 0.5f ) * 2.5f };
    m_loot.push_back( l );
}

void Game::updateLoot( float dt )
{
    m_bagFull -= dt;
    for ( Loot &l : m_loot )
    {
        if ( !l.m_alive ) continue;
        l.m_t += dt;
        l.m_vel.y -= m_level.m_gravity * dt;
        l.m_pos = Vector3Add( l.m_pos, Vector3Scale( l.m_vel, dt ) );
        m_level.collide( l.m_pos, 0.15f, 0.3f );
        float floor = groundAt( l.m_pos );
        if ( floor > -100.0f && l.m_pos.y < floor + 0.1f )
        {
            l.m_pos.y = floor + 0.1f;
            l.m_vel = { l.m_vel.x * 0.5f, 0.0f, l.m_vel.z * 0.5f };
        }
        if ( l.m_pos.y < -20.0f )
        {
            l.m_alive = false;
            continue;
        }
        // Walking over it picks it up, once it has landed; if nothing is worn in its place, it is put on.
        if ( l.m_t < 0.6f || m_dead || fabsf( l.m_pos.y - m_pos.y ) > 1.5f ||
             Vector2Distance( { l.m_pos.x, l.m_pos.z }, { m_pos.x, m_pos.z } ) > PickupReach )
            continue;
        if ( (int)m_progress.m_bag.size() >= BagSize )
        {
            if ( m_bagFull <= 0.0f )
            {
                m_toast = tr( "loot.full" );
                m_toastColor = { 255, 140, 110, 255 };
                m_toastTimer = 2.0f;
                m_bagFull = 4.0f;
            }
            continue;
        }
        l.m_alive = false;
        m_progress.m_bag.push_back( l.m_item );
        m_toast = itemName( l.m_item );
        m_toastColor = rarityColor( l.m_item.m_rarity );
        m_toastTimer = 2.5f;
        m_audio.play( SfxE::PICKUP, l.m_pos, m_camTarget, 0.8f );
        const ItemBase &b = itemBases()[l.m_item.m_base];
        bool            fits = b.m_hero < 0 || b.m_hero == heroIndex();
        int             slot = gearIndex( l.m_item );
        if ( fits && slot >= 0 && !m_progress.m_worn[slot] ) equip( (int)m_progress.m_bag.size() - 1 );
    }
    m_loot.erase( std::remove_if( m_loot.begin(), m_loot.end(), []( const Loot &l ) { return !l.m_alive; } ),
                  m_loot.end() );
}

void Game::poseLoot( std::vector<Drawable> &out )
{
    if ( m_lootMesh[0].vertexCount == 0 )
    {
        // What lies on the floor: a long case for a weapon, a crate for armour, small ones for the rest; a beam of
        // light over the better ones.
        const Vector3 half[(int)SlotE::COUNT] = { { 0.08f, 0.07f, 0.42f }, { 0.28f, 0.16f, 0.22f },
                                                  { 0.16f, 0.14f, 0.16f }, { 0.14f, 0.06f, 0.12f },
                                                  { 0.13f, 0.1f, 0.18f },  { 0.08f, 0.08f, 0.08f },
                                                  { 0.26f, 0.05f, 0.1f },  { 0.14f, 0.14f, 0.06f },
                                                  { 0.09f, 0.14f, 0.09f }, { 0.06f, 0.03f, 0.06f } };
        for ( int i = 0; i < (int)SlotE::COUNT; ++i )
        {
            MeshBuilder b;
            b.roundedBox( place( { 0.0f, half[i].y, 0.0f } ), half[i], 0.03f, 2 );
            m_lootMesh[i] = b.build().front();
        }
        MeshBuilder beam;
        beam.box( place( { 0.0f, 0.9f, 0.0f } ), { 0.025f, 0.9f, 0.025f } );
        m_beamMesh = beam.build().front();
    }
    for ( const Loot &l : m_loot )
    {
        Drawable d;
        d.m_transform =
            MatrixMultiply( MatrixRotateY( l.m_t * 0.8f ), MatrixTranslate( l.m_pos.x, l.m_pos.y, l.m_pos.z ) );
        d.m_mesh = m_lootMesh[(int)itemSlot( l.m_item )];
        d.m_surface = S_METAL;
        Color c = rarityColor( l.m_item.m_rarity );
        float glow = l.m_item.m_rarity == RarityE::COMMON ? 0.05f : 0.2f;
        d.m_emissive = { c.r / 255.0f * glow, c.g / 255.0f * glow, c.b / 255.0f * glow, 0.0f };
        out.push_back( d );
        if ( l.m_item.m_rarity == RarityE::COMMON ) continue;
        d.m_mesh = m_beamMesh;
        d.m_surface = S_GLOW;
        float pulse = 0.8f + 0.3f * sinf( l.m_t * 4.0f );
        d.m_emissive = { c.r / 255.0f * pulse, c.g / 255.0f * pulse, c.b / 255.0f * pulse, 0.0f };
        d.m_transform = MatrixTranslate( l.m_pos.x, l.m_pos.y, l.m_pos.z );
        d.m_sunShadow = d.m_spotShadow = false;
        out.push_back( d );
    }
}

void Game::equip( int i )
{
    if ( i < 0 || i >= (int)m_progress.m_bag.size() ) return;
    Item            it = m_progress.m_bag[i];
    const ItemBase &b = itemBases()[it.m_base];
    if ( b.m_hero >= 0 && b.m_hero != heroIndex() )
    {
        m_toast = tr( "loot.wrong" );
        m_toastColor = { 255, 140, 110, 255 };
        m_toastTimer = 2.0f;
        return;
    }
    int slot = gearIndex( it );
    if ( slot < 0 )
    {
        insertModule( i );   // a module goes into a socket
        return;
    }
    m_progress.m_bag.erase( m_progress.m_bag.begin() + i );
    if ( m_progress.m_worn[slot] ) m_progress.m_bag.push_back( m_progress.m_gear[slot] );
    m_progress.m_gear[slot] = it;
    m_progress.m_worn[slot] = true;
    float part = m_hp / fmaxf( 1.0f, hpMax() );
    refreshStats();
    m_hp = fminf( hpMax(), part * hpMax() );
    m_shield = fminf( m_shield, m_stats.m_shield );
    dressHero();
}

void Game::unequip( int slot )
{
    // The weapon stays in the hand; anything else goes back into the bag if there is room.
    if ( slot <= 0 || slot >= EquipSlots || !m_progress.m_worn[slot] || (int)m_progress.m_bag.size() >= BagSize )
        return;
    m_progress.m_bag.push_back( m_progress.m_gear[slot] );
    m_progress.m_worn[slot] = false;
    refreshStats();
    m_hp = fminf( m_hp, hpMax() );
    m_shield = fminf( m_shield, m_stats.m_shield );
    dressHero();
}

void Game::salvage( int i )
{
    if ( i < 0 || i >= (int)m_progress.m_bag.size() ) return;
    int scrap = std::max( 1, itemValue( m_progress.m_bag[i] ) / 3 );
    m_scrap += scrap;
    m_progress.m_bag.erase( m_progress.m_bag.begin() + i );
    m_toast = TextFormat( tr( "loot.salvaged" ), scrap );
    m_toastColor = { 240, 235, 225, 255 };
    m_toastTimer = 2.0f;
    m_audio.play( SfxE::PICKUP, m_pos, m_camTarget, 0.6f );
}

void Game::useSkills( float dt, const GameInput &in )
{
    for ( float &c : m_cool ) c = fmaxf( 0.0f, c - dt );
    m_overcharge = fmaxf( 0.0f, m_overcharge - dt );
    m_levelUp = fmaxf( 0.0f, m_levelUp - dt );
    if ( m_dead || m_finished || m_scene > 0 || m_level.m_peaceful ) return;
    int   hero = heroIndex();
    float cooldown = 1.0f - fminf( 0.6f, stat( StatE::COOLDOWN ) / 100.0f );
    float power = skillPower();
    // B / F is Corvin's first skill; Dorra's is her blast bolt (thrown in updateMelee), Ione's the takeover (updateIone).
    bool pressed[4] = { false, hero == 0 && in.m_special, in.m_skill2, in.m_skill3 };
    for ( int slot = 1; slot <= 3; ++slot )
    {
        if ( !pressed[slot] || m_cool[slot] > 0.0f ) continue;
        int k = activeSkill( hero, slot ), r = rank( k );
        if ( k < 0 || r <= 0 )
        {
            m_toast = tr( "skill.unlearnt" );
            m_toastColor = { 200, 200, 200, 255 };
            m_toastTimer = 1.5f;
            continue;
        }
        const SkillDef &def = skillDefs()[k];
        std::string     key = def.m_key;
        m_cool[slot] = def.m_cooldown * cooldown;
        Vector3 fwd = { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
        if ( key == "skill.overcharge" )
        {
            // The rifle runs twice as fast and stays cold for a few seconds.
            m_overcharge = 3.0f + 0.5f * r;
            m_cool[slot] = ( def.m_cooldown - 1.5f * ( r - 1 ) ) * cooldown;
            m_heat = 0.0f;
            m_overheat = false;
            m_audio.play( SfxE::ZAP, m_pos, m_camTarget, 0.9f );
            sparks( Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ), { 0.0f, 1.0f, 0.0f }, 30, { 140, 200, 255, 255 }, 3.0f );
        }
        else if ( key == "skill.grenade" )
            throwBolt( ( 50.0f + 25.0f * r ) * power, 3.0f + 0.2f * r );
        else if ( key == "skill.roll" || key == "skill.charge" )
        {
            // A roll goes where the stick points (or ahead), a charge straight ahead and through them.
            bool charge = key == "skill.charge";
            m_dashKind = charge ? 1 : 0;
            m_dash = charge ? 0.38f : 0.28f;
            Vector3 v = { m_vel.x, 0.0f, m_vel.z };
            m_dashDir = !charge && Vector3Length( v ) > 1.0f ? Vector3Normalize( v ) : fwd;
            m_dashDamage = ( 30.0f + 15.0f * r ) * power;
            m_dashHit.clear();
            m_invuln = fmaxf( m_invuln, m_dash + 0.15f );
            if ( !charge ) m_cool[slot] = ( def.m_cooldown - 0.5f * ( r - 1 ) ) * cooldown;
            m_audio.play( charge ? SfxE::SWING : SfxE::JUMP, m_pos, m_camTarget, 0.8f );
        }
        else if ( key == "skill.quake" )
            quake( r );
        else if ( key == "skill.repair" || key == "skill.storm" )
            ioneSkill( key, r );
    }
}

void Game::quake( int r )
{
    // Dorra drives the hammer into the deck: a ring that throws and stuns everything near.
    float   damage = ( 40.0f + 20.0f * r ) * skillPower(), radius = ( 4.0f + 0.25f * r ) * areaScale();
    Vector3 at = { m_pos.x, m_pos.y + 0.05f, m_pos.z };
    for ( int i = 0; i < 48; ++i )
    {
        float a = i * PI * 2.0f / 48.0f;
        m_particles.push_back( { at,
                                 { cosf( a ) * radius * 1.6f, 0.3f + rnd() * 0.5f, sinf( a ) * radius * 1.6f },
                                 0.6f,
                                 0.6f,
                                 0.3f,
                                 { 200, 175, 150, 24 },
                                 1 } );
    }
    sparks( at, { 0.0f, 1.0f, 0.0f }, 40, { 255, 190, 100, 255 }, 5.0f );
    m_flashes.push_back(
        { Vector3Add( at, { 0.0f, 0.5f, 0.0f } ), { 25.0f, 14.0f, 5.0f }, radius + 2.0f, 0.2f, 0.2f } );
    m_shake = fmaxf( m_shake, 0.6f );
    m_audio.play( SfxE::SMASH, at, m_camTarget, 1.0f );
    m_audio.play( SfxE::BOOM, at, m_camTarget, 0.7f );
    for ( Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden ) continue;
        Vector3 d = { f.m_pos.x - at.x, 0.0f, f.m_pos.z - at.z };
        float   dist = Vector3Length( d );
        if ( dist > radius + foeRadius( f ) || fabsf( f.m_pos.y - at.y ) > 2.0f ) continue;
        Vector3 dir = dist > 1e-3f ? Vector3Scale( d, 1.0f / dist ) : Vector3{ 0.0f, 0.0f, 1.0f };
        dealHit( f, damage * ( 1.0f - 0.4f * fminf( 1.0f, dist / radius ) ), dir, f.m_pos );
        f.m_stun = fmaxf( f.m_stun, 1.2f + 0.1f * r );
        f.m_vel = Vector3Add( f.m_vel, { dir.x * 3.0f, 3.0f, dir.z * 3.0f } );
        f.m_ground = false;
    }
}

void Game::drawLootHud( int w, int h, float s )
{
    auto text = [&]( const char *t, float x, float y, float size, Color c, bool centre = false )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        Vector2 p = { centre ? x - m.x * 0.5f : x, y };
        DrawTextEx( m_font, t, { p.x + 2.0f * s, p.y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
        DrawTextEx( m_font, t, p, size * s, 1.0f, c );
    };
    // The names of what lies near, in their rarity's colour.
    for ( const Loot &l : m_loot )
    {
        if ( Vector3Distance( l.m_pos, m_pos ) > 12.0f || m_talk || m_menu ) continue;
        Vector2     p = GetWorldToScreenEx( Vector3Add( l.m_pos, { 0.0f, 0.6f, 0.0f } ), m_cam, w, h );
        std::string name = itemName( l.m_item );
        Vector2     m = MeasureTextEx( m_font, name.c_str(), 18.0f * s, 1.0f );
        DrawRectangle( (int)( p.x - m.x * 0.5f - 6 * s ), (int)( p.y - 3 * s ), (int)( m.x + 12 * s ),
                       (int)( m.y + 6 * s ), { 0, 0, 0, 150 } );
        if ( const Texture2D *pic = pictureOf( l.m_item ) )
        {
            float size = m.y + 6 * s;
            DrawTexturePro( *pic, { 0, 0, (float)pic->width, (float)pic->height },
                            { p.x - m.x * 0.5f - 6 * s - size, p.y - 3 * s, size, size }, { 0, 0 }, 0.0f, WHITE );
        }
        text( name.c_str(), p.x, p.y, 18.0f, rarityColor( l.m_item.m_rarity ), true );
    }
    if ( m_level.m_peaceful && m_progress.m_heroLevel <= 1 && m_progress.m_xp == 0 ) return;
    // Experience: a thin bar along the bottom, the level beside it.
    float bw = w * 0.42f, bx = ( w - bw ) * 0.5f, by = h - 14.0f * s;
    float part = Clamp( m_progress.m_xp / (float)xpNeed( m_progress.m_heroLevel ), 0.0f, 1.0f );
    DrawRectangle( (int)bx, (int)by, (int)bw, (int)( 6 * s ), { 0, 0, 0, 160 } );
    DrawRectangle( (int)bx, (int)by, (int)( bw * part ), (int)( 6 * s ), { 255, 200, 110, 220 } );
    text( TextFormat( tr( "hud.level" ), m_progress.m_heroLevel ),
          bx - 12 * s -
              MeasureTextEx( m_font, TextFormat( tr( "hud.level" ), m_progress.m_heroLevel ), 18 * s, 1.0f ).x,
          by - 8 * s, 18.0f, { 255, 210, 140, 230 } );
    if ( m_progress.m_points > 0 )
        text( TextFormat( tr( "hud.points" ), m_progress.m_points ), bx + bw + 12 * s, by - 8 * s, 18.0f,
              { 140, 220, 255, 230 } );
    // The active skills: a box per button, dark while it cools down.
    if ( !m_level.m_peaceful )
    {
        int         hero = heroIndex();
        const char *keys[4] = { "", "hud.key1", "hud.key2", "hud.key3" };
        for ( int slot = 1; slot <= 3; ++slot )
        {
            int k = activeSkill( hero, slot );
            if ( k < 0 ) continue;
            bool  known = rank( k ) > 0 || ( hero >= 1 && slot == 1 );   // the bolts, the takeover: always
            float size = 64.0f * s, x = w - ( 4 - slot ) * ( size + 10 * s ) - 20 * s, y = h - size - 40 * s;
            DrawRectangle( (int)x, (int)y, (int)size, (int)size, { 10, 12, 14, 200 } );
            float cool = slot == 1 && hero >= 1 ? m_boltTimer / ( hero == 1 ? BoltCooldown : skillDefs()[k].m_cooldown )
                                                : m_cool[slot] / fmaxf( 0.1f, skillDefs()[k].m_cooldown );
            if ( cool > 0.0f )
                DrawRectangle( (int)x, (int)( y + size * ( 1.0f - Clamp( cool, 0.0f, 1.0f ) ) ), (int)size,
                               (int)( size * Clamp( cool, 0.0f, 1.0f ) ), { 0, 0, 0, 170 } );
            DrawRectangleLinesEx( { x, y, size, size }, 2.0f * s,
                                  known ? Color{ 255, 190, 110, 200 } : Color{ 90, 90, 90, 160 } );
            text( tr( keys[slot] ), x + 6 * s, y + 4 * s, 16.0f, { 200, 200, 200, 230 } );
            text( tr( std::string( skillDefs()[k].m_key ) + ".short" ), x + size * 0.5f, y + size - 24 * s, 15.0f,
                  known ? Color{ 240, 230, 210, 240 } : Color{ 120, 120, 120, 200 }, true );
        }
    }
    // A level reached.
    if ( m_levelUp > 0.0f )
    {
        unsigned char a = (unsigned char)( 255 * Clamp( m_levelUp, 0.0f, 1.0f ) );
        text( TextFormat( tr( "hud.levelup" ), m_progress.m_heroLevel ), w * 0.5f, h * 0.18f, 40.0f,
              { 255, 210, 130, a }, true );
        text( tr( "hud.levelup.sub" ), w * 0.5f, h * 0.18f + 50 * s, 22.0f, { 220, 220, 210, a }, true );
    }
}

}   // namespace spaceopera
