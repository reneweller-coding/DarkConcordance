/**
 * @file Shaders.h
 * @brief The GLSL of the look test, kept in the program: lit surfaces (triplanar PBR, sun and lamp shadows, point
 *        lights), the depth pre-pass, the volumetric fog (its own pass at half size, from the depth), the glass floor,
 *        the sky, the gas giant, particles, the shadow maps and the post chain (bloom, ACES, grade, grain, FXAA).
 * @author Rene Weller
 */
#pragma once

namespace spaceopera
{

/**
 * @brief Vertex shader of everything lit: world and object position and normal, colour, UV. The depth pre-pass draws
 *        with it too: the position and the world point are invariant, and the pre-pass gives it the same mvp and model
 *        matrix as raylib's DrawMesh, so both passes reach the same depth to the bit and the lit pass can test EQUAL
 *        (and decals that lie at one height with another tie as they always did). Plants sway in the wind here (sway
 *        1-3: grass, leaf cards, fir branches): only the cut-outs, which the pre-pass and the shadow maps leave out; for
 *        everything else sway is 0 and the position is the same expression in every pass.
 */
inline const char *LitVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform float time;
uniform float sway;   // 0 still, 1 grass, 2 leaf cards, 3 fir branches, 4 leaves that do not move (Gfx.h, Surface)
uniform vec4 wind;    // where it blows (x, z), its speed (m/s; 0 none), a gust blowing now (0 .. 1)
invariant gl_Position;
invariant out vec3 fragPos;
out vec3 fragNormal;
out vec3 objPos;
out vec3 objNormal;
out vec4 fragColor;
out vec2 fragTexCoord;

// How far a plant's vertex is blown: bent along the wind, more in the gusts that travel over the place with it (bands
// some 18 m apart, never twice alike), each plant swinging back in its own phase (smooth over the place, so that a
// card's corners keep together), a leaf's flutter across it; a blade's tip dips as it bends.
vec3 windSway( vec3 p, vec2 uv )
{
    vec2 dir = wind.xy;
    float speed = wind.z;
    float along = dot( p.xz, dir );
    float band = 0.5 + 0.5 * sin( along * 0.35 - time * speed * 0.35 + 2.0 * sin( time * 0.21 + p.x * 0.05 ) );
    float gust = min( band * band * ( 0.7 + 0.3 * sin( dot( p.xz, vec2( 0.13, -0.09 ) ) + time * 0.4 ) ) + wind.w, 1.5 );
    float phase = dot( p.xz, vec2( 0.9, 1.3 ) ) + p.y * 0.7;
    float swing = 0.55 + 0.45 * sin( time * ( 1.1 + 0.25 * speed ) + phase );
    float flutter = sin( time * 6.3 + phase * 3.1 ) * 0.5 + sin( time * 9.7 + phase * 1.7 ) * 0.3;
    float strength = clamp( speed / 3.0, 0.4, 1.6 );
    vec2 side = vec2( -dir.y, dir.x );
    float amp;
    if( sway < 1.5 )
    {
        float tip = 1.0 - clamp( uv.y, 0.0, 1.0 );   // the card's top (v = 0) sways, its foot stays
        amp = tip * tip * 0.11;
    }
    else
    {
        amp = 0.025 + 0.018 * clamp( p.y, 0.0, 6.0 );   // higher up, more
        if( sway > 2.5 ) amp *= 0.4 + 0.6 * ( 1.0 - clamp( uv.y, 0.0, 1.0 ) );   // a branch's root less than its tip
    }
    amp *= strength;
    vec2 bend = dir * amp * ( 0.25 + 0.75 * gust ) * swing + side * amp * 0.25 * flutter;
    return vec3( bend.x, -0.35 * length( bend ) * step( sway, 1.5 ) + amp * 0.12 * flutter * step( 1.5, sway ),
                 bend.y );
}

void main()
{
    vec3 p = vertexPosition;
    if( sway > 0.5 && sway < 3.5 && wind.z > 0.0 ) p += windSway( p, vertexTexCoord );
    objPos = vertexPosition;
    objNormal = vertexNormal;
    fragPos = vec3( matModel * vec4( p, 1.0 ) );
    fragNormal = normalize( vec3( matNormal * vec4( vertexNormal, 0.0 ) ) );
    fragColor = vertexColor;
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * vec4( p, 1.0 );
}
)";

/**
 * @brief Vertex shader of skinned figures (bones from the glTF, matrices per figure): world position and normal
 *        skinned; objPos stays in the rest pose (the clothes' pattern sticks to the body), objNormal is skinned.
 */
inline const char *LitSkinVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[128];
invariant gl_Position;
invariant out vec3 fragPos;
out vec3 fragNormal;
out vec3 objPos;
out vec3 objNormal;
out vec4 fragColor;
out vec2 fragTexCoord;
void main()
{
    mat4 skin = boneMatrices[int( vertexBoneIds.x )] * vertexBoneWeights.x +
                boneMatrices[int( vertexBoneIds.y )] * vertexBoneWeights.y +
                boneMatrices[int( vertexBoneIds.z )] * vertexBoneWeights.z +
                boneMatrices[int( vertexBoneIds.w )] * vertexBoneWeights.w;
    vec4 pos = skin * vec4( vertexPosition, 1.0 );
    vec3 nrm = normalize( mat3( skin ) * vertexNormal );
    objPos = vertexPosition;
    objNormal = nrm;
    fragPos = vec3( matModel * pos );
    fragNormal = normalize( vec3( matNormal * vec4( nrm, 0.0 ) ) );
    fragColor = vertexColor;
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * pos;
}
)";

/** @brief Depth only, skinned (the shadow maps): the light's viewProj, the figure's model matrix and bones. */
inline const char *DepthSkinVs = R"(
#version 330
in vec3 vertexPosition;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
uniform mat4 viewProj;
uniform mat4 matModel;
uniform mat4 boneMatrices[128];
void main()
{
    mat4 skin = boneMatrices[int( vertexBoneIds.x )] * vertexBoneWeights.x +
                boneMatrices[int( vertexBoneIds.y )] * vertexBoneWeights.y +
                boneMatrices[int( vertexBoneIds.z )] * vertexBoneWeights.z +
                boneMatrices[int( vertexBoneIds.w )] * vertexBoneWeights.w;
    gl_Position = viewProj * ( matModel * ( skin * vec4( vertexPosition, 1.0 ) ) );
}
)";

/**
 * @brief What every lit fragment shader shares: the lights, the shadows, the fog's room. Gfx puts the #version line
 *        before it, and for the lit surfaces MAX_LIGHTS 16 (each draw gets only the lights that reach it).
 */
inline const char *CommonFs = R"(
const float PI = 3.14159265;
uniform vec3 viewPos;
uniform float time;
uniform float frame;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform mat4 sunVP;
uniform sampler2D sunShadow;
uniform float sunTexel;
uniform vec3 spotPos;
uniform vec3 spotDir;
uniform vec3 spotColor;
uniform vec2 spotCos;
uniform float spotRange;
uniform mat4 spotVP;
uniform sampler2D spotShadow;
uniform vec2 spotNF;
#ifndef MAX_LIGHTS
#define MAX_LIGHTS 32
#endif
uniform int lightCount;
uniform vec4 lightPos[MAX_LIGHTS];
uniform vec4 lightColor[MAX_LIGHTS];
uniform vec4 lightMask[MAX_LIGHTS];   // per light: its mask in the atlas (xy + world xz times z), w 1: the walls stop it
uniform sampler2D lightMasks;         // the level's light masks (Gfx::lightMasks): 1 where a light arrives
uniform vec3 ambUp;
uniform vec3 ambDown;
uniform vec4 fogParams;
uniform vec3 fogColor;
uniform vec3 roomMin;
uniform vec3 roomMax;
#define MAX_WATER 8
uniform int waterCount;
uniform vec4 waterBalls[MAX_WATER];
uniform int fogSteps;     // the fog's steps along a ray (the quality's: 8, 20, 40)
uniform int shadowTaps;   // a soft shadow's samples (4, 8, 12)
uniform vec4 clouds;      // the clouds' shadows: how much they darken the sun (0: none), their size (1/m), drift (m/s)
uniform vec4 wind;        // where the wind blows (x, z), its speed (m/s; 0: none), a gust blowing now (0 .. 1)

const vec2 Poisson[12] = vec2[]( vec2( -0.326, -0.406 ), vec2( -0.840, -0.074 ), vec2( -0.696, 0.457 ),
    vec2( -0.203, 0.621 ), vec2( 0.962, -0.195 ), vec2( 0.473, -0.480 ), vec2( 0.519, 0.767 ), vec2( 0.185, -0.893 ),
    vec2( 0.507, 0.064 ), vec2( 0.896, 0.412 ), vec2( -0.322, -0.933 ), vec2( -0.792, -0.598 ) );

float hash13( vec3 p )
{
    p = fract( p * 0.1031 );
    p += dot( p, p.zyx + 31.32 );
    return fract( ( p.x + p.y ) * p.z );
}

float noise3( vec3 p )
{
    vec3 i = floor( p );
    vec3 f = fract( p );
    f = f * f * ( 3.0 - 2.0 * f );
    return mix( mix( mix( hash13( i ), hash13( i + vec3( 1, 0, 0 ) ), f.x ),
                     mix( hash13( i + vec3( 0, 1, 0 ) ), hash13( i + vec3( 1, 1, 0 ) ), f.x ), f.y ),
                mix( mix( hash13( i + vec3( 0, 0, 1 ) ), hash13( i + vec3( 1, 0, 1 ) ), f.x ),
                     mix( hash13( i + vec3( 0, 1, 1 ) ), hash13( i + vec3( 1, 1, 1 ) ), f.x ), f.y ), f.z );
}

float ign( vec2 p )
{
    p += frame * 5.588238;
    return fract( 52.9829189 * fract( dot( p, vec2( 0.06711056, 0.00583715 ) ) ) );
}

vec3 toLinear( vec3 c ) { return pow( c, vec3( 2.2 ) ); }

float atten( float d, float r )
{
    float x = d / r;
    float w = clamp( 1.0 - x * x * x * x, 0.0, 1.0 );
    return w * w / ( d * d + 1.0 );
}

// How much of point light i reaches p past the walls (its mask seen from above; 1 for a light without one).
float lightVis( int i, vec3 p )
{
    if( lightMask[i].w < 0.5 ) return 1.0;
    return texture( lightMasks, lightMask[i].xy + p.xz * lightMask[i].z ).r;
}

