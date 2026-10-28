/**
 * @file main.cpp
 * @brief SpaceOpera: level 1 „Die Kühlgedärme“ (default) or the look test (--lookdev).
 *
 *        Move with WASD, the arrows or the left stick; turn the camera with the right stick or the right mouse button;
 *        jump with Space or A; shoot with the left mouse button, the right trigger or X (the rifle takes the nearest
 *        target ahead; the mouse can aim); Q or Y injects a stim; the mouse wheel or the shoulder buttons zoom; F1
 *        shows the frame rate, F12 saves a picture.
 *
 *        --shot FILE    renders one picture and exits (--w, --h, --ss, --at x,z, --face deg, --awake, --fire,
 *                       --autoplay, --warm s, --bench n); --report writes the autopilot's run into the log
 *        --autoplay     the autopilot plays (also in the window, as a demo)
 *        --lang CODE    the language (de, en; else the saved one, at first the system's)
 *        --mute         no sound
 *        --lookdev      the look test's room instead of the level
 * @author Rene Weller
 */
#include "Game.h"
#include "Bindings.h"
#include "Files.h"
#include "Glyphs.h"
#include "Journal.h"
#include "PlayLog.h"
#include "Gfx.h"
#include "Scene.h"
#include "Score.h"
#include "Text.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <memory>
#include <string>

using namespace spaceopera;

namespace
{

/** @brief The command line. */
struct Options
{
    int                      m_width = 0;                     ///< picture width (0: 1600, on a Steam Deck 1280)
    int                      m_height = 0;                    ///< picture height (0: 900, on a Steam Deck 800)
    float                    m_ss = -1.0f;                    ///< supersampling (negative: the quality's)
    int                      m_quality = -1;                  ///< 0 low, 1 middle, 2 high (negative: the setting's)
    std::string              m_shot;                          ///< render this picture and exit
    std::string              m_level = "gute_hoffnung.txt";   ///< the level (the prologue starts the game)
    bool                     m_hasAt = false;                 ///< --at was given
    float                    m_x = 0.0f;                      ///< where to put the hero (east)
    float                    m_z = 0.0f;                      ///< (south)
    float                    m_face = 0.0f;                   ///< heading (degrees, 0 = towards the camera)
    bool                     m_walk = false;                  ///< look test: caught mid-stride
    bool                     m_fire = false;                  ///< a shot just fired
    bool                     m_awake = false;                 ///< every enemy awake
    bool                     m_autoplay = false;              ///< the autopilot plays
    bool                     m_report = false;                ///< log the autopilot's run
    bool                     m_mute = false;                  ///< no sound
    bool                     m_lookdev = false;               ///< the look test
    std::string              m_dumpSound;          ///< write the synthesised sounds as WAVs into this folder, then end
    std::string              m_scoreTest;          ///< mix a scripted stretch of music into this WAV, then end
    bool                     m_silent = false;     ///< tests: the sound runs (device, music, beds), at volume 0
    float                    m_warm = 4.0f;        ///< seconds simulated before the picture
    float                    m_exposure = -1.0f;   ///< exposure (negative: the renderer's)
    int                      m_bench = 0;    ///< with --shot: render this many frames first and log the time per frame
    std::string              m_difficulty;   ///< leicht, normal, schwer (else the saved setting)
    std::string              m_language;     ///< de, en (else the saved setting)
    bool                     m_menu = false;       ///< with --shot: the title menu over the picture
    bool                     m_controls = false;   ///< with --shot: the controls' page over the picture
    std::string              m_photo;   ///< with --shot: the photo mode, its camera turned, tilted, pulled ("t,p,z")
    std::vector<std::string> m_binds;   ///< tests: keys and buttons bound otherwise ("<name> <key> <pad>")
    float                    m_camYaw = 0.0f;           ///< camera turned about the hero (degrees)
    Vector2                  m_move = { 0.0f, 0.0f };   ///< with --shot: the left stick held during the warm-up
    std::string              m_hero;                    ///< corvin, dorra (else the choice in the menu)
    std::string              m_flags;                   ///< what has happened already (comma-separated; tests)
    std::string              m_talk;                    ///< with --shot: a conversation open in the picture
    std::string              m_film;                    ///< a film played over the level (assets/films/<name>)
    int                      m_shop = 0;                ///< with --shot: Ottla's (1) or Kiri's (2) menu open
    int                      m_panel = -1;        ///< with --shot: the inventory (0), character (1) or map (2) page
    int                      m_stash = 0;         ///< with --shot: the first items of the bag into the stash
    int                      m_lootFilter = -1;   ///< tests: the loot filter (-1: as set)
    bool                     m_map = false;       ///< with --shot: the map over the game
    bool                     m_lineup = false;    ///< with --shot: a row of people in their animations before the hero
    int                      m_creatures = -1;    ///< with --shot: the creatures in front of the hero (page 0 or 1)
    int                      m_figures = -1;   ///< with --shot: figures in their poses in front of the hero (page 0, 1)
    std::string              m_pad;   ///< keys, xbox, ps, deck: the buttons shown in the texts (else the device used)
    int                      m_cycle = 0;      ///< tests: a new game+'s cycle
    std::string              m_playLog;        ///< tests: write the play log here
    std::string              m_saveFile;       ///< tests: where a capsule's save goes (instead of save.txt)
    std::string              m_loadFile;       ///< tests: a save to go on from
    int                      m_colours = -1;   ///< tests: colour vision 0..2 (else the setting)
    float                    m_zoom = 0.0f;    ///< with --shot: the camera's distance factor (0: as it is)
    int                      m_loot = 0;       ///< tests: items scattered round the hero at the start
    std::string              m_gear;           ///< tests: what the hero puts on at the start (--gear item.rail3,u12,s5)
    int                      m_xp = 0;         ///< tests: experience given at the start
    unsigned                 m_seed = 0;       ///< a generated level's seed (0: from the progress)
    std::string              m_dumpGen;        ///< write the generated level's file here
    int                      m_heroLevel = 0;   ///< tests: the hero's level at the start
    float                    m_trace = 0.0f;    ///< tests: log the run's state every so many seconds
};

FILE *g_log = nullptr;   ///< the log file next to the program

/** @brief raylib's log, into the file; the noise of optimised-away uniforms left out. */
void logToFile( int level, const char *text, va_list args )
{
    if ( !g_log || level < LOG_WARNING ) return;
    char line[1024];
    vsnprintf( line, sizeof( line ), text, args );
    if ( strstr( line, "Failed to find shader uniform" ) ) return;
    fprintf( g_log, "%s\n", line );
    fflush( g_log );
}

/** @brief Reads the command line. */
Options parse( int argc, char **argv )
{
    Options o;
    for ( int i = 1; i < argc; ++i )
    {
        std::string a = argv[i];
        auto        next = [&]() { return i + 1 < argc ? std::string( argv[++i] ) : std::string(); };
        if ( a == "--shot" )
            o.m_shot = next();
        else if ( a == "--w" )
            o.m_width = atoi( next().c_str() );
        else if ( a == "--h" )
            o.m_height = atoi( next().c_str() );
        else if ( a == "--ss" )
            o.m_ss = (float)atof( next().c_str() );
        else if ( a == "--quality" )
        {
            std::string q = next();
            o.m_quality = q == "low" ? 0 : ( q == "middle" ? 1 : ( q == "high" ? 2 : atoi( q.c_str() ) ) );
        }
        else if ( a == "--at" )
            o.m_hasAt = sscanf( next().c_str(), "%f,%f", &o.m_x, &o.m_z ) == 2;
        else if ( a == "--face" )
            o.m_face = (float)atof( next().c_str() );
        else if ( a == "--walk" )
            o.m_walk = true;
        else if ( a == "--fire" )
            o.m_fire = true;
        else if ( a == "--awake" )
            o.m_awake = true;
        else if ( a == "--autoplay" )
            o.m_autoplay = true;
        else if ( a == "--report" )
            o.m_report = true;
        else if ( a == "--mute" )
            o.m_mute = true;
        else if ( a == "--silent" )
            o.m_silent = true;
        else if ( a == "--lookdev" )
            o.m_lookdev = true;
        else if ( a == "--dumpsound" && i + 1 < argc )
            o.m_dumpSound = argv[++i];
        else if ( a == "--scoretest" && i + 1 < argc )
            o.m_scoreTest = argv[++i];
        else if ( a == "--level" )
            o.m_level = next();
        else if ( a == "--warm" )
            o.m_warm = (float)atof( next().c_str() );
        else if ( a == "--exposure" )
            o.m_exposure = (float)atof( next().c_str() );
        else if ( a == "--bench" )
            o.m_bench = atoi( next().c_str() );
        else if ( a == "--difficulty" )
            o.m_difficulty = next();
        else if ( a == "--menu" )
            o.m_menu = true;
        else if ( a == "--controls" )
            o.m_controls = true;
        else if ( a == "--photo" )
            o.m_photo = next();
        else if ( a == "--bind" )
            o.m_binds.push_back( next() );
        else if ( a == "--lang" )
            o.m_language = next();
        else if ( a == "--hero" )
            o.m_hero = next();
        else if ( a == "--flags" )
            o.m_flags = next();
        else if ( a == "--talk" )
            o.m_talk = next();
        else if ( a == "--film" )
            o.m_film = next();
        else if ( a == "--shop" )
            o.m_shop = atoi( next().c_str() );
        else if ( a == "--camyaw" )
            o.m_camYaw = (float)atof( next().c_str() );
        else if ( a == "--move" )
            sscanf( next().c_str(), "%f,%f", &o.m_move.x, &o.m_move.y );
        else if ( a == "--panel" )
            o.m_panel = atoi( next().c_str() );
        else if ( a == "--stash" )
            o.m_stash = atoi( next().c_str() );
        else if ( a == "--lootfilter" )
            o.m_lootFilter = atoi( next().c_str() );
        else if ( a == "--map" )
            o.m_map = true;
        else if ( a == "--lineup" )
            o.m_lineup = true;
        else if ( a == "--creatures" )
            o.m_creatures = atoi( next().c_str() );
        else if ( a == "--figures" )
            o.m_figures = atoi( next().c_str() );
        else if ( a == "--pad" )
            o.m_pad = next();
        else if ( a == "--cycle" )
            o.m_cycle = atoi( next().c_str() );
        else if ( a == "--playlog" )
            o.m_playLog = next();
        else if ( a == "--savefile" )
            o.m_saveFile = next();
        else if ( a == "--loadfile" )
            o.m_loadFile = next();
        else if ( a == "--colours" )
            o.m_colours = atoi( next().c_str() );
        else if ( a == "--zoom" )
            o.m_zoom = (float)atof( next().c_str() );
        else if ( a == "--loot" )
            o.m_loot = atoi( next().c_str() );
        else if ( a == "--gear" )
            o.m_gear = next();
        else if ( a == "--seed" )
            o.m_seed = (unsigned)strtoul( next().c_str(), nullptr, 10 );
        else if ( a == "--dumpgen" )
            o.m_dumpGen = next();
        else if ( a == "--trace" )
            o.m_trace = (float)atof( next().c_str() );
        else if ( a == "--herolevel" )
            o.m_heroLevel = atoi( next().c_str() );
        else if ( a == "--xp" )
            o.m_xp = atoi( next().c_str() );
    }
    return o;
}

/** @brief The assets folder: beside the program, else the source tree's. */
std::string assetsDir()
{
    std::string beside = std::string( GetApplicationDirectory() ) + "assets";
    if ( DirectoryExists( beside.c_str() ) ) return beside;
    return SPACEOPERA_ASSETS;
}

/** @brief Renders into a target through `draw` and writes it as a PNG. */
void savePicture( Gfx &gfx, const std::string &path, const std::function<void( const RenderTexture2D * )> &draw )
{
    RenderTexture2D rt = LoadRenderTexture( gfx.m_width, gfx.m_height );
    draw( &rt );
    Image img = LoadImageFromTexture( rt.texture );
    ImageFlipVertical( &img );
    ExportImage( img, path.c_str() );
    UnloadImage( img );
    UnloadRenderTexture( rt );
}

/** @brief Renders `frames` frames and logs the time per frame (the read-back waits for the GPU). */
void bench( Gfx &gfx, int frames, const Options &o, const std::function<void( const RenderTexture2D * )> &draw )
{
    RenderTexture2D rt = LoadRenderTexture( gfx.m_width, gfx.m_height );
    double          start = GetTime();
    for ( int i = 0; i < frames; ++i ) draw( &rt );
    Image img = LoadImageFromTexture( rt.texture );
    TraceLog( LOG_WARNING, "BENCH: %dx%d quality %d ss %.2f: %.2f ms per frame", o.m_width, o.m_height, gfx.m_quality,
              gfx.m_scale, ( GetTime() - start ) * 1000.0 / frames );
    UnloadImage( img );
    UnloadRenderTexture( rt );
}

/**
 * @brief The player's input from keyboard, mouse and gamepad (one stick moves, the other turns the camera, as on the
 *        Steam Deck; the rifle aims itself, the mouse can aim on the PC).
 */
GameInput readInput( Vector2 &lastMouse, float dt )
{
    // The actions as the player has bound them (Bindings.h); the arrows walk too, Enter and Start confirm.
    GameInput in;
    if ( held( BindE::UP ) || IsKeyDown( KEY_UP ) ) in.m_move.y -= 1.0f;
    if ( held( BindE::DOWN ) || IsKeyDown( KEY_DOWN ) ) in.m_move.y += 1.0f;
    if ( held( BindE::LEFT ) || IsKeyDown( KEY_LEFT ) ) in.m_move.x -= 1.0f;
    if ( held( BindE::RIGHT ) || IsKeyDown( KEY_RIGHT ) ) in.m_move.x += 1.0f;
    in.m_fire = held( BindE::FIRE );
    in.m_jump = pressed( BindE::JUMP );
    in.m_stim = pressed( BindE::STIM );
    in.m_special = pressed( BindE::SPECIAL );
    in.m_use = pressed( BindE::USE );
    in.m_map = pressed( BindE::MAP );
    in.m_skill2 = pressed( BindE::SKILL2 );
    in.m_skill3 = pressed( BindE::SKILL3 );
    in.m_confirm = IsKeyPressed( KEY_ENTER );
    in.m_strafe = held( BindE::STRAFE );
    in.m_cameraReset = pressed( BindE::CAMRESET );
    in.m_dodge = pressed( BindE::DODGE );
    Vector2 mouse = GetMousePosition();
    in.m_mouse = mouse;
    if ( IsMouseButtonDown( MOUSE_BUTTON_RIGHT ) )
        in.m_cameraTurn += ( mouse.x - lastMouse.x ) * 0.006f;   // dragging turns the camera and does not aim
    else
        in.m_mouseAim = Vector2Distance( mouse, lastMouse ) > 2.0f || IsMouseButtonDown( MOUSE_BUTTON_LEFT );
    lastMouse = mouse;
    if ( IsGamepadAvailable( 0 ) )
    {
        Vector2 stick = { GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_X ),
                          GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_Y ) };
        if ( Vector2Length( stick ) > 0.2f )
        {
            in.m_move = Vector2Add( in.m_move, stick );
            in.m_mouseAim = false;
        }
        float turn = GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_X );
        if ( fabsf( turn ) > 0.2f ) in.m_cameraTurn += ( turn - copysignf( 0.2f, turn ) ) / 0.8f * 2.4f * dt;
        in.m_confirm = in.m_confirm || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT );
        // The cross's up and down also step through the game's own menus (talks, shops, the lift).
        if ( IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_FACE_UP ) ) in.m_navStep = -1;
        if ( IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_FACE_DOWN ) ) in.m_navStep = 1;
    }
    return in;
}

