/**
 * @file Shaders.h
 * @brief The GLSL of the look test, kept in the program: lit surfaces (triplanar PBR, sun and lamp shadows, point
 *        lights, volumetric fog), the glass floor, the sky, the gas giant, particles, the depth pass and the post chain
 *        (bloom, ACES, grade, grain).
 * @author Rene Weller
 */
#pragma once

namespace spaceopera
{

/** @brief Vertex shader of everything lit: world and object position and normal, colour, UV. */
inline const char *LitVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPos;
out vec3 fragNormal;
out vec3 objPos;
out vec3 objNormal;
out vec4 fragColor;
out vec2 fragTexCoord;
void main()
{
    objPos = vertexPosition;
    objNormal = vertexNormal;
    fragPos = vec3( matModel * vec4( vertexPosition, 1.0 ) );
    fragNormal = normalize( vec3( matNormal * vec4( vertexNormal, 0.0 ) ) );
    fragColor = vertexColor;
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * vec4( vertexPosition, 1.0 );
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
out vec3 fragPos;
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

/** @brief Depth only, skinned (the shadow maps). */
inline const char *DepthSkinVs = R"(
#version 330
in vec3 vertexPosition;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
uniform mat4 mvp;
uniform mat4 boneMatrices[128];
void main()
{
    mat4 skin = boneMatrices[int( vertexBoneIds.x )] * vertexBoneWeights.x +
                boneMatrices[int( vertexBoneIds.y )] * vertexBoneWeights.y +
                boneMatrices[int( vertexBoneIds.z )] * vertexBoneWeights.z +
                boneMatrices[int( vertexBoneIds.w )] * vertexBoneWeights.w;
    gl_Position = mvp * ( skin * vec4( vertexPosition, 1.0 ) );
}
)";

/** @brief What every lit fragment shader shares: the lights, the shadows, the fog. */
inline const char *CommonFs = R"(
#version 330
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
#define MAX_LIGHTS 32
uniform int lightCount;
uniform vec4 lightPos[MAX_LIGHTS];
uniform vec4 lightColor[MAX_LIGHTS];
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

// The fog between `from` and `to`, inside the room only: rgb in-scattered light, a transmittance.
vec4 volumetric( vec3 from, vec3 to )
{
    vec3 seg = to - from;
    float len = length( seg );
    vec3 dir = seg / max( len, 1e-4 );
    vec3 inv = 1.0 / ( abs( dir ) + 1e-6 ) * sign( dir + 1e-7 );
    vec3 t0 = ( roomMin - from ) * inv;
    vec3 t1 = ( roomMax - from ) * inv;
    vec3 tmin = min( t0, t1 );
    vec3 tmax = max( t0, t1 );
    float tn = max( max( tmin.x, tmin.y ), max( tmin.z, 0.0 ) );
    float tf = min( min( tmax.x, tmax.y ), min( tmax.z, len ) );
    if( tf <= tn ) return vec4( 0.0, 0.0, 0.0, 1.0 );
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
    // Halos of the point lights: the in-scattering of a point source integrated along the ray.
    float densAvg = densSum / float( Steps );
    vec3 halo = vec3( 0.0 );
    for( int i = 0; i < lightCount; i++ )
    {
        if( lightColor[i].w <= 0.0 ) continue;
        float tc = dot( lightPos[i].xyz - from, dir );
        float h = max( length( lightPos[i].xyz - ( from + dir * tc ) ), 0.2 );
        float a = ( atan( ( tf - tc ) / h ) - atan( ( tn - tc ) / h ) ) / h;
        halo += lightColor[i].rgb * lightColor[i].w * a;
    }
    scat += halo * densAvg / ( 4.0 * PI ) * mix( 1.0, T, 0.5 );
    return vec4( scat, T );
}
)";

/** @brief Lit surfaces: triplanar albedo/ORM/normal (or UV), macro variation, emission, fog. */
inline const char *LitFs = R"(
in vec3 fragPos;
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
uniform float cutaway;     // 1: dissolves between the camera and the hero
uniform vec3 cutCentre;    // the hero (chest)
uniform float cutRadius;   // radius of the hole at the hero
out vec4 finalColor;