vec3 brdf( vec3 N, vec3 V, vec3 L, vec3 albedo, float rough, float metal )
{
    vec3 H = normalize( V + L );
    float NdL = max( dot( N, L ), 0.0 );
    float NdV = max( dot( N, V ), 1e-4 );
    float NdH = max( dot( N, H ), 0.0 );
    float VdH = max( dot( V, H ), 0.0 );
    float a = rough * rough;
    float a2 = a * a;
    float d = NdH * NdH * ( a2 - 1.0 ) + 1.0;
    float D = a2 / ( PI * d * d );
    float k = ( rough + 1.0 ) * ( rough + 1.0 ) / 8.0;
    float G = ( NdV / ( NdV * ( 1.0 - k ) + k ) ) * ( NdL / ( NdL * ( 1.0 - k ) + k ) );
    vec3 F0 = mix( vec3( 0.04 ), albedo, metal );
    vec3 F = F0 + ( 1.0 - F0 ) * pow( 1.0 - VdH, 5.0 );
    vec3 spec = D * G * F / ( 4.0 * NdV * NdL + 1e-4 );
    vec3 kd = ( 1.0 - F ) * ( 1.0 - metal );
    return ( kd * albedo / PI + spec ) * NdL;
}

float sunShadowAt( vec3 wp, vec3 n, float ndl, int taps )
{
    vec4 p = sunVP * vec4( wp + n * 0.04, 1.0 );
    vec3 q = p.xyz / p.w * 0.5 + 0.5;
    if( q.x <= 0.0 || q.x >= 1.0 || q.y <= 0.0 || q.y >= 1.0 || q.z >= 1.0 ) return 1.0;
    float bias = 0.0003 + 0.0007 * ( 1.0 - ndl );
    if( taps <= 1 ) return q.z - bias <= texture( sunShadow, q.xy ).r ? 1.0 : 0.0;
    float a = ign( gl_FragCoord.xy ) * 6.2831853;
    mat2 R = mat2( cos( a ), sin( a ), -sin( a ), cos( a ) );
    int count = clamp( taps, 2, 12 ), stride = 12 / count;
    float s = 0.0;
    for( int i = 0; i < count; i++ )
        s += q.z - bias <= texture( sunShadow, q.xy + R * Poisson[i * stride] * sunTexel * 1.6 ).r ? 1.0 : 0.0;
    return s / float( count );
}

// The shadows of clouds drifting over the place (where it has a sky with air): a slow noise in the plane the sunlight
// crosses above, carried by the wind, changing slowly as it goes; what stands higher meets it further along the sun.
float cloudShade( vec3 p )
{
    if( clouds.x <= 0.0 ) return 1.0;
    vec2 q = ( p.xz + sunDir.xz * ( p.y / max( -sunDir.y, 0.2 ) ) - clouds.zw * time ) * clouds.y;
    float n = noise3( vec3( q, time * 0.004 ) ) * 0.7 + noise3( vec3( q * 2.7, 5.0 + time * 0.007 ) ) * 0.3;
    return 1.0 - clouds.x * smoothstep( 0.42, 0.68, n );
}

float spotLinear( float d )
{
    float z = d * 2.0 - 1.0;
    return 2.0 * spotNF.x * spotNF.y / ( spotNF.y + spotNF.x - z * ( spotNF.y - spotNF.x ) );
}

float spotShadowAt( vec3 wp, vec3 n, int taps )
{
    vec4 p = spotVP * vec4( wp + n * 0.03, 1.0 );
    if( p.w <= 0.0 ) return 0.0;
    vec3 q = p.xyz / p.w * 0.5 + 0.5;
    if( q.x <= 0.0 || q.x >= 1.0 || q.y <= 0.0 || q.y >= 1.0 ) return 0.0;
    float dist = spotLinear( q.z );
    if( taps <= 1 ) return dist - 0.06 <= spotLinear( texture( spotShadow, q.xy ).r ) ? 1.0 : 0.0;
    int count = clamp( taps, 2, 12 ), stride = 12 / count;
    float s = 0.0;
    for( int i = 0; i < count; i++ )
        s += dist - 0.04 <= spotLinear( texture( spotShadow, q.xy + Poisson[i * stride] * 0.0016 ).r ) ? 1.0 : 0.0;
    return s / float( count );
}

vec3 spotLight( vec3 wp, out vec3 L )
{
    vec3 lv = spotPos - wp;
    float d = length( lv );
    L = lv / max( d, 1e-4 );
    float cone = smoothstep( spotCos.x, spotCos.y, dot( -L, spotDir ) );
    // Nothing within arm's length: the lamp sits on the hero's shoulder and must not light the rifle.
    return spotColor * cone * atten( d, spotRange ) * smoothstep( 0.7, 1.6, d );
}

float hg( float c, float g ) { return ( 1.0 - g * g ) / ( 4.0 * PI * pow( 1.0 + g * g - 2.0 * g * c, 1.5 ) ); }

// Light thrown through water: a moving web of bright lines on what lies near a ball of water (and inside it).
vec3 caustics( vec3 p )
{
    vec3 c = vec3( 0.0 );
    for( int i = 0; i < waterCount; i++ )
    {
        float d = length( p - waterBalls[i].xyz ) - waterBalls[i].w;
        if( d > 7.0 ) continue;
        vec3 q = vec3( p.x * 0.9 + time * 0.31, p.y * 0.5 - time * 0.22, p.z * 0.9 - time * 0.27 );
        float n1 = noise3( q );
        float n2 = noise3( q * 1.9 + vec3( 3.1, time * 0.4, 1.7 ) );
        float web = pow( 1.0 - abs( n1 - n2 ), 12.0 );
        c += vec3( 0.25, 0.75, 0.85 ) * web * smoothstep( 7.0, 0.0, d ) * ( d < 0.0 ? 1.6 : 1.0 );
    }
    return c;
}

// Where the segment from `from` to `to` runs inside the room's box (the fog fills it): along dir from tn to tf.
bool fogSpan( vec3 from, vec3 to, out vec3 dir, out float tn, out float tf )
{
    vec3 seg = to - from;
    float len = length( seg );
    dir = seg / max( len, 1e-4 );
    vec3 inv = 1.0 / ( abs( dir ) + 1e-6 ) * sign( dir + 1e-7 );
    vec3 t0 = ( roomMin - from ) * inv;
    vec3 t1 = ( roomMax - from ) * inv;
    vec3 tmin = min( t0, t1 );
    vec3 tmax = max( t0, t1 );
    tn = max( max( tmin.x, tmin.y ), max( tmin.z, 0.0 ) );
    tf = min( min( tmax.x, tmax.y ), min( tmax.z, len ) );
    return tf > tn;
}

// How much of what lies at `to` comes through the fog to `from`, without the fog's noise (its mean): for what is
// drawn over the fog pass (glass, water), which that pass does not see.
float fogTransmittance( vec3 from, vec3 to )
{
    vec3 dir;
    float tn, tf;
    if( !fogSpan( from, to, dir, tn, tf ) ) return 1.0;
    float dt = ( tf - tn ) * 0.25;
    float depth = 0.0;
    for( int i = 0; i < 4; i++ )
    {
        vec3 p = from + dir * ( tn + ( float( i ) + 0.5 ) * dt );
        depth += ( fogParams.x + fogParams.y * exp( -max( p.y, 0.0 ) * fogParams.z ) ) * dt;
    }
    return exp( -depth );
}
)";

/**
 * @brief Walls between the camera and the hero dissolve in a cone around the line of sight (ordered dither); leaves
 *        right at the camera too. Shared by the lit surfaces and the depth pre-pass, so both leave out the same pixels.
 */
inline const char *CutAwayFs = R"(
uniform float cutaway;     // 1: dissolves between the camera and the hero (2: and right at the camera)
uniform vec3 cutCentre;    // the hero (chest)
uniform float cutRadius;   // radius of the hole at the hero
void cutAway( vec3 p, vec3 eye, bool nearToo )
{
    vec3 toHero = cutCentre - eye;
    float dist = length( toHero );
    vec3 dir = toHero / dist;
    vec3 rel = p - eye;
    float t = dot( rel, dir );
    const float Bayer[16] = float[]( 0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0 );
    ivec2 q = ivec2( gl_FragCoord.xy ) & 3;
    float dither = ( Bayer[q.y * 4 + q.x] + 0.5 ) / 16.0;
    // Leaves right at the camera (a crown it has come into) go too: they would fill the picture.
    if( nearToo && smoothstep( 0.8, 2.0, length( rel ) ) < dither ) discard;
    if( cutRadius < 0.01 || t <= 0.0 || t >= dist - 0.6 || p.y < cutCentre.y - 0.9 ) return;
    float r = cutRadius * t / dist;
    float d = length( rel - dir * t );
    float keep = smoothstep( r * 0.65, r, d );
    if( keep < dither ) discard;
}
)";

/**
 * @brief The depth pre-pass of the solid surfaces (LitVs or LitSkinVs before it): only the cut-away's holes; the lit
 *        pass then shades each pixel once (depth EQUAL, no writes).
 */
inline const char *PrepassFs = R"(
invariant in vec3 fragPos;
uniform vec3 viewPos;
out vec4 finalColor;
void main()
{
    if( cutaway > 0.5 ) cutAway( fragPos, viewPos, cutaway > 1.5 );
    finalColor = vec4( 1.0 );
}
)";

/** @brief Lit surfaces: triplanar albedo/ORM/normal (or UV), macro variation, emission (the fog is its own pass). */
inline const char *LitFs = R"(
invariant in vec3 fragPos;
in vec3 fragNormal;
in vec3 objPos;
in vec3 objNormal;
in vec4 fragColor;
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D emissionMap;
uniform vec4 colDiffuse;
uniform mat4 matNormal;
uniform float uvScale;
uniform vec4 matParams;    // roughness scale, metallic scale, normal strength, mapping (0 triplanar, 1 UV, 2 clothed, 3 decal, 4 stain, 5 garment)
uniform vec4 drawTint;     // clothed: the clothes' colour, the height (rest pose) the skin shows above
uniform vec4 drawCloth;    // clothed: the trousers' colour (x < 0: as drawTint); w the cut: pattern + 10 sleeves rolled up + 20 bare hands
uniform vec4 emissive;     // rgb, mode (0 plain, 1 grooves glow, 2 emission map)
uniform vec2 emissiveMask; // the grooves' smoothstep
uniform float variation;
uniform float fill;        // figures: the fill light's strength (0: none, the level's own geometry)
uniform float splat;       // 1: two more materials blended in by a slow noise (the ground and the rock out in the open); 2 a patchwork, 3 the ground by its shapes (Gfx.h, Surface::m_layerMode)
uniform float sway;        // plants: 2 leaf cards, 3 fir branches, 4 leaves that do not move: lit softly (Gfx.h, Surface)
uniform sampler2D layer1Albedo;   // the second material's maps
uniform sampler2D layer1Orm;
uniform sampler2D layer1Normal;
uniform sampler2D layer2Albedo;   // the third's
uniform sampler2D layer2Orm;
uniform sampler2D layer2Normal;
uniform float envOn;              // 1: the level's cube map (Gfx::environment) is what reflects
uniform samplerCube envMap;
uniform vec4 wetInfo;             // the wet map: 1 / the level's width and depth (m), on (Gfx::wetMap)
uniform sampler2D wetMap;         // per cell: how wet (puddles), its floor's height ((h + 4) / 16), how polished
out vec4 finalColor;