/** @brief Zoom from the mouse wheel and the right stick's up and down (the shoulder buttons are skills now). */
float zoomStep( float dt )
{
    float zoom = -GetMouseWheelMove() * 0.08f;
    if ( IsGamepadAvailable( 0 ) )
    {
        float pull = GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_Y );
        if ( fabsf( pull ) > 0.3f ) zoom += pull * 0.8f * dt;
    }
    return zoom;
}

/** @brief The look test (the room of 25.10.2026). */
int runLookdev( const Options &o, Gfx &gfx )
{
    Scene scene;
    scene.load( gfx, assetsDir() );
    auto draw = [&]( const RenderTexture2D *rt ) { scene.render( gfx, rt ); };
    if ( !o.m_shot.empty() )
    {
        const float dt = 1.0f / 60.0f;
        float       yaw = o.m_face * DEG2RAD;
        Vector2     dir = { sinf( yaw ), cosf( yaw ) };
        int         frames = (int)( o.m_warm * 60.0f );
        float       back = o.m_walk ? 2.4f : 0.0f;
        scene.putHero( ( o.m_hasAt ? o.m_x : -3.0f ) - dir.x * back, ( o.m_hasAt ? o.m_z : 1.0f ) - dir.y * back,
                       o.m_face );
        for ( int i = 0; i < frames; ++i )
        {
            Controls c;
            if ( o.m_walk && i >= frames - 36 ) c.m_move = dir;
            if ( o.m_fire && i == frames - 9 ) scene.fire();
            scene.update( dt, c );
        }
        if ( o.m_bench > 0 ) bench( gfx, o.m_bench, o, draw );
        savePicture( gfx, o.m_shot, draw );
        return 0;
    }
    bool showFps = false;
    int  shots = 0;
    while ( !WindowShouldClose() )
    {
        if ( IsKeyPressed( KEY_ESCAPE ) ) break;
        float    dt = fminf( GetFrameTime(), 0.05f );
        Controls c;
        if ( IsKeyDown( KEY_W ) || IsKeyDown( KEY_UP ) ) c.m_move.y -= 1.0f;
        if ( IsKeyDown( KEY_S ) || IsKeyDown( KEY_DOWN ) ) c.m_move.y += 1.0f;
        if ( IsKeyDown( KEY_A ) || IsKeyDown( KEY_LEFT ) ) c.m_move.x -= 1.0f;
        if ( IsKeyDown( KEY_D ) || IsKeyDown( KEY_RIGHT ) ) c.m_move.x += 1.0f;
        c.m_fire = IsKeyDown( KEY_SPACE ) || IsMouseButtonDown( MOUSE_BUTTON_LEFT );
        scene.update( dt, c );
        scene.m_zoom = Clamp( scene.m_zoom * ( 1.0f + zoomStep( dt ) ), 0.55f, 1.7f );
        if ( IsKeyPressed( KEY_F1 ) ) showFps = !showFps;
        if ( IsKeyPressed( KEY_F12 ) )
            savePicture( gfx, std::string( GetApplicationDirectory() ) + TextFormat( "shot_%03d.png", ++shots ), draw );
        BeginDrawing();
        scene.render( gfx, nullptr );
        if ( showFps ) DrawFPS( 10, 10 );
        EndDrawing();
    }
    return 0;
}

