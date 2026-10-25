/**
 * @file Text.cpp
 * @brief The game's texts: reading the language files, the lookup with German behind it, the system's language.
 * @author Rene Weller
 */
#include "Text.h"

#include <raylib.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

#ifdef _WIN32
extern "C" __declspec( dllimport ) int __stdcall GetUserDefaultLocaleName( wchar_t *name, int size );
#endif

namespace spaceopera
{

namespace
{

std::map<std::string, std::string> s_texts;     ///< the chosen language
std::map<std::string, std::string> s_german;    ///< the source, for the gaps
std::set<std::string>              s_missing;   ///< keys without any text (they show themselves)
std::string                        s_language = "de";

std::string trim( const std::string &s )
{
    size_t a = s.find_first_not_of( " \t" ), b = s.find_last_not_of( " \t" );
    return a == std::string::npos ? std::string() : s.substr( a, b - a + 1 );
}

/** @brief A path from UTF-8 (raylib's paths are UTF-8; Windows would read a plain string in its code page). */
std::filesystem::path utf8Path( const std::string &s )
{
    return std::filesystem::path( std::u8string( s.begin(), s.end() ) );
}

/** @brief Reads every "key = text" line of a language's files into `out`; false if there is no such folder. */
bool readLanguage( const std::string &assets, const std::string &language, std::map<std::string, std::string> &out )
{
    namespace fs = std::filesystem;
    fs::path        dir = utf8Path( assets + "/text/" + language );
    std::error_code error;
    if ( !fs::is_directory( dir, error ) ) return false;
    for ( const auto &entry : fs::directory_iterator( dir, error ) )
    {
        if ( entry.path().extension() != ".txt" ) continue;
        std::ifstream in( entry.path(), std::ios::binary );
        std::string   line;
        bool          first = true;
        while ( std::getline( in, line ) )
        {
            if ( first && line.compare( 0, 3, "\xEF\xBB\xBF" ) == 0 ) line.erase( 0, 3 );
            first = false;
            if ( !line.empty() && line.back() == '\r' ) line.pop_back();
            std::string t = trim( line );
            size_t      eq = t.find( '=' );
            if ( t.empty() || t[0] == '#' || eq == std::string::npos ) continue;
            out[trim( t.substr( 0, eq ) )] = trim( t.substr( eq + 1 ) );
        }
    }
    return true;
}

}   // namespace

bool loadTexts( const std::string &assets, const std::string &language )
{
    s_texts.clear();
    s_german.clear();
    s_missing.clear();
    readLanguage( assets, "de", s_german );
    bool found = language == "de" || readLanguage( assets, language, s_texts );
    s_language = found ? language : "de";
    return found;
}

const char *tr( const std::string &key )
{
    auto it = s_texts.find( key );
    if ( it != s_texts.end() ) return it->second.c_str();
    it = s_german.find( key );
    if ( it != s_german.end() ) return it->second.c_str();
    return s_missing.insert( key ).first->c_str();
}

bool hasText( const std::string &key )
{
    return s_texts.count( key ) || s_german.count( key );
}

const char *trAs( const std::string &key, const std::string &who )
{
    std::string own = key + "." + who;
    return hasText( own ) ? tr( own ) : tr( key );
}

const std::string &language()
{
    return s_language;
}

std::vector<std::string> languages( const std::string &assets )
{
    namespace fs = std::filesystem;
    std::vector<std::string> out;
    std::error_code          error;
    for ( const auto &entry : fs::directory_iterator( utf8Path( assets + "/text" ), error ) )
        if ( entry.is_directory() ) out.push_back( entry.path().filename().string() );
    auto rank = []( const std::string &l ) { return l == "de" ? 0 : ( l == "en" ? 1 : 2 ); };
    std::sort( out.begin(), out.end(), [&]( const std::string &a, const std::string &b )
               { return rank( a ) != rank( b ) ? rank( a ) < rank( b ) : a < b; } );
    return out;
}

std::string systemLanguage( const std::string &assets )
{
    std::string code;
#ifdef _WIN32
    wchar_t name[85] = {};
    if ( GetUserDefaultLocaleName( name, 85 ) > 2 ) code = { (char)name[0], (char)name[1] };
#else
    const char *lang = getenv( "LANG" );
    if ( lang && lang[0] && lang[1] ) code = { lang[0], lang[1] };
#endif
    for ( char &c : code ) c = (char)tolower( (unsigned char)c );
    for ( const std::string &l : languages( assets ) )
        if ( l == code ) return l;
    return "en";
}

std::vector<int> textCodepoints()
{
    std::set<int> cps;
    for ( int c = 32; c < 256; ++c ) cps.insert( c );
    // Dashes, quotes, ellipsis, bullet, arrows: the code puts some of them between texts.
    for ( int c : { 0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E, 0x2022, 0x2026, 0x2190, 0x2191,
                    0x2192, 0x2193 } )
        cps.insert( c );
    for ( const auto *texts : { &s_texts, &s_german } )
        for ( const auto &kv : *texts )
        {
            const char *p = kv.second.c_str();
            while ( *p )
            {
                int size = 0;
                cps.insert( GetCodepointNext( p, &size ) );
                p += size > 0 ? size : 1;
            }
        }
    return std::vector<int>( cps.begin(), cps.end() );
}

}   // namespace spaceopera
