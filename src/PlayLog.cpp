/**
 * @file PlayLog.cpp
 * @brief The play log: appended a line at a time, flushed at once (a crash keeps what was written).
 * @author Rene Weller
 */
#include "PlayLog.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace spaceopera
{
namespace playlog
{

namespace
{

FILE       *g_file = nullptr;
std::string g_folder;   ///< where the log is

constexpr int Kept = 48, Wide = 240;   ///< the last lines kept for a crash report, each this long at most
char          g_kept[Kept][Wide] = {};
int           g_next = 0;    ///< where the next one goes
int           g_count = 0;   ///< how many there are

}   // namespace

void open( const std::string &path )
{
    if ( g_file ) return;
    g_file = fopen( path.c_str(), "a" );
    size_t slash = path.find_last_of( "/\\" );
    g_folder = slash == std::string::npos ? "" : path.substr( 0, slash + 1 );
    if ( g_file ) line( "session" );
}

bool on()
{
    return g_file != nullptr;
}

void line( const char *format, ... )
{
    // Kept for a crash report whether the log is written or not.
    char       stamp[32];
    time_t     now = time( nullptr );
    struct tm *t = localtime( &now );
    strftime( stamp, sizeof( stamp ), "%Y-%m-%d %H:%M:%S", t );
    char   *kept = g_kept[g_next];
    int     n = snprintf( kept, Wide, "%s  ", stamp );
    va_list args;
    va_start( args, format );
    vsnprintf( kept + n, (size_t)( Wide - n ), format, args );
    va_end( args );
    g_next = ( g_next + 1 ) % Kept;
    g_count = g_count < Kept ? g_count + 1 : Kept;
    if ( !g_file ) return;
    fputs( kept, g_file );
    fputc( '\n', g_file );
    fflush( g_file );
}

std::string folder()
{
    return g_file ? g_folder : "";
}

int recent( const char **lines, int most )
{
    int n = g_count < most ? g_count : most;
    for ( int i = 0; i < n; ++i ) lines[i] = g_kept[( g_next - n + i + Kept * 2 ) % Kept];
    return n;
}

}   // namespace playlog
}   // namespace spaceopera
