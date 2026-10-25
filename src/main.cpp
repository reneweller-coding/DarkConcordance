/**
 * @file main.cpp
 * @brief SpaceOpera: level 1 „Die Kühlgedärme“ (default) or the look test (--lookdev).
 *
 *        Move with WASD, the arrows or the left stick; jump with Space or A; shoot with the left mouse button, the
 *        right trigger or X (without aiming the rifle takes the nearest target ahead); aim with the mouse or the right
 *        stick; Q or Y injects a stim; the mouse wheel or the shoulder buttons zoom; F1 shows the frame rate, F12 saves
 *        a picture.
 *
 *        --shot FILE    renders one picture and exits (--w, --h, --ss, --at x,z, --face deg, --awake, --fire,
 *                       --autoplay, --warm s, --bench n); --report writes the autopilot's run into the log
 *        --autoplay     the autopilot plays (also in the window, as a demo)
 *        --mute         no sound
 *        --lookdev      the look test's room instead of the level
 * @author Rene Weller
 */
#include "Game.h"
#include "Gfx.h"
#include "Scene.h"

#include <raylib.h>
#include <raymath.h>

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
    std::string m_level = "kuehlgedaerme.txt";   ///< the level
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
    int         m_bench = 0;      ///< with --shot: render this many frames first and log the time per frame
    std::string m_difficulty;     ///< leicht, normal, schwer (else the saved setting)
    bool        m_menu = false;   ///< with --shot: the title menu over the picture
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

/** @brief The player's input from keyboard, mouse and gamepad. */
GameInput readInput( Vector2 &lastMouse )
{
    GameInput in;
    if ( IsKeyDown( KEY_W ) || IsKeyDown( KEY_UP ) ) in.m_move.y -= 1.0f;
    if ( IsKeyDown( KEY_S ) || IsKeyDown( KEY_DOWN ) ) in.m_move.y += 1.0f;
    if ( IsKeyDown( KEY_A ) || IsKeyDown( KEY_LEFT ) ) in.m_move.x -= 1.0f;
    if ( IsKeyDown( KEY_D ) || IsKeyDown( KEY_RIGHT ) ) in.m_move.x += 1.0f;
    in.m_fire = IsMouseButtonDown( MOUSE_BUTTON_LEFT ) || IsKeyDown( KEY_K );
    in.m_jump = IsKeyPressed( KEY_SPACE );
    in.m_stim = IsKeyPressed( KEY_Q );
    in.m_confirm = IsKeyPressed( KEY_ENTER );
    Vector2 mouse = GetMousePosition();
    in.m_mouse = mouse;
    in.m_mouseAim = Vector2Distance( mouse, lastMouse ) > 2.0f || IsMouseButtonDown( MOUSE_BUTTON_LEFT );
    lastMouse = mouse;
    if ( IsGamepadAvailable( 0 ) )
    {
        Vector2 stick = { GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_X ),
                          GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_Y ) };
        if ( Vector2Length( stick ) > 0.2f ) in.m_move = Vector2Add( in.m_move, stick );
        in.m_stick = { GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_X ),
                       GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_Y ) };
        in.m_fire = in.m_fire || GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_TRIGGER ) > 0.3f ||
                    IsGamepadButtonDown( 0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT );
        in.m_jump = in.m_jump || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN );
        in.m_stim = in.m_stim || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_RIGHT_FACE_UP );
        in.m_confirm = in.m_confirm || IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT );
        if ( Vector2Length( in.m_stick ) > 0.3f ) in.m_mouseAim = false;
    }
    return in;
}

