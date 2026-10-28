/**
 * @file Glyphs.cpp
 * @brief The buttons as pictures: which device is in hand, the actions' characters in the texts, a font whose atlas
 *        holds the letters (white, their coverage as alpha) and the buttons (in their colours).
 * @author Rene Weller
 */
#include "Glyphs.h"

#include "Text.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace spaceopera
{

namespace
{

const char *const ActionNames[(int)ActionE::COUNT] = { "use",       "jump",      "special",  "fire", "stim",   "skill2",
                                                       "skill3",    "strafe",    "confirm",  "back", "pause",  "map",
                                                       "inventory", "character", "salvage",  "page", "choose", "change",
                                                       "move",      "camera",    "camreset", "next", "dodge" };

DeviceE g_device = DeviceE::KEYBOARD;
bool    g_known = false;    ///< the first look has been taken
bool    g_forced = false;   ///< set by a test: stays

bool onDeck()
{
    const char *deck = getenv( "SteamDeck" );
    return deck && atoi( deck ) == 1;
}

/** @brief The kind of pad plugged in, by its name. */
DeviceE padKind()
{
    if ( onDeck() ) return DeviceE::DECK;
    std::string name = IsGamepadAvailable( 0 ) && GetGamepadName( 0 ) ? GetGamepadName( 0 ) : "";
    std::transform( name.begin(), name.end(), name.begin(), []( unsigned char ch ) { return (char)tolower( ch ); } );
    if ( name.find( "steam deck" ) != std::string::npos ) return DeviceE::DECK;
    for ( const char *ps : { "ps4", "ps5", "dualsense", "dualshock", "playstation", "sony", "wireless controller" } )
        if ( name.find( ps ) != std::string::npos ) return DeviceE::PLAYSTATION;
    return DeviceE::XBOX;
}

void encode( int cp, std::string &out )
{
    if ( cp < 0x80 )
        out += (char)cp;
    else if ( cp < 0x800 )
        out += (char)( 0xC0 | ( cp >> 6 ) ), out += (char)( 0x80 | ( cp & 0x3F ) );
    else
        out += (char)( 0xE0 | ( cp >> 12 ) ), out += (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) ),
            out += (char)( 0x80 | ( cp & 0x3F ) );
}

/** @brief One button of a glyph (a glyph may show two side by side). */
struct Part
{
    enum KindE
    {
        FACE,       ///< a face button: 0 south, 1 east, 2 west, 3 north
        SHOULDER,   ///< 0 left, 1 right
        TRIGGER,    ///< 0 left, 1 right
        MENU,       ///< start / options / the Deck's menu
        VIEW,       ///< back / create / the Deck's view
        DPAD,       ///< the cross: 1 up, 2 down, 4 left, 8 right
        STICK,      ///< 0 left, 1 right; +2 pressed in
        KEY,        ///< a key with its label
        MOUSE       ///< 0 left button, 1 right button
    } m_kind;
    int         m_which = 0;
    std::string m_label;
};

/** @brief What an action is on a device. */
std::vector<Part> partsOf( ActionE a, DeviceE d )
{
    using K = Part::KindE;
    if ( d == DeviceE::KEYBOARD )
    {
        auto key = []( const char *k ) { return Part{ K::KEY, 0, tr( k ) }; };
        auto plain = []( const char *label ) { return Part{ K::KEY, 0, label }; };
        switch ( a )
        {
        case ActionE::USE: return { plain( "E" ) };
        case ActionE::JUMP: return { key( "key.space" ) };
        case ActionE::SPECIAL: return { plain( "F" ) };
        case ActionE::FIRE: return { Part{ K::MOUSE, 0, "" } };
        case ActionE::STIM: return { plain( "Q" ) };
        case ActionE::SKILL2: return { plain( "R" ) };
        case ActionE::SKILL3: return { plain( "G" ) };
        case ActionE::STRAFE: return { key( "key.shift" ) };
        case ActionE::CONFIRM: return { plain( "Enter" ) };
        case ActionE::BACK: return { plain( "Esc" ) };
        case ActionE::PAUSE: return { plain( "Esc" ) };
        case ActionE::MAP: return { plain( "M" ) };
        case ActionE::INVENTORY: return { plain( "I" ) };
        case ActionE::CHARACTER: return { plain( "C" ) };
        case ActionE::SALVAGE: return { key( "key.delete" ) };
        case ActionE::PAGE: return { plain( "A" ), plain( "D" ) };
        case ActionE::CHOOSE: return { plain( "W" ), plain( "S" ) };
        case ActionE::CHANGE: return { plain( "A" ), plain( "D" ) };
        case ActionE::MOVE: return { plain( "WASD" ) };
        case ActionE::CAMERA: return { Part{ K::MOUSE, 1, "" } };
        case ActionE::CAMRESET: return { key( "key.backspace" ) };
        case ActionE::NEXT: return { plain( "Enter" ) };
        case ActionE::DODGE: return { key( "key.ctrl" ) };
        default: return { plain( "?" ) };
        }
    }
    switch ( a )
    {
    case ActionE::USE:
    case ActionE::JUMP:
    case ActionE::CONFIRM: return { Part{ K::FACE, 0, "" } };
    case ActionE::SPECIAL:
    case ActionE::BACK: return { Part{ K::FACE, 1, "" } };
    case ActionE::SALVAGE: return { Part{ K::FACE, 2, "" } };
    case ActionE::STIM: return { Part{ K::FACE, 3, "" } };
    case ActionE::FIRE: return { Part{ K::TRIGGER, 1, "" } };
    case ActionE::STRAFE: return { Part{ K::TRIGGER, 0, "" } };
    case ActionE::SKILL2: return { Part{ K::SHOULDER, 0, "" } };
    case ActionE::SKILL3: return { Part{ K::SHOULDER, 1, "" } };
    case ActionE::PAUSE:
    case ActionE::NEXT: return { Part{ K::MENU, 0, "" } };
    case ActionE::INVENTORY:
    case ActionE::CHARACTER: return { Part{ K::VIEW, 0, "" } };
    case ActionE::MAP: return { Part{ K::DPAD, 1, "" } };
    case ActionE::PAGE: return { Part{ K::SHOULDER, 0, "" }, Part{ K::SHOULDER, 1, "" } };
    case ActionE::CHOOSE: return { Part{ K::DPAD, 3, "" } };
    case ActionE::CHANGE: return { Part{ K::DPAD, 12, "" } };
    case ActionE::MOVE: return { Part{ K::STICK, 0, "" } };
    case ActionE::CAMERA: return { Part{ K::STICK, 1, "" } };
    case ActionE::CAMRESET: return { Part{ K::STICK, 3, "" } };
    case ActionE::DODGE: return { Part{ K::STICK, 2, "" } };
    default: return { Part{ K::FACE, 0, "" } };
    }
}

constexpr float Tall = 40.0f;   ///< a glyph's height at the font's size of 48
constexpr float Over = 2.0f;    ///< drawn this much larger, then scaled down (smooth edges)

const Color Body = { 44, 46, 52, 255 };     ///< a button's dark body
const Color Rim = { 120, 126, 136, 255 };   ///< its edge
const Color Ink = { 236, 238, 242, 255 };   ///< labels on dark buttons
const Color Cap = { 222, 222, 216, 255 };   ///< a key's top
const Color CapInk = { 30, 30, 34, 255 };   ///< its label

void roundRect( Image &im, float x, float y, float w, float h, float r, Color c )
{
    r = fminf( r, fminf( w, h ) * 0.5f );
    ImageDrawRectangle( &im, (int)( x + r ), (int)y, (int)( w - 2.0f * r ), (int)h, c );
    ImageDrawRectangle( &im, (int)x, (int)( y + r ), (int)w, (int)( h - 2.0f * r ), c );
    for ( float cx : { x + r, x + w - r - 1.0f } )
        for ( float cy : { y + r, y + h - r - 1.0f } ) ImageDrawCircle( &im, (int)cx, (int)cy, (int)r, c );
}

void label( Image &im, const Font &font, const std::string &text, float cx, float cy, float size, Color c )
{
    Vector2 m = MeasureTextEx( font, text.c_str(), size, 0.0f );
    ImageDrawTextEx( &im, font, text.c_str(), { cx - m.x * 0.5f, cy - m.y * 0.5f }, size, 0.0f, c );
}

/** @brief A part's width (at the glyph's own size, before Over). */
float widthOf( const Part &p, const Font &font )
{
    switch ( p.m_kind )
    {
    case Part::FACE:
    case Part::DPAD:
    case Part::STICK: return Tall;
    case Part::MOUSE: return 28.0f;
    case Part::KEY: return fmaxf( Tall * 0.9f, MeasureTextEx( font, p.m_label.c_str(), 20.0f, 0.0f ).x + 18.0f );
    default: return 50.0f;
    }
}

/** @brief Draws a part at x (Over is already applied to all lengths here). */
void drawPart( Image &im, const Part &p, DeviceE d, float x, const Font &font )
{
    const float k = Over, h = Tall * k, w = widthOf( p, font ) * k, cy = h * 0.5f;
    bool        ps = d == DeviceE::PLAYSTATION, deck = d == DeviceE::DECK;
    switch ( p.m_kind )
    {
    case Part::FACE:
    {
        float cx = x + w * 0.5f, r = h * 0.47f;
        ImageDrawCircle( &im, (int)cx, (int)cy, (int)r, Rim );
        ImageDrawCircle( &im, (int)cx, (int)cy, (int)( r - 2.0f * k ), Body );
        if ( ps )
        {
            // Cross, circle, square, triangle, each in its colour.
            float s = r * 0.42f, t = 3.2f * k;
            if ( p.m_which == 0 )
            {
                Color c = { 130, 170, 245, 255 };
                ImageDrawLineEx( &im, { cx - s, cy - s }, { cx + s, cy + s }, (int)t, c );
                ImageDrawLineEx( &im, { cx - s, cy + s }, { cx + s, cy - s }, (int)t, c );
            }
            else if ( p.m_which == 1 )
            {
                ImageDrawCircle( &im, (int)cx, (int)cy, (int)( s * 1.1f ), { 240, 95, 105, 255 } );
                ImageDrawCircle( &im, (int)cx, (int)cy, (int)( s * 1.1f - t ), Body );
            }
            else if ( p.m_which == 2 )
            {
                Color   c = { 235, 140, 210, 255 };
                Vector2 q[4] = { { cx - s, cy - s }, { cx + s, cy - s }, { cx + s, cy + s }, { cx - s, cy + s } };
                for ( int i = 0; i < 4; ++i ) ImageDrawLineEx( &im, q[i], q[( i + 1 ) % 4], (int)t, c );
            }
            else
            {
                Color   c = { 90, 215, 175, 255 };
                Vector2 q[3] = { { cx, cy - s * 1.1f },
                                 { cx + s * 1.1f, cy + s * 0.75f },
                                 { cx - s * 1.1f, cy + s * 0.75f } };
                for ( int i = 0; i < 3; ++i ) ImageDrawLineEx( &im, q[i], q[( i + 1 ) % 3], (int)t, c );
            }
            break;
        }
        // Xbox: green A, red B, blue X, yellow Y; the Deck's letters are white.
        const char *letters[4] = { "A", "B", "X", "Y" };
        const Color tints[4] = {
            { 110, 200, 90, 255 }, { 235, 85, 80, 255 }, { 80, 145, 240, 255 }, { 240, 200, 60, 255 }
        };
        label( im, font, letters[p.m_which], cx, cy + 1.0f * k, 24.0f * k, deck ? Ink : tints[p.m_which] );
        break;
    }
    case Part::SHOULDER:
    case Part::TRIGGER:
    {
        // Bumpers flat, triggers rounder; their names as the pad prints them.
        bool  trigger = p.m_kind == Part::TRIGGER;
        float top = trigger ? 2.0f * k : 7.0f * k, bottom = trigger ? h - 2.0f * k : h - 7.0f * k;
        roundRect( im, x, top, w, bottom - top, trigger ? 12.0f * k : 7.0f * k, Rim );
        roundRect( im, x + 2.0f * k, top + 2.0f * k, w - 4.0f * k, bottom - top - 4.0f * k,
                   trigger ? 10.0f * k : 5.0f * k, Body );
        const char *names[2][2] = { { "LB", "RB" }, { "LT", "RT" } };
        const char *sony[2][2] = { { "L1", "R1" }, { "L2", "R2" } };
        label( im, font, ( ps || deck ? sony : names )[trigger][p.m_which], x + w * 0.5f, cy + 1.0f * k, 19.0f * k,
               Ink );
        break;
    }
    case Part::MENU:
    case Part::VIEW:
    {
        roundRect( im, x, 6.0f * k, w, h - 12.0f * k, 14.0f * k, Rim );
        roundRect( im, x + 2.0f * k, 8.0f * k, w - 4.0f * k, h - 16.0f * k, 12.0f * k, Body );
        float cx = x + w * 0.5f;
        if ( p.m_kind == Part::MENU )
            for ( float dy : { -6.0f, 0.0f, 6.0f } )
                ImageDrawRectangle( &im, (int)( cx - 8.0f * k ), (int)( cy + ( dy - 1.2f ) * k ), (int)( 16.0f * k ),
                                    (int)( 2.4f * k ), Ink );
        else
        {
            // Two windows, one over the other.
            roundRect( im, cx - 9.0f * k, cy - 7.0f * k, 12.0f * k, 9.0f * k, 1.0f * k, Ink );
            roundRect( im, cx - 3.0f * k, cy - 1.0f * k, 12.0f * k, 9.0f * k, 1.0f * k, Ink );
            roundRect( im, cx - 1.5f * k, cy + 0.5f * k, 9.0f * k, 6.0f * k, 1.0f * k, Body );
        }
        break;
    }
    case Part::DPAD:
    {
        float cx = x + w * 0.5f, arm = h * 0.46f, half = h * 0.16f;
        ImageDrawRectangle( &im, (int)( cx - half ), (int)( cy - arm ), (int)( 2 * half ), (int)( 2 * arm ), Rim );
        ImageDrawRectangle( &im, (int)( cx - arm ), (int)( cy - half ), (int)( 2 * arm ), (int)( 2 * half ), Rim );
        ImageDrawRectangle( &im, (int)( cx - half + 2 * k ), (int)( cy - arm + 2 * k ), (int)( 2 * half - 4 * k ),
                            (int)( 2 * arm - 4 * k ), Body );
        ImageDrawRectangle( &im, (int)( cx - arm + 2 * k ), (int)( cy - half + 2 * k ), (int)( 2 * arm - 4 * k ),
                            (int)( 2 * half - 4 * k ), Body );
        // The arms meant, lit.
        Color lit = Ink;
        if ( p.m_which & 1 )
            ImageDrawRectangle( &im, (int)( cx - half + 3 * k ), (int)( cy - arm + 3 * k ), (int)( 2 * half - 6 * k ),
                                (int)( arm - half - 2 * k ), lit );
        if ( p.m_which & 2 )
            ImageDrawRectangle( &im, (int)( cx - half + 3 * k ), (int)( cy + half - 1 * k ), (int)( 2 * half - 6 * k ),
                                (int)( arm - half - 2 * k ), lit );
        if ( p.m_which & 4 )
            ImageDrawRectangle( &im, (int)( cx - arm + 3 * k ), (int)( cy - half + 3 * k ), (int)( arm - half - 2 * k ),
                                (int)( 2 * half - 6 * k ), lit );
        if ( p.m_which & 8 )
            ImageDrawRectangle( &im, (int)( cx + half - 1 * k ), (int)( cy - half + 3 * k ),
                                (int)( arm - half - 2 * k ), (int)( 2 * half - 6 * k ), lit );
        break;
    }
    case Part::STICK:
    {
        float cx = x + w * 0.5f, r = h * 0.47f;
        ImageDrawCircle( &im, (int)cx, (int)cy, (int)r, Rim );
        ImageDrawCircle( &im, (int)cx, (int)cy, (int)( r - 2.0f * k ), Body );
        ImageDrawCircle( &im, (int)cx, (int)cy, (int)( r * 0.62f ), Rim );
        ImageDrawCircle( &im, (int)cx, (int)cy, (int)( r * 0.62f - 2.0f * k ), Body );
        bool        right = p.m_which & 1, pressed = p.m_which & 2;
        std::string text =
            pressed ? ( ps || deck ? ( right ? "R3" : "L3" ) : ( right ? "RS" : "LS" ) ) : ( right ? "R" : "L" );
        label( im, font, text, cx, cy + 1.0f * k, ( pressed ? 15.0f : 19.0f ) * k, Ink );
        break;
    }
    case Part::KEY:
    {
        roundRect( im, x, 3.0f * k, w, h - 6.0f * k, 6.0f * k, { 70, 70, 74, 255 } );
        roundRect( im, x + 2.0f * k, 4.0f * k, w - 4.0f * k, h - 11.0f * k, 5.0f * k, Cap );
        label( im, font, p.m_label, x + w * 0.5f, cy - 1.5f * k, 20.0f * k, CapInk );
        break;
    }
    case Part::MOUSE:
    {
        float mx = x, mw = w, top = 2.0f * k;
        roundRect( im, mx, top, mw, h - 4.0f * k, 12.0f * k, Rim );
        roundRect( im, mx + 2.0f * k, top + 2.0f * k, mw - 4.0f * k, h - 8.0f * k, 10.0f * k, Body );
        // The button meant, lit; the line between the two.
        float half = ( mw - 4.0f * k ) * 0.5f;
        ImageDrawRectangle( &im, (int)( mx + 2.0f * k + ( p.m_which ? half + 1.0f * k : 3.0f * k ) ),
                            (int)( top + 4.0f * k ), (int)( half - 4.0f * k ), (int)( h * 0.36f ), Ink );
        ImageDrawRectangle( &im, (int)( mx + mw * 0.5f - 1.0f * k ), (int)( top + 2.0f * k ), (int)( 2.0f * k ),
                            (int)( h * 0.42f ), Rim );
        break;
    }
    }
}

/** @brief The picture of an action on a device. */
Image drawAction( ActionE a, DeviceE d, const Font &font )
{
    std::vector<Part> parts = partsOf( a, d );
    float             w = 0.0f;
    for ( size_t i = 0; i < parts.size(); ++i ) w += widthOf( parts[i], font ) + ( i ? 4.0f : 0.0f );
    Image im = GenImageColor( (int)ceilf( w * Over ), (int)( Tall * Over ), BLANK );
    float x = 0.0f;
    for ( const Part &p : parts )
    {
        drawPart( im, p, d, x, font );
        x += ( widthOf( p, font ) + 4.0f ) * Over;
    }
    ImageResize( &im, (int)ceilf( w ), (int)Tall );
    return im;
}

}   // namespace