void triplanar( vec3 p, vec3 n, out vec3 albedo, out vec3 orm, out vec3 nrm )
{
    vec3 bw = pow( abs( n ), vec3( 6.0 ) );
    bw /= ( bw.x + bw.y + bw.z );
    vec3 s = vec3( n.x < 0.0 ? -1.0 : 1.0, n.y < 0.0 ? -1.0 : 1.0, n.z < 0.0 ? -1.0 : 1.0 );
    vec2 ux = vec2( p.z * s.x, -p.y );
    vec2 uy = vec2( p.x * s.y, -p.z );
    vec2 uz = vec2( -p.x * s.z, -p.y );
    albedo = toLinear( texture( texture0, ux ).rgb ) * bw.x + toLinear( texture( texture0, uy ).rgb ) * bw.y +
             toLinear( texture( texture0, uz ).rgb ) * bw.z;
    orm = texture( texture1, ux ).rgb * bw.x + texture( texture1, uy ).rgb * bw.y + texture( texture1, uz ).rgb * bw.z;
    vec3 tx = texture( texture2, ux ).xyz * 2.0 - 1.0;
    vec3 ty = texture( texture2, uy ).xyz * 2.0 - 1.0;
    vec3 tz = texture( texture2, uz ).xyz * 2.0 - 1.0;
    tx.xy *= matParams.z;
    ty.xy *= matParams.z;
    tz.xy *= matParams.z;
    tx.x *= s.x;
    ty.x *= s.y;
    tz.x *= -s.z;
    tx = vec3( tx.xy + n.zy, abs( tx.z ) * n.x );
    ty = vec3( ty.xy + n.xz, abs( ty.z ) * n.y );
    tz = vec3( tz.xy + n.xy, abs( tz.z ) * n.z );
    nrm = normalize( tx.zyx * bw.x + ty.xzy * bw.y + tz.xyz * bw.z );
}

// The second and third material's tiles: turned and scaled their own ways, so that the three never line up.
const mat2 Turn1 = mat2( 0.6216, 0.7833, -0.7833, 0.6216 );    // 0.9 rad
const mat2 Turn2 = mat2( -0.1288, -0.9917, 0.9917, -0.1288 );  // -1.7 rad

// One material's maps on one projection, weighed into the sums (its normal turned back as its tiles were turned).
void layerTap( sampler2D a, sampler2D o, sampler2D nm, vec2 uv, vec2 dx, vec2 dy, mat2 back, float w,
               inout vec3 alb, inout vec3 rm, inout vec3 tn )
{
    alb += toLinear( textureGrad( a, uv, dx, dy ).rgb ) * w;
    rm += textureGrad( o, uv, dx, dy ).rgb * w;
    vec3 t = textureGrad( nm, uv, dx, dy ).xyz * 2.0 - 1.0;
    t.xy = back * t.xy;
    tn += t * w;
}

// The three materials on one projection, by their shares (a share too small to see is not sampled). A patchwork's
// variants lie as the plain one does (a plate's grid stays square); the ground's are turned and scaled.
void splatTap( vec2 uv, vec2 dx, vec2 dy, vec3 mw, bool turn, out vec3 alb, out vec3 rm, out vec3 tn )
{
    alb = vec3( 0.0 );
    rm = vec3( 0.0 );
    tn = vec3( 0.0 );
    if( mw.x > 0.0 ) layerTap( texture0, texture1, texture2, uv, dx, dy, mat2( 1.0 ), mw.x, alb, rm, tn );
    if( mw.y > 0.0 )
    {
        if( turn )
            layerTap( layer1Albedo, layer1Orm, layer1Normal, Turn1 * uv * 0.87 + vec2( 0.37, 0.71 ), Turn1 * dx * 0.87,
                      Turn1 * dy * 0.87, transpose( Turn1 ), mw.y, alb, rm, tn );
        else
            layerTap( layer1Albedo, layer1Orm, layer1Normal, uv, dx, dy, mat2( 1.0 ), mw.y, alb, rm, tn );
    }
    if( mw.z > 0.0 )
    {
        if( turn )
            layerTap( layer2Albedo, layer2Orm, layer2Normal, Turn2 * uv * 1.13 + vec2( 0.53, 0.19 ), Turn2 * dx * 1.13,
                      Turn2 * dy * 1.13, transpose( Turn2 ), mw.z, alb, rm, tn );
        else
            layerTap( layer2Albedo, layer2Orm, layer2Normal, uv, dx, dy, mat2( 1.0 ), mw.z, alb, rm, tn );
    }
}

// Triplanar as above, of three materials blended by a slow noise of the place (wp: metres): wide soft patches, no
// hard edges, no tile repeating. Only the projections that count are sampled (flat ground: the top one alone), with
// their gradients taken before (so the branches may skip them).
void splatted( vec3 p, vec3 wp, vec3 n, out vec3 albedo, out vec3 orm, out vec3 nrm )
{
    vec3 bw = pow( abs( n ), vec3( 6.0 ) );
    bw /= ( bw.x + bw.y + bw.z );
    bw *= step( 0.02, bw );
    bw /= ( bw.x + bw.y + bw.z );
    vec3 s = vec3( n.x < 0.0 ? -1.0 : 1.0, n.y < 0.0 ? -1.0 : 1.0, n.z < 0.0 ? -1.0 : 1.0 );
    vec2 ux = vec2( p.z * s.x, -p.y );
    vec2 uy = vec2( p.x * s.y, -p.z );
    vec2 uz = vec2( -p.x * s.z, -p.y );
    vec2 dxx = dFdx( ux ), dyx = dFdy( ux ), dxy = dFdx( uy ), dyy = dFdy( uy ), dxz = dFdx( uz ), dyz = dFdy( uz );
    vec3 mw;
    bool turn = splat < 1.5 || splat > 2.5;
    if( !turn )
    {
        // The patchwork: the vertex names its material (u's whole fours: 0 the plain one, 1 and 2 its variants).
        float pick = floor( fragTexCoord.x * 0.25 + 0.1 );
        mw = vec3( step( pick, 0.5 ), step( 0.5, pick ) * step( pick, 1.5 ), step( 1.5, pick ) );
    }
    else
    {
        float a = noise3( vec3( wp.xz * 0.09, 0.5 ) ) * 0.7 + noise3( vec3( wp.xz * 0.37, 1.5 ) ) * 0.3;
        float b = noise3( vec3( wp.xz * 0.07 + 31.0, 2.5 ) ) * 0.7 + noise3( vec3( wp.xz * 0.31 + 9.0, 3.5 ) ) * 0.3;
        float w1 = smoothstep( 0.5, 0.62, a ), w2 = smoothstep( 0.52, 0.64, b );
        if( splat > 2.5 )
        {
            // The ground's shapes: trodden along the ways (u), scree at the rock's foot (v), their rims ragged by a
            // finer noise; out in the open a few patches of each by the slow noise as well.
            float rag = noise3( vec3( wp.xz * 0.9, 5.5 ) ) - 0.5;
            vec2 shape = clamp( fragTexCoord, 0.0, 1.0 );
            w1 = max( smoothstep( 0.56, 0.68, a ) * 0.75, smoothstep( 0.2, 0.6, shape.x + rag * 0.45 ) );
            w2 = max( smoothstep( 0.6, 0.7, b ) * 0.6, smoothstep( 0.15, 0.55, shape.y + rag * 0.5 ) );
        }
        mw = vec3( ( 1.0 - w1 ) * ( 1.0 - w2 ), w1 * ( 1.0 - w2 ), w2 );
    }
    mw *= step( 0.02, mw );
    mw /= ( mw.x + mw.y + mw.z );
    vec3 ax = vec3( 0.0 ), ay = vec3( 0.0 ), az = vec3( 0.0 ), ox = vec3( 0.0 ), oy = vec3( 0.0 ), oz = vec3( 0.0 );
    vec3 tx = vec3( 0.0, 0.0, 1.0 ), ty = tx, tz = tx;
    if( bw.x > 0.0 ) splatTap( ux, dxx, dyx, mw, turn, ax, ox, tx );
    if( bw.y > 0.0 ) splatTap( uy, dxy, dyy, mw, turn, ay, oy, ty );
    if( bw.z > 0.0 ) splatTap( uz, dxz, dyz, mw, turn, az, oz, tz );
    albedo = ax * bw.x + ay * bw.y + az * bw.z;
    orm = ox * bw.x + oy * bw.y + oz * bw.z;
    tx.xy *= matParams.z;
    ty.xy *= matParams.z;
    tz.xy *= matParams.z;
    tx.x *= s.x;
    ty.x *= s.y;
    tz.x *= -s.z;
    tx = vec3( tx.xy + n.zy, abs( tx.z ) * n.x );
    ty = vec3( ty.xy + n.xz, abs( ty.z ) * n.y );
    tz = vec3( tz.xy + n.xy, abs( tz.z ) * n.z );
    nrm = normalize( tx.zyx * bw.x + ty.xzy * bw.y + tz.xyz * bw.z );
}

