/**
 * @file Figure.cpp
 * @brief The hero's parts, walk cycle and arm IK.
 * @author Rene Weller
 */
#include "Figure.h"

#include "MeshBuilder.h"

#include <raymath.h>

#include <cmath>
#include <functional>

namespace spaceopera
{

namespace
{

constexpr float HipHeight = 0.967f;   ///< pelvis origin above the ground at rest
constexpr float Thigh = 0.42f;        ///< hip to knee
constexpr float Shin = 0.40f;         ///< knee to ankle
constexpr float UpperArm = 0.31f;     ///< shoulder to elbow
constexpr float Forearm = 0.29f;      ///< elbow to wrist
constexpr float Stride = 2.0f;        ///< metres a full walk cycle
constexpr float RunSpeed = 4.2f;      ///< full stride amplitude at this speed

const Vector4 Visor = { 0.35f, 2.4f, 3.2f, 0.0f };   ///< the visor's cyan
const Vector4 Status = { 0.2f, 2.0f, 0.4f, 0.0f };   ///< the backpack's green
const Vector4 Alert = { 2.5f, 0.3f, 0.1f, 0.0f };    ///< one red light
const Vector4 Lens = { 6.0f, 5.5f, 4.6f, 0.0f };     ///< the lamp's lens
const Vector4 Cell = { 0.3f, 1.6f, 3.0f, 0.0f };     ///< the rifle's energy cell

/** @brief A bone frame from a joint to its child: -y points at the child, z leans towards `hint`. */
Matrix boneFrame( Vector3 from, Vector3 to, Vector3 hint )
{
    Vector3 y = Vector3Normalize( Vector3Subtract( from, to ) );
    Vector3 x = Vector3CrossProduct( y, hint );
    if ( Vector3Length( x ) < 1e-3f ) x = Vector3CrossProduct( y, { 1.0f, 0.0f, 0.0f } );
    x = Vector3Normalize( x );
    Vector3 z = Vector3CrossProduct( x, y );
    return frame( x, y, z, from );
}

/** @brief Two-bone IK: shoulder to wrist with the elbow towards `pole`; the two frames. */
void twoBone( Vector3 s, Vector3 t, float a, float b, Vector3 pole, Vector3 hint, Matrix &upper, Matrix &fore )
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
    Vector3 wrist = Vector3Add( s, Vector3Scale( dir, len ) );
    upper = boneFrame( s, e, hint );
    fore = boneFrame( e, wrist, hint );
}

/** @brief The shortest way from one angle to another. */
float angleTo( float from, float to )
{
    float d = fmodf( to - from + PI, 2.0f * PI );
    if ( d < 0.0f ) d += 2.0f * PI;
    return d - PI;
}

}   // namespace

