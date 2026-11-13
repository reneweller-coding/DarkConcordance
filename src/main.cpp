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
#include "GameShared.h"
#include "Bindings.h"
#include "Crash.h"
#include "Files.h"
#include "Glyphs.h"
#include "Journal.h"
#include "PlayLog.h"
#include "Rumble.h"
#include "Gfx.h"
#include "IconPng.h"
#include "Scene.h"
#include "Score.h"
#include "Text.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <sstream>
#include <functional>
#include <memory>
#include <string>

using namespace spaceopera;

namespace
{

/** @brief The command line. */
struct Options
{
    int                      m_width = 0;      ///< picture width (0: 1600, on a Steam Deck 1280)
    int                      m_height = 0;     ///< picture height (0: 900, on a Steam Deck 800)
    float                    m_ss = -1.0f;     ///< supersampling (negative: the quality's)
    int                      m_quality = -1;   ///< 0 low, 1 middle, 2 high, 3 frugal (negative: the setting's)
    std::string              m_shot;           ///< render this picture and exit
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
    int                      m_bench = 0;   ///< with --shot: render this many frames first and log the time per frame
    bool                     m_benchBig = false;      ///< with --bench: again at three times the size (the GPU's cost)
    bool                     m_benchFrugal = false;   ///< with --bench: again at frugal (the Deck on its battery)
    bool                     m_benchParts = false;    ///< with --bench: the interiors' features off in turn
    bool                     m_drawMesh = false;      ///< tests: the lit draws through raylib's DrawMesh
    float                    m_budget = 0.0f;         ///< with --benchbig: the dynamic resolution held to this (ms)
    std::string              m_difficulty;            ///< leicht, normal, schwer (else the saved setting)
    std::string              m_language;              ///< de, en (else the saved setting)
    bool                     m_menu = false;          ///< with --shot: the title menu over the picture
    bool                     m_controls = false;      ///< with --shot: the controls' page over the picture
    bool                     m_slots = false;         ///< with --shot: the list of saves over the picture
    std::string              m_photo;   ///< with --shot: the photo mode, its camera turned, tilted, pulled ("t,p,z")
    std::vector<std::string> m_binds;   ///< tests: keys and buttons bound otherwise ("<name> <key> <pad>")
    float                    m_camYaw = 0.0f;           ///< camera turned about the hero (degrees)
    float                    m_camRise = 0.0f;          ///< tests: the camera's height (0: as in play, 11.5)
    Vector2                  m_move = { 0.0f, 0.0f };   ///< with --shot: the left stick held during the warm-up
    std::string              m_hero;                    ///< corvin, dorra (else the choice in the menu)
    std::string              m_coop;                    ///< tests: a second hero beside the first, the autopilot's
    bool                     m_daily = false;     ///< the day's challenge at once (tests: with --autoplay, then end)
    std::string              m_flags;             ///< what has happened already (comma-separated; tests)
    std::string              m_talk;              ///< with --shot: a conversation open in the picture
    std::string              m_film;              ///< a film played over the level (assets/films/<name>)
    int                      m_shop = 0;          ///< with --shot: Ottla's (1) or Kiri's (2) menu open
    int                      m_panel = -1;        ///< with --shot: a page of the inventory (Game::PanelTabs: 0 weapons)
    int                      m_stash = 0;         ///< with --shot: the first items of the bag into the stash
    int                      m_lootFilter = -1;   ///< tests: the loot filter (-1: as set)
    bool                     m_map = false;       ///< with --shot: the map over the game
    bool                     m_lineup = false;    ///< with --shot: a row of people in their animations before the hero
    bool                     m_parade = false;    ///< with --shot: people walking and running to and fro (gaits)
    std::string              m_hunt;              ///< with --shot: these enemies come at an untouchable hero
    int                      m_creatures = -1;    ///< with --shot: the creatures in front of the hero (page 0 or 1)
    int                      m_figures = -1;   ///< with --shot: figures in their poses in front of the hero (page 0, 1)
    std::string              m_pad;   ///< keys, xbox, ps, deck: the buttons shown in the texts (else the device used)
    int                      m_cycle = 0;          ///< tests: a new game+'s cycle
    int                      m_tier = 0;           ///< tests: the Depths' tier (0: as saved)
    bool                     m_crash = false;      ///< tests: crash on purpose (the report)
    bool                     m_campaign = false;   ///< tests: the main path in order, what is gained carried on
    std::string              m_keep;               ///< tests: the campaign keeps the hero before each place here
    std::string              m_menuTest;           ///< tests: menu keys from a script (comma-separated), a real window
    std::string              m_menuDir;            ///< tests: where the script's shots go
    int                      m_places = 0;         ///< tests: the campaign stops after so many places (0: all)
    bool                     m_plus = false;       ///< tests: the hero of --loadfile carried into a new run
    std::string              m_playLog;            ///< tests: write the play log here
    std::string              m_saveFile;           ///< tests: where a capsule's save goes (instead of save.txt)
    std::string              m_loadFile;           ///< tests: a save to go on from
    int                      m_colours = -1;       ///< tests: colour vision 0..2 (else the setting)
    float                    m_zoom = 0.0f;        ///< with --shot: the camera's distance factor (0: as it is)
    int                      m_loot = 0;           ///< tests: items scattered round the hero at the start
    std::string              m_gear;       ///< tests: what the hero puts on at the start (--gear item.rail3,u12,s5)
    int                      m_xp = 0;     ///< tests: experience given at the start
    unsigned                 m_seed = 0;   ///< a generated level's seed (0: from the progress)
    std::string              m_dumpGen;    ///< write the generated level's file here
    int                      m_heroLevel = 0;   ///< tests: the hero's level at the start
    float                    m_trace = 0.0f;    ///< tests: log the run's state every so many seconds
    float                    m_clip = 0.0f;     ///< with --shot: the warm-up's last seconds as pictures (15 a second)
    std::string              m_tour;   ///< with --shot: more places after it ("a.txt;b.txt@x,z"), one picture each
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
            o.m_quality = q == "low" ? 0 : q == "middle" ? 1 : q == "high" ? 2 : q == "frugal" ? 3 : atoi( q.c_str() );
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
        else if ( a == "--benchbig" )
            o.m_benchBig = true;
        else if ( a == "--benchfrugal" )
            o.m_benchFrugal = true;
        else if ( a == "--benchparts" )
            o.m_benchParts = true;
        else if ( a == "--drawmesh" )
            o.m_drawMesh = true;
        else if ( a == "--plaininterior" )
            Level::s_plainInterior = true;   // the bench's breakdown: no sunken floors, no lamps of their own
        else if ( a == "--budget" )
            o.m_budget = (float)atof( next().c_str() );
        else if ( a == "--difficulty" )
            o.m_difficulty = next();
        else if ( a == "--menu" )
            o.m_menu = true;
        else if ( a == "--controls" )
            o.m_controls = true;
        else if ( a == "--slots" )
            o.m_slots = true;
        else if ( a == "--photo" )
            o.m_photo = next();
        else if ( a == "--bind" )
            o.m_binds.push_back( next() );
        else if ( a == "--lang" )
            o.m_language = next();
        else if ( a == "--hero" )
            o.m_hero = next();
        else if ( a == "--coop" )
            o.m_coop = next();
        else if ( a == "--daily" )
            o.m_daily = true;
        else if ( a == "--png" )
            play::useDds() = false;   // the PNGs, not the DDS beside them (to compare)
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
        else if ( a == "--parade" )
            o.m_parade = true;
        else if ( a == "--hunt" )
            o.m_hunt = next();
        else if ( a == "--creatures" )
            o.m_creatures = atoi( next().c_str() );
        else if ( a == "--figures" )
            o.m_figures = atoi( next().c_str() );
        else if ( a == "--pad" )
            o.m_pad = next();
        else if ( a == "--cycle" )
            o.m_cycle = atoi( next().c_str() );
        else if ( a == "--crash" )
            o.m_crash = true;
        else if ( a == "--campaign" )
            o.m_campaign = true;
        else if ( a == "--keep" )
            o.m_keep = next();
        else if ( a == "--menutest" )
            o.m_menuTest = next();
        else if ( a == "--menudir" )
            o.m_menuDir = next();
        else if ( a == "--places" )
            o.m_places = atoi( next().c_str() );
        else if ( a == "--plus" )
            o.m_plus = true;
        else if ( a == "--tier" )
            o.m_tier = atoi( next().c_str() );
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
        else if ( a == "--camrise" )
            o.m_camRise = (float)atof( next().c_str() );
        else if ( a == "--clip" )
            o.m_clip = (float)atof( next().c_str() );
        else if ( a == "--tour" )
            o.m_tour = next();
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

/**
 * @brief Renders `frames` frames and logs the time per frame (the read-back waits for the GPU), the renderer's own
 *        time on the processor and on the GPU (its timer queries) and its draws; again with the sun's shadows of what
 *        does not move drawn every frame (as while walking: the frames of a bench all show the same moment); with
 *        --benchbig again at three times the size, where the GPU's work outweighs the processor's (the per-pixel
 *        cost), and with --budget at six times the size (the GPU sets the pace) the dynamic resolution's answer to
 *        that frame budget; with --benchfrugal both again at frugal (the quality restored afterwards); with --benchparts
 *        again with each of the interiors' features off in turn and with raylib's DrawMesh instead of the lean path.
 */
void bench( Gfx &gfx, int frames, const Options &o, const std::function<void( const RenderTexture2D * )> &draw )
{
    gfx.m_stamps = true;
    auto run = [&]( const char *what )
    {
        RenderTexture2D rt = LoadRenderTexture( gfx.m_width, gfx.m_height );
        draw( &rt );   // the targets settle
        gfx.resetStats();
        double start = GetTime();
        for ( int i = 0; i < frames; ++i ) draw( &rt );
        Image      img = LoadImageFromTexture( rt.texture );
        Gfx::Stats s = gfx.stats();
        int        n = std::max( s.m_frames, 1 ), g = std::max( s.m_gpuFrames, 1 ), t = std::max( s.m_stampFrames, 1 );
        TraceLog(
            LOG_WARNING,
            "BENCH: %s%dx%d quality %d ss %.2f: %.2f ms per frame (renderer: cpu %.2f ms, gpu %.2f ms; %ld lit "
            "and %ld depth draws, %.1f lights a lit draw, %ld with more than 8, sun map drawn %d times, scene at %.2f; "
            "%.0fk lit and %.0fk depth triangles; cpu by part: shadows %.2f, sky and pre-pass %.2f, lit %.2f, fog and "
            "cones %.2f, rest %.2f; gpu by part: %.2f, %.2f, %.2f, %.2f, %.2f)",
            what, gfx.m_width, gfx.m_height, gfx.m_quality, gfx.m_scale, ( GetTime() - start ) * 1000.0 / frames,
            s.m_cpuMs / n, s.m_gpuMs / g, s.m_litDraws / n, s.m_depthDraws / n,
            (double)s.m_lightSum / std::max( s.m_litDraws, 1L ), s.m_lightsOver8 / n, s.m_sunRenders,
            gfx.m_scale * gfx.m_dynScale, s.m_litTris / n / 1000.0, s.m_depthTris / n / 1000.0, s.m_partMs[0] / n,
            s.m_partMs[1] / n, s.m_partMs[2] / n, s.m_partMs[3] / n, s.m_partMs[4] / n, s.m_gpuPartMs[0] / t,
            s.m_gpuPartMs[1] / t, s.m_gpuPartMs[2] / t, s.m_gpuPartMs[3] / t, s.m_gpuPartMs[4] / t );
        UnloadImage( img );
        UnloadRenderTexture( rt );
    };
    run( "" );
    gfx.m_cacheShadows = false;
    run( "walking " );
    gfx.m_cacheShadows = true;
    if ( o.m_benchFrugal && gfx.m_quality != 3 )
    {
        int quality = gfx.m_quality;
        gfx.setQuality( 3, gfx.m_scaleAsked );
        run( "frugal " );
        gfx.m_cacheShadows = false;
        run( "frugal walking " );
        gfx.m_cacheShadows = true;
        gfx.setQuality( quality, gfx.m_scaleAsked );
    }
    if ( o.m_benchParts )
    {
        // What each of the interiors' features costs: off in turn, then all of them, each with the lean lit path and
        // with raylib's DrawMesh (as before it).
        struct Part
        {
            const char *m_what;
            int         m_off;
            bool        m_lean;
        };
        const Part parts[] = { { "parts all on, DrawMesh ", 0, false },
                               { "parts masks off ", Gfx::OFF_MASKS, true },
                               { "parts cones off ", Gfx::OFF_CONES, true },
                               { "parts env and wet off ", Gfx::OFF_ENV, true },
                               { "parts all off ", Gfx::OFF_MASKS | Gfx::OFF_CONES | Gfx::OFF_ENV, true },
                               { "parts all off, DrawMesh ", Gfx::OFF_MASKS | Gfx::OFF_CONES | Gfx::OFF_ENV, false },
                               { "parts all on ", 0, true } };
        for ( const Part &p : parts )
        {
            gfx.m_off = p.m_off;
            gfx.m_lean = p.m_lean;
            run( p.m_what );
        }
        gfx.m_off = 0;
        gfx.m_lean = true;
        if ( gfx.m_quality != 3 )
        {
            int quality = gfx.m_quality;
            gfx.setQuality( 3, gfx.m_scaleAsked );
            gfx.m_lean = false;
            run( "parts frugal, DrawMesh " );
            gfx.m_lean = true;
            run( "parts frugal " );
            gfx.setQuality( quality, gfx.m_scaleAsked );
        }
    }
    if ( !o.m_benchBig )
    {
        gfx.m_stamps = false;
        return;
    }
    int w = gfx.m_width, h = gfx.m_height;
    gfx.resize( w * 3, h * 3 );
    run( "big " );
    if ( o.m_budget > 0.0f )
    {
        gfx.resize( w * 6, h * 6 );   // so large that the GPU, not the processor, sets the pace
        gfx.m_dynamicRes = true;
        gfx.m_frameBudget = o.m_budget;
        run( "dynamic " );
        gfx.m_dynamicRes = false;
        gfx.m_frameBudget = 25.0f;
        gfx.m_dynScale = 1.0f;
    }
    gfx.resize( w, h );
    gfx.m_stamps = false;
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
    // The swap: as bound, and on the cross's other side too where nothing else is bound (Dark Alliance's way).
    in.m_swap = pressed( BindE::SWAP );
    if ( IsGamepadAvailable( 0 ) && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_LEFT_FACE_LEFT ) )
    {
        bool free = true;
        for ( int b = 0; b < (int)BindE::SALVAGE; ++b )
            free = free && binding( (BindE)b ).m_pad != GAMEPAD_BUTTON_LEFT_FACE_LEFT;
        in.m_swap = in.m_swap || free;
    }
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

/**
 * @brief The second player's input from their pad (the first player's bindings, its buttons only; the right stick
 *        and the menus stay the first player's).
 * @param[in] pad  the pad
 * @return what it asks for this frame
 */
GameInput readPartner( int pad )
{
    GameInput in;
    if ( pad < 0 || !IsGamepadAvailable( pad ) ) return in;
    static bool was[2] = {};
    bool        now[2] = { GetGamepadAxisMovement( pad, GAMEPAD_AXIS_LEFT_TRIGGER ) > 0.3f,
                           GetGamepadAxisMovement( pad, GAMEPAD_AXIS_RIGHT_TRIGGER ) > 0.3f };
    auto        trigger = []( int button )
    {
        return button == GAMEPAD_BUTTON_LEFT_TRIGGER_2 ? 0 : button == GAMEPAD_BUTTON_RIGHT_TRIGGER_2 ? 1 : -1;
    };
    auto down = [&]( BindE b )
    {
        int button = binding( b ).m_pad, t = trigger( button );
        return button > 0 && ( IsGamepadButtonDown( pad, button ) || ( t >= 0 && now[t] ) );
    };
    auto hit = [&]( BindE b )
    {
        int button = binding( b ).m_pad, t = trigger( button );
        return button > 0 && ( IsGamepadButtonPressed( pad, button ) || ( t >= 0 && now[t] && !was[t] ) );
    };
    Vector2 stick = { GetGamepadAxisMovement( pad, GAMEPAD_AXIS_LEFT_X ),
                      GetGamepadAxisMovement( pad, GAMEPAD_AXIS_LEFT_Y ) };
    if ( Vector2Length( stick ) > 0.2f ) in.m_move = stick;
    in.m_fire = down( BindE::FIRE );
    in.m_jump = hit( BindE::JUMP );
    in.m_stim = hit( BindE::STIM );
    in.m_special = hit( BindE::SPECIAL );
    in.m_skill2 = hit( BindE::SKILL2 );
    in.m_skill3 = hit( BindE::SKILL3 );
    in.m_strafe = down( BindE::STRAFE );
    in.m_dodge = hit( BindE::DODGE );
    in.m_swap = hit( BindE::SWAP );
    was[0] = now[0], was[1] = now[1];
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

constexpr int Slots = 4;    ///< places to save in
int           g_slot = 1;   ///< the one played (1 .. Slots; the first is save.txt as before)

/** @brief What the player set in the menus; kept in settings.txt in the player's folder. */
struct Settings
{
    DifficultyE m_difficulty = DifficultyE::NORMAL;   ///< how hard
    float       m_brightness = 1.0f;                  ///< exposure factor
    float       m_music = 0.7f;                       ///< music volume 0..1
    float       m_voices = 0.9f;                      ///< voices' volume 0..1
    float       m_effects = 0.8f;                     ///< effects' and backgrounds' volume 0..1
    int         m_quality = -1;        ///< the graphics: 0 low, 1 middle, 2 high, 3 frugal (-1: not chosen yet)
    int         m_subtitles = 1;       ///< 0 small, 1 normal, 2 large
    int         m_colours = 0;         ///< colour vision: 0 as it is, 1 red-green, 2 blue-yellow
    int         m_shake = 2;           ///< the camera's shake: 0 off, 1 half, 2 full
    int         m_loot = 0;            ///< the loot filter: 0 keep all, 1 salvage common, 2 and magic
    int         m_slot = 1;            ///< the save played last
    int         m_display = 0;         ///< 0 a window, 1 borderless over the whole screen
    int         m_size = 1;            ///< the window's size (WindowSizes)
    int         m_vsync = 1;           ///< the picture waits for the screen
    int         m_fps = -1;            ///< at most so many frames a second (0: no limit, -1: not chosen)
    int         m_hud = 1;             ///< the HUD's size (HudSteps)
    int         m_text = -1;           ///< the text size (TextSteps; -1: not chosen, larger on a Steam Deck)
    int         m_rumble = 2;          ///< the pad's motors: 0 still, 1 weak, 2 strong
    int         m_minimap = 1;         ///< the map round the hero in the HUD
    bool        m_nightmare = false;   ///< Albtraum may be chosen (the game is through once)
    std::string m_language;            ///< the texts' language
};

/** @brief The window's sizes to choose from (larger ones than the screen are passed over). */
const int   WindowSizes[5][2] = { { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } };
const int   FpsSteps[7] = { 0, 30, 40, 60, 90, 120, 144 };   ///< the frame limits (0: as many as the screen takes)
const float HudSteps[4] = { 0.8f, 1.0f, 1.2f, 1.4f };
const float TextSteps[4] = { 1.0f, 1.15f, 1.3f, 1.45f };   ///< the whole interface's text sizes

/** @brief Whether the game runs on a Steam Deck (Steam sets SteamDeck=1 there). */
bool steamDeck()
{
    const char *deck = getenv( "SteamDeck" );
    return deck && atoi( deck ) == 1;
}

/**
 * @brief The frame limit: the chosen one, else 40 on a Steam Deck (its battery; with the screen at 40 Hz in the quick
 *        menu the frames come even) and none elsewhere.
 * @return frames a second, 0 for no limit
 */
int fpsOf( const Settings &s )
{
    return s.m_fps >= 0 ? s.m_fps : ( steamDeck() ? 40 : 0 );
}

/**
 * @brief The text size: the chosen one, else 130 % on a Steam Deck (its seven inches at 800 lines made the texts
 *        small) and as it came elsewhere.
 * @return an index into TextSteps
 */
int textOf( const Settings &s )
{
    return s.m_text >= 0 ? s.m_text : ( steamDeck() ? 2 : 0 );
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
            s.m_quality = (int)Clamp( (float)atoi( value ), 0.0f, 3.0f );
        else if ( !strcmp( key, "subtitles" ) )
            s.m_subtitles = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "colours" ) )
            s.m_colours = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "shake" ) )
            s.m_shake = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "loot" ) )
            s.m_loot = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "display" ) )
            s.m_display = atoi( value ) ? 1 : 0;
        else if ( !strcmp( key, "size" ) )
            s.m_size = (int)Clamp( (float)atoi( value ), 0.0f, 4.0f );
        else if ( !strcmp( key, "vsync" ) )
            s.m_vsync = atoi( value ) ? 1 : 0;
        else if ( !strcmp( key, "framerate" ) )
            s.m_fps = (int)Clamp( (float)atoi( value ), 0.0f, 1000.0f );
        else if ( !strcmp( key, "textsize" ) )
            s.m_text = (int)Clamp( (float)atoi( value ), 0.0f, 3.0f );
        else if ( !strcmp( key, "minimap" ) )
            s.m_minimap = atoi( value ) ? 1 : 0;
        else if ( !strcmp( key, "rumble" ) )
            s.m_rumble = (int)Clamp( (float)atoi( value ), 0.0f, 2.0f );
        else if ( !strcmp( key, "hud" ) )
            s.m_hud = (int)Clamp( (float)atoi( value ), 0.0f, 3.0f );
        else if ( !strcmp( key, "slot" ) )
            s.m_slot = (int)Clamp( (float)atoi( value ), 1.0f, (float)Slots );
        else if ( !strcmp( key, "nightmare" ) )
            s.m_nightmare = atoi( value ) != 0;
    }
    fclose( f );
    completeBindings();
    return s;
}