void main()
{
    if( cutaway > 0.5 ) cutAway( fragPos, viewPos, cutaway > 1.5 );
    vec3 n0 = normalize( objNormal );
    vec3 albedo;
    vec3 orm;
    vec3 nObj;
    float alpha = 1.0;
    if( matParams.w < 0.5 && splat > 0.5 )
        splatted( objPos * uvScale, objPos, n0, albedo, orm, nObj );
    else if( matParams.w < 0.5 )
        triplanar( objPos * uvScale, n0, albedo, orm, nObj );
    else
    {
        vec2 uv = fragTexCoord * uvScale;
        vec4 tex = texture( texture0, uv );
        albedo = toLinear( tex.rgb );
        // A crown's leaf cards and a fir's branches: their colour from a coarser level of the map, a little flatter
        // (the branches' fine needles speckled at the Deck's size); the leaves are large painted ones since phase F1, a
        // level down keeps their shapes. The clumps' light and dark come from the shading below.
        float coarse = sway < 2.5 ? 1.0 : 2.5;
        if( sway > 1.5 && sway < 3.5 && matParams.w > 5.5 && matParams.w < 6.5 )
            albedo = toLinear( texture( texture0, uv, coarse ).rgb ) * 0.9 + albedo * 0.1;
        orm = texture( texture1, uv ).rgb;
        nObj = n0;
        // A cut-out (hair, brows, lashes): what is clear in the texture is not there. A crown's leaf card takes its
        // outline from the same coarser level as its colour: the outline of its leaves, softened (cut a little lower).
        float cover = tex.a, cutAt = 0.5;
        if( sway > 1.5 && sway < 2.5 )
        {
            cover = texture( texture0, uv, coarse ).a;
            cutAt = 0.42;
        }
        if( matParams.w > 5.5 && matParams.w < 6.5 && cover < cutAt ) discard;
        // Tinted (a dressed body's clothes and armour): the person's or the faction's colour over the texture.
        if( matParams.w > 6.5 ) albedo *= drawTint.rgb;
        // A decal: blended by its alpha over what is under it (nothing written where it is empty); a stain multiplies
        // what is under it (the blending does it: destination times this).
        if( matParams.w > 2.5 && matParams.w < 4.5 )
        {
            alpha = tex.a;
            if( alpha < 0.03 ) discard;
            if( matParams.w > 3.5 )
            {
                // Its alpha: how much it darkens, for the fog pass (Gfx::fog darkens the fog's light there too).
                vec3 m = mix( vec3( 1.0 ), albedo, alpha * 0.65 );
                finalColor = vec4( m, dot( m, vec3( 1.0 / 3.0 ) ) );
                return;
            }
        }
    }
    bool garment = matParams.w > 4.5 && matParams.w < 5.5;
    float cut = max( drawCloth.w, 0.0 );
    float hands = step( 19.5, cut );
    float rolled = step( 9.5, cut - hands * 20.0 );
    float pattern = cut - hands * 20.0 - rolled * 10.0;
    float ax = abs( objPos.x );
    // Rolled-up sleeves and bare hands leave the body's skin (the arms reach out along x in the rest pose).
    bool bare = ( rolled > 0.5 && ax > 0.5 ) || ( hands > 0.5 && ax > 0.64 );
    // Below the neck, and the shoulders' tops beside it (they reach above the neck's height in the rest pose).
    bool clothed = objPos.y < drawTint.w || ( ax > 0.11 && objPos.y < drawTint.w + 0.1 );
    if( garment || ( matParams.w > 1.5 && matParams.w < 2.5 && clothed && !bare ) )
    {
        // Clothes over the body (below the neck in the rest pose) and the garments over them: the colour with a weave
        // in it, the jacket's pattern; boots, a belt and gloves darker. The trousers have their own colour.
        bool legs = !garment && objPos.y < 0.95 && ax < 0.4;
        vec3 colour = legs && drawCloth.x >= 0.0 ? drawCloth.rgb : drawTint.rgb;
        float weave = noise3( objPos * 60.0 ) * 0.4 + noise3( objPos * 6.0 ) * 0.6;
        float shade = 0.7 + 0.45 * weave;
        if( pattern > 0.5 && pattern < 1.5 && !legs )
            shade *= 0.84 + 0.3 * step( 0.86, fract( ( objPos.x + objPos.z ) * 42.0 ) );   // pinstripes
        else if( pattern > 1.5 && pattern < 2.5 && !legs )
        {
            // Checks, as on a flannel shirt.
            float u = step( 0.62, fract( ( objPos.x + objPos.z ) * 11.0 ) ), v = step( 0.62, fract( objPos.y * 11.0 ) );
            shade *= 0.7 + 0.26 * u + 0.26 * v + 0.12 * u * v;
        }
        else if( pattern > 2.5 && pattern < 3.5 && !garment )
        {
            // Reflective bands round the chest, the upper arms and the shins (overalls).
            bool band = ( objPos.y > 1.19 && objPos.y < 1.24 && ax < 0.3 ) || ( ax > 0.37 && ax < 0.41 && objPos.y > 1.25 )
                        || ( objPos.y > 0.4 && objPos.y < 0.45 && ax < 0.3 );
            if( band )
            {
                colour = vec3( 0.84, 0.86, 0.82 );   // silver, it throws the light back
                shade = 1.1 + 0.1 * weave;
            }
        }
        else if( pattern > 3.5 && pattern < 4.5 && !legs )
        {
            // Quilted, as a padded flight jacket.
            float q = min( abs( fract( ( objPos.x + objPos.y + objPos.z ) * 11.0 ) - 0.5 ),
                           abs( fract( ( objPos.x - objPos.y - objPos.z ) * 11.0 ) - 0.5 ) );
            shade *= 0.76 + 0.32 * smoothstep( 0.0, 0.22, q );
        }
        else if( pattern > 4.5 )
        {
            // Worn: patches of other cloth, stains.
            shade *= mix( 0.76, 1.14, hash13( floor( objPos * 7.0 ) ) ) * ( 0.84 + 0.16 * noise3( objPos * 23.0 ) );
        }
        albedo = toLinear( colour ) * shade;
        if( !garment )
        {
            if( objPos.y < 0.24 || ax > 0.64 )
                albedo *= 0.32;   // boots, gloves
            else if( objPos.y > 0.92 && objPos.y < 0.98 )
                albedo *= 0.4;    // the belt
            else if( rolled > 0.5 && ax > 0.47 )
                albedo *= 1.12;   // the rolled cuff
        }
        orm = vec3( 1.0, 0.85, 0.0 );
    }
    albedo *= colDiffuse.rgb * fragColor.rgb;
    float m = noise3( fragPos * 0.35 ) * 0.65 + noise3( fragPos * 1.7 ) * 0.35;
    albedo *= mix( 1.0, 0.65 + 0.7 * m, variation );
    float rough = clamp( orm.g * matParams.x + ( 0.5 - m ) * 0.25 * variation, 0.04, 1.0 );
    float metal = clamp( orm.b * matParams.y, 0.0, 1.0 );
    float ao = mix( 1.0, orm.r, 0.8 ) * fragColor.a;
    vec3 N = normalize( mat3( matNormal ) * nObj );
    vec3 Ng = normalize( fragNormal );
    vec3 V = normalize( viewPos - fragPos );
    // Seen from behind (a cavern from inside, nothing is culled): light the face that is seen. Not a crown's leaves:
    // their normal is the crown's, whichever side of the card shows.
    if( dot( Ng, V ) < 0.0 && sway < 1.5 )
    {
        N = -N;
        Ng = -Ng;
    }
    vec3 col = vec3( 0.0 );

    // Wet floors and polished ones (the level's wet map; only what faces up at its cell's floor): puddles in a slow
    // noise of the place, darker, smooth and flat, mirroring the lamps; a damp rim round them; a polished floor smoother
    // all over.
    float puddle = 0.0;
    if( wetInfo.z > 0.5 && matParams.w < 0.5 && Ng.y > 0.85 )
    {
        vec4 w = texture( wetMap, fragPos.xz * wetInfo.xy );
        if( fragPos.y < w.g * 16.0 - 3.75 && w.r + w.b > 0.02 )
        {
            // Few puddles, where the water gathers (the map's alpha: by the walls and the drains, in low spots; hardly
            // in the middle of a room), each with a soft damp rim (a noise over the whole floor read as camouflage).
            float n = noise3( vec3( fragPos.xz * 0.6, 7.0 ) ) * 0.65 + noise3( vec3( fragPos.xz * 2.2, 3.0 ) ) * 0.35;
            float t = 0.87 - 0.36 * w.a * w.r;
            float damp = smoothstep( t - 0.16, t + 0.02, n ) * step( 0.02, w.r );
            puddle = smoothstep( t, t + 0.05, n ) * step( 0.02, w.r );
            albedo *= mix( 1.0, 0.8, damp ) * mix( 1.0, 0.55, puddle );
            rough = mix( rough * ( 1.0 - 0.6 * w.b ), rough * 0.55, damp );
            rough = mix( rough, 0.18, puddle );
            N = normalize( mix( N, Ng, puddle ) );
        }
    }

    float ndl = max( dot( Ng, -sunDir ), 0.0 );
    bool leafy = sway > 1.5;
    if( leafy )
    {
        // A crown reads as a few clumps with darker gaps between them (a slow noise through it), not as one blob.
        if( sway < 2.5 )
        {
            float clump = noise3( fragPos * 1.3 + vec3( 3.1 ) ) * 0.6 + noise3( fragPos * 2.9 ) * 0.4;
            albedo *= mix( 0.38, 1.12, smoothstep( 0.32, 0.62, clump ) );
        }
        // Leaves lit as the crown they stand for: its normal (from its middle), the light wrapped round it and a little
        // through it, the sun's shadow looked up outside the crown (the dark core's own would speckle the cards); no
        // gloss. A crown then reads as a soft volume, lit above and dark below, not as noise.
        float wrap = clamp( dot( N, -sunDir ) * 0.6 + 0.4, 0.0, 1.0 );
        float through = 0.25 * clamp( dot( V, sunDir ), 0.0, 1.0 );
        col += albedo / PI * sunColor * ( wrap + through ) *
               sunShadowAt( fragPos + Ng * 0.3, Ng, max( wrap, 0.3 ), shadowTaps ) * cloudShade( fragPos );
        rough = 1.0;
        metal = 0.0;
    }
    else if( dot( N, -sunDir ) > 0.0 )
        col += brdf( N, V, -sunDir, albedo, rough, metal ) * sunColor * sunShadowAt( fragPos, Ng, ndl, shadowTaps ) *
               cloudShade( fragPos );
    // A figure's fill light from the side away from the sun, half as steep (as Dark Alliance's engine gives its
    // models; no shadow): the side turned from the sun does not sink into black.
    if( fill > 0.0 )
    {
        vec3 Lf = normalize( vec3( sunDir.x, -sunDir.y * 0.5, sunDir.z ) );
        col += brdf( N, V, Lf, albedo, rough, metal ) * ( sunColor + ambUp * 2.0 ) * fill;
    }
    vec3 L;
    vec3 sc = spotLight( fragPos, L );
    if( sc.r + sc.g + sc.b > 0.0 ) col += brdf( N, V, L, albedo, rough, metal ) * sc * spotShadowAt( fragPos, Ng, shadowTaps );
    // The fixed lights stop at the walls (their masks, looked up a little out of a wall's face so that it takes the
    // light of its own side).
    vec3 mp = fragPos + vec3( Ng.x, 0.0, Ng.z ) * 0.25;
    vec3 Rp = reflect( -V, N );
    vec3 mirror = vec3( 0.0 );
    for( int i = 0; i < lightCount; i++ )
    {
        vec3 lv = lightPos[i].xyz - fragPos;
        float d = length( lv );
        if( d < lightPos[i].w )
        {
            float vis = lightVis( i, mp );
            col += brdf( N, V, lv / d, albedo, rough, metal ) * lightColor[i].rgb * atten( d, lightPos[i].w ) * vis;
            // A face looking up over the lamp (a wall's lid, a cupboard's top) takes some of what the lamp throws on
            // the ceiling (phase F3: the walls' lids were black: the lamps hang below them), past the wall only from
            // its room's side (its mask looked up 0.6 m towards it).
            if( Ng.y > 0.6 && lv.y < -0.3 )
                col += albedo * ( 1.0 - metal ) * ( 0.12 / PI ) * Ng.y * lightColor[i].rgb * atten( d, lightPos[i].w ) *
                       lightVis( i, fragPos + vec3( lv.x, 0.0, lv.z ) / max( length( lv.xz ), 1e-3 ) * 0.6 );
            // A puddle mirrors the lamp where the reflected ray passes it: a small bright image and a halo round it
            // (by the angle the ray misses it by, so that a far lamp's image is small).
            float tc = dot( lv, Rp );
            if( puddle > 0.0 && tc > 0.0 )
            {
                float miss = length( lv - Rp * tc ) / max( tc, 0.3 );
                float image = 3.0 * exp( -miss * miss * 600.0 ) + 0.25 * exp( -miss * miss * 10.0 );
                mirror += lightColor[i].rgb * vis * image / ( 1.0 + tc * tc * 0.05 );
            }
        }
    }
    col += mirror * puddle * 0.02;

    // Ambient: a hemisphere for the diffuse part; what reflects mirrors the level's surroundings (its cube map, blurrier
    // the rougher it is), or a fake environment where there is none.
    vec3 amb = mix( ambDown, ambUp, N.y * 0.5 + 0.5 );
    vec3 F0 = mix( vec3( 0.04 ), albedo, metal );
    float NdV = max( dot( N, V ), 0.0 );
    vec3 Fr = F0 + ( max( vec3( 1.0 - rough ), F0 ) - F0 ) * pow( 1.0 - NdV, 5.0 );
    vec3 R = reflect( -V, N );
    vec3 env = envOn > 0.5 ? textureLod( envMap, R, rough * 5.0 ).rgb : mix( ambDown, ambUp, R.y * 0.5 + 0.5 ) * 3.0;
    col += ( amb * albedo * ( 1.0 - metal ) + env * Fr ) * ao;
    if( waterCount > 0 ) col += albedo * caustics( fragPos ) * 1.4 * ao;

    vec3 em = emissive.rgb;
    if( emissive.w > 0.5 && emissive.w < 1.5 )
        em *= smoothstep( emissiveMask.x, emissiveMask.y, 1.0 - orm.r );
    else if( emissive.w > 1.5 )
        em *= toLinear( texture( emissionMap, fragTexCoord ).rgb );
    col += em;
    finalColor = vec4( col, alpha );
}
)";

