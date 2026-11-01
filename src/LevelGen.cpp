/**
 * @file LevelGen.cpp
 * @brief The level generator: the layout (rooms on a way, side rooms, corridors, doors, the exit lift), the dressing
 *        per theme (cover, pits with bridges, raised floors, stalls, shelves, stacks), and what is placed (packs,
 *        champions, lockers, terminals, capsules, pickups, lights, triggers, the autopilot's route).
 * @author Rene Weller
 */
#include "LevelGen.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <set>
#include <sstream>
#include <vector>

namespace spaceopera
{

namespace
{

/** @brief A small random generator of its own (xorshift), so a seed always builds the same level. */
struct Rng
{
    unsigned m_s;
    unsigned next()
    {
        m_s ^= m_s << 13;
        m_s ^= m_s >> 17;
        m_s ^= m_s << 5;
        return m_s;
    }
    float f() { return ( next() >> 8 ) * ( 1.0f / 16777216.0f ); }
    int range( int lo, int hi ) { return hi <= lo ? lo : lo + (int)( next() % (unsigned)( hi - lo + 1 ) ); }
    bool chance( float p ) { return f() < p; }
};

/** @brief Rows and columns, inclusive. */
struct Rect
{
    int m_r0, m_c0, m_r1, m_c1;
    /** @return whether it comes closer than `margin` cells to the other */
    bool hits( const Rect &o, int margin ) const
    {
        return m_r0 - margin <= o.m_r1 && o.m_r0 <= m_r1 + margin && m_c0 - margin <= o.m_c1 && o.m_c0 <= m_c1 + margin;
    }
    int width() const { return m_c1 - m_c0 + 1; }
    int height() const { return m_r1 - m_r0 + 1; }
    Rect grown( int n ) const { return { m_r0 - n, m_c0 - n, m_r1 + n, m_c1 + n }; }
};

enum RoomKind
{
    START,
    MAIN,
    SIDE,
    ARENA
};

struct Room
{
    Rect        m_r;         ///< the floor inside the walls
    int         m_kind;      ///< START, MAIN, SIDE, ARENA
    std::string m_theme;     ///< its theme's name
    int         m_in = -1;   ///< the corridor one comes in by
};

struct Hall
{
    Rect m_r;            ///< its floor (through both walls)
    int  m_from, m_to;   ///< the rooms it joins
    int  m_dir;          ///< from `from` to `to`: 0 north, 1 east, 2 south, 3 west
    bool m_door;         ///< a door in the wall of `to`
};

struct Pack
{
    std::string m_letters;   ///< who (map letters)
    float       m_weight;    ///< how often
};

/** @brief What the gen... lines ask for. */
struct Recipe
{
    int                                 m_rooms = 8;   ///< rooms on the way, both ends included
    int                                 m_side = 3;    ///< side rooms
    int                                 m_wMin = 10, m_wMax = 16, m_hMin = 8, m_hMax = 13;   ///< a room's size (cells)
    std::vector<std::string>            m_themes;                                            ///< themes to choose from
    std::string                         m_arenaTheme;                                        ///< the set piece's theme
    std::vector<Pack>                   m_packs;                                             ///< enemy packs
    float                               m_density = 1.0f;       ///< packs per 100 floor cells
    float                               m_elite = 0.2f;         ///< share of packs led by a champion
    std::string                         m_arena = "guardian";   ///< the set piece: taxator, guardian
    std::string                         m_guard = "%";          ///< the guardian's kinds (letters)
    std::vector<std::string>            m_notes;                ///< terminal texts to scatter
    bool                                m_void = false;         ///< open space around the rooms instead of rock
    std::set<std::string>               m_outdoor;              ///< themes out in the open: no walls, no air
    std::set<std::string>               m_wild;   ///< themes laid out as in the wild: ragged rooms, winding ways
    std::string                         m_startTheme;           ///< the start's theme (an airlock, say)
    float                               m_oxygen = 0.0f;   ///< chance of an oxygen canister in a room out in the open
    float                               m_water = 0.0f;    ///< chance of a ball of water floating in a room
    std::vector<std::pair<char, float>> m_features;        ///< things per room: bore holes, geysers, coils, power cells
    struct ThingRate
    {
        std::string m_name, m_where;   ///< what, where: main, side, arena, all
        float       m_rate;
    };
    std::vector<ThingRate> m_things;             ///< things by name per room ("genthing name rate where")
    char                   m_hover = 0;          ///< who hangs over the abyss beside the rooms (balloon guards)
    float                  m_hoverRate = 0.0f;   ///< how many per room
    int                    m_power = 0;          ///< the ark: every door on the way locked until powered (its cost)
    bool machine() const
    {
        return m_arena == "tug" || m_arena == "rig" || m_arena == "bell" || m_arena == "lance" ||
               m_arena == "platform" || m_arena == "column" || m_arena == "catcher";
    }
};

const int DR[4] = { -1, 0, 1, 0 };   ///< north, east, south, west
const int DC[4] = { 0, 1, 0, -1 };

/** @brief Smooth value noise in two dimensions (-1 .. 1): the wild rooms' ragged outlines. */
float noise2( float x, float y, unsigned seed )
{
    auto lattice = [seed]( int i, int j )
    {
        unsigned h = (unsigned)i * 374761393u + (unsigned)j * 668265263u + seed * 2246822519u;
        h = ( h ^ ( h >> 13 ) ) * 1274126177u;
        return ( ( h ^ ( h >> 16 ) ) & 0xffffu ) / 32767.5f - 1.0f;
    };
    int   i = (int)floorf( x ), j = (int)floorf( y );
    float fx = x - i, fy = y - j;
    fx = fx * fx * ( 3.0f - 2.0f * fx ), fy = fy * fy * ( 3.0f - 2.0f * fy );
    float a = lattice( i, j ), b = lattice( i + 1, j ), c = lattice( i, j + 1 ), d = lattice( i + 1, j + 1 );
    return a + ( b - a ) * fx + ( c - a ) * fy + ( a - b - c + d ) * fx * fy;
}

std::string trim( const std::string &s )
{
    size_t a = s.find_first_not_of( " \t\r" );
    size_t b = s.find_last_not_of( " \t\r" );
    return a == std::string::npos ? std::string() : s.substr( a, b - a + 1 );
}

/** @brief Splits a level file into its recipe, the header lines to pass on and its sections. */
Recipe parse( const std::string &text, std::string &head, std::string &tail )
{
    Recipe            r;
    std::stringstream in( text );
    std::string       line;
    bool              body = false;
    while ( std::getline( in, line ) )
    {
        if ( !line.empty() && line.back() == '\r' ) line.pop_back();
        std::string t = trim( line );
        if ( !t.empty() && t[0] == '[' ) body = true;
        if ( body )
        {
            tail += line + "\n";
            continue;
        }
        std::stringstream kv( t );
        std::string       key;
        kv >> key;
        if ( key.rfind( "gen", 0 ) != 0 )
        {
            head += line + "\n";
            continue;
        }
        std::string s;
        if ( key == "genrooms" )
            kv >> r.m_rooms;
        else if ( key == "genside" )
            kv >> r.m_side;
        else if ( key == "gensize" )
            kv >> r.m_wMin >> r.m_wMax >> r.m_hMin >> r.m_hMax;
        else if ( key == "gentheme" )
            while ( kv >> s ) r.m_themes.push_back( s );
        else if ( key == "genarenatheme" )
            kv >> r.m_arenaTheme;
        else if ( key == "genpack" )
        {
            Pack p{ "", 1.0f };
            kv >> p.m_letters >> p.m_weight;
            if ( !p.m_letters.empty() ) r.m_packs.push_back( p );
        }
        else if ( key == "gendensity" )
            kv >> r.m_density;
        else if ( key == "genelite" )
            kv >> r.m_elite;
        else if ( key == "genarena" )
            kv >> r.m_arena;
        else if ( key == "genguard" )
            kv >> r.m_guard;
        else if ( key == "gennote" )
            while ( kv >> s ) r.m_notes.push_back( s );
        else if ( key == "genvoid" )
        {
            int on = 0;
            kv >> on;
            r.m_void = on != 0;
        }
        else if ( key == "genoutdoor" )
            while ( kv >> s ) r.m_outdoor.insert( s );
        else if ( key == "genwild" )
            while ( kv >> s ) r.m_wild.insert( s );
        else if ( key == "genstarttheme" )
            kv >> r.m_startTheme;
        else if ( key == "genoxygen" )
            kv >> r.m_oxygen;
        else if ( key == "genwater" )
            kv >> r.m_water;
        else if ( key == "genfeature" )
        {
            std::string sym;
            float       rate = 0.0f;
            kv >> sym >> rate;
            if ( !sym.empty() ) r.m_features.push_back( { sym[0], rate } );
        }
        else if ( key == "genthing" )
        {
            Recipe::ThingRate th;
            kv >> th.m_name >> th.m_rate >> th.m_where;
            if ( th.m_where.empty() ) th.m_where = "all";
            if ( !th.m_name.empty() ) r.m_things.push_back( th );
        }
        else if ( key == "genhover" )
        {
            std::string sym;
            kv >> sym >> r.m_hoverRate;
            r.m_hover = sym.empty() ? 0 : sym[0];
        }
        else if ( key == "genpower" )
        {
            kv >> r.m_power;
            head += "console " + std::to_string( r.m_power ) + "\n";
        }
    }
    if ( r.m_themes.empty() ) r.m_themes.push_back( "service" );
    if ( r.m_arenaTheme.empty() ) r.m_arenaTheme = r.m_themes[0];
    if ( r.m_packs.empty() ) r.m_packs.push_back( { "LLL", 1.0f } );
    r.m_rooms = std::max( 3, r.m_rooms );
    return r;
}

/** @brief One level being generated. */
class Gen
{
public:
    Gen( const Recipe &r, unsigned seed ) : m_rc( r ), m_rng{ seed * 2654435761u + 12345u } {}