/** @brief What the player set in the menus; kept in settings.txt beside the program. */
struct Settings
{
    DifficultyE m_difficulty = DifficultyE::NORMAL;   ///< how hard
    float       m_brightness = 1.0f;                  ///< exposure factor
    float       m_music = 0.7f;                       ///< music volume 0..1
    float       m_voices = 0.9f;                      ///< voices' volume 0..1
    float       m_effects = 0.8f;                     ///< effects' and backgrounds' volume 0..1
    int         m_quality = -1;                       ///< the graphics: 0 low, 1 middle, 2 high (-1: not chosen yet)
    int         m_subtitles = 1;                      ///< 0 small, 1 normal, 2 large
    int         m_colours = 0;                        ///< colour vision: 0 as it is, 1 red-green, 2 blue-yellow
    int         m_shake = 2;                          ///< the camera's shake: 0 off, 1 half, 2 full
    int         m_loot = 0;                           ///< the loot filter: 0 keep all, 1 salvage common, 2 and magic
    bool        m_nightmare = false;                  ///< Albtraum may be chosen (the game is through once)
    std::string m_language;                           ///< the texts' language
};

/** @brief Whether the game runs on a Steam Deck (Steam sets SteamDeck=1 there). */
bool steamDeck()
{
    const char *deck = getenv( "SteamDeck" );
    return deck && atoi( deck ) == 1;
}

/** @brief The graphics' quality to start with: the chosen one, else low on a Steam Deck and high elsewhere. */
int qualityOf( const Settings &s )
{
    return s.m_quality >= 0 ? s.m_quality : ( steamDeck() ? 0 : 2 );
}

/** @brief Where the settings live. */
std::string settingsPath()
{
    return userDir() + "settings.txt";
}

/** @brief Reads the settings (defaults if there are none; the language at first the system's). */
Settings loadSettings()
{
    Settings s;
    s.m_language = systemLanguage( assetsDir() );
    FILE *f = fopen( settingsPath().c_str(), "r" );
    if ( !f ) return s;
    char line[256];
    while ( fgets( line, sizeof( line ), f ) )
    {
        char key[32] = {}, value[200] = {};
        if ( sscanf( line, "%31s %199[^\r\n]", key, value ) != 2 ) continue;
        if ( !strcmp( key, "bind" ) )
            readBinding( value );
        else if ( !strcmp( key, "difficulty" ) )
            s.m_difficulty = (DifficultyE)Clamp( (float)atoi( value ), 0.0f, 3.0f );
        else if ( !strcmp( key, "brightness" ) )
            s.m_brightness = Clamp( (float)atof( value ), 0.6f, 2.0f );
        else if ( !strcmp( key, "language" ) )
            s.m_language = value;
        else if ( !strcmp( key, "music" ) )
            s.m_music = Clamp( (float)atof( value ), 0.0f, 1.0f );
        else if ( !strcmp( key, "voices" ) )
            s.m_voices = Clamp( (float)atof( value ), 0.0f, 1.0f );
        else if ( !strcmp( key, "effects" ) )
            s.m_effects = Clamp( (float)atof( value ), 0.0f, 1.0f );
        else if ( !strcmp( key, "graphics" ) )
            s.m_quality = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "subtitles" ) )
            s.m_subtitles = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "colours" ) )
            s.m_colours = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "shake" ) )
            s.m_shake = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "loot" ) )
            s.m_loot = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "nightmare" ) )
            s.m_nightmare = atoi( value ) != 0;
    }
    fclose( f );
    return s;
}

/** @brief Where the game is saved (the start of the level to play). */
std::string savePath()
{
    return userDir() + "save.txt";
}

/** @brief Reads the save; false if there is none. */
bool loadProgress( Progress &p, const std::string &path = savePath() )
{
    // A save cut short (the power gone while it was written) gives way to the one before it.
    bool  backup = path.size() >= 4 && path.compare( path.size() - 4, 4, ".bak" ) == 0;
    FILE *f = fopen( path.c_str(), "r" );
    if ( !f ) return !backup && loadProgress( p, path + ".bak" );
    char line[1024], key[32], value[128];
    p = Progress();
    while ( fgets( line, sizeof( line ), f ) )
    {
        if ( sscanf( line, "%31s %127s", key, value ) != 2 ) continue;
        const char *rest = strchr( line, ' ' ) ? strchr( line, ' ' ) + 1 : "";
        if ( !strcmp( key, "gear" ) )
        {
            // gear <slot> <item>
            int  slot = atoi( value );
            Item it;
            if ( slot >= 0 && slot < EquipSlots && strchr( rest, ' ' ) && loadItem( strchr( rest, ' ' ) + 1, it ) )
                p.m_gear[slot] = it, p.m_worn[slot] = true;
        }
        else if ( !strcmp( key, "bag" ) )
        {
            Item it;
            if ( loadItem( rest, it ) ) p.m_bag.push_back( it );
        }
        else if ( !strcmp( key, "rank" ) )
        {
            int skill = 0, rank = 0;
            if ( sscanf( rest, "%d %d", &skill, &rank ) == 2 && skill >= 0 && skill < 64 )
            {
                if ( (int)p.m_ranks.size() <= skill ) p.m_ranks.resize( skill + 1, 0 );
                p.m_ranks[skill] = rank;
            }
        }
        else if ( !strcmp( key, "xp" ) )
            p.m_xp = atoi( value );
        else if ( !strcmp( key, "herolevel" ) )
            p.m_heroLevel = std::max( 1, atoi( value ) );
        else if ( !strcmp( key, "points" ) )
            p.m_points = atoi( value );
        else if ( !strcmp( key, "seed" ) )
            p.m_seed = atoi( value );
        else if ( !strcmp( key, "deaths" ) )
            p.m_deaths = atoi( value );
        else if ( !strcmp( key, "cycle" ) )
            p.m_cycle = atoi( value );
        else if ( !strcmp( key, "spot" ) )
            p.m_hasSpot = sscanf( rest, "%f %f %f", &p.m_spot.x, &p.m_spot.y, &p.m_spot.z ) == 3;
        else if ( !strcmp( key, "fired" ) )
            p.m_fired.insert( value );
        else if ( !strcmp( key, "hero" ) )
            p.m_hero =
                !strcmp( value, "dorra" ) ? HeroE::DORRA : ( !strcmp( value, "ione" ) ? HeroE::IONE : HeroE::CORVIN );
        else if ( !strcmp( key, "level" ) )
            p.m_level = value;
        else if ( !strcmp( key, "scrap" ) )
            p.m_scrap = atoi( value );
        else if ( !strcmp( key, "stims" ) )
            p.m_stims = atoi( value );
        else if ( !strcmp( key, "weapon" ) )
            p.m_weapon = atoi( value );
        else if ( !strcmp( key, "armor" ) )
            p.m_armor = atoi( value );
        else if ( !strcmp( key, "flag" ) )
            p.m_flags.insert( value );
    }
    fclose( f );
    bool whole = !p.m_level.empty() && FileExists( ( assetsDir() + "/levels/" + p.m_level ).c_str() );
    return whole || ( !backup && loadProgress( p, path + ".bak" ) );
}

/** @brief Writes the save. */
void saveProgress( const Progress &p, const std::string &path = savePath() )
{
    FILE *f = beginWrite( path );
    if ( !f ) return;
    fprintf( f, "hero %s\nlevel %s\nscrap %d\nstims %d\nweapon %d\narmor %d\n", heroKey( p.m_hero ), p.m_level.c_str(),
             p.m_scrap, p.m_stims, p.m_weapon, p.m_armor );
    fprintf( f, "xp %d\nherolevel %d\npoints %d\nseed %d\ndeaths %d\ncycle %d\n", p.m_xp, p.m_heroLevel, p.m_points,
             p.m_seed, p.m_deaths, p.m_cycle );
    if ( p.m_hasSpot )
    {
        fprintf( f, "spot %.2f %.2f %.2f\n", p.m_spot.x, p.m_spot.y, p.m_spot.z );
        for ( const std::string &e : p.m_fired ) fprintf( f, "fired %s\n", e.c_str() );
    }
    for ( size_t i = 0; i < p.m_ranks.size(); ++i )
        if ( p.m_ranks[i] > 0 ) fprintf( f, "rank %d %d\n", (int)i, p.m_ranks[i] );
    for ( int i = 0; i < EquipSlots; ++i )
        if ( p.m_worn[i] ) fprintf( f, "gear %d %s\n", i, saveItem( p.m_gear[i] ).c_str() );
    for ( const Item &it : p.m_bag ) fprintf( f, "bag %s\n", saveItem( it ).c_str() );
    for ( const std::string &flag : p.m_flags ) fprintf( f, "flag %s\n", flag.c_str() );
    endWrite( f, path );
}