/** @brief Where the game is saved (the start of the level to play). */
/** @return a slot's save */
std::string slotPath( int slot )
{
    return userDir() + ( slot <= 1 ? std::string( "save.txt" ) : TextFormat( "save%d.txt", slot ) );
}

std::string savePath()
{
    return slotPath( g_slot );
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
        else if ( !strcmp( key, "spare" ) )
            p.m_hasSpare = loadItem( rest, p.m_spare );
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
        else if ( !strcmp( key, "depths" ) )
            sscanf( rest, "%d %d", &p.m_depthTier, &p.m_depthBest );
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
    fprintf( f, "depths %d %d\n", p.m_depthTier, p.m_depthBest );
    if ( p.m_hasSpot )
    {
        fprintf( f, "spot %.2f %.2f %.2f\n", p.m_spot.x, p.m_spot.y, p.m_spot.z );
        for ( const std::string &e : p.m_fired ) fprintf( f, "fired %s\n", e.c_str() );
    }
    for ( size_t i = 0; i < p.m_ranks.size(); ++i )
        if ( p.m_ranks[i] > 0 ) fprintf( f, "rank %d %d\n", (int)i, p.m_ranks[i] );
    for ( int i = 0; i < EquipSlots; ++i )
        if ( p.m_worn[i] ) fprintf( f, "gear %d %s\n", i, saveItem( p.m_gear[i] ).c_str() );
    if ( p.m_hasSpare ) fprintf( f, "spare %s\n", saveItem( p.m_spare ).c_str() );
    for ( const Item &it : p.m_bag ) fprintf( f, "bag %s\n", saveItem( it ).c_str() );
    for ( const std::string &flag : p.m_flags ) fprintf( f, "flag %s\n", flag.c_str() );
    fprintf( f, "when %lld\n", (long long)time( nullptr ) );   // for the list of saves
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
    fprintf( f, "subtitles %d\ncolours %d\nshake %d\nloot %d\nslot %d\nnightmare %d\n", s.m_subtitles, s.m_colours,
             s.m_shake, s.m_loot, s.m_slot, s.m_nightmare ? 1 : 0 );
    fprintf( f, "display %d\nsize %d\nvsync %d\nhud %d\nrumble %d\nminimap %d\n", s.m_display, s.m_size, s.m_vsync,
             s.m_hud, s.m_rumble, s.m_minimap );
    if ( s.m_fps >= 0 ) fprintf( f, "framerate %d\n", s.m_fps );
    if ( s.m_text >= 0 ) fprintf( f, "textsize %d\n", s.m_text );
    for ( int b = 0; b < (int)BindE::COUNT; ++b ) fprintf( f, "bind %s\n", bindingLine( (BindE)b ).c_str() );
    endWrite( f, settingsPath() );
}

