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
#include "Gfx.h"
#include "Scene.h"
#include "Text.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

using namespace spaceopera;

namespace
{

/** @brief The command line. */
struct Options
{
    int         m_width = 1600;                  ///< picture width
    int         m_height = 900;                  ///< picture height
    float       m_ss = 1.5f;                     ///< supersampling
    std::string m_shot;                          ///< render this picture and exit
    std::string m_level = "gute_hoffnung.txt";   ///< the level (the prologue starts the game)
    bool        m_hasAt = false;                 ///< --at was given
    float       m_x = 0.0f;                      ///< where to put the hero (east)
    float       m_z = 0.0f;                      ///< (south)
    float       m_face = 0.0f;                   ///< heading (degrees, 0 = towards the camera)
    bool        m_walk = false;                  ///< look test: caught mid-stride
    bool        m_fire = false;                  ///< a shot just fired
    bool        m_awake = false;                 ///< every enemy awake
    bool        m_autoplay = false;              ///< the autopilot plays
    bool        m_report = false;                ///< log the autopilot's run
    bool        m_mute = false;                  ///< no sound
    bool        m_lookdev = false;               ///< the look test
    float       m_warm = 4.0f;                   ///< seconds simulated before the picture
    float       m_exposure = -1.0f;              ///< exposure (negative: the renderer's)
    int         m_bench = 0;               ///< with --shot: render this many frames first and log the time per frame
    std::string m_difficulty;              ///< leicht, normal, schwer (else the saved setting)
    std::string m_language;                ///< de, en (else the saved setting)
    bool        m_menu = false;            ///< with --shot: the title menu over the picture
    float       m_camYaw = 0.0f;           ///< camera turned about the hero (degrees)
    Vector2     m_move = { 0.0f, 0.0f };   ///< with --shot: the left stick held during the warm-up
    std::string m_hero;                    ///< corvin, dorra (else the choice in the menu)
    std::string m_flags;                   ///< what has happened already (comma-separated; tests)
    std::string m_talk;                    ///< with --shot: a conversation open in the picture
    int         m_shop = 0;                ///< with --shot: Ottla's (1) or Kiri's (2) menu open
    int         m_panel = -1;              ///< with --shot: the inventory (0), character (1) or map (2) page
    bool        m_map = false;             ///< with --shot: the map over the game
    bool        m_lineup = false;          ///< with --shot: a row of people in their animations before the hero
    float       m_zoom = 0.0f;             ///< with --shot: the camera's distance factor (0: as it is)
    int         m_loot = 0;                ///< tests: items scattered round the hero at the start
    int         m_xp = 0;                  ///< tests: experience given at the start
    unsigned    m_seed = 0;                ///< a generated level's seed (0: from the progress)
    std::string m_dumpGen;                 ///< write the generated level's file here
    int         m_heroLevel = 0;           ///< tests: the hero's level at the start
    float       m_trace = 0.0f;            ///< tests: log the run's state every so many seconds
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
        else if ( a == "--lookdev" )
            o.m_lookdev = true;
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
        else if ( a == "--lang" )
            o.m_language = next();
        else if ( a == "--hero" )
            o.m_hero = next();
        else if ( a == "--flags" )
            o.m_flags = next();
        else if ( a == "--talk" )
            o.m_talk = next();
        else if ( a == "--shop" )
            o.m_shop = atoi( next().c_str() );
        else if ( a == "--camyaw" )
            o.m_camYaw = (float)atof( next().c_str() );
        else if ( a == "--move" )
            sscanf( next().c_str(), "%f,%f", &o.m_move.x, &o.m_move.y );
        else if ( a == "--panel" )
            o.m_panel = atoi( next().c_str() );
        else if ( a == "--map" )
            o.m_map = true;
        else if ( a == "--lineup" )
            o.m_lineup = true;
        else if ( a == "--zoom" )
            o.m_zoom = (float)atof( next().c_str() );
        else if ( a == "--loot" )
            o.m_loot = atoi( next().c_str() );
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
    TraceLog( LOG_WARNING, "BENCH: %dx%d ss %.2f: %.2f ms per frame", o.m_width, o.m_height, o.m_ss,
              ( GetTime() - start ) * 1000.0 / frames );
    UnloadImage( img );
    UnloadRenderTexture( rt );
}

/**
 * @brief The player's input from keyboard, mouse and gamepad (one stick moves, the other turns the camera, as on the
 *        Steam Deck; the rifle aims itself, the mouse can aim on the PC).
 */
GameInput readInput( Vector2 &lastMouse, float dt )
{
    GameInput in;
    if ( IsKeyDown( KEY_W ) || IsKeyDown( KEY_UP ) ) in.m_move.y -= 1.0f;
    if ( IsKeyDown( KEY_S ) || IsKeyDown( KEY_DOWN ) ) in.m_move.y += 1.0f;
    if ( IsKeyDown( KEY_A ) || IsKeyDown( KEY_LEFT ) ) in.m_move.x -= 1.0f;
    if ( IsKeyDown( KEY_D ) || IsKeyDown( KEY_RIGHT ) ) in.m_move.x += 1.0f;
    in.m_fire = IsMouseButtonDown( MOUSE_BUTTON_LEFT ) || IsKeyDown( KEY_K );
    in.m_jump = IsKeyPressed( KEY_SPACE );
    in.m_stim = IsKeyPressed( KEY_Q );
    in.m_special = IsKeyPressed( KEY_F );
    in.m_use = IsKeyPressed( KEY_E );
    in.m_map = IsKeyPressed( KEY_M ) || IsKeyPressed( KEY_TAB );
    in.m_skill2 = IsKeyPressed( KEY_R );
    in.m_skill3 = IsKeyPressed( KEY_G );
    in.m_confirm = IsKeyPressed( KEY_ENTER );
    in.m_strafe = IsKeyDown( KEY_LEFT_SHIFT ) || IsKeyDown( KEY_RIGHT_SHIFT );
    in.m_cameraReset = IsKeyPressed( KEY_BACKSPACE );
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
        in.m_cameraReset = in.m_cameraReset || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_THUMB );
        in.m_strafe = in.m_strafe || GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_TRIGGER ) > 0.3f;
        in.m_fire = in.m_fire || GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_TRIGGER ) > 0.3f ||
                    IsGamepadButtonDown( 0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT );
        in.m_jump = in.m_jump || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN );
        in.m_stim = in.m_stim || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_UP );
        in.m_special = in.m_special || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT );
        in.m_confirm = in.m_confirm || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT );
        in.m_map = in.m_map || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_FACE_UP );
        in.m_skill2 = in.m_skill2 || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_TRIGGER_1 );
        in.m_skill3 = in.m_skill3 || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1 );
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
    std::string m_language;                           ///< the texts' language
};