/** @brief Writes the settings. */
void saveSettings( const Settings &s )
{
    FILE *f = beginWrite( settingsPath() );
    if ( !f ) return;
    fprintf( f, "difficulty %d\nbrightness %.2f\nlanguage %s\nmusic %.1f\nvoices %.1f\neffects %.1f\n",
             (int)s.m_difficulty, s.m_brightness, s.m_language.c_str(), s.m_music, s.m_voices, s.m_effects );
    if ( s.m_quality >= 0 ) fprintf( f, "graphics %d\n", s.m_quality );
    fprintf( f, "subtitles %d\ncolours %d\nshake %d\nloot %d\nnightmare %d\n", s.m_subtitles, s.m_colours, s.m_shake,
             s.m_loot, s.m_nightmare ? 1 : 0 );
    for ( int b = 0; b < (int)BindE::COUNT; ++b ) fprintf( f, "bind %s\n", bindingLine( (BindE)b ).c_str() );
    endWrite( f, settingsPath() );
}

/** @brief Navigation in a menu: up, down, left, right, confirm, back (keyboard, gamepad, left stick). */
struct MenuKeys
{
    int  m_move = 0;          ///< -1 up, 1 down
    int  m_change = 0;        ///< -1 left, 1 right
    bool m_confirm = false;   ///< Enter, Space, A
    bool m_back = false;      ///< Esc, B, Start
};

/** @brief Reads the menu keys; the stick repeats slowly. */
MenuKeys menuKeys( float dt )
{
    static float repeat = 0.0f;
    MenuKeys     k;
    if ( IsKeyPressed( KEY_UP ) || IsKeyPressed( KEY_W ) ) k.m_move = -1;
    if ( IsKeyPressed( KEY_DOWN ) || IsKeyPressed( KEY_S ) ) k.m_move = 1;
    if ( IsKeyPressed( KEY_LEFT ) || IsKeyPressed( KEY_A ) ) k.m_change = -1;
    if ( IsKeyPressed( KEY_RIGHT ) || IsKeyPressed( KEY_D ) ) k.m_change = 1;
    k.m_confirm = IsKeyPressed( KEY_ENTER ) || IsKeyPressed( KEY_SPACE );
    k.m_back = IsKeyPressed( KEY_ESCAPE );
    if ( IsGamepadAvailable( 0 ) )
    {
        if ( IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_FACE_UP ) ) k.m_move = -1;
        if ( IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_FACE_DOWN ) ) k.m_move = 1;
        if ( IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_FACE_LEFT ) ) k.m_change = -1;
        if ( IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT ) ) k.m_change = 1;
        k.m_confirm = k.m_confirm || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN );
        k.m_back = k.m_back || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT ) ||
                   IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT );
        float x = GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_X ),
              y = GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_Y );
        repeat -= dt;
        if ( fabsf( x ) < 0.5f && fabsf( y ) < 0.5f )
            repeat = 0.0f;
        else if ( repeat <= 0.0f )
        {
            if ( fabsf( y ) > fabsf( x ) )
                k.m_move = y < 0.0f ? -1 : 1;
            else
                k.m_change = x < 0.0f ? -1 : 1;
            repeat = 0.25f;
        }
    }
    return k;
}

/** @brief Draws a menu over the frame: a title, a subtitle, items, the chosen one marked. */
void drawMenu( const Font &font, const char *title, const char *subtitle, const std::vector<std::string> &items,
               int chosen )
{
    int   w = GetScreenWidth(), h = GetScreenHeight();
    float s = h / 1080.0f;
    DrawRectangle( 0, 0, w, h, { 0, 0, 0, 150 } );
    auto text = [&]( const char *t, float y, float size, Color c )
    {
        Vector2 m = MeasureTextEx( font, t, size * s, 1.0f );
        DrawTextEx( font, t, { w * 0.5f - m.x * 0.5f + 2 * s, y + 2 * s }, size * s, 1.0f, { 0, 0, 0, 200 } );
        DrawTextEx( font, t, { w * 0.5f - m.x * 0.5f, y }, size * s, 1.0f, c );
    };
    text( title, h * 0.2f, 84, { 245, 236, 222, 255 } );
    text( subtitle, h * 0.2f + 96 * s, 28, { 255, 190, 120, 230 } );
    for ( size_t i = 0; i < items.size(); ++i )
    {
        bool  on = (int)i == chosen;
        float y = h * 0.36f + i * fminf( 46.0f, 470.0f / items.size() ) * s;   // above the game's subtitles
        if ( on )
            DrawRectangle( (int)( w * 0.5f - 260 * s ), (int)( y - 6 * s ), (int)( 520 * s ), (int)( 44 * s ),
                           { 255, 170, 80, 40 } );
        text( items[i].c_str(), y, 34, on ? Color{ 255, 205, 140, 255 } : Color{ 200, 200, 200, 220 } );
    }
    text( tr( "menu.help" ), h - 70 * s, 20, { 150, 150, 150, 200 } );
}

/**
 * @brief Draws the controls' page: per action its key and its pad button; then the defaults and back.
 * @param[in] font     the font
 * @param[in] chosen   the chosen row
 * @param[in] waiting  the chosen action waits for its new key or button
 */
void drawControls( const Font &font, int chosen, bool waiting )
{
    int   w = GetScreenWidth(), h = GetScreenHeight();
    float s = h / 1080.0f;
    DrawRectangle( 0, 0, w, h, { 0, 0, 0, 200 } );
    auto text = [&]( const std::string &t, float x, float y, float size, Color c, int align )
    {
        Vector2 m = MeasureTextEx( font, t.c_str(), size * s, 1.0f );
        float   at = align == 1 ? x - m.x * 0.5f : align == 2 ? x - m.x : x;
        DrawTextEx( font, t.c_str(), { at + 2 * s, y + 2 * s }, size * s, 1.0f, { 0, 0, 0, 200 } );
        DrawTextEx( font, t.c_str(), { at, y }, size * s, 1.0f, c );
    };
    text( tr( "menu.controls" ), w * 0.5f, 50 * s, 60, { 245, 236, 222, 255 }, 1 );
    float name = w * 0.5f - 430 * s, key = w * 0.5f + 150 * s, pad = w * 0.5f + 400 * s, top = 150 * s, row = 34 * s;
    text( tr( "controls.key" ), key, top - 40 * s, 22, { 255, 170, 90, 230 }, 1 );
    text( tr( "controls.pad" ), pad, top - 40 * s, 22, { 255, 170, 90, 230 }, 1 );
    int count = (int)BindE::COUNT + 2;
    for ( int i = 0; i < count; ++i )
    {
        float y = top + i * row + ( i >= (int)BindE::COUNT ? 16 * s : 0.0f );
        bool  on = i == chosen;
        if ( on )
            DrawRectangle( (int)( name - 16 * s ), (int)( y - 4 * s ), (int)( pad - name + 140 * s ), (int)( 32 * s ),
                           { 255, 170, 80, 40 } );
        Color c = on ? Color{ 255, 205, 140, 255 } : Color{ 200, 200, 200, 220 };
        if ( i >= (int)BindE::COUNT )
        {
            text( tr( i == (int)BindE::COUNT ? "controls.defaults" : "menu.back" ), name, y, 26, c, 0 );
            continue;
        }
        const Binding &b = binding( (BindE)i );
        text( tr( std::string( "bind." ) + bindName( (BindE)i ) ), name, y, 26, c, 0 );
        if ( on && waiting )
            text( tr( "controls.press" ), ( key + pad ) * 0.5f, y, 26, { 255, 220, 120, 255 }, 1 );
        else
        {
            text( keyLabel( b.m_key ), key, y, 26, c, 1 );
            text( padLabel( b.m_pad ), pad, y, 26, c, 1 );
        }
    }
    text( tr( "controls.help" ), w * 0.5f, h - 70 * s, 20, { 150, 150, 150, 200 }, 1 );
}

/** @brief A brightness bar as text. */
std::string brightnessBar( float b )
{
    int         n = (int)roundf( ( b - 0.6f ) / 0.1f );
    std::string bar = std::string( tr( "menu.brightness" ) ) + "  ";
    for ( int i = 0; i <= 14; ++i ) bar += i <= n ? "|" : ".";
    return bar;
}

/** @brief A volume as a bar (music, voices, effects). */
std::string volumeBar( const char *key, float v )
{
    int         n = (int)roundf( v * 10.0f );
    std::string bar = std::string( tr( key ) ) + "  ";
    for ( int i = 1; i <= 10; ++i ) bar += i <= n ? "|" : ".";
    return bar;
}