/** @brief The glass floor: smudges and scratches catch the light, the lights mirror in it; premultiplied alpha. As ice
 *         (iceMode 1): frost in drifts and cracks, a pale cyan body one still sees through. */
inline const char *GlassFs = R"(
uniform float iceMode;
invariant in vec3 fragPos;
in vec3 fragNormal;
in vec3 objPos;
in vec3 objNormal;
in vec4 fragColor;
in vec2 fragTexCoord;
out vec4 finalColor;

float ggx( vec3 N, vec3 V, vec3 L, float rough )
{
    vec3 H = normalize( V + L );
    float NdH = max( dot( N, H ), 0.0 );
    float NdL = max( dot( N, L ), 0.0 );
    float a2 = rough * rough * rough * rough;
    float d = NdH * NdH * ( a2 - 1.0 ) + 1.0;
    return a2 / ( PI * d * d ) * NdL * 0.25;
}

void main()
{
    vec3 N = vec3( 0.0, 1.0, 0.0 );
    vec3 V = normalize( viewPos - fragPos );
    float NdV = max( dot( N, V ), 0.0 );
    float F = 0.04 + 0.96 * pow( 1.0 - NdV, 5.0 );
    vec2 p = fragPos.xz;
    float smudge = smoothstep( 0.42, 0.85, noise3( vec3( p * 0.55, 1.3 ) ) * 0.7 + noise3( vec3( p * 2.3, 4.1 ) ) * 0.3 );
    vec2 q = p * 2.5;
    float line = abs( fract( q.x * 0.7 + q.y * 0.31 + noise3( vec3( q * 0.4, 2.0 ) ) * 2.0 ) - 0.5 );
    float scratch = smoothstep( 0.015, 0.0, line ) * step( 0.62, noise3( vec3( q * 0.7, 7.0 ) ) );
    float dirt = clamp( smudge * 0.55 + scratch * 0.22, 0.0, 1.0 );
    float frost = 0.0;
    if( iceMode > 0.5 )
    {
        // Frost blown into drifts, white cracks running through the clear: the glow below shows between them.
        frost = smoothstep( 0.5, 0.9, noise3( vec3( p * 0.35, 3.7 ) ) * 0.6 + noise3( vec3( p * 1.7, 8.2 ) ) * 0.4 );
        float crack = abs( fract( noise3( vec3( p * 0.6, 5.5 ) ) * 4.0 ) - 0.5 );
        frost = clamp( frost + smoothstep( 0.03, 0.0, crack ) * 0.22, 0.0, 1.0 );
        dirt = frost;
    }
    // Ice is rougher than a window: the lamps smear across it instead of standing in it as mirror images.
    float rough = iceMode > 0.5 ? 0.24 + frost * 0.3 : 0.05 + dirt * 0.25;

    vec3 diff = mix( ambDown, ambUp, 0.8 );
    vec3 spec = vec3( 0.0 );
    float sh = sunShadowAt( fragPos, N, max( -sunDir.y, 0.0 ), shadowTaps );
    diff += sunColor * max( -sunDir.y, 0.0 ) * sh;
    spec += ggx( N, V, -sunDir, rough ) * sunColor * sh;
    vec3 L;
    vec3 sc = spotLight( fragPos, L );
    if( sc.r + sc.g + sc.b > 0.0 )
    {
        float ss = spotShadowAt( fragPos, N, shadowTaps );
        diff += sc * max( L.y, 0.0 ) * ss;
        spec += ggx( N, V, L, rough ) * sc * ss;
    }
    for( int i = 0; i < lightCount; i++ )
    {
        vec3 lv = lightPos[i].xyz - fragPos;
        float d = length( lv );
        if( d >= lightPos[i].w ) continue;
        vec3 l = lv / d;
        float at = atten( d, lightPos[i].w ) * lightVis( i, fragPos );
        diff += lightColor[i].rgb * at * max( l.y, 0.0 );
        spec += ggx( N, V, l, rough ) * lightColor[i].rgb * at;
    }
    // Smudges catch a little of the lamps' light (too much, and a window over the dark turns to frosted glass).
    vec3 c = spec * F * 3.0 + min( diff, vec3( 4.0 ) ) * dirt * 0.012 * vec3( 0.85, 0.9, 1.0 ) +
             mix( ambDown, ambUp, 0.5 ) * F * 2.0;
    float alpha = clamp( 0.06 + F * 0.45 + dirt * 0.3, 0.0, 0.95 );
    if( iceMode > 0.5 )
    {
        vec3 body = vec3( 0.45, 0.68, 0.85 ) * ( min( diff, vec3( 2.0 ) ) * 0.012 + mix( ambDown, ambUp, 0.5 ) * 0.5 );
        c = c * 0.12 + body * ( 0.06 + frost * 0.35 );
        alpha = clamp( 0.05 + F * 0.15 + frost * 0.22, 0.0, 0.8 );
    }
    finalColor = vec4( c * fogTransmittance( viewPos, fragPos ), alpha );
}
)";

/** @brief A ball of water floating in low gravity: rippled skin, reflections at the rim, the depths dark green, a
 *         shimmer of caustics inside; premultiplied alpha. */
inline const char *WaterFs = R"(
invariant in vec3 fragPos;
in vec3 fragNormal;
in vec3 objPos;
in vec3 objNormal;
in vec4 fragColor;
in vec2 fragTexCoord;
out vec4 finalColor;

void main()
{
    vec3 N = normalize( fragNormal );
    vec3 V = normalize( viewPos - fragPos );
    if( dot( N, V ) < 0.0 ) N = -N;
    vec3 q = fragPos * 1.4 + vec3( time * 0.6, time * 0.4, -time * 0.5 );
    N = normalize( N + 0.18 * vec3( noise3( q ) - 0.5, noise3( q + 7.3 ) - 0.5, noise3( q + 13.1 ) - 0.5 ) );
    float NdV = max( dot( N, V ), 0.0 );
    float F = 0.02 + 0.98 * pow( 1.0 - NdV, 5.0 );
    vec3 spec = vec3( 0.0 );
    for( int i = 0; i < lightCount; i++ )
    {
        vec3 lv = lightPos[i].xyz - fragPos;
        float d = length( lv );
        if( d >= lightPos[i].w ) continue;
        vec3 H = normalize( V + lv / d );
        spec += lightColor[i].rgb * atten( d, lightPos[i].w ) * pow( max( dot( N, H ), 0.0 ), 120.0 ) * 2.0 *
                lightVis( i, fragPos );
    }
    vec3 L;
    vec3 sc = spotLight( fragPos, L );
    if( sc.r + sc.g + sc.b > 0.0 ) spec += sc * pow( max( dot( N, normalize( V + L ) ), 0.0 ), 120.0 ) * 2.0;
    vec3 env = mix( ambDown, ambUp, reflect( -V, N ).y * 0.5 + 0.5 ) * 4.0;
    vec3 body = vec3( 0.02, 0.09, 0.1 ) * ( 0.6 + caustics( fragPos ).g * 2.0 );
    vec3 c = body * ( 1.0 - F ) + ( env + spec ) * F + spec * 0.3;
    float alpha = clamp( 0.22 + F * 0.6, 0.0, 0.9 );
    finalColor = vec4( c * fogTransmittance( viewPos, fragPos ) * alpha, alpha );
}
)";

/**
 * @brief A pond in a garden (a disc over the ground's hollow, texture coordinates -1 .. 1 across it): the wind's small
 *        waves, rings where a drop falls and round what wades in it, the sky and the sun mirrored, the body dark and
 *        deeper darker, fading out towards the shore (the bank rises through it); premultiplied alpha.
 */
inline const char *PondFs = R"(
invariant in vec3 fragPos;
in vec3 fragNormal;
in vec3 objPos;
in vec3 objNormal;
in vec4 fragColor;
in vec2 fragTexCoord;
uniform vec4 ripples[4];   // what wades: x, z, strength
uniform int rippleCount;
uniform vec3 pondSky;      // the sky's colour (linear) it mirrors
uniform float pondShallow; // 1: shallow water over the lawn (the ground shows through; a damp rim; splashes)
out vec4 finalColor;

// The slope of a ring travelling out from a point, at distance d from it (its front at r).
float ring( float d, float r, float width )
{
    float x = ( d - r ) / width;
    return -x * exp( -x * x );
}