/** @brief Where the settings live. */
std::string settingsPath()
{
    return std::string( GetApplicationDirectory() ) + "settings.txt";
}

/** @brief Reads the settings (defaults if there are none; the language at first the system's). */
Settings loadSettings()
{
    Settings s;
    s.m_language = systemLanguage( assetsDir() );
    FILE *f = fopen( settingsPath().c_str(), "r" );
    if ( !f ) return s;
    char key[32], value[64];
    while ( fscanf( f, "%31s %63s", key, value ) == 2 )
    {
        if ( !strcmp( key, "difficulty" ) )
            s.m_difficulty = (DifficultyE)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "brightness" ) )
            s.m_brightness = Clamp( (float)atof( value ), 0.6f, 2.0f );
        else if ( !strcmp( key, "language" ) )
            s.m_language = value;
    }
    fclose( f );
    return s;
}

/** @brief Where the game is saved (the start of the level to play). */
std::string savePath()
{
    return std::string( GetApplicationDirectory() ) + "save.txt";
}

/** @brief Reads the save; false if there is none. */
bool loadProgress( Progress &p )
{
    FILE *f = fopen( savePath().c_str(), "r" );
    if ( !f ) return false;
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
        else if ( !strcmp( key, "hero" ) )
            p.m_hero = !strcmp( value, "dorra" ) ? HeroE::DORRA : HeroE::CORVIN;
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
    return !p.m_level.empty() && FileExists( ( assetsDir() + "/levels/" + p.m_level ).c_str() );
}