/** @brief What a menu row does. */
enum class RowE
{
    CONTINUE,
    PLUS,
    START,
    RESUME,
    OPTIONS,
    RESTART,
    QUIT,
    DIFFICULTY,
    BRIGHTNESS,
    GRAPHICS,
    MUSIC,
    VOICES,
    EFFECTS,
    LANGUAGE,
    SUBTITLES,
    COLOURS,
    SHAKE,
    LOOT,
    CONTROLS,
    PHOTO,
    BACK
};

/**
 * @brief A menu's rows. Title: continue, a new game+ (once the game is through), start, options, quit. Pause: resume,
 *        options, restart, quit. Options: difficulty, brightness, graphics, the three volumes, language, subtitles,
 *        colour vision, shake, back.
 * @param[in] menu   0 title, 1 pause, 2 options
 * @param[in] saved  there is a save
 * @param[in] plus   the save has the game through
 * @return the rows
 */
std::vector<RowE> menuRows( int menu, bool saved, bool plus )
{
    using R = RowE;
    if ( menu == 2 )
        return { R::DIFFICULTY, R::BRIGHTNESS, R::GRAPHICS, R::MUSIC, R::VOICES,   R::EFFECTS, R::LANGUAGE,
                 R::SUBTITLES,  R::COLOURS,    R::SHAKE,    R::LOOT,  R::CONTROLS, R::BACK };
    if ( menu == 1 ) return { R::RESUME, R::PHOTO, R::OPTIONS, R::RESTART, R::QUIT };
    std::vector<RowE> rows;
    if ( saved ) rows.push_back( R::CONTINUE );
    if ( plus ) rows.push_back( R::PLUS );
    for ( R r : { R::START, R::OPTIONS, R::QUIT } ) rows.push_back( r );
    return rows;
}

/** @brief A row as it reads (a setting with its value). */
std::string rowText( RowE r, const Settings &s, int cycle )
{
    auto pick = []( const char *key, const std::string &value )
    { return std::string( tr( key ) ) + "  " + tr( value ); };
    switch ( r )
    {
    case RowE::CONTINUE: return tr( "menu.continue" );
    case RowE::PLUS: return TextFormat( tr( "menu.plus" ), cycle + 1 );
    case RowE::START: return tr( "menu.start" );
    case RowE::RESUME: return tr( "menu.resume" );
    case RowE::OPTIONS: return tr( "menu.options" );
    case RowE::RESTART: return tr( "menu.restart" );
    case RowE::QUIT: return tr( "menu.quit" );
    case RowE::DIFFICULTY: return std::string( tr( "menu.difficulty" ) ) + "  " + difficultyName( s.m_difficulty );
    case RowE::BRIGHTNESS: return brightnessBar( s.m_brightness );
    case RowE::GRAPHICS: return pick( "menu.graphics", "graphics." + std::to_string( qualityOf( s ) ) );
    case RowE::MUSIC: return volumeBar( "menu.music", s.m_music );
    case RowE::VOICES: return volumeBar( "menu.voices", s.m_voices );
    case RowE::EFFECTS: return volumeBar( "menu.effects", s.m_effects );
    case RowE::LANGUAGE: return std::string( tr( "menu.language" ) ) + "  " + tr( "language.name" );
    case RowE::SUBTITLES: return pick( "menu.subtitles", "subtitles." + std::to_string( s.m_subtitles ) );
    case RowE::COLOURS: return pick( "menu.colours", "colours." + std::to_string( s.m_colours ) );
    case RowE::SHAKE: return pick( "menu.shake", "shake." + std::to_string( s.m_shake ) );
    case RowE::LOOT: return pick( "menu.loot", "lootfilter." + std::to_string( s.m_loot ) );
    case RowE::CONTROLS: return tr( "menu.controls" );
    case RowE::PHOTO: return tr( "menu.photo" );
    default: return tr( "menu.back" );
    }
}

/** @brief The rows' texts. */
std::vector<std::string> rowTexts( const std::vector<RowE> &rows, const Settings &s, int cycle )
{
    std::vector<std::string> items;
    for ( RowE r : rows ) items.push_back( rowText( r, s, cycle ) );
    return items;
}

/**
 * @brief The music's test, silent: mixes a scripted stretch of play into a WAV (the title; a level calm and tense;
 *        its fight; a boss in both halves; the stingers; a long calm stretch in which the takes change; silence).
 * @param[in] path  the WAV
 */
int runScoreTest( const std::string &path )
{
    Score score;
    score.init( assetsDir() + "/music", true, false );
    std::vector<std::string> ids = { "title", "kuehlgedaerme", "kuehlgedaerme_fight", "boss", "victory", "death" };
    score.keep( ids );
    for ( const std::string &id : ids )
        for ( int i = 0; i < 600 && !score.has( id ); ++i ) WaitTime( 0.05 );
    struct Step
    {
        float       m_at;
        const char *m_cue;
        float       m_heat;
        const char *m_sting;
    };
    const Step         steps[] = { { 0.0f, "title", 0.0f, nullptr },
                                   { 10.0f, "kuehlgedaerme", 0.0f, nullptr },
                                   { 30.0f, "kuehlgedaerme", 1.0f, nullptr },
                                   { 45.0f, "kuehlgedaerme_fight", 1.0f, nullptr },
                                   { 60.0f, "boss", 0.0f, nullptr },
                                   { 75.0f, "boss", 1.0f, nullptr },
                                   { 90.0f, "kuehlgedaerme", 0.0f, "victory" },
                                   { 100.0f, "kuehlgedaerme", 0.0f, "death" },
                                   { 230.0f, "", 0.0f, nullptr } };
    const int          perFrame = 800;   // 1/60 s at 48 kHz
    const float        dt = 1.0f / 60.0f, length = 240.0f;
    std::vector<float> mixed( perFrame * 2 );
    std::vector<short> out;
    int                step = -1;
    for ( float t = 0.0f; t < length; t += dt )
    {
        int now = step;
        while ( now + 1 < (int)( sizeof( steps ) / sizeof( steps[0] ) ) && steps[now + 1].m_at <= t ) ++now;
        if ( now != step && steps[now].m_sting ) score.sting( steps[now].m_sting );
        step = now;
        score.update( dt, steps[step].m_cue, steps[step].m_heat );
        score.mix( mixed.data(), perFrame );
        for ( float v : mixed ) out.push_back( (short)( v * 32767.0f ) );
    }
    Wave w = { (unsigned)( out.size() / 2 ), 48000, 16, 2, out.data() };
    bool ok = ExportWave( w, path.c_str() );
    score.shutdown();
    return ok ? 0 : 1;
}