    /** @return false if the rooms would not fit (another seed then) */
    bool run()
    {
        if ( !layout() ) return false;
        normalise();
        carve();
        for ( size_t i = 0; i < m_rooms.size(); ++i ) dress( (int)i );
        populate();
        crownLeader();
        return true;
    }

    /** @brief One champion of the level leads its pack: its line gets " leader" (a name, two gifts, an escort). */
    void crownLeader()
    {
        std::vector<size_t> elites;
        for ( size_t i = 0; i < m_extra.size(); ++i )
            if ( m_extra[i].rfind( "elite ", 0 ) == 0 && m_extra[i].find( " boss" ) == std::string::npos )
                elites.push_back( i );
        if ( !elites.empty() ) m_extra[elites[elites.size() * 2 / 3]] += " leader";
    }

    /** @return the level file */
    std::string text( const std::string &head, const std::string &tail, unsigned seed ) const
    {
        std::string out = "# Generated (seed " + std::to_string( seed ) + ").\n" + head;
        for ( const std::string &l : m_extra ) out += l + "\n";
        out += "\n[map]\n";
        for ( std::string row : m_g )
        {
            for ( char &ch : row )
                if ( ch == '\\' ) ch = '.';   // the things' cells, kept free while the rooms were filled
            out += row + "\n";
        }
        out += "\n" + tail;
        return out;
    }

private:
    const Recipe             m_rc;
    Rng                      m_rng;
    std::vector<Room>        m_rooms;
    std::vector<Hall>        m_halls;
    Rect                     m_exit = { 0, 0, 0, 0 };
    bool                     m_hasExit = false;
    Rect                     m_tug = { 0, 0, 0, 0 };   ///< open space beside the set piece where the tug lies
    bool                     m_hasTug = false;
    int                      m_tugSide = 1;   ///< which side of the set piece (1 east, 3 west)
    int                      m_rows = 0, m_cols = 0;
    std::vector<std::string> m_g;
    std::set<int>            m_reserved;   ///< by the corridors' mouths: nothing stands there
    std::set<int>            m_foes;       ///< cells with an enemy
    std::vector<std::string> m_extra;      ///< header lines: zones, the route, notes, champions

    bool outdoor( const std::string &theme ) const { return m_rc.m_outdoor.count( theme ) > 0; }

    /** @return a theme with walls and air (for the huts out in the open) */
    std::string indoorTheme() const
    {
        if ( !m_rc.m_startTheme.empty() && !outdoor( m_rc.m_startTheme ) ) return m_rc.m_startTheme;
        for ( const std::string &t : m_rc.m_themes )
            if ( !outdoor( t ) ) return t;
        return "service";
    }

    std::string pickTheme( const std::string &previous )
    {
        if ( !previous.empty() && m_rng.chance( 0.5f ) ) return previous;
        return m_rc.m_themes[m_rng.next() % m_rc.m_themes.size()];
    }

    /**
     * @brief A room beside room `from` in direction `dir`, joined by a straight corridor `len` cells long (through
     *        both walls) and `width` wide, if it fits.
     */
    bool place( int from, int dir, int w, int h, int len, int width, Room &room, Hall &hall )
    {
        const Rect &a = m_rooms[from].m_r;
        bool        vertical = dir == 0 || dir == 2;
        int         lo = vertical ? a.m_c0 + 1 : a.m_r0 + 1;
        int         hi = vertical ? a.m_c1 - width : a.m_r1 - width;
        int         span = vertical ? w : h;
        if ( hi < lo || span < width + 2 ) return false;
        int  k = m_rng.range( lo, hi );
        int  s0 = m_rng.range( k + width - span + 1, k - 1 );
        Rect b, c;
        switch ( dir )
        {
        case 0:
            b = { a.m_r0 - len - h, s0, a.m_r0 - len - 1, s0 + w - 1 };
            c = { b.m_r1 + 1, k, a.m_r0 - 1, k + width - 1 };
            break;
        case 1:
            b = { s0, a.m_c1 + len + 1, s0 + h - 1, a.m_c1 + len + w };
            c = { k, a.m_c1 + 1, k + width - 1, b.m_c0 - 1 };
            break;
        case 2:
            b = { a.m_r1 + len + 1, s0, a.m_r1 + len + h, s0 + w - 1 };
            c = { a.m_r1 + 1, k, b.m_r0 - 1, k + width - 1 };
            break;
        default:
            b = { s0, a.m_c0 - len - w, s0 + h - 1, a.m_c0 - len - 1 };
            c = { k, b.m_c1 + 1, k + width - 1, a.m_c0 - 1 };
            break;
        }
        for ( size_t i = 0; i < m_rooms.size(); ++i )
        {
            if ( (int)i == from ) continue;
            if ( b.hits( m_rooms[i].m_r, 3 ) || c.hits( m_rooms[i].m_r, 1 ) ) return false;
        }
        for ( const Hall &o : m_halls )
            if ( b.hits( o.m_r, 2 ) || c.hits( o.m_r, 1 ) ) return false;
        if ( m_hasExit && ( b.hits( m_exit, 2 ) || c.hits( m_exit, 1 ) ) ) return false;
        if ( m_hasTug && ( b.hits( m_tug, 2 ) || c.hits( m_tug, 1 ) ) ) return false;
        room.m_r = b;
        hall = { c, from, (int)m_rooms.size(), dir, false };
        return true;
    }

    bool layout()
    {
        m_rooms.clear();
        m_halls.clear();
        m_hasExit = false;
        m_hasTug = false;
        Room start;
        start.m_kind = START;
        start.m_r = { 0, 0, m_rng.range( 8, 10 ) - 1, m_rng.range( 9, 11 ) - 1 };
        start.m_theme = m_rc.m_startTheme.empty() ? pickTheme( "" ) : m_rc.m_startTheme;
        m_rooms.push_back( start );
        int prev = m_rng.range( 0, 3 ), cur = 0;
        for ( int i = 1; i < m_rc.m_rooms; ++i )
        {
            bool arena = i == m_rc.m_rooms - 1, ok = false;
            for ( int t = 0; t < 80 && !ok; ++t )
            {
                int dir = m_rng.chance( 0.45f ) ? prev : m_rng.range( 0, 3 );
                if ( dir == ( prev + 2 ) % 4 && t < 50 ) continue;
                // The set piece's exit leaves by its south wall, so its way in may not.
                if ( arena && dir == 0 ) continue;
                int w = arena ? m_rng.range( 20, 24 ) : m_rng.range( m_rc.m_wMin, m_rc.m_wMax );
                int h = arena ? m_rng.range( 15, 18 ) : m_rng.range( m_rc.m_hMin, m_rc.m_hMax );
                int len = m_rng.range( 2, 9 );
                int width = m_rng.chance( 0.25f ) ? 2 : 3;
                if ( arena || m_rc.m_power ) width = 3, len = std::max( len, 4 );
                Room room;
                Hall hall;
                if ( !place( cur, dir, w, h, len, width, room, hall ) ) continue;
                Rect exit = { 0, 0, 0, 0 };
                if ( arena )
                {
                    int mid = ( room.m_r.m_c0 + room.m_r.m_c1 ) / 2;
                    exit = { room.m_r.m_r1 + 1, mid - 2, room.m_r.m_r1 + 7, mid + 2 };
                    bool clash = false;
                    for ( const Room &o : m_rooms ) clash = clash || exit.hits( o.m_r, 2 );
                    for ( const Hall &o : m_halls ) clash = clash || exit.hits( o.m_r, 1 );
                    clash = clash || exit.hits( hall.m_r, 1 );
                    if ( m_rc.machine() )
                    {
                        // The tug lies in the open beside the hall, on the side away from the way in (never the
                        // exit's south).
                        int         side = dir == 2 ? 1 : dir;
                        const Rect &a = room.m_r;
                        Rect        berth = side == 1 ? Rect{ a.m_r0 + 1, a.m_c1 + 1, a.m_r1 - 1, a.m_c1 + 12 }
                                                      : Rect{ a.m_r0 + 1, a.m_c0 - 12, a.m_r1 - 1, a.m_c0 - 1 };
                        for ( const Room &o : m_rooms ) clash = clash || berth.hits( o.m_r, 1 );
                        for ( const Hall &o : m_halls ) clash = clash || berth.hits( o.m_r, 1 );
                        clash = clash || berth.hits( hall.m_r, 1 );
                        if ( !clash ) m_tug = berth, m_tugSide = side;
                    }
                    if ( clash ) continue;
                }
                room.m_kind = arena ? ARENA : MAIN;
                room.m_theme = arena ? m_rc.m_arenaTheme : pickTheme( m_rooms[cur].m_theme );
                room.m_in = (int)m_halls.size();
                hall.m_door =
                    width == 3 && ( arena || m_rc.m_power || m_rng.chance( 0.2f ) ) && !outdoor( room.m_theme );
                m_rooms.push_back( room );
                m_halls.push_back( hall );
                if ( arena ) m_exit = exit, m_hasExit = true, m_hasTug = m_rc.machine();
                prev = dir;
                cur = i;
                ok = true;
            }
            if ( !ok ) return false;
        }
        // Side rooms off the way: behind a door, loot and a champion.
        for ( int s = 0; s < m_rc.m_side; ++s )
            for ( int t = 0; t < 80; ++t )
            {
                int  from = m_rng.range( 1, m_rc.m_rooms - 2 );
                Room room;
                Hall hall;
                if ( !place( from, m_rng.range( 0, 3 ), m_rng.range( 7, 11 ), m_rng.range( 6, 9 ), m_rng.range( 2, 5 ),
                             3, room, hall ) )
                    continue;
                room.m_kind = SIDE;
                room.m_theme = m_rng.chance( 0.7f ) ? m_rooms[from].m_theme : pickTheme( "" );
                if ( !m_rc.m_outdoor.empty() ) room.m_theme = indoorTheme();   // out in the open: a hut with air
                room.m_in = (int)m_halls.size();
                hall.m_door = true;
                m_rooms.push_back( room );
                m_halls.push_back( hall );
                break;
            }
        return true;
    }