void main()
{
    vec2 p = fragPos.xz;
    float depth = clamp( 1.0 - dot( fragTexCoord, fragTexCoord ), 0.0, 1.0 );   // 0 at the shore, 1 in the middle
    // The wind's small waves: four trains of them round its direction, uneven in strength over the pond (their slopes).
    vec2 dir = wind.z > 0.0 ? wind.xy : vec2( 0.8, 0.6 );
    float calm = 0.35 + 0.65 * smoothstep( 0.0, 0.35, depth );
    float patchy = 0.4 + 0.6 * noise3( vec3( p * 0.7 - dir * time * 0.4, time * 0.1 ) );
    vec2 slope = vec2( 0.0 );
    for( int i = 0; i < 4; i++ )
    {
        float a = ( float( i ) - 1.5 ) * 0.55, k = 4.0 + 3.1 * float( i );
        vec2 d = vec2( dir.x * cos( a ) - dir.y * sin( a ), dir.x * sin( a ) + dir.y * cos( a ) );
        slope += d * k * 0.011 * cos( dot( p, d ) * k - time * sqrt( 9.8 * k ) * 0.6 + float( i ) * 1.7 );
    }
    slope *= patchy * calm * clamp( 0.5 + wind.z * 0.25, 0.5, 1.6 );
    // A drop now and then (from the leaves above, a midge): a ring in each cell of 1.3 m, at its own time (not in the
    // shallows over the lawn: what wades through them makes their rings).
    vec2 cell = floor( p / 1.3 );
    float period = 2.5 + 4.0 * hash13( vec3( cell, 3.0 ) );
    float age = fract( time / period + hash13( vec3( cell, 7.0 ) ) ) * period;
    vec2 at = ( cell + 0.3 + 0.4 * vec2( hash13( vec3( cell, 11.0 ) ), hash13( vec3( cell, 13.0 ) ) ) ) * 1.3;
    vec2 dv = p - at;
    float dist = length( dv );
    slope += dv / max( dist, 1e-3 ) * ring( dist, 0.03 + age * 0.3, 0.025 + 0.015 * age ) * exp( -age * 1.4 ) *
             smoothstep( 0.55, 0.3, dist ) * 0.5 * calm * step( pondShallow, 0.5 );
    // What wades: rings going out from it, one after another; in shallow water a splash at the feet of what moves
    // through it (white flecks round them, more the faster).
    float foam = 0.0;
    for( int i = 0; i < rippleCount; i++ )
    {
        vec2 w = p - ripples[i].xy;
        float d = length( w ), s = 0.0;
        for( int k = 0; k < 3; k++ )
        {
            float r = fract( time * 0.55 + float( k ) / 3.0 );
            s += ring( d, 0.2 + r * 1.3, 0.05 ) * ( 1.0 - r );
        }
        slope += w / max( d, 1e-3 ) * s * ripples[i].z * smoothstep( 1.8, 0.2, d ) * 0.45;
        if( pondShallow > 0.5 )
            foam += ripples[i].w * smoothstep( 0.5, 0.15, d ) *
                    smoothstep( 0.58, 0.82, noise3( vec3( p * 9.0, time * 7.0 ) ) );
    }
    vec3 N = normalize( vec3( -slope.x, 1.0, -slope.y ) );
    vec3 V = normalize( viewPos - fragPos );
    float NdV = max( dot( N, V ), 0.0 );
    // (More than water's 0.02 head on: the small waves must show in the sky they mirror from above too; not much
    // more: seen from the game's height a pond is darker than the grass round it, not a pale sheet.)
    float F = 0.05 + 0.95 * pow( 1.0 - NdV, 5.0 );
    vec3 R = reflect( -V, N );
    float sh = sunShadowAt( fragPos, vec3( 0.0, 1.0, 0.0 ), 1.0, shadowTaps ) * cloudShade( fragPos );
    // The sky it mirrors: paler towards the horizon (steeply, so that each wave's slope shows as a lighter or darker
    // band); where the trees stand over it (their shadow), darker.
    vec3 sky = mix( pondSky * 2.2 + ambUp * 1.5, pondSky * 0.45, smoothstep( 0.55, 1.0, R.y ) ) * ( 0.45 + 0.55 * sh );
    vec3 spec = sunColor * pow( max( dot( R, -sunDir ), 0.0 ), 90.0 ) * 3.0 * sh;
    vec3 L;
    vec3 sc = spotLight( fragPos, L );
    if( sc.r + sc.g + sc.b > 0.0 ) spec += sc * pow( max( dot( R, L ), 0.0 ), 60.0 );
    // (The lamps glint in a pond; the shallows' glints are the sun's: a hand of water over a lawn by day.)
    for( int i = 0; i < ( pondShallow > 0.5 ? 0 : lightCount ); i++ )
    {
        vec3 lv = lightPos[i].xyz - fragPos;
        float d = length( lv );
        if( d < lightPos[i].w )
            spec += lightColor[i].rgb * atten( d, lightPos[i].w ) * pow( max( dot( R, lv / d ), 0.0 ), 60.0 ) *
                    lightVis( i, fragPos );
    }
    // The body: brown-green in the shallows, dark in the deep, lit by the sky and the sun through the water.
    vec3 light = ambUp * 3.0 + sunColor * max( -sunDir.y, 0.0 ) * sh * 0.3;
    if( pondShallow > 0.5 )
    {
        // A hand of water over the lawn: the ground shows through it, darker and browner, light dancing over it where
        // the waves gather it; a soft shore and a damp rim past it (only darkening).
        // Phase F3: its shore and damp rim wander (a noise of the place: no round disc), the water murky and brown,
        // thin enough for the path and the lawn to show through it, the sky in it far weaker (it read glassy and blue).
        float t = length( fragTexCoord ) + ( noise3( vec3( p * 1.4, 2.0 ) ) - 0.5 ) * 0.22 +
                  ( noise3( vec3( p * 4.1, 5.0 ) ) - 0.5 ) * 0.08;
        float water = 1.0 - smoothstep( 0.72, 1.0, t ), rim = 1.0 - smoothstep( 0.95, 1.18, t );
        float dancing = smoothstep( 0.6, 0.88, noise3( vec3( p * 2.3 + slope * 8.0, time * 0.5 ) ) ) * water;
        float a = max( rim * 0.2, water * ( 0.42 + 0.18 * F ) );
        vec3 lit = ambUp * 3.0 + sunColor * max( -sunDir.y, 0.0 ) * sh;
        vec3 mud = vec3( 0.03, 0.024, 0.014 ) * light;
        vec3 cs = mud * water + ( mix( sky, mud * 8.0, 0.5 ) * F * 0.22 + spec * 0.2 ) * water +
                  lit * 0.012 * dancing + lit * 0.35 * min( foam, 1.0 );
        finalColor = vec4( cs * fogTransmittance( viewPos, fragPos ), min( a + 0.35 * min( foam, 1.0 ), 1.0 ) );
        return;
    }
    vec3 body = mix( vec3( 0.036, 0.038, 0.022 ), vec3( 0.004, 0.011, 0.011 ), smoothstep( 0.0, 0.6, depth ) ) * light;
    float alpha = smoothstep( 0.0, 0.3, depth ) * ( 0.78 + 0.2 * F );
    float shine = smoothstep( 0.0, 0.12, depth );
    vec3 c = body * alpha + ( sky * F + spec ) * shine;
    finalColor = vec4( c * fogTransmittance( viewPos, fragPos ), alpha );
}
)";

/**
 * @brief A lamp's soft cone of light in the haze (Gfx::cones; LitVs before it, no depth buffer bound: the scene's depth
 *        is read instead): brightest at the lamp, fading along the cone, faint at its rims, and soft where it meets
 *        what stands in it; added to the picture.
 */
inline const char *ConeFs = R"(
#version 330
invariant in vec3 fragPos;
in vec3 fragNormal;
in vec3 objPos;
in vec3 objNormal;
in vec4 fragColor;
in vec2 fragTexCoord;
uniform sampler2D depthTex;   // the scene's depth
uniform vec2 nearFar;
uniform vec3 viewPos;
uniform float coneGain;       // how strongly the haze shows them (the level's fog)
out vec4 finalColor;
float linear( float d )
{
    float z = d * 2.0 - 1.0;
    return 2.0 * nearFar.x * nearFar.y / ( nearFar.y + nearFar.x - z * ( nearFar.y - nearFar.x ) );
}
void main()
{
    float scene = linear( texelFetch( depthTex, ivec2( gl_FragCoord.xy ), 0 ).r );
    float own = linear( gl_FragCoord.z );
    float soft = clamp( ( scene - own ) / 0.7, 0.0, 1.0 );
    if( soft <= 0.0 ) discard;
    vec3 V = normalize( viewPos - fragPos );
    float facing = abs( dot( normalize( fragNormal ), V ) );
    float v = clamp( fragTexCoord.y, 0.0, 1.0 );
    float along = ( 1.0 - v ) * ( 1.0 - v ) * smoothstep( 0.0, 0.15, v );
    float near = smoothstep( 1.5, 4.0, length( viewPos - fragPos ) );
    vec3 c = pow( fragColor.rgb, vec3( 2.2 ) ) * fragColor.a * coneGain * along * facing * soft * near;
    finalColor = vec4( c, 1.0 );
}
)";

/** @brief Fullscreen vertex shader: the quad's texture coordinates are its place on screen. */
inline const char *FullscreenVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 uv;
void main()
{
    uv = vertexTexCoord;
    gl_Position = vec4( vertexTexCoord * 2.0 - 1.0, 0.0, 1.0 );
}
)";

/** @brief The sky behind the windows and below the glass: the nebula, stars, the sun (the fog pass adds the room's
 *         fog where nothing else is). */
inline const char *SkyFs = R"(
in vec2 uv;
uniform sampler2D texture0;
uniform mat4 invViewProj;
uniform float nebula;
out vec4 finalColor;

vec3 stars( vec3 dir )
{
    vec3 c = vec3( 0.0 );
    for( int layer = 0; layer < 2; layer++ )
    {
        float scale = layer == 0 ? 180.0 : 420.0;
        vec3 g = dir * scale;
        vec3 cell = floor( g );
        float h = hash13( cell + float( layer ) * 17.0 );
        if( h > 0.985 )
        {
            vec3 centre = cell + 0.5 + ( vec3( hash13( cell + 1.7 ), hash13( cell + 4.3 ), hash13( cell + 9.1 ) ) - 0.5 ) * 0.6;
            float d = length( g - centre );
            float b = pow( ( h - 0.985 ) / 0.015, 3.0 ) * ( layer == 0 ? 14.0 : 5.0 );
            vec3 tint = mix( vec3( 0.7, 0.8, 1.0 ), vec3( 1.0, 0.85, 0.7 ), hash13( cell + 3.3 ) );
            c += tint * b * exp( -d * d * 40.0 );
        }
    }
    return c;
}