/** @brief The level. */
int runGame( const Options &o, Gfx &gfx )
{
    bool     sound = ( !o.m_mute && o.m_shot.empty() ) || o.m_silent;
    Settings settings = loadSettings();
    for ( const std::string &line : o.m_binds ) readBinding( line );
    if ( !o.m_playLog.empty() ) playlog::open( o.m_playLog );
    // The journal (awards, the codex) only for a player: tests count in memory and keep nothing.
    if ( o.m_shot.empty() && !o.m_autoplay )
    {
        journal::load( userDir() + "journal.txt" );
        playlog::open( userDir() + "playlog.txt" );
    }
    // The device is opened once: the music plays on across levels and menus.
    if ( sound ) InitAudioDevice();
    Score score;
    score.init( assetsDir() + "/music", sound );
    score.m_volume = settings.m_music;
    if ( !o.m_language.empty() ) settings.m_language = o.m_language;
    if ( !loadTexts( assetsDir(), settings.m_language ) ) settings.m_language = "de";
    HeroE       hero = o.m_hero == "dorra" ? HeroE::DORRA : ( o.m_hero == "ione" ? HeroE::IONE : HeroE::CORVIN );
    std::string level = o.m_level;
    Progress    progress, saved;
    bool        hasSave = o.m_shot.empty() && loadProgress( saved );
    progress.m_hero = hero;
    progress.m_level = level;
    for ( size_t at = 0; at < o.m_flags.size(); )
    {
        size_t comma = o.m_flags.find( ',', at );
        if ( comma == std::string::npos ) comma = o.m_flags.size();
        progress.m_flags.insert( o.m_flags.substr( at, comma - at ) );
        at = comma + 1;
    }
    auto game = std::make_unique<Game>();
    game->m_heroKind = hero;
    if ( o.m_heroLevel > 1 ) progress.m_heroLevel = o.m_heroLevel, progress.m_points = o.m_heroLevel;
    progress.m_cycle = o.m_cycle;
    if ( !o.m_loadFile.empty() && loadProgress( progress, o.m_loadFile ) )
        level = progress.m_level, hero = progress.m_hero, game->m_heroKind = hero;
    if ( o.m_colours >= 0 ) settings.m_colours = o.m_colours;
    if ( o.m_lootFilter >= 0 ) settings.m_loot = o.m_lootFilter;
    game->m_progress = progress;
    game->m_genSeed = o.m_seed;
    if ( !game->load( gfx, assetsDir(), level, sound ) )
    {
        TraceLog( LOG_ERROR, "LEVEL: %s not found", level.c_str() );
        return 1;
    }
    {
        std::vector<std::string> pieces = game->musicPieces();
        pieces.push_back( "title" );
        score.keep( pieces );
    }
    if ( o.m_silent ) SetMasterVolume( 0.0f );
    if ( !o.m_dumpGen.empty() && !game->generated().empty() )
        SaveFileText( o.m_dumpGen.c_str(), const_cast<char *>( game->generated().c_str() ) );
    if ( !o.m_difficulty.empty() )
        settings.m_difficulty = o.m_difficulty == "leicht"
                                    ? DifficultyE::EASY
                                    : ( o.m_difficulty == "schwer" ? DifficultyE::HARD : DifficultyE::NORMAL );
    // What the settings ask of a game and the renderer.
    auto apply = [&]()
    {
        game->setDifficulty( settings.m_difficulty );
        game->m_brightness = settings.m_brightness;
        game->setVolumes( settings.m_voices, settings.m_effects );
        game->m_subtitleScale = settings.m_subtitles == 0 ? 0.8f : ( settings.m_subtitles == 2 ? 1.3f : 1.0f );
        game->m_shakeScale = settings.m_shake * 0.5f;
        game->m_lootFilter = settings.m_loot;
        gfx.m_colourVision = settings.m_colours;
        // The shared stash: only for real play (tests keep theirs in memory).
        if ( o.m_shot.empty() && !o.m_autoplay && game->m_stashPath.empty() )
            game->loadStash( userDir() + "stash.txt" );
    };
    apply();
    game->m_autoplay = o.m_autoplay;
    game->m_camYaw = o.m_camYaw * DEG2RAD;
    if ( o.m_hasAt ) game->putHero( o.m_x, o.m_z, o.m_face );
    if ( o.m_lineup ) game->lineup();
    if ( o.m_creatures >= 0 ) game->creatureLineup( o.m_creatures );
    if ( o.m_figures >= 0 ) game->figureLineup( o.m_figures );
    if ( o.m_zoom > 0.0f ) game->m_zoom = o.m_zoom;
    if ( o.m_loot > 0 || o.m_xp > 0 ) game->testGifts( o.m_loot, o.m_xp );
    if ( !o.m_gear.empty() ) game->testGear( o.m_gear );
    if ( o.m_awake ) game->wakeAll();
    if ( !o.m_film.empty() ) game->startFilm( o.m_film );
    auto draw = [&]( const RenderTexture2D *rt ) { game->render( gfx, rt ); };

    if ( !o.m_shot.empty() )
    {
        const float dt = 1.0f / 60.0f;
        int         frames = (int)( o.m_warm * 60.0f );
        for ( int i = 0; i < frames && !( o.m_report && game->finished() ); ++i )
        {
            GameInput in;
            in.m_fire = o.m_fire && i >= frames - 20;
            in.m_move = o.m_move;
            in.m_map = o.m_map && i == 0;
            game->update( dt, in );
            if ( game->takeCheckpoint() && !o.m_saveFile.empty() ) saveProgress( game->checkpoint(), o.m_saveFile );
            for ( const std::string &s : game->takeStings() ) score.sting( s );
            score.update( dt, game->musicCue(), game->musicHeat() );
            if ( o.m_trace > 0.0f && i % std::max( 1, (int)( o.m_trace * 60.0f ) ) == 0 )
                TraceLog( LOG_WARNING, "TRACE: %s music=%s/%.2f", game->report().c_str(), game->musicCue().c_str(),
                          game->musicHeat() );
        }
        if ( o.m_report ) TraceLog( LOG_WARNING, "RUN: %s", game->report().c_str() );
        if ( !o.m_talk.empty() || o.m_shop ) game->openForPicture( o.m_talk, o.m_shop );
        if ( o.m_bench > 0 ) bench( gfx, o.m_bench, o, draw );
        if ( !o.m_photo.empty() )
        {
            Game::PhotoInput pin;
            sscanf( o.m_photo.c_str(), "%f,%f,%f", &pin.m_turn, &pin.m_tilt, &pin.m_zoom );
            game->startPhoto();
            game->photo( 0.0f, pin );
        }
        for ( int i = 0; i < o.m_stash && !game->m_progress.m_bag.empty(); ++i )
        {
            game->m_stash.push_back( game->m_progress.m_bag.front() );
            game->m_progress.m_bag.erase( game->m_progress.m_bag.begin() );
        }
        if ( o.m_panel >= 0 )
            savePicture( gfx, o.m_shot,
                         [&]( const RenderTexture2D *rt )
                         {
                             game->render( gfx, rt );
                             BeginTextureMode( *rt );
                             game->drawPanel( rt->texture.width, rt->texture.height, o.m_panel, 0 );
                             EndTextureMode();
                         } );
        else if ( o.m_controls )
            savePicture( gfx, o.m_shot,
                         [&]( const RenderTexture2D *rt )
                         {
                             game->render( gfx, rt );
                             BeginTextureMode( *rt );
                             drawControls( game->font(), 4, false );
                             EndTextureMode();
                         } );
        else if ( o.m_menu )
        {
            // The title menu over the frame (to look at it without a window).
            std::vector<std::string> items = rowTexts( menuRows( 0, false, false ), settings, 0 );
            savePicture( gfx, o.m_shot,
                         [&]( const RenderTexture2D *rt )
                         {
                             game->render( gfx, rt );
                             BeginTextureMode( *rt );
                             drawMenu( game->font(), "DARK CONCORDANCE", game->title().c_str(), items, 0 );
                             EndTextureMode();
                         } );
        }
        else
            savePicture( gfx, o.m_shot, draw );
        game->shutdown();
        score.shutdown();
        if ( IsAudioDeviceReady() ) CloseAudioDevice();
        return 0;
    }

    bool    showFps = false;
    int     shots = 0;
    Vector2 lastMouse = GetMousePosition();
    // The title menu first (not when the autopilot shows off), then who to play; Esc or Start pauses.
    enum
    {
        TITLE,
        CHOOSE,
        PLAY,
        PAUSE,
        PANEL,
        OPTIONS,
        CONTROLS,
        PHOTO
    } mode = o.m_autoplay ? PLAY : TITLE;
    bool        photoHelp = true;      ///< the photo mode's help shows
    float       photoToast = 0.0f;     ///< a picture was saved: how long it is still said
    std::string photoSaved;            ///< where
    bool        waiting = false;       ///< the controls' page waits for a key or button
    auto        optionsFrom = TITLE;   // where the options go back to
    int         chosen = 0;
    int         panelTab = 0, panelRow = 0;   ///< the inventory, character and map pages: which, and the chosen row
    bool        quit = false;
    auto        restart = [&]()
    {
        float zoom = game->m_zoom;
        game->shutdown();
        game = std::make_unique<Game>();
        game->m_heroKind = hero;
        game->m_progress = progress;
        game->load( gfx, assetsDir(), level, sound );
        apply();
        game->m_autoplay = o.m_autoplay;
        game->m_zoom = zoom;
        score.keep( game->musicPieces() );
        if ( o.m_silent ) SetMasterVolume( 0.0f );
    };
    while ( !WindowShouldClose() && !quit )
    {
        // The buttons in the texts follow the device last used.
        updateBindings();
        if ( noteInput() ) game->refreshFont();
        float dt = fminf( GetFrameTime(), 0.05f );
        if ( mode == PLAY )
        {
            GameInput in = readInput( lastMouse, dt );
            bool      pause = IsKeyPressed( KEY_ESCAPE ) ||
                              ( IsGamepadAvailable( 0 ) && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT ) );
            // The inventory, the character page (as bound); the game waits meanwhile.
            bool inventory = pressed( BindE::INVENTORY ), character = pressed( BindE::CHARACTER );
            if ( pause && !game->finished() )
            {
                mode = PAUSE;
                chosen = 0;
            }
            else if ( pressed( BindE::PHOTO ) && !game->finished() )
            {
                game->startPhoto();
                mode = PHOTO;
            }
            else if ( ( inventory || character ) && !game->finished() )
            {
                mode = PANEL;
                panelTab = character ? 2 : 0;
                panelRow = 0;
            }
            else
            {
                game->update( dt, in );
                // A capsule woke: the game is saved there.
                if ( game->takeCheckpoint() && o.m_hero.empty() )
                {
                    progress = game->checkpoint();
                    saveProgress( progress );
                    saved = progress;
                    hasSave = true;
                }
                game->m_zoom = Clamp( game->m_zoom * ( 1.0f + zoomStep( dt ) ), 0.55f, 1.7f );
                bool onward = game->autoAdvance() ? game->finishedFor() > 1.2f : in.m_confirm;
                if ( game->finished() && onward )
                {
                    // On to the next level with the same hero and what was gained; saved at its start. "end <level>":
                    // an act is over; saved where the story goes on, and back to the title.
                    bool ending = game->next().compare( 0, 4, "end " ) == 0;
                    if ( !game->next().empty() )
                    {
                        progress = game->progress();
                        level = ending ? game->next().substr( 4 ) : game->next();
                        progress.m_level = level;
                        if ( o.m_hero.empty() ) saveProgress( progress ), saved = progress, hasSave = true;
                        // Through once: Albtraum may be chosen from now on.
                        if ( progress.m_flags.count( "gamedone" ) && !settings.m_nightmare )
                            settings.m_nightmare = true, saveSettings( settings );
                    }
                    restart();
                    if ( ending && !o.m_autoplay ) mode = TITLE, chosen = 0;
                }
            }
        }
        else if ( mode == PANEL )
        {
            // Left and right (or the shoulder buttons) change the page, up and down choose a row; back, I, C or View
            // close it.
            MenuKeys k = menuKeys( dt );
            bool     pad = IsGamepadAvailable( 0 );
            int      turn = k.m_change;
            if ( pad && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_TRIGGER_1 ) ) turn = -1;
            if ( pad && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1 ) ) turn = 1;
            if ( turn ) panelTab = ( panelTab + turn + Game::PanelTabs ) % Game::PanelTabs, panelRow = 0;
            int rows = game->panelRows( panelTab );
            if ( rows > 0 ) panelRow = ( panelRow + k.m_move + rows ) % rows;
            // A or Enter: put on, take off, learn; X or Delete: salvage.
            if ( k.m_confirm ) game->panelAction( panelTab, panelRow, 0 );
            if ( pressed( BindE::SALVAGE ) ) game->panelAction( panelTab, panelRow, 1 );
            // Into the stash, out of it (in a hub).
            if ( pressed( BindE::STORE ) ) game->panelAction( panelTab, panelRow, 2 );
            rows = game->panelRows( panelTab );
            if ( panelRow >= rows ) panelRow = std::max( 0, rows - 1 );
            if ( k.m_back || pressed( BindE::INVENTORY ) || pressed( BindE::CHARACTER ) ) mode = PLAY;
            lastMouse = GetMousePosition();
        }
        else if ( mode == CHOOSE )
        {
            // Dorra, Corvin, Ione.
            MenuKeys k = menuKeys( dt );
            chosen = ( chosen + k.m_move + 3 ) % 3;
            if ( k.m_confirm )
            {
                // A new game: from the prologue, with nothing.
                hero = chosen == 0 ? HeroE::DORRA : ( chosen == 1 ? HeroE::CORVIN : HeroE::IONE );
                level = o.m_level;
                progress = Progress();
                progress.m_hero = hero;
                progress.m_level = level;
                saveProgress( progress );
                hasSave = true;
                restart();
                mode = PLAY;
            }
            if ( k.m_back )
            {
                mode = TITLE;
                chosen = 0;
            }
            lastMouse = GetMousePosition();
        }
        else if ( mode == PHOTO )
        {
            // The game stands still; the camera pans (WASD, the arrows, the left stick), turns (Q E, the right
            // stick, the right mouse button), tilts (R F, the right stick), goes in and out (the wheel, the triggers).
            Game::PhotoInput pin;
            bool             pad = IsGamepadAvailable( 0 );
            auto             key = []( int a, int b )
            { return ( IsKeyDown( a ) ? 1.0f : 0.0f ) - ( IsKeyDown( b ) ? 1.0f : 0.0f ); };
            pin.m_move = { key( KEY_D, KEY_A ) + key( KEY_RIGHT, KEY_LEFT ),
                           key( KEY_S, KEY_W ) + key( KEY_DOWN, KEY_UP ) };
            pin.m_turn = key( KEY_E, KEY_Q ) * 1.6f * dt;
            pin.m_tilt = key( KEY_R, KEY_F ) * 0.9f * dt;
            pin.m_zoom = -GetMouseWheelMove() * 0.1f;
            Vector2 mouse = GetMousePosition();
            if ( IsMouseButtonDown( MOUSE_BUTTON_RIGHT ) )
            {
                pin.m_turn += ( mouse.x - lastMouse.x ) * 0.006f;
                pin.m_tilt += ( mouse.y - lastMouse.y ) * 0.004f;
            }
            lastMouse = mouse;
            if ( pad )
            {
                auto axis = []( int a )
                {
                    float v = GetGamepadAxisMovement( 0, a );
                    return fabsf( v ) > 0.2f ? v : 0.0f;
                };
                pin.m_move = Vector2Add( pin.m_move, { axis( GAMEPAD_AXIS_LEFT_X ), axis( GAMEPAD_AXIS_LEFT_Y ) } );
                pin.m_turn += axis( GAMEPAD_AXIS_RIGHT_X ) * 2.0f * dt;
                pin.m_tilt -= axis( GAMEPAD_AXIS_RIGHT_Y ) * 1.2f * dt;
                pin.m_zoom += ( ( GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_TRIGGER ) + 1.0f ) -
                                ( GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_TRIGGER ) + 1.0f ) ) *
                              0.6f * dt;
            }
            if ( Vector2Length( pin.m_move ) > 1.0f ) pin.m_move = Vector2Normalize( pin.m_move );
            game->photo( dt, pin );
            // H or Y: the help; Enter or A: the picture (into the player's folder); Esc, B, Start or the key: back.
            if ( IsKeyPressed( KEY_H ) || ( pad && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_UP ) ) )
                photoHelp = !photoHelp;
            if ( IsKeyPressed( KEY_ENTER ) || ( pad && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN ) ) )
            {
                time_t now = time( nullptr );
                char   stamp[32];
                strftime( stamp, sizeof( stamp ), "%Y%m%d_%H%M%S", localtime( &now ) );
                photoSaved = photoDir() + "photo_" + stamp + ".png";
                savePicture( gfx, photoSaved, draw );
                photoToast = 3.0f;
            }
            photoToast -= dt;
            if ( IsKeyPressed( KEY_ESCAPE ) || pressed( BindE::PHOTO ) ||
                 ( pad && ( IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT ) ||
                            IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT ) ) ) )
            {
                game->endPhoto();
                mode = PLAY;
            }
        }
        else if ( mode == CONTROLS )
        {
            // A row chosen and confirmed waits for the key or the button (Esc or Start: as it was).
            int  count = (int)BindE::COUNT + 2;
            bool changed = false;
            if ( waiting )
            {
                int key = capturedKey(), button = capturedPad();
                if ( IsKeyPressed( KEY_ESCAPE ) ||
                     ( IsGamepadAvailable( 0 ) && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT ) ) )
                    waiting = false;
                else if ( key != 0 )
                    bind( (BindE)chosen, false, key ), waiting = false, changed = true;
                else if ( button != 0 )
                    bind( (BindE)chosen, true, button ), waiting = false, changed = true;
            }
            else
            {
                MenuKeys k = menuKeys( dt );
                chosen = ( chosen + k.m_move + count ) % count;
                auto back = [&]()
                {
                    std::vector<RowE> rows = menuRows( 2, false, false );
                    mode = OPTIONS;
                    chosen = (int)( std::find( rows.begin(), rows.end(), RowE::CONTROLS ) - rows.begin() );
                };
                if ( k.m_confirm && chosen < (int)BindE::COUNT )
                    waiting = true;
                else if ( k.m_confirm && chosen == (int)BindE::COUNT )
                    resetBindings(), changed = true;
                else if ( k.m_confirm || k.m_back )
                    back();
            }
            if ( changed )
            {
                saveSettings( settings );
                game->refreshFont();
            }
            lastMouse = GetMousePosition();
        }
        else
        {
            // The title, the pause and the options: a list of rows; settings change left and right (or with A).
            MenuKeys          k = menuKeys( dt );
            bool              plus = hasSave && saved.m_flags.count( "gamedone" );
            std::vector<RowE> rows = menuRows( mode == OPTIONS ? 2 : ( mode == TITLE ? 0 : 1 ), hasSave, plus );
            int               count = (int)rows.size();
            chosen = ( chosen % count + k.m_move + count ) % count;
            RowE row = rows[chosen];
            int  step = k.m_change != 0 ? k.m_change : ( k.m_confirm ? 1 : 0 );
            auto cycle = [&]( int &v, int n ) { v = ( v + step + n ) % n; };
            auto volume = [&]( float &v ) { v = Clamp( roundf( v * 10.0f + k.m_change ) / 10.0f, 0.0f, 1.0f ); };
            bool changed = true;
            switch ( row )
            {
            case RowE::DIFFICULTY:
            {
                int d = (int)settings.m_difficulty;
                if ( step ) cycle( d, settings.m_nightmare ? 4 : 3 );
                settings.m_difficulty = (DifficultyE)d;
                break;
            }
            case RowE::BRIGHTNESS:
                settings.m_brightness = Clamp( settings.m_brightness + 0.1f * k.m_change, 0.6f, 2.0f );
                break;
            case RowE::GRAPHICS:
                if ( step )
                {
                    settings.m_quality = qualityOf( settings );
                    cycle( settings.m_quality, 3 );
                    gfx.setQuality( settings.m_quality );
                }
                break;
            case RowE::MUSIC:
                volume( settings.m_music );
                score.m_volume = settings.m_music;
                break;
            case RowE::VOICES: volume( settings.m_voices ); break;
            case RowE::EFFECTS: volume( settings.m_effects ); break;
            case RowE::LANGUAGE:
                if ( step )
                {
                    std::vector<std::string> all = languages( assetsDir() );
                    if ( all.empty() ) break;
                    auto it = std::find( all.begin(), all.end(), settings.m_language );
                    int  i = it == all.end() ? 0 : (int)( it - all.begin() );
                    settings.m_language = all[( i + step + all.size() ) % all.size()];
                    loadTexts( assetsDir(), settings.m_language );
                    game->setLanguage();
                }
                break;
            case RowE::SUBTITLES:
                if ( step ) cycle( settings.m_subtitles, 3 );
                break;
            case RowE::COLOURS:
                if ( step ) cycle( settings.m_colours, 3 );
                break;
            case RowE::SHAKE:
                if ( step ) cycle( settings.m_shake, 3 );
                break;
            case RowE::LOOT:
                if ( step ) cycle( settings.m_loot, 3 );
                break;
            default: changed = false; break;
            }
            if ( changed && step )
            {
                apply();
                saveSettings( settings );
            }
            if ( k.m_confirm && !changed )
            {
                if ( row == RowE::CONTINUE && loadProgress( saved ) )
                {
                    // Continue: the saved level with what was gained.
                    progress = saved;
                    hero = saved.m_hero;
                    level = saved.m_level;
                    restart();
                    mode = PLAY;
                }
                else if ( row == RowE::PLUS )
                {
                    // A new game+: the hero as they are (level, gear, skills, scrap), the story from the prologue,
                    // every place twelve areas further on. The notes read stay read.
                    progress = saved;
                    progress.m_cycle += 1;
                    std::set<std::string> kept;
                    for ( const std::string &f : progress.m_flags )
                        if ( f.compare( 0, 5, "note:" ) == 0 || f == "gamedone" ) kept.insert( f );
                    progress.m_flags = kept;
                    progress.m_deaths = 0;
                    journal::win( "plus" );
                    progress.m_level = level = o.m_level;
                    hero = progress.m_hero;
                    saveProgress( progress );
                    saved = progress;
                    restart();
                    mode = PLAY;
                }
                else if ( row == RowE::START && o.m_hero.empty() )
                    mode = CHOOSE, chosen = hero == HeroE::DORRA ? 0 : ( hero == HeroE::IONE ? 2 : 1 );
                else if ( row == RowE::START || row == RowE::RESUME )
                    mode = PLAY;
                else if ( row == RowE::OPTIONS )
                    optionsFrom = mode, mode = OPTIONS, chosen = 0;
                else if ( row == RowE::CONTROLS )
                    mode = CONTROLS, chosen = 0, waiting = false;
                else if ( row == RowE::PHOTO )
                {
                    game->startPhoto();
                    mode = PHOTO;
                    lastMouse = GetMousePosition();
                }
                else if ( row == RowE::BACK )
                    mode = optionsFrom, chosen = 1;
                else if ( row == RowE::RESTART )
                {
                    // From the level's start, not from its last capsule.
                    progress.m_hasSpot = false;
                    progress.m_fired.clear();
                    restart();
                    mode = PLAY;
                }
                else if ( row == RowE::QUIT )
                    quit = true;
            }
            if ( k.m_back && mode == OPTIONS )
                mode = optionsFrom, chosen = 1;
            else if ( k.m_back && mode == PAUSE )
                mode = PLAY;
            lastMouse = GetMousePosition();
        }
        // The music: the title's piece in the menus before the game, else what the game calls for; quieter in a menu.
        for ( const std::string &s : game->takeStings() ) score.sting( s );
        score.m_duck = mode == PAUSE ? 0.5f : ( mode == PANEL ? 0.75f : 1.0f );
        score.update( dt, mode == TITLE || mode == CHOOSE ? "title" : game->musicCue(), game->musicHeat() );
        if ( IsKeyPressed( KEY_F1 ) ) showFps = !showFps;
        if ( IsKeyPressed( KEY_F12 ) )
            savePicture( gfx, std::string( GetApplicationDirectory() ) + TextFormat( "shot_%03d.png", ++shots ), draw );
        BeginDrawing();
        game->render( gfx, nullptr );
        if ( mode == PANEL ) game->drawPanel( GetScreenWidth(), GetScreenHeight(), panelTab, panelRow );
        if ( mode == CHOOSE )
        {
            const char              *keys[3] = { "dorra", "corvin", "ione" };
            std::vector<std::string> items;
            for ( const char *key : keys ) items.push_back( tr( std::string( "choose." ) + key ) );
            drawMenu( game->font(), tr( "menu.choose" ), tr( std::string( "choose." ) + keys[chosen] + ".about" ),
                      items, chosen );
        }
        else if ( mode == CONTROLS )
            drawControls( game->font(), chosen, waiting );
        else if ( mode == PHOTO )
        {
            // Its help at the bottom (the picture saved is taken without it), what was saved.
            int   w = GetScreenWidth(), h = GetScreenHeight();
            float s = h / 1080.0f;
            auto  line = [&]( const std::string &t, float y, float size, Color c )
            {
                Vector2 m = MeasureTextEx( game->font(), t.c_str(), size * s, 1.0f );
                DrawRectangle( (int)( w * 0.5f - m.x * 0.5f - 14 * s ), (int)( y - 6 * s ), (int)( m.x + 28 * s ),
                               (int)( m.y + 12 * s ), { 0, 0, 0, 140 } );
                DrawTextEx( game->font(), t.c_str(), { w * 0.5f - m.x * 0.5f, y }, size * s, 1.0f, c );
            };
            if ( photoHelp ) line( tr( "photo.help" ), h - 70 * s, 20, { 220, 215, 205, 230 } );
            if ( photoToast > 0.0f )
                line( TextFormat( tr( "photo.saved" ), photoSaved.c_str() ), 40 * s, 22, { 160, 255, 190, 240 } );
        }
        else if ( mode != PLAY && mode != PANEL )
        {
            bool              plus = hasSave && saved.m_flags.count( "gamedone" );
            std::string       subtitle = mode == TITLE     ? game->title()
                                         : mode == OPTIONS ? std::string( tr( "menu.options" ) )
                                                           : std::string( difficultyName( settings.m_difficulty ) );
            std::vector<RowE> rows = menuRows( mode == OPTIONS ? 2 : ( mode == TITLE ? 0 : 1 ), hasSave, plus );
            drawMenu( game->font(), mode == PAUSE ? tr( "menu.pause" ) : "DARK CONCORDANCE", subtitle.c_str(),
                      rowTexts( rows, settings, saved.m_cycle ), chosen % (int)rows.size() );
        }
        if ( showFps ) DrawFPS( 10, 10 );
        EndDrawing();
    }
    game->shutdown();
    score.shutdown();
    if ( IsAudioDeviceReady() ) CloseAudioDevice();
    return 0;
}

}   // namespace

