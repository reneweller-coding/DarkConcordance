/**
 * @file GameSkin.cpp
 * @brief People as skinned figures: Quaternius' base bodies (CC0) dressed by the shader (the clothes' colour below the
 *        neck, their own face above, their hair), playing clips from the two animation libraries: walking, standing
 *        and talking, sitting at a table, and more for the pictures.
 * @author Rene Weller
 */
#include "Game.h"
#include "GameShared.h"

#include <raymath.h>

#include <cmath>
#include <iterator>

namespace spaceopera
{

using namespace play;

namespace
{

/** @brief The clips taken from the first library (Rigify rig, retargeted) and the second (the bodies' own rig). */
const char *const Clips1[] = { "Idle_Loop",
                               "Walk_Loop",
                               "Jog_Fwd_Loop",
                               "Sprint_Loop",
                               "Sitting_Idle_Loop",
                               "Sitting_Talking_Loop",
                               "Idle_Talking_Loop",
                               "Death01",
                               "Hit_Chest",
                               "Interact",
                               "Crouch_Idle_Loop",
                               "Pistol_Idle_Loop",
                               "Pistol_Shoot",
                               "Punch_Jab",
                               "Punch_Cross",
                               "Sword_Attack",
                               "Roll",
                               "Dance_Loop",
                               "Spell_Simple_Shoot",
                               "PickUp_Table",
                               "Jump_Loop",
                               "Pistol_Aim_Neutral",
                               "Sword_Idle" };
const char *const Clips2[] = { "Idle_FoldArms_Loop",
                               "Idle_TalkingPhone_Loop",
                               "Sword_Regular_A",
                               "Sword_Regular_B",
                               "Sword_Regular_C",
                               "Sword_Block",
                               "Idle_Shield_Loop",
                               "OverhandThrow",
                               "Hit_Knockback",
                               "Zombie_Walk_Fwd_Loop",
                               "Zombie_Scratch",
                               "LayToIdle",
                               "Consume",
                               "Idle_No_Loop" };
const char *const Bodies[2] = { "Superhero_Male_FullBody.gltf", "Superhero_Female_FullBody.gltf" };
const char *const LightSkin[2] = { "T_Superhero_Male_Ligh.png", "T_Superhero_Female_Light_BaseColor.png" };
/** @brief Hair per body: three to choose from, the eyebrows last (always worn). */
const char *const Hair[2][4] = { { "Hair_SimpleParted", "Hair_Buzzed", "Hair_Beard", "Eyebrows_Regular" },
                                 { "Hair_Long", "Hair_Buns", "Hair_BuzzedFemale", "Eyebrows_Female" } };
constexpr int     HairStyles = 3;
/** @brief Second colours: scarves, caps, vests, packs (rust, mustard, teal, plum, bone, olive, slate). */
const Vector3 Accent[] = { { 0.55f, 0.16f, 0.12f }, { 0.62f, 0.5f, 0.18f }, { 0.16f, 0.36f, 0.38f },
                           { 0.34f, 0.2f, 0.36f },  { 0.7f, 0.66f, 0.56f }, { 0.3f, 0.34f, 0.22f },
                           { 0.2f, 0.22f, 0.3f } };
/** @brief Trousers: charcoal, brown, navy, olive, black, faded khaki. */
const Vector3 Trousers[] = { { 0.16f, 0.16f, 0.18f }, { 0.26f, 0.2f, 0.15f }, { 0.13f, 0.16f, 0.26f },
                             { 0.25f, 0.27f, 0.19f }, { 0.09f, 0.09f, 0.1f }, { 0.36f, 0.33f, 0.28f } };
/** @brief What the people of the stations do, as their clothes tell it. */
enum class OutfitE : int
{
    DOCKER,      ///< overalls with reflective bands, a tool belt, knee pads, often a cap
    TRADER,      ///< a long coat, a satchel, sometimes a scarf
    SPACER,      ///< a quilted flight jacket with a collar, cargo pockets
    TRAVELLER,   ///< worn clothes, a pack with a bedroll, a scarf, the hood up or down
    TECH,        ///< a checked shirt, sleeves rolled up, a work vest, a tool belt
    CIVILIAN,    ///< a jacket, a collar or a scarf, now and then a bag
    VENDOR,      ///< an apron, sleeves rolled up (only who stands at a stall)
    COUNT        ///< how many
};
constexpr unsigned wearing( GarmentE g )
{
    return 1u << (int)g;
}
constexpr float WalkSpeed = 1.3f;   ///< m/s the walk clip covers

/** @brief Clothes by the box figures' looks (their surfaces' tints), a little brighter. */
const Vector3 Cloth[] = { { 0.42f, 0.45f, 0.5f },  { 0.33f, 0.24f, 0.17f }, { 0.32f, 0.4f, 0.34f },
                          { 0.85f, 0.6f, 0.2f },   { 0.22f, 0.15f, 0.3f },  { 0.75f, 0.36f, 0.12f },
                          { 0.58f, 0.27f, 0.24f }, { 0.29f, 0.36f, 0.52f }, { 0.58f, 0.27f, 0.24f } };

}   // namespace

void Game::loadSkins( Gfx &gfx )
{
    std::string dir = m_assets + "/models/quaternius/";
    if ( !FileExists( ( dir + Bodies[0] ).c_str() ) ) return;
    // The figures' own textures by their UVs; rough, not metal; no normal map.
    Image     orm = GenImageColor( 1, 1, { 255, 170, 0, 255 } );
    Texture2D ormTex = LoadTextureFromImage( orm );
    UnloadImage( orm );
    Image     nrm = GenImageColor( 1, 1, { 128, 128, 255, 255 } );
    Texture2D nrmTex = LoadTextureFromImage( nrm );
    UnloadImage( nrm );
    auto surface = [&]( Texture2D albedo, float mode )
    {
        GenTextureMipmaps( &albedo );
        SetTextureFilter( albedo, TEXTURE_FILTER_TRILINEAR );
        Surface s = gfx.surface( albedo, ormTex, nrmTex, Texture2D{} );
        s.m_uvScale = 1.0f;
        s.m_params = { 1.0f, 0.0f, 0.0f, mode };
        s.m_variation = 0.0f;
        m_surfaces.push_back( s );
        return (int)m_surfaces.size() - 1;
    };
    std::vector<std::string> one( std::begin( Clips1 ), std::end( Clips1 ) );
    std::vector<std::string> two( std::begin( Clips2 ), std::end( Clips2 ) );
    for ( int b = 0; b < 2; ++b )
    {
        SkinModel &m = m_body[b];
        if ( !m.load( dir + Bodies[b], dir + "anim_library2.glb" ) ) return;
        m.addClips( dir + "anim_library.glb", one );
        m.addClips( dir + "anim_library2.glb", two );
        // The body is clothed by the shader; the eyes and brows on the face use their texture as it is.
        Texture2D light = LoadTexture( ( dir + LightSkin[b] ).c_str() );
        for ( int i = 0; i < m.m_body.meshCount; ++i )
        {
            Texture2D albedo = m.m_body.materials[m.m_body.meshMaterial[i]].maps[MATERIAL_MAP_ALBEDO].texture;
            bool      body = m.m_body.meshes[i].vertexCount > 3000;
            m_bodySurface[b][0].push_back( surface( albedo, body ? 2.0f : 1.0f ) );
            m_bodySurface[b][1].push_back( body && light.id > 0 ? surface( light, 2.0f ) : m_bodySurface[b][0].back() );
        }
        // Hair textures are grey, made to be tinted: black, brown, blond, red, grey.
        const Color tints[5] = { { 46, 38, 34, 255 },
                                 { 105, 70, 45, 255 },
                                 { 190, 160, 105, 255 },
                                 { 150, 70, 35, 255 },
                                 { 165, 165, 160, 255 } };
        m_garments[b] = buildGarments( m );
        for ( const char *h : Hair[b] )
        {
            int    k = m.addHair( dir + h + ".gltf" );
            Model &hair = m.m_hair[k];
            for ( int c = 0; c < 5; ++c )
            {
                m_hairSurface[b][c].push_back(
                    surface( hair.materials[hair.meshMaterial[0]].maps[MATERIAL_MAP_ALBEDO].texture, 1.0f ) );
                m_surfaces.back().m_tint = tints[c];
            }
        }
    }
    Image     white = GenImageColor( 4, 4, WHITE );
    Texture2D cloth = LoadTextureFromImage( white );
    UnloadImage( white );
    m_garmentSurface = surface( cloth, 5.0f );
    for ( const auto &list : { one, two } )
        for ( const std::string &name : list ) m_clip[name] = m_body[0].clip( name );
    m_skins = m_clip["Idle_Loop"] >= 0 && m_clip["Walk_Loop"] >= 0;
}

void Game::bodyFigures()
{
    if ( !m_skins ) return;
    // A body, its skin, its hair and the clothes' colour below the neck (the plates cover the rest of a suit).
    auto put = [&]( Figure &f, int body, int tone, int hair, int colour, Vector3 cloth, bool shown )
    {
        FigureBody b;
        b.m_model = &m_body[body];
        b.m_surfaces = m_bodySurface[body][tone];
        b.m_hair = hair;
        b.m_hairSurface = hair >= 0 ? m_hairSurface[body][colour][hair] : -1;
        b.m_brows = HairStyles;
        b.m_browSurface = m_hairSurface[body][colour][HairStyles];
        b.m_cloth = cloth;
        b.m_shown = shown;
        f.body( b );
    };
    const Vector3 suit = { 0.1f, 0.1f, 0.115f };
    // The heroes as their portraits: Corvin's short dark hair, Dorra's red bun, Ione's black crop.
    if ( m_heroKind == HeroE::DORRA )
        put( m_hero, 1, 1, 1, 3, { 0.16f, 0.11f, 0.08f }, true );
    else if ( m_heroKind == HeroE::IONE )
        put( m_hero, 1, 1, 2, 0, suit, true );
    else
        put( m_hero, 0, 1, 0, 0, suit, true );
    // Men in suits; the frames twice a man's height move as men do, but nobody is seen inside their plates.
    for ( Figure *f : { &m_boarderBaton, &m_boarderGun, &m_guardFigure, &m_heavyFigure } )
        put( *f, 0, 0, 1, 0, suit, true );
    for ( Figure *f : { &m_taxatorFigure, &m_hydraFigure, &m_okaforFigure, &m_leaderFigure, &m_salvagerFigure,
                        &m_lammFigure, &m_chiefFigure, &m_foremanFigure, &m_varnFigure } )
        put( *f, 0, 0, -1, 0, suit, false );
    // People in clothes: the worn ones, Ilka (a girl), Ysolde (grey), Kiri (black), the crowd's, the child.
    const int body[FolkCount] = { 0, 1, 0, 1, 1, 1, 0, 1, 0 };
    const int hair[FolkCount] = { 1, 0, 0, 0, 1, 2, 2, 1, 1 };
    const int colour[FolkCount] = { 1, 2, 0, 3, 4, 0, 3, 1, 2 };
    for ( int i = 0; i < FolkCount; ++i )
        put( m_folk[i], body[i], i % 2, hair[i], colour[i], Cloth[std::min( i, (int)std::size( Cloth ) - 1 )], true );
}

void Game::dressPerson( Person &q, unsigned seed )
{
    if ( !m_skins || q.m_look == 3 ) return;
    // Ysolde and Kiri are women; the others by the seed. Clothes as their look, hair and skin by the seed.
    q.m_body = q.m_look == 4 || q.m_look == 5 ? 1 : (int)( seed % 2 );
    q.m_hair = (int)( ( seed / 2 ) % HairStyles );
    q.m_tone = (int)( ( seed / 7 ) % 2 );
    q.m_hairColour = q.m_look == 4 ? 4 : q.m_look == 5 ? 0 : (int)( ( seed / 11 ) % 5 );
    q.m_cloth = Cloth[std::min( q.m_look, (int)std::size( Cloth ) - 1 )];
    q.m_scale = q.m_look == 8 ? 0.66f : 0.95f + ( seed % 5 ) * 0.012f;
    // What they wear over it, by what they do: Ysolde a trader's coat, Kiri a technician's vest, the child a
    // traveller's pack; the others by the seed (an apron only for who stands still).
    q.m_accent = Accent[( seed / 17 ) % std::size( Accent )];
    q.m_lower = Trousers[( seed / 19 ) % std::size( Trousers )];
    OutfitE outfit = q.m_look == 4   ? OutfitE::TRADER
                     : q.m_look == 5 ? OutfitE::TECH
                     : q.m_look == 8 ? OutfitE::TRAVELLER
                                     : (OutfitE)( ( seed / 13 ) % (int)OutfitE::COUNT );
    if ( outfit == OutfitE::VENDOR && ( q.m_walks || q.m_chair > 0.5f ) ) outfit = OutfitE::CIVILIAN;
    unsigned roll = seed / 23;
    switch ( outfit )
    {
    case OutfitE::DOCKER:
        q.m_lower = q.m_cloth;
        q.m_cut = 3.0f + ( roll % 2 ? 10.0f : 0.0f );
        q.m_wear = wearing( GarmentE::TOOLBELT ) | wearing( GarmentE::KNEEPADS ) |
                   ( roll / 2 % 2 ? wearing( GarmentE::CAP ) : 0u );
        break;
    case OutfitE::TRADER:
        q.m_cut = 20.0f;
        q.m_wear = wearing( GarmentE::COAT ) | wearing( GarmentE::SATCHEL ) |
                   ( roll % 3 == 0 || q.m_look == 4 ? wearing( GarmentE::SCARF ) : 0u );
        break;
    case OutfitE::SPACER:
        q.m_cut = 4.0f + ( roll % 2 ? 20.0f : 0.0f );
        q.m_wear = wearing( GarmentE::COLLAR ) | wearing( GarmentE::POCKETS ) |
                   ( roll % 4 == 1 ? wearing( GarmentE::SCARF ) : 0u ) |
                   ( roll % 3 == 2 ? wearing( GarmentE::TOOLBELT ) : 0u );
        break;
    case OutfitE::TRAVELLER:
        q.m_cut = 25.0f;
        q.m_wear = wearing( GarmentE::BACKPACK ) | wearing( GarmentE::SCARF ) |
                   wearing( roll % 5 < 2 ? GarmentE::HOOD_UP : GarmentE::HOOD );
        break;
    case OutfitE::TECH:
        q.m_cut = 32.0f;
        q.m_wear = wearing( GarmentE::VEST ) | wearing( GarmentE::TOOLBELT ) |
                   ( roll % 2 || q.m_look == 5 ? wearing( GarmentE::POCKETS ) : 0u ) |
                   ( roll % 4 == 1 && q.m_look != 5 ? wearing( GarmentE::CAP ) : 0u );
        break;
    case OutfitE::CIVILIAN:
        q.m_cut = 20.0f + ( roll % 3 == 0 ? 1.0f : 0.0f );
        q.m_wear = ( roll % 2 ? wearing( GarmentE::COLLAR ) : 0u ) |
                   ( roll % 3 == 1 ? wearing( GarmentE::SCARF ) : 0u ) |
                   ( roll % 4 == 2 ? wearing( GarmentE::SATCHEL ) : 0u );
        break;
    default:
        q.m_cut = 30.0f + ( roll % 2 ? 2.0f : 0.0f );
        q.m_wear = wearing( GarmentE::APRON ) | ( roll % 3 == 0 ? wearing( GarmentE::SCARF ) : 0u ) |
                   ( roll % 4 == 0 ? wearing( GarmentE::CAP ) : 0u );
        break;
    }
    q.m_wear |= wearing( GarmentE::BOOTS );
    q.m_skin = SkinState();
    q.m_skin.m_clip = m_clip["Idle_Loop"];
    q.m_skin.m_time = ( seed % 97 ) * 0.11f;
}

void Game::updateSkins( float dt )
{
    if ( !m_skins ) return;
    for ( size_t i = 0; i < m_people.size(); ++i )
    {
        Person &q = m_people[i];
        if ( q.m_body < 0 ) continue;
        const SkinModel &m = m_body[q.m_body];
        float            speed = Vector2Length( q.m_vel ), rate = 1.0f;
        int              clip = m_clip["Idle_Loop"];
        unsigned         h = (unsigned)i * 2654435761u;
        if ( q.m_force >= 0 )
            clip = q.m_force;
        else if ( speed > 0.25f )
        {
            clip = m_clip["Walk_Loop"];
            rate = speed / WalkSpeed;
        }
        else if ( q.m_chair > 0.5f )
            clip = ( ( h >> 8 ) + (unsigned)( m_time / 11.0f ) ) % 3 == 0 ? m_clip["Sitting_Talking_Loop"]
                                                                          : m_clip["Sitting_Idle_Loop"];
        else if ( !q.m_walks )
        {
            // Those who stand: talking, arms folded, on the phone, or just standing.
            const char *idle[4] = { "Idle_Talking_Loop", "Idle_FoldArms_Loop", "Idle_TalkingPhone_Loop", "Idle_Loop" };
            clip = m_clip[idle[( h >> 12 ) % 4]];
        }
        if ( clip < 0 ) clip = m_clip["Idle_Loop"];
        m.play( q.m_skin, clip, 0.3f );
        m.advance( q.m_skin, dt, rate );
    }
    // The people to talk to: talking while they do, else standing (Ottla with her arms folded).
    for ( size_t i = 0; i < m_npcs.size(); ++i )
    {
        Person &q = m_npcs[i].m_figure;
        if ( q.m_body < 0 ) continue;
        bool        talking = m_talk && m_talkNpc == (int)i;
        const char *clip = talking ? "Idle_Talking_Loop" : q.m_look == 1 ? "Idle_FoldArms_Loop" : "Idle_Loop";
        if ( q.m_chair > 0.5f )
            clip = talking ? "Sitting_Talking_Loop" : "Sitting_Idle_Loop";   // a companion at the table
        m_body[q.m_body].play( q.m_skin, m_clip[clip], 0.4f );
        m_body[q.m_body].advance( q.m_skin, dt );
    }
}

void Game::poseSkin( Person &q, std::vector<Drawable> &out )
{
    const SkinModel &m = m_body[q.m_body];
    m.pose( q.m_skin, q.m_bones );
    // On a stool the pelvis sits on the seat.
    float y = q.m_pos.y;
    if ( q.m_chair > 0.5f ) y = q.m_seat + 0.1f - m.hipHeight( m_clip["Sitting_Idle_Loop"] ) * q.m_scale;
    float  s = q.m_scale;
    Matrix place = MatrixMultiply( MatrixMultiply( MatrixScale( s, s, s ), MatrixRotateY( q.m_yaw ) ),
                                   MatrixTranslate( q.m_pos.x, y, q.m_pos.z ) );
    Matrix head = MatrixMultiply( q.m_bones[m.head()], place );
    float  neck = m.m_body.bindPose[m.head()].translation.y - 0.07f;
    for ( int i = 0; i < m.m_body.meshCount; ++i )
    {
        Drawable d;
        d.m_mesh = m.m_body.meshes[i];
        d.m_surface = m_bodySurface[q.m_body][q.m_tone][i];
        d.m_transform = place;
        if ( m.m_skinned[i] )
        {
            d.m_bones = q.m_bones.data();
            d.m_boneCount = (int)q.m_bones.size();
        }
        else
            d.m_transform = head;
        d.m_tint = { q.m_cloth.x, q.m_cloth.y, q.m_cloth.z, neck };
        d.m_cloth = { q.m_lower.x, q.m_lower.y, q.m_lower.z, q.m_cut };
        d.m_spotShadow = false;
        out.push_back( d );
    }
    // The garments, each piece on its bone. On a stool a coat shows only its collar; no apron, no pack.
    unsigned wear = q.m_wear;
    if ( q.m_chair > 0.5f )
    {
        if ( wear & wearing( GarmentE::COAT ) )
            wear = ( wear & ~wearing( GarmentE::COAT ) ) | wearing( GarmentE::COLLAR );
        wear &= ~( wearing( GarmentE::APRON ) | wearing( GarmentE::BACKPACK ) );
    }
    float         pattern = fmodf( q.m_cut, 10.0f );
    const Vector3 colours[(int)GarmentColourE::COUNT] = {
        q.m_cloth, q.m_lower.x < 0.0f ? q.m_cloth : q.m_lower, q.m_accent, { 0.3f, 0.2f, 0.13f }, { 0.1f, 0.1f, 0.11f }
    };
    for ( int g = 0; g < (int)m_garments[q.m_body].size(); ++g )
    {
        if ( !( wear & ( 1u << g ) ) ) continue;
        for ( const GarmentPiece &piece : m_garments[q.m_body][g] )
        {
            if ( piece.m_bone < 0 || piece.m_bone >= (int)q.m_bones.size() ) continue;
            Vector3  c = colours[(int)piece.m_colour];
            Drawable d;
            d.m_mesh = piece.m_mesh;
            d.m_surface = m_garmentSurface;
            d.m_transform = MatrixMultiply( q.m_bones[piece.m_bone], place );
            d.m_tint = { c.x, c.y, c.z, 0.0f };
            // The jacket's pattern on what is of its cloth; worn clothes worn all over.
            d.m_cloth = { -1.0f, 0.0f, 0.0f,
                          piece.m_colour == GarmentColourE::UPPER || pattern > 4.5f ? pattern : 0.0f };
            d.m_spotShadow = false;
            out.push_back( d );
        }
    }
    // The hair and the brows, rigid on the head; long hair stays under a cap, all but a beard under a hood.
    bool capped = wear & wearing( GarmentE::CAP ), hooded = wear & wearing( GarmentE::HOOD_UP );
    bool shortHair = q.m_hair == ( q.m_body == 0 ? 1 : 2 ) || ( q.m_body == 0 && q.m_hair == 2 );
    for ( int k : { q.m_hair, HairStyles } )
    {
        if ( k < 0 || k >= (int)m.m_hair.size() ) continue;
        if ( k == q.m_hair && ( ( capped && !shortHair ) || ( hooded && !( q.m_body == 0 && k == 2 ) ) ) ) continue;
        for ( int i = 0; i < m.m_hair[k].meshCount; ++i )
        {
            Drawable d;
            d.m_mesh = m.m_hair[k].meshes[i];
            d.m_surface = m_hairSurface[q.m_body][q.m_hairColour][k];
            d.m_transform = head;
            d.m_spotShadow = false;
            out.push_back( d );
        }
    }
}

void Game::figureLineup( int page )
{
    // A row in front of the hero, facing the camera.
    std::vector<std::pair<Figure *, FigurePose>> row;
    auto                                         add = [&]( Figure &f, auto set )
    {
        FigurePose p;
        p.m_time = 1.3f;
        set( p );
        row.push_back( { &f, p } );
    };
    if ( page == 0 )
    {
        add( m_hero, []( FigurePose & ) {} );
        add( m_hero, []( FigurePose &p ) { p.m_vel = { 0.0f, 1.2f }, p.m_phase = 1.2f; } );
        add( m_hero, []( FigurePose &p ) { p.m_vel = { 0.0f, 4.6f }, p.m_phase = 2.0f; } );
        add( m_hero, []( FigurePose &p ) { p.m_recoil = 0.6f; } );
        add( m_hero, []( FigurePose &p ) { p.m_swing = 0, p.m_swingT = 0.3f; } );
        add( m_hero, []( FigurePose &p ) { p.m_swing = 2, p.m_swingT = 0.55f; } );
        add( m_hero, []( FigurePose &p ) { p.m_block = 1.0f; } );
        add( m_hero, []( FigurePose &p ) { p.m_air = true; } );
        add( m_hero, []( FigurePose &p ) { p.m_down = 0.5f; } );
    }
    else if ( page == 2 )
    {
        // Hits, a heavy blow, the dodge roll, the hero and a carbine man side by side.
        add( m_hero, []( FigurePose & ) {} );
        add( m_hero, []( FigurePose &p ) { p.m_hit = 0.7f; } );
        add( m_hero, []( FigurePose &p ) { p.m_knock = 0.6f; } );
        add( m_hero, []( FigurePose &p ) { p.m_roll = 0.3f; } );
        add( m_hero, []( FigurePose &p ) { p.m_roll = 0.6f; } );
        add( m_boarderGun, []( FigurePose & ) {} );
        add( m_boarderGun, []( FigurePose &p ) { p.m_hit = 0.7f; } );
        add( m_boarderGun, []( FigurePose &p ) { p.m_knock = 0.5f; } );
    }
    else
    {
        add( m_boarderBaton, []( FigurePose & ) {} );
        add( m_boarderGun, []( FigurePose &p ) { p.m_vel = { 0.0f, 1.2f }, p.m_phase = 0.6f; } );
        add( m_guardFigure, []( FigurePose &p ) { p.m_block = 1.0f; } );
        add( m_heavyFigure, []( FigurePose &p ) { p.m_recoil = 0.6f; } );
        add( m_folk[0], []( FigurePose &p ) { p.m_vel = { 0.0f, 1.2f }, p.m_phase = 2.5f; } );
        add( m_folk[3], []( FigurePose & ) {} );
        add( m_folk[4], []( FigurePose & ) {} );
        add( m_folk[5], []( FigurePose &p ) { p.m_sit = 1.0f; } );
        add( m_folk[8], []( FigurePose & ) {} );
        add( m_taxatorFigure, []( FigurePose & ) {} );
    }
    float gap = page == 0 ? 1.4f : 1.7f;
    for ( size_t i = 0; i < row.size(); ++i )
    {
        FigurePose &p = row[i].second;
        p.m_pos = { m_pos.x + ( i - ( row.size() - 1 ) * 0.5f ) * gap, m_pos.y, m_pos.z + 2.0f };
        p.m_yaw = 0.0f;
        if ( Vector2Length( p.m_vel ) > 0.0f )
            p.m_yaw = PI * 0.15f,
            p.m_vel = { sinf( p.m_yaw ) * Vector2Length( p.m_vel ), cosf( p.m_yaw ) * Vector2Length( p.m_vel ) };
    }
    m_lined = row;
}

void Game::lineup()
{
    if ( !m_skins ) return;
    // A row in front of the hero, facing the camera, each in another clip and caught a little way in.
    const char *clips[] = { "Idle_Loop",         "Walk_Loop",           "Jog_Fwd_Loop",    "Sitting_Idle_Loop",
                            "Idle_Talking_Loop", "Pistol_Shoot",        "Sword_Regular_A", "Punch_Cross",
                            "Death01",           "Zombie_Walk_Fwd_Loop" };
    int         n = (int)std::size( clips );
    for ( int i = 0; i < n; ++i )
    {
        Person q;
        q.m_pos = { m_pos.x + ( i % 5 - 2 ) * 0.95f, m_pos.y, m_pos.z + ( i < 5 ? 1.4f : -1.3f ) };   // around the hero
        q.m_yaw = 0.0f;
        q.m_look = i % 2 ? 7 : 6;
        q.m_sit = 0.0f;
        q.m_down = 0.0f;
        dressPerson( q, (unsigned)( i * 13 + 1 + i / 7 * 23 ) );   // each in another outfit
        q.m_look = i == 0 ? 0 : i % 9 == 3 ? 5 : q.m_look;
        q.m_force = m_clip[clips[i]];
        q.m_skin.m_clip = q.m_force;
        q.m_skin.m_time = 0.35f;
        m_people.push_back( q );
    }
}

}   // namespace spaceopera
