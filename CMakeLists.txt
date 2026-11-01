# DARK CONCORDANCE (the repository's working name: SpaceOpera) -- a dark space opera action RPG in the manner of
# Baldur's Gate: Dark Alliance.
# C++20 on raylib (fetched at configure time), like Sunstreak.
cmake_minimum_required( VERSION 3.22 )

# raylib 5.5 and its GLFW still ask for old CMake versions; CMake 4 refuses those without this floor.
set( CMAKE_POLICY_VERSION_MINIMUM 3.5 )
set( CMAKE_POLICY_DEFAULT_CMP0091 NEW )
set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )

project( SpaceOpera VERSION 1.1.0 LANGUAGES C CXX )

set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

set( SPACEOPERA_BIN_DIR "" CACHE PATH "where the game is copied after each build (bin/<preset>)" )

include( FetchContent )
set( SPACEOPERA_EXTRACT_TIMESTAMP "" )
if( CMAKE_VERSION VERSION_GREATER_EQUAL 3.24 )
    set( SPACEOPERA_EXTRACT_TIMESTAMP DOWNLOAD_EXTRACT_TIMESTAMP TRUE )
endif()
set( BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
set( BUILD_GAMES OFF CACHE BOOL "" FORCE )
set( BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE )
FetchContent_Declare( raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    ${SPACEOPERA_EXTRACT_TIMESTAMP} )
FetchContent_MakeAvailable( raylib )

file( GLOB SPACEOPERA_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.h )
add_executable( SpaceOpera WIN32 ${SPACEOPERA_SOURCES} )
target_link_libraries( SpaceOpera PRIVATE raylib )
# The films are AV1, decoded by dav1d (BSD-2): built once with Meson for MSVC into third_party/dav1d (8 bit, no
# assembly, static /MT); elsewhere the system's library.
if( MSVC )
    target_include_directories( SpaceOpera PRIVATE ${CMAKE_SOURCE_DIR}/third_party/dav1d/include )
    target_link_libraries( SpaceOpera PRIVATE ${CMAKE_SOURCE_DIR}/third_party/dav1d/lib/win64/dav1d.lib )
else()
    find_package( PkgConfig REQUIRED )
    pkg_check_modules( DAV1D REQUIRED IMPORTED_TARGET dav1d )
    target_link_libraries( SpaceOpera PRIVATE PkgConfig::DAV1D )
endif()
# The textures are read from assets/ beside the program, else from the source tree.
target_compile_definitions( SpaceOpera PRIVATE SPACEOPERA_ASSETS="${CMAKE_SOURCE_DIR}/assets"
                                                  SPACEOPERA_VERSION="${PROJECT_VERSION}" )
if( MSVC )
    # The program's icon in Explorer and on the taskbar (tools/make_icon.py draws it).
    target_sources( SpaceOpera PRIVATE ${CMAKE_SOURCE_DIR}/res/darkconcordance.rc )
    target_compile_options( SpaceOpera PRIVATE /W4 /permissive- /utf-8 /wd4244 /wd4267 /wd4305 )
    target_compile_definitions( SpaceOpera PRIVATE _CRT_SECURE_NO_WARNINGS )
    # A window program without a console that still starts in main().
    target_link_options( SpaceOpera PRIVATE /ENTRY:mainCRTStartup )
else()
    # -fwrapv: an int that overflows wraps round as with MSVC, so a seed builds the same level everywhere.
    target_compile_options( SpaceOpera PRIVATE -Wall -Wextra -Wno-missing-field-initializers -fwrapv )
    target_link_options( SpaceOpera PRIVATE -static-libstdc++ -static-libgcc )
    # A release carries the system's dav1d in lib/ beside the program (Deploy/build_linux.sh): looked for there first.
    set_target_properties( SpaceOpera PROPERTIES BUILD_RPATH "$ORIGIN/lib" )
endif()

if( SPACEOPERA_BIN_DIR )
    add_custom_command( TARGET SpaceOpera POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPACEOPERA_BIN_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SpaceOpera> "${SPACEOPERA_BIN_DIR}" )
endif()
