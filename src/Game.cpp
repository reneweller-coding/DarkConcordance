/**
 * @file Game.cpp
 * @brief Level 1 played: Corvin, the lice, the press, effects, story, HUD, camera, autopilot.
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
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <sstream>

namespace spaceopera
{

using namespace play;

float Game::rnd()
{
    m_seed = m_seed * 1664525u + 1013904223u;
    return ( m_seed >> 8 ) * ( 1.0f / 16777216.0f );
}

void Game::loadSurfaces( Gfx &gfx, const std::string &assets )
{
    std::string dir = assets + "/textures/";
    m_surfaces.resize( S_COUNT );
    auto tile = [&]( int s, const char *name, float uv, Vector4 params, float variation, Color tint = WHITE )
    {
        Texture2D a = loadTexture( dir + name + "_albedo.png" );
        Texture2D o = loadTexture( dir + name + "_orm.png" );
        Texture2D n = loadTexture( dir + name + "_normal.png" );
        Surface   surf = gfx.surface( a, o, n, Texture2D{} );
        surf.m_uvScale = uv;
        surf.m_params = params;
        surf.m_variation = variation;
        surf.m_tint = tint;
        m_surfaces[s] = surf;
    };
    tile( S_ROCK, "rock", 0.22f, { 1.0f, 1.0f, 1.3f, 0.0f }, 0.6f );
    tile( S_FLOOR, "floor", 0.25f, { 1.0f, 1.0f, 1.0f, 0.0f }, 0.6f, { 200, 190, 185, 255 } );
    tile( S_GRATE, "grate", 0.5f, { 1.0f, 1.0f, 1.2f, 0.0f }, 0.4f );
    tile( S_METAL, "wall", 0.35f, { 1.0f, 1.0f, 1.1f, 0.0f }, 0.5f );
    tile( S_HAZARD, "hazard", 0.7f, { 1.0f, 1.0f, 1.0f, 0.0f }, 0.3f );
    tile( S_RUST, "rust", 0.6f, { 1.0f, 1.0f, 1.0f, 0.0f }, 0.4f );
    tile( S_FOIL, "metal", 1.2f, { 0.35f, 1.0f, 2.0f, 0.0f }, 0.2f, { 235, 215, 150, 255 } );
    tile( S_GLOW, "metal", 0.5f, { 0.5f, 0.0f, 0.0f, 0.0f }, 0.0f, { 25, 25, 25, 255 } );
    tile( S_ARMOR, "armor", 1.6f, { 1.0f, 1.0f, 1.5f, 0.0f }, 0.15f, { 132, 104, 78, 255 } );
    tile( S_DARK, "armor", 1.6f, { 0.9f, 1.0f, 1.5f, 0.0f }, 0.15f, { 46, 44, 48, 255 } );
    tile( S_CRATE, "crate", 0.6f, { 1.0f, 1.0f, 1.0f, 0.0f }, 0.4f );
    tile( S_BOX, "container", 0.45f, { 1.0f, 1.0f, 1.4f, 0.0f }, 0.3f );
    tile( S_HULL, "wall", 0.3f, { 0.9f, 1.0f, 1.1f, 0.0f }, 0.4f, { 160, 168, 178, 255 } );
    tile( S_DORRA, "armor", 1.6f, { 1.1f, 1.0f, 1.5f, 0.0f }, 0.15f, { 196, 150, 40, 255 } );
    tile( S_BOARDER, "armor", 1.6f, { 0.8f, 1.0f, 1.5f, 0.0f }, 0.15f, { 96, 30, 26, 255 } );
    tile( S_CLOTH, "fabric", 2.0f, { 1.0f, 0.0f, 1.0f, 0.0f }, 0.2f, { 84, 90, 100, 255 } );
    tile( S_CLOTH2, "fabric", 2.0f, { 1.0f, 0.0f, 1.0f, 0.0f }, 0.2f, { 96, 78, 60, 255 } );
    tile( S_CLOTH3, "fabric", 2.0f, { 1.0f, 0.0f, 1.0f, 0.0f }, 0.2f, { 64, 80, 68, 255 } );
    tile( S_ILKA, "fabric", 2.0f, { 1.0f, 0.0f, 1.0f, 0.0f }, 0.2f, { 175, 125, 40, 255 } );
    tile( S_YSOLDE, "fabric", 2.0f, { 0.8f, 0.0f, 1.0f, 0.0f }, 0.1f, { 44, 30, 62, 255 } );
    tile( S_KIRI, "fabric", 2.0f, { 1.0f, 0.0f, 1.0f, 0.0f }, 0.2f, { 160, 75, 25, 255 } );
    tile( S_CAVERN, "rock", 0.004f, { 1.0f, 0.0f, 1.0f, 0.0f }, 0.8f, { 12, 11, 10, 255 } );
    tile( S_CLOTH4, "fabric", 2.0f, { 1.0f, 0.0f, 1.0f, 0.0f }, 0.2f, { 118, 54, 48, 255 } );
    tile( S_CLOTH5, "fabric", 2.0f, { 1.0f, 0.0f, 1.0f, 0.0f }, 0.2f, { 58, 72, 104, 255 } );
    tile( S_SKIN, "fabric", 0.15f, { 0.75f, 0.0f, 0.2f, 0.0f }, 0.0f, { 196, 146, 118, 255 } );
    tile( S_HAIR, "fabric", 4.0f, { 0.9f, 0.0f, 1.0f, 0.0f }, 0.1f, { 48, 40, 34, 255 } );
    tile( S_HAIR2, "fabric", 4.0f, { 0.9f, 0.0f, 1.0f, 0.0f }, 0.1f, { 132, 56, 30, 255 } );
    tile( S_HAIR3, "fabric", 4.0f, { 0.9f, 0.0f, 1.0f, 0.0f }, 0.1f, { 150, 146, 140, 255 } );
    tile( S_GUARD, "armor", 1.6f, { 0.9f, 1.0f, 1.5f, 0.0f }, 0.15f, { 58, 76, 104, 255 } );
    tile( S_TAXATOR, "armor", 1.6f, { 1.2f, 1.0f, 1.5f, 0.0f }, 0.15f, { 182, 138, 58, 255 } );
}

bool Game::load( Gfx &gfx, const std::string &assets, const std::string &level, bool sound )
{
    m_assets = assets;
    m_levelFile = level;
    // A generated level: the same seed builds the same layout (by default from the progress, so it differs per game).
    unsigned seed = m_genSeed;
    if ( seed == 0 )
    {
        seed = 2166136261u ^ (unsigned)m_progress.m_seed * 2654435761u;
        for ( char ch : level ) seed = ( seed ^ (unsigned char)ch ) * 16777619u;
    }
    if ( !m_level.load( assets + "/levels/" + level, seed ) ) return false;
    // "area 0": as deep as the hero is strong (the depths).
    if ( m_level.m_area <= 0 ) m_level.m_area = std::max( 2, ( m_progress.m_heroLevel + 4 ) / 2 );
    m_areaHit = 1.0f + 0.12f * ( m_level.m_area - 1 );
    loadSurfaces( gfx, assets );
    // The lift's cell is part of its well.
    for ( const Marker &m : m_level.m_markers )
        if ( m.m_kind == 'X' ) m_level.setVoid( m.m_row, m.m_col );
    m_level.build(
        { S_ROCK, S_FLOOR, S_GRATE, S_METAL, S_HAZARD, S_RUST, S_FOIL, S_GLOW, S_CRATE, S_BOX, S_HULL, S_CLOTH2 },
        m_static );

    // Signs: each picture its own surface, lit a little from within so it reads in the dark.
    for ( const Sign &sign : m_level.m_signs )
    {
        Texture2D tex = signTexture( assets, sign.m_texture );
        Surface   surf = gfx.surface( tex, m_surfaces[S_METAL].m_material.maps[MATERIAL_MAP_METALNESS].texture,
                                      m_surfaces[S_METAL].m_material.maps[MATERIAL_MAP_NORMAL].texture, tex );
        surf.m_uvScale = 1.0f;
        surf.m_params = { 0.8f, 0.3f, 0.0f, 1.0f };
        surf.m_emissive = { 0.22f, 0.21f, 0.2f, 2.0f };
        surf.m_variation = 0.0f;
        m_surfaces.push_back( surf );
        m_signSurfaces.push_back( (int)m_surfaces.size() - 1 );
        MeshBuilder b;
        b.quad( sign.m_corner[0], sign.m_corner[1], sign.m_corner[2], sign.m_corner[3] );
        for ( Mesh mesh : b.build() )
        {
            Drawable d;
            d.m_mesh = mesh;
            d.m_surface = (int)m_surfaces.size() - 1;
            d.m_transform = MatrixIdentity();
            m_static.push_back( d );
        }
    }

    // The hero: Corvin with the rifle and an amber visor, or Dorra, short and broad, with hammer and shield.
    FigureSurfaces fs = { S_ARMOR, S_DARK, S_METAL, S_GLOW };
    fs.m_visor = { 3.2f, 1.6f, 0.3f, 0.0f };
    // Where there is air and nobody shoots, the helmet comes off.
    fs.m_helmet = !m_level.m_peaceful;
    fs.m_skin = S_SKIN;
    fs.m_hair = m_heroKind == HeroE::DORRA ? S_HAIR2 : S_HAIR;
    if ( m_heroKind == HeroE::DORRA )
    {
        fs.m_armor = S_DORRA;
        fs.m_visor = { 0.6f, 3.0f, 1.4f, 0.0f };
        fs.m_weapon = WeaponE::HAMMER;
        fs.m_scale = { 1.14f, 0.9f, 1.1f };
    }
    m_hero.build( fs );
    // A new hero starts with the weapon of their kind and the suit; skills all at rank 0.
    int hero = m_heroKind == HeroE::DORRA ? 1 : 0;
    if ( !m_progress.m_worn[0] ) m_progress.m_gear[0] = startItem( hero, SlotE::WEAPON ), m_progress.m_worn[0] = true;
    if ( !m_progress.m_worn[1] ) m_progress.m_gear[1] = startItem( hero, SlotE::ARMOR ), m_progress.m_worn[1] = true;
    m_progress.m_ranks.resize( skillDefs().size(), 0 );
    refreshStats();
    m_hp = hpMax();
    buildCrew( assets );
    m_louse.build( S_RUST, S_METAL, S_GLOW, false );
    m_welder.build( S_RUST, S_METAL, S_GLOW, true );
    buildFoes();

    MeshBuilder chunk;
    chunk.box( MatrixIdentity(), { 0.07f, 0.04f, 0.09f } );
    m_chunk = chunk.build().front();
    MeshBuilder scrap;
    scrap.box( place( { 0.0f, 0.06f, 0.0f }, { 0.0f, 0.4f, 0.0f } ), { 0.1f, 0.05f, 0.07f } );
    scrap.box( place( { 0.08f, 0.1f, 0.05f }, { 0.3f, 1.1f, 0.2f } ), { 0.06f, 0.03f, 0.09f } );
    scrap.box( place( { -0.07f, 0.12f, -0.04f }, { 0.0f, 2.0f, 0.5f } ), { 0.05f, 0.05f, 0.05f } );
    m_scrapMesh = scrap.build().front();
    MeshBuilder stim;
    stim.cylinder( place( { 0.0f, 0.05f, 0.0f } ), 0.07f, 0.22f, 12 );
    m_stimMesh = stim.build().front();
    MeshBuilder cap;
    cap.cylinder( place( { 0.0f, 0.0f, 0.0f } ), 0.08f, 0.05f, 12 );
    cap.cylinder( place( { 0.0f, 0.27f, 0.0f } ), 0.08f, 0.05f, 12 );
    m_stimCap = cap.build().front();

    // The rescue capsule: base, pod, a window that glows when it wakes.
    {
        MeshBuilder base;
        base.roundedBox( place( { 0.0f, 0.15f, 0.0f } ), { 0.75f, 0.15f, 0.75f }, 0.05f, 2 );
        base.lathe( place( { 0.0f, 0.3f, 0.0f } ),
                    { { 0.0f, 0.0f },
                      { 0.55f, 0.0f },
                      { 0.55f, 0.0f },
                      { 0.62f, 0.2f },
                      { 0.62f, 2.0f },
                      { 0.45f, 2.35f },
                      { 0.0f, 2.45f } },
                    28 );
        for ( Mesh m : base.build() ) m_capsuleMeshes.push_back( m ), m_capsuleSurfaces.push_back( S_METAL );
        MeshBuilder stripe;
        stripe.lathe( place( { 0.0f, 0.35f, 0.0f } ), { { 0.63f, 0.0f }, { 0.63f, 0.25f } }, 28 );
        for ( Mesh m : stripe.build() ) m_capsuleMeshes.push_back( m ), m_capsuleSurfaces.push_back( S_HAZARD );
        MeshBuilder window;
        window.lathe( place( { 0.0f, 0.3f, 0.0f } ), { { 0.635f, 0.6f }, { 0.635f, 1.9f } }, 12, PI * 0.5f - 0.7f,
                      PI * 0.5f + 0.7f );
        for ( Mesh m : window.build() )
            m_capsuleMeshes.push_back( m ), m_capsuleSurfaces.push_back( -1 );   // -1: the glow
    }

    Image dot = GenImageGradientRadial( 64, 64, 0.0f, WHITE, BLANK );
    m_dot = LoadTextureFromImage( dot );
    UnloadImage( dot );
    Image streak = GenImageColor( 8, 64, BLANK );
    for ( int y = 0; y < 64; ++y )
    {
        float d = ( y - 31.5f ) / 12.0f;
        for ( int x = 0; x < 8; ++x )
            ImageDrawPixel( &streak, x, y, { 255, 255, 255, (unsigned char)( 255 * expf( -d * d ) ) } );
    }
    m_streak = LoadTextureFromImage( streak );
    SetTextureFilter( m_streak, TEXTURE_FILTER_BILINEAR );
    UnloadImage( streak );

    // Markers: Corvin, enemies, capsules, pickups, the swarms, vents, the press, the door.
    const int DR[4] = { 1, 0, 0, -1 }, DC[4] = { 0, 1, -1, 0 };
    for ( const Marker &m : m_level.m_markers )
    {
        Vector3 p = m.m_pos;
        size_t  foes = m_foes.size();
        switch ( m.m_kind )
        {
        case '$': spawnFoe( FoeE::SHIELD, p, false ); break;
        case '%': spawnFoe( FoeE::HEAVY, p, false ); break;
        case '^': spawnFoe( FoeE::DRONE, Vector3Add( p, { 0.0f, 1.75f, 0.0f } ), false ); break;
        case '*': spawnFoe( FoeE::SAW, p, false ); break;
        case '?': spawnFoe( FoeE::BOMBER, p, false ); break;
        case '@': spawnFoe( FoeE::TAXATOR, p, false ); break;
        case 'P':
            m_pos = p;
            m_respawn = p;
            break;
        case 'L': spawnFoe( FoeE::LOUSE, p, false ); break;
        case 'W': spawnFoe( FoeE::WELDER, p, false ); break;
        case 'E':
        case 'T':
        case 'Y':
        case 'U':
        {
            // Boarders; the bridge's wake together, Y and U come aboard once the bridge is clear.
            spawnFoe( m.m_kind == 'E' || m.m_kind == 'Y' ? FoeE::BATON : FoeE::GUNNER, p, false );
            Foe &f = m_foes.back();
            if ( m.m_kind == 'Y' || m.m_kind == 'U' )
                f.m_group = 2, f.m_hidden = true;
            else if ( m_level.theme( m.m_row, m.m_col ) == ThemeE::BRIDGE )
                f.m_group = 1;
            break;
        }
        case 'H': m_pickups.push_back( { 1, 1, p, { 0, 0, 0 } } ); break;
        case 'S': m_pickups.push_back( { 0, 10, p, { 0, 0, 0 } } ); break;
        case 'C':
        {
            // The capsule faces where there is most room.
            int best = 0, bestRoom = -1;
            for ( int d = 0; d < 4; ++d )
            {
                int room = 0;
                for ( int k = 1; k <= 3 && m_level.walkable( m.m_row + DR[d] * k, m.m_col + DC[d] * k ); ++k ) ++room;
                if ( room > bestRoom ) bestRoom = room, best = d;
            }
            Vector3 dir = { (float)DC[best], 0.0f, (float)DR[best] };
            m_capsules.push_back( { p, Vector3Add( p, Vector3Scale( dir, 1.4f ) ), atan2f( dir.x, dir.z ) } );
            m_blocked.insert( m.m_row * m_level.cols() + m.m_col );
            break;
        }
        case 'D':
            for ( int i = 0; i < 900; ++i )
            {
                Vector3 o = { rnd() - 0.5f, rnd() - 0.5f, rnd() - 0.5f };
                o = Vector3Scale( Vector3Normalize( o ), 3.2f * powf( rnd(), 0.5f ) );
                o.y *= 0.45f;
                m_drops.push_back(
                    { { p.x, p.y + 2.0f, p.z }, o, 0.03f + rnd() * 0.08f, rnd() * 6.28f, 0.012f + rnd() * 0.02f } );
            }
            break;
        case 'V': m_vents.push_back( { p, 1.0f + rnd() * 4.0f, 0.0f } ); break;
        case 'K': m_blocked.insert( m.m_row * m_level.cols() + m.m_col ); break;
        case 'B':
        {
            Vector3 o = { p.x, 0.0f, m.m_row * CellSize };
            m_pressModel.build( o, S_METAL, S_HAZARD, S_RUST, S_GLOW );
            MeshBuilder anvil;
            anvil.box( place( { 0.0f, 0.25f, 2.2f } ), { 2.25f, 0.008f, 1.95f } );
            m_anvilGlow = anvil.build().front();
            // The cells the housing, the tanks and the gantry's legs stand on are no way through.
            for ( int r = 0; r < m_level.rows(); ++r )
                for ( int c = 0; c < m_level.cols(); ++c )
                {
                    Vector3 q = m_level.centre( r, c );
                    bool    taken = q.x > o.x - 8.8f && q.x < o.x + 8.8f && q.z > o.z - 5.0f && q.z < o.z + 0.9f;
                    for ( float side : { -1.0f, 1.0f } )
                    {
                        taken = taken || Vector2Distance( { q.x, q.z }, { o.x + side * 9.2f, o.z + 0.5f } ) < 1.9f;
                        taken = taken || Vector2Distance( { q.x, q.z }, { o.x + side * 2.6f, o.z + 4.3f } ) < 1.0f;
                    }
                    if ( taken ) m_blocked.insert( r * m_level.cols() + c );
                }
            break;
        }
        case 'X':
        {
            // The freight lift: a platform over its well (the X cell and the shaft cells beside and before it).
            int c0 = m.m_col, c1 = m.m_col;
            while ( m_level.cell( m.m_row, c0 - 1 ).m_kind == CellE::VOID ) --c0;
            while ( m_level.cell( m.m_row, c1 + 1 ).m_kind == CellE::VOID ) ++c1;
            int r0 = m.m_row;
            while ( m_level.cell( r0 - 1, m.m_col ).m_kind == CellE::VOID ) --r0;
            m_liftMin = { c0 * CellSize, 0.0f, r0 * CellSize };
            m_liftMax = { ( c1 + 1 ) * CellSize, 0.0f, ( m.m_row + 1 ) * CellSize };
            MeshBuilder deck;
            Vector3     mid = Vector3Scale( Vector3Add( m_liftMin, m_liftMax ), 0.5f );
            Vector3     half = Vector3Scale( Vector3Subtract( m_liftMax, m_liftMin ), 0.5f );
            deck.box( place( { 0.0f, -0.12f, 0.0f } ), { half.x - 0.05f, 0.12f, half.z - 0.05f } );
            for ( float sx : { -1.0f, 1.0f } )
                for ( float sz : { -1.0f, 1.0f } )
                    deck.box( place( { sx * ( half.x - 0.2f ), 1.6f, sz * ( half.z - 0.2f ) } ),
                              { 0.08f, 1.6f, 0.08f } );
            deck.box( place( { 0.0f, 3.2f, -( half.z - 0.2f ) } ), { half.x - 0.1f, 0.08f, 0.08f } );
            deck.box( place( { 0.0f, 3.2f, half.z - 0.2f } ), { half.x - 0.1f, 0.08f, 0.08f } );
            m_liftDeck = deck.build();
            MeshBuilder rim;
            for ( float sz : { -1.0f, 1.0f } )
                rim.box( place( { 0.0f, 0.01f, sz * ( half.z - 0.25f ) } ), { half.x - 0.1f, 0.012f, 0.2f } );
            for ( float sx : { -1.0f, 1.0f } )
                rim.box( place( { sx * ( half.x - 0.25f ), 0.01f, 0.0f } ), { 0.2f, 0.012f, half.z - 0.1f } );
            m_liftRim = rim.build();
            MeshBuilder beacons;
            for ( float sx : { -1.0f, 1.0f } )
                for ( float sz : { -1.0f, 1.0f } )
                    beacons.roundedBox( place( { sx * ( half.x - 0.2f ), 3.3f, sz * ( half.z - 0.2f ) } ),
                                        { 0.12f, 0.1f, 0.12f }, 0.05f, 2 );
            m_liftBeacons = beacons.build();
            m_lift = mid;
            break;
        }
        case 'G':
        {
            int c0 = m.m_col, c1 = m.m_col;
            while ( m_level.walkable( m.m_row, c0 - 1 ) ) --c0;
            while ( m_level.walkable( m.m_row, c1 + 1 ) ) ++c1;
            m_door = { ( c0 + c1 + 1 ) * 0.5f * CellSize, 0.0f, p.z };
            m_doorHalf = ( c1 - c0 + 1 ) * 0.5f * CellSize;
            MeshBuilder frame;
            for ( float side : { -1.0f, 1.0f } )
                frame.box( place( { m_door.x + side * ( m_doorHalf + 0.2f ), 2.0f, m_door.z } ),
                           { 0.25f, 2.0f, 0.35f } );
            frame.box( place( { m_door.x, 4.2f, m_door.z } ), { m_doorHalf + 0.45f, 0.25f, 0.35f } );
            m_doorFrame = frame.build();
            MeshBuilder panel;
            panel.box( place( { 0.0f, 1.9f, 0.0f } ), { m_doorHalf * 0.5f, 1.9f, 0.12f } );
            m_doorPanel = panel.build();
            break;
        }
        default: break;
        }
        // A champion (or the guardian of the level's end) where the level says so.
        auto elite = m_level.m_elites.find( m.m_row * 1000 + m.m_col );
        if ( m_foes.size() > foes && elite != m_level.m_elites.end() ) makeElite( m_foes.back(), elite->second );
    }
    loadSkins( gfx );
    loadShip( gfx, assets );
    loadHub( gfx, assets );
    loadUsables();
    loadPanel();
    // Who stands, walks or sits on a stool is a skinned figure; those huddled on the floor and Ilka stay as they are.
    for ( size_t i = 0; i < m_people.size(); ++i )
        if ( m_people[i].m_sit < 0.5f ) dressPerson( m_people[i], (unsigned)( i * 7919u + 13u ) );
    if ( !m_level.m_intro.empty() ) m_intro = IntroTime;
    m_hero.animate( 0.0f, m_pos, { 0, 0 }, 0.0f, false, 0.0f, 0.0f );
    m_yaw = m_aimYaw = 0.0f;
    // What the hero brings along; what has happened (it opens doors and changes what people say, quietly).
    m_stims = m_progress.m_stims;
    m_scrap = m_progress.m_scrap;
    for ( const std::string &f : m_progress.m_flags ) m_fired.insert( f );

    loadFont();
    renderBoard();
    m_audio.init( sound, m_level.m_ambience );
    putHero( m_pos.x, m_pos.z, 0.0f );
    return true;
}

void Game::loadFont()
{
    if ( m_ownFont ) UnloadFont( m_font );
    m_ownFont = false;
    // Bahnschrift with every character the loaded texts use (umlauts, dashes, quotes); raylib's font if it is missing.
    std::vector<int> cps = textCodepoints();
    const char      *fontPath = "C:/Windows/Fonts/bahnschrift.ttf";
    if ( FileExists( fontPath ) )
    {
        m_font = LoadFontEx( fontPath, 48, cps.data(), (int)cps.size() );
        SetTextureFilter( m_font.texture, TEXTURE_FILTER_BILINEAR );
        m_ownFont = m_font.texture.id > 0;
    }
    if ( !m_ownFont ) m_font = GetFontDefault();
}

void Game::setLanguage()
{
    for ( size_t i = 0; i < m_signSurfaces.size() && i < m_level.m_signs.size(); ++i )
    {
        Material &m = m_surfaces[m_signSurfaces[i]].m_material;
        UnloadTexture( m.maps[MATERIAL_MAP_ALBEDO].texture );
        Texture2D tex = signTexture( m_assets, m_level.m_signs[i].m_texture );
        m.maps[MATERIAL_MAP_ALBEDO].texture = tex;
        m.maps[MATERIAL_MAP_EMISSION].texture = tex;
    }
    loadFont();
    renderBoard();
}

Progress Game::progress() const
{
    Progress p = m_progress;
    p.m_hero = m_heroKind;
    p.m_scrap = m_scrap;
    p.m_stims = m_stims;
    p.m_level = m_levelFile;
    if ( m_finished )
    {
        p.m_flags.insert( "done." + m_levelFile.substr( 0, m_levelFile.find( '.' ) ) );
        p.m_level = next();
    }
    return p;
}

std::string Game::title() const
{
    return std::string( tr( m_level.m_act ) ) + "  —  " + tr( m_level.m_name );
}

void Game::shutdown()
{
    m_audio.shutdown();
    if ( m_ownFont ) UnloadFont( m_font );
}

const char *difficultyName( DifficultyE d )
{
    switch ( d )
    {
    case DifficultyE::EASY: return tr( "difficulty.easy" );
    case DifficultyE::HARD: return tr( "difficulty.hard" );
    default: return tr( "difficulty.normal" );
    }
}

const char *heroKey( HeroE h )
{
    switch ( h )
    {
    case HeroE::DORRA: return "dorra";
    case HeroE::IONE: return "ione";
    default: return "corvin";
    }
}

float Game::hpMax() const
{
    return m_stats.m_health;
}

float Game::runSpeed() const
{
    return ( m_heroKind == HeroE::DORRA ? DorraSpeed : RunSpeed ) *
           Clamp( 1.0f + stat( StatE::SPEED ) / 100.0f, 0.7f, 1.5f );
}

float Game::foeRadius( const Foe &f ) const
{
    switch ( f.m_kind )
    {
    case FoeE::WELDER: return 0.85f;
    case FoeE::HEAVY: return 0.55f;
    case FoeE::TAXATOR: return 0.95f;
    case FoeE::DRONE: return 0.6f;
    case FoeE::SAW: return 0.42f;
    case FoeE::BOMBER: return 0.55f;
    default: return human( f ) ? 0.42f : 0.5f;
    }
}

float Game::foeHeight( const Foe &f ) const
{
    if ( f.m_kind == FoeE::DRONE ) return 0.0f;   // its position is its body, in the air
    if ( human( f ) ) return foeTall( f ) * 0.62f;
    return f.m_kind == FoeE::WELDER ? 0.75f : ( f.m_kind == FoeE::SAW ? 0.35f : 0.45f );
}

void Game::setDifficulty( DifficultyE d )
{
    float scale = toughness( d ) / toughness( m_difficulty );
    for ( Foe &f : m_foes ) f.m_hp *= scale, f.m_hpMax *= scale;
    m_tank[0] *= scale;
    m_tank[1] *= scale;
    m_core *= scale;
    m_difficulty = d;
}

void Game::spawnFoe( FoeE kind, Vector3 pos, bool awake )
{
    Foe f;
    f.m_kind = kind;
    f.m_pos = pos;
    const float hp[10] = {
        LouseHp, WelderHp, BatonHp, GunnerHp, ShieldHp, HeavyHp, DroneHp, SawHp, BomberHp, TaxatorHp
    };
    f.m_hp = hp[(int)kind] * toughness( m_difficulty ) * ( 1.0f + 0.25f * ( m_level.m_area - 1 ) );
    f.m_home = f.m_safe = pos;
    f.m_hpMax = f.m_hp;
    if ( kind == FoeE::DRONE ) f.m_rush.y = pos.y - 1.75f;
    f.m_awake = awake;
    f.m_yaw = rnd() * 6.28f;
    f.m_id = (int)m_foes.size();
    f.m_spit = 1.5f + rnd() * 1.5f;
    m_foes.push_back( f );
}

void Game::putHero( float x, float z, float yaw )
{
    m_pos = { x, m_level.floorAt( x, z ), z };
    if ( m_pos.y > 100.0f || m_pos.y < -100.0f ) m_pos.y = 0.0f;
    m_vel = { 0, 0, 0 };
    m_yaw = m_aimYaw = yaw * DEG2RAD;
    m_hero.animate( 0.0f, m_pos, { 0, 0 }, m_yaw, false, 0.0f, m_time );
    m_camTarget = Vector3Add( m_pos, { 0.0f, 1.0f, 0.0f } );
    m_cam.target = m_camTarget;
    m_cam.position =
        Vector3Add( m_camTarget, Vector3Scale( { 8.1f * sinf( m_camYaw ), 11.5f, 8.1f * cosf( m_camYaw ) }, m_zoom ) );
    m_cam.up = { 0.0f, 1.0f, 0.0f };
    m_cam.fovy = 38.0f;
    m_cam.projection = CAMERA_PERSPECTIVE;
}

void Game::wakeAll()
{
    for ( Foe &f : m_foes ) f.m_awake = true;
}

float Game::groundAt( Vector3 p ) const
{
    // The lift's platform covers its well.
    if ( m_liftMax.x > m_liftMin.x && p.x > m_liftMin.x && p.x < m_liftMax.x && p.z > m_liftMin.z && p.z < m_liftMax.z )
        return m_liftY;
    return m_level.floorAt( p.x, p.z );
}

bool Game::los( Vector3 a, Vector3 b ) const
{
    Vector3 hit, n;
    return !m_level.raycast( a, b, hit, n );
}

bool Game::gapBetween( Vector3 a, Vector3 b ) const
{
    // A shaft anywhere on the straight line between the two (every half metre).
    float len = Vector2Distance( { a.x, a.z }, { b.x, b.z } );
    int   steps = (int)( len / 0.5f );
    for ( int i = 1; i < steps; ++i )
        if ( groundAt( Vector3Lerp( a, b, (float)i / steps ) ) < -100.0f ) return true;
    return false;
}

void Game::blockHero( Vector3 &p ) const
{
    for ( const Capsule &c : m_capsules ) pushOutCircle( p, HeroRadius, c.m_pos.x, c.m_pos.z, 0.75f );
    for ( const Marker &m : m_level.m_markers )
        if ( m.m_kind == 'K' ) pushOutCircle( p, HeroRadius, m.m_pos.x, m.m_pos.z - 0.2f, 1.0f );
    for ( const Person &q : m_people ) pushOutCircle( p, HeroRadius, q.m_pos.x, q.m_pos.z, 0.45f );
    for ( const Cargo &q : m_cargo ) pushOutCircle( p, HeroRadius, q.m_pos.x, q.m_pos.z, 0.55f );
    if ( m_doorHalf > 0.0f && m_doorOpen < 0.8f && fabsf( p.x - m_door.x ) < m_doorHalf + 0.5f )
        pushOutRect( p, HeroRadius, m_door.x - m_doorHalf - 0.5f, m_door.x + m_doorHalf + 0.5f, m_door.z - 0.2f,
                     m_door.z + 0.2f );
    Vector3 o = m_pressModel.m_origin;
    if ( o.x != 0.0f )
    {
        pushOutRect( p, HeroRadius, o.x - 8.0f, o.x + 8.0f, o.z - 4.4f, o.z + 0.25f );
        for ( float side : { -1.0f, 1.0f } )
        {
            pushOutCircle( p, HeroRadius, o.x + side * 2.6f, o.z + 4.3f, 0.35f );
            pushOutCircle( p, HeroRadius, o.x + side * 9.2f, o.z + 0.5f, 1.2f );
        }
    }
}

void Game::fire( const std::string &trigger )
{
    if ( m_fired.count( trigger ) ) return;
    m_fired.insert( trigger );
    // The press's moments go before everything waiting: they matter now.
    bool urgent = trigger == "bossstart" || trigger == "ramwarn" || trigger == "coreopen" || trigger == "tank1" ||
                  trigger == "armour";
    if ( urgent )
    {
        m_hintQueue.clear();
        m_hint = nullptr;
        m_hintTimer = 0.0f;
    }
    // What one hero says or needs to know and the other does not: "-".
    const char *who = heroKey( m_heroKind );
    for ( const Line &l : m_level.m_lines )
        if ( l.m_trigger == trigger && strcmp( trAs( l.m_id, who ), "-" ) != 0 ) m_lineQueue.push_back( &l );
    for ( const Hint &h : m_level.m_hints )
        if ( h.m_trigger == trigger && strcmp( trAs( h.m_id, who ), "-" ) != 0 ) m_hintQueue.push_back( &h );
    // Entering the bridge wakes all who hold it.
    if ( trigger == "f" )
        for ( Foe &f : m_foes )
            if ( f.m_group == 1 ) f.m_awake = true;
}

bool Game::shielded( Vector3 from ) const
{
    if ( m_heroKind != HeroE::DORRA || m_block < 0.5f ) return false;
    Vector3 d = { from.x - m_pos.x, 0.0f, from.z - m_pos.z };
    float   len = Vector3Length( d );
    return len > 1e-3f && ( d.x * sinf( m_yaw ) + d.z * cosf( m_yaw ) ) / len > 0.35f;
}

void Game::hurtHero( float amount, Vector3 push, Vector3 from )
{
    if ( m_dead || m_invuln > 0.0f || m_finished || m_liftGoing || m_scene > 0 ) return;
    if ( shielded( from ) )
    {
        // The shield takes it: a clang, sparks off the plates, a little of it comes through.
        Vector3 at = Vector3Add( Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } ),
                                 Vector3Scale( { sinf( m_yaw ), 0.0f, cosf( m_yaw ) }, 0.5f ) );
        sparks( at, { sinf( m_yaw ), 0.3f, cosf( m_yaw ) }, 10, { 255, 220, 160, 255 }, 3.0f );
        m_audio.play( SfxE::BLOCK, at, m_camTarget, 0.8f );
        amount *= 0.15f;
        push = Vector3Scale( push, 0.3f );
    }
    amount *= harm( m_difficulty ) * ( 1.0f - armorCut() ) * m_areaHit * m_hitScale;
    // Thorns: whoever stands close takes some back.
    if ( stat( StatE::THORNS ) > 0.0f )
        for ( Foe &f : m_foes )
        {
            Vector3 d = Vector3Subtract( f.m_pos, m_pos );
            float   len = Vector3Length( d );
            if ( f.m_alive && !f.m_hidden && len < 2.6f )
                hurtFoe( f, stat( StatE::THORNS ), len > 1e-3f ? Vector3Scale( d, 1.0f / len ) : Vector3{ 0, 0, 1 } );
        }
    m_sinceHurt = 0.0f;
    m_hp -= amount;
    m_invuln = 0.45f;
    m_hurtFlash = 1.0f;
    m_vel = Vector3Add( m_vel, push );
    if ( push.y > 0.5f ) m_ground = false;
    m_audio.play( SfxE::HURT, m_pos, m_camTarget, 0.8f );
    if ( m_hp < hpMax() * 0.5f ) fire( "hurt" );
    if ( m_hp <= 0.0f )
    {
        m_hp = 0.0f;
        m_dead = true;
        m_deadTimer = 0.0f;
        ++m_deaths;
        if ( m_autoplay )
        {
            // Who stood near: what killed the autopilot.
            std::string near;
            const char *kinds[] = { "louse", "welder", "baton", "gunner", "shield",
                                    "heavy", "drone",  "saw",   "bomber", "taxator" };
            for ( const Foe &f : m_foes )
                if ( f.m_alive && !f.m_hidden && f.m_awake && Vector3Distance( f.m_pos, m_pos ) < 14.0f )
                    near += std::string( " " ) + kinds[(int)f.m_kind] + ( f.m_elite ? "*" : "" );
            TraceLog( LOG_WARNING, "DEATH: hit at %.1f,%.1f t=%.0f by%s", m_pos.x, m_pos.z, m_time, near.c_str() );
        }
    }
}

void Game::sparks( Vector3 p, Vector3 n, int count, Color c, float speed )
{
    for ( int i = 0; i < count; ++i )
    {
        Vector3 r = Vector3Normalize( { rnd() - 0.5f, rnd() - 0.5f, rnd() - 0.5f } );
        Vector3 v =
            Vector3Scale( Vector3Normalize( Vector3Add( n, Vector3Scale( r, 0.95f ) ) ), speed * ( 0.3f + rnd() ) );
        float life = 0.3f + rnd() * 0.6f;
        m_particles.push_back( { p, v, life, life, 0.01f + rnd() * 0.012f, c, 0 } );
    }
}

void Game::explode( Vector3 p, float size )
{
    sparks( p, { 0, 1, 0 }, (int)( 40 * size ), { 255, 170, 80, 255 }, 5.0f * size );
    m_particles.push_back( { p, { 0, 0, 0 }, 0.18f, 0.18f, 0.9f * size, { 255, 190, 120, 255 }, 2 } );
    for ( int i = 0; i < (int)( 8 * size ); ++i )
    {
        Vector3 v = { ( rnd() - 0.5f ) * 5.0f, 1.5f + rnd() * 3.5f, ( rnd() - 0.5f ) * 5.0f };
        m_debris.push_back( { p,
                              Vector3Scale( v, size ),
                              { rnd() * 6, rnd() * 6, rnd() * 6 },
                              { rnd() * 8 - 4, rnd() * 8 - 4, rnd() * 8 - 4 },
                              5.0f } );
    }
    m_flashes.push_back( { Vector3Add( p, { 0, 0.5f, 0 } ), { 30.0f, 16.0f, 6.0f }, 7.0f * size, 0.25f, 0.25f } );
    m_audio.play( size > 1.5f ? SfxE::BOOM : SfxE::POP, p, m_camTarget, 1.0f );
}

void Game::killFoe( Foe &f, bool drops )
{
    f.m_alive = false;
    ++m_kills;
    // Experience for every kill; now and then an item; a champion always leaves one, a better one.
    gainXp( xpFor( f ) );
    if ( drops && rnd() < 0.09f * ( 1.0f + stat( StatE::FIND ) / 100.0f ) )
        dropItem( Vector3Add( f.m_pos, { 0.0f, 0.8f, 0.0f } ), 1.0f );
    if ( f.m_elite == 1 ) dropItem( Vector3Add( f.m_pos, { 0.0f, 0.8f, 0.0f } ), 2.5f );
    if ( f.m_kind == FoeE::TAXATOR || f.m_elite == 2 ) bossDown( f );
    if ( f.m_kind == FoeE::BOMBER ) detonate( f, 32.0f, 3.2f );
    if ( f.m_affix == 2 ) detonate( f, 26.0f, 3.5f );
    float   r = rnd();
    Vector3 v = { ( rnd() - 0.5f ) * 2.0f, 2.5f, ( rnd() - 0.5f ) * 2.0f };
    if ( human( f ) )
    {
        // A boarder's suit bursts: its power cell shorts, the plates fly apart; now and then a stim rolls out.
        f.m_swingT = -1.0f;
        f.m_aim = 0.0f;
        burst( f );
        if ( drops && r < 0.3f ) m_pickups.push_back( { 1, 1, Vector3Add( f.m_pos, { 0, 0.6f, 0 } ), v } );
        return;
    }
    float size = f.m_kind == FoeE::WELDER ? 1.4f : 0.8f;
    explode( Vector3Add( f.m_pos, { 0, f.m_kind == FoeE::DRONE ? 0.0f : 0.4f, 0 } ), size );
    if ( !drops || f.m_kind == FoeE::BOMBER ) return;
    if ( f.m_kind == FoeE::WELDER )
    {
        m_pickups.push_back( { 0, 15, Vector3Add( f.m_pos, { 0, 0.5f, 0 } ), v } );
        if ( r < 0.4f ) m_pickups.push_back( { 1, 1, Vector3Add( f.m_pos, { 0, 0.5f, 0 } ), Vector3Negate( v ) } );
    }
    else if ( r < 0.6f )
        m_pickups.push_back( { 0, 3 + (int)( rnd() * 6 ), Vector3Add( f.m_pos, { 0, 0.4f, 0 } ), v } );
    else if ( r < 0.72f )
        m_pickups.push_back( { 1, 1, Vector3Add( f.m_pos, { 0, 0.4f, 0 } ), v } );
}

void Game::hurtFoe( Foe &f, float amount, Vector3 dir )
{
    if ( !f.m_alive ) return;
    Vector3 face = { sinf( f.m_yaw ), 0.0f, cosf( f.m_yaw ) };
    if ( f.m_kind == FoeE::SHIELD && f.m_swingT < 0.0f && f.m_stun <= 0.0f &&
         -( face.x * dir.x + face.z * dir.z ) > 0.4f )
    {
        // On the shield: sparks, a clang, little gets through (the ram hammer gets more through than slugs).
        amount *= m_heroKind == HeroE::DORRA ? 0.5f : 0.25f;
        sparks( Vector3Add( f.m_pos, Vector3Add( Vector3Scale( face, 0.55f ), { 0.0f, 1.2f, 0.0f } ) ), face, 6,
                { 160, 200, 255, 255 }, 2.5f );
        if ( rnd() < 0.3f ) m_audio.play( SfxE::BLOCK, f.m_pos, m_camTarget, 0.5f );
        fire( "shieldguard" );
    }
    if ( f.m_affix == 1 ) amount *= 0.5f;
    f.m_hp -= amount;
    f.m_flash = 1.0f;
    if ( !f.m_awake && !human( f ) ) m_audio.play( SfxE::CHITTER, f.m_pos, m_camTarget, 0.6f );
    f.m_awake = true;
    // Low gravity: a hit lifts a louse off its feet.
    float push = f.m_kind == FoeE::LOUSE ? 1.8f : ( human( f ) ? 0.8f : 0.5f );
    if ( f.m_kind == FoeE::TAXATOR ) push = 0.0f;
    if ( f.m_kind == FoeE::HEAVY ) push = 0.2f;
    f.m_vel = Vector3Add( f.m_vel, { dir.x * push, push * 0.45f, dir.z * push } );
    if ( f.m_kind == FoeE::LOUSE ) f.m_ground = false;
    if ( f.m_hp <= 0.0f ) killFoe( f, true );
}

Vector3 Game::targetPoint( const Target &t ) const
{
    Vector3 o = m_pressModel.m_origin;
    switch ( t.m_kind )
    {
    case 0:
    {
        const Foe &f = m_foes[t.m_index];
        return Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } );
    }
    case 1: return { o.x - 9.2f, 2.3f, o.z + 0.5f };
    case 2: return { o.x + 9.2f, 2.3f, o.z + 0.5f };
    case 3: return { o.x, 3.4f, o.z - 0.4f };
    default: return { 0, 0, 0 };
    }
}

Game::Target Game::findTarget( Vector3 from, Vector3 dir, float cone ) const
{
    Target best;
    float  bestScore = 1e9f;
    auto   consider = [&]( Target t )
    {
        Vector3 p = targetPoint( t );
        Vector3 d = Vector3Subtract( p, from );
        float   dist = sqrtf( d.x * d.x + d.z * d.z );
        if ( dist > 24.0f || dist < 0.3f ) return;
        float a = acosf( Clamp(
            ( d.x * dir.x + d.z * dir.z ) / ( dist * sqrtf( dir.x * dir.x + dir.z * dir.z ) + 1e-6f ), -1.0f, 1.0f ) );
        if ( a > cone ) return;
        if ( !los( from, p ) ) return;
        float score = a * 2.0f + dist * 0.03f;
        if ( score < bestScore )
        {
            bestScore = score;
            best = t;
            best.m_point = p;
        }
    };
    for ( int i = 0; i < (int)m_foes.size(); ++i )
        if ( m_foes[i].m_alive && !m_foes[i].m_hidden ) consider( { 0, i, {} } );
    if ( m_pressActive && !m_pressDead && !m_pressDying )
    {
        if ( m_tank[0] > 0.0f ) consider( { 1, 0, {} } );
        if ( m_tank[1] > 0.0f ) consider( { 2, 0, {} } );
        if ( m_pressPose.m_shutter > 0.85f && m_core > 0.0f ) consider( { 3, 0, {} } );
    }
    return best;
}

void Game::hitTank( int side, float amount, Vector3 at, Vector3 n )
{
    if ( m_tank[side] <= 0.0f ) return;
    Vector3 o = m_pressModel.m_origin;
    m_tank[side] -= amount;
    m_pressPose.m_tankFlash[side] = 1.0f;
    sparks( at, n, 8, { 140, 200, 255, 255 }, 3.0f );
    if ( m_tank[side] <= 0.0f )
    {
        m_tank[side] = 0.0f;
        Vector3 tp = { o.x + ( side ? 9.2f : -9.2f ), 2.0f, o.z + 0.5f };
        explode( tp, 2.0f );
        m_audio.play( SfxE::BURST, tp, m_camTarget, 1.0f );
        m_shake = 0.8f;
        if ( m_tank[1 - side] > 0.0f ) fire( "tank1" );
    }
}

void Game::hitCore( float amount, Vector3 at, Vector3 n )
{
    if ( m_pressPose.m_shutter < 0.85f || m_core <= 0.0f ) return;
    m_core -= amount;
    m_pressPose.m_coreFlash = 1.0f;
    sparks( at, n, 12, { 255, 160, 60, 255 }, 4.0f );
}

void Game::shoot()
{
    // Every pellet of the weapon is its own shot; the first carries the sound and the recoil.
    for ( int p = 0; p < m_stats.m_pellets; ++p ) shootPellet( p == 0 );
}

void Game::shootPellet( bool first )
{
    Vector3 fwd = { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
    Vector3 right = { -fwd.z, 0.0f, fwd.x };
    Vector3 muzzle = Vector3Add( Vector3Add( m_pos, { 0.0f, 1.3f, 0.0f } ),
                                 Vector3Add( Vector3Scale( fwd, 0.75f ), Vector3Scale( right, 0.06f ) ) );
    Vector3 dir = m_target.m_kind >= 0 ? Vector3Normalize( Vector3Subtract( targetPoint( m_target ), muzzle ) ) : fwd;
    float   spread = m_stats.m_spread * 2.0f + 0.012f;
    dir = Vector3Normalize(
        Vector3Add( dir, { ( rnd() - 0.5f ) * spread, ( rnd() - 0.5f ) * spread * 0.6f, ( rnd() - 0.5f ) * spread } ) );
    if ( first ) ++m_shots;

    Vector3 end = Vector3Add( muzzle, Vector3Scale( dir, m_stats.m_range ) );
    Vector3 hit, normal;
    m_level.raycast( muzzle, end, hit, normal );
    float tBest = Vector3Distance( muzzle, hit );
    int   kind = -1, index = -1;   // -1 wall, 0 foe, 1/2 tank, 3 core, 4 armour of the press
    // The foes on the line, nearest first: a slug that pierces goes on through the next ones.
    std::vector<std::pair<float, int>> along;
    for ( int i = 0; i < (int)m_foes.size(); ++i )
    {
        const Foe &f = m_foes[i];
        if ( !f.m_alive || f.m_hidden ) continue;
        float t = human( f )
                      ? rayCylinder( muzzle, dir, f.m_pos, foeRadius( f ), foeTall( f ) )
                      : raySphere( muzzle, dir, Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ), foeRadius( f ) );
        if ( t >= 0.0f && t < tBest ) along.push_back( { t, i } );
    }
    std::sort( along.begin(), along.end() );
    if ( !along.empty() ) tBest = along[0].first, kind = 0, index = along[0].second;
    Vector3 o = m_pressModel.m_origin;
    if ( o.x != 0.0f )
    {
        for ( int side = 0; side < 2; ++side )
        {
            float t = rayCylinder( muzzle, dir, { o.x + ( side ? 9.2f : -9.2f ), 0.0f, o.z + 0.5f }, 1.15f,
                                   m_tank[side] > 0.0f ? 4.7f : 1.9f );
            if ( t >= 0.0f && t < tBest ) tBest = t, kind = m_tank[side] > 0.0f ? 1 + side : 4;
        }
        if ( m_pressPose.m_shutter > 0.85f && m_core > 0.0f )
        {
            float t = raySphere( muzzle, dir, { o.x, 3.4f, o.z - 1.0f }, 0.95f );
            if ( t >= 0.0f && t < tBest ) tBest = t, kind = 3;
        }
        struct Box
        {
            Vector3 lo, hi;
        };
        float     shutter = m_pressPose.m_shutter;
        float     ram = m_pressPose.m_ram;
        const Box boxes[] = { { { o.x - 8.0f, 0.0f, o.z - 4.4f }, { o.x + 8.0f, 6.5f, o.z - 1.6f } },
                              { { o.x - 8.0f, 0.0f, o.z - 1.6f }, { o.x - 1.6f, 6.5f, o.z } },
                              { { o.x + 1.6f, 0.0f, o.z - 1.6f }, { o.x + 8.0f, 6.5f, o.z } },
                              { { o.x - 1.6f, 0.0f, o.z - 1.6f }, { o.x + 1.6f, 2.2f, o.z } },
                              { { o.x - 1.6f, 4.6f, o.z - 1.6f }, { o.x + 1.6f, 6.5f, o.z } },
                              { { o.x - 1.6f + shutter * 1.55f, 2.2f, o.z },
                                { o.x + 1.6f - shutter * 1.55f, 4.6f, o.z + 0.14f } },
                              { { o.x - 2.0f, ram - 0.8f, o.z + 0.35f }, { o.x + 2.0f, ram + 0.8f, o.z + 4.05f } } };
        for ( const Box &b : boxes )
        {
            if ( b.lo.x >= b.hi.x ) continue;
            float t = rayBox( muzzle, dir, b.lo, b.hi );
            if ( t >= 0.0f && t < tBest ) tBest = t, kind = 4;
        }
    }
    Vector3 at = Vector3Add( muzzle, Vector3Scale( dir, tBest ) );
    float   damage = m_stats.m_damage;
    if ( kind == 0 )
    {
        // The first one hit and those the slug goes on through; an arc jumps on from the first.
        int count = std::min( (int)along.size(), 1 + m_stats.m_pierce );
        for ( int k = 0; k < count; ++k )
        {
            Vector3 p = Vector3Add( muzzle, Vector3Scale( dir, along[k].first ) );
            dealHit( m_foes[along[k].second], damage * ( k == 0 ? 1.0f : 0.8f ), dir, p );
            sparks( p, Vector3Negate( dir ), 10, { 255, 200, 120, 255 }, 3.5f );
            at = p;
        }
        std::set<int> struck = { index };
        Vector3       from = Vector3Add( m_foes[index].m_pos, { 0.0f, 1.0f, 0.0f } );
        for ( int c = 0; c < m_stats.m_chain; ++c )
        {
            int   next = -1;
            float best = 7.0f;
            for ( int i = 0; i < (int)m_foes.size(); ++i )
            {
                const Foe &f = m_foes[i];
                if ( !f.m_alive || f.m_hidden || struck.count( i ) ) continue;
                float d = Vector3Distance( from, f.m_pos );
                if ( d < best && los( from, Vector3Add( f.m_pos, { 0.0f, 0.8f, 0.0f } ) ) ) best = d, next = i;
            }
            if ( next < 0 ) break;
            Vector3 to = Vector3Add( m_foes[next].m_pos, { 0.0f, 1.0f, 0.0f } );
            m_tracers.push_back( { from, to, 0.12f } );
            sparks( to, { 0.0f, 1.0f, 0.0f }, 8, { 150, 200, 255, 255 }, 2.5f );
            dealHit( m_foes[next], damage * 0.7f, Vector3Normalize( Vector3Subtract( to, from ) ), to );
            struck.insert( next );
            from = to;
        }
    }
    if ( m_stats.m_splash > 0.0f && kind <= 0 )
    {
        // A plasma ball bursts where it lands.
        explode( at, 0.5f );
        for ( Foe &f : m_foes )
        {
            if ( !f.m_alive || f.m_hidden ) continue;
            Vector3 d = Vector3Subtract( Vector3Add( f.m_pos, { 0.0f, foeHeight( f ), 0.0f } ), at );
            float   dist = Vector3Length( d );
            if ( dist > m_stats.m_splash + foeRadius( f ) ) continue;
            dealHit( f, damage * 0.5f * ( 1.0f - 0.5f * fminf( 1.0f, dist / m_stats.m_splash ) ),
                     dist > 1e-3f ? Vector3Scale( d, 1.0f / dist ) : dir, f.m_pos );
        }
    }
    if ( kind == 1 || kind == 2 )
        hitTank( kind - 1, damage, at, Vector3Negate( dir ) );
    else if ( kind == 3 )
        hitCore( damage, at, Vector3Negate( dir ) );
    else if ( kind == 4 )
    {
        // The press's armour: grey sparks that bounce off, and a ring for "this does nothing".
        sparks( at, Vector3Negate( dir ), 6, { 150, 150, 150, 255 }, 2.0f );
        m_armourHint += 1.0f;
    }
    else if ( kind < 0 )
        sparks( at, normal, 9, { 200, 225, 255, 255 }, 3.0f );
    m_tracers.push_back( { muzzle, at, 0.08f } );
    if ( !first ) return;
    m_flashes.push_back( { muzzle, { 10.0f, 16.0f, 24.0f }, 5.0f, 0.06f, 0.06f } );
    m_flashes.push_back( { Vector3Add( at, Vector3Scale( dir, -0.3f ) ), { 6.0f, 9.0f, 14.0f }, 3.5f, 0.1f, 0.1f } );
    m_audio.play( SfxE::SHOT, muzzle, m_camTarget, 0.85f );
    m_audio.play( SfxE::HIT, at, m_camTarget, 0.5f );

    // Recoil: on the ground a nudge, in the air a shove (heavy guns kick harder).
    float kick = ( m_ground ? 0.5f : 1.3f ) * Clamp( m_stats.m_heat / 7.5f, 0.6f, 2.5f );
    m_vel.x -= dir.x * kick;
    m_vel.z -= dir.z * kick;
    m_hero.recoil();
}

void Game::updateHero( float dt, const GameInput &in )
{
    if ( m_finished || m_scene >= 2 )
    {
        // Over, or shot down in the last scene.
        m_vel = { 0, 0, 0 };
        m_swing = -1;
        m_hero.m_swing = -1;
        m_hero.animate( dt, m_pos, { 0, 0 }, m_yaw, false, m_sceneDown, m_time );
        return;
    }
    if ( m_dead )
    {
        m_deadTimer += dt;
        m_vel.x *= 0.9f;
        m_vel.z *= 0.9f;
        m_swing = -1;
        m_hero.m_swing = -1;
        m_hero.animate( dt, m_pos, { 0, 0 }, m_yaw, false, fminf( 1.0f, m_deadTimer / 0.5f ), m_time );
        if ( m_deadTimer > 2.4f )
        {
            // Back at the last capsule.
            m_dead = false;
            m_hp = hpMax();
            m_heat = 0.0f;
            m_overheat = false;
            m_pos = m_respawn;
            m_vel = { 0, 0, 0 };
            m_invuln = 1.5f;
            for ( int i = 0; i < 30; ++i )
                m_particles.push_back(
                    { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.8f, rnd() * 2.0f, ( rnd() - 0.5f ) * 0.8f } ),
                      { 0, 0.6f, 0 },
                      1.0f,
                      1.0f,
                      0.02f,
                      { 120, 255, 160, 255 },
                      3 } );
            m_audio.play( SfxE::CAPSULE, m_pos, m_camTarget, 0.8f );
        }
        return;
    }

    Vector2 move = m_liftGoing || m_scene > 1 ? Vector2{ 0.0f, 0.0f } : in.m_move;
    if ( Vector2Length( move ) > 1.0f ) move = Vector2Normalize( move );
    // Dorra slows while she swings or holds the shield up.
    float pace = 1.0f;
    if ( m_heroKind == HeroE::DORRA ) pace = ( m_swing >= 0 ? 0.4f : 1.0f ) * ( 1.0f - 0.45f * m_block );
    Vector2 want = Vector2Scale( move, runSpeed() * pace );
    Vector2 v = { m_vel.x, m_vel.z };
    Vector2 dv = Vector2Subtract( want, v );
    // A dodge of the ship pulls the floor from under the feet: less grip, and the push itself.
    float grip = 1.0f - 0.75f * fminf( 1.0f, m_thrustAmt / ThrustPeak );
    float step = ( m_ground ? GroundAccel * grip : AirAccel ) * dt;
    if ( Vector2Length( dv ) > step ) dv = Vector2Scale( Vector2Normalize( dv ), step );
    m_vel.x += dv.x + thrust().x * dt;
    m_vel.z += dv.y + thrust().z * dt;
    if ( in.m_jump && m_ground && m_swing < 0 && m_scene == 0 )
    {
        m_vel.y = m_heroKind == HeroE::DORRA ? DorraJump : JumpSpeed;
        m_ground = false;
        m_audio.play( SfxE::JUMP, m_pos, m_camTarget, 0.6f );
        if ( m_autoplay )
            TraceLog( LOG_WARNING, "JUMP at %.1f,%.1f v=%.1f,%.1f t=%.1f", m_pos.x, m_pos.z, m_vel.x, m_vel.z, m_time );
    }
    if ( m_heroKind == HeroE::DORRA )
        updateMelee( dt, in, move );
    else
        updateGunner( dt, in, move );

    // A roll or a charge carries the hero (stopping short of a shaft); the charge strikes what it runs through.
    if ( m_dash > 0.0f )
    {
        m_dash -= dt;
        float speed = m_dashKind == 1 ? 17.0f : 13.0f;
        if ( groundAt( Vector3Add( m_pos, Vector3Scale( m_dashDir, 0.9f ) ) ) < -100.0f ) m_dash = 0.0f;
        m_vel.x = m_dashDir.x * speed * ( m_dash > 0.0f ? 1.0f : 0.3f );
        m_vel.z = m_dashDir.z * speed * ( m_dash > 0.0f ? 1.0f : 0.3f );
        if ( m_dashKind == 1 )
            for ( int i = 0; i < (int)m_foes.size(); ++i )
            {
                Foe &f = m_foes[i];
                if ( !f.m_alive || f.m_hidden || m_dashHit.count( i ) || Vector3Distance( f.m_pos, m_pos ) > 1.8f )
                    continue;
                m_dashHit.insert( i );
                dealHit( f, m_dashDamage, m_dashDir, f.m_pos );
                f.m_vel = Vector3Add( f.m_vel, { m_dashDir.x * 6.0f, 2.5f, m_dashDir.z * 6.0f } );
                f.m_ground = false;
                f.m_stun = fmaxf( f.m_stun, 0.6f );
                m_audio.play( SfxE::SMASH, f.m_pos, m_camTarget, 0.8f );
            }
        if ( rnd() < dt * 40.0f )
            m_particles.push_back( { Vector3Add( m_pos, { 0.0f, 0.2f, 0.0f } ),
                                     { -m_dashDir.x, 0.4f, -m_dashDir.z },
                                     0.5f,
                                     0.5f,
                                     0.2f,
                                     { 180, 165, 150, 30 },
                                     1 } );
    }
    // Health that comes back by itself (gear, Nanoheilung).
    if ( stat( StatE::REGEN ) > 0.0f ) m_hp = fminf( hpMax(), m_hp + stat( StatE::REGEN ) * dt );

    if ( in.m_stim && m_stims > 0 && m_hp < hpMax() )
    {
        m_hp = fminf( hpMax(), m_hp + StimHeal * ( 1.0f + stat( StatE::STIMS ) / 100.0f ) );
        --m_stims;
        m_audio.play( SfxE::STIM, m_pos, m_camTarget, 0.8f );
        for ( int i = 0; i < 16; ++i )
            m_particles.push_back(
                { Vector3Add( m_pos, { ( rnd() - 0.5f ) * 0.6f, 0.5f + rnd() * 1.2f, ( rnd() - 0.5f ) * 0.6f } ),
                  { 0, 0.8f, 0 },
                  0.8f,
                  0.8f,
                  0.015f,
                  { 120, 255, 140, 255 },
                  3 } );
    }
    moveHero( dt );
}

void Game::updateGunner( float dt, const GameInput &in, Vector2 move )
{
    // Aim: the mouse for a while after it moved (the autopilot names a direction), else where Corvin runs, or where he
    // faces while strafing; the rifle finds the targets.
    Vector3 chest = Vector3Add( m_pos, { 0.0f, 1.25f, 0.0f } );
    bool    manual = false;
    if ( in.m_mouseAim )
    {
        Ray   ray = GetScreenToWorldRay( in.m_mouse, m_cam );
        float plane = chest.y;
        if ( fabsf( ray.direction.y ) > 1e-3f )
        {
            float   t = ( plane - ray.position.y ) / ray.direction.y;
            Vector3 p = Vector3Add( ray.position, Vector3Scale( ray.direction, t ) );
            if ( t > 0.0f && Vector3Distance( p, chest ) > 0.4f )
            {
                m_aimYaw = atan2f( p.x - chest.x, p.z - chest.z );
                m_aimTimer = 2.0f;
            }
        }
    }
    if ( Vector2Length( in.m_stick ) > 0.3f )
    {
        m_aimYaw = atan2f( in.m_stick.x, in.m_stick.y );
        m_aimTimer = 2.0f;
    }
    if ( m_aimTimer > 0.0f )
    {
        m_aimTimer -= dt;
        manual = true;
    }
    else if ( in.m_strafe )
        m_aimYaw = m_yaw;   // holding the facing: Corvin backs off or sidesteps while he shoots
    else if ( Vector2Length( move ) > 0.2f )
        m_aimYaw = atan2f( move.x, move.y );
    Vector3 aimDir = { sinf( m_aimYaw ), 0.0f, cosf( m_aimYaw ) };
    m_target = findTarget( chest, aimDir, manual ? AssistCone : AutoCone );
    m_firing = in.m_fire;
    float faceYaw = m_aimYaw;
    if ( m_firing && m_target.m_kind >= 0 )
    {
        Vector3 p = targetPoint( m_target );
        faceYaw = atan2f( p.x - m_pos.x, p.z - m_pos.z );
    }
    else if ( !manual && !m_firing && !in.m_strafe && Vector2Length( move ) > 0.2f )
        faceYaw = atan2f( move.x, move.y );
    float turn = ( m_firing || manual ? 16.0f : 11.0f ) * dt;
    m_yaw += Clamp( angleTo( m_yaw, faceYaw ), -turn, turn );
    m_aimDir = aimDir;

    m_fireTimer -= dt;
    if ( m_firing && !m_overheat && m_fireTimer <= 0.0f )
    {
        shoot();
        // Overcharged: twice as fast, and no heat.
        m_fireTimer = m_stats.m_interval * ( m_overcharge > 0.0f ? 0.5f : 1.0f );
        m_heat += m_overcharge > 0.0f ? 0.0f : m_stats.m_heat;
        m_coolDelay = 0.3f;
        if ( m_heat >= 100.0f )
        {
            m_heat = 100.0f;
            m_overheat = true;
            fire( "overheat" );
            m_audio.play( SfxE::OVERHEAT, m_pos, m_camTarget, 0.8f );
        }
    }
    if ( m_coolDelay > 0.0f )
        m_coolDelay -= dt;
    else
        m_heat = fmaxf( 0.0f, m_heat - ( m_overheat ? 55.0f : 45.0f ) * dt );
    if ( m_overheat && m_heat < 25.0f ) m_overheat = false;
}

void Game::moveHero( float dt )
{
    // Move, collide, land.
    bool wasAir = !m_ground;
    m_vel.y -= m_level.m_gravity * dt;
    m_pos = Vector3Add( m_pos, Vector3Scale( m_vel, dt ) );
    m_level.collide( m_pos, HeroRadius, StepUp );
    blockHero( m_pos );
    float floor = groundAt( m_pos );
    if ( m_pos.y <= floor && m_vel.y <= 0.0f && floor > -100.0f )
    {
        if ( wasAir && m_vel.y < -1.5f ) m_audio.play( SfxE::LAND, m_pos, m_camTarget, 0.6f );
        m_pos.y = floor;
        m_vel.y = 0.0f;
        m_ground = true;
    }
    else if ( m_pos.y > floor + 0.08f )
        m_ground = false;
    if ( m_pos.y < -10.0f && !m_liftGoing )
    {
        m_hp = 0.0f;
        m_dead = true;
        m_deadTimer = 1.0f;
        ++m_deaths;
        if ( m_autoplay ) TraceLog( LOG_WARNING, "DEATH: fall at %.1f,%.1f t=%.0f", m_pos.x, m_pos.z, m_time );
    }
    m_invuln -= dt;
    m_hurtFlash = fmaxf( 0.0f, m_hurtFlash - dt * 2.5f );
    // On the easiest level health comes back slowly once nothing has hit for a while.
    m_sinceHurt += dt;
    if ( m_difficulty == DifficultyE::EASY && m_sinceHurt > 4.0f ) m_hp = fminf( hpMax(), m_hp + 2.5f * dt );

    // The lift: once the press is dead, standing on it a moment takes it down; the level ends below.
    bool onLift = m_liftMax.x > m_liftMin.x && m_pos.x > m_liftMin.x && m_pos.x < m_liftMax.x &&
                  m_pos.z > m_liftMin.z && m_pos.z < m_liftMax.z && m_ground;
    if ( liftOpen() && onLift && !m_liftGoing )
    {
        m_liftWait += dt;
        if ( m_liftWait > 0.8f )
        {
            m_liftGoing = true;
            m_audio.play( SfxE::KLAXON, m_lift, m_camTarget, 0.6f );
        }
    }
    else if ( !onLift )
        m_liftWait = 0.0f;
    if ( m_liftGoing )
    {
        m_liftY -= dt * fminf( 2.2f, 0.4f + m_liftY * -0.4f + 0.6f );
        if ( onLift || m_pos.y > m_liftY - 0.5f )
        {
            m_pos.y = m_liftY;
            m_vel = { 0, 0, 0 };
            m_ground = true;
        }
        if ( m_liftY < -11.0f ) m_finished = true;
    }

    // Capsules, pickups, triggers, the exit.
    for ( Capsule &c : m_capsules )
        if ( !c.m_active && Vector3Distance( c.m_pos, m_pos ) < 2.6f )
        {
            for ( Capsule &o : m_capsules ) o.m_active = false;
            c.m_active = true;
            m_respawn = c.m_front;
            fire( "capsule" );
            m_audio.play( SfxE::CAPSULE, c.m_pos, m_camTarget, 0.9f );
        }
    for ( Pickup &p : m_pickups )
    {
        if ( !p.m_alive ) continue;
        Vector3 d = Vector3Subtract( Vector3Add( m_pos, { 0, 0.6f, 0 } ), p.m_pos );
        float   dist = Vector3Length( d );
        if ( dist < 2.6f && ( p.m_kind == 0 || m_stims < 9 ) )
            p.m_pos = Vector3Add( p.m_pos, Vector3Scale( d, fminf( 1.0f, dt * 6.0f ) ) );
        if ( dist < 0.8f )
        {
            p.m_alive = false;
            if ( p.m_kind == 0 )
                m_scrap += (int)roundf( p.m_amount * ( 1.0f + stat( StatE::FIND ) / 100.0f ) );
            else
                ++m_stims;
            m_audio.play( SfxE::PICKUP, m_pos, m_camTarget, 0.7f );
        }
    }
    int r, c;
    m_level.cellOf( m_pos, r, c );
    char trig = m_level.cell( r, c ).m_trigger;
    // z is the level's last scene: it waits for the gate's event.
    if ( trig == 'z' && !m_fired.count( m_level.m_gate ) ) trig = 0;
    if ( trig ) fire( std::string( 1, trig ) );

    m_hero.m_swing = m_swing;
    m_hero.m_swingT = m_swingT;
    m_hero.m_block = m_block;
    m_hero.animate( dt, m_pos, { m_vel.x, m_vel.z }, m_yaw, !m_ground, 0.0f, m_time );
}

void Game::flow( int r, int c )
{
    int rows = m_level.rows(), cols = m_level.cols();
    m_flowDist.assign( (size_t)rows * cols, -1 );
    if ( !m_level.walkable( r, c ) ) return;
    std::deque<int> q;
    m_flowDist[(size_t)r * cols + c] = 0;
    q.push_back( r * cols + c );
    const int DR[4] = { -1, 0, 1, 0 }, DC[4] = { 0, 1, 0, -1 };
    while ( !q.empty() )
    {
        int cur = q.front();
        q.pop_front();
        int   cr = cur / cols, cc = cur % cols;
        float h = m_level.centre( cr, cc ).y;
        for ( int d = 0; d < 4; ++d )
        {
            int nr = cr + DR[d], nc = cc + DC[d];
            if ( !m_level.walkable( nr, nc ) || m_blocked.count( nr * cols + nc ) ) continue;
            if ( fabsf( m_level.centre( nr, nc ).y - h ) > 1.1f ) continue;
            int n = nr * cols + nc;
            if ( m_flowDist[n] >= 0 ) continue;
            m_flowDist[n] = m_flowDist[cur] + 1;
            q.push_back( n );
        }
    }
}

Vector3 Game::flowDir( Vector3 from ) const
{
    int r, c;
    m_level.cellOf( from, r, c );
    int cols = m_level.cols();
    if ( m_flowDist.empty() || r < 0 || c < 0 || r >= m_level.rows() || c >= cols ) return { 0, 0, 0 };
    int here = m_flowDist[(size_t)r * cols + c];
    if ( here < 0 ) return { 0, 0, 0 };
    int bestR = r, bestC = c, best = here;
    for ( int dr = -1; dr <= 1; ++dr )
        for ( int dc = -1; dc <= 1; ++dc )
        {
            int nr = r + dr, nc = c + dc;
            if ( nr < 0 || nc < 0 || nr >= m_level.rows() || nc >= cols ) continue;
            int d = m_flowDist[(size_t)nr * cols + nc];
            if ( d < 0 || d >= best ) continue;
            if ( dr && dc && ( m_flowDist[(size_t)r * cols + nc] < 0 || m_flowDist[(size_t)nr * cols + c] < 0 ) )
                continue;
            best = d, bestR = nr, bestC = nc;
        }
    if ( best == here ) return { 0, 0, 0 };
    Vector3 to = m_level.centre( bestR, bestC );
    Vector3 d = { to.x - from.x, 0.0f, to.z - from.z };
    return Vector3Normalize( d );
}

void Game::updateFoes( float dt )
{
    int hr, hc;
    m_level.cellOf( m_pos, hr, hc );
    m_flowTimer -= dt;
    if ( hr != m_flowR || hc != m_flowC || m_flowTimer <= 0.0f )
    {
        flow( hr, hc );
        m_flowR = hr, m_flowC = hc;
        m_flowTimer = 1.0f;
    }
    Vector3 chest = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
    float   g = m_level.m_gravity;
    for ( Foe &f : m_foes )
    {
        if ( f.m_hidden ) continue;
        if ( !f.m_alive )
        {
            // A fallen boarder: down, and slid along by the ship's dodges.
            if ( human( f ) )
            {
                f.m_down = fminf( 1.0f, f.m_down + dt * 2.2f );
                f.m_pos = Vector3Add( f.m_pos, Vector3Scale( thrust(), dt * 0.15f ) );
                m_level.collide( f.m_pos, 0.4f, 0.3f );
            }
            continue;
        }
        // Burning eats away at it; stunned, it stands.
        if ( f.m_burnTime > 0.0f )
        {
            f.m_burnTime -= dt;
            f.m_hp -= f.m_burn * dt;
            if ( rnd() < dt * 14.0f )
                m_particles.push_back(
                    { Vector3Add( f.m_pos, { ( rnd() - 0.5f ) * 0.5f, 0.3f + rnd(), ( rnd() - 0.5f ) * 0.5f } ),
                      { 0.0f, 1.2f, 0.0f },
                      0.6f,
                      0.6f,
                      0.02f,
                      { 255, 140, 50, 255 },
                      3 } );
            if ( f.m_hp <= 0.0f )
            {
                killFoe( f, true );
                continue;
            }
        }
        bool stunned = f.m_stun > 0.0f;
        if ( stunned ) f.m_stun -= dt;
        bool    welder = f.m_kind == FoeE::WELDER;
        bool    man = human( f );
        Vector3 eye = Vector3Add( f.m_pos, { 0.0f, welder ? 0.9f : ( man ? 1.55f : 0.5f ), 0.0f } );
        Vector3 to = Vector3Subtract( m_pos, f.m_pos );
        float   dist = sqrtf( to.x * to.x + to.z * to.z );
        Vector3 dir = dist > 1e-3f ? Vector3{ to.x / dist, 0.0f, to.z / dist } : Vector3{ 0, 0, 1 };
        f.m_losTimer -= dt;
        if ( f.m_losTimer <= 0.0f && dist < 26.0f )
        {
            f.m_los = los( eye, chest );
            f.m_losTimer = 0.2f + rnd() * 0.1f;
        }
        if ( !f.m_awake && !m_dead && ( ( dist < 13.0f && f.m_los ) || dist < 4.0f ) )
        {
            f.m_awake = true;
            if ( f.m_elite == 1 ) fire( "champion" );
            if ( !man ) m_audio.play( SfxE::CHITTER, f.m_pos, m_camTarget, 0.7f );
            for ( Foe &o : m_foes )
                if ( o.m_alive && !o.m_hidden && Vector3Distance( o.m_pos, f.m_pos ) < 7.0f ) o.m_awake = true;
        }

        Vector3 want = { 0, 0, 0 };
        float   faceYaw = f.m_yaw;
        bool    chase = f.m_awake && !m_dead && !m_finished && m_scene == 0 && !stunned;
        bool    drone = f.m_kind == FoeE::DRONE;
        f.m_timer -= dt;
        // A champion hits harder (berserk harder still); a swift one is quicker at everything.
        m_hitScale = f.m_elite ? ( f.m_affix == 3 ? 1.6f : 1.25f ) : 1.0f;
        if ( f.m_affix == 0 ) f.m_timer -= dt * 0.4f, f.m_spit -= dt * 0.4f;
        if ( chase && f.m_kind == FoeE::TAXATOR && !m_fired.count( "bossstart" ) ) fire( "bossstart" );
        if ( chase )
        {
            Vector3 path = ( f.m_los && dist < 9.0f ) ? dir : flowDir( f.m_pos );
            if ( Vector3Length( path ) < 0.5f ) path = dir;
            if ( f.m_kind == FoeE::SHIELD )
                updateGuard( f, dt, dir, dist, path, want, faceYaw );
            else if ( f.m_kind == FoeE::HEAVY )
                updateHeavy( f, dt, dir, dist, path, want, faceYaw );
            else if ( f.m_kind == FoeE::TAXATOR )
                updateTaxator( f, dt, dir, dist, path, want, faceYaw );
            else if ( man )
                updateBoarder( f, dt, dir, dist, path, want, faceYaw );
            else if ( f.m_kind == FoeE::SAW || f.m_kind == FoeE::BOMBER || drone )
                updateCritter( f, dt, dir, dist, path, want, faceYaw );
            else if ( !welder )
            {
                switch ( f.m_state )
                {
                case FoeStateE::CHASE:
                    want = Vector3Scale( path, 3.4f );
                    faceYaw = atan2f( path.x, path.z );
                    if ( dist < 3.8f && f.m_los && f.m_ground && fabsf( m_pos.y - f.m_pos.y ) < 1.5f &&
                         !gapBetween( f.m_pos, m_pos ) )
                    {
                        f.m_state = FoeStateE::WINDUP;
                        f.m_timer = 0.35f;
                    }
                    break;
                case FoeStateE::WINDUP:
                    faceYaw = atan2f( dir.x, dir.z );
                    f.m_crouch = fminf( 1.0f, f.m_crouch + dt * 4.0f );
                    if ( f.m_timer <= 0.0f )
                    {
                        f.m_state = FoeStateE::LEAP;
                        f.m_vel = { dir.x * 6.5f, 2.4f, dir.z * 6.5f };
                        f.m_ground = false;
                        f.m_timer = 3.0f;
                        m_audio.play( SfxE::LEAP, f.m_pos, m_camTarget, 0.7f );
                    }
                    break;
                case FoeStateE::LEAP:
                    faceYaw = atan2f( f.m_vel.x, f.m_vel.z );
                    if ( Vector3Distance( Vector3Add( f.m_pos, { 0, 0.4f, 0 } ), chest ) < 1.05f )
                    {
                        hurtHero( 10.0f, { dir.x * 2.5f, 1.0f, dir.z * 2.5f }, f.m_pos );
                        f.m_vel = { -dir.x * 2.0f, 1.0f, -dir.z * 2.0f };
                    }
                    if ( f.m_ground )
                    {
                        f.m_state = FoeStateE::RECOVER;
                        f.m_timer = 0.55f;
                    }
                    break;
                case FoeStateE::RECOVER:
                default:
                    f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 3.0f );
                    if ( f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE;
                    break;
                }
                if ( f.m_state != FoeStateE::WINDUP ) f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 3.0f );
            }
            else
            {
                // The welder keeps its distance, circles, and spits molten metal in arcs.
                faceYaw = atan2f( dir.x, dir.z );
                if ( f.m_state == FoeStateE::WINDUP )
                {
                    f.m_torch = fminf( 1.0f, f.m_torch + dt * 2.0f );
                    f.m_crouch = fminf( 0.6f, f.m_crouch + dt * 2.0f );
                    if ( f.m_timer <= 0.0f )
                    {
                        Vector3 nozzle = Vector3Add( f.m_pos, { dir.x * 1.4f, 0.75f, dir.z * 1.4f } );
                        for ( int i = 0; i < 3; ++i )
                        {
                            Vector3 aim = Vector3Add( m_pos, { m_vel.x * 0.6f + ( rnd() - 0.5f ) * 1.5f, 1.0f,
                                                               m_vel.z * 0.6f + ( rnd() - 0.5f ) * 1.5f } );
                            Vector3 d = Vector3Subtract( aim, nozzle );
                            float   flat = sqrtf( d.x * d.x + d.z * d.z );
                            float   t = fmaxf( 0.6f, flat / 9.0f );
                            Vector3 vel = { d.x / t, ( d.y + 0.5f * g * t * t ) / t, d.z / t };
                            m_blobs.push_back( { nozzle, vel, 4.0f } );
                        }
                        m_audio.play( SfxE::SPIT, nozzle, m_camTarget, 0.8f );
                        f.m_state = FoeStateE::RECOVER;
                        f.m_timer = 0.6f;
                        f.m_spit = 2.4f + rnd();
                    }
                }
                else
                {
                    f.m_torch = fmaxf( 0.0f, f.m_torch - dt * 1.5f );
                    f.m_crouch = fmaxf( 0.0f, f.m_crouch - dt * 2.0f );
                    if ( f.m_state == FoeStateE::RECOVER && f.m_timer <= 0.0f ) f.m_state = FoeStateE::CHASE;
                    if ( !f.m_los || dist > 9.0f )
                        want = Vector3Scale( path, 1.7f );
                    else if ( dist < 5.0f )
                        want = Vector3Scale( dir, -1.6f );
                    else
                    {
                        float side = ( f.m_id % 2 ) ? 1.0f : -1.0f;
                        want = { -dir.z * side * 1.2f, 0.0f, dir.x * side * 1.2f };
                    }
                    f.m_spit -= dt;
                    if ( f.m_spit <= 0.0f && f.m_los && dist < 15.0f && f.m_state == FoeStateE::CHASE )
                    {
                        f.m_state = FoeStateE::WINDUP;
                        f.m_timer = 0.6f;
                    }
                }
            }
        }
        if ( !f.m_alive ) continue;   // a bomber went off
        // A boss keeps to its hall: led too far away, it walks back.
        if ( ( f.m_kind == FoeE::TAXATOR || f.m_elite == 2 ) && Vector3Distance( f.m_pos, f.m_home ) > 16.0f &&
             Vector3Distance( m_pos, f.m_home ) > 14.0f )
        {
            Vector3 back = Vector3Subtract( f.m_home, f.m_pos );
            back.y = 0.0f;
            want = Vector3Scale( Vector3Normalize( back ), 2.4f );
            faceYaw = atan2f( back.x, back.z );
            f.m_state = FoeStateE::CHASE;
        }
        if ( f.m_affix == 0 || f.m_affix == 3 ) want = Vector3Scale( want, f.m_affix == 0 ? 1.45f : 1.15f );
        if ( drone )
        {
            // A drone flies: no edges, no gravity, it eases towards what it wants (up and down too).
            f.m_vel = Vector3Lerp( f.m_vel, want, fminf( 1.0f, dt * 3.0f ) );
            f.m_pos = Vector3Add( f.m_pos, Vector3Scale( f.m_vel, dt ) );
            m_level.collide( f.m_pos, 0.45f, 3.0f );
            f.m_ground = true;
            f.m_yaw += Clamp( angleTo( f.m_yaw, faceYaw ), -6.0f * dt, 6.0f * dt );
            f.m_flash = fmaxf( 0.0f, f.m_flash - dt * 5.0f );
            continue;
        }
        // Nobody walks off an edge: at a shaft the wish to go on stops, or slides along it (a hit or a fall from a
        // leap still carries them over).
        if ( f.m_ground && Vector3Length( want ) > 0.1f )
        {
            float speed = sqrtf( f.m_vel.x * f.m_vel.x + f.m_vel.z * f.m_vel.z );
            float look = foeRadius( f ) + 0.35f + speed * 0.25f;
            auto  edge = [&]( Vector3 w )
            {
                float len = Vector3Length( w );
                return len > 0.05f && groundAt( Vector3Add( f.m_pos, Vector3Scale( w, look / len ) ) ) < -100.0f;
            };
            if ( edge( want ) )
            {
                Vector3 alongX = { want.x, 0.0f, 0.0f }, alongZ = { 0.0f, 0.0f, want.z };
                want = !edge( alongX ) ? alongX : ( !edge( alongZ ) ? alongZ : Vector3{ 0.0f, 0.0f, 0.0f } );
                if ( Vector3Length( want ) < 0.1f ) f.m_vel.x = f.m_vel.z = 0.0f;
            }
        }
        // Physics: walk on the ground, fly in a leap or when hit; the ship's dodges push everyone.
        if ( f.m_ground )
        {
            Vector3 dv = { want.x - f.m_vel.x, 0.0f, want.z - f.m_vel.z };
            float   len = sqrtf( dv.x * dv.x + dv.z * dv.z );
            float   step = 14.0f * dt * ( 1.0f - 0.75f * fminf( 1.0f, m_thrustAmt / ThrustPeak ) );
            if ( len > step ) dv = Vector3Scale( dv, step / len );
            f.m_vel.x += dv.x;
            f.m_vel.z += dv.z;
        }
        f.m_vel = Vector3Add( f.m_vel, Vector3Scale( thrust(), dt ) );
        f.m_vel.y -= g * dt;
        f.m_pos = Vector3Add( f.m_pos, Vector3Scale( f.m_vel, dt ) );
        m_level.collide( f.m_pos, welder ? 0.7f : fminf( 0.9f, foeRadius( f ) ),
                         welder ? 0.6f : ( man ? StepUp : 1.1f ) );
        for ( const Person &q : m_people ) pushOutCircle( f.m_pos, 0.4f, q.m_pos.x, q.m_pos.z, 0.45f );
        for ( const Cargo &q : m_cargo ) pushOutCircle( f.m_pos, 0.4f, q.m_pos.x, q.m_pos.z, 0.55f );
        if ( m_doorHalf > 0.0f && m_doorOpen < 0.8f && fabsf( f.m_pos.x - m_door.x ) < m_doorHalf + 0.5f &&
             fabsf( f.m_pos.z - m_door.z ) < 1.2f )
        {
            // The shut gate: thick enough that no blow carries anyone through it.
            pushOutRect( f.m_pos, 0.4f, m_door.x - m_doorHalf - 0.5f, m_door.x + m_doorHalf + 0.5f, m_door.z - 0.6f,
                         m_door.z + 0.6f );
            f.m_vel.z = 0.0f;
        }
        // Never left inside rock or a shut door (a hard blow, a fall): back to where it last stood.
        {
            int sr, sc;
            m_level.cellOf( f.m_pos, sr, sc );
            const Cell &k = m_level.cell( sr, sc );
            if ( k.m_kind == CellE::ROCK || ( k.m_kind == CellE::DOOR && !k.m_open ) )
            {
                f.m_pos.x = f.m_safe.x, f.m_pos.z = f.m_safe.z;
                f.m_vel.x = f.m_vel.z = 0.0f;
            }
            else
                f.m_safe = f.m_pos;
        }
        if ( man )
        {
            FigurePose p;
            p.m_vel = { f.m_vel.x, f.m_vel.z };
            p.m_yaw = f.m_yaw;
            p.m_phase = f.m_phase;
            p.m_air = !f.m_ground;
            Figure::step( p, dt );
            f.m_phase = p.m_phase;
        }
        float floor = groundAt( f.m_pos );
        if ( f.m_pos.y <= floor && f.m_vel.y <= 0.0f && floor > -100.0f )
        {
            f.m_pos.y = floor;
            f.m_vel.y = 0.0f;
            f.m_ground = true;
        }
        else if ( f.m_pos.y > floor + 0.1f )
            f.m_ground = false;
        if ( f.m_pos.y < -12.0f )
        {
            f.m_alive = false;   // fell into a shaft
            ++m_kills;
            continue;
        }
        Vector3 o = m_pressModel.m_origin;
        if ( o.x != 0.0f ) pushOutRect( f.m_pos, 0.4f, o.x - 8.0f, o.x + 8.0f, o.z - 4.4f, o.z + 0.25f );
        float turn = f.m_kind == FoeE::SHIELD ? 2.6f : ( f.m_kind == FoeE::TAXATOR ? 3.0f : 8.0f );
        f.m_yaw += Clamp( angleTo( f.m_yaw, faceYaw ), -turn * dt, turn * dt );
        f.m_flash = fmaxf( 0.0f, f.m_flash - dt * 5.0f );
    }
    m_hitScale = 1.0f;
    // Those the Taxator called come in now (the list must not grow while it is walked).
    for ( const auto &s : m_summons )
    {
        spawnFoe( s.first, s.second, true );
        explode( Vector3Add( s.second, { 0.0f, 0.3f, 0.0f } ), 0.4f );
    }
    m_summons.clear();
    // Keep apart.
    for ( size_t i = 0; i < m_foes.size(); ++i )
        for ( size_t j = i + 1; j < m_foes.size(); ++j )
        {
            Foe &a = m_foes[i], &b = m_foes[j];
            if ( !a.m_alive || !b.m_alive || a.m_hidden || b.m_hidden ) continue;
            if ( a.m_kind == FoeE::DRONE || b.m_kind == FoeE::DRONE ) continue;
            float   min = fminf( 0.9f, foeRadius( a ) ) + fminf( 0.9f, foeRadius( b ) ) - 0.05f;
            Vector3 d = Vector3Subtract( a.m_pos, b.m_pos );
            d.y = 0.0f;
            float len = Vector3Length( d );
            if ( len < min && len > 1e-4f )
            {
                Vector3 push = Vector3Scale( d, ( min - len ) * 0.5f / len );
                a.m_pos = Vector3Add( a.m_pos, push );
                b.m_pos = Vector3Subtract( b.m_pos, push );
            }
        }
    // Feet.
    for ( Foe &f : m_foes )
    {
        if ( !f.m_alive || human( f ) || f.m_kind == FoeE::DRONE ) continue;
        const LouseModel &model =
            f.m_kind == FoeE::WELDER
                ? m_welder
                : ( f.m_kind == FoeE::SAW ? m_saw : ( f.m_kind == FoeE::BOMBER ? m_bomber : m_louse ) );
        LousePose pose;
        pose.m_pos = f.m_pos;
        pose.m_yaw = f.m_yaw;
        pose.m_air = !f.m_ground;
        pose.m_crouch = f.m_crouch;
        model.stepLegs( f.m_legs, pose, f.m_vel, f.m_ground ? f.m_pos.y : groundAt( f.m_pos ), dt );
    }
}

void Game::updatePress( float dt )
{
    Vector3 o = m_pressModel.m_origin;
    if ( o.x == 0.0f ) return;
    m_pressPose.m_time = m_time;
    for ( int i = 0; i < 2; ++i )
    {
        m_pressPose.m_tankFlash[i] = fmaxf( 0.0f, m_pressPose.m_tankFlash[i] - dt * 6.0f );
        m_pressPose.m_tank[i] = m_tank[i] > 0.0f;
        m_pressPose.m_hatch[i] = fmaxf( 0.0f, m_pressPose.m_hatch[i] - dt * 0.8f );
    }
    m_pressPose.m_coreFlash = fmaxf( 0.0f, m_pressPose.m_coreFlash - dt * 6.0f );
    if ( m_pressDead )
    {
        m_pressPose.m_dead = true;
        m_pressPose.m_ram = fmaxf( 1.05f, m_pressPose.m_ram - dt * 3.0f );
        m_pressPose.m_warn = 0.0f;
        return;
    }
    if ( !m_pressActive )
    {
        // It wakes when Corvin steps into the arena (south of the housing, within its walls).
        if ( m_pos.z > o.z - 2.0f && m_pos.z < o.z + 30.0f && fabsf( m_pos.x - o.x ) < 22.0f )
        {
            m_pressActive = true;
            m_audio.play( SfxE::KLAXON, o, m_camTarget, 1.0f );
            fire( "bossstart" );
        }
        return;
    }
    if ( m_armourHint > 6.0f && ( m_tank[0] > 0.0f || m_tank[1] > 0.0f ) ) fire( "armour" );
    if ( m_pressDying )
    {
        m_deathTimer += dt;
        if ( rnd() < dt * 7.0f )
            explode( { o.x + ( rnd() - 0.5f ) * 14.0f, 1.0f + rnd() * 5.0f, o.z + rnd() * 1.0f }, 0.8f + rnd() );
        m_pressPose.m_ram = fmaxf( 1.05f, m_pressPose.m_ram - dt * 4.0f );
        m_shake = fmaxf( m_shake, 0.3f );
        if ( m_deathTimer > 3.0f )
        {
            m_pressDead = true;
            explode( { o.x, 3.4f, o.z }, 2.5f );
            m_shake = 1.0f;
            fire( "bossdead" );
            // A boss: much experience, and its spoils thrown out of the wreck.
            gainXp( 250 );
            for ( int i = 0; i < 3; ++i ) dropItem( { o.x + ( i - 1 ) * 1.5f, 2.0f, o.z + 4.0f }, 4.0f );
            for ( Foe &f : m_foes )
                if ( f.m_alive && Vector3Distance( f.m_pos, o ) < 30.0f ) killFoe( f, false );
        }
        return;
    }
    bool open = m_tank[0] <= 0.0f && m_tank[1] <= 0.0f;
    m_pressPose.m_shutter = open ? fminf( 1.0f, m_pressPose.m_shutter + dt * 0.7f ) : 0.0f;
    if ( m_pressPose.m_shutter > 0.9f ) fire( "coreopen" );
    if ( m_core <= 0.0f )
    {
        m_core = 0.0f;
        m_pressDying = true;
        m_deathTimer = 0.0f;
        return;
    }

    // The ram's cycle: rest, rise with the klaxon, hold, slam.
    m_pressTimer -= dt;
    float rest = open ? 1.0f : 1.8f;
    switch ( m_pressState )
    {
    case 0:
        m_pressPose.m_ram = fmaxf( 1.05f, m_pressPose.m_ram - dt * 2.0f );
        m_pressPose.m_warn = fmaxf( 0.0f, m_pressPose.m_warn - dt * 2.0f );
        if ( m_pressTimer <= 0.0f )
        {
            m_pressState = 1;
            m_pressTimer = 1.4f;
            m_audio.play( SfxE::KLAXON, o, m_camTarget, 0.8f );
            if ( fabsf( m_pos.x - o.x ) < 6.0f && m_pos.z < o.z + 9.0f ) fire( "ramwarn" );
        }
        break;
    case 1:
        m_pressPose.m_ram = fminf( 5.0f, m_pressPose.m_ram + dt * 3.0f );
        m_pressPose.m_warn = 0.5f + 0.5f * sinf( m_time * 14.0f );
        if ( m_pressTimer <= 0.0f )
        {
            m_pressState = 2;
            m_pressTimer = 0.7f;
        }
        break;
    case 2:
        m_pressPose.m_warn = 1.0f;
        m_pressPose.m_ram = 5.0f + 0.05f * sinf( m_time * 40.0f );
        if ( m_pressTimer <= 0.0f )
        {
            m_pressState = 3;
            m_pressTimer = 0.12f;
        }
        break;
    case 3:
        m_pressPose.m_ram = fmaxf( 1.05f, m_pressPose.m_ram - dt * 40.0f );
        if ( m_pressTimer <= 0.0f )
        {
            m_pressPose.m_ram = 1.05f;
            m_pressState = 0;
            m_pressTimer = rest;
            m_shake = 0.7f;
            m_audio.play( SfxE::SLAM, { o.x, 0.0f, o.z + 2.2f }, m_camTarget, 1.0f );
            for ( int i = 0; i < 2; ++i )
                sparks( { o.x + ( i ? 2.0f : -2.0f ), 0.3f, o.z + 2.2f }, { i ? 1.0f : -1.0f, 0.3f, 0.0f }, 30,
                        { 255, 190, 110, 255 }, 6.0f );
            m_flashes.push_back( { { o.x, 0.6f, o.z + 2.5f }, { 30.0f, 18.0f, 8.0f }, 9.0f, 0.2f, 0.2f } );
            // Whatever stands on the anvil.
            auto onAnvil = [&]( Vector3 p )
            { return fabsf( p.x - o.x ) < 2.4f && p.z > o.z && p.z < o.z + 4.4f && p.y < 2.2f; };
            if ( onAnvil( m_pos ) ) hurtHero( 35.0f, { ( m_pos.x - o.x ) * 1.5f, 4.0f, 5.0f }, m_pos );
            for ( Foe &f : m_foes )
                if ( f.m_alive && onAnvil( f.m_pos ) ) killFoe( f, false );
        }
        break;
    default: break;
    }

    // New lice from the hatches.
    m_spawnTimer -= dt;
    int alive = 0;
    for ( const Foe &f : m_foes )
        if ( f.m_alive && Vector3Distance( f.m_pos, o ) < 32.0f ) ++alive;
    if ( m_spawnHatch < 0 && m_spawnTimer <= 0.8f && alive < 5 ) m_spawnHatch = (int)( rnd() * 2.0f ) & 1;
    if ( m_spawnHatch >= 0 ) m_pressPose.m_hatch[m_spawnHatch] = 1.0f;
    if ( m_spawnTimer <= 0.0f )
    {
        if ( m_spawnHatch >= 0 && alive < 5 )
        {
            ++m_spawns;
            FoeE kind = ( open && m_spawns % 3 == 0 ) ? FoeE::WELDER : FoeE::LOUSE;
            spawnFoe( kind, { o.x + ( m_spawnHatch ? 4.6f : -4.6f ), 0.0f, o.z + 1.3f }, true );
            m_foes.back().m_yaw = 0.0f;
            m_audio.play( SfxE::CHITTER, m_foes.back().m_pos, m_camTarget, 0.8f );
        }
        m_spawnHatch = -1;
        m_spawnTimer = open ? 5.0f : 6.5f;
    }
}

void Game::updateEffects( float dt )
{
    float g = m_level.m_gravity;
    for ( size_t i = 0; i < m_particles.size(); )
    {
        Particle &p = m_particles[i];
        p.m_life -= dt;
        if ( p.m_kind == 0 )
        {
            p.m_vel.y -= g * 1.5f * dt;
            p.m_pos = Vector3Add( p.m_pos, Vector3Scale( p.m_vel, dt ) );
            float floor = groundAt( p.m_pos );
            if ( p.m_pos.y < floor && floor > -100.0f )
            {
                p.m_pos.y = floor;
                p.m_vel = { p.m_vel.x * 0.5f, -p.m_vel.y * 0.35f, p.m_vel.z * 0.5f };
            }
        }
        else if ( p.m_kind == 1 )
        {
            p.m_pos = Vector3Add( p.m_pos, Vector3Scale( p.m_vel, dt ) );
            p.m_vel = Vector3Scale( p.m_vel, 1.0f - dt * 0.6f );
            p.m_size += dt * 0.5f;
        }
        else if ( p.m_kind == 3 )
            p.m_pos = Vector3Add( p.m_pos, Vector3Scale( p.m_vel, dt ) );
        if ( p.m_life <= 0.0f )
        {
            m_particles[i] = m_particles.back();
            m_particles.pop_back();
        }
        else
            ++i;
    }
    for ( size_t i = 0; i < m_debris.size(); )
    {
        Debris &d = m_debris[i];
        d.m_life -= dt;
        d.m_vel = Vector3Add( d.m_vel, Vector3Scale( thrust(), dt ) );
        d.m_vel.y -= g * dt;
        d.m_pos = Vector3Add( d.m_pos, Vector3Scale( d.m_vel, dt ) );
        d.m_rot = Vector3Add( d.m_rot, Vector3Scale( d.m_spin, dt ) );
        float floor = groundAt( d.m_pos );
        if ( d.m_pos.y < floor + 0.04f && floor > -100.0f )
        {
            d.m_pos.y = floor + 0.04f;
            d.m_vel = { d.m_vel.x * 0.6f, -d.m_vel.y * 0.3f, d.m_vel.z * 0.6f };
            d.m_spin = Vector3Scale( d.m_spin, 0.6f );
        }
        if ( d.m_life <= 0.0f || d.m_pos.y < -20.0f )
        {
            m_debris[i] = m_debris.back();
            m_debris.pop_back();
        }
        else
            ++i;
    }
    for ( size_t i = 0; i < m_blobs.size(); )
    {
        Blob   &b = m_blobs[i];
        Vector3 prev = b.m_pos;
        b.m_vel.y -= g * dt;
        b.m_pos = Vector3Add( b.m_pos, Vector3Scale( b.m_vel, dt ) );
        b.m_life -= dt;
        Vector3 hit, n;
        bool    done = b.m_life <= 0.0f;
        if ( !done && Vector3Distance( b.m_pos, Vector3Add( m_pos, { 0, 1.0f, 0 } ) ) < 0.75f && !m_dead )
        {
            hurtHero( 14.0f, { b.m_vel.x * 0.15f, 0.5f, b.m_vel.z * 0.15f }, Vector3Subtract( b.m_pos, b.m_vel ) );
            done = true;
        }
        if ( !done && m_level.raycast( prev, b.m_pos, hit, n ) )
        {
            sparks( hit, n, 14, { 255, 140, 50, 255 }, 2.0f );
            m_flashes.push_back(
                { Vector3Add( hit, Vector3Scale( n, 0.3f ) ), { 14.0f, 6.0f, 1.5f }, 4.0f, 0.4f, 0.4f } );
            m_audio.play( SfxE::SIZZLE, hit, m_camTarget, 0.5f );
            done = true;
        }
        if ( rnd() < dt * 30.0f )
            m_particles.push_back( { b.m_pos,
                                     { ( rnd() - 0.5f ) * 0.5f, 0.3f, ( rnd() - 0.5f ) * 0.5f },
                                     0.4f,
                                     0.4f,
                                     0.03f,
                                     { 255, 150, 60, 255 },
                                     3 } );
        if ( done )
        {
            m_blobs[i] = m_blobs.back();
            m_blobs.pop_back();
        }
        else
            ++i;
    }
    for ( size_t i = 0; i < m_tracers.size(); )
    {
        m_tracers[i].m_life -= dt;
        if ( m_tracers[i].m_life <= 0.0f )
        {
            m_tracers[i] = m_tracers.back();
            m_tracers.pop_back();
        }
        else
            ++i;
    }
    for ( size_t i = 0; i < m_flashes.size(); )
    {
        m_flashes[i].m_life -= dt;
        if ( m_flashes[i].m_life <= 0.0f )
        {
            m_flashes[i] = m_flashes.back();
            m_flashes.pop_back();
        }
        else
            ++i;
    }
    for ( Pickup &p : m_pickups )
    {
        if ( !p.m_alive ) continue;
        p.m_t += dt;
        float floor = groundAt( p.m_pos );
        if ( m_thrustAmt > 0.5f )
        {
            // A dodge rolls the pickups across the deck.
            Vector3 q = Vector3Add( p.m_pos, Vector3Scale( thrust(), dt * 0.25f ) );
            m_level.collide( q, 0.2f, 0.3f );
            p.m_pos.x = q.x, p.m_pos.z = q.z;
        }
        if ( p.m_pos.y > floor + 0.3f || p.m_vel.y > 0.0f )
        {
            p.m_vel.y -= g * dt;
            p.m_pos = Vector3Add( p.m_pos, Vector3Scale( p.m_vel, dt ) );
        }
        if ( floor > -100.0f && p.m_pos.y < floor + 0.3f )
        {
            p.m_pos.y = floor + 0.3f;
            p.m_vel = { 0, 0, 0 };
        }
        if ( p.m_pos.y < -20.0f ) p.m_alive = false;
    }
    // Steam from the vents.
    for ( Vent &v : m_vents )
    {
        v.m_timer -= dt;
        if ( v.m_timer <= 0.0f )
        {
            v.m_burst = 1.6f;
            v.m_timer = 4.0f + rnd() * 4.0f;
            m_audio.play( SfxE::BURST, v.m_pos, m_camTarget, 0.35f );
        }
        if ( v.m_burst > 0.0f )
        {
            v.m_burst -= dt;
            // Few, large and faint: many puffs on top of each other add up to a white blot.
            if ( rnd() < dt * 12.0f )
                m_particles.push_back(
                    { Vector3Add( v.m_pos, { ( rnd() - 0.5f ) * 0.6f, 0.1f, ( rnd() - 0.5f ) * 0.6f } ),
                      { ( rnd() - 0.5f ) * 0.6f, 1.4f + rnd() * 1.2f, ( rnd() - 0.5f ) * 0.6f },
                      2.2f,
                      2.2f,
                      0.4f,
                      { 200, 205, 215, 5 },
                      1 } );
        }
    }
    m_shake = fmaxf( 0.0f, m_shake - dt * 1.8f );
}

void Game::plan()
{
    // Breadth first over walkable cells; across a shaft, a straight jump of up to three cells.
    int rows = m_level.rows(), cols = m_level.cols();
    int sr, sc;
    m_level.cellOf( m_pos, sr, sc );
    int     goal = -1;
    Vector3 o = m_pressModel.m_origin;
    if ( m_waypoint < m_level.m_route.size() )
    {
        // The level's route, one waypoint after the other.
        goal = m_level.m_route[m_waypoint].first * cols + m_level.m_route[m_waypoint].second;
    }
    else if ( m_pressActive && !m_pressDead )
    {
        int gr, gc;
        m_level.cellOf( { o.x, 0.0f, o.z + 13.0f }, gr, gc );
        goal = gr * cols + gc;
    }
    else if ( boss() >= 0 )
    {
        // Corvin fights the boss from where he stands once it is in sight and near; until then he goes to it.
        Vector3 b = m_foes[boss()].m_pos;
        if ( m_heroKind != HeroE::DORRA && Vector3Distance( b, m_pos ) < 13.0f &&
             los( Vector3Add( m_pos, { 0, 1.3f, 0 } ), Vector3Add( b, { 0, 1.3f, 0 } ) ) )
        {
            m_path.clear();
            return;
        }
        int gr, gc;
        m_level.cellOf( m_foes[boss()].m_pos, gr, gc );
        goal = gr * cols + gc;
    }
    else if ( m_liftMax.x > m_liftMin.x )
    {
        // The cell before the lift; the last steps onto its platform go straight.
        int gr, gc;
        m_level.cellOf( { m_lift.x, 0.0f, m_liftMin.z - 1.0f }, gr, gc );
        goal = gr * cols + gc;
    }
    // Cells with a crate or a stall in them: round them if there is a way, through them if not.
    if ( m_propCells.empty() )
    {
        m_propCells.assign( (size_t)rows * cols, 0 );
        for ( const Prop &p : m_level.m_props )
        {
            int pr, pc;
            m_level.cellOf( p.m_pos, pr, pc );
            if ( pr >= 0 && pc >= 0 && pr < rows && pc < cols && p.m_radius > 0.3f )
                m_propCells[(size_t)pr * cols + pc] = 1;
        }
    }
    std::vector<int> prev;
    std::deque<int>  q;
    if ( !m_level.walkable( sr, sc ) ) return;
    for ( int pass = 0; pass < 2; ++pass )
    {
        prev.assign( (size_t)rows * cols, -2 );
        q.clear();
        prev[(size_t)sr * cols + sc] = -1;
        q.push_back( sr * cols + sc );
        const int DR[4] = { -1, 0, 1, 0 }, DC[4] = { 0, 1, 0, -1 };
        // How many cells of shaft a running jump clears in this gravity (three at most).
        float jump = m_heroKind == HeroE::DORRA ? DorraJump : JumpSpeed;
        int   maxGap = std::clamp( (int)( ( runSpeed() * 2.0f * jump / m_level.m_gravity - 0.8f ) / CellSize ), 0, 3 );
        // A shut door is a way through (the autopilot opens it), unless it is locked.
        auto passable = [&]( int r, int c )
        {
            if ( m_level.walkable( r, c ) ) return true;
            if ( m_level.cell( r, c ).m_kind != CellE::DOOR ) return false;
            auto lock = m_level.m_locks.find( r * 1000 + c );
            return lock == m_level.m_locks.end() || m_fired.count( lock->second ) > 0;
        };
        while ( !q.empty() )
        {
            int cur = q.front();
            q.pop_front();
            if ( cur == goal ) break;
            int   cr = cur / cols, cc = cur % cols;
            float h = m_level.centre( cr, cc ).y;
            for ( int d = 0; d < 4; ++d )
            {
                int nr = cr + DR[d], nc = cc + DC[d];
                int n = -1;
                if ( passable( nr, nc ) && !m_blocked.count( nr * cols + nc ) &&
                     m_level.centre( nr, nc ).y - h <= StepUp && ( pass == 1 || !m_propCells[(size_t)nr * cols + nc] ) )
                    n = nr * cols + nc;
                else if ( m_level.cell( nr, nc ).m_kind == CellE::VOID )
                    for ( int k = 2; k <= 1 + maxGap; ++k )
                    {
                        int jr = cr + DR[d] * k, jc = cc + DC[d] * k;
                        if ( m_level.cell( jr, jc ).m_kind == CellE::VOID ) continue;
                        if ( m_level.walkable( jr, jc ) && m_level.centre( jr, jc ).y - h <= 0.6f ) n = jr * cols + jc;
                        break;
                    }
                if ( n < 0 || prev[n] != -2 ) continue;
                prev[n] = cur;
                q.push_back( n );
            }
        }
        if ( goal >= 0 && prev[goal] != -2 ) break;
    }
    m_path.clear();
    if ( goal < 0 || prev[goal] == -2 ) return;
    for ( int c = goal; c >= 0; c = prev[c] ) m_path.push_back( c );
    std::reverse( m_path.begin(), m_path.end() );
    if ( !m_path.empty() ) m_path.erase( m_path.begin() );
}

void Game::autopilot( float dt, GameInput &in )
{
    in = GameInput();
    if ( m_dead || m_finished || m_scene > 0 ) return;
    // In a hub: read every conversation through (the first answer each time), close the menus, talk to whom it names.
    if ( m_talk )
    {
        in.m_jump = true;
        return;
    }
    if ( m_menu )
    {
        if ( m_menu == 3 )
            in.m_jump = true;   // the lift: its first way
        else
            in.m_special = true;
        return;
    }
    if ( !m_level.m_autotalk.empty() && !m_fired.count( "talked." + m_level.m_autotalk ) )
        for ( const NpcState &n : m_npcs )
        {
            if ( m_level.m_npcs[n.m_def].m_id != m_level.m_autotalk ) continue;
            // Within sight: straight to them, then talk.
            Vector2 to = { n.m_pos.x - m_pos.x, n.m_pos.z - m_pos.z };
            float   dist = Vector2Length( to );
            if ( dist > 10.0f || !los( Vector3Add( m_pos, { 0, 1.5f, 0 } ), Vector3Add( n.m_pos, { 0, 1.5f, 0 } ) ) )
                break;
            if ( dist > 1.8f )
                in.m_move = Vector2Scale( to, 1.0f / dist );
            else
                in.m_jump = true;
            return;
        }
    int cols = m_level.cols();
    if ( m_waypoint < m_level.m_route.size() )
    {
        // A waypoint on the way counts from near by; the last one only when standing in it.
        auto  wp = m_level.m_route[m_waypoint];
        float near = m_waypoint + 1 < m_level.m_route.size() ? 1.6f : 0.6f;
        if ( Vector3Distance( m_pos, m_level.centre( wp.first, wp.second ) ) < near ) ++m_waypoint, m_planTimer = 0.0f;
    }
    m_planTimer -= dt;
    if ( m_planTimer <= 0.0f )
    {
        plan();
        m_planTimer = 0.5f;
    }
    int r, c;
    m_level.cellOf( m_pos, r, c );
    while ( m_ground && !m_path.empty() && m_path.front() == r * cols + c ) m_path.erase( m_path.begin() );

    // Fight: aim at the nearest thing worth shooting, fire from the ground.
    Vector3 chest = Vector3Add( m_pos, { 0, 1.25f, 0 } );
    Target  best;
    float   bestDist = 1e9f;
    for ( int i = 0; i < (int)m_foes.size(); ++i )
    {
        const Foe &f = m_foes[i];
        if ( !f.m_alive || !f.m_awake || f.m_hidden ) continue;
        float d = Vector3Distance( f.m_pos, m_pos );
        if ( d < bestDist && d < 18.0f && los( chest, targetPoint( { 0, i, {} } ) ) ) bestDist = d, best = { 0, i, {} };
    }
    // At the press the weak spots come first, unless a louse is close (for the hammer: in reach).
    float close = m_heroKind == HeroE::DORRA ? 2.5f : 6.0f;
    if ( ( best.m_kind < 0 || bestDist > close ) && m_pressActive && !m_pressDead && !m_pressDying )
        for ( int k = 1; k <= 3; ++k )
        {
            if ( k == 1 ) bestDist = 1e9f;
            if ( ( k == 1 && m_tank[0] <= 0.0f ) || ( k == 2 && m_tank[1] <= 0.0f ) ||
                 ( k == 3 && ( m_pressPose.m_shutter < 0.9f || m_core <= 0.0f ) ) )
                continue;
            Target t = { k, 0, {} };
            float  d = Vector3Distance( targetPoint( t ), m_pos );
            if ( d < bestDist && los( chest, targetPoint( t ) ) ) bestDist = d, best = t;
        }
    if ( m_hp < hpMax() * 0.4f && m_stims > 0 ) in.m_stim = true;
    autoSkills( in, best, bestDist );
    if ( m_heroKind != HeroE::DORRA && best.m_kind >= 0 )
    {
        Vector3 p = targetPoint( best );
        in.m_stick = Vector2Normalize( { p.x - m_pos.x, p.z - m_pos.z } );
        in.m_fire = m_ground && ( !m_overheat ) && m_heat < 92.0f;
    }

    // Move along the path; keep off the anvil while the ram is up; back off from a louse in the face.
    Vector3 o = m_pressModel.m_origin;
    bool    anvilDanger =
        m_pressActive && !m_pressDead && m_pressState >= 1 && fabsf( m_pos.x - o.x ) < 3.6f && m_pos.z < o.z + 6.0f;
    if ( anvilDanger )
    {
        in.m_move = { m_pos.x < o.x ? -1.0f : 1.0f, 1.0f };
        return;
    }
    // Stuck while wanting to go somewhere: half a second in some direction, then plan again.
    auto unstick = [&]()
    {
        m_stuckTimer += dt;
        if ( m_stuckTimer > 2.5f )
        {
            if ( Vector3Distance( m_pos, m_lastPos ) < 0.4f && Vector2Length( in.m_move ) > 0.1f )
                m_unstick = 0.6f, m_unstickDir = Vector2Normalize( { rnd() - 0.5f, rnd() - 0.5f } ), m_planTimer = 0.0f;
            m_lastPos = m_pos;
            m_stuckTimer = 0.0f;
        }
        if ( m_unstick > 0.0f )
        {
            m_unstick -= dt;
            in.m_move = m_unstickDir;
        }
    };
    if ( m_heroKind == HeroE::DORRA && autopilotMelee( in, best, bestDist ) )
    {
        unstick();
        return;
    }
    if ( best.m_kind == 0 && bestDist < 2.5f && m_ground && m_heroKind != HeroE::DORRA )
    {
        // Back off, but never towards a shaft.
        Vector3 f = m_foes[best.m_index].m_pos;
        Vector2 away = Vector2Normalize( { m_pos.x - f.x, m_pos.z - f.z } );
        if ( groundAt( { m_pos.x + away.x * 1.2f, 0.0f, m_pos.z + away.y * 1.2f } ) > -100.0f )
        {
            in.m_move = away;
            return;
        }
    }
    if ( m_path.empty() )
    {
        if ( liftOpen() && ( m_doorHalf <= 0.0f || m_doorOpen > 0.9f ) && Vector3Distance( m_pos, m_lift ) < 6.0f )
            in.m_move = Vector2Normalize( { m_lift.x - m_pos.x, m_lift.z - m_pos.z } );
        if ( m_waypoint < m_level.m_route.size() )
        {
            // In the waypoint's cell already: the last steps to its middle.
            Vector3 wp = m_level.centre( m_level.m_route[m_waypoint].first, m_level.m_route[m_waypoint].second );
            Vector2 to = { wp.x - m_pos.x, wp.z - m_pos.z };
            if ( Vector2Length( to ) > 0.2f ) in.m_move = Vector2Normalize( to );
        }
        int b = boss();
        if ( b >= 0 && m_waypoint >= m_level.m_route.size() )
        {
            // Round the boss at eight to twelve metres, the way changing now and then; never towards a shaft.
            Vector3 f = m_foes[b].m_pos;
            Vector2 to = { f.x - m_pos.x, f.z - m_pos.z };
            float   d = Vector2Length( to );
            if ( d > 1e-3f ) to = Vector2Scale( to, 1.0f / d );
            float   side = fmodf( m_time, 8.0f ) < 4.0f ? 1.0f : -1.0f;
            Vector2 move = { -to.y * side, to.x * side };
            if ( d > 12.0f ) move = Vector2Add( move, to );
            if ( d < 8.0f ) move = Vector2Subtract( move, to );
            move = Vector2Normalize( move );
            if ( groundAt( { m_pos.x + move.x * 1.5f, 0.0f, m_pos.z + move.y * 1.5f } ) > -100.0f ) in.m_move = move;
        }
        return;
    }
    int     next = m_path.front();
    Vector3 to = m_level.centre( next / cols, next % cols );
    Vector2 d = { to.x - m_pos.x, to.z - m_pos.z };
    float   len = Vector2Length( d );
    if ( len > 1e-3f ) in.m_move = Vector2Scale( d, 1.0f / len );
    // A shut door on the way: open it.
    const Cell &door = m_level.cell( next / cols, next % cols );
    if ( door.m_kind == CellE::DOOR && !door.m_open ) in.m_use = true;
    // A jump edge: take a run-up, jump at the brink, and do not shoot in the air.
    int nr = next / cols, nc = next % cols;
    if ( abs( nr - r ) + abs( nc - c ) > 1 && m_ground )
    {
        in.m_fire = false;
        Vector3 ahead = Vector3Add( m_pos, { in.m_move.x * 0.6f, 0.0f, in.m_move.y * 0.6f } );
        float   speed = sqrtf( m_vel.x * m_vel.x + m_vel.z * m_vel.z );
        if ( m_runup > 0.0f )
        {
            m_runup -= dt;
            in.m_move = Vector2Negate( in.m_move );
        }
        else if ( groundAt( ahead ) < -100.0f )
        {
            if ( speed > 3.3f )
                in.m_jump = true;
            else
                m_runup = 0.45f;
        }
    }
    if ( !m_ground ) in.m_fire = false;
    unstick();
}

void Game::update( float dt, const GameInput &inRaw )
{
    GameInput in = inRaw;
    m_rawMove = inRaw.m_move;
    // A level that opens with a card: nothing moves under it until it has been read (A, Enter or a click ends it).
    if ( m_intro > 0.0f )
    {
        if ( m_autoplay ) m_intro = 0.0f;
        m_intro -= dt;
        if ( ( in.m_confirm || in.m_jump || in.m_fire ) && m_intro > 1.0f && m_intro < IntroTime - 1.5f )
            m_intro = 1.0f;
        m_titleTimer = 0.0f;
        in = GameInput();
        dt = 0.0f;
    }
    if ( m_finished ) m_endTimer += dt;
    m_time += dt;
    m_titleTimer = fmaxf( 0.0f, m_titleTimer - dt );
    // The camera turns about Corvin; the sticks and keys move him relative to it (the autopilot thinks in the world).
    m_camYaw += in.m_cameraTurn;
    if ( in.m_cameraReset ) m_camYaw = 0.0f;
    m_audio.m_yaw = m_camYaw;
    if ( m_autoplay )
        autopilot( dt, in );
    else
    {
        float   c = cosf( m_camYaw ), s = sinf( m_camYaw );
        Vector2 m = in.m_move;
        in.m_move = { m.x * c + m.y * s, -m.x * s + m.y * c };
    }
    if ( inRaw.m_map ) m_showMap = !m_showMap;
    updateMap( dt );
    updateScene( dt, in );
    if ( updateUse( dt, in ) ) in = GameInput();
    if ( updateHub( dt, in ) ) in = GameInput();
    updateThrust( dt );
    useSkills( dt, in );
    updateHero( dt, in );
    updateFoes( dt );
    updatePress( dt );
    updateEffects( dt );
    updatePieces( dt );
    updateLoot( dt );
    updateShots( dt );
    updateCargo( dt );
    updateIlka( dt );
    updateSkins( dt );

    // The door opens with the level's event (the press dead, the bridge clear).
    if ( m_doorHalf > 0.0f && m_fired.count( m_level.m_gate ) ) m_doorOpen = fminf( 1.0f, m_doorOpen + dt * 0.5f );
    // The bridge is clear once all who held it are down; then the others come aboard, by the containers.
    if ( m_fired.count( "f" ) && !m_fired.count( "bridgeclear" ) )
    {
        bool held = false, any = false;
        for ( const Foe &f : m_foes )
            if ( f.m_group == 1 ) any = true, held = held || f.m_alive;
        if ( any && !held ) fire( "bridgeclear" );
    }
    if ( m_fired.count( "bridgeclear" ) )
        for ( Foe &f : m_foes )
            if ( f.m_hidden ) f.m_hidden = false, f.m_awake = true;
    // What a level says at its start; on a ship the alarm goes after it, or as soon as the hero leaves the start.
    if ( m_time > 0.8f ) fire( !m_level.m_back.empty() && m_fired.count( m_level.m_back ) ? "back" : "start" );
    if ( m_level.m_thrust && !m_fired.count( "alarm" ) && ( m_time > 13.0f || m_fired.count( "b" ) ) ) fire( "alarm" );

    // Lines and hints, one at a time.
    m_lineTimer -= dt;
    if ( m_lineTimer <= 0.0f )
    {
        m_line = nullptr;
        if ( !m_lineQueue.empty() )
        {
            m_line = m_lineQueue.front();
            m_lineQueue.erase( m_lineQueue.begin() );
            // The voice in the chosen language, else the German one (under the chosen language's subtitles); a line
            // the hero says in their own way has its own file.
            std::string key = m_line->m_id;
            if ( hasText( key + "." + heroKey( m_heroKind ) ) ) key += std::string( "." ) + heroKey( m_heroKind );
            float spoken = m_audio.voice( m_assets + "/voice/" + language() + "/" + key );
            if ( spoken <= 0.0f && language() != "de" ) spoken = m_audio.voice( m_assets + "/voice/de/" + key );
            int chars = GetCodepointCount( tr( key ) );
            m_lineTimer = spoken > 0.0f ? spoken + 0.4f : 1.5f + 0.055f * (float)chars;
        }
    }
    m_hintTimer -= dt;
    if ( m_hintTimer <= 0.0f )
    {
        m_hint = nullptr;
        if ( !m_hintQueue.empty() )
        {
            m_hint = m_hintQueue.front();
            m_hintQueue.erase( m_hintQueue.begin() );
            m_hintTimer = 6.5f;
        }
    }

    // The camera follows a little ahead of where Corvin aims.
    // Ahead of where Corvin aims and runs. In the press's arena (only while he is inside it) it draws back and takes
    // the press into the picture -- but never so far that he would leave it.
    Vector3 hero = Vector3Add( m_pos, { 0.0f, 1.0f, 0.0f } );
    Vector3 lead = { Clamp( m_vel.x * 0.35f, -1.5f, 1.5f ), 0.0f, Clamp( m_vel.z * 0.35f, -1.5f, 1.5f ) };
    Vector3 want = Vector3Add( Vector3Add( hero, Vector3Scale( m_aimDir, 1.2f ) ), lead );
    Vector3 o = m_pressModel.m_origin;
    bool    arena = o.x != 0.0f && m_pressActive && !m_pressDead && fabsf( m_pos.x - o.x ) < 21.0f && m_pos.z > o.z &&
                    m_pos.z < o.z + 30.0f;
    if ( arena ) want = Vector3Lerp( want, { o.x, 1.0f, o.z + 4.0f }, 0.5f );
    if ( m_scene == 1 || m_scene == 2 ) want = Vector3Lerp( hero, { m_scanner.x, 0.6f, m_scanner.z }, 0.5f );
    Vector3 off = Vector3Subtract( want, hero );
    float   reach = arena ? 7.0f : 3.0f;
    if ( Vector3Length( off ) > reach ) want = Vector3Add( hero, Vector3Scale( Vector3Normalize( off ), reach ) );
    m_camTarget = Vector3Lerp( m_camTarget, want, 1.0f - expf( -dt * 4.0f ) );
    // However the target lags, Corvin stays within the middle of the picture.
    Vector3 lag = Vector3Subtract( m_camTarget, hero );
    float   keep = arena ? 8.0f : 4.0f;
    if ( Vector3Length( lag ) > keep ) m_camTarget = Vector3Add( hero, Vector3Scale( Vector3Normalize( lag ), keep ) );
    float pull = arena ? 1.45f : ( m_scene == 1 || m_scene == 2 ? 0.7f : 1.0f );
    m_camPull += ( pull - m_camPull ) * fminf( 1.0f, dt * 1.5f );
    Vector3 shake = { ( rnd() - 0.5f ) * m_shake * 0.5f, ( rnd() - 0.5f ) * m_shake * 0.3f,
                      ( rnd() - 0.5f ) * m_shake * 0.5f };
    m_cam.target = Vector3Add( m_camTarget, shake );
    m_cam.position = Vector3Add(
        m_cam.target, Vector3Scale( { 8.1f * sinf( m_camYaw ), 11.5f, 8.1f * cosf( m_camYaw ) }, m_zoom * m_camPull ) );
    // A dodge tilts the picture towards where it pushes.
    Vector3 look = Vector3Normalize( Vector3Subtract( m_cam.target, m_cam.position ) );
    Vector3 side = Vector3Normalize( Vector3CrossProduct( look, { 0.0f, 1.0f, 0.0f } ) );
    float   lean = Vector3DotProduct( thrust(), side ) / ThrustPeak * 0.06f;
    m_roll += ( lean - m_roll ) * fminf( 1.0f, dt * 3.0f );
    m_cam.up = Vector3Add( Vector3Scale( { 0.0f, 1.0f, 0.0f }, cosf( m_roll ) ), Vector3Scale( side, sinf( m_roll ) ) );
    m_cam.fovy = 38.0f;
    m_cam.projection = CAMERA_PERSPECTIVE;
    m_audio.update();
}

Lighting Game::lighting() const
{
    Lighting L;
    L.m_sunDir = Vector3Normalize( { 0.3f, -1.0f, 0.2f } );
    L.m_sunColor = { 0, 0, 0 };
    L.m_spotPos = m_hero.lampPos();
    L.m_spotDir = m_hero.lampDir();
    // The helmet lamp; turned low where the place has its own light (it would burn out faces at arm's length).
    L.m_spotColor =
        m_dead ? Vector3{ 0, 0, 0 } : Vector3Scale( { 48.0f, 43.0f, 35.0f }, m_level.m_peaceful ? 0.2f : 1.0f );
    L.m_spotOuter = 30.0f;
    L.m_spotInner = 14.0f;
    L.m_spotRange = 26.0f;
    L.m_ambUp = m_level.m_ambUp;
    L.m_ambDown = m_level.m_ambDown;
    L.m_fog = m_level.m_fog;
    L.m_fogColor = m_level.m_fogColor;
    L.m_roomMin = { 0.0f, -PitDepth, 0.0f };
    L.m_roomMax = { m_level.cols() * CellSize, m_level.m_wallHeight, m_level.rows() * CellSize };
    if ( !m_level.m_view.empty() ) L.m_roomMin.y = 0.0f;   // no fog below a window in the floor
    if ( m_skyTex.id > 0 )
    {
        // Out there is vacuum. The red dwarf shines up through the window in the floor.
        L.m_roomMin.y = 0.0f;
        L.m_sunDir = Vector3Normalize( Vector3Subtract( m_ownPlume, m_star ) );
        L.m_sunColor = { 2.6f, 0.75f, 0.3f };
    }

    std::vector<PointLight> all;
    all.push_back( { Vector3Add( m_pos, { 0.4f, 3.2f, 1.2f } ), 11.0f, { 13.0f, 11.5f, 10.0f }, 0.0f } );
    // The ship's warning lights while it dodges; the scanner by Ilka, still warm.
    if ( m_thrustState > 0 && fmodf( m_time, 0.5f ) < 0.3f )
        all.push_back( { Vector3Add( m_pos, { 0.0f, 4.5f, 0.0f } ), 12.0f, { 16.0f, 7.0f, 1.0f }, 0.6f } );
    if ( m_ilka >= 0 && m_fired.count( "bridgeclear" ) )
        all.push_back( { Vector3Add( m_scanner, { 0.0f, 0.4f, 0.0f } ), 3.0f, { 6.0f, 0.6f, 0.3f }, 0.5f } );
    for ( const Shot &s : m_shotsInFlight ) all.push_back( { s.m_pos, 3.0f, { 8.0f, 2.0f, 0.6f }, 1.0f } );
    for ( const Bolt &b : m_bolts ) all.push_back( { b.m_pos, 3.0f, { 9.0f, 4.0f, 1.0f }, 1.0f } );
    // The lift: red while the gate is shut, green when it waits.
    if ( m_liftMax.x > m_liftMin.x )
        all.push_back( { { m_lift.x, m_liftY + 3.0f, m_lift.z },
                         9.0f,
                         liftOpen() ? Vector3{ 4.0f, 22.0f, 9.0f } : Vector3{ 14.0f, 2.0f, 1.0f },
                         1.0f } );
    for ( size_t i = 0; i < m_level.m_lights.size(); ++i )
    {
        const LevelLight &l = m_level.m_lights[i];
        float             f = 1.0f;
        if ( l.m_flicker > 0.0f && hash1( (int)( m_time * 11.0f ) + (int)i * 31 ) > 0.72f ) f = 0.12f;
        all.push_back( { l.m_pos, l.m_radius, Vector3Scale( l.m_color, f ), 1.0f } );
    }
    for ( const Capsule &c : m_capsules )
        all.push_back( { Vector3Add( c.m_pos, { 0.0f, 1.6f, 0.0f } ), c.m_active ? 6.0f : 3.0f,
                         c.m_active ? Vector3{ 2.0f, 9.0f, 4.0f } : Vector3{ 0.3f, 1.5f, 0.6f }, 0.6f } );
    for ( const Flash &f : m_flashes )
        all.push_back( { f.m_pos, f.m_radius, Vector3Scale( f.m_color, f.m_life / f.m_max ), 1.0f } );
    for ( const Blob &b : m_blobs ) all.push_back( { b.m_pos, 4.0f, { 12.0f, 5.0f, 1.2f }, 1.0f } );
    for ( const Foe &f : m_foes )
        if ( f.m_alive && f.m_torch > 0.05f )
            all.push_back( { Vector3Add( f.m_pos, { sinf( f.m_yaw ) * 1.4f, 0.8f, cosf( f.m_yaw ) * 1.4f } ), 4.0f,
                             Vector3Scale( { 10.0f, 4.0f, 1.0f }, f.m_torch ), 1.0f } );
    Vector3 o = m_pressModel.m_origin;
    if ( o.x != 0.0f && !m_pressDead )
    {
        if ( m_pressPose.m_warn > 0.05f )
        {
            for ( float side : { -1.0f, 1.0f } )
                all.push_back( { { o.x + side * 7.5f, 7.0f, o.z + 0.4f },
                                 12.0f,
                                 Vector3Scale( { 20.0f, 1.5f, 0.6f }, m_pressPose.m_warn ),
                                 1.0f } );
            // The anvil itself glows red while the ram is up.
            all.push_back(
                { { o.x, 0.8f, o.z + 2.2f }, 6.0f, Vector3Scale( { 18.0f, 1.2f, 0.4f }, m_pressPose.m_warn ), 0.6f } );
        }
        for ( int s = 0; s < 2; ++s )
        {
            if ( m_tank[s] > 0.0f )
                all.push_back(
                    { { o.x + ( s ? 9.2f : -9.2f ), 2.4f, o.z + 2.0f }, 6.0f, { 1.5f, 5.0f, 10.0f }, 0.5f } );
            if ( m_pressPose.m_hatch[s] > 0.05f )
                all.push_back( { { o.x + ( s ? 4.6f : -4.6f ), 0.9f, o.z + 0.8f },
                                 5.0f,
                                 Vector3Scale( { 12.0f, 5.0f, 1.0f }, m_pressPose.m_hatch[s] ),
                                 0.8f } );
        }
        all.push_back( { { o.x, 3.4f, o.z + 0.8f },
                         8.0f,
                         Vector3Scale( { 16.0f, 6.0f, 1.2f }, 0.25f + m_pressPose.m_shutter ),
                         0.8f } );
    }
    Vector3 c = m_camTarget;
    std::sort( all.begin(), all.end(), [&]( const PointLight &a, const PointLight &b )
               { return Vector3DistanceSqr( a.m_pos, c ) < Vector3DistanceSqr( b.m_pos, c ); } );
    if ( all.size() > (size_t)MaxLights ) all.resize( MaxLights );
    L.m_points = all;
    return L;
}

void Game::drawParticles( Gfx &gfx )
{
    Vector3 fwd = Vector3Normalize( Vector3Subtract( m_cam.target, m_cam.position ) );
    Vector3 right = Vector3Normalize( Vector3CrossProduct( fwd, m_cam.up ) );
    Vector3 up = Vector3CrossProduct( right, fwd );
    auto    quad = [&]( Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col )
    {
        rlColor4ub( col.r, col.g, col.b, col.a );
        rlTexCoord2f( 0.0f, 0.0f );
        rlVertex3f( a.x, a.y, a.z );
        rlTexCoord2f( 0.0f, 1.0f );
        rlVertex3f( b.x, b.y, b.z );
        rlTexCoord2f( 1.0f, 1.0f );
        rlVertex3f( c.x, c.y, c.z );
        rlTexCoord2f( 1.0f, 0.0f );
        rlVertex3f( d.x, d.y, d.z );
    };
    auto billboard = [&]( Vector3 p, float s, Color col )
    {
        Vector3 r = Vector3Scale( right, s );
        Vector3 u = Vector3Scale( up, s );
        quad( Vector3Add( Vector3Subtract( p, r ), u ), Vector3Subtract( Vector3Subtract( p, r ), u ),
              Vector3Subtract( Vector3Add( p, r ), u ), Vector3Add( Vector3Add( p, r ), u ), col );
    };
    auto streak = [&]( Vector3 head, Vector3 tail, float w, Color col )
    {
        Vector3 axis = Vector3Subtract( head, tail );
        Vector3 side = Vector3CrossProduct( axis, fwd );
        if ( Vector3Length( side ) < 1e-5f ) side = right;
        side = Vector3Scale( Vector3Normalize( side ), w );
        Vector3 ext = Vector3Scale( Vector3Normalize( axis ), w );
        head = Vector3Add( head, ext );
        tail = Vector3Subtract( tail, ext );
        quad( Vector3Subtract( tail, side ), Vector3Add( tail, side ), Vector3Add( head, side ),
              Vector3Subtract( head, side ), col );
    };

    // Lit: the condensate swarms and the steam.
    gfx.beginParticles( 6.0f, true );
    rlSetTexture( m_dot.id );
    rlBegin( RL_QUADS );
    for ( const Drop &d : m_drops )
    {
        float   a = m_time * d.m_speed;
        Vector3 o = { d.m_off.x * cosf( a ) - d.m_off.z * sinf( a ),
                      d.m_off.y + 0.12f * sinf( m_time * 0.7f + d.m_phase ),
                      d.m_off.x * sinf( a ) + d.m_off.z * cosf( a ) };
        float   twinkle = 0.55f + 0.45f * sinf( m_time * 3.0f + d.m_phase * 7.0f );
        billboard( Vector3Add( d.m_centre, o ), d.m_size, { 220, 235, 255, (unsigned char)( 255 * twinkle ) } );
    }
    for ( const Particle &p : m_particles )
        if ( p.m_kind == 1 )
        {
            Color c = p.m_color;
            c.a = (unsigned char)( c.a * sinf( PI * p.m_life / p.m_max ) );
            billboard( p.m_pos, p.m_size, c );
        }
    rlEnd();
    rlSetTexture( 0 );
    gfx.endParticles();

    // Glowing: sparks, flashes, embers, tracers, molten blobs.
    gfx.beginParticles( 6.0f, false );
    rlSetTexture( m_dot.id );
    rlBegin( RL_QUADS );
    for ( const Particle &p : m_particles )
    {
        float f = p.m_life / p.m_max;
        Color c = p.m_color;
        if ( p.m_kind == 0 )
        {
            c.a = (unsigned char)( 255 * fminf( 1.0f, f * 2.0f ) );
            streak( p.m_pos, Vector3Subtract( p.m_pos, Vector3Scale( p.m_vel, 0.035f ) ), p.m_size, c );
        }
        else if ( p.m_kind == 2 )
        {
            c.a = (unsigned char)( 255 * f );
            billboard( p.m_pos, p.m_size * ( 0.6f + 0.4f * f ), c );
        }
        else if ( p.m_kind == 3 )
        {
            c.a = (unsigned char)( 200 * f );
            billboard( p.m_pos, p.m_size, c );
        }
    }
    for ( const Tracer &t : m_tracers )
    {
        float f = t.m_life / 0.08f;
        streak( t.m_to, t.m_from, 0.025f, { 230, 245, 255, (unsigned char)( 255 * f ) } );
        streak( t.m_to, t.m_from, 0.09f, { 90, 160, 255, (unsigned char)( 120 * f ) } );
    }
    for ( const Blob &b : m_blobs )
    {
        billboard( b.m_pos, 0.12f, { 255, 220, 150, 255 } );
        billboard( b.m_pos, 0.35f, { 255, 120, 40, 120 } );
    }
    // Dorra's blast bolts, the carbines' bolts, and the laser of a carbine taking aim.
    for ( const Bolt &b : m_bolts )
    {
        billboard( b.m_pos, 0.1f, { 255, 230, 170, 255 } );
        billboard( b.m_pos, 0.3f, { 255, 140, 50, 140 } );
        streak( b.m_pos, Vector3Subtract( b.m_pos, Vector3Scale( b.m_vel, 0.05f ) ), 0.04f, { 255, 160, 60, 160 } );
    }
    for ( const Shot &s : m_shotsInFlight )
    {
        Vector3 tail = Vector3Subtract( s.m_pos, Vector3Scale( s.m_vel, 0.045f ) );
        streak( s.m_pos, tail, 0.03f, { 255, 235, 200, 255 } );
        streak( s.m_pos, tail, 0.1f, { 255, 90, 40, 130 } );
    }
    for ( const Foe &f : m_foes )
    {
        if ( !f.m_alive || f.m_hidden || f.m_kind != FoeE::GUNNER || f.m_aim <= 0.02f ) continue;
        Vector3 from = Vector3Add( f.m_pos, { sinf( f.m_yaw ) * 0.6f, 1.3f, cosf( f.m_yaw ) * 0.6f } );
        Vector3 to = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
        Vector3 hit, n;
        if ( m_level.raycast( from, to, hit, n ) ) to = hit;
        unsigned char a = (unsigned char)( 200 * f.m_aim );
        streak( to, from, 0.012f, { 255, 40, 30, a } );
        billboard( to, 0.06f + 0.06f * f.m_aim, { 255, 60, 40, a } );
    }
    rlEnd();
    rlSetTexture( 0 );
    gfx.endParticles();
}

void Game::render( Gfx &gfx, const RenderTexture2D *out )
{
    m_dynamic.clear();
    m_hero.pose( m_dynamic );
    poseCrew( m_dynamic );
    poseShip( m_dynamic );
    poseHub( m_dynamic );
    poseUsables( m_dynamic );
    poseLoot( m_dynamic );
    poseFoes( m_dynamic );
    for ( const Foe &f : m_foes )
    {
        if ( !f.m_alive || human( f ) || f.m_kind == FoeE::DRONE ) continue;
        LousePose pose;
        pose.m_pos = f.m_pos;
        pose.m_yaw = f.m_yaw;
        pose.m_crouch = f.m_crouch;
        pose.m_air = !f.m_ground;
        pose.m_pitch = f.m_ground ? -0.15f * f.m_crouch : Clamp( f.m_vel.y * 0.15f, -0.5f, 0.5f );
        pose.m_flash = f.m_flash;
        pose.m_torch = f.m_torch;
        pose.m_time = m_time + f.m_id;
        pose.m_flash = fmaxf( f.m_flash, f.m_fuse >= 0.0f && fmodf( f.m_fuse * 12.0f, 1.0f ) > 0.5f ? 0.6f : 0.0f );
        ( f.m_kind == FoeE::WELDER
              ? m_welder
              : ( f.m_kind == FoeE::SAW ? m_saw : ( f.m_kind == FoeE::BOMBER ? m_bomber : m_louse ) ) )
            .draw( pose, f.m_legs, m_dynamic );
    }
    if ( m_pressModel.m_origin.x != 0.0f ) m_pressModel.draw( m_pressPose, m_dynamic );
    for ( const Capsule &c : m_capsules )
    {
        Matrix at = MatrixMultiply( MatrixRotateY( c.m_yaw ), MatrixTranslate( c.m_pos.x, c.m_pos.y, c.m_pos.z ) );
        for ( size_t i = 0; i < m_capsuleMeshes.size(); ++i )
        {
            Drawable d;
            d.m_mesh = m_capsuleMeshes[i];
            d.m_transform = at;
            d.m_surface = m_capsuleSurfaces[i] < 0 ? S_GLOW : m_capsuleSurfaces[i];
            if ( m_capsuleSurfaces[i] < 0 )
                d.m_emissive = c.m_active ? Vector4{ 0.4f, 3.0f, 1.0f, 0.0f } : Vector4{ 0.05f, 0.4f, 0.12f, 0.0f };
            m_dynamic.push_back( d );
        }
    }
    if ( m_doorHalf > 0.0f )
    {
        for ( Mesh m : m_doorFrame )
        {
            Drawable d;
            d.m_mesh = m;
            d.m_surface = S_HAZARD;
            d.m_transform = MatrixIdentity();
            m_dynamic.push_back( d );
        }
        for ( float side : { -1.0f, 1.0f } )
            for ( Mesh m : m_doorPanel )
            {
                Drawable d;
                d.m_mesh = m;
                d.m_surface = S_METAL;
                d.m_transform =
                    MatrixTranslate( m_door.x + side * m_doorHalf * ( 0.5f + m_doorOpen * 0.95f ), 0.0f, m_door.z );
                m_dynamic.push_back( d );
            }
    }
    for ( const Pickup &p : m_pickups )
    {
        if ( !p.m_alive ) continue;
        Matrix at = MatrixMultiply( MatrixRotateY( p.m_t * 1.5f ),
                                    MatrixTranslate( p.m_pos.x, p.m_pos.y + 0.08f * sinf( p.m_t * 3.0f ), p.m_pos.z ) );
        Drawable d;
        d.m_transform = at;
        if ( p.m_kind == 0 )
        {
            d.m_mesh = m_scrapMesh;
            d.m_surface = S_METAL;
            d.m_emissive = { 0.25f, 0.2f, 0.12f, 0.0f };
            m_dynamic.push_back( d );
        }
        else
        {
            d.m_mesh = m_stimMesh;
            d.m_surface = S_GLOW;
            d.m_emissive = { 0.4f, 3.0f, 0.8f, 0.0f };
            m_dynamic.push_back( d );
            d.m_mesh = m_stimCap;
            d.m_surface = S_METAL;
            d.m_emissive = { -1, 0, 0, 0 };
            m_dynamic.push_back( d );
        }
    }
    for ( const Debris &b : m_debris )
    {
        Drawable d;
        d.m_mesh = m_chunk;
        d.m_surface = S_RUST;
        d.m_transform =
            MatrixMultiply( MatrixRotateXYZ( b.m_rot ), MatrixTranslate( b.m_pos.x, b.m_pos.y, b.m_pos.z ) );
        d.m_spotShadow = false;
        m_dynamic.push_back( d );
    }
    // The lift: deck, posts and rails, its hazard rim, beacons red while the gate is shut, green when it waits.
    if ( m_liftMax.x > m_liftMin.x )
    {
        Matrix  at = MatrixTranslate( m_lift.x, m_liftY, m_lift.z );
        Vector4 beacon = liftOpen() ? Vector4{ 0.6f, 5.0f, 1.6f, 0.0f } : Vector4{ 5.0f, 0.4f, 0.2f, 0.0f };
        if ( liftOpen() && fmodf( m_time, 1.0f ) > 0.6f && !m_liftGoing ) beacon = { 0.1f, 1.0f, 0.3f, 0.0f };
        auto add = [&]( const std::vector<Mesh> &meshes, int surface, Vector4 glow )
        {
            for ( Mesh m : meshes )
            {
                Drawable d;
                d.m_mesh = m;
                d.m_surface = surface;
                d.m_transform = at;
                d.m_emissive = glow;
                m_dynamic.push_back( d );
            }
        };
        add( m_liftDeck, S_METAL, { -1, 0, 0, 0 } );
        add( m_liftRim, S_HAZARD, { -1, 0, 0, 0 } );
        add( m_liftBeacons, S_GLOW, beacon );
    }
    // The anvil glows red while the ram is up.
    Vector3 po = m_pressModel.m_origin;
    if ( po.x != 0.0f && m_pressPose.m_warn > 0.05f && !m_pressDead )
    {
        Drawable d;
        d.m_mesh = m_anvilGlow;
        d.m_surface = S_GLOW;
        d.m_transform = MatrixTranslate( po.x, 0.0f, po.z );
        float w = m_pressPose.m_warn;
        d.m_emissive = { 3.5f * w, 0.25f * w, 0.1f * w, 0.0f };
        m_dynamic.push_back( d );
    }

    Lighting L = lighting();
    gfx.m_sunShadows = m_skyTex.id > 0;
    gfx.m_cutCentre = Vector3Add( m_pos, { 0.0f, 1.2f, 0.0f } );
    gfx.m_cutRadius = 2.6f;
    gfx.m_exposure = 1.7f * m_brightness;
    gfx.shadows( L, m_pos, { &m_static, &m_dynamic } );
    gfx.beginScene( m_cam, L, m_time, m_skyTex.id > 0 ? m_skyTex : m_dot );
    drawView( gfx );
    gfx.draw( m_static, m_surfaces );
    gfx.draw( m_dynamic, m_surfaces );
    if ( m_glassMesh.vertexCount > 0 ) gfx.glass( m_glassMesh, MatrixIdentity() );
    drawParticles( gfx );
    gfx.endScene();
    gfx.post( out );

    int w = out ? out->texture.width : GetScreenWidth();
    int h = out ? out->texture.height : GetScreenHeight();
    if ( out ) BeginTextureMode( *out );
    drawHud( w, h );
    if ( out ) EndTextureMode();
}

void Game::drawHud( int w, int h )
{
    float s = h / 1080.0f;
    auto  text = [&]( const char *t, float x, float y, float size, Color c, bool centre = false )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        Vector2 p = { centre ? x - m.x * 0.5f : x, y };
        DrawTextEx( m_font, t, { p.x + 2.0f * s, p.y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
        DrawTextEx( m_font, t, p, size * s, 1.0f, c );
    };
    // Wraps to a width and draws centred lines; returns the height used.
    auto wrapped = [&]( const std::string &t, float x, float y, float size, float width, Color c )
    {
        std::vector<std::string> lines;
        std::string              line, word;
        auto                     flush = [&]()
        {
            std::string test = line.empty() ? word : line + " " + word;
            if ( MeasureTextEx( m_font, test.c_str(), size * s, 1.0f ).x > width && !line.empty() )
            {
                lines.push_back( line );
                line = word;
            }
            else
                line = test;
            word.clear();
        };
        for ( char ch : t )
        {
            if ( ch == ' ' )
                flush();
            else
                word += ch;
        }
        flush();
        if ( !line.empty() ) lines.push_back( line );
        for ( size_t i = 0; i < lines.size(); ++i )
            text( lines[i].c_str(), x, y + i * size * 1.25f * s, size, c, true );
        return lines.size() * size * 1.25f * s;
    };

    // Hurt: red at the edges.
    if ( m_hurtFlash > 0.0f )
    {
        unsigned char a = (unsigned char)( 100 * m_hurtFlash );
        DrawRectangleGradientV( 0, 0, w, h / 4, { 140, 0, 0, a }, { 140, 0, 0, 0 } );
        DrawRectangleGradientV( 0, h * 3 / 4, w, h / 4, { 140, 0, 0, 0 }, { 140, 0, 0, a } );
    }

    // The auto-aim's target: four corners.
    if ( m_target.m_kind >= 0 && !m_dead )
    {
        Vector2 p = GetWorldToScreenEx( targetPoint( m_target ), m_cam, w, h );
        float   r = 22.0f * s, l = 8.0f * s;
        Color   c = { 255, 190, 80, 200 };
        for ( int k = 0; k < 4; ++k )
        {
            float   sx = k & 1 ? 1.0f : -1.0f, sy = k & 2 ? 1.0f : -1.0f;
            Vector2 corner = { p.x + sx * r, p.y + sy * r };
            DrawLineEx( corner, { corner.x - sx * l, corner.y }, 2.0f * s, c );
            DrawLineEx( corner, { corner.x, corner.y - sy * l }, 2.0f * s, c );
        }
    }

    // The ship's dodges, the patrol out there, the last scene's dark and the clinic.
    drawShipHud( w, h, s );

    // The hero: health, the rifle's heat or the next blast bolt, stims, scrap.
    float x = 40.0f * s, y = h - 150.0f * s;
    float bw = 320.0f * s, bh = 14.0f * s;
    if ( m_scene < 2 )
    {
        DrawRectangle( (int)( x - 14 * s ), (int)( y - 14 * s ), (int)( 360 * s ), (int)( 124 * s ), { 0, 0, 0, 120 } );
        text( tr( std::string( "hero." ) + heroKey( m_heroKind ) ), x, y, 22, { 255, 200, 120, 230 } );
        DrawRectangle( (int)x, (int)( y + 32 * s ), (int)bw, (int)bh, { 40, 10, 10, 200 } );
        DrawRectangleGradientH( (int)x, (int)( y + 32 * s ), (int)( bw * m_hp / hpMax() ), (int)bh,
                                { 150, 20, 20, 255 }, { 230, 60, 40, 255 } );
        DrawRectangle( (int)x, (int)( y + 54 * s ), (int)bw, (int)( 8 * s ), { 10, 20, 30, 200 } );
        if ( m_heroKind == HeroE::DORRA )
        {
            float ready = 1.0f - Clamp( m_boltTimer / BoltCooldown, 0.0f, 1.0f );
            Color c = ready >= 1.0f ? Color{ 255, 170, 60, 255 } : Color{ 140, 90, 40, 255 };
            DrawRectangle( (int)x, (int)( y + 54 * s ), (int)( bw * ready ), (int)( 8 * s ), c );
        }
        else
        {
            Color heat = m_overheat ? Color{ 255, 80, 30, 255 }
                                    : Color{ (unsigned char)( 80 + 175 * m_heat / 100 ),
                                             (unsigned char)( 200 - 80 * m_heat / 100 ),
                                             (unsigned char)( 255 - 200 * m_heat / 100 ), 255 };
            DrawRectangle( (int)x, (int)( y + 54 * s ), (int)( bw * m_heat / 100.0f ), (int)( 8 * s ), heat );
        }
        if ( m_overheat && fmodf( m_time, 0.4f ) < 0.25f )
        {
            const char *hot = tr( "hud.overheat" );
            text( hot, x + bw - MeasureTextEx( m_font, hot, 18 * s, 1.0f ).x, y + 4 * s, 18, { 255, 120, 60, 255 } );
        }
        text( TextFormat( tr( "hud.stock" ), m_stims, m_scrap ), x, y + 72 * s, 20, { 200, 220, 210, 230 } );
    }

    // The press: its bar in three parts (tank, core, tank), and a marker over each weak spot that can be hit now.
    if ( m_pressActive && !m_pressDead )
    {
        float tough = toughness( m_difficulty );
        float pw = 640.0f * s, px = w * 0.5f - pw * 0.5f, py = 40.0f * s, gap = 10.0f * s;
        float part = ( pw - 2.0f * gap ) / 3.0f;
        text( tr( "boss.press" ), w * 0.5f, py - 4 * s, 24, { 255, 210, 150, 240 }, true );
        const float fill[3] = { m_tank[0] / ( TankHp * tough ), m_core / ( CoreHp * tough ),
                                m_tank[1] / ( TankHp * tough ) };
        const char *names[3] = { tr( "hud.tank" ), tr( "hud.core" ), tr( "hud.tank" ) };
        bool        coreOpen = m_pressPose.m_shutter > 0.85f;
        for ( int i = 0; i < 3; ++i )
        {
            float bx = px + i * ( part + gap );
            bool  hittable = i == 1 ? coreOpen : fill[i] > 0.0f;
            Color lo = i == 1 ? Color{ 200, 80, 20, 255 } : Color{ 40, 120, 220, 255 };
            Color hi = i == 1 ? Color{ 255, 170, 60, 255 } : Color{ 120, 200, 255, 255 };
            DrawRectangle( (int)bx, (int)( py + 26 * s ), (int)part, (int)( 12 * s ), { 30, 10, 0, 200 } );
            DrawRectangleGradientH( (int)bx, (int)( py + 26 * s ), (int)( part * Clamp( fill[i], 0.0f, 1.0f ) ),
                                    (int)( 12 * s ), lo, hi );
            if ( i == 1 && !coreOpen )
                DrawRectangle( (int)bx, (int)( py + 26 * s ), (int)part, (int)( 12 * s ), { 0, 0, 0, 150 } );
            text( names[i], bx + part * 0.5f, py + 42 * s, 16,
                  hittable ? Color{ 230, 230, 230, 220 } : Color{ 120, 120, 120, 180 }, true );
        }
        Vector3 o = m_pressModel.m_origin;
        float   pulse = 0.6f + 0.4f * sinf( m_time * 6.0f );
        auto    marker = [&]( Vector3 p, Color c )
        {
            Vector2 q = GetWorldToScreenEx( p, m_cam, w, h );
            float   r = ( 16.0f + 6.0f * pulse ) * s;
            DrawPolyLinesEx( q, 4, r, 45.0f, 3.0f * s, c );
            DrawPolyLinesEx( q, 4, r * 0.45f, 45.0f, 2.0f * s, c );
        };
        for ( int side = 0; side < 2; ++side )
            if ( m_tank[side] > 0.0f )
                marker( { o.x + ( side ? 9.2f : -9.2f ), 5.4f, o.z + 0.5f },
                        { 120, 200, 255, (unsigned char)( 255 * pulse ) } );
        if ( coreOpen ) marker( { o.x, 3.4f, o.z }, { 255, 170, 60, (unsigned char)( 255 * pulse ) } );
    }

    drawFoeHud( w, h, s );

    // A hint, the level's title, the subtitles.
    if ( m_hint )
    {
        float       a = Clamp( m_hintTimer * 2.0f, 0.0f, 1.0f );
        bool        bar = ( m_pressActive && !m_pressDead ) || ( boss() >= 0 && m_foes[boss()].m_awake );
        float       hy = ( bar ? 125.0f : 50.0f ) * s;
        const char *hint = trAs( m_hint->m_id, heroKey( m_heroKind ) );
        Vector2     m = MeasureTextEx( m_font, hint, 24 * s, 1.0f );
        DrawRectangle( (int)( w * 0.5f - m.x * 0.5f - 18 * s ), (int)( hy - 10 * s ), (int)( m.x + 36 * s ),
                       (int)( m.y + 20 * s ), { 0, 0, 0, (unsigned char)( 150 * a ) } );
        text( hint, w * 0.5f, hy, 24, { 230, 235, 240, (unsigned char)( 255 * a ) }, true );
    }
    if ( m_titleTimer > 0.0f )
    {
        float a = Clamp( fminf( ( 6.0f - m_titleTimer ) * 1.5f, m_titleTimer * 0.8f ), 0.0f, 1.0f );
        text( tr( m_level.m_act ), w * 0.5f, h * 0.30f, 26, { 255, 190, 120, (unsigned char)( 220 * a ) }, true );
        text( tr( m_level.m_name ), w * 0.5f, h * 0.30f + 36 * s, 64, { 240, 235, 230, (unsigned char)( 255 * a ) },
              true );
    }
    if ( m_line && !m_talk && !m_menu )
    {
        float       a = Clamp( m_lineTimer * 3.0f, 0.0f, 1.0f );
        std::string speaker = m_line->m_speaker == "hero" ? heroKey( m_heroKind ) : m_line->m_speaker;
        std::string who = tr( "speaker." + speaker );
        for ( char &ch : who ) ch = (char)toupper( (unsigned char)ch );
        float ly = h - 190.0f * s;
        text( who.c_str(), w * 0.5f, ly, 22, { 255, 190, 110, (unsigned char)( 255 * a ) }, true );
        wrapped( trAs( m_line->m_id, heroKey( m_heroKind ) ), w * 0.5f, ly + 30 * s, 28, w * 0.55f,
                 { 245, 242, 236, (unsigned char)( 255 * a ) } );
    }

    // Death, the way down, and the end.
    if ( m_dead )
    {
        float a = Clamp( ( m_deadTimer - 0.5f ) / 1.2f, 0.0f, 1.0f );
        DrawRectangle( 0, 0, w, h, { 0, 0, 0, (unsigned char)( 255 * a ) } );
    }
    if ( m_liftGoing && !m_finished )
    {
        float a = Clamp( ( -m_liftY - 4.0f ) / 6.0f, 0.0f, 1.0f );
        DrawRectangle( 0, 0, w, h, { 0, 0, 0, (unsigned char)( 255 * a ) } );
    }
    if ( !m_finished && liftOpen() && !m_liftGoing && m_liftWait > 0.0f )
        text( tr( "hud.lift" ), w * 0.5f, h * 0.62f, 26, { 160, 255, 190, 230 }, true );
    if ( m_finished && autoAdvance() )
        DrawRectangle( 0, 0, w, h, { 0, 0, 0, (unsigned char)( 255 * Clamp( m_endTimer, 0.0f, 1.0f ) ) } );
    else if ( m_finished )
    {
        float a = Clamp( m_endTimer / 2.0f, 0.0f, 1.0f );
        DrawRectangle( 0, 0, w, h, { 0, 0, 0, (unsigned char)( 225 * a ) } );
        unsigned char ta = (unsigned char)( 255 * Clamp( m_endTimer - 1.0f, 0.0f, 1.0f ) );
        text( tr( m_level.m_act ), w * 0.5f, h * 0.34f, 26, { 255, 190, 120, ta }, true );
        text( tr( m_level.m_name ), w * 0.5f, h * 0.34f + 36 * s, 60, { 240, 235, 230, ta }, true );
        text( TextFormat( tr( "hud.summary" ), (int)m_time / 60, (int)m_time % 60, m_scrap, m_kills ), w * 0.5f,
              h * 0.34f + 120 * s, 26, { 210, 215, 220, ta }, true );
        text( tr( next().empty() ? "hud.again" : "hud.next" ), w * 0.5f, h * 0.34f + 180 * s, 22, { 160, 165, 170, ta },
              true );
    }
    // The map over the game, when asked for.
    if ( m_showMap && !m_talk && !m_menu && m_reading.empty() && !m_finished )
        drawMap( { w * 0.16f, h * 0.1f, w * 0.68f, h * 0.72f }, 0.8f, s );
    // The hub's people: the prompt, a conversation, Ottla's stims, Kiri's workshop; doors, lockers, terminals.
    drawHubHud( w, h, s );
    drawUseHud( w, h, s );
    drawLootHud( w, h, s );

    if ( m_intro > 0.0f )
    {
        // The opening card: black, the act and the name, then what has brought the hero here, a paragraph at a time.
        float a = Clamp( m_intro, 0.0f, 1.0f ), t = IntroTime - m_intro;
        DrawRectangle( 0, 0, w, h, { 0, 0, 0, (unsigned char)( 255 * a ) } );
        auto fade = [&]( float from ) { return (unsigned char)( 255 * a * Clamp( ( t - from ) * 1.2f, 0.0f, 1.0f ) ); };
        float top = h * 0.22f;
        text( tr( m_level.m_act ), w * 0.5f, top, 26, { 255, 190, 120, fade( 0.3f ) }, true );
        text( tr( m_level.m_name ), w * 0.5f, top + 36 * s, 60, { 240, 235, 230, fade( 0.3f ) }, true );
        top += 150 * s;
        std::string       intro = tr( m_level.m_intro ), part;
        std::stringstream parts( intro );
        for ( int i = 0; std::getline( parts, part, '|' ); ++i )
            top += wrapped( part, w * 0.5f, top, 30, w * 0.56f, { 215, 210, 200, fade( 1.5f + i * 2.5f ) } ) + 24 * s;
        text( tr( "talk.next" ), w * 0.5f, h * 0.9f, 20, { 150, 150, 150, fade( 4.0f ) }, true );
    }
}

std::string Game::report() const
{
    char buf[384];
    int  alive = 0;
    for ( const Foe &f : m_foes ) alive += f.m_alive ? 1 : 0;
    snprintf(
        buf, sizeof( buf ),
        "hero=%s finished=%d time=%.0fs deaths=%d kills=%d shots=%d hp=%.0f scrap=%d stims=%d press=%s tanks=%.0f/%.0f "
        "core=%.0f alive=%d at=%.1f,%.1f waypoint=%d/%d scene=%d boss=%s(%.0f@%.0f,%.0f) level=%d path=%d lift=%d next=%d,%d read=%d",
        heroKey( m_heroKind ), m_finished ? 1 : 0, m_time, m_deaths, m_kills, m_shots, m_hp, m_scrap, m_stims,
        m_pressDead ? "dead" : ( m_pressActive ? "active" : "asleep" ), m_tank[0], m_tank[1], m_core, alive, m_pos.x,
        m_pos.z, (int)m_waypoint, (int)m_level.m_route.size(), m_scene,
        boss() >= 0 ? "alive" : ( m_fired.count( "bossdead" ) ? "dead" : "none" ),
        boss() >= 0 ? m_foes[boss()].m_hp : 0.0f, boss() >= 0 ? m_foes[boss()].m_pos.x : 0.0f,
        boss() >= 0 ? m_foes[boss()].m_pos.z : 0.0f, m_progress.m_heroLevel, (int)m_path.size(), liftOpen() ? 1 : 0,
        m_path.empty() ? -1 : m_path.front() / m_level.cols(), m_path.empty() ? -1 : m_path.front() % m_level.cols(),
        m_reading.empty() ? 0 : 1 );
    return buf;
}

}   // namespace spaceopera