    /** @brief Moves everything so the map starts at (0, 0) with two cells of rock around. */
    void normalise()
    {
        Rect box = m_rooms[0].m_r.grown( 1 );
        auto add = [&]( const Rect &r )
        {
            box.m_r0 = std::min( box.m_r0, r.m_r0 ), box.m_c0 = std::min( box.m_c0, r.m_c0 );
            box.m_r1 = std::max( box.m_r1, r.m_r1 ), box.m_c1 = std::max( box.m_c1, r.m_c1 );
        };
        for ( const Room &r : m_rooms ) add( r.m_r.grown( 1 ) );
        for ( const Hall &h : m_halls ) add( h.m_r );
        if ( m_hasExit ) add( m_exit );
        if ( m_hasTug ) add( m_tug );
        int  dr = 2 - box.m_r0, dc = 2 - box.m_c0;
        auto shift = [&]( Rect &r ) { r.m_r0 += dr, r.m_r1 += dr, r.m_c0 += dc, r.m_c1 += dc; };
        for ( Room &r : m_rooms ) shift( r.m_r );
        for ( Hall &h : m_halls ) shift( h.m_r );
        shift( m_exit );
        shift( m_tug );
        m_rows = box.m_r1 + dr + 3;
        m_cols = box.m_c1 + dc + 3;
    }

    int key( int r, int c ) const { return r * 1000 + c; }
    bool free( int r, int c ) const { return m_g[r][c] == '.' && !m_reserved.count( key( r, c ) ); }

    /** @brief The cells inside a room by a corridor's end, `depth` deep and one wider on each side. */
    Rect mouth( const Hall &h, bool atTo, int depth ) const
    {
        int         d = atTo ? h.m_dir : ( h.m_dir + 2 ) % 4;
        const Rect &c = h.m_r;
        switch ( d )
        {
        case 0: return { c.m_r0 - depth, c.m_c0 - 1, c.m_r0 - 1, c.m_c1 + 1 };
        case 1: return { c.m_r0 - 1, c.m_c1 + 1, c.m_r1 + 1, c.m_c1 + depth };
        case 2: return { c.m_r1 + 1, c.m_c0 - 1, c.m_r1 + depth, c.m_c1 + 1 };
        default: return { c.m_r0 - 1, c.m_c0 - depth, c.m_r1 + 1, c.m_c0 - 1 };
        }
    }

    void carve()
    {
        // Rock all round, or open space: then rooms out in the open have no walls, huts get one, and corridors cross
        // the void as gratings.
        m_g.assign( m_rows, std::string( m_cols, m_rc.m_void ? '~' : '#' ) );
        auto fill = [&]( const Rect &r, char ch )
        {
            for ( int y = r.m_r0; y <= r.m_r1; ++y )
                for ( int x = r.m_c0; x <= r.m_c1; ++x ) m_g[y][x] = ch;
        };
        for ( const Room &r : m_rooms )
        {
            if ( m_rc.m_void && !outdoor( r.m_theme ) ) fill( r.m_r.grown( 1 ), '#' );
            fill( r.m_r, '.' );
        }
        // In rock the machine's berth is a breach in the hall's wall, open to what lies beyond.
        if ( m_hasTug && !m_rc.m_void ) fill( m_tug, '~' );
        for ( const Hall &h : m_halls )
        {
            for ( int y = h.m_r.m_r0; y <= h.m_r.m_r1; ++y )
                for ( int x = h.m_r.m_c0; x <= h.m_r.m_c1; ++x ) m_g[y][x] = m_g[y][x] == '~' ? '=' : '.';
            if ( !h.m_door ) continue;
            // The door sits in the wall of the room it leads into, walls on both sides of it.
            const Rect &c = h.m_r;
            int         wall = h.m_dir == 0 ? c.m_r0 : ( h.m_dir == 2 ? c.m_r1 : ( h.m_dir == 1 ? c.m_c1 : c.m_c0 ) );
            for ( int i = 0; i < 3; ++i )
            {
                int r = h.m_dir == 0 || h.m_dir == 2 ? wall : c.m_r0 + i;
                int col = h.m_dir == 0 || h.m_dir == 2 ? c.m_c0 + i : wall;
                m_g[r][col] = i == 1 ? '/' : '#';
            }
        }
        if ( m_hasExit )
        {
            // The way out: a corridor through the south wall, the gate, the freight lift in its well.
            const Rect &a = m_rooms[m_rc.m_rooms - 1].m_r;
            int         mid = ( a.m_c0 + a.m_c1 ) / 2;
            fill( { a.m_r1 + 1, mid - 1, a.m_r1 + 4, mid + 1 }, '.' );
            if ( m_rc.m_void ) fill( { a.m_r1 + 5, mid - 2, a.m_r1 + 7, mid + 2 }, '#' );   // a housing round the well
            fill( { a.m_r1 + 5, mid - 1, a.m_r1 + 6, mid + 1 }, '~' );
            m_g[a.m_r1 + 2][mid] = 'G';
            m_g[a.m_r1 + 6][mid] = 'X';
            m_g[a.m_r1 + 4][mid + 1] = 'R';
            for ( int r = a.m_r1 - 2; r <= a.m_r1 + 4; ++r )
                for ( int c = mid - 2; c <= mid + 2; ++c ) m_reserved.insert( key( r, c ) );
        }
        wilden();
        for ( const Hall &h : m_halls )
            for ( bool atTo : { false, true } )
            {
                Rect m = mouth( h, atTo, 3 );
                for ( int r = m.m_r0; r <= m.m_r1; ++r )
                    for ( int c = m.m_c0; c <= m.m_c1; ++c ) m_reserved.insert( key( r, c ) );
            }
    }

    /** @brief Whether a room is laid out as in the wild (a main room or the start of a genwild theme). */
    bool wild( const Room &room ) const
    {
        return ( room.m_kind == MAIN || room.m_kind == START ) && m_rc.m_wild.count( room.m_theme ) > 0;
    }