/** @brief Writes the save. */
void saveProgress( const Progress &p )
{
    FILE *f = fopen( savePath().c_str(), "w" );
    if ( !f ) return;
    fprintf( f, "hero %s\nlevel %s\nscrap %d\nstims %d\nweapon %d\narmor %d\n", heroKey( p.m_hero ), p.m_level.c_str(),
             p.m_scrap, p.m_stims, p.m_weapon, p.m_armor );
    fprintf( f, "xp %d\nherolevel %d\npoints %d\nseed %d\n", p.m_xp, p.m_heroLevel, p.m_points, p.m_seed );
    for ( size_t i = 0; i < p.m_ranks.size(); ++i )
        if ( p.m_ranks[i] > 0 ) fprintf( f, "rank %d %d\n", (int)i, p.m_ranks[i] );
    for ( int i = 0; i < EquipSlots; ++i )
        if ( p.m_worn[i] ) fprintf( f, "gear %d %s\n", i, saveItem( p.m_gear[i] ).c_str() );
    for ( const Item &it : p.m_bag ) fprintf( f, "bag %s\n", saveItem( it ).c_str() );
    for ( const std::string &flag : p.m_flags ) fprintf( f, "flag %s\n", flag.c_str() );
    fclose( f );
}

/** @brief Writes the settings. */
void saveSettings( const Settings &s )
{
    FILE *f = fopen( settingsPath().c_str(), "w" );
    if ( !f ) return;
    fprintf( f, "difficulty %d\nbrightness %.2f\nlanguage %s\n", (int)s.m_difficulty, s.m_brightness,
             s.m_language.c_str() );
    fclose( f );
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
        float y = h * 0.46f + i * 58 * s;
        if ( on )
            DrawRectangle( (int)( w * 0.5f - 260 * s ), (int)( y - 8 * s ), (int)( 520 * s ), (int)( 50 * s ),
                           { 255, 170, 80, 40 } );
        text( items[i].c_str(), y, 34, on ? Color{ 255, 205, 140, 255 } : Color{ 200, 200, 200, 220 } );
    }
    text( tr( "menu.help" ), h - 70 * s, 20, { 150, 150, 150, 200 } );
}

/** @brief A brightness bar as text. */
std::string brightnessBar( float b )
{
    int         n = (int)roundf( ( b - 0.6f ) / 0.1f );
    std::string bar = std::string( tr( "menu.brightness" ) ) + "  ";
    for ( int i = 0; i <= 14; ++i ) bar += i <= n ? "|" : ".";
    return bar;
}

/**
 * @brief The menu's items. Title: start, difficulty, brightness, language, quit. Pause: resume, difficulty,
 *        brightness, language, restart, quit.
 */
std::vector<std::string> menuItems( bool title, const Settings &s, bool saved )
{
    std::vector<std::string> items;
    if ( title && saved ) items.push_back( tr( "menu.continue" ) );
    items.push_back( tr( title ? "menu.start" : "menu.resume" ) );
    items.push_back( std::string( tr( "menu.difficulty" ) ) + "  " + difficultyName( s.m_difficulty ) );
    items.push_back( brightnessBar( s.m_brightness ) );
    items.push_back( std::string( tr( "menu.language" ) ) + "  " + tr( "language.name" ) );
    if ( !title ) items.push_back( tr( "menu.restart" ) );
    items.push_back( tr( "menu.quit" ) );
    return items;
}