void main()
{
    vec4 w = invViewProj * vec4( uv * 2.0 - 1.0, 1.0, 1.0 );
    vec3 dir = normalize( w.xyz / w.w - viewPos );
    // Equirectangular around the north-south axis, turned so the nebula's heart lies below the glass.
    vec2 st = vec2( atan( -dir.y, -dir.x ) / ( 2.0 * PI ) + 0.27, acos( clamp( dir.z, -1.0, 1.0 ) ) / PI - 0.17 );
    vec3 c = toLinear( textureLod( texture0, st, 0.0 ).rgb ) * nebula;
    c += stars( dir );
    float s = max( dot( dir, -sunDir ), 0.0 );
    c += sunColor * ( pow( s, 4000.0 ) * 60.0 + pow( s, 60.0 ) * 0.3 );
    finalColor = vec4( c, 1.0 );
}
)";

/**
 * @brief The volumetric fog, its own pass at half the scene's size (Gfx::fog): for every second pixel of the scene's
 *        depth the fog between the eye and what is seen there (the sky: 200 m out), raymarched through the room's box
 *        with its density over height and noise, the sun's shafts and the lamp's cone (their shadow maps) and the
 *        halos of the point lights. Out: in-scattered light, transmittance.
 */
inline const char *FogFs = R"(
// The fog between `from` and `to`, inside the room only: rgb in-scattered light, a transmittance.
vec4 volumetric( vec3 from, vec3 to )
{
    vec3 dir;
    float tn, tf;
    if( !fogSpan( from, to, dir, tn, tf ) ) return vec4( 0.0, 0.0, 0.0, 1.0 );
    int Steps = max( fogSteps, 4 );
    float dt = ( tf - tn ) / float( Steps );
    float jitter = ign( gl_FragCoord.xy );
    // Two lobes: forward scattering towards the sun and some back to it (the camera looks away from the sun).
    float phaseSun = mix( hg( dot( sunDir, -dir ), 0.55 ), hg( dot( sunDir, -dir ), -0.3 ), 0.5 );
    vec3 scat = vec3( 0.0 );
    float T = 1.0;
    float densSum = 0.0;
    for( int i = 0; i < Steps; i++ )
    {
        vec3 p = from + dir * ( tn + ( float( i ) + jitter ) * dt );
        float dens = fogParams.x + fogParams.y * exp( -max( p.y, 0.0 ) * fogParams.z );
        dens *= 0.45 + 1.1 * noise3( p * 0.35 + vec3( time * 0.07, -time * 0.02, time * 0.05 ) );
        vec3 Li = fogColor;
        vec4 sp = sunVP * vec4( p, 1.0 );
        vec3 sq = sp.xyz / sp.w * 0.5 + 0.5;
        float sh = ( sq.z - 0.0005 <= texture( sunShadow, sq.xy ).r ) ? 1.0 : 0.0;
        Li += sunColor * sh * phaseSun * fogParams.w;
        vec3 L;
        vec3 sc = spotLight( p, L );
        if( sc.r + sc.g + sc.b > 0.0 )
            Li += sc * spotShadowAt( p, vec3( 0.0 ), 1 ) * hg( dot( -L, -dir ), 0.35 ) * fogParams.w;
        float ext = dens * dt;
        scat += T * Li * ext;
        T *= exp( -ext );
        densSum += dens;
    }
    // Halos of the point lights: the in-scattering of a point source integrated along the ray; a fixed light's only
    // where its mask lets it reach the ray's nearest point to it (no glow over the walls from the next room).
    float densAvg = densSum / float( Steps );
    vec3 halo = vec3( 0.0 );
    for( int i = 0; i < lightCount; i++ )
    {
        if( lightColor[i].w <= 0.0 ) continue;
        float tc = dot( lightPos[i].xyz - from, dir );
        float h = max( length( lightPos[i].xyz - ( from + dir * tc ) ), 0.2 );
        float a = ( atan( ( tf - tc ) / h ) - atan( ( tn - tc ) / h ) ) / h;
        halo += lightColor[i].rgb * lightColor[i].w * a * lightVis( i, from + dir * clamp( tc, tn, tf ) );
    }
    scat += halo * densAvg / ( 4.0 * PI ) * mix( 1.0, T, 0.5 );
    return vec4( scat, T );
}

in vec2 uv;
uniform sampler2D texture0;   // the scene's depth
uniform mat4 invViewProj;
uniform vec2 sceneSize;       // the scene's size this frame (pixels)
uniform int fogStep;          // 2: half size (1: full)
out vec4 finalColor;
void main()
{
    ivec2 at = ivec2( gl_FragCoord.xy ) * fogStep;
    float d = texelFetch( texture0, at, 0 ).r;
    vec2 ndc = ( vec2( at ) + 0.5 ) / sceneSize * 2.0 - 1.0;
    vec3 to;
    if( d >= 1.0 )
    {
        vec4 w = invViewProj * vec4( ndc, 1.0, 1.0 );
        to = viewPos + normalize( w.xyz / w.w - viewPos ) * 200.0;
    }
    else
    {
        vec4 w = invViewProj * vec4( ndc, d * 2.0 - 1.0, 1.0 );
        to = w.xyz / w.w;
    }
    finalColor = volumetric( viewPos, to );
}
)";

/**
 * @brief The fog over the scene at its full size: the four half-size texels around a pixel, each weighed by how near
 *        its depth lies to the pixel's own (edges keep the fog of their side), blended as colour times transmittance
 *        plus the in-scattered light times the target's alpha (a stain's darkening; Gfx::fog sets that blend).
 */
inline const char *FogUpFs = R"(
#version 330
in vec2 uv;
uniform sampler2D texture0;   // the fog at half size
uniform sampler2D depthTex;   // the scene's depth
uniform vec2 nearFar;
uniform ivec2 fogLast;        // the last fog texel drawn this frame
uniform int fogStep;          // 2: half size (1: full)
out vec4 finalColor;
float linear( float d )
{
    float z = d * 2.0 - 1.0;
    return 2.0 * nearFar.x * nearFar.y / ( nearFar.y + nearFar.x - z * ( nearFar.y - nearFar.x ) );
}
void main()
{
    ivec2 p = ivec2( gl_FragCoord.xy );
    float z = linear( texelFetch( depthTex, p, 0 ).r );
    ivec2 b = p / fogStep;
    vec2 f = vec2( p - b * fogStep ) * 0.5;
    vec4 sum = vec4( 0.0 );
    float total = 0.0;
    for( int j = 0; j < 2; j++ )
        for( int i = 0; i < 2; i++ )
        {
            ivec2 q = min( b + ivec2( i, j ), fogLast );
            float zq = linear( texelFetch( depthTex, q * fogStep, 0 ).r );
            float w = ( i == 0 ? 1.0 - f.x : f.x ) * ( j == 0 ? 1.0 - f.y : f.y );
            w = ( w + 1e-4 ) / ( 0.002 + abs( z - zq ) / z );
            sum += texelFetch( texture0, q, 0 ) * w;
            total += w;
        }
    finalColor = sum / total;
}
)";

/** @brief The gas giant below the station: its map, day and night, the atmosphere's rim (fogged by the fog pass when
 *         drawn before it); a star or the ring habitat's band, drawn after the fog pass (their own light). */
inline const char *PlanetFs = R"(
invariant in vec3 fragPos;
in vec3 fragNormal;
in vec3 objPos;
in vec3 objNormal;
in vec4 fragColor;
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float spin;
uniform float glow;
uniform float ringMode;
uniform vec3 ringSky;
out vec4 finalColor;
void main()
{
    if( ringMode > 0.5 )
    {
        // The ring seen from its inside: land in daylight, fading into the sky's colour with the air the sight line
        // goes through (a layer a sixteenth of the radius deep on both sides; a chord meets the ring at the same
        // angle at both ends): long and hazy near the horizon, where the land rises out of the haze, short and clear
        // towards the arch overhead. There, on the far side, the shadow's night, the towns glittering (the map's
        // alpha marks them).
        vec4 m = texture( texture0, fragTexCoord );
        vec3 land = toLinear( m.rgb );
        float up = clamp( ( fragPos.y - viewPos.y ) / ( 2.0 * ringMode ), 0.0, 1.0 );
        float night = smoothstep( 0.62, 0.92, up );
        vec2 cell = floor( fragTexCoord * vec2( 2400.0, 300.0 ) );
        float spark = step( 0.55, fract( sin( dot( cell, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 ) );
        vec3 towns = vec3( 1.0, 0.72, 0.38 ) * m.a * spark * 1.6;
        vec3 c = mix( land * sunColor * 0.13, towns + land * 0.002, night );
        float d = length( viewPos - fragPos );
        float rise = max( ( fragPos.y - viewPos.y ) / max( d, 1.0 ), 0.02 );
        float air = min( d, 0.12 * ringMode / rise );
        float haze = ( 1.0 - exp( -air / ( 0.25 * ringMode ) ) ) * ( 1.0 - night * 0.75 );
        c = mix( c, ringSky * 0.85, haze * 0.9 );
        finalColor = vec4( c, 1.0 );
        return;
    }
    vec3 n = normalize( objNormal );
    vec2 st = vec2( atan( n.z, n.x ) / ( 2.0 * PI ) + 0.5 + spin, acos( clamp( n.y, -1.0, 1.0 ) ) / PI );
    vec3 alb = toLinear( textureLod( texture0, st, 0.0 ).rgb );
    // Finer swirls from the same map at a higher frequency, so the clouds hold up close.
    vec3 fine = toLinear( textureLod( texture0, st * vec2( 5.0, 3.0 ) + vec2( 0.37, 0.11 ), 0.0 ).rgb );
    alb *= 1.0 + ( dot( fine, vec3( 0.333 ) ) - 0.3 ) * 0.7;
    vec3 N = normalize( fragNormal );
    vec3 V = normalize( viewPos - fragPos );
    if( glow > 0.0 )
    {
        // A star: its own light, darker towards the limb, a red rim of corona.
        float mu = max( dot( N, V ), 0.0 );
        vec3 c = alb * glow * ( 0.3 + 0.7 * pow( mu, 0.45 ) );
        c += vec3( 1.0, 0.25, 0.08 ) * glow * 0.6 * pow( 1.0 - mu, 4.0 );
        finalColor = vec4( c, 1.0 );
        return;
    }
    float l = dot( N, -sunDir );
    float day = smoothstep( -0.12, 0.45, l );
    float rim = pow( 1.0 - max( dot( N, V ), 0.0 ), 3.0 );
    vec3 c = alb * sunColor * 0.13 * day + alb * vec3( 0.002, 0.003, 0.006 );
    c += vec3( 0.25, 0.55, 0.9 ) * rim * ( 0.05 + 0.7 * smoothstep( -0.3, 0.5, l ) );
    finalColor = vec4( c, 1.0 );
}
)";

/** @brief Vertex shader of the particles (the batch's vertices are in world space already). */
inline const char *ParticleVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec3 fragPos;
out vec2 fragTexCoord;
out vec4 fragColor;
void main()
{
    fragPos = vertexPosition;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4( vertexPosition, 1.0 );
}
)";

