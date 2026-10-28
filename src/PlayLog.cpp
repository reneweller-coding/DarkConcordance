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

FILE *g_file = nullptr;

}   // namespace

void open( const std::string &path )
{
    if ( g_file ) return;
    g_file = fopen( path.c_str(), "a" );
    if ( g_file ) line( "session" );
}

bool on()
{
    return g_file != nullptr;
}

void line( const char *format, ... )
{
    if ( !g_file ) return;
    char       stamp[32];
    time_t     now = time( nullptr );
    struct tm *t = localtime( &now );
    strftime( stamp, sizeof( stamp ), "%Y-%m-%d %H:%M:%S", t );
    fprintf( g_file, "%s  ", stamp );
    va_list args;
    va_start( args, format );
    vfprintf( g_file, format, args );
    va_end( args );
    fputc( '\n', g_file );
    fflush( g_file );
}

}   // namespace playlog
}   // namespace spaceopera