DeviceE device()
{
    if ( !g_known && !g_forced )
    {
        g_known = true;
        g_device = onDeck() ? DeviceE::DECK : ( IsGamepadAvailable( 0 ) ? padKind() : DeviceE::KEYBOARD );
    }
    return g_device;
}

void setDevice( DeviceE d )
{
    g_device = d;
    g_forced = true;
}

bool noteInput()
{
    DeviceE was = device();
    if ( g_forced ) return false;
    bool pad = false;
    if ( IsGamepadAvailable( 0 ) )
    {
        for ( int b = 1; b <= GAMEPAD_BUTTON_RIGHT_THUMB && !pad; ++b ) pad = IsGamepadButtonPressed( 0, b );
        for ( int a = 0; a < 4 && !pad; ++a ) pad = fabsf( GetGamepadAxisMovement( 0, a ) ) > 0.5f;
    }
    if ( pad ) g_device = padKind();
    // A key, a click or a real move of the mouse (the Deck's trackpads move it too, so a little does not count).
    if ( GetKeyPressed() != 0 || IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) ||
         IsMouseButtonPressed( MOUSE_BUTTON_RIGHT ) || Vector2Length( GetMouseDelta() ) > 12.0f )
        g_device = DeviceE::KEYBOARD;
    return g_device != was;
}

