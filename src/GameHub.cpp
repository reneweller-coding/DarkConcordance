/**
 * @file GameHub.cpp
 * @brief A hub: the people to talk to and their conversations, Ottla's stims, Kiri's workshop, the harvest board
 *        drawn with the game's own font (so it speaks every language), the asteroid's cavern and the other drums
 *        turning past beneath the floor, and the lift out.
 * @author Rene Weller
 */
#include "Game.h"
#include "PlayLog.h"
#include "GameShared.h"

#include "MeshBuilder.h"
#include "Rig.h"

#include <raymath.h>

#include <cmath>
#include <sstream>

namespace spaceopera
{

using namespace play;

namespace
{

constexpr float TalkReach = 2.4f;      ///< how near one has to be to talk
constexpr int   StimPrice = 15;        ///< Ottla's price
constexpr int   WorkshopLevels = 10;   ///< how far Kiri takes the weapon and the armour
const int       WeaponPrice[WorkshopLevels] = { 40, 80, 120, 300, 500, 800, 1200, 1700, 2300, 3000 };   ///< by level
const int       ArmorPrice[WorkshopLevels] = { 30, 60, 90, 250, 420, 650, 950, 1350, 1800, 2400 };
const int       SocketPrice[3] = { 120, 260, 480 };   ///< Kiri drills a socket: the first, the second, the third
constexpr float DrumRadius = 280.0f;                  ///< our drum: its axis this high above the floor

/** @brief What the Harvest has swallowed (proper names, the same in every language); the newest last. */
const char *const Harvested[] = {
    "Mareth",   "Ostgrube",   "Halvar",      "Brennerhang",  "Dorn II",     "Ankerfeld",  "Vellis",  "Sankt Iwo",
    "Ruhn",     "Morrow",     "Schalk",      "Pell",         "Ivarsheim",   "Kaltbecken", "Nimra",   "Ostar III",
    "Cassel",   "Grauhafen",  "Lune XVII",   "Varga",        "Feldmark",    "Okana",      "Bettel",  "Silberfaden",
    "Hollin",   "Weißnarbe",  "Trask",       "Amsel",        "Kerrin",      "Zinnober",   "Lasso",   "Neumond",
    "Drift",    "Rostgürtel", "Quell",       "Sarai",        "Haken",       "Wendel",     "Beryll",  "Molch",
    "Fahl VII", "Tannhang",   "Isk",         "Orrin",        "Krähenstein", "Halde IV",   "Moos",    "Ebbe",
    "Saffran",  "Gallow",     "Kiel",        "Tessa",        "Brandmark",   "Vigil",      "Schlick", "Rade",
    "Akkra",    "Seren-Süd",  "Hesper-Tief", "Langmut-Dorf", "Talgrund"
};
constexpr int HarvestCount = (int)( sizeof( Harvested ) / sizeof( Harvested[0] ) );

}   // namespace

void Game::loadHub( Gfx &gfx, const std::string &assets )
{
    // The lines one overhears in passing, started somewhere in the list.
    while ( hasText( TextFormat( "bark.%d", m_barks + 1 ) ) ) ++m_barks;
    m_barkNext = m_barks ? (int)( rnd() * m_barks ) % m_barks : 0;
    // Guests on most of the stools, at the tables and the counter (in a hub's bar, in a ship's mess; not at a
    // market's stalls while it is fought over).
    std::set<size_t> taken;
    seatCompanions( taken );
    if ( m_level.m_generated.empty() )
    {
        const int adults[5] = { 0, 1, 2, 6, 7 };
        for ( size_t i = 0; i < m_level.m_seats.size(); ++i )
        {
            if ( rnd() > 0.65f || taken.count( i ) ) continue;
            const Seat &s = m_level.m_seats[i];
            Person      q;
            q.m_pos = s.m_pos;
            q.m_yaw = s.m_yaw + ( rnd() - 0.5f ) * 0.4f;
            q.m_look = adults[i % 5];
            q.m_sit = 0.0f;
            q.m_down = 0.0f;
            q.m_chair = 1.0f;
            q.m_seat = s.m_height;
            q.m_fear = -2;   // in a ship's mess they get up when it is boarded (updateRefugees)
            q.m_cratePos = s.m_pos;
            q.m_crateYaw = q.m_yaw;
            m_people.push_back( q );
        }
    }
    // People: a figure by their look (or the station's terminal), and their portrait for the conversations.
    for ( size_t i = 0; i < m_level.m_npcs.size(); ++i )
    {
        const Npc  &n = m_level.m_npcs[i];
        NpcState    s;
        std::string look = n.m_look;
        // A companion who is the one played is not there (a terminal stands in so the indices hold, hidden).
        s.m_def = (int)i;
        s.m_pos = m_level.centre( n.m_row, n.m_col );
        if ( n.m_row == -2 )
        {
            float a = -0.6f + 1.2f * (float)( i % 3 );
            s.m_pos = Vector3Add( m_pos, { sinf( a ) * 4.0f, 0.0f, -cosf( a ) * 4.0f } );
            int r, c;
            m_level.cellOf( s.m_pos, r, c );
            if ( !m_level.walkable( r, c ) )
                s.m_pos = Vector3Add( m_pos, { 2.0f * ( i % 2 ? 1.0f : -1.0f ), 0.0f, 0.0f } );
        }
        s.m_yaw = 0.0f;
        s.m_look = look == "terminal" ? -1 : look == "ysolde" ? 4 : look == "kiri" ? 5 : look == "ottla" ? 1 : 0;
        // Drawn as a skinned figure where the bodies are there: Ysolde's hair pinned up and grey, Ottla's cropped
        // and grey, Kiri's long and black.
        s.m_figure.m_pos = s.m_pos;
        s.m_figure.m_yaw = 0.0f;
        s.m_figure.m_look = std::max( s.m_look, 0 );
        s.m_figure.m_sit = 0.0f;
        s.m_figure.m_down = 0.0f;
        if ( s.m_look >= 0 ) dressPerson( s.m_figure, (unsigned)( i * 131u + 7u ) );
        if ( s.m_figure.m_body >= 0 )
        {
            s.m_figure.m_body = 1;
            s.m_figure.m_hair = look == "ysolde" ? 1 : look == "ottla" ? 2 : 0;
            s.m_figure.m_hairColour = look == "kiri" ? 0 : 4;
            s.m_figure.m_tone = look == "kiri" ? 0 : 1;
            // The later people: body (0 a man, 1 a woman), hair (-1 none), its colour (black, brown, blond, red,
            // grey), skin (0 dark, 1 light), clothes.
            struct Who
            {
                const char *m_look;
                int         m_body, m_hair, m_colour, m_tone;
                Vector3     m_cloth;
                float       m_scale = 1.0f;
            };
            static const Who who[] = {
                { "okafor", 1, 2, 4, 0, { 0.33f, 0.36f, 0.24f } },
                { "hale", 1, 1, 4, 1, { 0.1f, 0.1f, 0.12f } },
                { "pell", 0, 0, 4, 1, { 0.42f, 0.34f, 0.24f } },
                { "ruul", 0, -1, 0, 1, { 0.55f, 0.44f, 0.28f } },
                { "ilka", 1, 2, 0, 1, { 0.4f, 0.42f, 0.46f } },
                { "lamm", 0, 0, 2, 1, { 0.5f, 0.1f, 0.12f } },
                { "crew", 0, 1, 1, 0, { 0.3f, 0.32f, 0.36f } },
                { "mira", 1, 2, 2, 1, { 0.72f, 0.55f, 0.18f }, 0.62f },
            };
            for ( const Who &w : who )
                if ( look == w.m_look )
                {
                    s.m_figure.m_body = w.m_body;
                    s.m_figure.m_hair = w.m_hair;
                    s.m_figure.m_hairColour = w.m_colour;
                    s.m_figure.m_tone = w.m_tone;
                    s.m_figure.m_cloth = w.m_cloth;
                    s.m_figure.m_scale = w.m_scale;
                }
            // A whole person where one is made by their look (the portrait's name).
            s.m_figure.m_dressed = -1;
            dressAs( s.m_figure, look );
            if ( companion( look ) ) dressCompanion( s, look );
        }
        if ( companion( look ) && look == heroKey( m_tale ? m_realHero : m_heroKind ) ) s.m_hidden = true;
        m_npcs.push_back( s );
        m_blocked.insert( n.m_row * m_level.cols() + n.m_col );
        std::string portrait = assets + "/textures/portrait_" + n.m_id + ".png";
        if ( !m_portraits.count( n.m_id ) && FileExists( portrait.c_str() ) )
        {
            m_portraits[n.m_id] = LoadTexture( portrait.c_str() );
            SetTextureFilter( m_portraits[n.m_id], TEXTURE_FILTER_BILINEAR );
        }
    }
    MeshBuilder terminal;
    terminal.roundedBox( place( { 0.0f, 0.75f, 0.0f } ), { 0.38f, 0.75f, 0.26f }, 0.05f, 2 );
    terminal.roundedBox( place( { 0.0f, 1.62f, -0.05f }, { -0.3f, 0.0f, 0.0f } ), { 0.42f, 0.3f, 0.2f }, 0.04f, 2 );
    m_terminalMesh = terminal.build().front();
    MeshBuilder screen;
    screen.box( place( { 0.0f, 1.64f, 0.16f }, { -0.3f, 0.0f, 0.0f } ), { 0.34f, 0.22f, 0.01f } );
    m_screenMesh = screen.build().front();
    // The lift out: a plate with a hazard rim.
    for ( const Marker &m : m_level.m_markers )
        if ( m.m_kind == 'O' )
        {
            m_exit = m.m_pos;
            m_hasExit = true;
        }
    MeshBuilder pad;
    pad.lathe( place( { 0.0f, 0.0f, 0.0f } ), { { 0.0f, 0.03f }, { 0.95f, 0.03f }, { 1.0f, 0.0f } }, 32 );
    m_padMesh = pad.build().front();

    // The harvest board: its own surface (renderBoard paints it once the font is there).
    if ( m_level.m_boardRow >= 0 )
    {
        Image     blank = GenImageColor( 4, 4, { 30, 32, 32, 255 } );
        Texture2D tex = LoadTextureFromImage( blank );
        UnloadImage( blank );
        Surface surf = gfx.surface( tex, m_surfaces[S_METAL].m_material.maps[MATERIAL_MAP_METALNESS].texture,
                                    m_surfaces[S_METAL].m_material.maps[MATERIAL_MAP_NORMAL].texture, tex );
        surf.m_uvScale = 1.0f;
        surf.m_params = { 0.9f, 0.1f, 0.0f, 1.0f };
        surf.m_emissive = { 0.18f, 0.18f, 0.17f, 2.0f };
        surf.m_variation = 0.0f;
        m_surfaces.push_back( surf );
        m_boardSurface = (int)m_surfaces.size() - 1;
        m_boardTex = tex;
        MeshBuilder    b;
        const Vector3 *c = m_level.m_boardCorner;
        b.quad( c[0], c[1], c[2], c[3] );
        for ( Mesh mesh : b.build() )
        {
            Drawable d;
            d.m_mesh = mesh;
            d.m_surface = m_boardSurface;
            d.m_transform = MatrixIdentity();
            m_static.push_back( d );
        }
    }

    // Beneath the floor: the asteroid's cavern and the other two drums, all turning past as ours spins.
    if ( m_level.m_view != "drums" ) return;
    Vector3 c = { m_level.cols() * CellSize * 0.5f, 0.0f, m_level.rows() * CellSize * 0.5f };
    if ( !m_level.m_glass.empty() )
    {
        MeshBuilder glass;
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
    m_axis = { c.x, DrumRadius, c.z };
    Matrix along = MatrixRotateZ( -PI * 0.5f );   // a cylinder along +x
    auto   add = [&]( MeshBuilder &b, int surface, Vector4 glow )
    {
        for ( Mesh mesh : b.build() )
        {
            Drawable d;
            d.m_mesh = mesh;
            d.m_surface = surface;
            d.m_transform = MatrixIdentity();
            d.m_emissive = glow;
            d.m_sunShadow = false;
            d.m_spotShadow = false;
            m_drums.push_back( d );
        }
    };
    // The cavern: rock all round, far off; work lights on it.
    MeshBuilder cavern;
    cavern.cylinder( MatrixMultiply( along, MatrixTranslate( m_axis.x - 4000.0f, m_axis.y, m_axis.z ) ), 1900.0f,
                     8000.0f, 64 );
    add( cavern, S_CAVERN, { 0.012f, 0.011f, 0.01f, 0.0f } );
    MeshBuilder lights;
    unsigned    seed = 1234;
    auto        rand01 = [&]() { return ( ( seed = seed * 1664525u + 1013904223u ) >> 8 ) / 16777216.0f; };
    for ( int i = 0; i < 700; ++i )
    {
        float a = rand01() * 2.0f * PI, x = ( rand01() - 0.5f ) * 6000.0f, r = 1880.0f;
        lights.box( place( { m_axis.x + x, m_axis.y + r * cosf( a ), m_axis.z + r * sinf( a ) } ),
                    { 5.0f, 5.0f, 5.0f } );
    }
    add( lights, S_GLOW, { 5.0f, 3.6f, 2.0f, 0.0f } );
    // Girders across the cavern, beacons along them: always something passing beneath.
    MeshBuilder girders, beacons;
    for ( int i = 0; i < 14; ++i )
    {
        float   a = rand01() * 2.0f * PI, b = a + 0.8f + rand01() * 1.6f, x = ( rand01() - 0.5f ) * 2400.0f;
        Vector3 p = { m_axis.x + x, m_axis.y + 1850.0f * cosf( a ), m_axis.z + 1850.0f * sinf( a ) };
        Vector3 q = { m_axis.x + x + ( rand01() - 0.5f ) * 1200.0f, m_axis.y + 1850.0f * cosf( b ),
                      m_axis.z + 1850.0f * sinf( b ) };
        Vector3 d = Vector3Subtract( q, p );
        float   len = Vector3Length( d );
        Vector3 z = Vector3Scale( d, 1.0f / len );
        Vector3 xAxis = Vector3Normalize( Vector3CrossProduct( { 0, 1, 0 }, z ) );
        girders.box( frame( xAxis, Vector3CrossProduct( z, xAxis ), z, Vector3Lerp( p, q, 0.5f ) ),
                     { 9.0f, 9.0f, len * 0.5f } );
        for ( int k = 1; k < 12; ++k ) beacons.box( place( Vector3Lerp( p, q, k / 12.0f ) ), { 7.0f, 7.0f, 7.0f } );
    }
    add( girders, S_HULL, { 0.02f, 0.02f, 0.025f, 0.0f } );
    add( beacons, S_GLOW, { 9.0f, 1.2f, 0.6f, 0.0f } );
    // The other drums: dark hulls, six long window bands glowing warm, a ring of beacons at each end.
    for ( int k = 0; k < 2; ++k )
    {
        float       a = k ? 0.75f + PI : 0.75f, rho = 760.0f;
        Vector3     centre = { m_axis.x, m_axis.y - rho * cosf( a ), m_axis.z - rho * sinf( a ) };
        MeshBuilder hull, strips, rings;
        hull.cylinder( MatrixMultiply( along, MatrixTranslate( centre.x - 1300.0f, centre.y, centre.z ) ), 250.0f,
                       2600.0f, 48 );
        add( hull, S_CAVERN, { 0.0f, 0.0f, 0.0f, 0.0f } );   // matte and unlit: metal would mirror the bar's light
        for ( int s = 0; s < 6; ++s )
        {
            float   b = s * PI / 3.0f + k * 0.5f;
            Vector3 n = { 0.0f, cosf( b ), sinf( b ) };
            Vector3 p = Vector3Add( centre, Vector3Scale( n, 252.0f ) );
            Matrix  m = frame( { 1, 0, 0 }, n, Vector3CrossProduct( { 1, 0, 0 }, n ), p );
            strips.box( m, { 1180.0f, 2.0f, 16.0f } );
        }
        add( strips, S_GLOW, { 2.2f, 1.5f, 0.8f, 0.0f } );
        // Lit rooms all over the hull: people live in there.
        MeshBuilder rooms;
        for ( int i = 0; i < 1400; ++i )
        {
            float   b = rand01() * 2.0f * PI;
            Vector3 n = { 0.0f, cosf( b ), sinf( b ) };
            Vector3 p = Vector3Add(
                centre, Vector3Add( { ( rand01() - 0.5f ) * 2500.0f, 0.0f, 0.0f }, Vector3Scale( n, 251.0f ) ) );
            rooms.box( frame( { 1, 0, 0 }, n, Vector3CrossProduct( { 1, 0, 0 }, n ), p ),
                       { 3.0f + rand01() * 4.0f, 1.0f, 2.5f } );
        }
        add( rooms, S_GLOW, { 3.0f, 2.1f, 1.1f, 0.0f } );
        for ( float end : { -1300.0f, 1300.0f } )
            for ( int i = 0; i < 16; ++i )
            {
                float b = i * PI / 8.0f;
                rings.box( place( { centre.x + end, centre.y + 256.0f * cosf( b ), centre.z + 256.0f * sinf( b ) } ),
                           { 8.0f, 8.0f, 8.0f } );
            }
        add( rings, S_GLOW, { 8.0f, 6.0f, 4.0f, 0.0f } );
    }
}

void Game::renderBoard()
{
    if ( m_boardSurface < 0 || m_font.texture.id == 0 ) return;
    // A slate, chalk on it: the title, and the names in columns, the newest one fresh in red.
    const int       W = 1600, H = 800;
    RenderTexture2D rt = LoadRenderTexture( W, H );
    BeginTextureMode( rt );
    ClearBackground( { 30, 33, 33, 255 } );
    unsigned seed = 77;
    auto     rand01 = [&]() { return ( ( seed = seed * 1664525u + 1013904223u ) >> 8 ) / 16777216.0f; };
    for ( int i = 0; i < 60; ++i )
        DrawCircleV( { rand01() * W, rand01() * H }, 30.0f + rand01() * 120.0f, { 255, 255, 255, 5 } );
    auto chalk = [&]( const char *t, float x, float y, float size, Color c, bool centre )
    {
        Vector2 m = MeasureTextEx( m_font, t, size, 1.0f );
        DrawTextEx( m_font, t, { centre ? x - m.x * 0.5f : x, y }, size, 1.0f, c );
    };
    chalk( tr( "board.title" ), W * 0.5f, 26.0f, 84.0f, { 236, 232, 220, 255 }, true );
    chalk( tr( "board.sub" ), W * 0.5f, 112.0f, 30.0f, { 190, 186, 176, 255 }, true );
    const char *kinds[4] = { tr( "board.moon" ), tr( "board.ocean" ), tr( "board.settlement" ), tr( "board.belt" ) };
    int         perColumn = ( HarvestCount + 3 ) / 4;
    for ( int i = 0; i < HarvestCount; ++i )
    {
        float x = 60.0f + ( i / perColumn ) * 390.0f, y = 172.0f + ( i % perColumn ) * 38.0f;
        bool  newest = i == HarvestCount - 1;
        Color c = newest ? Color{ 255, 120, 90, 255 } : Color{ 228, 224, 212, 235 };
        chalk( TextFormat( "%2d  %s", i + 1, Harvested[i] ), x, y, 28.0f, c, false );
        unsigned kind = ( (unsigned)i * 2654435761u >> 7 ) % 4;
        if ( newest ) kind = 2;
        chalk( kinds[kind], x + 245.0f, y + 6.0f, 20.0f, { 160, 158, 150, 220 }, false );
    }
    EndTextureMode();
    Image img = LoadImageFromTexture( rt.texture );
    ImageFlipVertical( &img );
    Texture2D tex = LoadTextureFromImage( img );
    UnloadImage( img );
    UnloadRenderTexture( rt );
    GenTextureMipmaps( &tex );
    SetTextureFilter( tex, TEXTURE_FILTER_TRILINEAR );
    Material &mat = m_surfaces[m_boardSurface].m_material;
    if ( m_boardTex.id > 0 ) UnloadTexture( m_boardTex );
    m_boardTex = tex;
    mat.maps[MATERIAL_MAP_ALBEDO].texture = tex;
    mat.maps[MATERIAL_MAP_EMISSION].texture = tex;
}

int Game::nearNpc() const
{
    int   best = -1;
    float bestDist = TalkReach;
    for ( size_t i = 0; i < m_npcs.size(); ++i )
    {
        if ( m_npcs[i].m_hidden ) continue;
        float d = Vector2Distance( { m_npcs[i].m_pos.x, m_npcs[i].m_pos.z }, { m_pos.x, m_pos.z } );
        if ( d < bestDist ) bestDist = d, best = (int)i;
    }
    return best;
}

std::vector<std::string> Game::openExits( bool &promised ) const
{
    std::vector<std::string> open;
    promised = false;
    for ( const auto &e : m_level.m_exits )
    {
        std::string done = "done." + e.first.substr( 0, e.first.find( '.' ) );
        if ( !m_fired.count( e.second ) ) continue;
        promised = true;
        if ( !m_fired.count( done ) || m_level.m_repeat.count( e.first ) ) open.push_back( e.first );
    }
    return open;
}

void Game::openTalk( int npc )
{
    // Where the conversation starts depends on what has happened.
    const Npc  &def = m_level.m_npcs[m_npcs[npc].m_def];
    std::string node = def.m_start;
    for ( const auto &after : def.m_after )
        if ( m_fired.count( after.first ) ) node = after.second;
    m_talkNpc = npc;
    m_vel.x = m_vel.z = 0.0f;
    m_hint = nullptr;
    m_hintQueue.clear();
    m_fired.insert( "talked." + def.m_id );
    enterNode( m_level.talk( node ) );
}

void Game::openForPicture( const std::string &who, int menu )
{
    for ( size_t i = 0; i < m_npcs.size(); ++i )
        if ( m_level.m_npcs[m_npcs[i].m_def].m_id == who ) openTalk( (int)i );
    m_menu = menu;
    // The lift with the places open (as stepping on it does).
    bool promised = false;
    if ( menu == 3 ) m_liftChoices = openExits( promised );
}

void Game::enterNode( const TalkNode *node )
{
    // An answer may need something to have happened ("key>node?flag"), or not ("?!flag").
    m_talk = nullptr;
    m_choice = 0;
    m_audio.stopVoice();
    if ( !node ) return;
    m_shown = *node;
    speak( node->m_id );
    m_shown.m_answers.clear();
    for ( const auto &a : node->m_answers )
    {
        size_t q = a.second.find( '?' );
        if ( q == std::string::npos )
        {
            m_shown.m_answers.push_back( a );
            continue;
        }
        // Several conditions joined by '&' must all hold ("?hero.dorra&!done.tale_dorra").
        std::stringstream all( a.second.substr( q + 1 ) );
        std::string       flag;
        bool              ok = true;
        while ( std::getline( all, flag, '&' ) )
        {
            bool not_ = !flag.empty() && flag[0] == '!';
            if ( not_ ) flag = flag.substr( 1 );
            if ( ( m_fired.count( flag ) > 0 ) == not_ ) ok = false;
        }
        if ( ok ) m_shown.m_answers.push_back( { a.first, a.second.substr( 0, q ) } );
    }
    m_talk = &m_shown;
    if ( node->m_event.empty() ) return;
    // A step's own events run as it is shown; the shop and the workshop open once it has been read.
    std::stringstream events( node->m_event );
    std::string       e;
    while ( std::getline( events, e, ',' ) )
        if ( e != "shop" && e != "workshop" ) runEvent( e );
}

void Game::runEvent( const std::string &event )
{
    // "go:<level>": the conversation takes everyone there (a tale), at once.
    if ( event.compare( 0, 3, "go:" ) == 0 )
    {
        m_exitTo = event.substr( 3 ) + ".txt";
        m_finished = true;
        m_talk = nullptr;
        return;
    }
    if ( event.compare( 0, 5, "scrap" ) == 0 )
    {
        m_scrap += atoi( event.c_str() + 5 );
        m_audio.play( SfxE::PICKUP, m_pos, m_camTarget, 0.8f );
        return;
    }
    // A new body: the talents are free to be given again (once, at the clinic).
    if ( event == "respec" )
    {
        int spent = 0;
        for ( int &k : m_progress.m_ranks ) spent += k, k = 0;
        m_progress.m_points += spent;
        refreshStats();
        m_hp = fminf( m_hp, hpMax() );
    }
    // Anything else is something that has happened: it stays, from level to level.
    fire( event );
    m_progress.m_flags.insert( event );
}

void Game::buy( int item )
{
    auto pay = [&]( int price )
    {
        if ( m_scrap < price ) return false;
        m_scrap -= price;
        m_audio.play( SfxE::PICKUP, m_pos, m_camTarget, 0.8f );
        return true;
    };
    if ( m_menu == 1 && item == 0 && m_stims < maxStims() && pay( StimPrice ) ) ++m_stims;
    // A sealed crate: an item for the slot chosen, at the place's level, the rare ones a little more often.
    if ( m_menu == 1 && item == 1 && (int)m_progress.m_bag.size() < BagSize && pay( cratePrice() ) )
    {
        unsigned seed = (unsigned)m_progress.m_seed;
        Item     it = makeItem( std::max( 1, areaLevel() ), heroIndex(), seed, 1.6f, m_crateSlot );
        m_progress.m_seed = (int)( seed & 0x7fffffff );
        m_progress.m_bag.push_back( it );
        m_toast = itemName( it );
        m_toastColor = rarityColor( it.m_rarity );
        m_toastTimer = 2.5f;
        playlog::line( "crate %s rarity %d level %d", itemName( it ).c_str(), (int)it.m_rarity, it.m_level );
    }
    if ( m_menu == 2 && item == 0 && m_progress.m_weapon < WorkshopLevels && pay( WeaponPrice[m_progress.m_weapon] ) )
        ++m_progress.m_weapon;
    if ( m_menu == 2 && item == 1 && m_progress.m_armor < WorkshopLevels && pay( ArmorPrice[m_progress.m_armor] ) )
    {
        ++m_progress.m_armor;
        m_hp = fminf( hpMax(), m_hp + 20.0f );
    }
    // An affix rolled anew or the item made better, on the worn item chosen.
    if ( m_menu == 2 && ( item == 4 || item == 5 ) && m_progress.m_worn[m_craftSlot] )
    {
        Item    &it = m_progress.m_gear[m_craftSlot];
        bool     upgrade = item == 5;
        int      price = craftPrice( it, upgrade );
        unsigned seed = (unsigned)m_progress.m_seed;
        Item     made = it;
        if ( price > 0 && m_scrap >= price && craftItem( made, heroIndex(), seed, upgrade ) && pay( price ) )
        {
            it = made;
            m_progress.m_seed = (int)( seed & 0x7fffffff );
            playlog::line( "craft %s %s", upgrade ? "better" : "reroll", itemName( it ).c_str() );
            refreshStats();
            dressHero();
        }
    }
    // A socket drilled into the weapon or the suit (as many as the kind can take).
    if ( m_menu == 2 && ( item == 2 || item == 3 ) )
    {
        int   slot = item == 2 ? 0 : 1;
        Item &it = m_progress.m_gear[slot];
        if ( m_progress.m_worn[slot] && it.m_sockets < maxSockets( itemSlot( it ) ) && it.m_rarity != RarityE::UNIQUE &&
             it.m_rarity != RarityE::SET && pay( SocketPrice[it.m_sockets] ) )
            ++it.m_sockets;
    }
}

int Game::cratePrice() const
{
    return 40 + 14 * std::max( 1, areaLevel() );
}

void Game::campaignPrep()
{
    // Better is what is worth more (Items' worth weighs level, rarity and affixes); the best weapon of each other kind
    // stays in the bag as a spare (the autopilot changes to it where a foe resists its weapon fully), the rest salvaged.
    auto sort = [&]()
    {
        for ( bool again = true; again; )
        {
            again = false;
            for ( int i = 0; i < (int)m_progress.m_bag.size() && !again; ++i )
            {
                const Item     &it = m_progress.m_bag[i];
                const ItemBase &b = itemBases()[it.m_base];
                int             slot = gearIndex( it );
                if ( slot < 0 || ( b.m_hero >= 0 && b.m_hero != heroIndex() ) ) continue;
                if ( !m_progress.m_worn[slot] || itemValue( it ) > itemValue( m_progress.m_gear[slot] ) )
                    equip( i ), again = true;
            }
        }
        std::vector<char> spare( m_progress.m_bag.size(), 0 );
        for ( int kind = 0; kind < 4 && m_progress.m_worn[0]; ++kind )
        {
            if ( kind == weaponDamage( m_progress.m_gear[0] ) ) continue;
            int pick = -1;
            for ( int i = 0; i < (int)m_progress.m_bag.size(); ++i )
            {
                const Item     &it = m_progress.m_bag[i];
                const ItemBase &b = itemBases()[it.m_base];
                if ( gearIndex( it ) != 0 || ( b.m_hero >= 0 && b.m_hero != heroIndex() ) ||
                     weaponDamage( it ) != kind )
                    continue;
                if ( pick < 0 || itemValue( it ) > itemValue( m_progress.m_bag[pick] ) ) pick = i;
            }
            if ( pick >= 0 ) spare[pick] = 1;
        }
        for ( int i = (int)m_progress.m_bag.size() - 1; i >= 0; --i )
            if ( !spare[i] ) salvage( i );
    };
    sort();
    int menu = m_menu;
    // Stims first, then the workshop (the cheaper of weapon and armour), then the worn items made better, then
    // crates for the weakest slot while a good deal of scrap is left.
    m_menu = 1;
    for ( int k = 0; k < 8; ++k ) buy( 0 );
    m_menu = 2;
    for ( int k = 0; k < 20; ++k )
    {
        int before = m_scrap;
        buy( m_progress.m_weapon <= m_progress.m_armor ? 0 : 1 );
        if ( m_scrap == before ) buy( m_progress.m_weapon <= m_progress.m_armor ? 1 : 0 );
        if ( m_scrap == before ) break;
    }
    for ( int slot = 0; slot < EquipSlots; ++slot )
        if ( m_progress.m_worn[slot] )
        {
            m_craftSlot = slot;
            while ( craftPrice( m_progress.m_gear[slot], true ) > 0 &&
                    m_scrap >= craftPrice( m_progress.m_gear[slot], true ) * 2 )
            {
                int before = m_scrap;
                buy( 5 );
                if ( m_scrap == before ) break;
            }
        }
    m_menu = 1;
    for ( int k = 0; k < 6 && m_scrap > cratePrice() * 3; ++k )
    {
        int weakest = 0, worth = 1 << 30;
        for ( int slot = 0; slot < (int)SlotE::MODULE; ++slot )
        {
            int at = slot == (int)SlotE::HARNESS   ? 7
                     : slot == (int)SlotE::EMITTER ? 8
                     : slot == (int)SlotE::CELL    ? 9
                                                   : slot;
            int v = m_progress.m_worn[at] ? itemValue( m_progress.m_gear[at] ) : 0;
            if ( v < worth ) worth = v, weakest = slot;
        }
        m_crateSlot = weakest;
        buy( 1 );
        sort();
    }
    m_menu = menu;
}

std::string Game::campaignLine() const
{
    int worth = 0;
    for ( int i = 0; i < EquipSlots; ++i ) worth += m_progress.m_worn[i] ? itemValue( m_progress.m_gear[i] ) : 0;
    return TextFormat( "%s hero=%s herolevel=%d area=%d finished=%d time=%.0f deaths=%d kills=%d scrap=%d stims=%d "
                       "gear=%d workshop=%d/%d stalls=%d",
                       m_levelFile.c_str(), heroKey( m_heroKind ), m_progress.m_heroLevel, m_level.m_area,
                       m_finished ? 1 : 0, m_time, m_deaths, m_kills, m_scrap, m_stims, worth, m_progress.m_weapon,
                       m_progress.m_armor, m_stalls );
}

Vector3 Game::strollTo( const Person &q )
{
    // Somewhere in sight on open floor, the way there clear of props and of those who sit or stand.
    int r0, c0;
    m_level.cellOf( q.m_pos, r0, c0 );
    for ( int tries = 0; tries < 16; ++tries )
    {
        int r = r0 + (int)( ( rnd() - 0.5f ) * 8.0f ), c = c0 + (int)( ( rnd() - 0.5f ) * 16.0f );
        if ( !m_level.walkable( r, c ) || m_level.cell( r, c ).m_kind != CellE::FLOOR ||
             m_blocked.count( r * m_level.cols() + c ) )
            continue;
        Vector3 to = m_level.centre( r, c );
        to.x += ( rnd() - 0.5f ) * 1.2f;
        to.z += ( rnd() - 0.5f ) * 1.2f;
        float len = Vector3Distance( q.m_pos, to );
        bool  clear = len > 2.0f;
        for ( float t = 0.0f; clear && t <= len; t += 0.4f )
        {
            Vector3 p = Vector3Lerp( q.m_pos, to, t / len );
            int     r1, c1;
            m_level.cellOf( p, r1, c1 );
            clear = m_level.walkable( r1, c1 ) && m_level.cell( r1, c1 ).m_kind == CellE::FLOOR;
            for ( const Prop &pr : m_level.m_props )
                clear = clear && Vector2Distance( { p.x, p.z }, { pr.m_pos.x, pr.m_pos.z } ) > pr.m_radius + 0.45f;
            for ( const Person &o : m_people )
                clear = clear && ( o.m_walks || Vector2Distance( { p.x, p.z }, { o.m_pos.x, o.m_pos.z } ) > 0.9f );
            for ( const NpcState &n : m_npcs )
                clear = clear && Vector2Distance( { p.x, p.z }, { n.m_pos.x, n.m_pos.z } ) > 0.9f;
        }
        if ( clear ) return to;
    }
    return q.m_pos;
}

void Game::updateCrowd( float dt )
{
    // Passers-by stroll somewhere, stand about a while, stroll on; they wait when the hero or another is in the way.
    for ( Person &q : m_people )
    {
        if ( !q.m_walks ) continue;
        Vector2 want = { 0.0f, 0.0f };
        Vector2 to = { q.m_to.x - q.m_pos.x, q.m_to.z - q.m_pos.z };
        float   dist = Vector2Length( to );
        if ( q.m_wait > 0.0f )
        {
            q.m_wait -= dt;
            if ( q.m_wait <= 0.0f ) q.m_to = strollTo( q );
        }
        else if ( dist < 0.4f )
            q.m_wait = 2.5f + rnd() * 6.0f;
        else
        {
            Vector2 dir = Vector2Scale( to, 1.0f / dist );
            bool    blocked = false;
            auto    inWay = [&]( Vector3 p )
            {
                Vector2 d = { p.x - q.m_pos.x, p.z - q.m_pos.z };
                return Vector2Length( d ) < 1.3f && Vector2DotProduct( d, dir ) > 0.3f;
            };
            blocked = inWay( m_pos );
            for ( const Person &o : m_people )
                if ( &o != &q && inWay( o.m_pos ) ) blocked = true;
            want = blocked ? Vector2{ 0.0f, 0.0f } : Vector2Scale( dir, q.m_look == 8 ? 1.7f : 1.15f );
            // Stuck behind someone for long: go elsewhere.
            if ( blocked && rnd() < dt * 0.3f ) q.m_wait = 1.0f;
        }
        q.m_vel = Vector2Lerp( q.m_vel, want, fminf( 1.0f, dt * 5.0f ) );
        q.m_pos.x += q.m_vel.x * dt;
        q.m_pos.z += q.m_vel.y * dt;
        if ( Vector2Length( q.m_vel ) > 0.15f )
            q.m_yaw += Clamp( angleTo( q.m_yaw, atan2f( q.m_vel.x, q.m_vel.y ) ), -5.0f * dt, 5.0f * dt );
        FigurePose p;
        p.m_yaw = q.m_yaw;
        p.m_vel = q.m_vel;
        p.m_phase = q.m_phase;
        m_folk[q.m_look].step( p, dt );
        q.m_phase = p.m_phase;
    }

    // Now and then the nearest one says something the hero overhears; never the same one twice in a while.
    m_barkShow -= dt;
    if ( m_barkShow <= 0.0f ) m_barkWho = -1;
    m_barkPause -= dt;
    if ( m_barks == 0 || m_barkPause > 0.0f || m_talk || m_menu ) return;
    int   who = -1;
    float best = 4.5f;
    for ( size_t i = 0; i < m_people.size(); ++i )
    {
        const Person &q = m_people[i];
        float         d = Vector3Distance( q.m_pos, m_pos );
        if ( d < best && m_time - q.m_said > 45.0f && (int)i != m_ilka ) best = d, who = (int)i;
    }
    if ( who < 0 ) return;
    m_barkWho = who;
    m_barkKey = TextFormat( "bark.%d", m_barkNext + 1 );
    m_barkNext = ( m_barkNext + 1 ) % m_barks;
    m_barkShow = 4.5f;
    m_barkPause = 7.0f + rnd() * 5.0f;
    m_people[who].m_said = m_time;
}

bool Game::updateHub( float dt, GameInput &in )
{
    if ( !m_level.m_peaceful ) return updateFightTalk( in );
    // No fighting here: the trigger does nothing, B goes back in the menus.
    in.m_fire = false;
    bool back = in.m_special;
    in.m_special = false;
    m_spin += dt * sqrtf( m_level.m_gravity / DrumRadius );
    updateCrowd( dt );
    // People turn to the hero when near.
    for ( NpcState &n : m_npcs )
    {
        if ( n.m_look < 0 || n.m_seated ) continue;
        Vector3 d = Vector3Subtract( m_pos, n.m_pos );
        float   want = Vector2Length( { d.x, d.z } ) < 6.0f ? atan2f( d.x, d.z ) : 0.0f;
        n.m_yaw += Clamp( angleTo( n.m_yaw, want ), -3.0f * dt, 3.0f * dt );
    }
    // Menus: the stick's, the cross's or the arrows' up and down in steps.
    int step = 0;
    if ( fabsf( m_rawMove.y ) > 0.5f && fabsf( m_navY ) <= 0.5f ) step = m_rawMove.y > 0.0f ? 1 : -1;
    if ( m_rawNav ) step = m_rawNav;
    m_navY = m_rawMove.y;
    int side = 0;
    if ( fabsf( m_rawMove.x ) > 0.5f && fabsf( m_navX ) <= 0.5f ) side = m_rawMove.x > 0.0f ? 1 : -1;
    m_navX = m_rawMove.x;
    bool ok = in.m_jump || in.m_confirm || in.m_use;
    if ( m_menu )
    {
        int count = m_menu == 3 ? (int)m_liftChoices.size() + 1 : ( m_menu == 1 ? 3 : 7 );
        // At the trader, left and right choose the crate's slot.
        if ( m_menu == 1 && side != 0 ) m_crateSlot = ( m_crateSlot + side + (int)SlotE::MODULE ) % (int)SlotE::MODULE;
        m_menuChoice = ( m_menuChoice + step + count ) % count;
        // At the lift, left and right choose the Depths' tier (up to one past the deepest done).
        if ( m_menu == 3 && side != 0 && m_menuChoice < (int)m_liftChoices.size() &&
             m_liftChoices[m_menuChoice] == "tiefen.txt" )
            m_progress.m_depthTier =
                std::clamp( m_progress.m_depthTier + side, 1, std::min( 10, m_progress.m_depthBest + 1 ) );
        // In the workshop, left and right pick the worn item Kiri works on.
        if ( m_menu == 2 && side != 0 )
            for ( int k = 1; k <= EquipSlots; ++k )
            {
                int slot = ( m_craftSlot + side * k + EquipSlots * 2 ) % EquipSlots;
                if ( m_progress.m_worn[slot] )
                {
                    m_craftSlot = slot;
                    break;
                }
            }
        if ( ok && m_menuChoice == count - 1 )
            m_menu = 0;
        else if ( ok && m_menu == 3 )
        {
            m_exitTo = m_liftChoices[m_menuChoice];
            m_finished = true;
            m_menu = 0;
            m_audio.play( SfxE::KLAXON, m_exit, m_camTarget, 0.6f );
        }
        else if ( ok )
            buy( m_menuChoice );
        if ( back ) m_menu = 0;
        return true;
    }
    if ( m_talk ) return talkStep( step, ok, back );
    // Not talking: A or E near someone starts a conversation (instead of a jump).
    int near = nearNpc();
    if ( near >= 0 && ( in.m_jump || in.m_use ) )
    {
        openTalk( near );
        return true;
    }
    // The lift out: where to depends on what has happened.
    if ( m_hasExit && !m_finished )
    {
        float d = Vector2Distance( { m_pos.x, m_pos.z }, { m_exit.x, m_exit.z } );
        if ( d < 1.2f && m_padArmed )
        {
            // One way: it goes. Several (or one that is always open): the lift asks where to.
            bool                     promised = false;
            std::vector<std::string> open = openExits( promised );
            m_padArmed = false;
            if ( open.size() == 1 && !m_level.m_repeat.count( open[0] ) )
            {
                m_exitTo = open[0];
                m_finished = true;
                m_audio.play( SfxE::KLAXON, m_exit, m_camTarget, 0.6f );
            }
            else if ( !open.empty() )
            {
                m_liftChoices = open;
                m_menu = 3;
                m_menuChoice = 0;
            }
            else
                fire( promised ? "exitlater" : "exitlocked" );
        }
        else if ( d > 3.0f )
            m_padArmed = true, m_fired.erase( "exitlater" ), m_fired.erase( "exitlocked" );
    }
    return false;
}

bool Game::talkStep( int step, bool ok, bool back )
{
    int n = (int)m_talk->m_answers.size();
    if ( n > 0 ) m_choice = ( m_choice + step + n ) % n;
    if ( back )
        m_talk = nullptr;
    else if ( ok && n == 0 )
    {
        // Read: the conversation ends; the shop or the workshop opens if it said so.
        const std::string &e = m_talk->m_event;
        m_menu = e.find( "workshop" ) != std::string::npos ? 2 : ( e.find( "shop" ) != std::string::npos ? 1 : 0 );
        m_menuChoice = 0;
        m_talk = nullptr;
    }
    else if ( ok )
    {
        const auto &a = m_talk->m_answers[m_choice];
        if ( a.second == "-" )
            m_talk = nullptr;
        else
            enterNode( m_level.talk( a.second ) );
    }
    if ( !m_talk ) m_audio.stopVoice();   // closed: the voice stops with it
    return true;
}

bool Game::updateFightTalk( GameInput &in )
{
    // In a fight's level someone may wait to be spoken to (E): the game holds while the conversation is open.
    if ( m_npcs.empty() ) return false;
    int step = 0;
    if ( fabsf( m_rawMove.y ) > 0.5f && fabsf( m_navY ) <= 0.5f ) step = m_rawMove.y > 0.0f ? 1 : -1;
    if ( m_rawNav ) step = m_rawNav;
    m_navY = m_rawMove.y;
    if ( m_talk ) return talkStep( step, in.m_jump || in.m_confirm || in.m_use, in.m_special );
    int near = nearNpc();
    if ( m_autoplay && m_autoTalkTo >= 0 && near >= 0 ) near = m_autoTalkTo;
    if ( near >= 0 && in.m_use )
    {
        openTalk( near );
        return true;
    }
    return false;
}

void Game::poseHub( std::vector<Drawable> &out )
{
    for ( NpcState &n : m_npcs )
    {
        if ( n.m_hidden ) continue;
        if ( n.m_figure.m_body >= 0 )
        {
            n.m_figure.m_pos = n.m_pos;
            n.m_figure.m_yaw = n.m_yaw;
            poseSkin( n.m_figure, out );
            continue;
        }
        if ( n.m_look >= 0 )
        {
            FigurePose p;
            p.m_pos = n.m_pos;
            p.m_yaw = n.m_yaw;
            p.m_time = m_time + n.m_def * 2.3f;
            m_folk[n.m_look].pose( p, { -1.0f, 0.0f, 0.0f, 0.0f }, out );
            continue;
        }
        // The terminal, its amber screen.
        Drawable d;
        d.m_transform = MatrixMultiply( MatrixRotateY( n.m_yaw ), MatrixTranslate( n.m_pos.x, n.m_pos.y, n.m_pos.z ) );
        d.m_mesh = m_terminalMesh;
        d.m_surface = S_METAL;
        out.push_back( d );
        d.m_mesh = m_screenMesh;
        d.m_surface = S_GLOW;
        float flicker = 0.85f + 0.15f * sinf( m_time * 13.0f ) * sinf( m_time * 2.1f );
        d.m_emissive = { 3.2f * flicker, 1.8f * flicker, 0.4f * flicker, 0.0f };
        out.push_back( d );
    }
    if ( m_hasExit )
    {
        // The lift's plate: green once it goes somewhere, red until then.
        bool     promised = false;
        bool     open = !openExits( promised ).empty();
        Drawable d;
        d.m_transform = MatrixTranslate( m_exit.x, m_exit.y + 0.01f, m_exit.z );
        d.m_mesh = m_padMesh;
        d.m_surface = S_HAZARD;
        out.push_back( d );
        d.m_transform =
            MatrixMultiply( MatrixScale( 6.0f, 1.0f, 6.0f ), MatrixTranslate( m_exit.x, m_exit.y + 0.03f, m_exit.z ) );
        d.m_mesh = m_scannerMesh;
        d.m_surface = S_GLOW;
        float pulse = 0.6f + 0.4f * sinf( m_time * 3.0f );
        d.m_emissive =
            open ? Vector4{ 0.4f * pulse, 3.0f * pulse, 1.0f * pulse, 0.0f } : Vector4{ 2.5f, 0.3f, 0.2f, 0.0f };
        out.push_back( d );
    }
    // The view: everything outside turns about our drum's axis.
    if ( !m_drums.empty() )
    {
        Matrix spin = MatrixMultiply(
            MatrixMultiply( MatrixTranslate( -m_axis.x, -m_axis.y, -m_axis.z ), MatrixRotateX( m_spin ) ),
            MatrixTranslate( m_axis.x, m_axis.y, m_axis.z ) );
        for ( Drawable d : m_drums )
        {
            d.m_transform = spin;
            out.push_back( d );
        }
    }
}

void Game::drawHubHud( int w, int h, float s )
{
    if ( !m_level.m_peaceful ) return;
    auto text = [&]( const char *t, float x, float y, float size, Color c, bool centre = false )
    {
        Vector2 m = MeasureTextEx( m_font, t, size * s, 1.0f );
        Vector2 p = { centre ? x - m.x * 0.5f : x, y };
        DrawTextEx( m_font, t, { p.x + 2.0f * s, p.y + 2.0f * s }, size * s, 1.0f,
                    { 0, 0, 0, (unsigned char)( c.a * 0.8f ) } );
        DrawTextEx( m_font, t, p, size * s, 1.0f, c );
    };
    // Words into lines no wider than `width`.
    auto split = [&]( const std::string &t, float size, float width )
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
            if ( ch == ' ' )
                flush();
            else
                word += ch;
        flush();
        if ( !line.empty() ) lines.push_back( line );
        return lines;
    };
    // Lines that wrap at a width; returns the height used.
    auto wrapped = [&]( const std::string &t, float x, float y, float size, float width, Color c )
    {
        std::vector<std::string> lines = split( t, size, width );
        for ( size_t i = 0; i < lines.size(); ++i ) text( lines[i].c_str(), x, y + i * size * 1.3f * s, size, c );
        return lines.size() * size * 1.3f * s;
    };
    const char *who = heroKey( m_heroKind );

    // A line overheard, over the one who says it, fading in and out.
    if ( m_barkWho >= 0 && !m_talk && !m_menu )
    {
        const Person &q = m_people[m_barkWho];
        float   up = q.m_sit > 0.5f ? 1.3f : q.m_chair > 0.5f ? q.m_seat + 1.35f : ( q.m_look == 8 ? 1.6f : 2.25f );
        Vector2 at = GetWorldToScreenEx( Vector3Add( q.m_pos, { 0.0f, up, 0.0f } ), m_cam, w, h );
        float   a = Clamp( fminf( m_barkShow, 4.5f - m_barkShow ) * 3.0f, 0.0f, 1.0f );
        std::vector<std::string> lines = split( tr( m_barkKey ), 20.0f, 380.0f * s );
        float                    step = 20.0f * 1.3f * s, y = at.y - lines.size() * step;
        for ( size_t i = 0; i < lines.size(); ++i )
            text( lines[i].c_str(), at.x, y + i * step, 20.0f, { 235, 225, 205, (unsigned char)( 235 * a ) }, true );
    }

    if ( m_talk )
    {
        // The conversation: a panel at the bottom, the portrait on the left, the line, the answers.
        float x0 = w * 0.1f, x1 = w * 0.9f, ph = 300.0f * s, y0 = h - ph - 30.0f * s;
        DrawRectangle( (int)x0, (int)y0, (int)( x1 - x0 ), (int)ph, { 8, 10, 12, 225 } );
        DrawRectangleLinesEx( { x0, y0, x1 - x0, ph }, 2.0f * s, { 255, 190, 120, 120 } );
        const Npc &def = m_level.m_npcs[m_npcs[m_talkNpc].m_def];
        float      tx = x0 + 24.0f * s;
        if ( const Texture2D *face = portrait( m_talk->m_speaker ) )
        {
            float     size = ph - 40.0f * s;
            Texture2D t = *face;
            DrawTexturePro( t, { 0, 0, (float)t.width, (float)t.height },
                            { x0 + 20.0f * s, y0 + 20.0f * s, size, size }, { 0, 0 }, 0.0f, WHITE );
            tx = x0 + size + 44.0f * s;
        }
        (void)def;
        std::string name = tr( "speaker." + m_talk->m_speaker );
        text( name.c_str(), tx, y0 + 20.0f * s, 24, { 255, 190, 110, 255 } );
        float used =
            wrapped( spoken( m_talk->m_id ), tx, y0 + 56.0f * s, 26, x1 - tx - 30.0f * s, { 240, 236, 228, 255 } );
        float ay = y0 + 66.0f * s + used;
        if ( m_talk->m_answers.empty() )
            text( tr( "talk.next" ), x1 - 24.0f * s - MeasureTextEx( m_font, tr( "talk.next" ), 20 * s, 1.0f ).x,
                  y0 + ph - 40.0f * s, 20, { 170, 170, 170, 220 } );
        for ( size_t i = 0; i < m_talk->m_answers.size(); ++i )
        {
            bool on = (int)i == m_choice;
            if ( on )
                DrawRectangle( (int)( tx - 10 * s ), (int)( ay - 4 * s ), (int)( x1 - tx - 20 * s ), (int)( 34 * s ),
                               { 255, 170, 80, 40 } );
            text( TextFormat( "%s  %s", on ? ">" : " ", trAs( m_talk->m_answers[i].first, who ) ), tx, ay, 24,
                  on ? Color{ 255, 210, 150, 255 } : Color{ 200, 200, 200, 230 } );
            ay += 36.0f * s;
        }
        return;
    }
    if ( m_menu )
    {
        // Ottla's stims or Kiri's workshop: a panel in the middle, the scrap in hand, the items with their prices.
        float pw = 1000.0f * s, x0 = w * 0.5f - pw * 0.5f, y0 = h * 0.25f;
        int   count = m_menu == 3 ? (int)m_liftChoices.size() + 1 : ( m_menu == 1 ? 3 : 7 );
        float ph = ( 130.0f + 52.0f * count ) * s;
        DrawRectangle( (int)x0, (int)y0, (int)pw, (int)ph, { 8, 10, 12, 230 } );
        DrawRectangleLinesEx( { x0, y0, pw, ph }, 2.0f * s, { 255, 190, 120, 140 } );
        text( tr( m_menu == 3 ? "lift.title" : ( m_menu == 1 ? "shop.title" : "workshop.title" ) ), w * 0.5f,
              y0 + 20.0f * s, 32, { 255, 200, 130, 255 }, true );
        if ( m_menu != 3 )
            text( TextFormat( tr( "shop.have" ), m_scrap ), w * 0.5f, y0 + 62.0f * s, 22, { 200, 220, 210, 230 },
                  true );
        for ( int i = 0; i < count; ++i )
        {
            float       y = y0 + ( 110.0f + 52.0f * i ) * s;
            bool        on = i == m_menuChoice;
            std::string label, price;
            if ( i == count - 1 )
                label = tr( "shop.back" );
            else if ( m_menu == 3 && m_liftChoices[i] == "tiefen.txt" )
                label = TextFormat( tr( "dest.tiers" ), std::clamp( m_progress.m_depthTier, 1, 10 ),
                                    std::min( 10, m_progress.m_depthBest + 1 ) );
            else if ( m_menu == 3 )
                label = tr( ( "dest." + m_liftChoices[i].substr( 0, m_liftChoices[i].find( '.' ) ) ).c_str() );
            else if ( m_menu == 1 && i == 1 )
            {
                label = TextFormat( tr( "shop.crate" ), tr( TextFormat( "slotkind.%d", m_crateSlot ) ) );
                price = TextFormat( tr( "shop.price" ), cratePrice() );
            }
            else if ( m_menu == 1 )
            {
                label = TextFormat( "%s  (%d/%d)", tr( "shop.stim" ), m_stims, maxStims() );
                price = TextFormat( tr( "shop.price" ), StimPrice );
            }
            else if ( i >= 4 )
            {
                // An affix anew or the item better: the worn item chosen with left and right, what it costs.
                bool        worn = m_progress.m_worn[m_craftSlot];
                const Item &it = m_progress.m_gear[m_craftSlot];
                bool        upgrade = i == 5;
                int         cost = worn ? craftPrice( it, upgrade ) : 0;
                label = TextFormat( tr( upgrade ? "workshop.better" : "workshop.reroll" ),
                                    tr( TextFormat( "slot.%d", m_craftSlot ) ), worn ? itemName( it ).c_str() : "-" );
                bool fixed = worn && ( it.m_rarity == RarityE::UNIQUE || it.m_rarity == RarityE::SET );
                price = cost > 0                           ? TextFormat( tr( "shop.price" ), cost )
                        : fixed                            ? tr( "workshop.fixed" )
                        : !upgrade && it.m_affixes.empty() ? tr( "workshop.plain" )
                                                           : tr( "workshop.max" );
            }
            else if ( i >= 2 )
            {
                // A socket for the weapon or the suit; not in uniques and set pieces (Kiri would not dare).
                int         slot = i == 2 ? 0 : 1;
                const Item &it = m_progress.m_gear[slot];
                bool        fixed = it.m_rarity == RarityE::UNIQUE || it.m_rarity == RarityE::SET;
                label = TextFormat( tr( i == 2 ? "workshop.socketweapon" : "workshop.socketsuit" ), it.m_sockets,
                                    maxSockets( itemSlot( it ) ) );
                price = fixed ? tr( "workshop.fixed" )
                        : it.m_sockets < maxSockets( itemSlot( it ) )
                            ? TextFormat( tr( "shop.price" ), SocketPrice[std::min( it.m_sockets, 2 )] )
                            : tr( "workshop.max" );
            }
            else
            {
                int         level = i == 0 ? m_progress.m_weapon : m_progress.m_armor;
                const char *key = i == 1
                                      ? "workshop.armor"
                                      : ( m_heroKind == HeroE::DORRA
                                              ? "workshop.hammer"
                                              : ( m_heroKind == HeroE::IONE ? "workshop.swarm" : "workshop.rifle" ) );
                label = TextFormat( tr( key ), level < WorkshopLevels ? level + 1 : WorkshopLevels );
                price = level < WorkshopLevels
                            ? TextFormat( tr( "shop.price" ), ( i == 0 ? WeaponPrice : ArmorPrice )[level] )
                            : tr( "workshop.max" );
            }
            if ( on )
                DrawRectangle( (int)( x0 + 16 * s ), (int)( y - 6 * s ), (int)( pw - 32 * s ), (int)( 42 * s ),
                               { 255, 170, 80, 40 } );
            text( label.c_str(), x0 + 36.0f * s, y, 24,
                  on ? Color{ 255, 210, 150, 255 } : Color{ 210, 210, 210, 230 } );
            if ( !price.empty() )
                text( price.c_str(), x0 + pw - 36.0f * s - MeasureTextEx( m_font, price.c_str(), 24 * s, 1.0f ).x, y,
                      24, { 200, 220, 210, 230 } );
        }
        return;
    }
    // Someone near: the prompt over their head.
    int near = nearNpc();
    if ( near >= 0 && !m_finished )
    {
        Vector2 p = GetWorldToScreenEx( Vector3Add( m_npcs[near].m_pos, { 0.0f, 2.3f, 0.0f } ), m_cam, w, h );
        text( tr( "h_talk" ), p.x, p.y, 22, { 255, 220, 160, 240 }, true );
    }
}

}   // namespace spaceopera
