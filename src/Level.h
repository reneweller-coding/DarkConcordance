/**
 * @file Level.h
 * @brief A level from an ASCII map (2 m cells): walls, floors at heights, shafts, grating bridges, heat exchangers as
 *        cover, and markers for spawns, lights, pickups and triggers; plus its spoken lines and hints. Answers the
 *        game's questions (floor height, walls, rays) and builds the geometry with its dressing.
 * @author Rene Weller
 */
#pragma once

#include "Gfx.h"

#include <raylib.h>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace spaceopera
{

class MeshBuilder;

constexpr float CellSize = 2.0f;    ///< metres per map cell
constexpr float FinHeight = 2.5f;   ///< height of a heat exchanger block
constexpr float PitDepth = 30.0f;   ///< how deep the shafts go

/** @brief What a cell is. */
enum class CellE : uint8_t
{
    ROCK,      ///< solid
    FLOOR,     ///< walkable at its height
    VOID,      ///< a shaft: no floor
    GRATE,     ///< a grating bridge over a shaft
    FIN,       ///< a heat exchanger block: cover, its top walkable
    GLASS,     ///< a window in the floor (walkable; open space under it)
    COUNTER,   ///< a bar counter: waist-high, in the way, shots pass over it
    DOOR       ///< a door: solid while shut (A / E opens it, unless it is locked)
};

constexpr float DoorHeight = 2.6f;   ///< the opening of a door

constexpr float CounterHeight = 1.1f;   ///< height of a counter cell

/** @brief Someone to talk to (a hub's people). */
struct Npc
{
    std::string                                      m_id;        ///< name (texts: speaker.<id>, portrait_<id>)
    int                                              m_row = 0;   ///< map row
    int                                              m_col = 0;   ///< map column
    std::string                                      m_look;      ///< how they look (ysolde, ottla, kiri, terminal ...)
    std::string                                      m_start;     ///< the conversation's first node
    std::vector<std::pair<std::string, std::string>> m_after;     ///< (flag, node): once the flag is set, start there
};

/** @brief A step of a conversation: what is said, what can be answered. */
struct TalkNode
{
    std::string m_id;        ///< key of its text
    std::string m_speaker;   ///< who says it
    std::string m_event;     ///< what it sets off (flags, shop, workshop, scrapN)
    std::vector<std::pair<std::string, std::string>>
        m_answers;   ///< (key of the answer, the node it leads to; "-" ends)
};

/** @brief One map cell. */
struct Cell
{
    CellE m_kind = CellE::ROCK;   ///< what it is
    float m_height = 0.0f;        ///< floor height (m)
    char  m_trigger = 0;          ///< trigger letter (a-z) or 0
    bool  m_open = false;         ///< a door: open
};

/** @brief Something placed in the map (an upper-case letter). */
struct Marker
{
    char    m_kind;   ///< the letter
    int     m_row;    ///< map row
    int     m_col;    ///< map column
    Vector3 m_pos;    ///< centre of the cell on its floor
};

/** @brief Something placed by name in the level's head ("thing r c name"): the later places' things. */
struct Thing
{
    std::string m_name;       ///< what it is
    int         m_row = 0;    ///< map row
    int         m_col = 0;    ///< map column
    Vector3     m_pos = {};   ///< centre of the cell on its floor
};

/** @brief A spoken line. */
struct Line
{
    std::string m_id;        ///< key of its text and name of its voice (assets/voice/<language>/<id>.ogg)
    std::string m_trigger;   ///< trigger letter or event name
    std::string m_speaker;   ///< who speaks (key speaker.<name>)
};

/** @brief A hint on screen. */
struct Hint
{
    std::string m_id;        ///< key of its text
    std::string m_trigger;   ///< trigger letter or event name
};

/** @brief A fixed light of the level. */
struct LevelLight
{
    Vector3 m_pos;       ///< where
    Vector3 m_color;     ///< colour times intensity
    float   m_radius;    ///< reach
    float   m_flicker;   ///< 0 steady .. 1 failing
};

/** @brief A kind of decal (assets/textures/decals/decals.txt): where it goes, how large, which themes lay it. */
struct DecalDef
{
    std::string m_name;               ///< its picture (assets/textures/decals/<name>.png)
    bool        m_wall = false;       ///< on a wall (else on the floor)
    float       m_width = 1.0f;       ///< metres
    float       m_height = 1.0f;      ///< metres
    float       m_y = 1.6f;           ///< on a wall: the height of its middle
    float       m_rough = 0.8f;       ///< roughness (puddles and oil shine)
    float       m_glow = 0.0f;        ///< how bright it glows by itself (neon, screens, glyphs)
    bool        m_multiply = false;   ///< laid by multiplying (a stain: what is under it shows through)
    Vector3     m_color;              ///< its mean colour (the colour of its light)
    uint64_t    m_themes = 0;         ///< one bit per ThemeE that lays it
};

/** @brief A decal laid in the level: a quad a little in front of a floor or a wall. */
struct Decal
{
    int     m_def;         ///< which kind (index into the defs)
    Vector3 m_corner[4];   ///< top left, top right, bottom right, bottom left
    bool    m_cut;         ///< on a wall: dissolves with it
};

/** @brief Surfaces the level geometry uses. */
struct LevelSurfaces
{
    int m_rock;                  ///< rock walls
    int m_floor;                 ///< deck plates
    int m_grate;                 ///< grating
    int m_metal;                 ///< steel
    int m_hazard;                ///< hazard stripes
    int m_rust;                  ///< rusty steel
    int m_foil;                  ///< thermal foil (the bodies)
    int m_glow;                  ///< lamps
    int m_crate;                 ///< painted crates
    int m_box;                   ///< corrugated container steel
    int m_hull;                  ///< the ship's bulkheads
    int m_cloth;                 ///< fabric (bundles, bags)
    int m_themeWall[64] = {};    ///< per theme (ThemeE): its walls' own surface + 1 (0: the common ones)
    int m_themeFloor[64] = {};   ///< ... its floors' own surface + 1
    int m_foliage = -1;          ///< the gardens' leaves, tinted by each plant (-1: the park keeps its plain walls)
    int m_bark = -1;             ///< their trunks
    int m_gravel = -1;           ///< their paths
    std::map<int, std::vector<int>> m_variants;           ///< a wall's or floor's variants, laid as a patchwork
    const std::vector<DecalDef>    *m_decals = nullptr;   ///< the decals there are (nullptr: none)
};

/** @brief The character of an area: colours, lining, pipes, props. */
enum class ThemeE : uint8_t
{
    SERVICE,     ///< maintenance access: clean panels, crates
    EXCHANGER,   ///< heat exchangers: cold blue, many pipes, puddles
    SHAFT,       ///< the cooling shaft: raw rock, rust, cable drums
    COLDSTORE,   ///< the cold store: pale frosted panels, frost
    WORKS,       ///< the drone works: hazard paint, scrap, barrels
    CARGO,       ///< a ship's hold: walls of stacked containers, tie-down rails, bundles
    GANGWAY,     ///< a ship's gangway: bulkheads, cable trays, lockers
    BRIDGE,      ///< the bridge deck: clean bulkheads, consoles
    BAR,         ///< a bar: warm panels, round tables with stools
    STREET,      ///< a habitat's alley: house fronts with windows and doors, roofs, lights strung across, clutter
    HULL,        ///< out on a station's skin: plates, radiators, masts, hazard edges over open space
    VAULT,       ///< a bank's vaults: clean pale bulkheads, polished floors (the racks of cores are the level's)
    ICE,         ///< an ice moon: ridges of blue ice, snow, pump housings (the open floors are clear ice)
    DECK,        ///< a refinery deck in the clouds: rust-red plating, tanks, valves
    DUSK,        ///< the twilight belt: warm rock, dust, boulders
    PODS,        ///< an ark's sleeper halls: white bulkheads, rows of pods with blue windows
    STATION,     ///< the star lifter's station: dark plates, coolant tanks with glowing bands
    SNOW,        ///< a dark world of nitrogen snow: black rock, white ground, floodlight masts
    HOLD,        ///< a transport's hold: container walls, crates of cores glowing blue
    ALIEN,       ///< the object of Tesh: black smooth walls with faint lines of light, terraces, crystal nodes
    PARK,        ///< the ring's gardens: grass, hedges, trees, benches
    CLINIC,      ///< the clinic of new bodies: white walls, beds, plants
    ARENA,       ///< the dying gardens' arena: sand, sandstone tiers, banners
    YARD,        ///< the gate's yard: yellow plating over open space, girder stacks, welding rigs
    HALL,        ///< the hearing's hall in an asteroid: polished stone, rows of benches
    CRYO,        ///< the computing moon: frosted blue bulkheads, blocks of computers
    PRISON,      ///< vault zero: scorched plating, cells with bars
    MOON,        ///< Brannt being peeled: grey regolith, boulders
    CONTAINER,   ///< the ore stream: container roofs over open space
    FURNACE,     ///< inside the Mill: dark rock, plating, crucibles glowing
    SERVER,      ///< the control room: dark bulkheads, racks of lights
    NET,         ///< the night shift's world: pale walls, a canteen, a classroom, a kitchen
    COUNT        ///< number of themes
};

/**
 * @brief A theme by its name in a level file.
 * @param[in] name  e.g. street, ice, park
 * @return the theme, or -1 cast to ThemeE when there is none of that name
 */
ThemeE themeNamed( const std::string &name );

/** @brief A rectangle of the map with a theme. */
struct Zone
{
    int    m_r0, m_c0, m_r1, m_c1;   ///< rows and columns (inclusive)
    ThemeE m_theme;                  ///< its character
};

/** @brief A sign on a wall or on posts. */
struct Sign
{
    std::string m_texture;     ///< picture (assets/textures/<name>.<language>.png) and key of its text
    Vector3     m_corner[4];   ///< top left, top right, bottom right, bottom left
    Vector3     m_lamp;        ///< the small lamp above it
};

/** @brief Things that break when hit: the level places them (by the walls of its wider rooms), the game breaks them. */
enum class BreakE : int
{
    CRATE,       ///< a supply crate
    DRUM,        ///< a fuel drum
    CANISTERS,   ///< gas canisters, strapped together
    CASE,        ///< equipment cases, stacked
    TANK,        ///< a glass tank of glowing coolant
    SPOOL,       ///< a cable spool
    CRYSTAL,     ///< a cluster of crystals (the object of Tesh)
    LOCKER,      ///< a standing locker
    CONSOLE,     ///< a screen on a stand
    BATTERY,     ///< a bank of power cells, their bands lit
    SACKS,       ///< sacks piled up
    POT,         ///< a plant in a pot
    VIALS,       ///< a rack of glowing sample vials
    COUNT
};

/** @brief Where a breakable stands. */
struct BreakSpot
{
    Vector3 m_pos;    ///< on the floor
    float   m_yaw;    ///< which way it faces
    BreakE  m_kind;   ///< what it is
    int     m_prop;   ///< its circle in the level's props (the game moves it away when it breaks)
};

/** @brief A solid prop on the floor (crates, barrels, scrap): a circle that blocks. */
struct Prop
{
    Vector3 m_pos;      ///< on the floor
    float   m_radius;   ///< what it blocks
};

/** @brief A seat: a stool at a table or at the counter. */
struct Seat
{
    Vector3 m_pos;      ///< on the floor under it
    float   m_yaw;      ///< which way one sits (towards the table)
    float   m_height;   ///< the seat's top
};

/**
 * @brief A cheap hash of a cell and a salt.
 * @param[in] r     row
 * @param[in] c     column
 * @param[in] salt  what it is for
 * @return the hash
 */
unsigned hashCell( int r, int c, int salt );

/** @brief An anomaly of a place built anew each time (the Depths): lines for its recipe, what it does to the fight. */
struct AnomalyDef
{
    const char *m_key;             ///< its name's text key ("anomaly.<key>")
    const char *m_lines;           ///< recipe lines it adds before the place is built ("" none)
    float       m_hp = 1.0f;       ///< the enemies' health
    float       m_tempo = 1.0f;    ///< their pace
    float       m_harm = 1.0f;     ///< what they do to the hero
    float       m_stims = 1.0f;    ///< what a stim heals
    float       m_scrap = 1.0f;    ///< the scrap found
    bool        m_burst = false;   ///< the dead burst
    float       m_reward = 0.3f;   ///< better finds (+30 %) and more experience (two thirds of that)
};

/** @return the anomalies there are */
const std::vector<AnomalyDef> &anomalyDefs();

/**
 * @brief The hash as a number in 0..1.
 * @param[in] r     row
 * @param[in] c     column
 * @param[in] salt  what it is for
 * @return the number
 */
float hash01( int r, int c, int salt );

/** @brief A level. */
class Level
{
public:
    /**
     * @brief Reads a level file; a recipe (see LevelGen.h) is generated first.
     * @param[in] path  the file
     * @param[in] seed  a generated level's seed
     * @return false if it cannot be read
     */
    bool load( const std::string &path, unsigned seed = 0 );

    /**
     * @brief Reads a level from its text.
     * @param[in] text  the level file's contents
     * @return false if it has no map
     */
    bool loadText( const std::string &text );

    /**
     * @brief Builds the geometry: walls, floors, dressing, lamps; adds the level's lights.
     * @param[in]  s    the surfaces
     * @param[out] out  the drawables
     */
    void build( const LevelSurfaces &s, std::vector<Drawable> &out );

    /** @return rows of the map */
    int rows() const { return m_rows; }
    /** @return columns of the map */
    int cols() const { return m_cols; }

    /**
     * @brief A cell (outside the map: rock).
     * @param[in] r  row
     * @param[in] c  column
     * @return the cell
     */
    const Cell &cell( int r, int c ) const;

    /**
     * @brief The cell a point lies in.
     * @param[in]  p  the point
     * @param[out] r  row
     * @param[out] c  column
     */
    void cellOf( Vector3 p, int &r, int &c ) const;

    /**
     * @brief The centre of a cell on its floor.
     * @param[in] r  row
     * @param[in] c  column
     * @return the point
     */
    Vector3 centre( int r, int c ) const;

    /**
     * @brief The floor under a point.
     * @param[in] x  east (m)
     * @param[in] z  south (m)
     * @return its height; -1000 over a shaft, 1000 in rock
     */
    float floorAt( float x, float z ) const;

    /**
     * @brief Pushes a circle out of what blocks it at its height.
     * @param[in,out] pos     the feet
     * @param[in]     radius  the circle
     * @param[in]     step    the highest step it walks up without jumping
     * @return true if it touched something
     */
    bool collide( Vector3 &pos, float radius, float step ) const;

    /**
     * @brief Follows a ray to the first wall, block or floor.
     * @param[in]  from    start
     * @param[in]  to      end
     * @param[out] hit     where it stops
     * @param[out] normal  the surface's normal there
     * @return true if something was hit before `to`
     */
    bool raycast( Vector3 from, Vector3 to, Vector3 &hit, Vector3 &normal ) const;

    /**
     * @brief Whether a cell can be walked on by something that climbs up to `climb`.
     * @param[in] r      row
     * @param[in] c      column
     * @return true for floors, gratings and heat exchanger tops
     */
    bool walkable( int r, int c ) const;

    /**
     * @brief Turns a cell into a shaft (before build(); the exit lift's well).
     * @param[in] r  row
     * @param[in] c  column
     */
    void setVoid( int r, int c );

    /**
     * @brief Turns a cell back into a pane (a moon's crust growing over again).
     * @param[in] r  row
     * @param[in] c  column
     */
    void setGlass( int r, int c );

    /**
     * @brief Opens a door (it no longer blocks).
     * @param[in] r  row
     * @param[in] c  column
     */
    void openDoor( int r, int c );

    /**
     * @brief The theme of a cell.
     * @param[in] r  row
     * @param[in] c  column
     * @return the zone's theme (SERVICE outside every zone)
     */
    ThemeE theme( int r, int c ) const;

    /**
     * @brief Whether a cell is out in the open (no air in a vacuum).
     * @param[in] r  row
     * @param[in] c  column
     * @return true if its zone's theme is an outdoor one ("outdoor" lines)
     */
    bool outdoor( int r, int c ) const;

    std::string             m_name;              ///< key of the name shown at the start
    std::string             m_act;               ///< key of the act's name
    std::vector<int>        m_anomalies;         ///< a generated place's anomalies this time (anomalyDefs())
    int                     m_anomalyMin = 0;    ///< at least so many (the Depths' tier; set before load)
    float                   m_gravity = 9.81f;   ///< m/s²
    Vector4                 m_fog = { 0.01f, 0.05f, 0.5f, 1.0f };                   ///< fog as in Lighting
    Vector3                 m_fogColor = { 0.002f, 0.002f, 0.003f };                ///< the fog's own glow
    Vector3                 m_ambUp = { 0.01f, 0.01f, 0.01f };                      ///< ambient from above
    Vector3                 m_ambDown = { 0.01f, 0.01f, 0.01f };                    ///< ambient from below
    float                   m_wallHeight = 4.5f;                                    ///< wall height
    std::vector<Marker>     m_markers;                                              ///< spawns, lights, pickups
    std::vector<Line>       m_lines;                                                ///< spoken lines
    std::vector<Hint>       m_hints;                                                ///< hints
    std::vector<LevelLight> m_lights;                                               ///< fixed lights (after build)
    std::vector<Zone>       m_zones;                                                ///< areas with their themes
    std::vector<Sign>       m_signs;                                                ///< signs (after build)
    std::vector<Prop>       m_props;                                                ///< solid props (after build)
    std::vector<BreakSpot>  m_breaks;                                               ///< things that break (after build)
    std::vector<Seat>       m_seats;                                                ///< stools (after build)
    std::vector<Decal>      m_decals;                                               ///< decals (after build)
    std::vector<std::pair<std::string, std::pair<int, int>>> m_signRequests;        ///< "sign" lines: texture, cell
    std::string                                              m_sky;                 ///< the sky's picture ("" none)
    bool                                                     m_thrust = false;      ///< the ship dodges: gravity tilts
    std::string                                              m_next;                ///< the level after this one
    std::string                                              m_gate = "bossdead";   ///< the event that opens the door
    std::string                                              m_ambience = "tunnels";   ///< the background sound
    std::string                      m_music;              ///< the place's piece (assets/music/music.txt); "" none
    std::string                      m_bossMusic;          ///< the piece of its boss fight ("" the common one, boss)
    std::vector<std::pair<int, int>> m_route;              ///< the autopilot's waypoints (cells), in order
    std::vector<std::pair<int, int>> m_glass;              ///< the window cells (after build: the game lays the panes)
    bool                             m_peaceful = false;   ///< a hub: no fighting, the trigger and A talk
    std::string                      m_view;               ///< what the floor windows show: star, drums
    std::string                      m_back;               ///< with this flag the level starts with "back", not "start"
    std::string                      m_autotalk;           ///< whom the autopilot talks to
    std::string                      m_intro;              ///< the opening card's text (paragraphs split by '|')
    int                              m_area = 1;           ///< how strong its enemies and its items are
    float                            m_coriolis = 0.0f;    ///< a drum's spin (rad/s): what flies is bent by it
    bool                             m_space = false;      ///< open space under the gaps (thin plates, the stars)
    bool                             m_vacuum = false;     ///< no air outdoors: oxygen runs out, sound is muffled
    float                            m_flares = 0.0f;      ///< seconds between the star's radiation storms (0 none)
    Vector4                          m_starAt = { 0, 0, 0, 0 };   ///< the star: where (from the middle), how big
    Vector3                          m_sunDir = { 0, 0, 0 };      ///< the light's way (0: from the star)
    Vector3                          m_sunColor = { 0, 0, 0 };    ///< its colour (0: the red dwarf's)
    int                              m_tugRow = -1, m_tugCol = 0, m_tugSide = 1;   ///< the tug's berth (an edge cell)
    std::vector<Vector4>             m_water;     ///< balls of water floating in low gravity: cell row, column, radius
    std::vector<std::pair<int, int>> m_relays;    ///< Mother Sevenfold's relays (cells)
    std::vector<std::pair<int, int>> m_mothers;   ///< her bodies (cells)
    int                              m_birds = 0;                      ///< birds that fly over it and never land
    int                              m_ilkaRow = -1, m_ilkaCol = -1;   ///< where Ilka stands at the start
    std::vector<std::pair<int, int>> m_ilkaWay;   ///< where she runs after the alarm (then she is gone)
    std::map<int, std::string>       m_locks;     ///< door cell (row * 1000 + col) -> the event that unlocks it
    std::map<int, std::string>       m_notes;     ///< terminal cell (row * 1000 + col) -> the key of its text
    std::map<int, int> m_elites;   ///< enemy cell (row * 1000 + col) -> 1 a champion, 2 the guardian, 3 a leader
    /** @brief One of the places' own creatures where the generator put it. */
    struct Native
    {
        int m_row, m_col, m_which;   ///< its cell; which of the level's natives (1 ..)
    };
    std::vector<Native>      m_natives;       ///< creatures placed ("native r c n")
    std::vector<std::string> m_nativeNames;   ///< the families the digits stand for ("natives crystal shade")
    std::string              m_generated;     ///< a generated level's file (empty for a written one)
    std::vector<std::pair<std::string, std::string>>
                                       m_exits;           ///< (level, flag): where the lift goes once the flag is set
    std::set<std::string>              m_repeat;          ///< exits that stay open once done (the depths)
    std::set<int>                      m_outdoorThemes;   ///< themes out in the open (ThemeE as int)
    int                                m_boardRow = -1, m_boardCol = 0, m_boardWidth = 0;   ///< the harvest board
    Vector3                            m_boardCorner[4] = {};   ///< its picture's corners (after build)
    std::vector<Npc>                   m_npcs;                  ///< people to talk to
    std::map<std::string, std::string> m_npcAfter;   ///< in a fight's level: who shows only once something happened
    // Act II's places.
    std::string m_meter;                 ///< the suit's other need: "warmth" (cold), "dose" (radiation), "climate"
    float       m_meterSecs = 120.0f;    ///< how long it lasts where it drains
    bool        m_meterIndoor = false;   ///< it drains indoors too (only heaters help)
    int         m_bandHot = -1;          ///< "climate": west of this column the day side burns
    int         m_bandCold = 1 << 20;    ///< ... east of this one the night side freezes
    float       m_bandHotF = 0.0f, m_bandColdF = 0.0f;   ///< "climate a b": the band as shares of the width
    std::string m_talkMusic;                             ///< the piece while someone talks in a fight's level (Hale)
    Vector3     m_skyColor = { -1, -1, -1 };             ///< a sky of one colour instead of a picture (x < 0: none)
    Vector3     m_windDir = { 0, 0, 0 };                 ///< where the wind blows to
    float       m_windStrength = 0.0f;                   ///< how hard it bends what flies (m/s²)
    float       m_gusts = 0.0f;                          ///< seconds between gusts that push (0 none)
    bool        m_ice = false;                ///< the glass is ice over an ocean: it breaks; falling in is cold
    std::string m_below;                      ///< what open space shows below: "" the stars, "ocean", "clouds"
    bool        m_dark = false;               ///< darkness: enemies see near only, the hero carries a lamp
    std::string m_planetTex = "reddwarf";     ///< the body in the sky ("star")
    float       m_planetGlow = 2.2f;          ///< its own light
    std::string m_faction;                    ///< whose men: "" the Skimmers, "concession", "clearance", "security"
    float       m_pressure = 0.0f;            ///< seconds between the guard's reinforcements (0 none)
    std::string m_machine = "tug";            ///< the machine at the berth: tug, rig, bell, lance
    std::string m_menuTitle = "lift.title";   ///< the exit's menu: a lift, a ship's navigation
    int         m_consoleCost = 0;            ///< sleepers a power console costs when no cell is fitted
    std::string m_whiteout;          ///< after the boss: this event, then the screen goes white and the act is over
    int         m_bossVariant = 0;   ///< the exoskeleton's man: 0 the Taxator, 2 Okafor, 3 the boarding leader
    bool        m_plasma = false;    ///< a river of star matter arcs through the sky
    // Acts III and IV.
    std::string        m_endCard = "act.2end";   ///< the white-out's words ("endcard key music flag")
    std::string        m_endMusic = "act2end";   ///< ... its piece
    std::string        m_endFlag = "act2done";   ///< ... what is known afterwards (a flag)
    std::string        m_epilogue;               ///< after this event the endings' and the epilogue's cards
    std::vector<Thing> m_things;                 ///< things by name ("thing r c name")
    std::vector<std::pair<std::string, std::string>> m_wallTex, m_floorTex;   ///< a theme's own surfaces here
    std::map<std::string, std::string> m_keys;   ///< the later places' settings by key (see GameAct34.cpp)
    /**
     * @param[in] key   a setting of the later places
     * @param[in] def   its value when the level does not set it
     * @return its first number
     */
    float num( const std::string &key, float def = 0.0f ) const;
    /**
     * @param[in] key   a setting of the later places
     * @param[in] i     which of its numbers
     * @param[in] def   its value when missing
     * @return that number
     */
    float num( const std::string &key, int i, float def ) const;
    /**
     * @param[in] key  a setting of the later places
     * @return its words ("" when not set)
     */
    std::string word( const std::string &key ) const;
    std::vector<TalkNode> m_talk;   ///< the conversations' steps

    /**
     * @brief A conversation's step by its key.
     * @param[in] id  the key
     * @return the step, or nullptr
     */
    const TalkNode *talk( const std::string &id ) const;

private:
    /** @brief A glowing box (lamps, panes, bulbs); boxes of one colour become one mesh. */
    struct GlowBox
    {
        Vector3 m_at;
        Vector3 m_half;
        Vector4 m_glow;
        bool    m_cut = false;   ///< in a wall: dissolves with it
    };
    /** @brief A wall face free for a decal (no rib, shelf or door in front of it). */
    struct FaceSlot
    {
        int   m_r, m_c, m_d;   ///< the open cell and the direction of the wall
        float m_front;         ///< how far in front of the wall a decal goes (over the lining)
        float m_base;          ///< the floor's height
    };
    /** @brief The builders the dressing adds to (Level::build's). */
    struct DressKit
    {
        MeshBuilder          &m_metal, &m_rust, &m_crate, &m_box, &m_cloth, &m_hazard, &m_grate, &m_rock, &m_hull;
        MeshBuilder          &m_drape;   ///< cloth that dissolves with the walls (what hangs on them)
        std::vector<GlowBox> &m_glows;
    };

    /** @brief The builders a garden adds to (LevelGarden.cpp). */
    struct GardenKit
    {
        MeshBuilder &m_foliage, &m_bark, &m_gravel, &m_stone;
        MeshBuilder &m_canopy;   ///< the forest's crowns deep in it: leaves that throw no shadow
        /** the park's hedge (wall) or lawn (floor) builder for a cell, as the patchwork lays its variants */
        std::function<MeshBuilder &( int r, int c, bool wall )> m_park;
        std::vector<GlowBox>                                  &m_glows;
    };
    /** @brief What grows in a garden (LevelGarden.cpp, plant). */
    enum class PlantE
    {
        PARASOL,    ///< a slender trunk under a wide flat crown, seed pods glowing beneath it
        LANTERN,    ///< a dome of silver leaves, strands hanging from it with lights at their ends
        SPIRAL,     ///< a topiary: a coil of leaves wound up round a stalk
        ORBS,       ///< a topiary: balls of leaves stacked on a trunk
        TUBES,      ///< a cluster of open tubes, coral-coloured, some lit inside
        FRONDS,     ///< long curved blades from the ground
        BUSH,       ///< a few clumps of leaves
        BED,        ///< a low patch of coloured blooms
        CANOPY,     ///< a forest tree: a big dark crown on a trunk
        COUNT
    };

    /**
     * @brief Plans the gardens (LevelGarden.cpp): which wall cells of a park become hedges (by the paths) and what lies
     *        behind them (garden, then forest), from each wall cell's distance to the open cells.
     * @param[in] on  the level has gardens (its park keeps the hedges' surface)
     */
    void planGarden( bool on );
    /** @return the cell's garden depth: 0 none, 1 a hedge, 2 and more what lies behind it */
    int garden( int r, int c ) const;
    /**
     * @brief A garden wall cell (LevelGarden.cpp): a hedge's lumpy top and sides (its body is the caller's), a strip of
     *        garden behind it, or forest.
     * @param[in]     r    the row
     * @param[in]     c    the column
     * @param[in,out] kit  the builders
     */
    void gardenCell( int r, int c, GardenKit &kit );
    /**
     * @brief Now and then low plants at a hedge's foot, on the floor in front of it (LevelGarden.cpp).
     * @param[in]     r    the floor cell's row
     * @param[in]     c    its column
     * @param[in]     d    the direction of the hedge from it (0 north, 1 east, 2 south, 3 west)
     * @param[in,out] kit  the builders
     */
    void hedgeFoot( int r, int c, int d, GardenKit &kit );
    /**
     * @brief The forest beyond the level's edge where a garden reaches it, and the gravel paths along the route
     *        (LevelGarden.cpp).
     * @param[in,out] kit  the builders
     */
    void gardenEdges( GardenKit &kit );
    /**
     * @brief A plant (LevelGarden.cpp).
     * @param[in]     kind  what grows
     * @param[in]     at    its foot
     * @param[in]     size  its scale
     * @param[in]     h     a hash: its shape and colours
     * @param[in,out] kit   the builders
     * @param[in]     full  all of it (false: a forest tree deep in the forest, only its crown)
     */
    void plant( PlantE kind, Vector3 at, float size, unsigned h, GardenKit &kit, bool full = true );
    /**
     * @brief The lawn's top on a garden cell or beyond the edge (LevelGarden.cpp).
     * @param[in]     r    the row
     * @param[in]     c    the column
     * @param[in,out] kit  the builders
     */
    void lawn( int r, int c, GardenKit &kit );

    /**
     * @brief Dresses the level after its walls and floors (LevelDress.cpp): decals on floors and walls, clutter by the
     *        walls, a show piece in the larger rooms, their lights.
     * @param[in]     s    the surfaces (the decals there are)
     * @param[in,out] kit  the builders
     */
    void dress( const LevelSurfaces &s, DressKit &kit );
    /**
     * @brief Fittings on a wall face, as the place's kind has them (LevelDress.cpp): girders, kick plates, junction
     *        boxes, valves, vents, ladders, cabinets, terminals, rails, insulation, rock bolts, lines of light. All of it
     *        lies flat on the wall: nothing stands in the way. A face that carries a decal keeps its middle free.
     * @param[in]     r      the cell's row (in front of the wall)
     * @param[in]     c      its column
     * @param[in]     d      the direction of the wall from it (0 north, 1 east, 2 south, 3 west)
     * @param[in]     face   the middle of the face on the floor
     * @param[in]     n      the face's normal, into the cell
     * @param[in]     base   the floor's height
     * @param[in]     walls  what the walls are made of (0 rock, 1 containers, 2 bulkheads, 3 house fronts)
     * @param[in]     decal  the face carries a decal
     * @param[in,out] kit    the builders
     */
    void wallKit( int r, int c, int d, Vector3 face, Vector3 n, float base, int walls, bool decal, DressKit &kit );
    /**
     * @brief What lies on the top of a wall by an open cell (LevelDress.cpp): a coping along the edge, runs of pipes,
     *        cable trays or ducts that line up along the wall, lines of light, now and then a machine; it dissolves with
     *        the wall.
     * @param[in]     r      the wall cell's row
     * @param[in]     c      its column
     * @param[in]     walls  what the walls are made of (0 rock, 1 containers, 2 bulkheads, 3 house fronts)
     * @param[in]     mid    the middle of the cell on the floor
     * @param[in,out] kit    the builders
     */
    void crownKit( int r, int c, int walls, Vector3 mid, DressKit &kit );
    /**
     * @brief Something standing by a wall (LevelDress.cpp).
     * @param[in]     group  the place's kind (DressE)
     * @param[in]     h      a hash: which one, how turned
     * @param[in]     at     where (on the floor)
     * @param[in]     yaw    which way it faces (away from the wall)
     * @param[in,out] kit    the builders
     */
    void clutter( int group, unsigned h, Vector3 at, float yaw, DressKit &kit );
    /**
     * @brief A show piece in the middle of a room (LevelDress.cpp).
     * @param[in]     group  the place's kind (DressE)
     * @param[in]     h      a hash: which one
     * @param[in]     at     where (on the floor)
     * @param[in,out] kit    the builders
     */
    void showPiece( int group, unsigned h, Vector3 at, DressKit &kit );

    /**
     * @brief Plans the rooms' shapes before they are built: which outer corners of the walls are cut off, which inner
     *        corners filled, which wall cells stand back as a niche, where galleries run along the walls of the halls.
     */
    void planShapes();
    /**
     * @brief The kind of place a theme is (LevelDress.cpp's groups: 0 industrial, 1 street, 2 clean, 3 cold, 4 outdoor,
     *        5 alien, 6 garden).
     * @param[in] t  the theme
     * @return its group
     */
    static int dressGroup( ThemeE t );

    int                        m_rows = 0;
    int                        m_cols = 0;
    std::vector<Cell>          m_cells;
    std::vector<FaceSlot>      m_faces;     ///< wall faces free for decals (during build)
    std::vector<unsigned char> m_cut;       ///< per wall cell: its outer corners cut off (bit k: NW, NE, SE, SW)
    std::vector<unsigned char> m_fill;      ///< per floor cell: its inner corners filled (bits as m_cut)
    std::vector<signed char>   m_niche;     ///< per wall cell: the side its niche opens to (-1: none)
    std::vector<unsigned char> m_gallery;   ///< per floor cell: the walls a gallery runs along (bit d)
    std::vector<unsigned char> m_garden;    ///< per wall cell: its garden depth (0 none, 1 a hedge, 2.. behind)
    GardenKit                 *m_gardenKit = nullptr;   ///< during build: the garden's builders (null: no gardens)

    /** @brief A cell raised by a "raise" line. */
    struct Raise
    {
        int   m_row, m_col;
        float m_height;
    };
    std::vector<Raise> m_raise;   ///< the level file's raised markers
};

}   // namespace spaceopera
