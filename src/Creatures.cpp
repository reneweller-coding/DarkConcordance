/**
 * @file Creatures.cpp
 * @brief Rust lice and the drone press: their parts, the lice's gait, the press's moving parts.
 * @author Rene Weller
 */
#include "Creatures.h"

#include "Rig.h"

#include <raymath.h>

#include <cmath>

namespace spaceopera
{

namespace
{

const int     GroupA[3] = { 0, 4, 2 };   ///< left front, right middle, left rear step together
const int     GroupB[3] = { 3, 1, 5 };   ///< and the other tripod
const Vector4 Own = { -1.0f, 0.0f, 0.0f, 0.0f };

}   // namespace

void LouseModel::build( int rust, int metal, int glow, bool welder, float scale )
{
    m_welder = welder;
    m_metal = metal;
    float s = scale > 0.0f ? scale : ( welder ? 1.7f : 1.0f );
    m_scale = s;
    m_height = 0.42f * s;
    m_thigh = 0.42f * s;
    m_shin = 0.55f * s;
    auto part = [&]( int surface, bool isGlow, bool isTorch, auto shape )
    {
        MeshBuilder b;
        shape( b );
        for ( Mesh m : b.build() ) m_parts.push_back( { m, surface, isGlow, isTorch } );
    };
    auto rbox = [&]( int surface, Vector3 at, Vector3 half, float r, Vector3 rot = { 0, 0, 0 } )
    {
        part( surface, false, false, [&]( MeshBuilder &b )
              { b.roundedBox( place( Vector3Scale( at, s ), rot ), Vector3Scale( half, s ), r * s, 3 ); } );
    };
    rbox( rust, { 0.0f, 0.05f, -0.28f }, { 0.28f, 0.17f, 0.32f }, 0.12f );
    for ( int i = 0; i < 3; ++i )
        rbox( rust, { 0.0f, 0.2f - i * 0.02f, -0.12f - i * 0.17f }, { 0.25f - i * 0.02f, 0.03f, 0.08f }, 0.02f,
              { 0.15f + i * 0.12f, 0.0f, 0.0f } );
    rbox( rust, { 0.0f, 0.08f, 0.12f }, { 0.22f, 0.13f, 0.18f }, 0.08f );
    rbox( metal, { 0.0f, 0.06f, 0.38f }, { 0.15f, 0.1f, 0.11f }, 0.05f );
    for ( float side : { -1.0f, 1.0f } )
        rbox( metal, { side * 0.07f, -0.03f, 0.52f }, { 0.025f, 0.025f, 0.09f }, 0.01f, { 0.0f, side * 0.35f, 0.0f } );
    rbox( metal, { 0.08f, 0.28f, 0.25f }, { 0.01f, 0.12f, 0.01f }, 0.005f );
    part( glow, true, false,
          [&]( MeshBuilder &b )
          {
              b.roundedBox( place( Vector3Scale( { 0.0f, 0.1f, 0.49f }, s ) ),
                            Vector3Scale( { 0.06f, 0.03f, 0.012f }, s ), 0.01f * s, 2 );
          } );
    if ( welder )
    {
        part( metal, false, false,
              [&]( MeshBuilder &b )
              {
                  b.cylinder( place( Vector3Scale( { 0.0f, 0.32f, -0.72f }, s ), { PI * 0.5f, 0.0f, 0.0f } ), 0.12f * s,
                              0.55f * s, 14 );
              } );
        part( metal, false, false,
              [&]( MeshBuilder &b )
              {
                  b.cylinder( place( Vector3Scale( { 0.0f, -0.02f, 0.45f }, s ), { PI * 0.5f, 0.0f, 0.0f } ), 0.05f * s,
                              0.4f * s, 10 );
              } );
        part( glow, false, true,
              [&]( MeshBuilder &b )
              {
                  b.cylinder( place( Vector3Scale( { 0.0f, -0.02f, 0.85f }, s ), { PI * 0.5f, 0.0f, 0.0f } ),
                              0.065f * s, 0.06f * s, 12 );
              } );
    }
    MeshBuilder upper;
    upper.roundedBox( place( { 0.0f, -m_thigh * 0.5f, 0.0f } ), { 0.035f * s, m_thigh * 0.5f, 0.035f * s }, 0.02f * s,
                      2 );
    m_upper = upper.build().front();
    MeshBuilder lower;
    lower.roundedBox( place( { 0.0f, -m_shin * 0.5f, 0.0f } ), { 0.025f * s, m_shin * 0.5f, 0.025f * s }, 0.015f * s,
                      2 );
    m_lower = lower.build().front();
}

Matrix LouseModel::body( const LousePose &pose ) const
{
    float bob = 0.012f * m_scale * sinf( pose.m_time * 7.0f );
    float y = m_height * ( 1.0f - 0.45f * pose.m_crouch ) + bob;
    return MatrixMultiply( MatrixMultiply( MatrixRotateX( -pose.m_pitch ), MatrixRotateY( pose.m_yaw ) ),
                           MatrixTranslate( pose.m_pos.x, pose.m_pos.y + y, pose.m_pos.z ) );
}

Vector3 LouseModel::hip( int i ) const
{
    const float z[3] = { 0.22f, 0.05f, -0.15f };
    float       side = i < 3 ? 1.0f : -1.0f;
    return { side * 0.2f * m_scale, 0.0f, z[i % 3] * m_scale };
}

Vector3 LouseModel::rest( int i ) const
{
    const float z[3] = { 0.45f, 0.05f, -0.38f };
    float       side = i < 3 ? 1.0f : -1.0f;
    return { side * 0.68f * m_scale, -m_height, z[i % 3] * m_scale };
}

void LouseModel::stepLegs( LouseLegs &legs, const LousePose &pose, Vector3 vel, float ground, float dt ) const
{
    Matrix flat =
        MatrixMultiply( MatrixRotateY( pose.m_yaw ), MatrixTranslate( pose.m_pos.x, ground + m_height, pose.m_pos.z ) );
    if ( pose.m_air )
    {
        // Leaping: the legs fold back; they plant again on landing.
        Matrix b = body( pose );
        for ( int i = 0; i < 6; ++i )
        {
            Vector3 r = rest( i );
            legs.m_foot[i] = Vector3Transform( { r.x * 0.75f, r.y * 0.4f, r.z - 0.25f * m_scale }, b );
            legs.m_step[i] = 0.0f;
        }
        legs.m_init = false;
        return;
    }
    Vector3 want[6];
    for ( int i = 0; i < 6; ++i ) want[i] = Vector3Transform( rest( i ), flat );
    if ( !legs.m_init )
    {
        for ( int i = 0; i < 6; ++i )
        {
            legs.m_foot[i] = want[i];
            legs.m_step[i] = 0.0f;
        }
        legs.m_init = true;
    }
    float duration = 0.12f * sqrtf( m_scale );
    for ( int i = 0; i < 6; ++i )
    {
        if ( legs.m_step[i] <= 0.0f ) continue;
        legs.m_step[i] = fminf( 1.0f, legs.m_step[i] + dt / duration );
        float t = legs.m_step[i];
        legs.m_foot[i] = Vector3Lerp( legs.m_from[i], legs.m_to[i], t );
        legs.m_foot[i].y += sinf( t * PI ) * 0.16f * m_scale;
        if ( t >= 1.0f )
        {
            legs.m_step[i] = 0.0f;
            legs.m_foot[i] = legs.m_to[i];
        }
    }
    auto stepping = [&]( const int *group )
    {
        for ( int k = 0; k < 3; ++k )
            if ( legs.m_step[group[k]] > 0.0f ) return true;
        return false;
    };
    bool    aBusy = stepping( GroupA );
    bool    bBusy = stepping( GroupB );
    Vector3 lead = { vel.x * 0.1f, 0.0f, vel.z * 0.1f };
    for ( int g = 0; g < 2; ++g )
    {
        const int *group = g == 0 ? GroupA : GroupB;
        if ( g == 0 ? bBusy : aBusy ) continue;
        for ( int k = 0; k < 3; ++k )
        {
            int     i = group[k];
            Vector3 d = Vector3Subtract( want[i], legs.m_foot[i] );
            d.y = 0.0f;
            float lag = Vector3Length( d );
            if ( lag > 1.4f * m_scale )
            {
                legs.m_foot[i] = want[i];   // knocked far: snap
                continue;
            }
            if ( legs.m_step[i] <= 0.0f && lag > 0.28f * m_scale )
            {
                legs.m_from[i] = legs.m_foot[i];
                legs.m_to[i] = Vector3Add( want[i], lead );
                legs.m_step[i] = 1e-4f;
                if ( g == 0 )
                    aBusy = true;
                else
                    bBusy = true;
            }
        }
    }
}

void LouseModel::draw( const LousePose &pose, const LouseLegs &legs, std::vector<Drawable> &out ) const
{
    Matrix  b = body( pose );
    Vector4 flash =
        pose.m_flash > 0.0f ? Vector4{ 3.0f * pose.m_flash, 3.0f * pose.m_flash, 3.0f * pose.m_flash, 0.0f } : Own;
    for ( const Part &p : m_parts )
    {
        Drawable d;
        d.m_mesh = p.m_mesh;
        d.m_surface = p.m_surface;
        d.m_transform = b;
        d.m_emissive = flash;
        if ( p.m_glow ) d.m_emissive = { 4.0f * ( 1.0f + 2.0f * pose.m_crouch ), 0.35f, 0.08f, 0.0f };
        if ( p.m_torch )
            d.m_emissive = { 6.0f * ( 0.15f + 2.0f * pose.m_torch ), 2.4f * ( 0.15f + 2.0f * pose.m_torch ), 0.5f,
                             0.0f };
        out.push_back( d );
    }
    Vector3 fwd = { sinf( pose.m_yaw ), 0.0f, cosf( pose.m_yaw ) };
    Vector3 left = { fwd.z, 0.0f, -fwd.x };
    for ( int i = 0; i < 6; ++i )
    {
        float   side = i < 3 ? 1.0f : -1.0f;
        Vector3 hipW = Vector3Transform( hip( i ), b );
        Vector3 pole = Vector3Add( { 0.0f, 1.0f, 0.0f }, Vector3Scale( left, side * 0.9f ) );
        Matrix  upper, lower;
        twoBone( hipW, legs.m_foot[i], m_thigh, m_shin, pole, fwd, upper, lower );
        Drawable d;
        d.m_surface = m_metal;
        d.m_emissive = flash;
        d.m_mesh = m_upper;
        d.m_transform = upper;
        out.push_back( d );
        d.m_mesh = m_lower;
        d.m_transform = lower;
        out.push_back( d );
    }
}

void PressModel::add( MeshBuilder &b, int surface, int role )
{
    for ( Mesh m : b.build() ) m_parts.push_back( { m, surface, role } );
}

void PressModel::build( Vector3 origin, int metal, int hazard, int rust, int glow )
{
    m_origin = origin;
    {
        // The housing: a back block and a facade around the core's recess.
        MeshBuilder m;
        m.box( place( { 0.0f, 3.25f, -3.0f } ), { 8.0f, 3.25f, 1.4f } );
        m.box( place( { -4.8f, 3.25f, -0.8f } ), { 3.2f, 3.25f, 0.8f } );
        m.box( place( { 4.8f, 3.25f, -0.8f } ), { 3.2f, 3.25f, 0.8f } );
        m.box( place( { 0.0f, 1.1f, -0.8f } ), { 1.6f, 1.1f, 0.8f } );
        m.box( place( { 0.0f, 5.55f, -0.8f } ), { 1.6f, 0.95f, 0.8f } );
        for ( float x = -7.0f; x <= 7.0f; x += 2.0f )
            if ( fabsf( x ) > 2.6f && fabsf( fabsf( x ) - 4.6f ) > 0.8f )
                m.box( place( { x, 3.2f, 0.12f } ), { 0.15f, 3.2f, 0.12f } );
        // The gantry over the anvil, the sleeves of the ram's pistons, the anvil.
        for ( float x : { -2.6f, 2.6f } )
        {
            m.box( place( { x, 3.3f, 4.3f } ), { 0.25f, 3.3f, 0.25f } );
            m.box( place( { x, 6.55f, 2.2f } ), { 0.25f, 0.25f, 2.1f } );
        }
        m.box( place( { 0.0f, 6.55f, 4.3f } ), { 2.85f, 0.25f, 0.25f } );
        for ( float x : { -1.2f, 1.2f } ) m.cylinder( place( { x, 4.6f, 2.2f } ), 0.32f, 1.95f, 16 );
        m.box( place( { 0.0f, 0.12f, 2.2f } ), { 2.3f, 0.12f, 2.0f } );
        // Machinery on the roof.
        m.box( place( { -5.0f, 7.0f, -2.5f } ), { 1.4f, 0.5f, 1.2f } );
        m.cylinder( place( { 5.0f, 6.5f, -2.5f } ), 0.9f, 1.4f, 20 );
        m.cylinder( place( { 2.0f, 6.5f, -3.4f } ), 0.35f, 2.5f, 12 );
        add( m, metal, 0 );

        MeshBuilder h;
        h.box( place( { 0.0f, 0.35f, 0.03f } ), { 8.0f, 0.35f, 0.04f } );
        for ( float x : { -2.6f, 2.6f } ) h.box( place( { x, 0.5f, 4.3f } ), { 0.27f, 0.5f, 0.27f } );
        h.box( place( { 0.0f, 0.245f, 0.35f } ), { 2.3f, 0.006f, 0.15f } );
        h.box( place( { 0.0f, 0.245f, 4.05f } ), { 2.3f, 0.006f, 0.15f } );
        for ( float x : { -4.6f, 4.6f } )
        {
            h.box( place( { x, 1.75f, 0.05f } ), { 1.05f, 0.12f, 0.06f } );
            for ( float dx : { -1.0f, 1.0f } ) h.box( place( { x + dx, 0.85f, 0.05f } ), { 0.1f, 0.85f, 0.06f } );
        }
        add( h, hazard, 0 );

        MeshBuilder r;
        for ( float y : { 5.6f, 5.95f } )
            r.cylinder( place( { -8.0f, y, 0.2f }, { 0.0f, 0.0f, -PI * 0.5f } ), y > 5.7f ? 0.1f : 0.16f, 16.0f, 14 );
        for ( float side : { -1.0f, 1.0f } )
            r.cylinder( place( { side * 8.0f, 2.2f, 0.5f }, { 0.0f, 0.0f, side * -PI * 0.5f } ), 0.2f, 0.8f, 12 );
        add( r, rust, 0 );
    }
    {
        // The ram (its middle at the origin's height 0; the pose lifts it).
        MeshBuilder m;
        m.roundedBox( place( { 0.0f, 0.0f, 2.2f } ), { 2.0f, 0.8f, 1.85f }, 0.08f, 2 );
        for ( float x : { -1.2f, 1.2f } ) m.cylinder( place( { x, 0.8f, 2.2f } ), 0.22f, 4.3f, 14 );
        add( m, metal, 1 );
        MeshBuilder h;
        h.box( place( { 0.0f, -0.55f, 4.07f } ), { 2.0f, 0.2f, 0.02f } );
        add( h, hazard, 1 );
    }
    for ( int side = 0; side < 2; ++side )
    {
        MeshBuilder s;
        s.box( place( { side ? 0.8f : -0.8f, 3.4f, 0.08f } ), { 0.8f, 1.2f, 0.06f } );
        add( s, metal, 2 + side );
    }
    {
        MeshBuilder          c;
        std::vector<Vector2> sphere;
        for ( int i = 0; i <= 12; ++i )
        {
            float a = -PI * 0.5f + PI * i / 12.0f;
            sphere.push_back( { 0.95f * cosf( a ), 0.95f * sinf( a ) } );
        }
        c.lathe( place( { 0.0f, 3.4f, -1.0f } ), sphere, 24 );
        add( c, glow, 6 );
    }
    for ( int side = 0; side < 2; ++side )
    {
        float       x = side ? 9.2f : -9.2f;
        MeshBuilder t;
        t.lathe( place( { x, 0.0f, 0.5f } ),
                 { { 0.0f, 0.0f },
                   { 1.15f, 0.0f },
                   { 1.15f, 0.0f },
                   { 1.1f, 0.3f },
                   { 1.1f, 4.2f },
                   { 0.8f, 4.6f },
                   { 0.0f, 4.75f } },
                 28 );
        for ( float y : { 0.3f, 4.2f } )
            t.lathe(
                place( { x, y - 0.1f, 0.5f } ),
                { { 1.12f, 0.0f }, { 1.2f, 0.0f }, { 1.2f, 0.0f }, { 1.2f, 0.2f }, { 1.2f, 0.2f }, { 1.12f, 0.2f } },
                28 );
        add( t, metal, 4 + side );
        MeshBuilder band;
        for ( float y : { 1.5f, 3.0f } )
            band.lathe( place( { x, y, 0.5f } ), { { 1.13f, 0.0f }, { 1.13f, 0.25f } }, 28 );
        add( band, glow, 12 + side );
        MeshBuilder broken;
        broken.lathe( place( { x, 0.0f, 0.5f } ),
                      { { 0.0f, 0.0f },
                        { 1.15f, 0.0f },
                        { 1.15f, 0.0f },
                        { 1.1f, 0.3f },
                        { 1.1f, 1.6f },
                        { 0.95f, 1.9f },
                        { 0.9f, 1.4f } },
                      28 );
        add( broken, rust, 10 + side );
        MeshBuilder hatch;
        hatch.box( place( { side ? 4.6f : -4.6f, 0.85f, 0.02f } ), { 0.9f, 0.8f, 0.02f } );
        add( hatch, glow, 8 + side );
    }
    {
        MeshBuilder w;
        for ( float x : { -7.5f, 7.5f } )
            w.roundedBox( place( { x, 6.75f, -0.2f } ), { 0.25f, 0.25f, 0.25f }, 0.1f, 3 );
        add( w, glow, 7 );
    }
}

void PressModel::draw( const PressPose &pose, std::vector<Drawable> &out ) const
{
    Matrix at = MatrixTranslate( m_origin.x, m_origin.y, m_origin.z );
    for ( const Part &p : m_parts )
    {
        Drawable d;
        d.m_mesh = p.m_mesh;
        d.m_surface = p.m_surface;
        d.m_transform = at;
        d.m_emissive = Own;
        switch ( p.m_role )
        {
        case 1: d.m_transform = MatrixMultiply( MatrixTranslate( 0.0f, pose.m_ram, 0.0f ), at ); break;
        case 2: d.m_transform = MatrixMultiply( MatrixTranslate( -1.55f * pose.m_shutter, 0.0f, 0.0f ), at ); break;
        case 3: d.m_transform = MatrixMultiply( MatrixTranslate( 1.55f * pose.m_shutter, 0.0f, 0.0f ), at ); break;
        case 4:
        case 5:
        {
            int side = p.m_role - 4;
            if ( !pose.m_tank[side] ) continue;
            float f = pose.m_tankFlash[side];
            if ( f > 0.0f ) d.m_emissive = { 3.0f * f, 3.0f * f, 3.0f * f, 0.0f };
            break;
        }
        case 10:
        case 11:
            if ( pose.m_tank[p.m_role - 10] ) continue;
            break;
        case 12:
        case 13:
            if ( !pose.m_tank[p.m_role - 12] ) continue;
            d.m_emissive = pose.m_dead ? Vector4{ 0, 0, 0, 0 } : Vector4{ 0.4f, 1.6f, 3.5f, 0.0f };
            break;
        case 6:
        {
            float pulse = 0.7f + 0.3f * sinf( pose.m_time * 6.0f );
            d.m_emissive =
                pose.m_dead ? Vector4{ 0.05f, 0.02f, 0.0f, 0.0f }
                            : Vector4{ 6.0f * pulse + 4.0f * pose.m_coreFlash, 2.2f * pulse + 4.0f * pose.m_coreFlash,
                                       0.4f + 4.0f * pose.m_coreFlash, 0.0f };
            break;
        }
        case 7: d.m_emissive = { 8.0f * pose.m_warn, 0.5f * pose.m_warn, 0.2f * pose.m_warn, 0.0f }; break;
        case 8:
        case 9:
        {
            float g = pose.m_hatch[p.m_role - 8];
            d.m_emissive = { 0.3f + 5.0f * g, 0.12f + 2.0f * g, 0.04f + 0.4f * g, 0.0f };
            if ( pose.m_dead ) d.m_emissive = { 0.0f, 0.0f, 0.0f, 0.0f };
            break;
        }
        default: break;
        }
        out.push_back( d );
    }
}

}   // namespace spaceopera