/**
 * @brief Tests (--menutest): the menu keys played from a script, a step every few frames, in a real window -- up,
 *        down, left, right, ok, back, waitN (frames), shot:NAME (the whole window into --menudir), quit.
 */
std::deque<std::string> g_script;
std::string             g_scriptShot, g_scriptDir;
int                     g_scriptWait = 0;
bool                    g_scriptQuit = false;

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
    if ( !g_script.empty() || g_scriptWait > 0 )
    {
        // The test's script instead of the keys.
        if ( g_scriptWait > 0 )
        {
            --g_scriptWait;
            return k;
        }
        std::string t = g_script.front();
        g_script.pop_front();
        k.m_move = t == "up" ? -1 : t == "down" ? 1 : 0;
        k.m_change = t == "left" ? -1 : t == "right" ? 1 : 0;
        k.m_confirm = t == "ok";
        k.m_back = t == "back";
        if ( t.rfind( "wait", 0 ) == 0 ) g_scriptWait = atoi( t.c_str() + 4 );
        if ( t.rfind( "shot:", 0 ) == 0 ) g_scriptShot = t.substr( 5 );
        g_scriptQuit = g_scriptQuit || t == "quit";
        g_scriptWait = std::max( g_scriptWait, 8 );
        return k;
    }
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

/**
 * @brief A frame of the loading screen, drawn while a level loads (the game stood still before and seemed to have
 *        crashed): the place's name, "Lädt …", a bar.
 * @param[in] share  how far the loading is (0 to 1)
 * @param[in] place  the place's name ("" while it is not known)
 */
void drawLoading( float share, const char *place )
{
    // Its own font: the game's is made anew with each level, at the end of its loading.
    static Font font = {};
    if ( font.texture.id == 0 )
    {
        std::string      own = assetsDir() + "/fonts/Barlow-Regular.ttf";
        std::string      path = FileExists( "C:/Windows/Fonts/bahnschrift.ttf" ) && !getenv( "SPACEOPERA_BARLOW" )
                                    ? std::string( "C:/Windows/Fonts/bahnschrift.ttf" )
                                    : own;
        std::vector<int> cps = textCodepoints();
        font = LoadFontEx( path.c_str(), 48, cps.data(), (int)cps.size() );
        SetTextureFilter( font.texture, TEXTURE_FILTER_BILINEAR );
    }
    int   w = GetScreenWidth(), h = GetScreenHeight();
    float s = textScale( h );
    BeginDrawing();
    ClearBackground( { 6, 6, 8, 255 } );
    auto line = [&]( const char *t, float y, float size, Color c )
    {
        Vector2 m = MeasureTextEx( font, t, size * s, 1.0f );
        DrawTextEx( font, t, { w * 0.5f - m.x * 0.5f, y }, size * s, 1.0f, c );
    };
    if ( place && *place ) line( place, h * 0.4f, 52, { 245, 236, 222, 255 } );
    line( tr( "loading" ), h * 0.4f + 72 * s, 26, { 255, 190, 120, 220 } );
    float bw = w * 0.36f, by = h * 0.4f + 120 * s;
    DrawRectangle( (int)( w * 0.5f - bw * 0.5f ), (int)by, (int)bw, (int)( 6 * s ), { 60, 52, 44, 255 } );
    DrawRectangle( (int)( w * 0.5f - bw * 0.5f ), (int)by, (int)( bw * Clamp( share, 0.0f, 1.0f ) ), (int)( 6 * s ),
                   { 255, 170, 80, 255 } );
    EndDrawing();
}

