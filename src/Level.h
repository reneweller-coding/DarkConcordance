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
#include <map>
#include <set>
#include <string>
#include <vector>

namespace spaceopera
{

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

/** @brief Surfaces the level geometry uses. */
struct LevelSurfaces
{
    int m_rock;     ///< rock walls
    int m_floor;    ///< deck plates
    int m_grate;    ///< grating
    int m_metal;    ///< steel
    int m_hazard;   ///< hazard stripes
    int m_rust;     ///< rusty steel
    int m_foil;     ///< thermal foil (the bodies)
    int m_glow;     ///< lamps
    int m_crate;    ///< painted crates
    int m_box;      ///< corrugated container steel
    int m_hull;     ///< the ship's bulkheads
    int m_cloth;    ///< fabric (bundles, bags)
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
    COUNT        ///< number of themes
};

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

    std::string             m_name;                                              ///< key of the name shown at the start
    std::string             m_act;                                               ///< key of the act's name
    float                   m_gravity = 9.81f;                                   ///< m/s²
    Vector4                 m_fog = { 0.01f, 0.05f, 0.5f, 1.0f };                ///< fog as in Lighting
    Vector3                 m_fogColor = { 0.002f, 0.002f, 0.003f };             ///< the fog's own glow
    Vector3                 m_ambUp = { 0.01f, 0.01f, 0.01f };                   ///< ambient from above
    Vector3                 m_ambDown = { 0.01f, 0.01f, 0.01f };                 ///< ambient from below
    float                   m_wallHeight = 4.5f;                                 ///< wall height
    std::vector<Marker>     m_markers;                                           ///< spawns, lights, pickups
    std::vector<Line>       m_lines;                                             ///< spoken lines
    std::vector<Hint>       m_hints;                                             ///< hints
    std::vector<LevelLight> m_lights;                                            ///< fixed lights (after build)
    std::vector<Zone>       m_zones;                                             ///< areas with their themes
    std::vector<Sign>       m_signs;                                             ///< signs (after build)
    std::vector<Prop>       m_props;                                             ///< solid props (after build)
    std::vector<Seat>       m_seats;                                             ///< stools (after build)
    std::vector<std::pair<std::string, std::pair<int, int>>> m_signRequests;     ///< "sign" lines: texture, cell
    std::string                                              m_sky;              ///< the sky's picture ("" none)
    bool                                                     m_thrust = false;   ///< the ship dodges: gravity tilts
    std::string                                              m_next;             ///< the level after this one
    std::string                                              m_gate = "bossdead";   ///< the event that opens the door
    std::string                                              m_ambience = "tunnels";   ///< the background sound
    std::vector<std::pair<int, int>>                         m_route;   ///< the autopilot's waypoints (cells), in order
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
    std::vector<std::pair<int, int>> m_ilkaWay;     ///< where she runs after the alarm (then she is gone)
    std::map<int, std::string>       m_locks;       ///< door cell (row * 1000 + col) -> the event that unlocks it
    std::map<int, std::string>       m_notes;       ///< terminal cell (row * 1000 + col) -> the key of its text
    std::map<int, int>               m_elites;      ///< enemy cell (row * 1000 + col) -> 1 a champion, 2 the guardian
    std::string                      m_generated;   ///< a generated level's file (empty for a written one)
    std::vector<std::pair<std::string, std::string>>
                          m_exits;           ///< (level, flag): where the lift goes once the flag is set
    std::set<std::string> m_repeat;          ///< exits that stay open once done (the depths)
    std::set<int>         m_outdoorThemes;   ///< themes out in the open (ThemeE as int)
    int                   m_boardRow = -1, m_boardCol = 0, m_boardWidth = 0;   ///< the harvest board
    Vector3               m_boardCorner[4] = {};                               ///< its picture's corners (after build)
    std::vector<Npc>      m_npcs;                                              ///< people to talk to
    std::vector<TalkNode> m_talk;                                              ///< the conversations' steps

    /**
     * @brief A conversation's step by its key.
     * @param[in] id  the key
     * @return the step, or nullptr
     */
    const TalkNode *talk( const std::string &id ) const;

private:
    int               m_rows = 0;
    int               m_cols = 0;
    std::vector<Cell> m_cells;
};

}   // namespace spaceopera