int main( int argc, char **argv )
{
    Options     o = parse( argc, argv );
    std::string logPath = std::string( GetApplicationDirectory() ) + "spaceopera.log";
    g_log = fopen( logPath.c_str(), "w" );
    SetTraceLogCallback( logToFile );
    SetTraceLogLevel( LOG_WARNING );

    // On a Steam Deck the window fills its screen (gamescope shows it whole) and the graphics start low.
    if ( o.m_width <= 0 ) o.m_width = steamDeck() ? 1280 : 1600;
    if ( o.m_height <= 0 ) o.m_height = steamDeck() ? 800 : 900;
    // Pictures for the tests are drawn high unless asked otherwise, whatever the player chose.
    int quality = o.m_quality >= 0 ? o.m_quality : ( o.m_shot.empty() ? qualityOf( loadSettings() ) : 2 );

    unsigned flags = FLAG_VSYNC_HINT;
    if ( !o.m_shot.empty() ) flags |= FLAG_WINDOW_HIDDEN;
    SetConfigFlags( flags );
    InitWindow( o.m_width, o.m_height, "SpaceOpera" );
    if ( !o.m_pad.empty() )
        setDevice( o.m_pad == "xbox"   ? DeviceE::XBOX
                   : o.m_pad == "ps"   ? DeviceE::PLAYSTATION
                   : o.m_pad == "deck" ? DeviceE::DECK
                                       : DeviceE::KEYBOARD );
    SetExitKey( KEY_NULL );   // Esc pauses; the menu quits

    Gfx gfx;
    gfx.init( o.m_width, o.m_height, quality, o.m_ss );
    if ( o.m_exposure > 0.0f ) gfx.m_exposure = o.m_exposure;
    if ( !o.m_scoreTest.empty() )
    {
        int r = runScoreTest( o.m_scoreTest );
        CloseWindow();
        return r;
    }
    if ( !o.m_dumpSound.empty() )
    {
        Audio::dump( o.m_dumpSound );
        CloseWindow();
        return 0;
    }
    int result = o.m_lookdev ? runLookdev( o, gfx ) : runGame( o, gfx );
    CloseWindow();
    if ( g_log ) fclose( g_log );
    return result;
}