void Figure::build( const FigureSurfaces &s )
{
    // Each part is its own small mesh in its bone's frame.
    auto add = [&]( BoneE bone, int surface, Vector4 glow, const std::function<void( MeshBuilder & )> &shape )
    {
        MeshBuilder b;
        shape( b );
        for ( Mesh m : b.build() ) m_parts.push_back( { m, surface, bone, glow } );
    };
    const Vector4 own = { -1.0f, 0.0f, 0.0f, 0.0f };
    auto          rbox = [&]( BoneE bone, int surface, Vector3 at, Vector3 half, float r, Vector3 rot = { 0, 0, 0 },
                              Vector4 glow = { -1.0f, 0.0f, 0.0f, 0.0f } )
    { add( bone, surface, glow, [&]( MeshBuilder &b ) { b.roundedBox( place( at, rot ), half, r, 3 ); } ); };

    // Pelvis.
    rbox( BoneE::PELVIS, s.m_armor, { 0.0f, 0.0f, 0.0f }, { 0.17f, 0.1f, 0.13f }, 0.05f );
    rbox( BoneE::PELVIS, s.m_metal, { 0.0f, 0.09f, 0.0f }, { 0.18f, 0.03f, 0.14f }, 0.02f );
    rbox( BoneE::PELVIS, s.m_dark, { 0.0f, -0.07f, 0.11f }, { 0.08f, 0.07f, 0.03f }, 0.02f );
    for ( float side : { -1.0f, 1.0f } )
        rbox( BoneE::PELVIS, s.m_armor, { side * 0.19f, -0.06f, 0.0f }, { 0.035f, 0.1f, 0.1f }, 0.02f,
              { 0.0f, 0.0f, side * 0.15f } );

    // Chest, backpack, shoulders, lamp.
    rbox( BoneE::SPINE, s.m_metal, { 0.0f, 0.09f, 0.0f }, { 0.14f, 0.1f, 0.11f }, 0.04f );
    rbox( BoneE::SPINE, s.m_armor, { 0.0f, 0.33f, 0.0f }, { 0.24f, 0.17f, 0.16f }, 0.07f );
    rbox( BoneE::SPINE, s.m_armor, { 0.0f, 0.35f, 0.15f }, { 0.18f, 0.12f, 0.04f }, 0.03f );
    rbox( BoneE::SPINE, s.m_dark, { 0.0f, 0.35f, 0.19f }, { 0.03f, 0.1f, 0.015f }, 0.01f );
    rbox( BoneE::SPINE, s.m_armor, { 0.0f, 0.33f, -0.24f }, { 0.19f, 0.22f, 0.09f }, 0.04f );
    for ( float side : { -1.0f, 1.0f } )
    {
        add( BoneE::SPINE, s.m_metal, own,
             [&]( MeshBuilder &b ) { b.cylinder( place( { side * 0.11f, 0.12f, -0.33f } ), 0.045f, 0.42f, 12 ); } );
        rbox( BoneE::SPINE, s.m_armor, { side * 0.33f, 0.47f, 0.0f }, { 0.11f, 0.075f, 0.14f }, 0.04f,
              { 0.0f, 0.0f, -side * 0.3f } );
    }
    rbox( BoneE::SPINE, s.m_glow, { 0.13f, 0.47f, -0.335f }, { 0.02f, 0.012f, 0.006f }, 0.004f, { 0, 0, 0 }, Status );
    rbox( BoneE::SPINE, s.m_glow, { 0.07f, 0.47f, -0.335f }, { 0.02f, 0.012f, 0.006f }, 0.004f, { 0, 0, 0 }, Status );
    rbox( BoneE::SPINE, s.m_glow, { -0.13f, 0.47f, -0.335f }, { 0.012f, 0.012f, 0.006f }, 0.004f, { 0, 0, 0 }, Alert );
    add( BoneE::SPINE, s.m_metal, own,
         [&]( MeshBuilder &b )
         {
             b.lathe( place( { 0.0f, 0.49f, 0.0f } ),
                      { { 0.0f, 0.0f },
                        { 0.1f, 0.0f },
                        { 0.1f, 0.0f },
                        { 0.13f, 0.02f },
                        { 0.13f, 0.05f },
                        { 0.1f, 0.07f },
                        { 0.0f, 0.07f } },
                      16 );
         } );
    add( BoneE::SPINE, s.m_metal, own, [&]( MeshBuilder &b )
         { b.cylinder( place( { -0.33f, 0.57f, 0.03f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.05f, 0.13f, 14 ); } );
    add( BoneE::SPINE, s.m_glow, Lens, [&]( MeshBuilder &b )
         { b.cylinder( place( { -0.33f, 0.57f, 0.16f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.04f, 0.012f, 14 ); } );

    // Helmet.
    rbox( BoneE::HEAD, s.m_armor, { 0.0f, 0.14f, 0.0f }, { 0.13f, 0.15f, 0.15f }, 0.09f );
    rbox( BoneE::HEAD, s.m_dark, { 0.0f, 0.11f, 0.12f }, { 0.11f, 0.09f, 0.05f }, 0.04f );
    rbox( BoneE::HEAD, s.m_glow, { 0.0f, 0.15f, 0.168f }, { 0.095f, 0.016f, 0.012f }, 0.008f, { 0, 0, 0 }, Visor );
    rbox( BoneE::HEAD, s.m_armor, { 0.0f, 0.29f, -0.01f }, { 0.025f, 0.03f, 0.13f }, 0.015f );
    for ( float side : { -1.0f, 1.0f } )
        rbox( BoneE::HEAD, s.m_metal, { side * 0.14f, 0.13f, 0.0f }, { 0.02f, 0.05f, 0.05f }, 0.015f );
    rbox( BoneE::HEAD, s.m_metal, { 0.1f, 0.33f, -0.08f }, { 0.006f, 0.08f, 0.006f }, 0.003f );

    // Rifle.
    rbox( BoneE::RIFLE, s.m_dark, { 0.0f, 0.0f, 0.0f }, { 0.04f, 0.065f, 0.22f }, 0.015f );
    rbox( BoneE::RIFLE, s.m_metal, { 0.0f, 0.015f, 0.32f }, { 0.032f, 0.035f, 0.12f }, 0.012f );
    add( BoneE::RIFLE, s.m_metal, own, [&]( MeshBuilder &b )
         { b.cylinder( place( { 0.0f, 0.015f, 0.42f }, { PI * 0.5f, 0.0f, 0.0f } ), 0.016f, 0.2f, 10 ); } );
    rbox( BoneE::RIFLE, s.m_dark, { 0.0f, -0.02f, -0.3f }, { 0.03f, 0.055f, 0.1f }, 0.015f );
    rbox( BoneE::RIFLE, s.m_dark, { 0.0f, -0.1f, -0.08f }, { 0.022f, 0.06f, 0.03f }, 0.01f, { 0.3f, 0.0f, 0.0f } );
    rbox( BoneE::RIFLE, s.m_metal, { 0.0f, -0.1f, 0.07f }, { 0.025f, 0.06f, 0.035f }, 0.01f );
    rbox( BoneE::RIFLE, s.m_glow, { 0.041f, 0.02f, 0.02f }, { 0.004f, 0.012f, 0.13f }, 0.002f, { 0, 0, 0 }, Cell );
    rbox( BoneE::RIFLE, s.m_dark, { 0.0f, 0.085f, 0.0f }, { 0.018f, 0.022f, 0.06f }, 0.008f );

    // Arms.
    for ( BoneE upper : { BoneE::UPPER_L, BoneE::UPPER_R } )
    {
        rbox( upper, s.m_armor, { 0.0f, -0.15f, 0.0f }, { 0.065f, 0.14f, 0.065f }, 0.03f );
        rbox( upper, s.m_metal, { 0.0f, -0.3f, 0.0f }, { 0.055f, 0.04f, 0.055f }, 0.03f );
    }
    for ( BoneE fore : { BoneE::FORE_L, BoneE::FORE_R } )
    {
        rbox( fore, s.m_armor, { 0.0f, -0.13f, 0.0f }, { 0.07f, 0.13f, 0.07f }, 0.03f );
        rbox( fore, s.m_dark, { 0.0f, -0.2f, 0.0f }, { 0.08f, 0.05f, 0.08f }, 0.025f );
        rbox( fore, s.m_metal, { 0.0f, -0.33f, 0.0f }, { 0.045f, 0.055f, 0.05f }, 0.02f );
    }

    // Legs.
    for ( BoneE thigh : { BoneE::THIGH_L, BoneE::THIGH_R } )
    {
        rbox( thigh, s.m_armor, { 0.0f, -0.2f, 0.01f }, { 0.09f, 0.19f, 0.1f }, 0.04f );
        rbox( thigh, s.m_dark, { 0.0f, -0.18f, 0.1f }, { 0.06f, 0.12f, 0.025f }, 0.015f );
    }
    for ( BoneE shin : { BoneE::SHIN_L, BoneE::SHIN_R } )
    {
        rbox( shin, s.m_metal, { 0.0f, 0.0f, 0.0f }, { 0.07f, 0.06f, 0.07f }, 0.04f );
        rbox( shin, s.m_armor, { 0.0f, 0.0f, 0.08f }, { 0.07f, 0.075f, 0.035f }, 0.025f );
        rbox( shin, s.m_armor, { 0.0f, -0.2f, 0.0f }, { 0.08f, 0.18f, 0.085f }, 0.035f );
    }
    for ( BoneE foot : { BoneE::FOOT_L, BoneE::FOOT_R } )
    {
        rbox( foot, s.m_armor, { 0.0f, -0.045f, 0.05f }, { 0.08f, 0.055f, 0.15f }, 0.03f );
        rbox( foot, s.m_metal, { 0.0f, -0.095f, 0.05f }, { 0.085f, 0.012f, 0.155f }, 0.008f );
    }
}

Vector3 Figure::forward() const
{
    return { sinf( m_yaw ), 0.0f, cosf( m_yaw ) };
}

void Figure::update( float dt, Vector2 wanted, float time )
{
    m_time = time;
    // Accelerate towards the wanted velocity, turn towards where we go.
    Vector2 dv = Vector2Subtract( wanted, m_vel );
    float   step = 18.0f * dt;
    if ( Vector2Length( dv ) > step ) dv = Vector2Scale( Vector2Normalize( dv ), step );
    m_vel = Vector2Add( m_vel, dv );
    float speed = Vector2Length( m_vel );
    if ( Vector2Length( wanted ) > 0.1f )
    {
        float target = atan2f( wanted.x, wanted.y );
        float d = angleTo( m_yaw, target );
        float turn = 11.0f * dt;
        m_yaw += Clamp( d, -turn, turn );
    }
    m_phase = fmodf( m_phase + speed * dt * 2.0f * PI / Stride, 2.0f * PI );
    m_recoil = fmaxf( 0.0f, m_recoil - dt * 7.0f );
}

void Figure::solve()
{
    float speed = Vector2Length( m_vel );
    float amp = Clamp( speed / RunSpeed, 0.0f, 1.0f );
    float ph = m_phase;
    float breath = 0.006f * sinf( m_time * 1.8f ) * ( 1.0f - amp );
    float bob = 0.03f * amp * cosf( 2.0f * ph ) + breath;
    float twist = 0.1f * amp * sinf( ph );
    float lean = 0.15f * amp + 0.03f * m_recoil;

    Matrix root = MatrixMultiply( MatrixRotateY( m_yaw ), MatrixTranslate( m_pos.x, m_pos.y, m_pos.z ) );
    Matrix pelvis = MatrixMultiply(
        MatrixMultiply( MatrixRotateY( twist ), MatrixTranslate( 0.0f, HipHeight + bob, 0.0f ) ), root );
    // The chest turns to the right for the rifle stance and against the hips' swing.
    float  chestYaw = -0.35f - twist * 1.6f;
    Matrix spine = MatrixMultiply( MatrixMultiply( MatrixRotateY( chestYaw ), MatrixRotateX( lean ) ),
                                   MatrixMultiply( MatrixTranslate( 0.0f, 0.1f, 0.0f ), pelvis ) );
    Matrix head = MatrixMultiply( MatrixMultiply( MatrixRotateY( 0.3f + twist * 1.6f + 0.05f * sinf( m_time * 0.7f ) ),
                                                  MatrixRotateX( -lean * 0.7f ) ),
                                  MatrixMultiply( MatrixTranslate( 0.0f, 0.56f, 0.0f ), spine ) );
    // The rifle stays aimed forward: it follows the root, the bob and a little sway, not the chest.
    float  sway = 0.02f * amp * sinf( 2.0f * ph ) + 0.008f * sinf( m_time * 1.3f );
    Matrix aim = MatrixMultiply( MatrixTranslate( 0.0f, HipHeight + bob + 0.1f, 0.0f ), root );
    Matrix rifle = MatrixMultiply(
        MatrixMultiply( MatrixRotateX( sway - 0.12f * m_recoil ), MatrixRotateY( 0.015f * amp * sinf( ph ) ) ),
        MatrixMultiply( MatrixTranslate( -0.06f, 0.24f, 0.24f - 0.05f * m_recoil ), aim ) );
    m_bones[(int)BoneE::PELVIS] = pelvis;
    m_bones[(int)BoneE::SPINE] = spine;
    m_bones[(int)BoneE::HEAD] = head;
    m_bones[(int)BoneE::RIFLE] = rifle;

    // Arms: the hands hold the rifle.
    Vector3 fwd = forward();
    Vector3 left = { fwd.z, 0.0f, -fwd.x };   // +x of the root
    Vector3 shoulderL = Vector3Transform( { 0.29f, 0.44f, 0.0f }, spine );
    Vector3 shoulderR = Vector3Transform( { -0.29f, 0.44f, 0.0f }, spine );
    Vector3 wristR = Vector3Transform( { 0.0f, -0.06f, -0.1f }, rifle );
    Vector3 wristL = Vector3Transform( { 0.0f, -0.05f, 0.2f }, rifle );
    Vector3 down = { 0.0f, -1.0f, 0.0f };
    Vector3 poleR = Vector3Add( Vector3Add( Vector3Scale( left, -1.0f ), down ), Vector3Scale( fwd, -0.3f ) );
    Vector3 poleL = Vector3Add( Vector3Add( Vector3Scale( left, 0.6f ), down ), Vector3Scale( fwd, -0.2f ) );
    twoBone( shoulderR, wristR, UpperArm, Forearm, poleR, fwd, m_bones[(int)BoneE::UPPER_R],
             m_bones[(int)BoneE::FORE_R] );
    twoBone( shoulderL, wristL, UpperArm, Forearm, poleL, fwd, m_bones[(int)BoneE::UPPER_L],
             m_bones[(int)BoneE::FORE_L] );

    // Legs: hips swing, knees bend in the swing, boots stay flat.
    float a = 0.5f * amp;
    for ( int side = 0; side < 2; ++side )
    {
        float  p = ph + ( side ? PI : 0.0f );
        float  s = a * sinf( p );
        float  k = 0.06f + amp * 1.05f * powf( fmaxf( 0.0f, cosf( p ) ), 1.5f );
        float  x = side ? -0.11f : 0.11f;
        Matrix thigh =
            MatrixMultiply( MatrixMultiply( MatrixRotateX( -s ), MatrixTranslate( x, -0.04f, 0.0f ) ), pelvis );
        Matrix shin =
            MatrixMultiply( MatrixMultiply( MatrixRotateX( k ), MatrixTranslate( 0.0f, -Thigh, 0.0f ) ), thigh );
        Matrix foot =
            MatrixMultiply( MatrixMultiply( MatrixRotateX( s - k ), MatrixTranslate( 0.0f, -Shin, 0.0f ) ), shin );
        m_bones[(int)( side ? BoneE::THIGH_R : BoneE::THIGH_L )] = thigh;
        m_bones[(int)( side ? BoneE::SHIN_R : BoneE::SHIN_L )] = shin;
        m_bones[(int)( side ? BoneE::FOOT_R : BoneE::FOOT_L )] = foot;
    }

    m_lampPos = Vector3Transform( { -0.33f, 0.57f, 0.2f }, spine );
    m_lampDir = Vector3Normalize(
        Vector3Add( Vector3Scale( fwd, cosf( 24.0f * DEG2RAD ) ), { 0.0f, -sinf( 24.0f * DEG2RAD ), 0.0f } ) );
    m_muzzle = Vector3Transform( { 0.0f, 0.015f, 0.64f }, rifle );
}

void Figure::pose( std::vector<Drawable> &out )
{
    solve();
    for ( const Part &p : m_parts )
    {
        Drawable d;
        d.m_mesh = p.m_mesh;
        d.m_surface = p.m_surface;
        d.m_transform = m_bones[(int)p.m_bone];
        d.m_emissive = p.m_emissive;
        d.m_spotShadow = false;   // the lamp sits on the figure: no shadow of itself
        out.push_back( d );
    }
}

}   // namespace spaceopera