// Walls between the camera and the hero dissolve in a cone around the line of sight (ordered dither).
void cutAway()
{
    vec3 toHero = cutCentre - viewPos;
    float dist = length( toHero );
    vec3 dir = toHero / dist;
    vec3 rel = fragPos - viewPos;
    float t = dot( rel, dir );
    if( t <= 0.0 || t >= dist - 0.6 || fragPos.y < cutCentre.y - 0.9 ) return;
    float r = cutRadius * t / dist;
    float d = length( rel - dir * t );
    float keep = smoothstep( r * 0.65, r, d );
    const float Bayer[16] = float[]( 0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0 );
    ivec2 q = ivec2( gl_FragCoord.xy ) & 3;
    if( keep < ( Bayer[q.y * 4 + q.x] + 0.5 ) / 16.0 ) discard;
}

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

void main()
{
    if( cutaway > 0.5 ) cutAway();
    vec3 n0 = normalize( objNormal );
    vec3 albedo;
    vec3 orm;
    vec3 nObj;
    float alpha = 1.0;
    if( matParams.w < 0.5 )
        triplanar( objPos * uvScale, n0, albedo, orm, nObj );
    else
    {
        vec2 uv = fragTexCoord * uvScale;
        vec4 tex = texture( texture0, uv );
        albedo = toLinear( tex.rgb );
        orm = texture( texture1, uv ).rgb;
        nObj = n0;
        // A decal: blended by its alpha over what is under it (nothing written where it is empty); a stain multiplies
        // what is under it (the blending does it: destination times this).
        if( matParams.w > 2.5 && matParams.w < 4.5 )
        {
            alpha = tex.a;
            if( alpha < 0.03 ) discard;
            if( matParams.w > 3.5 )
            {
                finalColor = vec4( mix( vec3( 1.0 ), albedo, alpha * 0.65 ), 1.0 );
                return;
            }
        }
    }
    bool garment = matParams.w > 4.5;
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
    // Seen from behind (a cavern from inside, nothing is culled): light the face that is seen.
    if( dot( Ng, V ) < 0.0 )
    {
        N = -N;
        Ng = -Ng;
    }
    vec3 col = vec3( 0.0 );

    float ndl = max( dot( Ng, -sunDir ), 0.0 );
    if( dot( N, -sunDir ) > 0.0 )
        col += brdf( N, V, -sunDir, albedo, rough, metal ) * sunColor * sunShadowAt( fragPos, Ng, ndl, shadowTaps );
    vec3 L;
    vec3 sc = spotLight( fragPos, L );
    if( sc.r + sc.g + sc.b > 0.0 ) col += brdf( N, V, L, albedo, rough, metal ) * sc * spotShadowAt( fragPos, Ng, shadowTaps );
    for( int i = 0; i < lightCount; i++ )
    {
        vec3 lv = lightPos[i].xyz - fragPos;
        float d = length( lv );
        if( d < lightPos[i].w )
            col += brdf( N, V, lv / d, albedo, rough, metal ) * lightColor[i].rgb * atten( d, lightPos[i].w );
    }

    // Ambient: a hemisphere for the diffuse part and a fake environment for the metals.
    vec3 amb = mix( ambDown, ambUp, N.y * 0.5 + 0.5 );
    vec3 F0 = mix( vec3( 0.04 ), albedo, metal );
    float NdV = max( dot( N, V ), 0.0 );
    vec3 Fr = F0 + ( max( vec3( 1.0 - rough ), F0 ) - F0 ) * pow( 1.0 - NdV, 5.0 );
    vec3 R = reflect( -V, N );
    vec3 env = mix( ambDown, ambUp, R.y * 0.5 + 0.5 ) * 3.0;
    col += ( amb * albedo * ( 1.0 - metal ) + env * Fr ) * ao;
    if( waterCount > 0 ) col += albedo * caustics( fragPos ) * 1.4 * ao;

    vec3 em = emissive.rgb;
    if( emissive.w > 0.5 && emissive.w < 1.5 )
        em *= smoothstep( emissiveMask.x, emissiveMask.y, 1.0 - orm.r );
    else if( emissive.w > 1.5 )
        em *= toLinear( texture( emissionMap, fragTexCoord ).rgb );
    col += em;

    vec4 fog = volumetric( viewPos, fragPos );
    finalColor = vec4( col * fog.a + fog.rgb, alpha );
}
)";

/** @brief The glass floor: smudges and scratches catch the light, the lights mirror in it; premultiplied alpha. As ice
 *         (iceMode 1): frost in drifts and cracks, a pale cyan body one still sees through. */
inline const char *GlassFs = R"(
uniform float iceMode;
in vec3 fragPos;
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
        float at = atten( d, lightPos[i].w );
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
    vec4 fog = volumetric( viewPos, fragPos );
    finalColor = vec4( c * fog.a, alpha );
}
)";

/** @brief A ball of water floating in low gravity: rippled skin, reflections at the rim, the depths dark green, a
 *         shimmer of caustics inside; premultiplied alpha. */
