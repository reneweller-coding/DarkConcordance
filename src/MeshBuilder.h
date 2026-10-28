/**
 * @file MeshBuilder.h
 * @brief Builds raylib meshes out of shapes: boxes, rounded boxes, lathed profiles, quads, faceted prisms. Everything
 *        the look test shows is made of these (no modelled assets).
 * @author Rene Weller
 */
#pragma once

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace spaceopera
{

/** @brief Collects triangles (positions, normals, UVs, colours) and turns them into GPU meshes of at most 65000 vertices. */
class MeshBuilder
{
public:
    Color m_color = WHITE;   ///< colour of the vertices added next (alpha: baked occlusion)

    /**
     * @brief A box with flat faces.
     * @param[in] m     where it goes (no scaling)
     * @param[in] half  half its size
     */
    void box( const Matrix &m, Vector3 half );

    /**
     * @brief A box with rounded edges and smooth normals.
     * @param[in] m       where it goes (no scaling)
     * @param[in] half    half its size
     * @param[in] radius  radius of the edges
     * @param[in] seg     segments a quarter round
     */
    void roundedBox( const Matrix &m, Vector3 half, float radius, int seg = 4 );

    /**
     * @brief A profile turned around the y axis.
     * @param[in] m        where it goes (no scaling)
     * @param[in] profile  (radius, height) points from bottom to top; a repeated point makes a hard edge
     * @param[in] sides    segments around
     * @param[in] from     start angle (radians)
     * @param[in] to       end angle (radians; 2 pi for a whole turn)
     */
    void lathe( const Matrix &m, const std::vector<Vector2> &profile, int sides, float from = 0.0f,
                float to = 6.2831853f );

    /**
     * @brief A cylinder along y with caps.
     * @param[in] m       where it goes (its base centre)
     * @param[in] radius  radius
     * @param[in] height  height
     * @param[in] sides   segments around
     */
    void cylinder( const Matrix &m, float radius, float height, int sides = 16 );

    /**
     * @brief A quad with UVs from 0 to 1 (a to b is u, a to d is v).
     * @param[in] a  first corner
     * @param[in] b  second corner
     * @param[in] c  third corner
     * @param[in] d  fourth corner
     */
    void quad( Vector3 a, Vector3 b, Vector3 c, Vector3 d );

    /**
     * @brief A faceted prism: rings of corners joined with flat faces, closed at both ends.
     * @param[in] m      where it goes
     * @param[in] rings  each ring's corners (same count per ring), from bottom to top
     */
    void prism( const Matrix &m, const std::vector<std::vector<Vector3>> &rings );

    /**
     * @brief A smooth skin through rings of points (cloth over a body): each corner's normal from its neighbours.
     * @param[in] m       where it goes
     * @param[in] rings   each ring's points (same count per ring), from bottom to top; open at both ends
     * @param[in] closed  each ring goes all the way round (false: an arc, open at its ends)
     */
    void loft( const Matrix &m, const std::vector<std::vector<Vector3>> &rings, bool closed );

    /**
     * @brief One flat triangle.
     * @param[in] a  first corner
     * @param[in] b  second corner
     * @param[in] c  third corner
     */
    void triangle( Vector3 a, Vector3 b, Vector3 c );

    /**
     * @brief The meshes, uploaded to the GPU; the builder is empty afterwards.
     * @return one mesh per 65000 vertices
     */
    std::vector<Mesh> build();

    /** @return true if nothing has been added */
    bool empty() const { return m_pos.empty() && m_done.empty(); }

private:
    struct Chunk
    {
        std::vector<Vector3>        m_pos;   ///< positions
        std::vector<Vector3>        m_nrm;   ///< normals
        std::vector<Vector2>        m_uv;    ///< texture coordinates
        std::vector<Color>          m_col;   ///< colours
        std::vector<unsigned short> m_idx;   ///< triangles
    };

    /** @brief Starts a new chunk if `count` more vertices would not fit. */
    void reserve( size_t count );
    /** @brief Adds a vertex; @return its index */
    unsigned short vertex( Vector3 p, Vector3 n, Vector2 uv );

    std::vector<Vector3>        m_pos;    ///< positions of the open chunk
    std::vector<Vector3>        m_nrm;    ///< its normals
    std::vector<Vector2>        m_uv;     ///< its texture coordinates
    std::vector<Color>          m_col;    ///< its colours
    std::vector<unsigned short> m_idx;    ///< its triangles
    std::vector<Chunk>          m_done;   ///< full chunks
};

/**
 * @brief A frame from three axes and a position.
 * @param[in] x  image of the x axis
 * @param[in] y  image of the y axis
 * @param[in] z  image of the z axis
 * @param[in] t  translation
 * @return the matrix
 */
Matrix frame( Vector3 x, Vector3 y, Vector3 z, Vector3 t );

/**
 * @brief Translation and rotation (radians, applied x, y, z) in one.
 * @param[in] t    translation
 * @param[in] rot  rotation
 * @return the matrix
 */
Matrix place( Vector3 t, Vector3 rot = { 0.0f, 0.0f, 0.0f } );

}   // namespace spaceopera