/** @brief Zoom from the mouse wheel and the shoulder buttons. */
float zoomStep( float dt )
{
    float zoom = -GetMouseWheelMove() * 0.08f;
    if ( IsGamepadAvailable( 0 ) )
    {
        if ( IsGamepadButtonDown( 0, GAMEPAD_BUTTON_LEFT_TRIGGER_1 ) ) zoom -= dt;
        if ( IsGamepadButtonDown( 0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1 ) ) zoom += dt;
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
};

/** @brief Where the settings live. */
std::string settingsPath()
{
    return std::string( GetApplicationDirectory() ) + "settings.txt";
}

/** @brief Reads the settings (defaults if there are none). */
Settings loadSettings()
{
    Settings s;
    FILE    *f = fopen( settingsPath().c_str(), "r" );
    if ( !f ) return s;
    int   d = 1;
    float b = 1.0f;
    if ( fscanf( f, "difficulty %d\nbrightness %f", &d, &b ) >= 1 )
    {
        s.m_difficulty = (DifficultyE)Clamp( (float)d, 0.0f, 2.0f );
        s.m_brightness = Clamp( b, 0.6f, 2.0f );
    }
    fclose( f );
    return s;
}

/** @brief Writes the settings. */
void saveSettings( const Settings &s )
{
    FILE *f = fopen( settingsPath().c_str(), "w" );
    if ( !f ) return;
    fprintf( f, "difficulty %d\nbrightness %.2f\n", (int)s.m_difficulty, s.m_brightness );
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
    text( "Oben/Unten: wählen    Links/Rechts: ändern    Enter / A: bestätigen    Esc / Start: Pause", h - 70 * s, 20,
          { 150, 150, 150, 200 } );
}

/** @brief A brightness bar as text. */
std::string brightnessBar( float b )
{
    int         n = (int)roundf( ( b - 0.6f ) / 0.1f );
    std::string bar = "Helligkeit  ";
    for ( int i = 0; i <= 14; ++i ) bar += i <= n ? "|" : ".";
    return bar;
}

/** @brief The level. */
int runGame( const Options &o, Gfx &gfx )
{
    bool sound = !o.m_mute && o.m_shot.empty();
    auto game = std::make_unique<Game>();
    if ( !game->load( gfx, assetsDir(), o.m_level, sound ) )
    {
        TraceLog( LOG_ERROR, "LEVEL: %s not found", o.m_level.c_str() );
        return 1;
    }
    Settings settings = loadSettings();
    if ( !o.m_difficulty.empty() )
        settings.m_difficulty = o.m_difficulty == "leicht"
                                    ? DifficultyE::EASY
                                    : ( o.m_difficulty == "schwer" ? DifficultyE::HARD : DifficultyE::NORMAL );
    game->setDifficulty( settings.m_difficulty );
    game->m_brightness = settings.m_brightness;
    game->m_autoplay = o.m_autoplay;
    if ( o.m_hasAt ) game->putHero( o.m_x, o.m_z, o.m_face );
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
            game->update( dt, in );
        }
        if ( o.m_report ) TraceLog( LOG_WARNING, "RUN: %s", game->report().c_str() );
        if ( o.m_bench > 0 ) bench( gfx, o.m_bench, o, draw );
        if ( o.m_menu )
        {
            // The title menu over the frame (to look at it without a window).
            std::vector<std::string> items = { "Spiel starten",
                                               std::string( "Schwierigkeit  " ) +
                                                   difficultyName( settings.m_difficulty ),
                                               brightnessBar( settings.m_brightness ), "Beenden" };
            savePicture( gfx, o.m_shot,
                         [&]( const RenderTexture2D *rt )
                         {
                             game->render( gfx, rt );
                             BeginTextureMode( *rt );
                             drawMenu( game->font(), "DARK CONCORDANCE", "Akt I · Velm  —  Die Kühlgedärme", items, 0 );
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
    // The title menu first (not when the autopilot shows off), Esc or Start pauses.
    enum
    {
        TITLE,
        PLAY,
        PAUSE
    } mode = o.m_autoplay ? PLAY : TITLE;
    int  chosen = 0;
    bool quit = false;
    auto restart = [&]()
    {
        float zoom = game->m_zoom;
        game->shutdown();
        game = std::make_unique<Game>();
        game->load( gfx, assetsDir(), o.m_level, sound );
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
            GameInput in = readInput( lastMouse );
            bool      pause = IsKeyPressed( KEY_ESCAPE ) ||
                              ( IsGamepadAvailable( 0 ) && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT ) );
            if ( pause && !game->finished() )
            {
                mode = PAUSE;
                chosen = 0;
            }
            else
            {
                game->update( dt, in );
                game->m_zoom = Clamp( game->m_zoom * ( 1.0f + zoomStep( dt ) ), 0.55f, 1.7f );
                if ( game->finished() && in.m_confirm ) restart();
            }
        }
        else
        {
            // Title: start, difficulty, brightness, quit. Pause: resume, difficulty, brightness, restart, quit.
            MenuKeys k = menuKeys( dt );
            int      count = mode == TITLE ? 4 : 5;
            chosen = ( chosen + k.m_move + count ) % count;
            int diffItem = 1, brightItem = 2;
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
                if ( chosen == 0 )
                    mode = PLAY;
                else if ( chosen == diffItem )
                {
                    settings.m_difficulty = (DifficultyE)( ( (int)settings.m_difficulty + 1 ) % 3 );
                    game->setDifficulty( settings.m_difficulty );
                    saveSettings( settings );
                }
                else if ( mode == PAUSE && chosen == 3 )
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
        if ( mode != PLAY )
        {
            std::vector<std::string> items;
            items.push_back( mode == TITLE ? "Spiel starten" : "Weiter" );
            items.push_back( std::string( "Schwierigkeit  " ) + difficultyName( settings.m_difficulty ) );
            items.push_back( brightnessBar( settings.m_brightness ) );
            if ( mode == PAUSE ) items.push_back( "Level neu starten" );
            items.push_back( "Beenden" );
            drawMenu( game->font(), mode == TITLE ? "DARK CONCORDANCE" : "PAUSE",
                      mode == TITLE ? "Akt I · Velm  —  Die Kühlgedärme" : difficultyName( settings.m_difficulty ),
                      items, chosen );
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