std::string actionTokens( const std::string &text )
{
    if ( text.find( '{' ) == std::string::npos ) return text;
    std::string out;
    for ( size_t i = 0; i < text.size(); ++i )
    {
        bool done = false;
        if ( text[i] == '{' )
            for ( int a = 0; a < (int)ActionE::COUNT && !done; ++a )
            {
                size_t n = strlen( ActionNames[a] );
                if ( text.compare( i + 1, n, ActionNames[a] ) == 0 && i + 1 + n < text.size() &&
                     text[i + 1 + n] == '}' )
                {
                    encode( GlyphBase + a, out );
                    i += n + 1;
                    done = true;
                }
            }
        if ( !done ) out += text[i];
    }
    return out;
}

Font glyphFont( const char *path, int size, const std::vector<int> &codepoints )
{
    Font           font = {};
    int            bytes = 0;
    unsigned char *data = LoadFileData( path, &bytes );
    if ( !data ) return font;
    std::vector<int> cps;
    for ( int c : codepoints )
        if ( c < GlyphBase || c >= GlyphBase + (int)ActionE::COUNT ) cps.push_back( c );
    // The keys' labels: the same face, larger (the buttons are drawn at twice their size).
    static Font labels = {};
    if ( labels.texture.id == 0 )
    {
        std::vector<int> latin;
        for ( int c = 32; c < 256; ++c ) latin.push_back( c );
        labels = LoadFontFromMemory( ".ttf", data, bytes, 56, latin.data(), (int)latin.size() );
        SetTextureFilter( labels.texture, TEXTURE_FILTER_BILINEAR );
    }
    GlyphInfo *letters = LoadFontData( data, bytes, size, cps.data(), (int)cps.size(), FONT_DEFAULT );
    UnloadFileData( data );
    if ( !letters ) return font;
    int        n = (int)cps.size(), m = (int)ActionE::COUNT, all = n + m;
    GlyphInfo *glyphs = (GlyphInfo *)MemAlloc( sizeof( GlyphInfo ) * all );
    memcpy( glyphs, letters, sizeof( GlyphInfo ) * n );
    MemFree( letters );
    for ( int a = 0; a < m; ++a )
    {
        GlyphInfo &g = glyphs[n + a];
        g.value = GlyphBase + a;
        g.image = drawAction( (ActionE)a, device(), labels );
        g.offsetX = 1;
        g.offsetY = (int)( ( size - Tall ) * 0.5f + size * 0.04f );
        g.advanceX = g.image.width + 4;
    }
    // The atlas, packed in rows: letters white with their coverage as alpha, the buttons as drawn.
    const int  pad = 4, width = 2048;
    Rectangle *recs = (Rectangle *)MemAlloc( sizeof( Rectangle ) * all );
    int        x = pad, y = pad, row = 0;
    for ( int i = 0; i < all; ++i )
    {
        const Image &im = glyphs[i].image;
        if ( x + im.width + pad > width ) x = pad, y += row + pad, row = 0;
        recs[i] = { (float)x, (float)y, (float)im.width, (float)im.height };
        x += im.width + pad;
        row = std::max( row, im.height );
    }
    Image atlas = GenImageColor( width, y + row + pad, BLANK );
    auto *dst = (unsigned char *)atlas.data;
    for ( int i = 0; i < all; ++i )
    {
        const Image &im = glyphs[i].image;
        if ( !im.data ) continue;
        bool gray = im.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        if ( !gray && im.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 ) continue;
        auto *src = (const unsigned char *)im.data;
        for ( int py = 0; py < im.height; ++py )
            for ( int px = 0; px < im.width; ++px )
            {
                unsigned char *o = dst + ( ( (int)recs[i].y + py ) * width + (int)recs[i].x + px ) * 4;
                if ( gray )
                    o[0] = o[1] = o[2] = 255, o[3] = src[py * im.width + px];
                else
                    memcpy( o, src + ( py * im.width + px ) * 4, 4 );
            }
    }
    font.baseSize = size;
    font.glyphCount = all;
    font.glyphPadding = pad;
    font.glyphs = glyphs;
    font.recs = recs;
    font.texture = LoadTextureFromImage( atlas );
    UnloadImage( atlas );
    SetTextureFilter( font.texture, TEXTURE_FILTER_BILINEAR );
    return font;
}

}   // namespace spaceopera