/** @brief The level. */
int runGame( const Options &o, Gfx &gfx )
{
    bool     sound = !o.m_mute && o.m_shot.empty();
    Settings settings = loadSettings();
    if ( !o.m_language.empty() ) settings.m_language = o.m_language;
    if ( !loadTexts( assetsDir(), settings.m_language ) ) settings.m_language = "de";
    HeroE       hero = o.m_hero == "dorra" ? HeroE::DORRA : HeroE::CORVIN;
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
    game->m_progress = progress;
    game->m_genSeed = o.m_seed;
    if ( !game->load( gfx, assetsDir(), level, sound ) )
    {
        TraceLog( LOG_ERROR, "LEVEL: %s not found", level.c_str() );
        return 1;
    }
    if ( !o.m_dumpGen.empty() && !game->generated().empty() )
        SaveFileText( o.m_dumpGen.c_str(), const_cast<char *>( game->generated().c_str() ) );
    if ( !o.m_difficulty.empty() )
        settings.m_difficulty = o.m_difficulty == "leicht"
                                    ? DifficultyE::EASY
                                    : ( o.m_difficulty == "schwer" ? DifficultyE::HARD : DifficultyE::NORMAL );
    game->setDifficulty( settings.m_difficulty );
    game->m_brightness = settings.m_brightness;
    game->m_autoplay = o.m_autoplay;
    game->m_camYaw = o.m_camYaw * DEG2RAD;
    if ( o.m_hasAt ) game->putHero( o.m_x, o.m_z, o.m_face );
    if ( o.m_lineup ) game->lineup();
    if ( o.m_zoom > 0.0f ) game->m_zoom = o.m_zoom;
    if ( o.m_loot > 0 || o.m_xp > 0 ) game->testGifts( o.m_loot, o.m_xp );
    if ( o.m_awake ) game->wakeAll();
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
            if ( o.m_trace > 0.0f && i % std::max( 1, (int)( o.m_trace * 60.0f ) ) == 0 )
                TraceLog( LOG_WARNING, "TRACE: %s", game->report().c_str() );
        }
        if ( o.m_report ) TraceLog( LOG_WARNING, "RUN: %s", game->report().c_str() );
        if ( !o.m_talk.empty() || o.m_shop ) game->openForPicture( o.m_talk, o.m_shop );
        if ( o.m_bench > 0 ) bench( gfx, o.m_bench, o, draw );
        if ( o.m_panel >= 0 )
            savePicture( gfx, o.m_shot,
                         [&]( const RenderTexture2D *rt )
                         {
                             game->render( gfx, rt );
                             BeginTextureMode( *rt );
                             game->drawPanel( rt->texture.width, rt->texture.height, o.m_panel, 0 );
                             EndTextureMode();
                         } );
        else if ( o.m_menu )
        {
            // The title menu over the frame (to look at it without a window).
            std::vector<std::string> items = menuItems( true, settings, false );
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
        PANEL
    } mode = o.m_autoplay ? PLAY : TITLE;
    int  chosen = 0;
    int  panelTab = 0, panelRow = 0;   ///< the inventory, character and map pages: which, and the chosen row
    bool quit = false;
    auto restart = [&]()
    {
        float zoom = game->m_zoom;
        game->shutdown();
        game = std::make_unique<Game>();
        game->m_heroKind = hero;
        game->m_progress = progress;
        game->load( gfx, assetsDir(), level, sound );
        game->setDifficulty( settings.m_difficulty );
        game->m_brightness = settings.m_brightness;
        game->m_autoplay = o.m_autoplay;
        game->m_zoom = zoom;
    };
    while ( !WindowShouldClose() && !quit )
    {
        float dt = fminf( GetFrameTime(), 0.05f );
        if ( mode == PLAY )
        {
            GameInput in = readInput( lastMouse, dt );
            bool      pause = IsKeyPressed( KEY_ESCAPE ) ||
                              ( IsGamepadAvailable( 0 ) && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT ) );
            // I the inventory, C the character page, the pad's View button both; the game waits meanwhile.
            bool pad = IsGamepadAvailable( 0 ) && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_LEFT );
            bool inventory = IsKeyPressed( KEY_I ) || pad, character = IsKeyPressed( KEY_C );
            if ( pause && !game->finished() )
            {
                mode = PAUSE;
                chosen = 0;
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
                game->m_zoom = Clamp( game->m_zoom * ( 1.0f + zoomStep( dt ) ), 0.55f, 1.7f );
                bool onward = game->autoAdvance() ? game->finishedFor() > 1.2f : in.m_confirm;
                if ( game->finished() && onward )
                {
                    // On to the next level with the same hero and what was gained; saved at its start.
                    if ( !game->next().empty() )
                    {
                        progress = game->progress();
                        level = game->next();
                        progress.m_level = level;
                        if ( o.m_hero.empty() ) saveProgress( progress ), hasSave = true;
                    }
                    restart();
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
            if ( turn ) panelTab = ( panelTab + turn + 4 ) % 4, panelRow = 0;
            int rows = game->panelRows( panelTab );
            if ( rows > 0 ) panelRow = ( panelRow + k.m_move + rows ) % rows;
            // A or Enter: put on, take off, learn; X or Delete: salvage.
            if ( k.m_confirm ) game->panelAction( panelTab, panelRow, 0 );
            if ( IsKeyPressed( KEY_DELETE ) || IsKeyPressed( KEY_X ) ||
                 ( pad && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT ) ) )
                game->panelAction( panelTab, panelRow, 1 );
            rows = game->panelRows( panelTab );
            if ( panelRow >= rows ) panelRow = std::max( 0, rows - 1 );
            if ( k.m_back || IsKeyPressed( KEY_I ) || IsKeyPressed( KEY_C ) ||
                 ( pad && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_LEFT ) ) )
                mode = PLAY;
            lastMouse = GetMousePosition();
        }
        else if ( mode == CHOOSE )
        {
            // Dorra, Corvin; Ione is not playable yet.
            MenuKeys k = menuKeys( dt );
            chosen = ( chosen + k.m_move + 3 ) % 3;
            if ( k.m_confirm && chosen < 2 )
            {
                // A new game: from the prologue, with nothing.
                hero = chosen == 0 ? HeroE::DORRA : HeroE::CORVIN;
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
        else
        {
            // The items as menuItems lists them (the title's "continue" first when there is a save).
            MenuKeys k = menuKeys( dt );
            int      base = mode == TITLE && hasSave ? 1 : 0;
            int      count = ( mode == TITLE ? 5 : 6 ) + base;
            chosen = ( chosen + k.m_move + count ) % count;
            int  diffItem = 1 + base, brightItem = 2 + base, langItem = 3 + base, restartItem = mode == PAUSE ? 4 : -1;
            auto nextLanguage = [&]( int step )
            {
                std::vector<std::string> all = languages( assetsDir() );
                if ( all.empty() ) return;
                auto it = std::find( all.begin(), all.end(), settings.m_language );
                int  i = it == all.end() ? 0 : (int)( it - all.begin() );
                settings.m_language = all[( i + step + all.size() ) % all.size()];
                loadTexts( assetsDir(), settings.m_language );
                game->setLanguage();
                saveSettings( settings );
            };
            if ( chosen == langItem && k.m_change ) nextLanguage( k.m_change );
            if ( chosen == diffItem && k.m_change )
            {
                settings.m_difficulty = (DifficultyE)( ( (int)settings.m_difficulty + k.m_change + 3 ) % 3 );
                game->setDifficulty( settings.m_difficulty );
                saveSettings( settings );
            }
            if ( chosen == brightItem && k.m_change )
            {
                settings.m_brightness = Clamp( settings.m_brightness + 0.1f * k.m_change, 0.6f, 2.0f );
                game->m_brightness = settings.m_brightness;
                saveSettings( settings );
            }
            if ( k.m_confirm )
            {
                if ( chosen == 0 && base == 1 && loadProgress( saved ) )
                {
                    // Continue: the saved level with what was gained.
                    progress = saved;
                    hero = saved.m_hero;
                    level = saved.m_level;
                    restart();
                    mode = PLAY;
                }
                else if ( chosen == base && mode == TITLE && o.m_hero.empty() )
                    mode = CHOOSE, chosen = hero == HeroE::DORRA ? 0 : 1;
                else if ( chosen == base )
                    mode = PLAY;
                else if ( chosen == diffItem )
                {
                    settings.m_difficulty = (DifficultyE)( ( (int)settings.m_difficulty + 1 ) % 3 );
                    game->setDifficulty( settings.m_difficulty );
                    saveSettings( settings );
                }
                else if ( chosen == langItem )
                    nextLanguage( 1 );
                else if ( chosen == restartItem )
                {
                    restart();
                    mode = PLAY;
                }
                else if ( chosen == count - 1 )
                    quit = true;
            }
            if ( k.m_back && mode == PAUSE ) mode = PLAY;
            lastMouse = GetMousePosition();
        }
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
        else if ( mode != PLAY && mode != PANEL )
        {
            std::string subtitle = mode == TITLE ? game->title() : difficultyName( settings.m_difficulty );
            drawMenu( game->font(), mode == TITLE ? "DARK CONCORDANCE" : tr( "menu.pause" ), subtitle.c_str(),
                      menuItems( mode == TITLE, settings, hasSave ), chosen );
        }
        if ( showFps ) DrawFPS( 10, 10 );
        EndDrawing();
    }
    game->shutdown();
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

    unsigned flags = FLAG_VSYNC_HINT;
    if ( !o.m_shot.empty() ) flags |= FLAG_WINDOW_HIDDEN;
    SetConfigFlags( flags );
    InitWindow( o.m_width, o.m_height, "SpaceOpera" );
    SetExitKey( KEY_NULL );   // Esc pauses; the menu quits

    Gfx gfx;
    gfx.init( o.m_width, o.m_height, o.m_ss );
    if ( o.m_exposure > 0.0f ) gfx.m_exposure = o.m_exposure;
    int result = o.m_lookdev ? runLookdev( o, gfx ) : runGame( o, gfx );
    CloseWindow();
    if ( g_log ) fclose( g_log );
    return result;
}