inline const char *WaterFs = R"(
in vec3 fragPos;
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
        spec += lightColor[i].rgb * atten( d, lightPos[i].w ) * pow( max( dot( N, H ), 0.0 ), 120.0 ) * 2.0;
    }
    vec3 L;
    vec3 sc = spotLight( fragPos, L );
    if( sc.r + sc.g + sc.b > 0.0 ) spec += sc * pow( max( dot( N, normalize( V + L ) ), 0.0 ), 120.0 ) * 2.0;
    vec3 env = mix( ambDown, ambUp, reflect( -V, N ).y * 0.5 + 0.5 ) * 4.0;
    vec3 body = vec3( 0.02, 0.09, 0.1 ) * ( 0.6 + caustics( fragPos ).g * 2.0 );
    vec3 c = body * ( 1.0 - F ) + ( env + spec ) * F + spec * 0.3;
    float alpha = clamp( 0.22 + F * 0.6, 0.0, 0.9 );
    vec4 fog = volumetric( viewPos, fragPos );
    finalColor = vec4( c * fog.a * alpha, alpha );
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

/** @brief The sky behind the windows and below the glass: the nebula, stars, the sun, and the room's fog. */
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
    vec4 fog = volumetric( viewPos, viewPos + dir * 200.0 );
    finalColor = vec4( c * fog.a + fog.rgb, 1.0 );
}
)";

/** @brief The gas giant below the station: its map, day and night, the atmosphere's rim. */
inline const char *PlanetFs = R"(
in vec3 fragPos;
in vec3 fragNormal;
in vec3 objPos;
in vec3 objNormal;
in vec4 fragColor;
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float spin;
uniform float glow;
uniform float ringMode;
out vec4 finalColor;
void main()
{
    if( ringMode > 0.5 )
    {
        // The ring seen from its inside: land in daylight, hazier with distance; overhead, on the far side, the
        // shadow's night, the towns glittering (the map's alpha marks them).
        vec4 m = textureLod( texture0, fragTexCoord, 0.0 );
        vec3 land = toLinear( m.rgb );
        float up = clamp( ( fragPos.y - viewPos.y ) / ( 2.0 * ringMode ), 0.0, 1.0 );
        float night = smoothstep( 0.62, 0.92, up );
        vec2 cell = floor( fragTexCoord * vec2( 2400.0, 300.0 ) );
        float spark = step( 0.55, fract( sin( dot( cell, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 ) );
        vec3 towns = vec3( 1.0, 0.72, 0.38 ) * m.a * spark * 1.6;
        vec3 c = mix( land * sunColor * 0.13, towns + land * 0.002, night );
        float d = length( viewPos - fragPos );
        float haze = ( 1.0 - exp( -d / ( 0.9 * ringMode ) ) ) * ( 1.0 - night * 0.7 );
        c = mix( c, vec3( 0.36, 0.5, 0.74 ) * sunColor * 0.06, haze * 0.8 );
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
    vec4 fog = volumetric( viewPos, fragPos );
    finalColor = vec4( c * fog.a + fog.rgb, 1.0 );
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
            if( d < lightPos[i].w ) light += lightColor[i].rgb * atten( d, lightPos[i].w );
        }
        // Capped: a cloud of steam beside a lamp must not add up to white.
        c *= min( light, vec3( 2.5 ) );
    }
    finalColor = vec4( c, 1.0 );
}
)";

/** @brief Depth only (the shadow maps). */
inline const char *DepthVs = R"(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
void main() { gl_Position = mvp * vec4( vertexPosition, 1.0 ); }
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
out vec4 finalColor;
void main()
{
    vec3 c = ( texture( texture0, uv + texel * vec2( -1.0, -1.0 ) ).rgb + texture( texture0, uv + texel * vec2( 1.0, -1.0 ) ).rgb +
               texture( texture0, uv + texel * vec2( -1.0, 1.0 ) ).rgb + texture( texture0, uv + texel * vec2( 1.0, 1.0 ) ).rgb ) * 0.25;
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

/** @brief The picture: bloom, exposure, ACES, a split tone, vignette, a little chromatic aberration and grain. */
inline const char *FinalFs = R"(
#version 330
in vec2 uv;
uniform sampler2D texture0;
uniform sampler2D bloomTex;
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
void main()
{
    vec2 d = uv - 0.5;
    vec3 c;
    c.r = texture( texture0, uv - d * 0.004 ).r;
    c.g = texture( texture0, uv ).g;
    c.b = texture( texture0, uv + d * 0.004 ).b;
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
