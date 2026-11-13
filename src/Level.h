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
constexpr float PondLevel = -0.07f;   ///< a garden pond's water, a little below the lawn

constexpr float ShallowLevel = 0.03f;   ///< the gardens' shallow water over the lawn (only seen: one walks on the lawn)

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
    int m_gravel = -1;           ///< their coral tubes (the sand's grain, tinted)
    int m_cards = -1;            ///< cut-out clusters of leaves round the crowns
    int m_blades = -1;           ///< cut-out tufts of grass on the lawn
    int m_boulder = -1;          ///< the pale stone of their rocky edges
    int m_needles = -1;          ///< cut-out fir branches (none: no firs)
    std::map<int, std::vector<int>> m_variants;           ///< a wall's or floor's variants, laid as a patchwork
    std::map<int, int>
        m_splat;   ///< a wall's or floor's surface with two variants blended in (ground and rock outside)
    /// The floors per kind of room (Level::planRooms): per theme a hall's, a corridor's and a side room's own floor + 1
    /// (0: the theme's own, or the common one).
    int                m_kindFloor[64][3] = {};
    int                m_border[64] = {};   ///< per theme the band along its halls' walls + 1 (0: none)
    int                m_sill[64] = {};     ///< per theme the plates in its doorways and where floors change + 1
    bool               m_bays[64] = {};     ///< per theme: its halls get painted bay lines (Level::bayLines)
    std::map<int, int> m_patch;    ///< a wall's or floor's surface -> the same with its two variants in one draw
    std::map<int, int> m_shaped;   ///< a ground's surface -> the same with its trodden ways and scree (out in the open)
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
    /** @return whether the cell is raw rock (outside, in a shaft): its corners all cut or filled, half a cell deep */
    bool natural( int r, int c ) const;
    /**
     * @brief Whether something seen stands between two points (for the camera): walls, rocks and bushes from the floor
     *        up, crowns and lintels between their heights, as the build left them per cell.
     * @param[in] from  start (the hero)
     * @param[in] to    end (the camera)
     * @return true if the sight line passes through any of it
     */
    bool sightBlocked( Vector3 from, Vector3 to ) const;

    /**
     * @brief The occlusion baked into the level's geometry (LevelRelief.cpp): how much of the sky a point with a normal
     *        sees past what stands round it within 1.6 m (walls, rocks, the things on the floor), and how near the floor
     *        a face looking sideways is. Only the grid and the props are asked: no runtime cost, darker at the walls'
     *        feet, in inner corners, under and beside things.
     * @param[in] p  the point
     * @param[in] n  its normal
     * @return 1 open .. about 0.4 deep in a corner
     */
    float occlusion( Vector3 p, Vector3 n ) const;

    /**
     * @brief Bakes occlusion() into a builder's vertices (MeshBuilder::shade).
     * @param[in,out] b  the builder
     */
    void shadeOcclusion( MeshBuilder &b ) const;

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
    float                   m_wallHeight = 3.2f;                                    ///< wall height
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
    /// The gardens' ponds (LevelGarden.cpp; behind the edges, never by a path): middle x, z and the radii along x and z
    /// (their shore wavers round that ellipse, pondShape). Nothing walks there: they only show.
    std::vector<Vector4>             m_ponds;
    std::vector<Vector4>             m_shallows;    ///< shallow water on the lawn by the ways (as m_ponds; only seen)
    std::vector<std::pair<int, int>> m_relays;      ///< Mother Sevenfold's relays (cells)
    std::vector<std::pair<int, int>> m_mothers;     ///< her bodies (cells)
    int                              m_birds = 0;   ///< birds that fly over it and never land
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
    float       m_lampGain = 1.0f;            ///< the place's own lamps this much stronger (pools of light, phase F2)
    float       m_lampReach = 1.0f;           ///< ... and their reach times this
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
    std::vector<std::pair<std::string, std::string>> m_hallTex;   ///< a theme's own halls' floor here ("hallfloor")
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

    /**
     * @brief How a pond's shore wavers round its ellipse (LevelGarden.cpp; the ground's hollow and the water's disc
     *        both follow it).
     * @param[in] i      the pond
     * @param[in] angle  the direction from its middle (radians, in the ellipse's own measure)
     * @return the shore's distance as a share of the ellipse's (0.86 .. 1.16)
     */
    float pondShape( size_t i, float angle ) const;
    /**
     * @param[in] i  the pond
     * @param[in] x  east (m)
     * @param[in] z  south (m)
     * @return how far a point lies out from the pond's middle: 0 there, 1 at its shore
     */
    float pondReach( size_t i, float x, float z ) const;
    /**
     * @param[in] i  the shallow water (m_shallows)
     * @param[in] x  east (m)
     * @param[in] z  south (m)
     * @return how far a point lies out from its middle: 0 there, 1 at its shore
     */
    float shallowReach( size_t i, float x, float z ) const;

    /**
     * @brief The fixed lights' masks (LevelInterior.cpp; after build): for each light, seen from above, where its light
     *        reaches past the walls (rays from it over a fine grid of the walls, their cut corners and filled corners; a
     *        wall's first 0.6 m still lit, the rest and what lies behind it dark), softened at the edges. A light in a
     *        wall shines from the nearest open point in front of it (within 0.7 m); the gardens' hedges and the sloped
     *        rock out in the open let it through. One nothing stops within its reach has none.
     * @return the masks, one square of 64 texels a light (an empty atlas when none has one)
     */
    LightMasks lightMasks() const;

    /**
     * @brief The wet map (LevelInterior.cpp; Gfx::wetMap): per cell how wet its floor is (puddles where water runs: the
     *        exchangers, the shafts, the streets; none in a vacuum, out in the open or where it burns), its floor's
     *        height, how polished it is (the vaults, the bridge, the clinic) and where its water gathers (by the walls,
     *        the drains, in low spots); the walls take their floor's values.
     * @return RGBA texels, row by row (empty where no floor is wet or polished)
     */
    std::vector<unsigned char> wetMap() const;

    std::vector<Drawable> m_cones;   ///< the lamps' soft cones of light in the haze (after build; Gfx::cones)
    /// The bench's breakdown (--plaininterior): levels built without the floors' sunken channels and without the
    /// places' own wall lamps (the old housings, no cones), as before the graphics round's fourth phase.
    static inline bool s_plainInterior = false;

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
        MeshBuilder &m_cards;    ///< cut-out leaf clusters round the crowns and bushes (no shadow)
        MeshBuilder &m_blades;   ///< cut-out tufts of grass (no shadow)
        MeshBuilder &m_needles;  ///< cut-out fir branches (no shadow)
        MeshBuilder &m_deep;     ///< leaf cards deeper in the crowns (no shadow; not on frugal)
        MeshBuilder &m_lush;     ///< more tufts of grass (no shadow; middle and high only)
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
        FIR,        ///< a fir: tiers of drooping branches round a trunk, narrower upwards
        COUNT
    };

    /**
     * @brief Plans the gardens (LevelGarden.cpp): which wall cells of a park become its edge (by the paths) and what lies
     *        behind them (garden, then forest), from each wall cell's distance to the open cells.
     * @param[in] on  the level has gardens (its park keeps the hedges' surface)
     */
    void planGarden( bool on );
    /** @return the cell's garden depth: 0 none, 1 its edge by the paths, 2 and more what lies behind it */
    int garden( int r, int c ) const;
    /**
     * @brief A garden wall cell (LevelGarden.cpp): its edge by the paths (rocks, or a thicket of bushes and trees),
     *        a strip of garden behind it, or forest.
     * @param[in]     r    the row
     * @param[in]     c    the column
     * @param[in,out] kit  the builders
     */
    void gardenCell( int r, int c, GardenKit &kit );
    /**
     * @brief A park floor cell's ground (LevelGarden.cpp): the lawn's top in large patches of lusher and drier grass
     *        (one colour function for all cells: no seams), tufts of grass on it.
     * @param[in]     r    the row
     * @param[in]     c    the column
     * @param[in]     h    its height
     * @param[in,out] kit  the builders
     */
    void groundTop( int r, int c, float h, GardenKit &kit );
    /**
     * @brief The gardens' paths along the route as decals of trodden earth fading into the grass, and patches of earth
     *        here and there (LevelGarden.cpp; after the dressing, which lays the decals anew).
     * @param[in] s  the surfaces (the decals there are)
     */
    void gardenPaths( const LevelSurfaces &s );
    /**
     * @brief Leaves hanging over an edge (LevelGarden.cpp): clumps over the rim, ivy flat on the face below, strands.
     * @param[in]     rim   the middle of the edge, at its height
     * @param[in]     n     out of the face (level)
     * @param[in]     drop  how far down the face they reach at most
     * @param[in]     h     a hash: their shapes
     * @param[in,out] kit   the builders
     */
    void creeper( Vector3 rim, Vector3 n, float drop, unsigned h, GardenKit &kit );
    /**
     * @brief Now and then low plants at the foot of a garden's edge, on the floor in front of it (LevelGarden.cpp).
     * @param[in]     r    the floor cell's row
     * @param[in]     c    its column
     * @param[in]     d    the direction of the hedge from it (0 north, 1 east, 2 south, 3 west)
     * @param[in,out] kit  the builders
     */
    void hedgeFoot( int r, int c, int d, GardenKit &kit );
    /**
     * @brief The forest beyond the level's edge where a garden reaches it (LevelGarden.cpp).
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
     * @brief The lawn's top on a garden cell or beyond the edge, in patches (LevelGarden.cpp).
     * @param[in]     r      the row
     * @param[in]     c      the column
     * @param[in,out] kit    the builders
     * @param[in]     shade  darker under trees (1: in the open)
     */
    void lawn( int r, int c, GardenKit &kit, float shade = 1.0f );
    /**
     * @brief Plans the gardens' ponds (LevelGarden.cpp, after planGarden's depths): in the garden behind the edges where
     *        neither the edge nor a path is within reach of their banks, far apart.
     */
    void planPonds();
    /**
     * @brief Plans the gardens' shallow water (LevelGarden.cpp, at the build's end): on the ways over the lawn, where
     *        all round is level lawn and nothing stands in its middle, far apart and from the ponds.
     */
    void planShallows();
    /**
     * @param[in] p       a point
     * @param[in] margin  how far round a pond's bank counts (m)
     * @return whether it lies in a pond or its bank
     */
    bool inPond( Vector3 p, float margin ) const;
    /**
     * @brief The ground by a pond: its hollow under the water and its bank (LevelGarden.cpp).
     * @param[in]  x    east (m)
     * @param[in]  z    south (m)
     * @param[out] wet  how wet it is: 1 under the water, falling to 0 up the bank
     * @return the ground's height there (0 away from the ponds)
     */
    float pondGround( float x, float z, float &wet ) const;
    /**
     * @brief The ponds' shores (LevelGarden.cpp): reeds, stones half in the water, leaves floating on it.
     * @param[in,out] kit  the builders
     */
    void gardenPonds( GardenKit &kit );

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

    /**
     * @brief Plans the rock out in the open (LevelRelief.cpp): the wall cells whose nearest open cell is the ground of
     *        a natural place out in the open (dust, snow, ice, regolith), with no door, shaft or other place beside
     *        them, become sloped rock: their outer corners all cut, the filled corners in front of them dropped (the
     *        slope rounds them), no niches. Before the build, after planShapes().
     */
    void planRelief();
    /**
     * @param[in] r  row
     * @param[in] c  column
     * @return whether the wall cell is built as sloped rock (planRelief)
     */
    bool sloped( int r, int c ) const;
    /**
     * @param[in] r  row
     * @param[in] c  column
     * @return the place whose rock a sloped cell is (its nearest open cell's theme)
     */
    ThemeE rockTheme( int r, int c ) const;
    /**
     * @brief The sloped rock's height at a point (LevelRelief.cpp): from the floor at the open ground's edge up a steep,
     *        lumpy face that leans back to an uneven top; a little below the floor under open ground; level with the
     *        walls' tops where it meets a wall that stands upright.
     * @param[in] x  east (m)
     * @param[in] z  south (m)
     * @return the height
     */
    float reliefAt( float x, float z ) const;
    /**
     * @brief The relief at a point of the cells' grid (six to a cell's edge, closer by its edges), computed once.
     * @param[in] gx  the point's column (cell column times six plus its step)
     * @param[in] gz  its row
     * @return the height
     */
    float reliefPoint( int gx, int gz ) const;
    /**
     * @brief The relief between the grid's points (for the occlusion).
     * @param[in] x  east (m)
     * @param[in] z  south (m)
     * @return the height
     */
    float reliefLookup( float x, float z ) const;
    /**
     * @brief A sloped rock cell (LevelRelief.cpp): the relief over its square, its faces in the rock's colour, its
     *        flat tops darker (as the walls' lids are).
     * @param[in]     r     the row
     * @param[in]     c     the column
     * @param[in,out] b     the builder
     * @param[in]     face  the colour of its faces
     * @param[in]     top   the colour of its tops
     */
    void reliefCell( int r, int c, MeshBuilder &b, Color face, Color top );
    /**
     * @brief Stones at the foot of a sloped rock's face, on the floor in front of it (LevelRelief.cpp).
     * @param[in]     r       the floor cell's row
     * @param[in]     c       its column
     * @param[in]     d       the direction of the rock from it (0 north, 1 east, 2 south, 3 west)
     * @param[in,out] b       the builder (the rock's)
     * @param[in]     colour  their colour
     */
    void rubble( int r, int c, int d, MeshBuilder &b, Color colour );
    /**
     * @brief The foot of a sloped rock's face in front of a floor cell (LevelRelief.cpp): a low lumpy band of the rock
     *        bulging up to some 15 cm out over the floor, wavering along the face, so that the rock does not meet the
     *        ground along the cells' straight line (only seen: collision and the floors stay as they are).
     * @param[in]     r       the floor cell's row
     * @param[in]     c       its column
     * @param[in]     d       the direction of the rock from it (0 north, 1 east, 2 south, 3 west)
     * @param[in,out] b       the builder (the rock's)
     * @param[in]     colour  its colour
     */
    void rockFoot( int r, int c, int d, MeshBuilder &b, Color colour );
    /**
     * @brief A slope of scree at the foot of a sloped rock's face (LevelRelief.cpp, phase F2): from inside the face out
     *        20-46 cm over the ground, its outline wandering by a noise of the place, of the ground's own surface (its
     *        shapes make it scree there). Only seen: collision, groundAt, the paths and the sight lines stay.
     * @param[in]     r  the floor cell's row
     * @param[in]     c  its column
     * @param[in]     d  the direction of the rock from it (0 north, 1 east, 2 south, 3 west)
     * @param[in,out] b  the builder (the cell's ground)
     */
    void screeApron( int r, int c, int d, MeshBuilder &b ) const;
    /** @brief The props near each cell and the highest that stands in it, for occlusion() (after the build's props). */
    void prepareOcclusion();
    /**
     * @brief The top of what stands at a point, for the occlusion: walls, rocks, the relief, props (as 0.9 m high).
     * @param[in] x  east (m)
     * @param[in] z  south (m)
     * @return the height (-1000 over a shaft)
     */
    float standsAt( float x, float z ) const;
    /**
     * @brief The floor a point stands over, for the occlusion: its cell's, or in rock the open cell its normal faces.
     * @param[in]  p  the point
     * @param[in]  n  its normal
     * @param[out] h  the floor's height
     * @return false: none (a shaft, deep in rock)
     */
    bool floorFor( Vector3 p, Vector3 n, float &h ) const;

    /** @brief A sunken channel in a floor cell under a grating (LevelInterior.cpp): only seen, walked over as before. */
    enum PitE : unsigned char
    {
        PIT_NONE,       ///< none
        PIT_SUMP,       ///< a square sump in the middle (where a plate of grating lay on the floor before)
        PIT_DUCT_X,     ///< a duct along a corridor running east-west
        PIT_DUCT_Z,     ///< ... north-south
        PIT_GUTTER_N,   ///< a gutter along the wall to the north (a street's)
        PIT_GUTTER_E,   ///< ... east
        PIT_GUTTER_S,   ///< ... south
        PIT_GUTTER_W    ///< ... west
    };
    /**
     * @brief Plans the floors' sunken channels (LevelInterior.cpp; before the build): ducts along the corridors indoors,
     *        gutters along a street's walls, a sump where a plate of grating lay; only on level floors with no step,
     *        shaft, door, window or filled corner beside them.
     */
    void planPits();
    /**
     * @brief Where a floor cell's channel opens (in eighths of the cell).
     * @param[in]  r   the row
     * @param[in]  c   the column
     * @param[out] i0  its first eighth along x
     * @param[out] i1  one past its last
     * @param[out] j0  its first along z
     * @param[out] j1  one past its last
     * @return false: the cell has none
     */
    bool pitRect( int r, int c, int &i0, int &i1, int &j0, int &j1 ) const;
    /**
     * @brief A floor cell's channel below its opening (LevelInterior.cpp): its sides and bottom (open where the next
     *        cell's runs on), what runs in it as the place has it (pipes, cables, a line of light, glowing slag, water),
     *        and the grating over it, flush with the floor.
     * @param[in]     r      the row
     * @param[in]     c      the column
     * @param[in]     h      the floor's height
     * @param[in,out] sunk   the builder (darkened by hand with the depth: not the baked occlusion)
     * @param[in,out] rust   the grating's builder in the industrial places
     * @param[in,out] steel  ... in the clean and cold ones
     * @param[in,out] glows  the glowing boxes
     */
    void pitCell( int r, int c, float h, MeshBuilder &sunk, MeshBuilder &rust, MeshBuilder &steel,
                  std::vector<GlowBox> &glows );
    /**
     * @param[in] x  east (m)
     * @param[in] z  south (m)
     * @return whether a lamp's light stops there, seen from above: a wall (not its cut corners, not a garden's), a
     *         filled corner; not doors, counters or heat exchangers (they stand below the lamps)
     */
    bool blocksLight( float x, float z ) const;
    /**
     * @brief A wall lamp of the kind its place has (LevelInterior.cpp): an industrial shade on an arm, a street's
     *        lantern, a clean light bar under a hood, a cluster of glowing crystal; and its cone of light in the haze.
     * @param[in]     at      the old housing's middle (0.18 m out of the wall, at the lamp's height)
     * @param[in]     n       out of the wall
     * @param[in]     t       the place
     * @param[in]     glow    the glass's glow
     * @param[in]     colour  its light's colour times intensity (the cone's colour)
     * @param[in]     cone    with a cone (a steady lamp)
     * @param[in,out] kit     the builders
     * @param[in]     reach   how much further out of the wall it hangs (in front of the wall's pipes; phase F4)
     * @return false: the place has no lamp of its own (the old housing is built)
     */
    bool lamp( Vector3 at, Vector3 n, ThemeE t, Vector4 glow, Vector3 colour, bool cone, DressKit &kit,
               float reach = 0.0f );
    /** @brief The lamps' cones as meshes (m_cones), at the build's end. */
    void buildCones();
    /** @brief A cone of light under a lamp. */
    struct Cone
    {
        Vector3 m_apex;     ///< at the lamp's glass
        Vector3 m_axis;     ///< where it points (unit)
        float   m_length;   ///< how long (m)
        float   m_radius;   ///< its radius at the glass
        float   m_spread;   ///< its radius grows this much a metre
        Color   m_colour;   ///< the light's colour (alpha: its strength)
    };
    std::vector<Cone>          m_coneSpecs;   ///< during the build
    std::vector<unsigned char> m_pit;         ///< per cell: its sunken channel (PitE)

    /** @brief The kind of room a cell lies in (planRooms; LevelFloors.cpp): its floor follows it. */
    enum RoomE : unsigned char
    {
        ROOM_NONE,       ///< a wall, a door
        ROOM_HALL,       ///< a large room (a hundred cells and more)
        ROOM_CORRIDOR,   ///< a way of up to four cells' width, and what joins nothing but it
        ROOM_SIDE        ///< a smaller room
    };
    /**
     * @brief Plans the rooms (LevelFloors.cpp; before the build): every open cell that a block of 5 x 5 open cells
     *        covers belongs to a room (the blocks' middles that touch are one room; a hundred cells or more make a
     *        hall), the rest to corridors; a pocket of up to three cells off a single room is that room's.
     */
    void planRooms();
    /**
     * @param[in] r  row
     * @param[in] c  column
     * @return the kind of room the cell lies in (RoomE)
     */
    int roomKind( int r, int c ) const;
    /**
     * @param[in] r  row
     * @param[in] c  column
     * @return the cell's room (-1 none; corridors are rooms of their own)
     */
    int roomOf( int r, int c ) const;
    /**
     * @param[in] r  row
     * @param[in] c  column
     * @return where a hall's floor cell has its band along the walls: bit d (0 north .. 3 west) along that wall, bit
     *         4 + q round the outer corner q of a wall beside it (0 north-west .. 3 south-west); 0 none
     */
    unsigned bandSides( int r, int c ) const;
    /**
     * @param[in] band   a cell's band (bandSides)
     * @param[in] i      a square of its top's grid along x
     * @param[in] j      ... along z
     * @param[in] steps  squares of the grid along each edge (the band is a quarter of them wide: half a metre)
     * @return whether the square lies in the band
     */
    static bool bandSquare( unsigned band, int i, int j, int steps );
    /**
     * @brief The thin dark joints where a hall cell's band meets the floor (LevelFloors.cpp): along each edge of a
     *        band's square whose neighbour (in this cell or the next) is floor without a band.
     * @param[in]     r     row
     * @param[in]     c     column
     * @param[in]     band  its band (bandSides)
     * @param[in]     s     the surfaces (the places' bands)
     * @param[in,out] b     the builder (steel)
     */
    void bandJoints( int r, int c, unsigned band, const LevelSurfaces &s, MeshBuilder &b ) const;
    /**
     * @brief Which material of a patchwork a cell takes (as the variants were laid before they were one draw): the
     *        floors in ragged patches of a few cells, the walls in short runs, the plain one most.
     * @param[in] r     row
     * @param[in] c     column
     * @param[in] wall  a wall's (else a floor's)
     * @param[in] n     the variants there are
     * @return 0 .. n - 1 a variant, n or n + 1 the plain one
     */
    static int patchPick( int r, int c, bool wall, int n );
    /**
     * @brief The halls' centre motifs (LevelFloors.cpp; after the dressing): a painted mark, a mosaic or an inlay of
     *        the place's kind in the middle of each hall, where no step, shaft, channel or thing on the floor is in the
     *        way (under a show piece there may be one).
     * @param[in] s  the surfaces (the decals there are)
     */
    void motifs( const LevelSurfaces &s );
    /**
     * @brief The halls' painted bay lines (LevelFloors.cpp; after the motifs, phase F4) where their place's halls take
     *        them (LevelSurfaces::m_bays: the works' concrete): two lines along the middle of each hall's long axis (a
     *        walkway), and in a hall wide enough a row of bays by each long wall, open to the walkway; laid in pieces of
     *        two metres as decals (`bay_line`) only where the floor under them is the hall's own, even, with nothing on
     *        it and the motif clear. Only seen.
     * @param[in] s  the surfaces (the decals there are, which places take the lines)
     */
    void bayLines( const LevelSurfaces &s );
    /**
     * @brief The ways along the route (the autopilot's: through each corridor, across each room, kept to the middle
     *        of the open floor; LevelFloors.cpp): m_way. The gardens' paths and the trodden ground out in the open.
     */
    void planWays();
    /**
     * @brief The ground's shapes out in the open at a point (LevelFloors.cpp): how trodden (along the ways) and how
     *        much scree (at the foot of the rock), each 0 .. 1, fading over a metre and more.
     * @param[in] p  the point
     * @return x trodden, y scree
     */
    Vector2 groundShape( Vector3 p ) const;
    std::vector<unsigned char> m_room;     ///< per cell: the kind of its room (RoomE)
    std::vector<int>           m_roomOf;   ///< per cell: its room (-1 none)
    std::vector<unsigned char> m_way;      ///< per cell: 1 on a way along the route (planWays)

    int                        m_rows = 0;
    int                        m_cols = 0;
    std::vector<Cell>          m_cells;
    std::vector<FaceSlot>      m_faces;     ///< wall faces free for decals (during build)
    std::vector<unsigned char> m_cut;       ///< per wall cell: its outer corners cut off (bit k: NW, NE, SE, SW)
    std::vector<unsigned char> m_fill;      ///< per floor cell: its inner corners filled (bits as m_cut)
    std::vector<signed char>   m_niche;     ///< per wall cell: the side its niche opens to (-1: none)
    std::vector<unsigned char> m_gallery;   ///< per floor cell: the walls a gallery runs along (bit d)
    std::vector<unsigned char> m_garden;    ///< per wall cell: its garden depth (0 none, 1 the edge, 2.. behind)
    std::vector<unsigned char> m_path;      ///< per floor cell: a garden's path runs over it
    std::vector<unsigned char> m_slope;     ///< per wall cell: 1 sloped rock, 2 and all round it too (a coarse grid)
    std::vector<unsigned char> m_rockOf;    ///< per sloped cell: the theme of its nearest open cell (ThemeE)
    mutable std::vector<float>
        m_relief;   ///< the relief at the grid's points ((6 cols + 1) x (6 rows + 1); NaN: not yet)
    std::vector<std::vector<int>> m_occProps;      ///< per cell: the props over it (for the occlusion; not what breaks)
    std::vector<float>            m_occTop;        ///< per cell: the highest that stands in it (for the occlusion)
    std::vector<std::vector<int>> m_propsByCell;   ///< during the lawn's build: the props near each cell
    std::vector<Vector3>       m_sight;     ///< per cell what is seen there: x up to where it stands from the floor,
                                            ///< y to z a crown or lintel above (y > z: none); for the camera
    /** @brief Something standing from the floor up to `top` over a footprint (the cells it mostly covers). */
    void stand( Vector3 centre, Vector3 radii, float top );
    /** @brief Something hanging between `low` and `high` over a footprint (a crown, a lintel). */
    void hang( Vector3 centre, Vector3 radii, float low, float high );
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
