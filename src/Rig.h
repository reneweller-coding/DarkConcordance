/**
 * @file Rig.h
 * @brief Helpers for procedural rigs: a bone frame between two joints and two-bone IK (arms, insect legs).
 * @author Rene Weller
 */
#pragma once

#include "MeshBuilder.h"

#include <raylib.h>
#include <raymath.h>

#include <cmath>

namespace spaceopera
{

/**
 * @brief A bone frame from a joint to its child: -y points at the child, z leans towards `hint`.
 * @param[in] from  the joint
 * @param[in] to    the child joint
 * @param[in] hint  roughly where the bone's front faces
 * @return the frame (origin at `from`)
 */
inline Matrix boneFrame( Vector3 from, Vector3 to, Vector3 hint )
{
    Vector3 y = Vector3Normalize( Vector3Subtract( from, to ) );
    Vector3 x = Vector3CrossProduct( y, hint );
    if ( Vector3Length( x ) < 1e-3f ) x = Vector3CrossProduct( y, { 1.0f, 0.0f, 0.0f } );
    if ( Vector3Length( x ) < 1e-3f ) x = Vector3CrossProduct( y, { 0.0f, 0.0f, 1.0f } );
    x = Vector3Normalize( x );
    Vector3 z = Vector3CrossProduct( x, y );
    return frame( x, y, z, from );
}

/**
 * @brief Two-bone IK: from a root joint to a target with the middle joint bent towards `pole`.
 * @param[in]  s      root joint (shoulder, hip)
 * @param[in]  t      target (wrist, foot)
 * @param[in]  a      length of the first bone
 * @param[in]  b      length of the second bone
 * @param[in]  pole   where the middle joint should point
 * @param[in]  hint   where the bones' fronts face
 * @param[out] upper  frame of the first bone
 * @param[out] lower  frame of the second bone
 */
inline void twoBone( Vector3 s, Vector3 t, float a, float b, Vector3 pole, Vector3 hint, Matrix &upper, Matrix &lower )
{
    Vector3 d = Vector3Subtract( t, s );
    float   dist = Vector3Length( d );
    Vector3 dir = Vector3Scale( d, 1.0f / fmaxf( dist, 1e-4f ) );
    float   len = Clamp( dist, 0.05f, a + b - 0.002f );
    float   cosA = Clamp( ( a * a + len * len - b * b ) / ( 2.0f * a * len ), -1.0f, 1.0f );
    float   sinA = sqrtf( 1.0f - cosA * cosA );
    Vector3 p = Vector3Subtract( pole, Vector3Scale( dir, Vector3DotProduct( pole, dir ) ) );
    if ( Vector3Length( p ) < 1e-4f ) p = { 0.0f, -1.0f, 0.0f };
    p = Vector3Normalize( p );
    Vector3 e = Vector3Add( s, Vector3Add( Vector3Scale( dir, a * cosA ), Vector3Scale( p, a * sinA ) ) );
    Vector3 end = Vector3Add( s, Vector3Scale( dir, len ) );
    upper = boneFrame( s, e, hint );
    lower = boneFrame( e, end, hint );
}

/**
 * @brief The shortest turn from one angle to another.
 * @param[in] from  start (radians)
 * @param[in] to    goal (radians)
 * @return the difference in -pi..pi
 */
inline float angleTo( float from, float to )
{
    float d = fmodf( to - from + PI, 2.0f * PI );
    if ( d < 0.0f ) d += 2.0f * PI;
    return d - PI;
}

}   // namespace spaceopera