    /**
     * @brief Out in the wild the rooms lose their rectangles: rounded, their edges ragged by a slow noise; and the
     *        corridors between two of them swell and narrow (two to five cells). The corridors' mouths and the rooms'
     *        middles stay as they were (the route, the start and the capsules use them); a swelling never reaches
     *        anything open but its own corridor; what the shaping cuts off a room goes back to rock.
     */
    void wilden()
    {
        if ( m_rc.m_wild.empty() ) return;
        unsigned seed = m_rng.next();
        for ( size_t i = 0; i < m_rooms.size(); ++i )
        {
            if ( !wild( m_rooms[i] ) ) continue;
            const Rect   &a = m_rooms[i].m_r;
            std::set<int> keep;
            for ( const Hall &h : m_halls )
            {
                if ( h.m_from != (int)i && h.m_to != (int)i ) continue;
                Rect m = mouth( h, h.m_to == (int)i, 3 );
                for ( int r = m.m_r0; r <= m.m_r1; ++r )
                    for ( int c = m.m_c0; c <= m.m_c1; ++c ) keep.insert( key( r, c ) );
            }
            int mr = ( a.m_r0 + a.m_r1 ) / 2, mc = ( a.m_c0 + a.m_c1 ) / 2;
            for ( int r = mr - 1; r <= mr + 1; ++r )
                for ( int c = mc - 1; c <= mc + 1; ++c ) keep.insert( key( r, c ) );
            // A rounded rectangle (a superellipse), its rim moved in and out by the noise: the corners go, the
            // straight edges wander.
            float hy = a.height() * 0.5f, hx = a.width() * 0.5f;
            for ( int r = a.m_r0; r <= a.m_r1; ++r )
                for ( int c = a.m_c0; c <= a.m_c1; ++c )
                {
                    if ( keep.count( key( r, c ) ) || m_g[r][c] != '.' ) continue;
                    float u = fabsf( c + 0.5f - ( a.m_c0 + hx ) ) / hx, v = fabsf( r + 0.5f - ( a.m_r0 + hy ) ) / hy;
                    float rho = powf( powf( u, 2.6f ) + powf( v, 2.6f ), 1.0f / 2.6f );
                    float rim = 0.93f + 0.22f * noise2( c * 0.33f, r * 0.33f, seed ) +
                                0.08f * noise2( c * 0.9f, r * 0.9f, seed + 7u );
                    if ( rho > rim ) m_g[r][c] = '#';
                }
            // What no longer reaches the middle goes back to rock.
            std::set<int>   seen = { key( mr, mc ) };
            std::deque<int> q = { key( mr, mc ) };
            while ( !q.empty() )
            {
                int cur = q.front();
                q.pop_front();
                for ( int d = 0; d < 4; ++d )
                {
                    int nr = cur / 1000 + DR[d], nc = cur % 1000 + DC[d];
                    if ( nr < a.m_r0 || nr > a.m_r1 || nc < a.m_c0 || nc > a.m_c1 || m_g[nr][nc] != '.' ) continue;
                    if ( seen.insert( key( nr, nc ) ).second ) q.push_back( key( nr, nc ) );
                }
            }
            for ( int r = a.m_r0; r <= a.m_r1; ++r )
                for ( int c = a.m_c0; c <= a.m_c1; ++c )
                    if ( m_g[r][c] == '.' && !seen.count( key( r, c ) ) ) m_g[r][c] = '#';
        }
        for ( const Hall &h : m_halls )
        {
            if ( !wild( m_rooms[h.m_from] ) || !wild( m_rooms[h.m_to] ) ) continue;
            const Rect   &c = h.m_r;
            bool          along = h.m_dir == 0 || h.m_dir == 2;   // it runs north-south
            int           len = along ? c.height() : c.width();
            std::set<int> own;
            auto          ours = [&]( int r, int col )
            { return ( r >= c.m_r0 && r <= c.m_r1 && col >= c.m_c0 && col <= c.m_c1 ) || own.count( key( r, col ) ); };
            for ( int side : { -1, 1 } )
                for ( int t = 1; t + 1 < len; ++t )   // never at its ends, where the rooms' walls are
                {
                    float n = noise2( t * 0.3f, side * 7.0f + h.m_from * 3.1f + h.m_to * 1.7f, seed + 11u );
                    int   shift = n > 0.22f ? 1 : ( n < -0.3f ? -1 : 0 );
                    if ( shift == 0 ) continue;
                    int lo = along ? c.m_c0 : c.m_r0, hi = along ? c.m_c1 : c.m_r1;
                    int across = side < 0 ? ( shift > 0 ? lo - 1 : lo ) : ( shift > 0 ? hi + 1 : hi );
                    int r = along ? c.m_r0 + t : across, col = along ? across : c.m_c0 + t;
                    if ( shift > 0 )
                    {
                        // Swell into rock that touches nothing open but this corridor.
                        if ( m_g[r][col] != '#' ) continue;
                        bool alone = true;
                        for ( int dr = -1; dr <= 1 && alone; ++dr )
                            for ( int dc = -1; dc <= 1 && alone; ++dc )
                                alone = ours( r + dr, col + dc ) || m_g[r + dr][col + dc] == '#';
                        if ( !alone ) continue;
                        m_g[r][col] = '.';
                        own.insert( key( r, col ) );
                    }
                    else
                    {
                        // Narrow where it is three wide, to two.
                        int open = 0;
                        for ( int k = lo; k <= hi; ++k ) open += m_g[along ? r : k][along ? k : col] == '.';
                        if ( open >= 3 && m_g[r][col] == '.' ) m_g[r][col] = '#';
                    }
                }
        }
    }

    /**
     * @brief A rock standing in a wild room (or a mound, `ch` a height): a few cells grown together, never cutting the
     *        room.
     */
    void islet( const Rect &room, char ch = '#', int least = 3, int most = 6 )
    {
        Rect             at = anywhere( room, 1, 1 );
        int              n = m_rng.range( least, most );
        std::vector<int> cells = { key( at.m_r0, at.m_c0 ) };
        for ( int t = 0; t < 60 && (int)cells.size() < n; ++t )
        {
            int pick = cells[m_rng.next() % cells.size()], d = m_rng.range( 0, 3 );
            int r = pick / 1000 + DR[d], c = pick % 1000 + DC[d];
            if ( r <= room.m_r0 || r >= room.m_r1 || c <= room.m_c0 || c >= room.m_c1 ) continue;
            if ( std::find( cells.begin(), cells.end(), key( r, c ) ) == cells.end() ) cells.push_back( key( r, c ) );
        }
        for ( int k : cells )
            if ( !free( k / 1000, k % 1000 ) ) return;
        std::vector<std::string> saved = m_g;
        for ( int k : cells ) m_g[k / 1000][k % 1000] = ch;
        if ( !connected( room ) ) m_g = saved;
    }

    /** @brief Whether every walkable cell of the room reaches every other (steps of half a metre at most). */
    bool connected( const Rect &room ) const
    {
        auto walk = [&]( int r, int c )
        {
            char ch = m_g[r][c];
            return ch == '.' || ch == '=' || ( ch >= '1' && ch <= '9' );
        };
        auto height = [&]( int r, int c )
        {
            char ch = m_g[r][c];
            return ch >= '1' && ch <= '9' ? ch - '0' : 0;
        };
        int total = 0, sr = -1, sc = -1;
        for ( int r = room.m_r0; r <= room.m_r1; ++r )
            for ( int c = room.m_c0; c <= room.m_c1; ++c )
                if ( walk( r, c ) )
                {
                    ++total;
                    if ( sr < 0 ) sr = r, sc = c;
                }
        if ( sr < 0 ) return false;
        std::set<int>   seen = { key( sr, sc ) };
        std::deque<int> q = { key( sr, sc ) };
        while ( !q.empty() )
        {
            int cur = q.front();
            q.pop_front();
            int r = cur / 1000, c = cur % 1000;
            for ( int d = 0; d < 4; ++d )
            {
                int nr = r + DR[d], nc = c + DC[d];
                if ( nr < room.m_r0 || nr > room.m_r1 || nc < room.m_c0 || nc > room.m_c1 ) continue;
                if ( !walk( nr, nc ) || abs( height( nr, nc ) - height( r, c ) ) > 1 ) continue;
                if ( !seen.insert( key( nr, nc ) ).second ) continue;
                q.push_back( key( nr, nc ) );
            }
        }
        return (int)seen.size() == total;
    }

    /** @brief Fills a rectangle of free cells with `ch` (and its inside with `inner`) unless that cuts the room. */
    bool block( const Rect &room, const Rect &f, char ch, char inner = 0 )
    {
        if ( f.m_r0 < room.m_r0 || f.m_c0 < room.m_c0 || f.m_r1 > room.m_r1 || f.m_c1 > room.m_c1 ) return false;
        for ( int r = f.m_r0; r <= f.m_r1; ++r )
            for ( int c = f.m_c0; c <= f.m_c1; ++c )
                if ( !free( r, c ) ) return false;
        std::vector<std::string> saved = m_g;
        for ( int r = f.m_r0; r <= f.m_r1; ++r )
            for ( int c = f.m_c0; c <= f.m_c1; ++c )
            {
                bool edge = r == f.m_r0 || r == f.m_r1 || c == f.m_c0 || c == f.m_c1;
                m_g[r][c] = inner && !edge ? inner : ch;
            }
        if ( connected( room ) ) return true;
        m_g = saved;
        return false;
    }

    /** @brief A random rectangle of the given size inside the room, a cell off its walls. */
    Rect anywhere( const Rect &room, int h, int w )
    {
        int r = m_rng.range( room.m_r0 + 1, std::max( room.m_r0 + 1, room.m_r1 - h ) );
        int c = m_rng.range( room.m_c0 + 1, std::max( room.m_c0 + 1, room.m_c1 - w ) );
        return { r, c, r + h - 1, c + w - 1 };
    }