/** @brief Draws a menu over the frame: a title, a subtitle, items, the chosen one marked. */
void drawMenu( const Font &font, const char *title, const char *subtitle, const std::vector<std::string> &items,
               int chosen )
{
    int   w = GetScreenWidth(), h = GetScreenHeight();
    float s = textScale( h );
    DrawRectangle( 0, 0, w, h, { 0, 0, 0, 150 } );
    auto text = [&]( const char *t, float y, float size, Color c )
    {
        Vector2 m = MeasureTextEx( font, t, size * s, 1.0f );
        // A line wider than the window (a save's long place name) is set smaller until it fits.
        if ( m.x > w * 0.94f ) size *= w * 0.94f / m.x, m = MeasureTextEx( font, t, size * s, 1.0f );
        DrawTextEx( font, t, { w * 0.5f - m.x * 0.5f + 2 * s, y + 2 * s }, size * s, 1.0f, { 0, 0, 0, 200 } );
        DrawTextEx( font, t, { w * 0.5f - m.x * 0.5f, y }, size * s, 1.0f, c );
    };
    text( title, h * 0.2f, 84, { 245, 236, 222, 255 } );
    // A long subtitle (a hero's description) in two lines, broken at the space nearest its middle.
    std::string sub = subtitle;
    if ( MeasureTextEx( font, subtitle, 28 * s, 1.0f ).x > w * 0.8f && sub.find( ' ' ) != std::string::npos )
    {
        size_t mid = sub.size() / 2, cut = sub.rfind( ' ', mid );
        size_t after = sub.find( ' ', mid );
        if ( cut == std::string::npos || ( after != std::string::npos && after - mid < mid - cut ) ) cut = after;
        text( sub.substr( 0, cut ).c_str(), h * 0.2f + 92 * s, 28, { 255, 190, 120, 230 } );
        text( sub.substr( cut + 1 ).c_str(), h * 0.2f + 124 * s, 28, { 255, 190, 120, 230 } );
    }
    else
        text( subtitle, h * 0.2f + 96 * s, 28, { 255, 190, 120, 230 } );
    // The list ends above the hints at the bottom, however large the texts are.
    float step = fminf( fminf( 46.0f, 520.0f / items.size() ), h * 0.52f / ( s * items.size() ) );
    float size = fminf( 34.0f, step * 0.86f );
    for ( size_t i = 0; i < items.size(); ++i )
    {
        bool  on = (int)i == chosen;
        float y = h * 0.36f + i * step * s;   // above the game's subtitles
        if ( on )
            DrawRectangle( (int)( w * 0.5f - 260 * s ), (int)( y - 6 * s ), (int)( 520 * s ), (int)( 44 * s ),
                           { 255, 170, 80, 40 } );
        text( items[i].c_str(), y, size, on ? Color{ 255, 205, 140, 255 } : Color{ 200, 200, 200, 220 } );
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
    float s = textScale( h );
    DrawRectangle( 0, 0, w, h, { 0, 0, 0, 200 } );
    auto text = [&]( const std::string &t, float x, float y, float size, Color c, int align )
    {
        Vector2 m = MeasureTextEx( font, t.c_str(), size * s, 1.0f );
        float   at = align == 1 ? x - m.x * 0.5f : align == 2 ? x - m.x : x;
        DrawTextEx( font, t.c_str(), { at + 2 * s, y + 2 * s }, size * s, 1.0f, { 0, 0, 0, 200 } );
        DrawTextEx( font, t.c_str(), { at, y }, size * s, 1.0f, c );
    };
    text( tr( "menu.controls" ), w * 0.5f, 50 * s, 60, { 245, 236, 222, 255 }, 1 );
    float name = w * 0.5f - 430 * s, key = w * 0.5f + 150 * s, pad = w * 0.5f + 400 * s, top = 150 * s;
    // The rows close up where larger texts would run into the help line.
    int   count = (int)BindE::COUNT + 2;
    float row = fminf( 34 * s, ( h - 110 * s - top ) / count ), size = fminf( 26.0f, row / s * 0.78f );
    text( tr( "controls.key" ), key, top - 40 * s, 22, { 255, 170, 90, 230 }, 1 );
    text( tr( "controls.pad" ), pad, top - 40 * s, 22, { 255, 170, 90, 230 }, 1 );
    for ( int i = 0; i < count; ++i )
    {
        float y = top + i * row + ( i >= (int)BindE::COUNT ? 16 * s : 0.0f );
        bool  on = i == chosen;
        if ( on )
            DrawRectangle( (int)( name - 16 * s ), (int)( y - 4 * s ), (int)( pad - name + 140 * s ),
                           (int)( row * 0.94f ), { 255, 170, 80, 40 } );
        Color c = on ? Color{ 255, 205, 140, 255 } : Color{ 200, 200, 200, 220 };
        if ( i >= (int)BindE::COUNT )
        {
            text( tr( i == (int)BindE::COUNT ? "controls.defaults" : "menu.back" ), name, y, size, c, 0 );
            continue;
        }
        const Binding &b = binding( (BindE)i );
        text( tr( std::string( "bind." ) + bindName( (BindE)i ) ), name, y, size, c, 0 );
        if ( on && waiting )
            text( tr( "controls.press" ), ( key + pad ) * 0.5f, y, size, { 255, 220, 120, 255 }, 1 );
        else
        {
            text( keyLabel( b.m_key ), key, y, size, c, 1 );
            text( padLabel( b.m_pad ), pad, y, size, c, 1 );
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
/**
 * @brief The day's challenge in the Depths: by the local date the same hero, level, tier and layout for everyone who
 *        plays on that day; nothing of it is saved but its times (challenge.txt in the player's folder).
 */
struct Daily
{
    int   m_date = 0;               ///< YYYYMMDD, the seed too
    HeroE m_hero = HeroE::CORVIN;   ///< who is played
    int   m_level = 10;             ///< the hero's level (10 to 30)
    int   m_tier = 3;               ///< the Depths' tier (3 to 8)
};

/** @return today's challenge */
Daily today()
{
    time_t      now = time( nullptr );
    struct tm   t = *localtime( &now );
    Daily       d;
    unsigned    h = (unsigned)( t.tm_year * 366 + t.tm_yday ) * 2654435761u;
    const HeroE heroes[3] = { HeroE::CORVIN, HeroE::DORRA, HeroE::IONE };
    d.m_date = ( t.tm_year + 1900 ) * 10000 + ( t.tm_mon + 1 ) * 100 + t.tm_mday;
    d.m_hero = heroes[h % 3];
    d.m_level = 10 + 5 * (int)( h / 3 % 5 );
    d.m_tier = 3 + (int)( h / 15 % 6 );
    return d;
}

/** @return a run's score: its time with half a minute more for each death (lower is better) */
float dailyScore( float seconds, int deaths )
{
    return seconds + 30.0f * deaths;
}

/** @return a time as minutes and seconds */
std::string clockOf( float seconds )
{
    return TextFormat( "%d:%02d", (int)seconds / 60, (int)seconds % 60 );
}

/**
 * @brief The best score of a day's runs so far.
 * @param[in]  date  the day
 * @param[out] runs  how many
 * @return the best score (negative: none)
 */
float dailyBest( int date, int &runs )
{
    float best = -1.0f;
    runs = 0;
    if ( FILE *f = fopen( ( userDir() + "challenge.txt" ).c_str(), "r" ) )
    {
        int   day, level, tier, deaths, kills;
        char  hero[32];
        float seconds;
        while ( fscanf( f, "%d %31s %d %d %f %d %d", &day, hero, &level, &tier, &seconds, &deaths, &kills ) == 7 )
            if ( day == date )
            {
                float score = dailyScore( seconds, deaths );
                if ( best < 0.0f || score < best ) best = score;
                ++runs;
            }
        fclose( f );
    }
    return best;
}

std::string g_dailyRow;    ///< the title's row for the challenge (refreshDaily)
std::string g_dailyLang;   ///< ... in this language
int         g_dailyDate;   ///< ... for this day

/** @brief The title's row for today's challenge: who, how strong, how deep, the best run so far. */
void refreshDaily()
{
    Daily d = today();
    int   runs = 0;
    float best = dailyBest( d.m_date, runs );
    g_dailyLang = language();
    g_dailyDate = d.m_date;
    g_dailyRow =
        TextFormat( tr( "menu.daily" ), tr( std::string( "hero." ) + heroKey( d.m_hero ) ), d.m_level, d.m_tier );
    g_dailyRow += "  ·  " + ( runs ? std::string( TextFormat( tr( "daily.best" ), clockOf( best ).c_str(), runs ) )
                                   : std::string( tr( "daily.none" ) ) );
}

/**
 * @brief A run of the challenge through: written to the list, the row refreshed.
 * @return what the title says about it (the time, the deaths, the score, whether it is the day's best)
 */
std::string recordDaily( const Daily &d, float seconds, int deaths, int kills )
{
    int   runs = 0;
    float best = dailyBest( d.m_date, runs );
    if ( FILE *f = fopen( ( userDir() + "challenge.txt" ).c_str(), "a" ) )
    {
        fprintf( f, "%d %s %d %d %.1f %d %d\n", d.m_date, heroKey( d.m_hero ), d.m_level, d.m_tier, seconds, deaths,
                 kills );
        fclose( f );
    }
    refreshDaily();
    float score = dailyScore( seconds, deaths );
    return TextFormat( tr( best < 0.0f || score < best ? "daily.record" : "daily.done" ), clockOf( seconds ).c_str(),
                       deaths, clockOf( score ).c_str() );
}

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
    LOAD,
    DAILY,
    DISPLAY,
    WINDOW,
    SIZE,
    VSYNC,
    FPS,
    TEXTSIZE,
    HUD,
    RUMBLE,
    MINIMAP,
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
std::vector<RowE> menuRows( int menu, bool saved, bool plus, bool any = false )
{
    using R = RowE;
    if ( menu == 3 ) return { R::WINDOW, R::SIZE, R::VSYNC, R::FPS, R::TEXTSIZE, R::HUD, R::MINIMAP, R::BACK };
    if ( menu == 2 )
        return { R::DIFFICULTY, R::BRIGHTNESS, R::GRAPHICS, R::DISPLAY, R::MUSIC, R::VOICES,   R::EFFECTS, R::LANGUAGE,
                 R::SUBTITLES,  R::COLOURS,    R::SHAKE,    R::RUMBLE,  R::LOOT,  R::CONTROLS, R::BACK };
    if ( menu == 1 ) return { R::RESUME, R::PHOTO, R::OPTIONS, R::RESTART, R::QUIT };
    std::vector<RowE> rows;
    if ( saved ) rows.push_back( R::CONTINUE );
    if ( plus ) rows.push_back( R::PLUS );
    rows.push_back( R::START );
    if ( any ) rows.push_back( R::LOAD );
    rows.push_back( R::DAILY );
    for ( R r : { R::OPTIONS, R::QUIT } ) rows.push_back( r );
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
    case RowE::LOAD: return tr( "menu.load" );
    case RowE::DAILY:
        if ( g_dailyRow.empty() || g_dailyLang != language() || g_dailyDate != today().m_date ) refreshDaily();
        return g_dailyRow;
    case RowE::DISPLAY: return tr( "menu.display" );
    case RowE::RUMBLE: return pick( "menu.rumble", "rumble." + std::to_string( s.m_rumble ) );
    case RowE::MINIMAP: return pick( "menu.minimap", s.m_minimap ? "on" : "off" );
    case RowE::WINDOW: return pick( "menu.window", "window." + std::to_string( s.m_display ) );
    case RowE::SIZE:
        return std::string( tr( "menu.size" ) ) + "  " +
               TextFormat( "%d x %d", WindowSizes[s.m_size][0], WindowSizes[s.m_size][1] );
    case RowE::VSYNC: return pick( "menu.vsync", s.m_vsync ? "on" : "off" );
    case RowE::FPS:
        return std::string( tr( "menu.fps" ) ) + "  " +
               ( fpsOf( s ) ? std::to_string( fpsOf( s ) ) : std::string( tr( "fps.free" ) ) );
    case RowE::TEXTSIZE:
        return std::string( tr( "menu.textsize" ) ) + "  " +
               std::to_string( (int)roundf( TextSteps[textOf( s )] * 100 ) ) + " %";
    case RowE::HUD:
        return std::string( tr( "menu.hud" ) ) + "  " + std::to_string( (int)( HudSteps[s.m_hud] * 100 ) ) + " %";
    default: return tr( "menu.back" );
    }
}

/**
 * @brief A hero carried into a new run: as they are (level, gear, skills, scrap, the workshop, the Depths), the story
 *        from its start, the notes read still read, the cycle one further.
 * @param[in] done   the finished run's save
 * @param[in] start  where the story starts
 * @return the new run's start
 */
Progress carriedFrom( const Progress &done, const std::string &start )
{
    Progress p = done;
    p.m_cycle += 1;
    std::set<std::string> kept;
    for ( const std::string &f : p.m_flags )
        if ( f.compare( 0, 5, "note:" ) == 0 || f == "gamedone" || f == "hardcore" ) kept.insert( f );
    p.m_flags = kept;
    p.m_deaths = 0;
    p.m_hasSpot = false;
    p.m_fired.clear();
    p.m_level = start;
    return p;
}

/** @return the name of the place a save is at (its level's "name" key, read from the file) */
std::string placeName( const std::string &level )
{
    char *text = LoadFileText( ( assetsDir() + "/levels/" + level ).c_str() );
    if ( !text ) return level;
    std::stringstream in( text );
    UnloadFileText( text );
    std::string line;
    while ( std::getline( in, line ) )
        if ( line.compare( 0, 5, "name " ) == 0 )
        {
            std::string key = line.substr( 5 );
            key.erase( 0, key.find_first_not_of( " \t" ) );
            while ( !key.empty() && ( key.back() == '\r' || key.back() == ' ' ) ) key.pop_back();
            return tr( key );
        }
    return level;
}

/**
 * @brief A slot as it reads in the list: who, how far, where, when; or that it is free.
 * @param[in] slot  1 .. Slots
 * @param[out] used whether there is a save
 * @return the line
 */
/** @return whether the save in a slot is a fallen hero's (one life, spent): it shows, but does not go on */
bool fallenIn( int slot )
{
    Progress p;
    return loadProgress( p, slotPath( slot ) ) && p.m_flags.count( "fallen" );
}

std::string slotLine( int slot, bool &used )
{
    Progress p;
    used = loadProgress( p, slotPath( slot ) );
    if ( !used ) return TextFormat( "%d   %s", slot, tr( "slot.empty" ) );
    long long when = 0;
    if ( FILE *f = fopen( slotPath( slot ).c_str(), "r" ) )
    {
        char line[1024];
        while ( fgets( line, sizeof( line ), f ) )
            if ( sscanf( line, "when %lld", &when ) == 1 ) break;
        fclose( f );
    }
    char   stamp[32] = "";
    time_t t = (time_t)when;
    if ( when > 0 ) strftime( stamp, sizeof( stamp ), "%d.%m. %H:%M", localtime( &t ) );
    std::string line =
        TextFormat( tr( p.m_flags.count( "fallen" ) ? "slot.fallen" : "slot.line" ), slot,
                    tr( std::string( "hero." ) + heroKey( p.m_hero ) ), p.m_heroLevel, placeName( p.m_level ).c_str() );
    if ( p.m_flags.count( "hardcore" ) && !p.m_flags.count( "fallen" ) )
        line += std::string( "  ·  " ) + tr( "slot.onelife" );
    return when > 0 ? line + "  ·  " + stamp : line;
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
    g_slot = settings.m_slot;
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
    bool        hasSave = o.m_shot.empty() && loadProgress( saved ) && !saved.m_flags.count( "fallen" );
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
    if ( o.m_tier > 0 ) progress.m_depthTier = o.m_tier, progress.m_depthBest = o.m_tier - 1;
    if ( !o.m_loadFile.empty() && loadProgress( progress, o.m_loadFile ) )
        level = progress.m_level, hero = progress.m_hero, game->m_heroKind = hero;
    if ( o.m_plus && !o.m_loadFile.empty() ) progress = carriedFrom( progress, level = o.m_level );
    if ( o.m_colours >= 0 ) settings.m_colours = o.m_colours;
    if ( o.m_lootFilter >= 0 ) settings.m_loot = o.m_lootFilter;
    game->m_progress = progress;
    game->m_genSeed = o.m_seed;
    if ( o.m_crash )
    {
        volatile int *nowhere = nullptr;
        *nowhere = 1;   // the crash report's test
    }
    if ( !o.m_difficulty.empty() )
        settings.m_difficulty = o.m_difficulty == "leicht" ? DifficultyE::EASY
                                                           : ( o.m_difficulty == "schwer"     ? DifficultyE::HARD
                                                               : o.m_difficulty == "albtraum" ? DifficultyE::NIGHTMARE
                                                                                              : DifficultyE::NORMAL );
    game->setDifficulty( settings.m_difficulty );   // before the load: the places keep up by it
    if ( o.m_shot.empty() ) game->m_loading = drawLoading;
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
    // Tests: a second hero beside the first, played by the autopilot.
    HeroE coopHero = o.m_coop == "dorra" ? HeroE::DORRA : ( o.m_coop == "ione" ? HeroE::IONE : HeroE::CORVIN );
    bool  coop = !o.m_coop.empty();
    auto  partnerFor = [&]( HeroE leader )
    {
        // The one asked for, else the next of the other two.
        if ( coopHero != leader ) return coopHero;
        return leader == HeroE::CORVIN ? HeroE::DORRA : HeroE::CORVIN;
    };
    if ( coop ) game->joinPartner( partnerFor( hero ), o.m_autoplay );
    if ( !o.m_dumpGen.empty() && !game->generated().empty() )
        SaveFileText( o.m_dumpGen.c_str(), const_cast<char *>( game->generated().c_str() ) );
    // What the settings ask of a game and the renderer.
    bool challengeOn = false;   ///< apply() keeps the day's challenge on Normal (set from the loop's challenge)
    auto apply = [&]()
    {
        game->setDifficulty( challengeOn ? DifficultyE::NORMAL : settings.m_difficulty );
        game->m_brightness = settings.m_brightness;
        game->setVolumes( settings.m_voices, settings.m_effects );
        game->m_subtitleScale = settings.m_subtitles == 0 ? 0.8f : ( settings.m_subtitles == 2 ? 1.3f : 1.0f );
        game->m_shakeScale = settings.m_shake * 0.5f;
        game->m_lootFilter = settings.m_loot;
        game->m_hudScale = HudSteps[settings.m_hud];
        setTextSize( TextSteps[textOf( settings )] );
        game->m_minimap = settings.m_minimap != 0;
        gfx.m_colourVision = settings.m_colours;
        // The shared stash: only for real play (tests keep theirs in memory).
        if ( o.m_shot.empty() && !o.m_autoplay && game->m_stashPath.empty() )
            game->loadStash( userDir() + "stash.txt" );
    };
    apply();
    game->m_autoplay = o.m_autoplay;
    game->m_camYaw = o.m_camYaw * DEG2RAD;
    if ( o.m_camRise > 0.0f ) game->m_camRise = o.m_camRise;
    if ( o.m_hasAt ) game->putHero( o.m_x, o.m_z, o.m_face );
    if ( o.m_lineup ) game->lineup();
    if ( o.m_parade ) game->parade();
    if ( !o.m_hunt.empty() ) game->hunt( o.m_hunt );
    if ( o.m_creatures >= 0 ) game->creatureLineup( o.m_creatures );
    if ( o.m_figures >= 0 ) game->figureLineup( o.m_figures );
    if ( o.m_zoom > 0.0f ) game->m_zoom = o.m_zoom;
    if ( o.m_loot > 0 || o.m_xp > 0 ) game->testGifts( o.m_loot, o.m_xp );
    if ( !o.m_gear.empty() ) game->testGear( o.m_gear );
    if ( o.m_awake ) game->wakeAll();
    if ( !o.m_film.empty() ) game->startFilm( o.m_film );
    auto draw = [&]( const RenderTexture2D *rt ) { game->render( gfx, rt ); };

    if ( !o.m_shot.empty() && o.m_campaign )
    {
        // The campaign test: the story's fighting places in order from --level on, the autopilot playing each for at
        // most --warm seconds; in between a player's ways (better gear on, the rest salvaged, stims and the
        // workshop); a CAMPAIGN line per place in the log.
        const char *const Path[] = { "gute_hoffnung.txt", "kuehlgedaerme.txt",  "knochenmarkt.txt", "aussenhaut.txt",
                                     "ertrunken.txt",     "kernbank.txt",       "seren.txt",        "hesper.txt",
                                     "halden.txt",        "arche.txt",          "tesh.txt",         "kael.txt",
                                     "wanderer.txt",      "kuehltransport.txt", "anhoerung.txt",    "sterbegaerten.txt",
                                     "werft.txt",         "rechenmond.txt",     "verlies.txt",      "velm2.txt",
                                     "brannt.txt",        "erzstrom.txt",       "muehle.txt",       "leitwerk.txt",
                                     "netz.txt" };
        const int         count = (int)( sizeof( Path ) / sizeof( Path[0] ) );
        int               first = 0;
        for ( int k = 0; k < count; ++k )
            if ( level == Path[k] ) first = k;
        Progress carried = game->progress();
        carried.m_hasSpot = false;
        int last = o.m_places > 0 ? std::min( count, first + o.m_places ) : count;
        for ( int k = first; k < last; ++k )
        {
            if ( k > first )
            {
                game->shutdown();
                game = std::make_unique<Game>();
                game->m_heroKind = hero;
                carried.m_level = Path[k];
                game->m_progress = carried;
                game->setDifficulty( settings.m_difficulty );
                if ( !game->load( gfx, assetsDir(), Path[k], sound ) ) break;
                if ( coop ) game->joinPartner( partnerFor( hero ), true );
                if ( o.m_silent ) SetMasterVolume( 0.0f );   // each place's sound sets its volume again
                apply();
            }
            // The hero as they come to this place, to play it again alone (--loadfile, --places 1).
            if ( !o.m_keep.empty() )
            {
                Progress before = carried;
                before.m_level = Path[k];
                saveProgress( before, o.m_keep + "/" + Path[k] );
            }
            game->m_autoplay = true;
            game->m_unstall = true;
            game->campaignPrep();
            const float dt = 1.0f / 60.0f;
            for ( int i = 0; i < (int)( o.m_warm * 60.0f ) && !game->finished() && !game->fallen(); ++i )
            {
                GameInput in;
                game->update( dt, in );
                game->takeStings();
                if ( o.m_trace > 0.0f && i % std::max( 1, (int)( o.m_trace * 60.0f ) ) == 0 )
                    TraceLog( LOG_WARNING, "TRACE: %s %s", Path[k], game->report().c_str() );
            }
            TraceLog( LOG_WARNING, "CAMPAIGN: %s", game->campaignLine().c_str() );
            if ( game->fallen() )
            {
                TraceLog( LOG_WARNING, "FALLEN: %s", Path[k] );   // one life (--flags hardcore): the run ends here
                break;
            }
            carried = game->progress();
            carried.m_hasSpot = false;
            carried.m_fired.clear();
        }
        // The hero at the end, for a run after it (--loadfile with --plus).
        carried.m_flags.insert( "gamedone" );
        if ( carried.m_level.compare( 0, 4, "end " ) == 0 )
            carried.m_level = carried.m_level.substr( 4 );   // as play does
        if ( !o.m_saveFile.empty() ) saveProgress( carried, o.m_saveFile );
        game->shutdown();
        score.shutdown();
        return 0;
    }
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
            if ( o.m_clip > 0.0f && i >= frames - (int)( o.m_clip * 60.0f ) && i % 4 == 0 )
            {
                // A series to watch motion by: <shot>_0000.png on.
                std::string base = o.m_shot.substr( 0, o.m_shot.rfind( '.' ) );
                savePicture( gfx, TextFormat( "%s_%04d.png", base.c_str(), i / 4 ), draw );
            }
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
                             game->preparePanel( gfx, o.m_panel, 0.0f, rt->texture.width, rt->texture.height );
                             BeginTextureMode( *rt );
                             game->drawPanel( rt->texture.width, rt->texture.height, o.m_panel, 0 );
                             EndTextureMode();
                         } );
        else if ( o.m_slots )
            savePicture( gfx, o.m_shot,
                         [&]( const RenderTexture2D *rt )
                         {
                             game->render( gfx, rt );
                             BeginTextureMode( *rt );
                             std::vector<std::string> items;
                             for ( int i = 1; i <= Slots; ++i )
                             {
                                 bool used = false;
                                 items.push_back( slotLine( i, used ) );
                             }
                             items.push_back( tr( "menu.back" ) );
                             drawMenu( game->font(), "DARK CONCORDANCE", tr( "menu.slots.load" ), items, 0 );
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
        // Tests: more places in the same window (--tour "a.txt;b.txt@x,z"), each loaded anew with the progress the
        // first had and warmed up as long; one with @x,z has the hero put there (no autopilot). Each picture is
        // <shot>_<place>.png (_at.png when put), each run's report a RUN line with its place, each benched by --bench.
        for ( size_t at = 0; at < o.m_tour.size(); )
        {
            size_t end = o.m_tour.find( ';', at );
            if ( end == std::string::npos ) end = o.m_tour.size();
            std::string entry = o.m_tour.substr( at, end - at ), place = entry;
            at = end + 1;
            // ("#mask" at its end: what to leave out of its picture, Gfx::OffE's bits; its name gets "_off<mask>".)
            int    off = 0;
            size_t hash = entry.find( '#' );
            if ( hash != std::string::npos )
                off = atoi( entry.c_str() + hash + 1 ), entry = place = entry.substr( 0, hash );
            float  x = 0.0f, z = 0.0f;
            size_t mark = entry.find( '@' );
            bool   put = mark != std::string::npos && sscanf( entry.c_str() + mark + 1, "%f,%f", &x, &z ) == 2;
            if ( mark != std::string::npos ) place = entry.substr( 0, mark );
            game->shutdown();
            game = std::make_unique<Game>();
            game->m_heroKind = hero;
            Progress fresh = progress;
            fresh.m_level = place;
            game->m_progress = fresh;
            game->m_genSeed = o.m_seed;
            game->setDifficulty( settings.m_difficulty );
            if ( !game->load( gfx, assetsDir(), place, sound ) )
            {
                TraceLog( LOG_ERROR, "TOUR: %s not found", place.c_str() );
                break;
            }
            if ( !o.m_dumpGen.empty() && !game->generated().empty() )
                SaveFileText( ( o.m_dumpGen.substr( 0, o.m_dumpGen.rfind( '.' ) ) + "_" + place ).c_str(),
                              const_cast<char *>( game->generated().c_str() ) );
            apply();
            game->m_autoplay = o.m_autoplay && !put;
            game->m_camYaw = o.m_camYaw * DEG2RAD;
            if ( o.m_camRise > 0.0f ) game->m_camRise = o.m_camRise;
            if ( put ) game->putHero( x, z, o.m_face );
            if ( o.m_zoom > 0.0f ) game->m_zoom = o.m_zoom;
            for ( int i = 0; i < frames && !( o.m_report && game->finished() ); ++i )
            {
                GameInput in;
                game->update( dt, in );
                game->takeStings();
            }
            if ( o.m_report ) TraceLog( LOG_WARNING, "RUN: %s %s", place.c_str(), game->report().c_str() );
            if ( o.m_bench > 0 )
            {
                TraceLog( LOG_WARNING, "TOUR: bench %s", place.c_str() );
                bench( gfx, o.m_bench, o, draw );
            }
            std::string base = o.m_shot.substr( 0, o.m_shot.rfind( '.' ) );
            gfx.m_off = off;
            savePicture( gfx,
                         base + "_" + place.substr( 0, place.rfind( '.' ) ) + ( put ? "_at" : "" ) +
                             ( off ? "_off" + std::to_string( off ) : "" ) + ".png",
                         draw );
            gfx.m_off = 0;
        }
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
        PHOTO,
        SLOTS,
        SCREEN,
        CARRY
    } mode = o.m_autoplay ? PLAY : TITLE;
    std::vector<int> finished;   ///< for a new game: the slots whose run with this hero is through (CARRY)
    auto             menuOf = [&]() { return mode == SCREEN ? 3 : mode == OPTIONS ? 2 : ( mode == TITLE ? 0 : 1 ); };
    // The window as the settings have it (not for pictures, nor on a Deck): borderless over the screen or a window
    // of a size, the wait for the screen or a cap on the frames; the renderer's targets follow.
    auto applyDisplay = [&]()
    {
        if ( !o.m_shot.empty() ) return;
        bool borderless = IsWindowState( FLAG_BORDERLESS_WINDOWED_MODE );
        if ( settings.m_display == 1 && !borderless ) ToggleBorderlessWindowed();
        if ( settings.m_display == 0 && !steamDeck() )
        {
            if ( borderless ) ToggleBorderlessWindowed();
            int monitor = GetCurrentMonitor();
            int w = WindowSizes[settings.m_size][0], h = WindowSizes[settings.m_size][1];
            SetWindowSize( w, h );
            SetWindowPosition( std::max( 0, ( GetMonitorWidth( monitor ) - w ) / 2 ),
                               std::max( 0, ( GetMonitorHeight( monitor ) - h ) / 2 ) );
        }
        if ( settings.m_vsync )
            SetWindowState( FLAG_VSYNC_HINT );
        else
            ClearWindowState( FLAG_VSYNC_HINT );
        SetTargetFPS( fpsOf( settings ) );
        gfx.resize( GetScreenWidth(), GetScreenHeight() );
    };
    applyDisplay();
    float       leaveHeld = 0.0f;    ///< the second pad's View held this long (to leave the co-op)
    bool        challenge = false;   ///< the day's challenge is played (nothing is saved)
    Daily       daily;               ///< which
    std::string notice;              ///< the title's line about the last challenge
    refreshDaily();
    bool slotNew = false;   ///< the slots' list: for a new game (else to load one)
    bool oneLife = false;   ///< a new game with one life (the slots' list for a new game asks)
    bool closed = false;    ///< a hero with one life fell: their save is closed
    int  armed = -1;        ///< a used slot chosen for a new game once: confirmed again, it is overwritten
    auto anySave = [&]()
    {
        bool used = false;
        for ( int i = 1; i <= Slots && !used; ++i ) slotLine( i, used );
        return o.m_shot.empty() && used;
    };
    bool        photoHelp = true;                                    ///< the photo mode's help shows
    float       photoToast = 0.0f;                                   ///< a picture was saved: how long it is still said
    std::string photoSaved;                                          ///< where
    bool        waiting = false;                                     ///< the controls' page waits for a key or button
    std::string crashed = o.m_shot.empty() ? crash::unseen() : "";   ///< a report from the time before
    auto        optionsFrom = TITLE;                                 // where the options go back to
    int         chosen = 0;
    int         panelTab = 0, panelRow = 0;   ///< the inventory, character and map pages: which, and the chosen row
    float       heroTurn = 0.0f;              ///< the gear pages: how far the view is turned round the hero
    bool        quit = false;
    // Outside play the game waits, so its picture is drawn once and shown behind the menus: again when a setting
    // changes it, and four times a second behind the inventory, where gear changes the hero's looks.
    RenderTexture2D still = {};
    bool            stillValid = false;
    float           stillAge = 0.0f;
    auto            restart = [&]()
    {
        stillValid = false;
        closed = false;
        float zoom = game->m_zoom;
        game->shutdown();
        game = std::make_unique<Game>();
        game->m_heroKind = hero;
        game->m_progress = progress;
        game->setDifficulty( challenge ? DifficultyE::NORMAL
                                       : settings.m_difficulty );   // the challenge: alike for all
        if ( o.m_shot.empty() ) game->m_loading = drawLoading;
        SetTargetFPS( 0 );   // the loading screen's frames wait for nothing
        game->load( gfx, assetsDir(), level, sound );
        if ( challenge ) game->challengePrep( (unsigned)daily.m_date );
        if ( coop ) game->joinPartner( partnerFor( hero ), o.m_autoplay && !o.m_coop.empty() );
        apply();
        game->m_autoplay = o.m_autoplay;
        game->m_zoom = zoom;
        score.keep( game->musicPieces() );
        if ( o.m_silent ) SetMasterVolume( 0.0f );
    };
    // The day's challenge: the day's hero at the day's level in the Depths at the day's tier, on Normal.
    auto startDaily = [&]()
    {
        daily = today();
        challenge = true;
        notice.clear();
        progress = Progress();
        progress.m_hero = hero = daily.m_hero;
        progress.m_heroLevel = daily.m_level;
        progress.m_depthTier = daily.m_tier;
        progress.m_depthBest = daily.m_tier - 1;
        progress.m_seed = daily.m_date;
        progress.m_level = level = "tiefen.txt";
        restart();
        mode = PLAY;
    };
    if ( o.m_daily ) startDaily();
    while ( !WindowShouldClose() && !quit && !g_scriptQuit )
    {
        // The pad's motors while it is played (still in the menus, the inventory, the photo mode).
        {
            float low = 0.0f, high = 0.0f, strength = settings.m_rumble * 0.5f;
            if ( mode == PLAY && strength > 0.0f && IsGamepadAvailable( 0 ) ) game->rumbleNow( low, high );
            rumble::set( low * strength, high * strength );
        }
        // The buttons in the texts follow the device last used.
        updateBindings();
        if ( noteInput() ) game->refreshFont();
        float dt = fminf( GetFrameTime(), 0.05f );
        if ( mode == PLAY )
        {
            GameInput in = readInput( lastMouse, dt );
            challengeOn = challenge;
            // Two at one screen: the second pad's Start joins (or changes the hero), its View held leaves.
            if ( IsGamepadAvailable( 1 ) && !game->finished() )
            {
                if ( IsGamepadButtonPressed( 1, GAMEPAD_BUTTON_MIDDLE_RIGHT ) )
                {
                    HeroE next = game->partner() ? game->partnerKind() : hero;
                    do next = (HeroE)( ( (int)next + 1 ) % 3 );
                    while ( next == hero );
                    coop = true, coopHero = next;
                    game->joinPartner( next );
                }
                leaveHeld = IsGamepadButtonDown( 1, GAMEPAD_BUTTON_MIDDLE_LEFT ) ? leaveHeld + dt : 0.0f;
                if ( leaveHeld > 1.5f && game->partner() ) game->leavePartner(), coop = false;
            }
            if ( game->partner() && !o.m_autoplay ) game->m_partnerIn = readPartner( 1 );
            bool pause = IsKeyPressed( KEY_ESCAPE ) ||
                         ( IsGamepadAvailable( 0 ) && IsGamepadButtonPressed( 0, GAMEPAD_BUTTON_MIDDLE_RIGHT ) );
            // The inventory, the character page (as bound); the game waits meanwhile.
            bool inventory = pressed( BindE::INVENTORY ), character = pressed( BindE::CHARACTER );
            if ( game->fallen() )
                pause = inventory = character = false;   // one life spent: nothing left but the way to the title
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
                panelTab = character ? Game::TabCharacter : 0;
                heroTurn = 0.0f;
                panelRow = 0;
            }
            else
            {
                game->update( dt, in );
                // A capsule woke: the game is saved there.
                if ( game->takeCheckpoint() && o.m_hero.empty() && !challenge )
                {
                    progress = game->checkpoint();
                    saveProgress( progress );
                    saved = progress;
                    hasSave = true;
                }
                game->m_zoom = Clamp( game->m_zoom * ( 1.0f + zoomStep( dt ) ), 0.55f, 1.7f );
                bool onward = game->autoAdvance() ? game->finishedFor() > 1.2f : in.m_confirm;
                // One life spent: the save is closed at once (quitting now changes nothing), the title after the card.
                if ( game->fallen() && !closed )
                {
                    closed = true;
                    if ( o.m_hero.empty() && loadProgress( saved ) )
                        saved.m_flags.insert( "fallen" ), saveProgress( saved );
                    hasSave = false;
                    if ( o.m_autoplay ) TraceLog( LOG_WARNING, "FALLEN: %s", level.c_str() ), quit = true;
                }
                if ( game->fallen() && game->fallenFor() > 4.0f && in.m_confirm ) mode = TITLE, chosen = 0;
                if ( game->finished() && onward && challenge )
                {
                    // The day's challenge through: its time into the list, the title again (nothing is saved).
                    notice = recordDaily( daily, game->playTime(), game->levelDeaths(), game->levelKills() );
                    challenge = challengeOn = false;
                    restart();
                    mode = TITLE, chosen = 0;
                    if ( o.m_autoplay ) TraceLog( LOG_WARNING, "DAILY: %s", notice.c_str() ), quit = true;
                }
                else if ( game->finished() && onward )
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
            // The hero on a gear page turns with the right stick, Q and E, or the right mouse button held.
            float spin = pad ? GetGamepadAxisMovement( 0, GAMEPAD_AXIS_RIGHT_X ) : 0.0f;
            if ( fabsf( spin ) < 0.2f ) spin = 0.0f;
            spin += ( IsKeyDown( KEY_E ) ? 1.0f : 0.0f ) - ( IsKeyDown( KEY_Q ) ? 1.0f : 0.0f );
            heroTurn += spin * 2.4f * dt;
            if ( IsMouseButtonDown( MOUSE_BUTTON_RIGHT ) ) heroTurn += GetMouseDelta().x * 0.01f;
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
                hero = chosen == 0 ? HeroE::DORRA : ( chosen == 1 ? HeroE::CORVIN : HeroE::IONE );
                // A run with this hero through somewhere: begin anew, or carry the hero over (CARRY asks).
                finished.clear();
                for ( int i = 1; i <= Slots; ++i )
                {
                    Progress p;
                    if ( loadProgress( p, slotPath( i ) ) && p.m_hero == hero && p.m_flags.count( "gamedone" ) )
                        finished.push_back( i );
                }
                k.m_confirm = false;
                if ( !finished.empty() )
                    mode = CARRY, chosen = 0;
                else
                    k.m_confirm = true;
            }
            if ( k.m_confirm )
            {
                // A new game: from the prologue, with nothing (and with one life if the slots' list said so).
                level = o.m_level;
                progress = Progress();
                progress.m_hero = hero;
                progress.m_level = level;
                if ( oneLife ) progress.m_flags.insert( "hardcore" );
                saveProgress( progress );
                hasSave = true;
                restart();
                mode = PLAY;
            }
            if ( k.m_back )
            {
                mode = SLOTS;
                chosen = g_slot - 1;
            }
            lastMouse = GetMousePosition();
        }
        else if ( mode == CARRY )
        {
            // Anew, or with a hero whose run is through (as they are, the story from its start); the difficulty with
            // left and right on its row.
            MenuKeys k = menuKeys( dt );
            int      count = (int)finished.size() + 3, difficulty = (int)finished.size() + 1;
            chosen = ( chosen + k.m_move + count ) % count;
            int step = chosen == difficulty ? ( k.m_change != 0 ? k.m_change : ( k.m_confirm ? 1 : 0 ) ) : 0;
            if ( step )
            {
                int n = settings.m_nightmare ? 4 : 3;
                settings.m_difficulty = (DifficultyE)( ( (int)settings.m_difficulty + step + n ) % n );
                saveSettings( settings );
                apply();
            }
            else if ( k.m_confirm && chosen == count - 1 )
                k.m_back = true;
            else if ( k.m_confirm )
            {
                level = o.m_level;
                if ( chosen == 0 )
                {
                    progress = Progress();
                    progress.m_hero = hero;
                    progress.m_level = level;
                    if ( oneLife ) progress.m_flags.insert( "hardcore" );
                }
                else
                {
                    Progress done;
                    loadProgress( done, slotPath( finished[chosen - 1] ) );
                    progress = carriedFrom( done, level );
                    journal::win( "plus" );
                }
                saveProgress( progress );
                saved = progress;
                hasSave = true;
                restart();
                mode = PLAY;
            }
            if ( k.m_back ) mode = CHOOSE, chosen = hero == HeroE::DORRA ? 0 : ( hero == HeroE::IONE ? 2 : 1 );
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
        else if ( mode == SLOTS )
        {
            // The four saves and back. A new game in a free one at once, in a used one on a second confirm.
            MenuKeys k = menuKeys( dt );
            int      count = Slots + ( slotNew ? 2 : 1 );   // a new game: the row for one life before back
            int      before = chosen;
            chosen = ( chosen + k.m_move + count ) % count;
            if ( chosen != before ) armed = -1;
            if ( slotNew && chosen == Slots && ( k.m_confirm || k.m_change ) )
                oneLife = !oneLife;
            else if ( k.m_confirm && chosen == count - 1 )
                k.m_back = true;
            else if ( k.m_confirm )
            {
                bool used = false;
                slotLine( chosen + 1, used );
                if ( slotNew && ( !used || armed == chosen ) )
                {
                    g_slot = settings.m_slot = chosen + 1;
                    saveSettings( settings );
                    mode = CHOOSE;
                    chosen = hero == HeroE::DORRA ? 0 : ( hero == HeroE::IONE ? 2 : 1 );
                }
                else if ( slotNew )
                    armed = chosen;
                else if ( used && !fallenIn( chosen + 1 ) )   // a fallen hero's save only shows
                {
                    g_slot = settings.m_slot = chosen + 1;
                    saveSettings( settings );
                    loadProgress( saved );
                    hasSave = true;
                    progress = saved;
                    hero = saved.m_hero;
                    level = saved.m_level;
                    restart();
                    mode = PLAY;
                }
            }
            if ( k.m_back ) mode = TITLE, chosen = 0;
            lastMouse = GetMousePosition();
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
            std::vector<RowE> rows = menuRows( menuOf(), hasSave, plus, mode == TITLE && anySave() );
            int               count = (int)rows.size();
            chosen = ( chosen % count + k.m_move + count ) % count;
            RowE row = rows[chosen];
            int  step = k.m_change != 0 ? k.m_change : ( k.m_confirm ? 1 : 0 );
            if ( step ) stillValid = false;
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
                    // In the order frugal, low, middle, high.
                    const int order[4] = { 3, 0, 1, 2 };
                    int       i = (int)( std::find( order, order + 4, qualityOf( settings ) ) - order ) % 4;
                    cycle( i, 4 );
                    settings.m_quality = order[i];
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
            case RowE::WINDOW:
                if ( step ) cycle( settings.m_display, 2 ), applyDisplay();
                break;
            case RowE::SIZE:
                if ( step )
                {
                    // Only sizes the screen holds.
                    int monitor = GetCurrentMonitor();
                    for ( int i = 0; i < 5; ++i )
                    {
                        cycle( settings.m_size, 5 );
                        if ( WindowSizes[settings.m_size][0] <= GetMonitorWidth( monitor ) &&
                             WindowSizes[settings.m_size][1] <= GetMonitorHeight( monitor ) )
                            break;
                    }
                    applyDisplay();
                }
                break;
            case RowE::VSYNC:
                if ( step ) cycle( settings.m_vsync, 2 ), applyDisplay();
                break;
            case RowE::FPS:
                if ( step )
                {
                    int i = (int)( std::find( FpsSteps, FpsSteps + 7, fpsOf( settings ) ) - FpsSteps ) % 7;
                    cycle( i, 7 );
                    settings.m_fps = FpsSteps[i];
                    applyDisplay();
                }
                break;
            case RowE::TEXTSIZE:
                if ( step )
                {
                    int i = textOf( settings );
                    cycle( i, 4 );
                    settings.m_text = i;
                    setTextSize( TextSteps[i] );
                }
                break;
            case RowE::HUD:
                if ( step ) cycle( settings.m_hud, 4 );
                break;
            case RowE::RUMBLE:
                if ( step ) cycle( settings.m_rumble, 3 );
                break;
            case RowE::MINIMAP:
                if ( step ) cycle( settings.m_minimap, 2 );
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
                else if ( row == RowE::DAILY )
                    startDaily();
                else if ( row == RowE::PLUS )
                {
                    // A new game+: the hero as they are (level, gear, skills, scrap), the story from the prologue,
                    // every place twelve areas further on. The notes read stay read.
                    progress = carriedFrom( saved, o.m_level );
                    journal::win( "plus" );
                    level = o.m_level;
                    hero = progress.m_hero;
                    saveProgress( progress );
                    saved = progress;
                    restart();
                    mode = PLAY;
                }
                else if ( ( row == RowE::START || row == RowE::LOAD ) && o.m_hero.empty() )
                {
                    // Which slot: for a new game the first free one is offered, to load the one played last.
                    slotNew = row == RowE::START;
                    mode = SLOTS;
                    armed = -1;
                    chosen = g_slot - 1;
                    bool used = false;
                    if ( slotNew )
                        for ( int i = 1; i <= Slots; ++i )
                            if ( slotLine( i, used ), !used )
                            {
                                chosen = i - 1;
                                break;
                            }
                }
                else if ( row == RowE::START || row == RowE::RESUME )
                    mode = PLAY;
                else if ( row == RowE::OPTIONS )
                    optionsFrom = mode, mode = OPTIONS, chosen = 0;
                else if ( row == RowE::CONTROLS )
                    mode = CONTROLS, chosen = 0, waiting = false;
                else if ( row == RowE::DISPLAY )
                    mode = SCREEN, chosen = 0;
                else if ( row == RowE::PHOTO )
                {
                    game->startPhoto();
                    mode = PHOTO;
                    lastMouse = GetMousePosition();
                }
                else if ( row == RowE::BACK && mode == SCREEN )
                    mode = OPTIONS, chosen = 3;
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
            if ( k.m_back && mode == SCREEN )
                mode = OPTIONS, chosen = 3;
            else if ( k.m_back && mode == OPTIONS )
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
        stillAge += dt;
        if ( mode == PLAY || mode == PHOTO )
        {
            stillValid = false;
            game->render( gfx, nullptr );
        }
        else
        {
            int w = GetScreenWidth(), h = GetScreenHeight();
            if ( still.id == 0 || still.texture.width != w || still.texture.height != h )
            {
                if ( still.id ) UnloadRenderTexture( still );
                still = LoadRenderTexture( w, h );
                stillValid = false;
            }
            // Behind the inventory nothing: its pages cover the screen (and show the hero themselves).
            if ( mode == PANEL )
                ClearBackground( BLACK );
            else
            {
                if ( !stillValid ) game->render( gfx, &still ), stillValid = true, stillAge = 0.0f;
                DrawTextureRec( still.texture, { 0.0f, 0.0f, (float)w, -(float)h }, { 0.0f, 0.0f }, WHITE );
            }
        }
        if ( mode == PANEL )
        {
            game->preparePanel( gfx, panelTab, heroTurn, GetScreenWidth(), GetScreenHeight() );
            game->drawPanel( GetScreenWidth(), GetScreenHeight(), panelTab, panelRow );
        }
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
        else if ( mode == CARRY )
        {
            std::vector<std::string> items = { tr( "carry.fresh" ) };
            for ( int slot : finished )
            {
                Progress p;
                loadProgress( p, slotPath( slot ) );
                items.push_back( TextFormat( tr( "carry.hero" ), tr( std::string( "hero." ) + heroKey( p.m_hero ) ),
                                             p.m_heroLevel, slot, p.m_cycle + 1 ) );
            }
            items.push_back( TextFormat( tr( "carry.difficulty" ), difficultyName( settings.m_difficulty ) ) );
            items.push_back( tr( "menu.back" ) );
            drawMenu( game->font(), "DARK CONCORDANCE", tr( "carry.title" ), items, chosen );
        }
        else if ( mode == SLOTS )
        {
            std::vector<std::string> items;
            for ( int i = 1; i <= Slots; ++i )
            {
                bool        used = false;
                std::string line = slotLine( i, used );
                if ( slotNew && used && armed == i - 1 ) line = TextFormat( tr( "slot.overwrite" ), i );
                items.push_back( line );
            }
            if ( slotNew ) items.push_back( tr( oneLife ? "slot.onelife.on" : "slot.onelife.off" ) );
            items.push_back( tr( "menu.back" ) );
            drawMenu( game->font(), "DARK CONCORDANCE", tr( slotNew ? "menu.slots.new" : "menu.slots.load" ), items,
                      chosen );
        }
        else if ( mode == PHOTO )
        {
            // Its help at the bottom (the picture saved is taken without it), what was saved.
            int   w = GetScreenWidth(), h = GetScreenHeight();
            float s = textScale( h );
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
                                         : mode == SCREEN  ? std::string( tr( "menu.display" ) )
                                         : mode == OPTIONS ? std::string( tr( "menu.options" ) )
                                                           : std::string( difficultyName( settings.m_difficulty ) );
            std::vector<RowE> rows = menuRows( menuOf(), hasSave, plus, mode == TITLE && anySave() );
            drawMenu( game->font(), mode == PAUSE ? tr( "menu.pause" ) : "DARK CONCORDANCE", subtitle.c_str(),
                      rowTexts( rows, settings, saved.m_cycle ), chosen % (int)rows.size() );
            if ( mode == TITLE && !notice.empty() && crashed.empty() )
            {
                float   s = textScale( GetScreenHeight() );
                Vector2 m = MeasureTextEx( game->font(), notice.c_str(), 22 * s, 1.0f );
                DrawTextEx( game->font(), notice.c_str(),
                            { GetScreenWidth() * 0.5f - m.x * 0.5f, GetScreenHeight() - 110 * s }, 22 * s, 1.0f,
                            { 160, 255, 190, 230 } );
            }
            if ( mode == TITLE && !crashed.empty() )
            {
                const char *t = TextFormat( tr( "crash.before" ), crashed.c_str() );
                float       s = textScale( GetScreenHeight() );
                Vector2     m = MeasureTextEx( game->font(), t, 20 * s, 1.0f );
                DrawTextEx( game->font(), t, { GetScreenWidth() * 0.5f - m.x * 0.5f, GetScreenHeight() - 110 * s },
                            20 * s, 1.0f, { 255, 170, 120, 220 } );
            }
            if ( mode == TITLE )
            {
                // The version, small in the corner.
                float   s = textScale( GetScreenHeight() );
                Vector2 m = MeasureTextEx( game->font(), SPACEOPERA_VERSION, 18 * s, 1.0f );
                DrawTextEx( game->font(), SPACEOPERA_VERSION,
                            { GetScreenWidth() - m.x - 28 * s, GetScreenHeight() - 44 * s }, 18 * s, 1.0f,
                            { 200, 190, 175, 140 } );
            }
        }
        if ( showFps ) DrawFPS( 10, 10 );
        if ( !g_scriptShot.empty() )
        {
            // The menu test's picture: the whole window, menus and all.
            rlDrawRenderBatchActive();   // what is batched (menus, HUD) into the window first
            rlDisableFramebuffer();
            int   w = GetRenderWidth(), h = GetRenderHeight();
            Image img = { rlReadScreenPixels( w, h ), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
            ExportImage( img, ( g_scriptDir + g_scriptShot + ".png" ).c_str() );
            UnloadImage( img );
            g_scriptShot.clear();
        }
        // A still behind a menu needs no more than 30 frames (the battery of a Steam Deck).
        if ( o.m_shot.empty() )
        {
            int fps = fpsOf( settings );
            SetTargetFPS( mode == PLAY || mode == PHOTO ? fps : ( fps ? std::min( fps, 30 ) : 30 ) );
        }
        EndDrawing();
    }
    if ( still.id ) UnloadRenderTexture( still );
    rumble::stop();
    game->shutdown();
    score.shutdown();
    if ( IsAudioDeviceReady() ) CloseAudioDevice();
    return 0;
}

}   // namespace

int main( int argc, char **argv )
{
    Options o = parse( argc, argv );
    // The log beside the program (the tests read it there), else in the player's folder (a folder one may not write in).
    std::string logPath = std::string( GetApplicationDirectory() ) + "spaceopera.log";
    g_log = fopen( logPath.c_str(), "w" );
    if ( !g_log ) logPath = userDir() + "spaceopera.log", g_log = fopen( logPath.c_str(), "w" );
    crash::install( userDir() + "crashes/", logPath );
    SetTraceLogCallback( logToFile );
    SetTraceLogLevel( LOG_WARNING );

    // On a Steam Deck the window fills its screen (gamescope shows it whole) and the graphics start low.
    if ( o.m_width <= 0 ) o.m_width = steamDeck() ? 1280 : 1600;
    if ( o.m_height <= 0 ) o.m_height = steamDeck() ? 800 : 900;
    // Pictures for the tests are drawn high unless asked otherwise, whatever the player chose.
    int quality = o.m_quality >= 0 ? o.m_quality : ( o.m_shot.empty() ? qualityOf( loadSettings() ) : 2 );

    unsigned flags = FLAG_VSYNC_HINT;
    if ( !o.m_shot.empty() ) flags |= FLAG_WINDOW_HIDDEN;
    // The menu test: its script, in a real window that does not take the focus.
    if ( !o.m_menuTest.empty() )
    {
        flags |= FLAG_WINDOW_UNFOCUSED;
        std::stringstream steps( o.m_menuTest );
        for ( std::string step; std::getline( steps, step, ',' ); )
            if ( !step.empty() ) g_script.push_back( step );
        g_scriptDir = o.m_menuDir.empty() ? std::string( GetApplicationDirectory() ) : o.m_menuDir + "/";
    }
    SetConfigFlags( flags );
    InitWindow( o.m_width, o.m_height, "DARK CONCORDANCE" );
    // The window's icon (tools/make_icon.py), on every platform.
    Image icon = LoadImageFromMemory( ".png", IconPng, (int)sizeof( IconPng ) );
    if ( icon.data ) SetWindowIcon( icon );
    UnloadImage( icon );
    if ( !o.m_pad.empty() )
        setDevice( o.m_pad == "xbox"   ? DeviceE::XBOX
                   : o.m_pad == "ps"   ? DeviceE::PLAYSTATION
                   : o.m_pad == "deck" ? DeviceE::DECK
                                       : DeviceE::KEYBOARD );
    SetExitKey( KEY_NULL );   // Esc pauses; the menu quits

    Gfx gfx;
    gfx.init( o.m_width, o.m_height, quality, o.m_ss );
    if ( o.m_exposure > 0.0f ) gfx.m_exposure = o.m_exposure;
    gfx.m_lean = !o.m_drawMesh;
    // A test's picture is drawn at its quality's full size, whatever the GPU's time.
    gfx.m_dynamicRes = o.m_shot.empty();
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