/** @brief Additive particles; the dust is lit (it glitters where the sun and the lamp reach). */
inline const char *ParticleFs = R"(
in vec3 fragPos;
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float boost;
uniform int lit;
out vec4 finalColor;
void main()
{
    vec4 t = texture( texture0, fragTexCoord ) * fragColor;
    vec3 c = t.rgb * t.a * boost;
    if( lit == 1 )
    {
        vec3 light = fogColor * 20.0 + sunColor * sunShadowAt( fragPos, vec3( 0.0 ), 1.0, 1 );
        vec3 L;
        vec3 sc = spotLight( fragPos, L );
        if( sc.r + sc.g + sc.b > 0.0 ) light += sc * spotShadowAt( fragPos, vec3( 0.0 ), 1 );
        for( int i = 0; i < lightCount; i++ )
        {
            float d = length( lightPos[i].xyz - fragPos );
            if( d < lightPos[i].w ) light += lightColor[i].rgb * atten( d, lightPos[i].w ) * lightVis( i, fragPos );
        }
        // Capped: a cloud of steam beside a lamp must not add up to white.
        c *= min( light, vec3( 2.5 ) );
    }
    finalColor = vec4( c, 1.0 );
}
)";

/** @brief Depth only (the shadow maps): the light's viewProj and the caster's model matrix. */
inline const char *DepthVs = R"(
#version 330
in vec3 vertexPosition;
uniform mat4 viewProj;
uniform mat4 matModel;
void main() { gl_Position = viewProj * ( matModel * vec4( vertexPosition, 1.0 ) ); }
)";

/** @brief Depth only: nothing to colour. */
inline const char *DepthFs = R"(
#version 330
out vec4 finalColor;
void main() { finalColor = vec4( 1.0 ); }
)";

/** @brief Bloom, first step: what is brighter than the threshold (soft knee), at half size. */
inline const char *PrefilterFs = R"(
#version 330
in vec2 uv;
uniform sampler2D texture0;
uniform vec2 texel;
uniform float threshold;
uniform vec2 sceneScale;   // the part of the target the scene filled (dynamic resolution)
out vec4 finalColor;
vec3 s( vec2 o ) { return texture( texture0, clamp( uv * sceneScale + texel * o, texel * 0.5, sceneScale - texel * 0.5 ) ).rgb; }
void main()
{
    vec3 c = ( s( vec2( -1.0, -1.0 ) ) + s( vec2( 1.0, -1.0 ) ) + s( vec2( -1.0, 1.0 ) ) + s( vec2( 1.0, 1.0 ) ) ) * 0.25;
    float br = max( c.r, max( c.g, c.b ) );
    float knee = threshold * 0.5;
    float soft = clamp( br - threshold + knee, 0.0, 2.0 * knee );
    soft = soft * soft / ( 4.0 * knee + 1e-5 );
    float contrib = max( soft, br - threshold ) / max( br, 1e-5 );
    finalColor = vec4( min( c * contrib, vec3( 200.0 ) ), 1.0 );
}
)";

/** @brief Bloom downsampling (13 taps). */
inline const char *DownFs = R"(
#version 330
in vec2 uv;
uniform sampler2D texture0;
uniform vec2 texel;
out vec4 finalColor;
vec3 s( float x, float y ) { return texture( texture0, uv + texel * vec2( x, y ) ).rgb; }
void main()
{
    vec3 r = s( 0, 0 ) * 0.125 + ( s( -2, 2 ) + s( 2, 2 ) + s( -2, -2 ) + s( 2, -2 ) ) * 0.03125 +
             ( s( 0, 2 ) + s( -2, 0 ) + s( 2, 0 ) + s( 0, -2 ) ) * 0.0625 + ( s( -1, 1 ) + s( 1, 1 ) + s( -1, -1 ) + s( 1, -1 ) ) * 0.125;
    finalColor = vec4( r, 1.0 );
}
)";

/** @brief Bloom upsampling (3x3 tent), added onto the next larger level. */
inline const char *UpFs = R"(
#version 330
in vec2 uv;
uniform sampler2D texture0;
uniform vec2 texel;
out vec4 finalColor;
vec3 s( float x, float y ) { return texture( texture0, uv + texel * vec2( x, y ) ).rgb; }
void main()
{
    vec3 r = ( s( -1, -1 ) + s( 1, -1 ) + s( -1, 1 ) + s( 1, 1 ) + 2.0 * ( s( 0, -1 ) + s( -1, 0 ) + s( 1, 0 ) + s( 0, 1 ) ) +
               4.0 * s( 0, 0 ) ) / 16.0;
    finalColor = vec4( r, 1.0 );
}
)";

/**
 * @brief The picture: FXAA (on every quality: edges found in the tone-mapped luma of the scene's own pixels), bloom,
 *        exposure, ACES, a split tone, vignette, a little chromatic aberration and grain.
 */
inline const char *FinalFs = R"(
#version 330
in vec2 uv;
uniform sampler2D texture0;
uniform sampler2D bloomTex;
uniform vec2 sceneScale;    // the part of the scene's target drawn this frame (dynamic resolution)
uniform float exposure;
uniform float bloom;
uniform float time;
uniform vec2 resolution;
uniform vec4 grade;      // a tale's light: colour, contrast, extra grain, extra vignette
uniform vec3 gradeTint;
uniform int colourVision;   // 1 red-green, 2 blue-yellow: what the eye cannot tell apart is moved where it can
out vec4 finalColor;

vec3 daltonize( vec3 c, int kind )
{
    vec3 lms = vec3( 17.8824 * c.r + 43.5161 * c.g + 4.11935 * c.b, 3.45565 * c.r + 27.1554 * c.g + 3.86714 * c.b,
                     0.0299566 * c.r + 0.184309 * c.g + 1.46709 * c.b );
    if( kind == 1 ) lms.y = 0.494207 * lms.x + 1.24827 * lms.z;
    else lms.z = -0.395913 * lms.x + 0.801109 * lms.y;
    vec3 seen = vec3( 0.0809444479 * lms.x - 0.130504409 * lms.y + 0.116721066 * lms.z,
                      -0.0102485335 * lms.x + 0.0540193266 * lms.y - 0.113614708 * lms.z,
                      -0.000365296938 * lms.x - 0.00412161469 * lms.y + 0.693511405 * lms.z );
    vec3 lost = c - seen;
    // Red-green: what red carried goes to green and blue; blue-yellow: what blue carried goes to red and green.
    vec3 moved = kind == 1 ? vec3( 0.0, 0.7 * lost.r + lost.g, 0.7 * lost.r + lost.b )
                           : vec3( 0.7 * lost.b, 0.7 * lost.b, 0.0 );
    return clamp( c + moved, 0.0, 1.0 );
}
float hash( vec2 p ) { return fract( sin( dot( p, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 ); }

vec2 texel;   // one of the scene's pixels (uv)

vec3 scene( vec2 p ) { return texture( texture0, clamp( p, texel * 0.5, sceneScale - texel * 0.5 ) ).rgb; }

float luma( vec3 c )
{
    c *= exposure;
    c = ( c * ( 2.51 * c + 0.03 ) ) / ( c * ( 2.43 * c + 0.59 ) + 0.14 );
    return sqrt( dot( clamp( c, 0.0, 1.0 ), vec3( 0.299, 0.587, 0.114 ) ) );
}

// FXAA's blend along an edge (Lottes' FXAA 2): two taps along it, or four where two overshoot the neighbours' range.
vec3 alongEdge( vec2 p, vec2 dir, float lo, float hi )
{
    vec3 a = 0.5 * ( scene( p + dir * ( 1.0 / 3.0 - 0.5 ) ) + scene( p + dir * ( 2.0 / 3.0 - 0.5 ) ) );
    vec3 b = a * 0.5 + 0.25 * ( scene( p - dir * 0.5 ) + scene( p + dir * 0.5 ) );
    float lb = luma( b );
    return ( lb < lo || lb > hi ) ? a : b;
}

void main()
{
    texel = 1.0 / vec2( textureSize( texture0, 0 ) );
    vec2 d = uv - 0.5;
    vec2 p = uv * sceneScale;
    vec2 split = d * 0.004 * sceneScale;   // the chromatic aberration
    vec3 mid = scene( p );
    float lm = luma( mid );
    float nw = luma( scene( p + vec2( -1.0, -1.0 ) * texel ) );
    float ne = luma( scene( p + vec2( 1.0, -1.0 ) * texel ) );
    float sw = luma( scene( p + vec2( -1.0, 1.0 ) * texel ) );
    float se = luma( scene( p + vec2( 1.0, 1.0 ) * texel ) );
    float lo = min( lm, min( min( nw, ne ), min( sw, se ) ) );
    float hi = max( lm, max( max( nw, ne ), max( sw, se ) ) );
    vec3 c;
    if( hi - lo < max( 0.0625, hi * 0.125 ) )
        c = vec3( scene( p - split ).r, mid.g, scene( p + split ).b );   // no edge: as it is
    else
    {
        vec2 dir = vec2( ( sw + se ) - ( nw + ne ), ( nw + sw ) - ( ne + se ) );
        float reduce = max( ( nw + ne + sw + se ) * ( 0.25 / 8.0 ), 1.0 / 128.0 );
        dir = clamp( dir / ( min( abs( dir.x ), abs( dir.y ) ) + reduce ), vec2( -8.0 ), vec2( 8.0 ) ) * texel;
        c = vec3( alongEdge( p - split, dir, lo, hi ).r, alongEdge( p, dir, lo, hi ).g, alongEdge( p + split, dir, lo, hi ).b );
    }
    vec3 b = texture( bloomTex, uv ).rgb;
    c = mix( c, b, bloom ) + b * bloom * 0.3;
    c *= exposure;
    c = ( c * ( 2.51 * c + 0.03 ) ) / ( c * ( 2.43 * c + 0.59 ) + 0.14 );
    float l = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
    c = mix( c, c * vec3( 0.88, 1.0, 1.12 ), smoothstep( 0.45, 0.0, l ) * 0.6 );
    c = mix( c, c * vec3( 1.07, 1.0, 0.92 ), smoothstep( 0.3, 1.0, l ) * 0.45 );
    c *= 1.0 - 0.45 * pow( length( d ) * 1.25, 2.4 );
    c = pow( clamp( c, 0.0, 1.0 ), vec3( 1.0 / 2.2 ) );
    float g = dot( c, vec3( 0.299, 0.587, 0.114 ) );
    c = clamp( ( mix( vec3( g ), c, grade.x ) * gradeTint - 0.5 ) * grade.y + 0.5, 0.0, 1.0 );
    c *= 1.0 - grade.w * pow( length( d ) * 1.3, 2.0 );
    c += ( hash( uv * resolution + fract( time ) * 91.7 ) - 0.5 ) * ( 0.035 + grade.z );
    if( colourVision > 0 ) c = daltonize( c, colourVision );
    finalColor = vec4( c, 1.0 );
}
)";

}   // namespace spaceopera
