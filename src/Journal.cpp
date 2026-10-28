/**
 * @file Journal.cpp
 * @brief The journal across all games: counts, what was met, the awards (journal.txt: "count <name> <n>",
 *        "met <tag>", "won <award>").
 * @author Rene Weller
 */
#include "Journal.h"

#include "Files.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <set>

namespace spaceopera
{
namespace journal
{

namespace
{

std::string                g_path;
std::map<std::string, int> g_counts;
std::set<std::string>      g_met, g_won;
std::vector<std::string>   g_fresh;
bool                       g_dirty = false;

}   // namespace

const std::vector<AwardDef> &awards()
{
    static const std::vector<AwardDef> all = {
        { "first_blood", "kills", 1 },
        { "kills_100", "kills", 100 },
        { "kills_1000", "kills", 1000 },
        { "kills_5000", "kills", 5000 },
        { "boss_first", "bosses", 1 },
        { "bosses_25", "bosses", 25 },
        { "act1", nullptr, 0 },
        { "act2", nullptr, 0 },
        { "act3", nullptr, 0 },
        { "act4", nullptr, 0 },
        { "ending_a", nullptr, 0 },
        { "ending_b", nullptr, 0 },
        { "ending_c", nullptr, 0 },
        { "all_endings", nullptr, 0 },
        { "hero_corvin", nullptr, 0 },
        { "hero_dorra", nullptr, 0 },
        { "hero_ione", nullptr, 0 },
        { "tales_all", "tales", 4 },
        { "flawless", "flawless", 1 },
        { "flawless_10", "flawless", 10 },
        { "deaths_100", "deaths", 100 },
        { "level_20", nullptr, 0 },
        { "level_40", nullptr, 0 },
        { "finds_250", "finds", 250 },
        { "unique_first", "uniques", 1 },
        { "uniques_10", "uniques", 10 },
        { "set_first", "setpieces", 1 },
        { "set_full", nullptr, 0 },
        { "module", nullptr, 0 },
        { "broken_100", "broken", 100 },
        { "broken_1000", "broken", 1000 },
        { "stims_100", "stims", 100 },
        { "dodges_100", "dodges", 100 },
        { "notes_25", "notes", 25 },
        { "notes_all", "notes", 133 },
        { "codex_families", nullptr, 0 },
        { "codex_all", nullptr, 0 },
        { "cores_spared", nullptr, 0 },
        { "molten", nullptr, 0 },
        { "grabbed", nullptr, 0 },
        { "plus", nullptr, 0 },
        { "nightmare_level", nullptr, 0 },
    };
    return all;
}

void load( const std::string &path )
{
    if ( !g_path.empty() ) return;
    g_path = path;
    FILE *f = fopen( path.c_str(), "r" );
    if ( !f ) return;
    char kind[16], name[128];
    int  n = 0;
    char line[256];
    while ( fgets( line, sizeof( line ), f ) )
    {
        if ( sscanf( line, "%15s %127s %d", kind, name, &n ) < 2 ) continue;
        if ( !strcmp( kind, "count" ) )
            g_counts[name] = n;
        else if ( !strcmp( kind, "met" ) )
            g_met.insert( name );
        else if ( !strcmp( kind, "won" ) )
            g_won.insert( name );
    }
    fclose( f );
}

void flush()
{
    if ( !g_dirty || g_path.empty() ) return;
    FILE *f = beginWrite( g_path );
    if ( !f ) return;
    for ( const auto &[name, n] : g_counts ) fprintf( f, "count %s %d\n", name.c_str(), n );
    for ( const std::string &t : g_met ) fprintf( f, "met %s\n", t.c_str() );
    for ( const std::string &a : g_won ) fprintf( f, "won %s\n", a.c_str() );
    endWrite( f, g_path );
    g_dirty = false;
}

int count( const std::string &name, int add )
{
    int &n = g_counts[name];
    n += add;
    g_dirty = true;
    for ( const AwardDef &a : awards() )
        if ( a.m_count && name == a.m_count && n >= a.m_goal ) win( a.m_key );
    return n;
}

int counted( const std::string &name )
{
    auto it = g_counts.find( name );
    return it == g_counts.end() ? 0 : it->second;
}

void win( const std::string &key )
{
    if ( !g_won.insert( key ).second ) return;
    g_fresh.push_back( key );
    g_dirty = true;
    flush();
}

bool won( const std::string &key )
{
    return g_won.count( key ) > 0;
}

bool meet( const std::string &tag )
{
    if ( !g_met.insert( tag ).second ) return false;
    g_dirty = true;
    return true;
}

bool met( const std::string &tag )
{
    return g_met.count( tag ) > 0;
}

std::vector<std::string> metWith( const std::string &prefix )
{
    std::vector<std::string> out;
    for ( auto it = g_met.lower_bound( prefix ); it != g_met.end() && it->compare( 0, prefix.size(), prefix ) == 0;
          ++it )
        out.push_back( *it );
    return out;
}

std::vector<std::string> fresh()
{
    std::vector<std::string> out;
    out.swap( g_fresh );
    return out;
}

}   // namespace journal
}   // namespace spaceopera
