/**
 * @file main.cpp
 * @brief The look test of a dark space opera action RPG in the manner of Baldur's Gate: Dark Alliance: one room of an
 *        abandoned station, a hero in a hardsuit, light, fog, sparks. Walk with WASD, the arrows or the left stick;
 *        shoot with Space, the left mouse button or the right trigger; F1 shows the frame rate, F12 saves a picture.
 *
 *        --shot FILE   renders one picture and exits (--w, --h, --ss, --at x,z, --face deg, --walk, --fire, --warm s)
 * @author Rene Weller
 */
#include "Gfx.h"
#include "Scene.h"

#include <raylib.h>
#include <raymath.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spaceopera;

namespace
{

/** @brief The command line. */
struct Options
{
    int         m_width = 1600;       ///< picture width
    int         m_height = 900;       ///< picture height
    float       m_ss = 1.5f;          ///< supersampling
    std::string m_shot;               ///< render this picture and exit
    float       m_x = -3.0f;          ///< hero east
    float       m_z = 1.0f;           ///< hero south
    float       m_face = 200.0f;      ///< hero heading (degrees, 0 = towards the camera)
    bool        m_walk = false;       ///< caught mid-stride
    bool        m_fire = false;       ///< a bolt in flight
    float       m_warm = 4.0f;        ///< seconds simulated before the picture
    float       m_exposure = -1.0f;   ///< exposure (negative: the renderer's)
    int         m_bench = 0;          ///< with --shot: render this many frames first and log the time per frame
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
            sscanf( next().c_str(), "%f,%f", &o.m_x, &o.m_z );
        else if ( a == "--face" )
            o.m_face = (float)atof( next().c_str() );
        else if ( a == "--walk" )
            o.m_walk = true;
        else if ( a == "--fire" )
            o.m_fire = true;
        else if ( a == "--warm" )
            o.m_warm = (float)atof( next().c_str() );
        else if ( a == "--exposure" )
            o.m_exposure = (float)atof( next().c_str() );
        else if ( a == "--bench" )
            o.m_bench = atoi( next().c_str() );
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

/** @brief Renders into a target and writes it as a PNG. */
void savePicture( Scene &scene, Gfx &gfx, const std::string &path )
{
    RenderTexture2D rt = LoadRenderTexture( gfx.m_width, gfx.m_height );
    scene.render( gfx, &rt );
    Image img = LoadImageFromTexture( rt.texture );
    ImageFlipVertical( &img );
    ExportImage( img, path.c_str() );
    UnloadImage( img );
    UnloadRenderTexture( rt );
}

/** @brief The player's controls from keyboard, mouse and gamepad. */
Controls readControls()
{
    Controls c;
    if ( IsKeyDown( KEY_W ) || IsKeyDown( KEY_UP ) ) c.m_move.y -= 1.0f;
    if ( IsKeyDown( KEY_S ) || IsKeyDown( KEY_DOWN ) ) c.m_move.y += 1.0f;
    if ( IsKeyDown( KEY_A ) || IsKeyDown( KEY_LEFT ) ) c.m_move.x -= 1.0f;
    if ( IsKeyDown( KEY_D ) || IsKeyDown( KEY_RIGHT ) ) c.m_move.x += 1.0f;
    c.m_fire = IsKeyDown( KEY_SPACE ) || IsMouseButtonDown( MOUSE_BUTTON_LEFT );
    if ( IsGamepadAvailable( 0 ) )
    {
        Vector2 stick = { GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_X ),
                          GetGamepadAxisMovement( 0, GAMEPAD_AXIS_LEFT_Y ) };
        if ( Vector2Length( stick ) > 0.2f ) c.m_move = Vector2Add( c.m_move, stick );
        c.m_fire = c.m_fire || GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_TRIGGER ) > 0.3f ||
                   IsGamepadButtonDown( 0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN );
    }
    return c;
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
    InitWindow( o.m_width, o.m_height, "SpaceOpera - look test" );

    Gfx gfx;
    gfx.init( o.m_width, o.m_height, o.m_ss );
    if ( o.m_exposure > 0.0f ) gfx.m_exposure = o.m_exposure;
    Scene scene;
    scene.load( gfx, assetsDir() );

    if ( !o.m_shot.empty() )
    {
        const float dt = 1.0f / 60.0f;
        float       yaw = o.m_face * DEG2RAD;
        Vector2     dir = { sinf( yaw ), cosf( yaw ) };
        int         frames = (int)( o.m_warm * 60.0f );
        // Walking: start a little behind the spot and stride into it.
        float back = o.m_walk ? 2.4f : 0.0f;
        scene.putHero( o.m_x - dir.x * back, o.m_z - dir.y * back, o.m_face );
        for ( int i = 0; i < frames; ++i )
        {
            Controls c;
            if ( o.m_walk && i >= frames - 36 ) c.m_move = dir;
            if ( o.m_fire && i == frames - 9 ) scene.fire();
            scene.update( dt, c );
        }
        if ( o.m_bench > 0 )
        {
            // The read-back at the end waits for the GPU, so the average is honest.
            RenderTexture2D rt = LoadRenderTexture( gfx.m_width, gfx.m_height );
            double          start = GetTime();
            for ( int i = 0; i < o.m_bench; ++i )
            {
                scene.update( dt, Controls() );
                scene.render( gfx, &rt );
            }
            Image img = LoadImageFromTexture( rt.texture );
            TraceLog( LOG_WARNING, "BENCH: %dx%d ss %.2f: %.2f ms per frame", o.m_width, o.m_height, o.m_ss,
                      ( GetTime() - start ) * 1000.0 / o.m_bench );
            UnloadImage( img );
            UnloadRenderTexture( rt );
        }
        savePicture( scene, gfx, o.m_shot );
        CloseWindow();
        if ( g_log ) fclose( g_log );
        return 0;
    }

    bool showFps = false;
    int  shots = 0;
    while ( !WindowShouldClose() )
    {
        float dt = fminf( GetFrameTime(), 0.05f );
        scene.update( dt, readControls() );
        if ( IsKeyPressed( KEY_F1 ) ) showFps = !showFps;
        float zoom = -GetMouseWheelMove() * 0.08f;
        if ( IsGamepadAvailable( 0 ) )
        {
            if ( IsGamepadButtonDown( 0, GAMEPAD_BUTTON_LEFT_TRIGGER_1 ) ) zoom -= dt;
            if ( IsGamepadButtonDown( 0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1 ) ) zoom += dt;
        }
        scene.m_zoom = Clamp( scene.m_zoom * ( 1.0f + zoom ), 0.55f, 1.7f );
        if ( IsKeyPressed( KEY_F12 ) )
            savePicture( scene, gfx,
                         std::string( GetApplicationDirectory() ) + TextFormat( "shot_%03d.png", ++shots ) );
        BeginDrawing();
        scene.render( gfx, nullptr );
        if ( showFps ) DrawFPS( 10, 10 );
        EndDrawing();
    }
    CloseWindow();
    if ( g_log ) fclose( g_log );
    return 0;
}