    /** @brief Cover, pits, stalls, shelves, stacks: what the room's theme puts in it. */
    void dress( int i )
    {
        const Room        &room = m_rooms[i];
        const Rect        &r = room.m_r;
        const std::string &t = room.m_theme;
        bool               big = r.width() >= 12 && r.height() >= 10;
        if ( room.m_kind == START ) return;
        if ( room.m_kind == ARENA )
        {
            // A vault's hall: racks of cores along the long walls (the fight goes on among them).
            if ( t == "vault" )
                for ( int y : { r.m_r0 + 1, r.m_r1 - 1 } )
                    for ( int x = r.m_c0 + 2; x <= r.m_c1 - 4; x += 5 ) block( r, { y, x, y, x + 2 }, '|' );
            // Four blocks to take cover behind, symmetric; the middle stays open.
            int qr = r.height() / 4, qc = r.width() / 4;
            for ( int sr : { -1, 1 } )
                for ( int sc : { -1, 1 } )
                {
                    int cr = ( r.m_r0 + r.m_r1 ) / 2 + sr * qr, cc = ( r.m_c0 + r.m_c1 ) / 2 + sc * qc;
                    block( r, { cr, cc, cr + 1, cc + 1 }, 'F' );
                }
            return;
        }
        if ( t == "vault" )
        {
            // Rows of racks of cores with aisles between them, like shelves in a library of the dead.
            for ( int y = r.m_r0 + 2; y <= r.m_r1 - 2; y += 3 )
                for ( int x = r.m_c0 + 2; x <= r.m_c1 - 3; )
                {
                    int len = m_rng.range( 3, 6 );
                    block( r, { y, x, y, std::min( x + len - 1, r.m_c1 - 2 ) }, '|' );
                    x += len + 2;
                }
            return;
        }
        if ( t == "alien" )
        {
            // Terraces in a geometry made for nobody: raised steps and slabs at odd places.
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k )
                block( r, anywhere( r, m_rng.range( 2, 4 ), m_rng.range( 2, 5 ) ), m_rng.chance( 0.5f ) ? '1' : '2' );
            for ( int k = m_rng.range( 1, 2 ); k > 0; --k ) block( r, anywhere( r, 1, m_rng.range( 2, 3 ) ), '#' );
            return;
        }
        if ( t == "park" )
        {
            // Gardens: hedges (in the wild rocks grown together), a raised lawn, benches along a path.
            for ( int k = m_rng.range( 2, 4 ); k > 0; --k )
                if ( wild( room ) )
                    islet( r );
                else
                    block( r, anywhere( r, 1, m_rng.range( 2, 4 ) ), '#' );
            if ( big && m_rng.chance( 0.6f ) )
            {
                if ( wild( room ) )
                    islet( r, '1', 7, 13 );   // a mound grown as the rocks are
                else
                    block( r, anywhere( r, m_rng.range( 3, 4 ), m_rng.range( 3, 5 ) ), '1' );
            }
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k ) block( r, anywhere( r, 1, 1 ), '+' );
            return;
        }
        if ( t == "clinic" || t == "hall" || t == "server" || t == "cryo" || t == "net" || t == "prison" )
        {
            // Rows: beds, benches, racks of computers, desks, bunks; aisles between them.
            int len0 = t == "hall" ? 4 : 2, len1 = t == "hall" ? 7 : 4;
            for ( int y = r.m_r0 + 2; y <= r.m_r1 - 2; y += t == "hall" ? 2 : 3 )
                for ( int x = r.m_c0 + 2; x <= r.m_c1 - 3; )
                {
                    int len = m_rng.range( len0, len1 );
                    block( r, { y, x, y, std::min( x + len - 1, r.m_c1 - 2 ) }, '+' );
                    x += len + 2;
                }
            if ( t == "cryo" || t == "prison" )
                for ( int k = m_rng.range( 1, 2 ); k > 0; --k ) block( r, anywhere( r, 2, 2 ), '#' );
            return;
        }
        if ( t == "arena" )
        {
            // Sand, a few pillars; the middle stays open.
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k ) block( r, anywhere( r, 1, 1 ), '#' );
            return;
        }
        if ( t == "yard" || t == "container" )
        {
            // Holes in the plating (space shows through), stacks, a generator to hide behind.
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k ) block( r, anywhere( r, 2, m_rng.range( 2, 3 ) ), '~' );
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k ) block( r, anywhere( r, 2, 2 ), '#' );
            if ( m_rng.chance( 0.5f ) ) block( r, anywhere( r, 2, 3 ), 'F' );
            return;
        }
        if ( t == "furnace" )
        {
            // Channels of molten metal across (the level's below shows them), crucibles to hide behind.
            for ( int k = m_rng.range( 1, 2 ); k > 0; --k ) block( r, anywhere( r, 1, m_rng.range( 3, 5 ) ), '~' );
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k ) block( r, anywhere( r, 2, 2 ), 'F' );
            return;
        }
        if ( t == "ice" || t == "dusk" || t == "snow" || t == "moon" )
        {
            // Out in the open: ridges of pressure ice, boulders, black rock; mounds one can stand on.
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k )
                block( r, m_rng.chance( 0.5f ) ? anywhere( r, 2, 2 ) : anywhere( r, 1, m_rng.range( 2, 4 ) ), '#' );
            if ( big && m_rng.chance( 0.5f ) ) block( r, anywhere( r, m_rng.range( 3, 4 ), m_rng.range( 3, 5 ) ), '1' );
            return;
        }
        if ( t == "pods" || t == "hold" )
        {
            // Rows of sleepers' pods (crates of cores in a hold), waist high, with aisles between them.
            for ( int y = r.m_r0 + 2; y <= r.m_r1 - 2; y += 3 )
                for ( int x = r.m_c0 + 2; x <= r.m_c1 - 3; )
                {
                    int len = m_rng.range( 2, 5 );
                    block( r, { y, x, y, std::min( x + len - 1, r.m_c1 - 2 ) }, '+' );
                    x += len + 2;
                }
            if ( t == "hold" )
                for ( int k = m_rng.range( 1, 2 ); k > 0; --k ) block( r, anywhere( r, 2, 3 ), '#' );
            return;
        }
        if ( t == "deck" || t == "station" )
        {
            // Holes in the plating (the depths show through), housings, a heat sink.
            for ( int k = m_rng.range( 1, 2 ); k > 0; --k ) block( r, anywhere( r, 2, m_rng.range( 2, 3 ) ), '~' );
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k ) block( r, anywhere( r, 2, 2 ), '#' );
            if ( m_rng.chance( 0.6f ) ) block( r, anywhere( r, 2, 4 ), t == "station" ? '+' : 'F' );
            return;
        }
        if ( t == "hull" )
        {
            // Out on the hull: holes in the plating (the stars show through), equipment housings, radiators.
            for ( int k = m_rng.range( 1, 2 ); k > 0; --k ) block( r, anywhere( r, 2, m_rng.range( 2, 3 ) ), '~' );
            for ( int k = m_rng.range( 2, 3 ); k > 0; --k )
                block( r, m_rng.chance( 0.5f ) ? anywhere( r, 2, 2 ) : anywhere( r, 1, 3 ), '#' );
            if ( m_rng.chance( 0.6f ) ) block( r, anywhere( r, 2, 4 ), 'F' );
            return;
        }
        if ( t == "shaft" && r.width() >= 11 && r.height() >= 9 )
        {
            // A shaft in the middle, a walk round it along the walls, a grating bridge across.
            Rect pit = { r.m_r0 + 3, r.m_c0 + 3, r.m_r1 - 3, r.m_c1 - 3 };
            for ( int y = pit.m_r0; y <= pit.m_r1; ++y )
                for ( int x = pit.m_c0; x <= pit.m_c1; ++x ) m_g[y][x] = '~';
            bool across = room.m_in >= 0 && ( m_halls[room.m_in].m_dir == 1 || m_halls[room.m_in].m_dir == 3 );
            int  mr = ( pit.m_r0 + pit.m_r1 ) / 2, mc = ( pit.m_c0 + pit.m_c1 ) / 2;
            if ( across )
                for ( int x = pit.m_c0; x <= pit.m_c1; ++x )
                    m_g[mr][x] = m_g[mr + 1 <= pit.m_r1 ? mr + 1 : mr][x] = '=';
            else
                for ( int y = pit.m_r0; y <= pit.m_r1; ++y )
                    m_g[y][mc] = m_g[y][mc + 1 <= pit.m_c1 ? mc + 1 : mc] = '=';
            return;
        }
        if ( t == "exchanger" )
        {
            for ( int y = r.m_r0 + 2, row = 0; y + 1 <= r.m_r1 - 2; y += 5, ++row )
                for ( int x = r.m_c0 + 2 + ( row % 2 ) * 3; x + 3 <= r.m_c1 - 2; x += 7 )
                    block( r, { y, x, y + 1, x + 3 }, 'F' );
        }
        else if ( t == "works" )
        {
            for ( int k = m_rng.range( 2, 4 ); k > 0; --k ) block( r, anywhere( r, 2, m_rng.range( 2, 3 ) ), 'F' );
        }
        else if ( t == "street" )
        {
            // Market stalls in rows (waist high: one shoots over them), a few against the walls.
            for ( int y = r.m_r0 + 3; y <= r.m_r1 - 3; y += 4 )
                for ( int x = r.m_c0 + 2 + m_rng.range( 0, 2 ); x <= r.m_c1 - 3; )
                {
                    int len = m_rng.range( 2, 4 );
                    block( r, { y, x, y, std::min( x + len - 1, r.m_c1 - 2 ) }, '+' );
                    x += len + m_rng.range( 2, 3 );
                }
            for ( int k = m_rng.range( 2, 4 ); k > 0; --k )
            {
                int y = 0, x = 0;
                if ( wallCell( r, y, x ) ) m_g[y][x] = 'M';
            }
            // Market folk who did not get away, crouched against the walls.
            for ( int k = m_rng.range( 0, 2 ); k > 0; --k )
            {
                int y = 0, x = 0;
                if ( wallCell( r, y, x ) ) m_g[y][x] = 'N';
            }
        }
        else if ( t == "coldstore" )
        {
            for ( int y = r.m_r0 + 2; y <= r.m_r1 - 2; y += 3 )
                for ( int x = r.m_c0 + 2; x <= r.m_c1 - 3; )
                {
                    int len = m_rng.range( 3, 5 );
                    block( r, { y, x, y, std::min( x + len - 1, r.m_c1 - 2 ) }, '#' );
                    x += len + 2;
                }
        }
        else if ( t == "cargo" )
        {
            for ( int k = m_rng.range( 3, 5 ); k > 0; --k )
                block( r, m_rng.chance( 0.5f ) ? anywhere( r, 2, 3 ) : anywhere( r, 3, 2 ), '#' );
        }
        else
        {
            for ( int k = m_rng.range( 1, 3 ); k > 0; --k )
                block( r, anywhere( r, m_rng.range( 1, 2 ), m_rng.range( 1, 2 ) ), '#' );
        }
        // Now and then a raised floor with a step round it; in the larger rooms indoors a walkway along a wall.
        if ( big && t != "street" && m_rng.chance( 0.3f ) )
            block( r, anywhere( r, m_rng.range( 4, 5 ), m_rng.range( 4, 6 ) ), '1', '2' );
        if ( r.width() >= 9 && r.height() >= 8 && !outdoor( t ) && t != "street" && room.m_kind == MAIN &&
             m_rng.chance( 0.45f ) )
            ledge( r );
    }

    /**
     * @brief A walkway raised along a long wall: a metre and a half up, two steps at one end; at the other, someone
     *        who shoots from up there (if the level's packs have shooters).
     * @param[in] r  the room
     */
    void ledge( const Rect &r )
    {
        for ( int tries = 0; tries < 8; ++tries )
        {
            int  side = m_rng.range( 0, 3 );   // 0 north, 1 east, 2 south, 3 west
            bool alongX = side == 0 || side == 2;
            int  len = std::min( 7, ( alongX ? r.width() : r.height() ) - 4 );
            if ( len < 5 ) return;
            int from =
                alongX ? m_rng.range( r.m_c0 + 2, r.m_c1 - 1 - len ) : m_rng.range( r.m_r0 + 2, r.m_r1 - 1 - len );
            std::vector<std::pair<int, int>> cells;
            for ( int k = 0; k < len; ++k )
                cells.push_back( alongX ? std::pair<int, int>{ side == 0 ? r.m_r0 : r.m_r1, from + k }
                                        : std::pair<int, int>{ from + k, side == 3 ? r.m_c0 : r.m_c1 } );
            bool ok = true;
            for ( auto [y, x] : cells )
            {
                ok = ok && free( y, x );
                for ( int d = 0; d < 4; ++d ) ok = ok && m_g[y + DR[d]][x + DC[d]] != '/';
            }
            if ( !ok ) continue;
            std::vector<std::string> saved = m_g;
            bool                     flip = m_rng.chance( 0.5f );   // the steps at the start or at the end
            for ( int k = 0; k < len; ++k )
            {
                int step = flip ? len - 1 - k : k;
                m_g[cells[k].first][cells[k].second] = step == 0 ? '1' : ( step == 1 ? '2' : '3' );
            }
            if ( !connected( r ) )
            {
                m_g = saved;
                continue;
            }
            // Up there, at the far end, one who shoots (a level's own shooters: carbines, shock rifles, rotary guns).
            char shooter = 0;
            for ( const Pack &p : m_rc.m_packs )
                for ( char ch : p.m_letters )
                    if ( !shooter && ( ch == 'T' || ch == 'U' || ch == '<' || ch == '%' ) ) shooter = ch;
            if ( shooter )
            {
                auto [y, x] = cells[flip ? 0 : len - 1];
                m_g[y][x] = shooter;
                m_foes.insert( key( y, x ) );
                m_extra.push_back( "raise " + std::to_string( y ) + " " + std::to_string( x ) + " 1.5" );
            }
            return;
        }
    }

    /** @return the cell lies in or on the wall of a room of this kind */
    bool inRoom( int y, int x, int kind ) const
    {
        for ( const Room &r : m_rooms )
            if ( r.m_kind == kind && y >= r.m_r.m_r0 - 1 && y <= r.m_r.m_r1 + 1 && x >= r.m_r.m_c0 - 1 &&
                 x <= r.m_r.m_c1 + 1 )
                return true;
        return false;
    }

    /** @brief A free cell against a wall of the room (not by a door); false if none was found. */
    bool wallCell( const Rect &room, int &orow, int &ocol )
    {
        for ( int t = 0; t < 60; ++t )
        {
            int r = m_rng.range( room.m_r0, room.m_r1 ), c = m_rng.range( room.m_c0, room.m_c1 );
            if ( !free( r, c ) ) continue;
            bool wall = false, door = false;
            for ( int d = 0; d < 4; ++d )
            {
                char n = m_g[r + DR[d]][c + DC[d]];
                wall = wall || n == '#';
                door = door || n == '/';
            }
            if ( wall && !door )
            {
                orow = r, ocol = c;
                return true;
            }
        }
        return false;
    }

    /** @brief The free cell of the rectangle nearest to (r, c) that `ok` accepts. */
    template <typename F>
    bool nearest( const Rect &in, float r, float c, int &orow, int &ocol, F ok ) const
    {
        float best = 1e9f;
        for ( int y = in.m_r0; y <= in.m_r1; ++y )
            for ( int x = in.m_c0; x <= in.m_c1; ++x )
            {
                if ( !free( y, x ) || !ok( y, x ) ) continue;
                float d = ( y - r ) * ( y - r ) + ( x - c ) * ( x - c );
                if ( d < best ) best = d, orow = y, ocol = x;
            }
        return best < 1e8f;
    }

    /** @brief An enemy pack: the leader in a free cell, the others round it, a cell apart. */
    void pack( const Rect &room, const std::string &letters, float cr, float cc, bool elite, bool boss = false )
    {
        int r0 = 0, c0 = 0;
        if ( !nearest( room, cr, cc, r0, c0, [&]( int y, int x ) { return apart( y, x ); } ) ) return;
        std::deque<int> q = { key( r0, c0 ) };
        std::set<int>   seen = { key( r0, c0 ) };
        size_t          placed = 0;
        while ( !q.empty() && placed < letters.size() )
        {
            int cur = q.front();
            q.pop_front();
            int r = cur / 1000, c = cur % 1000;
            if ( free( r, c ) && apart( r, c ) )
            {
                // A digit is one of the place's own creatures ("natives" in the level file names them): a line, not
                // a letter in the map.
                if ( letters[placed] >= '1' && letters[placed] <= '9' )
                {
                    m_reserved.insert( cur );
                    m_extra.push_back( "native " + std::to_string( r ) + " " + std::to_string( c ) + " " +
                                       std::string( 1, letters[placed] ) );
                }
                else
                    m_g[r][c] = letters[placed];
                m_foes.insert( cur );
                if ( placed == 0 && ( elite || boss ) )
                    m_extra.push_back( "elite " + std::to_string( r ) + " " + std::to_string( c ) +
                                       ( boss ? " boss" : "" ) );
                ++placed;
            }
            for ( int d = 0; d < 4; ++d )
            {
                int nr = r + DR[d], nc = c + DC[d];
                if ( nr < room.m_r0 || nr > room.m_r1 || nc < room.m_c0 || nc > room.m_c1 ) continue;
                if ( m_g[nr][nc] != '.' && !m_foes.count( key( nr, nc ) ) ) continue;
                if ( seen.insert( key( nr, nc ) ).second ) q.push_back( key( nr, nc ) );
            }
        }
    }

    bool apart( int r, int c ) const
    {
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
                if ( m_foes.count( key( r + dr, c + dc ) ) ) return false;
        return true;
    }

    const Pack &somePack()
    {
        float total = 0.0f;
        for ( const Pack &p : m_rc.m_packs ) total += p.m_weight;
        float pick = m_rng.f() * total;
        for ( const Pack &p : m_rc.m_packs )
        {
            pick -= p.m_weight;
            if ( pick <= 0.0f ) return p;
        }
        return m_rc.m_packs.back();
    }

    void put( const Rect &room, char what )
    {
        for ( int t = 0; t < 40; ++t )
        {
            int r = m_rng.range( room.m_r0, room.m_r1 ), c = m_rng.range( room.m_c0, room.m_c1 );
            if ( free( r, c ) )
            {
                m_g[r][c] = what;
                return;
            }
        }
    }

    void markHall( const Hall &h, char letter )
    {
        for ( int r = h.m_r.m_r0; r <= h.m_r.m_r1; ++r )
            for ( int c = h.m_r.m_c0; c <= h.m_r.m_c1; ++c )
                if ( m_g[r][c] == '.' || m_g[r][c] == '=' ) m_g[r][c] = letter;
    }

    std::string cellText( int r, int c ) const { return std::to_string( r ) + " " + std::to_string( c ); }

    void populate()
    {
        int n = m_rc.m_rooms;
        // Triggers in the corridors on the way (the lines and hints use them), before anything stands there.
        auto mainHall = [&]( int i ) -> const Hall & { return m_halls[std::min( n - 2, i )]; };
        markHall( m_halls[0], 'b' );
        markHall( mainHall( std::max( 1, n / 3 - 1 ) ), 'c' );
        markHall( mainHall( std::max( 2, n / 2 - 1 ) ), 'e' );
        markHall( mainHall( std::max( 3, 2 * n / 3 - 1 ) ), 'd' );
        markHall( m_halls[n - 2], 'g' );

        // The start, with its trigger round it.
        const Rect &s = m_rooms[0].m_r;
        int         pr = 0, pc = 0;
        nearest( s, ( s.m_r0 + s.m_r1 ) * 0.5f, ( s.m_c0 + s.m_c1 ) * 0.5f, pr, pc, []( int, int ) { return true; } );
        m_g[pr][pc] = 'P';
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
                if ( m_g[pr + dr][pc + dc] == '.' ) m_g[pr + dr][pc + dc] = 'a';

        // Rescue capsules along the way; the last one just before the set piece.
        for ( int at : { n / 3, 2 * n / 3 } )
        {
            const Rect m = mouth( m_halls[m_rooms[at].m_in], true, 1 );
            int        r = 0, c = 0;
            if ( nearest( m_rooms[at].m_r, ( m.m_r0 + m.m_r1 ) * 0.5f, ( m.m_c0 + m.m_c1 ) * 0.5f, r, c,
                          []( int, int ) { return true; } ) )
                m_g[r][c] = 'C';
        }
        {
            const Hall &last = m_halls[m_rooms[n - 1].m_in];
            const Rect  m = mouth( last, false, 1 );
            int         r = 0, c = 0;
            if ( nearest( m_rooms[last.m_from].m_r, ( m.m_r0 + m.m_r1 ) * 0.5f, ( m.m_c0 + m.m_c1 ) * 0.5f, r, c,
                          []( int, int ) { return true; } ) )
                m_g[r][c] = 'C';
        }

        // The set piece: the boss away from the way in.
        {
            const Room &a = m_rooms[n - 1];
            const Rect &r = a.m_r;
            int         dir = m_halls[a.m_in].m_dir;
            float       br = ( r.m_r0 + r.m_r1 ) * 0.5f + DR[dir] * r.height() * 0.25f;
            float       bc = ( r.m_c0 + r.m_c1 ) * 0.5f + DC[dir] * r.width() * 0.25f;
            if ( m_hasTug )
            {
                // The tug's berth: by the middle of the hall's edge on its side; a boarding party on the deck.
                int er = ( r.m_r0 + r.m_r1 ) / 2, ec = m_tugSide == 1 ? r.m_c1 : r.m_c0;
                m_extra.push_back( "tug " + cellText( er, ec ) + " " + std::to_string( m_tugSide ) );
                m_extra.push_back( "machine " + m_rc.m_arena );
                // Two air posts on the deck: the fight is long.
                for ( float f : { 0.3f, 0.7f } )
                {
                    int y = 0, x = 0;
                    if ( nearest( r, r.m_r0 + ( r.m_r1 - r.m_r0 ) * f, ( r.m_c0 + r.m_c1 ) * 0.5f, y, x,
                                  []( int, int ) { return true; } ) )
                        m_g[y][x] = ';';
                }
                pack( r, somePack().m_letters, ( r.m_r0 + r.m_r1 ) * 0.5f, ( r.m_c0 + r.m_c1 ) * 0.5f, false );
            }
            else if ( m_rc.m_arena == "mother" )
            {
                // Mother Sevenfold: three relays round her hall, her seven bodies in a ring at its middle.
                const float spots[3][2] = { { 0.25f, 0.2f }, { 0.25f, 0.8f }, { 0.8f, 0.5f } };
                for ( const auto &sp : spots )
                {
                    int y = 0, x = 0;
                    // In the open: every cell round it free, so it can be reached from all sides.
                    auto open = [&]( int yy, int xx )
                    {
                        for ( int dy = -1; dy <= 1; ++dy )
                            for ( int dx = -1; dx <= 1; ++dx )
                                if ( m_g[yy + dy][xx + dx] != '.' ) return false;
                        return true;
                    };
                    if ( !nearest( r, r.m_r0 + r.height() * sp[0], r.m_c0 + r.width() * sp[1], y, x, open ) ) continue;
                    m_extra.push_back( "relay " + cellText( y, x ) );
                    m_reserved.insert( key( y, x ) );
                }
                float cr = ( r.m_r0 + r.m_r1 ) * 0.5f, cc = ( r.m_c0 + r.m_c1 ) * 0.5f;
                for ( int k = 0; k < 7; ++k )
                {
                    float ang = k * 2.0f * 3.14159f / 7.0f;
                    int   y = 0, x = 0;
                    if ( !nearest( r, cr + sinf( ang ) * 3.0f, cc + cosf( ang ) * 3.5f, y, x,
                                   [&]( int yy, int xx ) { return apart( yy, xx ); } ) )
                        continue;
                    m_extra.push_back( "mother " + cellText( y, x ) );
                    m_foes.insert( key( y, x ) );
                    m_reserved.insert( key( y, x ) );
                }
            }
            else if ( m_rc.m_arena == "hydraulic" )
            {
                // The Hydrauliker in his flooded hall: two balls of water to fight round.
                int y = 0, x = 0;
                if ( nearest( r, br, bc, y, x, []( int, int ) { return true; } ) )
                    m_g[y][x] = '>', m_foes.insert( key( y, x ) );
                for ( float f : { 0.3f, 0.7f } )
                    m_extra.push_back(
                        "water " +
                        cellText( r.m_r0 + (int)( r.height() * f ), r.m_c0 + (int)( r.width() * ( 1.0f - f ) ) ) +
                        " 3.6" );
            }
            else if ( m_rc.m_arena == "taxator" )
            {
                int y = 0, x = 0;
                if ( nearest( r, br, bc, y, x, []( int, int ) { return true; } ) )
                    m_g[y][x] = '@', m_foes.insert( key( y, x ) );
            }
            else
            {
                std::string guard( 1, m_rc.m_guard[m_rng.next() % m_rc.m_guard.size()] );
                pack( r, guard, br, bc, false, true );
                for ( int k = 0; k < 2; ++k )
                    pack( r, somePack().m_letters, m_rng.range( r.m_r0, r.m_r1 ), m_rng.range( r.m_c0, r.m_c1 ),
                          false );
            }
        }

        // The rooms: packs (some led by a champion), lockers, pickups, lamps, scenery.
        size_t note = 0;
        for ( size_t i = 1; i < m_rooms.size(); ++i )
        {
            const Room &room = m_rooms[i];
            const Rect &r = room.m_r;
            if ( room.m_kind == ARENA ) continue;
            const Rect in = mouth( m_halls[room.m_in], true, 1 );
            float      ir = ( in.m_r0 + in.m_r1 ) * 0.5f, ic = ( in.m_c0 + in.m_c1 ) * 0.5f;
            int packs = std::max( 1, (int)( r.width() * r.height() * m_rc.m_density / 100.0f + m_rng.f() * 0.8f ) );
            if ( room.m_kind == SIDE ) packs = 1;
            for ( int k = 0; k < packs; ++k )
            {
                // Somewhere not right at the door one came in by.
                float cr = ir, cc = ic;
                for ( int t = 0; t < 20 && fabsf( cr - ir ) + fabsf( cc - ic ) < 5.0f; ++t )
                    cr = (float)m_rng.range( r.m_r0, r.m_r1 ), cc = (float)m_rng.range( r.m_c0, r.m_c1 );
                bool elite = room.m_kind == SIDE || m_rng.chance( m_rc.m_elite );
                pack( r, somePack().m_letters, cr, cc, elite );
            }
            int lockers = room.m_kind == SIDE ? m_rng.range( 2, 3 ) : ( m_rng.chance( 0.3f ) ? 1 : 0 );
            for ( int k = 0; k < lockers; ++k )
            {
                int y = 0, x = 0;
                if ( wallCell( r, y, x ) ) m_g[y][x] = '&';
            }
            if ( note < m_rc.m_notes.size() && ( room.m_kind == SIDE || m_rng.chance( 0.25f ) ) )
            {
                int y = 0, x = 0;
                if ( wallCell( r, y, x ) )
                {
                    m_g[y][x] = '!';
                    m_extra.push_back( "note " + cellText( y, x ) + " " + m_rc.m_notes[note++] );
                }
            }
            put( r, 'S' );
            if ( m_rng.chance( 0.5f ) ) put( r, 'S' );
            if ( m_rng.chance( 0.35f ) ) put( r, 'H' );
            if ( room.m_kind == SIDE && m_rng.chance( 0.25f ) )
            {
                int y = 0, x = 0;
                if ( wallCell( r, y, x ) ) m_g[y][x] = 'K';
            }
            if ( ( room.m_theme == "works" || room.m_theme == "exchanger" ) && m_rng.chance( 0.4f ) ) put( r, 'V' );
            if ( outdoor( room.m_theme ) && m_rng.chance( m_rc.m_oxygen ) ) put( r, ':' );
            if ( room.m_kind != START && m_rng.chance( m_rc.m_water ) && r.width() > 6 && r.height() > 6 )
            {
                // A ball of water floating over the floor, away from the walls.
                int   y = m_rng.range( r.m_r0 + 3, r.m_r1 - 3 ), x = m_rng.range( r.m_c0 + 3, r.m_c1 - 3 );
                float radius = 2.5f + m_rng.f() * 1.6f;
                m_extra.push_back( "water " + cellText( y, x ) + " " + std::to_string( radius ).substr( 0, 4 ) );
            }
            if ( outdoor( room.m_theme ) && m_rng.chance( m_rc.m_oxygen * 0.5f ) ) put( r, ';' );
            if ( outdoor( room.m_theme ) && m_rng.chance( m_rc.m_oxygen * 0.4f ) ) put( r, ':' );
            if ( room.m_theme == "exchanger" && m_rng.chance( 0.2f ) ) put( r, 'D' );
            // Things by name: a free cell each, kept free (marked until the map is written).
            for ( const auto &th : m_rc.m_things )
            {
                bool here =
                    th.m_where == "all" || ( th.m_where == "side" && room.m_kind == SIDE ) ||
                    ( th.m_where == "arena" && room.m_kind == ARENA ) ||
                    ( th.m_where == "main" && room.m_kind != SIDE && room.m_kind != ARENA && room.m_kind != START );
                if ( !here ) continue;
                int count = (int)th.m_rate + ( m_rng.chance( th.m_rate - (int)th.m_rate ) ? 1 : 0 );
                for ( int k = 0; k < count; ++k )
                    for ( int t = 0; t < 40; ++t )
                    {
                        int y = m_rng.range( r.m_r0 + 1, r.m_r1 - 1 ), x = m_rng.range( r.m_c0 + 1, r.m_c1 - 1 );
                        if ( !free( y, x ) ) continue;
                        m_g[y][x] = '\\';
                        m_extra.push_back( "thing " + cellText( y, x ) + " " + th.m_name );
                        break;
                    }
            }
            // The place's own things: bore holes in the ice, geysers, coils, power cells (in the side rooms).
            for ( const auto &f : m_rc.m_features )
            {
                if ( f.first == '0' && room.m_kind != SIDE ) continue;
                int count = (int)f.second + ( m_rng.chance( f.second - (int)f.second ) ? 1 : 0 );
                for ( int k = 0; k < count; ++k ) put( r, f.first );
            }
            // Guards hanging under balloons over the abyss beside the room.
            if ( m_rc.m_hover && room.m_kind != SIDE )
            {
                int count =
                    (int)m_rc.m_hoverRate + ( m_rng.chance( m_rc.m_hoverRate - (int)m_rc.m_hoverRate ) ? 1 : 0 );
                for ( int k = 0; k < count; ++k )
                    for ( int t = 0; t < 40; ++t )
                    {
                        int y = m_rng.range( r.m_r0 - 3, r.m_r1 + 3 ), x = m_rng.range( r.m_c0 - 3, r.m_c1 + 3 );
                        // Two cells to every side are looked at below: so far from the edge at least.
                        if ( y < 2 || x < 2 || y >= m_rows - 2 || x >= m_cols - 2 || m_g[y][x] != '~' ) continue;
                        bool beside = false;
                        for ( int dy = -2; dy <= 2; ++dy )
                            for ( int dx = -2; dx <= 2; ++dx ) beside = beside || m_g[y + dy][x + dx] == '.';
                        if ( !beside ) continue;
                        m_g[y][x] = m_rc.m_hover;
                        break;
                    }
            }
        }
        // The ark: every door on the way stays shut until power is routed to it.
        if ( m_rc.m_power )
        {
            int k = 0;
            for ( int y = 0; y < m_rows; ++y )
                for ( int x = 0; x < m_cols; ++x )
                    if ( m_g[y][x] == '/' && !inRoom( y, x, SIDE ) )
                        m_extra.push_back( "lock " + cellText( y, x ) + " power" + std::to_string( k++ ) );
        }
        put( m_rooms[0].m_r, 'H' );
        // Corridors on the way: now and then one or two waiting in a long one.
        for ( int h = 1; h < n - 1; ++h )
        {
            const Hall &hall = m_halls[h];
            int         len = std::max( hall.m_r.width(), hall.m_r.height() );
            if ( len < 6 || !m_rng.chance( 0.35f ) ) continue;
            std::string who = somePack().m_letters.substr( 0, m_rng.range( 1, 2 ) );
            float       cr = ( hall.m_r.m_r0 + hall.m_r.m_r1 ) * 0.5f, cc = ( hall.m_r.m_c0 + hall.m_r.m_c1 ) * 0.5f;
            for ( size_t k = 0; k < who.size(); ++k )
            {
                int   y = 0, x = 0;
                auto  ok = [&]( int yy, int xx ) { return apart( yy, xx ); };
                Rect  area = hall.m_r;
                bool  found = false;
                float best = 1e9f;
                for ( int yy = area.m_r0; yy <= area.m_r1; ++yy )
                    for ( int xx = area.m_c0; xx <= area.m_c1; ++xx )
                    {
                        char ch = m_g[yy][xx];
                        if ( !( ch >= 'a' && ch <= 'z' ) && ch != '.' ) continue;
                        if ( !ok( yy, xx ) ) continue;
                        float d = ( yy - cr ) * ( yy - cr ) + ( xx - cc ) * ( xx - cc );
                        if ( d < best ) best = d, y = yy, x = xx, found = true;
                    }
                if ( !found ) break;
                // A digit is one of the place's own creatures: a line, as in the rooms (in the map it would be a
                // raised floor, a metre high in a corridor, and nobody gets past it).
                if ( who[k] >= '1' && who[k] <= '9' )
                {
                    m_reserved.insert( key( y, x ) );
                    m_extra.push_back( "native " + std::to_string( y ) + " " + std::to_string( x ) + " " +
                                       std::string( 1, who[k] ) );
                }
                else
                    m_g[y][x] = who[k];
                m_foes.insert( key( y, x ) );
            }
        }
        lamps();
        zonesAndRoute();
    }

    void lamps()
    {
        // Along each room's walls every few cells (the corners first); in a corridor one in its middle.
        for ( const Room &room : m_rooms )
        {
            const Rect &r = room.m_r;
            bool        red = room.m_theme == "works" || room.m_theme == "shaft";
            auto        lamp = [&]( int y, int x )
            {
                if ( !free( y, x ) ) return;
                m_g[y][x] =
                    room.m_kind == ARENA ? ( ( y + x ) % 2 ? 'R' : 'A' ) : ( red && m_rng.chance( 0.6f ) ? 'R' : 'A' );
            };
            lamp( r.m_r0, r.m_c0 );
            lamp( r.m_r0, r.m_c1 );
            lamp( r.m_r1, r.m_c0 );
            lamp( r.m_r1, r.m_c1 );
            for ( int x = r.m_c0 + 6; x < r.m_c1 - 3; x += 7 ) lamp( r.m_r0, x ), lamp( r.m_r1, x );
            for ( int y = r.m_r0 + 6; y < r.m_r1 - 3; y += 7 ) lamp( y, r.m_c0 ), lamp( y, r.m_c1 );
        }
        for ( const Hall &h : m_halls )
        {
            int len = std::max( h.m_r.width(), h.m_r.height() );
            if ( len < 5 ) continue;
            bool vertical = h.m_dir == 0 || h.m_dir == 2;
            int  y = vertical ? ( h.m_r.m_r0 + h.m_r.m_r1 ) / 2 : h.m_r.m_r0;
            int  x = vertical ? h.m_r.m_c0 : ( h.m_r.m_c0 + h.m_r.m_c1 ) / 2;
            char ch = m_g[y][x];
            if ( ch == '.' || ( ch >= 'a' && ch <= 'z' ) ) m_g[y][x] = 'R';
        }
    }

    void zonesAndRoute()
    {
        auto zone = [&]( const Rect &r, const std::string &theme )
        { m_extra.push_back( "zone " + cellText( r.m_r0, r.m_c0 ) + " " + cellText( r.m_r1, r.m_c1 ) + " " + theme ); };
        if ( !m_rc.m_outdoor.empty() )
        {
            std::string line = "outdoor";
            for ( const std::string &t : m_rc.m_outdoor ) line += " " + t;
            m_extra.push_back( line );
        }
        for ( const Room &room : m_rooms ) zone( room.m_r.grown( 1 ), room.m_theme );
        for ( const Hall &h : m_halls ) zone( h.m_r.grown( 1 ), m_rooms[h.m_from].m_theme );
        if ( m_hasExit ) zone( m_exit, m_rc.m_arenaTheme );
        // The autopilot's way: through each corridor on the way, then into the middle of the set piece.
        std::string route = "route";
        auto        add = [&]( const Rect &m )
        {
            int r = ( m.m_r0 + m.m_r1 ) / 2, c = ( m.m_c0 + m.m_c1 ) / 2;
            route += "  " + cellText( r, c );
        };
        for ( int h = 0; h < m_rc.m_rooms - 1; ++h )
        {
            add( mouth( m_halls[h], false, 1 ) );
            add( mouth( m_halls[h], true, 1 ) );
        }
        const Rect &a = m_rooms[m_rc.m_rooms - 1].m_r;
        int         y = 0, x = 0;
        if ( nearest( a, ( a.m_r0 + a.m_r1 ) * 0.5f, ( a.m_c0 + a.m_c1 ) * 0.5f, y, x,
                      []( int, int ) { return true; } ) )
            route += "  " + cellText( y, x );
        m_extra.push_back( route );
    }
};

}   // namespace

bool isRecipe( const std::string &text )
{
    std::stringstream in( text );
    std::string       line;
    while ( std::getline( in, line ) )
    {
        std::string t = trim( line );
        if ( t.rfind( "genrooms", 0 ) == 0 ) return true;
        if ( !t.empty() && t[0] == '[' ) return false;
    }
    return false;
}

std::string generateLevel( const std::string &recipe, unsigned seed )
{
    std::string head, tail;
    Recipe      rc = parse( recipe, head, tail );
    for ( unsigned attempt = 0; attempt < 400; ++attempt )
    {
        Gen gen( rc, seed + attempt * 7919u );
        if ( gen.run() ) return gen.text( head, tail, seed );
    }
    return recipe;
}

}   // namespace spaceopera
