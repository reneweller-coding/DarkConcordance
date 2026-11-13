/**
 * @file MeshBuilder.h
 * @brief Builds raylib meshes out of shapes: boxes, rounded boxes, lathed profiles, quads, faceted prisms. Everything
 *        the look test shows is made of these (no modelled assets).
 * @author Rene Weller
 */
#pragma once

#include <raylib.h>

#include <cstddef>
#include <functional>
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
     * @brief A lumpy ellipsoid (a clump of leaves, a bush, a crown): its radius swells and dips with a smooth noise;
     *        its underside darker (the colour's alpha, the baked occlusion).
     * @param[in] m      where it goes (its middle; no scaling)
     * @param[in] radii  its half extents
     * @param[in] bumps  how far the noise moves the surface (a share of the radius)
     * @param[in] seed   which lumps
     * @param[in] seg    segments from pole to pole (twice as many around)
     * @param[in] rock   a stone: its hollows and its underside darker, moss on its top (the colour's green)
     */
    void blob( const Matrix &m, Vector3 radii, float bumps, unsigned seed, int seg = 8, bool rock = false );

    /**
     * @brief A tube along a line (a trunk, a branch, a stalk): a ring round each point, open at both ends.
     * @param[in] path   its middle line (at least two points)
     * @param[in] radii  its radius at each point
     * @param[in] sides  segments around
     */
    void tube( const std::vector<Vector3> &path, const std::vector<float> &radii, int sides = 8 );

    /**
     * @brief A surface over a square of the ground plan, its height a function of x and z (a hedge's top): a grid
     *        through the given steps, its normals from the function, so that neighbouring squares meet seamlessly.
     * @param[in] x0      the square's west edge
     * @param[in] z0      its north edge
     * @param[in] steps   the grid's offsets from there along x and along z (from 0 to the square's size)
     * @param[in] height  the height at (x, z)
     * @param[in] colour  the colour at (x, z) (empty: m_color)
     */
    void field( float x0, float z0, const std::vector<float> &steps, const std::function<float( float, float )> &height,
                const std::function<Color( float, float )> &colour = nullptr );

    /**
     * @brief A quad with its own normal for all four corners (a card of leaves or grass, lit like the volume it stands
     *        for); UVs as quad().
     * @param[in] a       first corner
     * @param[in] b       second corner
     * @param[in] c       third corner
     * @param[in] d       fourth corner
     * @param[in] normal  the normal
     */
    void card( Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 normal );

    /**
     * @brief A card as above with a normal of its own at each corner (leaves lit as the crown round them: each corner's
     *        normal from the crown's middle, so that the light runs smoothly over the cards).
     * @param[in] corners  the four corners (a, b, c, d as above)
     * @param[in] normals  their normals
     * @param[in] uv0      its picture's corner in the texture (a part of an atlas)
     * @param[in] uvSize   its picture's size in the texture
     */
    void card( const Vector3 corners[4], const Vector3 normals[4], Vector2 uv0 = { 0.0f, 0.0f }, float uvSize = 1.0f );

    /**
     * @brief One flat triangle.
     * @param[in] a  first corner
     * @param[in] b  second corner
     * @param[in] c  third corner
     */
    void triangle( Vector3 a, Vector3 b, Vector3 c );

    /**
     * @brief A quad as quad() (the same UVs and normal), cut into a grid of smaller ones (so that a colour baked into
     *        its corners, the occlusion at a wall's foot, can change across it).
     * @param[in] a      first corner
     * @param[in] b      second corner
     * @param[in] c      third corner
     * @param[in] d      fourth corner
     * @param[in] steps  quads of the grid along each side
     */
    void patch( Vector3 a, Vector3 b, Vector3 c, Vector3 d, int steps );

    /**
     * @brief A block over a square of the ground plan whose top is a grid (so that a colour baked into its corners,
     *        the occlusion at a wall's foot, can change across it); its four sides plain, no bottom.
     * @param[in] x0     the square's west edge
     * @param[in] z0     its north edge
     * @param[in] size   its edge
     * @param[in] y0     its bottom
     * @param[in] y1     its top
     * @param[in] steps  squares of the top's grid along each edge
     */
    void slab( float x0, float z0, float size, float y0, float y1, int steps );

    /**
     * @brief A slab as above with a hole in its top (a sunken channel's opening): the grid's squares i0 <= i < i1 along
     *        x and j0 <= j < j1 along z are left out.
     * @param[in] x0     the square's west edge
     * @param[in] z0     its north edge
     * @param[in] size   its edge
     * @param[in] y0     its bottom
     * @param[in] y1     its top
     * @param[in] steps  squares of the top's grid along each edge
     * @param[in] i0     the hole's first square along x
     * @param[in] i1     one past its last
     * @param[in] j0     its first along z
     * @param[in] j1     one past its last
     */
    void slabHole( float x0, float z0, float size, float y0, float y1, int steps, int i0, int i1, int j0, int j1 );

    /**
     * @brief The top of a slab alone (a floor's top laid of two materials: a hall's band along its walls): the grid's
     *        squares that `keep` takes.
     * @param[in] x0     the square's west edge
     * @param[in] z0     its north edge
     * @param[in] size   its edge
     * @param[in] y      its height
     * @param[in] steps  squares of the grid along each edge
     * @param[in] keep   whether square i (along x), j (along z) is laid
     */
    void top( float x0, float z0, float size, float y, int steps, const std::function<bool( int, int )> &keep );

    /**
     * @brief The four sides of a slab alone (as slab() lays them).
     * @param[in] x0    the square's west edge
     * @param[in] z0    its north edge
     * @param[in] size  its edge
     * @param[in] y0    its bottom
     * @param[in] y1    its top
     */
    void sides( float x0, float z0, float size, float y0, float y1 );

    /**
     * @brief Names each triangle's material in a patchwork (Surface::m_layerMode 2): its vertices' u becomes four
     *        times the number the function gives for the triangle's middle and normal (0 the plain one, 1 and 2 its
     *        variants), v a half. The meshes are triplanar: their UVs are free for it.
     * @param[in] material  the material for a triangle's middle and its normal
     */
    void tag( const std::function<int( Vector3, Vector3 )> &material );

    /**
     * @brief Sets every vertex's UV from its position (the ground's shapes, Surface::m_layerMode 3).
     * @param[in] uv  the UV at a point
     */
    void mapUv( const std::function<Vector2( Vector3 )> &uv );

    /**
     * @brief A sheet through a grid of given points (a terrain): each point with its own normal and colour, each
     *        square of the grid split along its flatter diagonal.
     * @param[in] pos  the points, row by row (nx a row, nz rows)
     * @param[in] nrm  their normals
     * @param[in] col  their colours
     * @param[in] nx   points in a row
     * @param[in] nz   rows
     */
    void sheet( const std::vector<Vector3> &pos, const std::vector<Vector3> &nrm, const std::vector<Color> &col, int nx,
                int nz );

    /**
     * @brief Bakes occlusion into every vertex added so far: its colour's alpha (which darkens the ambient light) times
     *        the occlusion, its colour a share of that (which darkens the direct light too, as a painted shadow does).
     * @param[in] occlusion  what reaches a point with a normal (1 all, 0 nothing)
     * @param[in] direct     the share of the darkening the direct light gets (0 .. 1)
     */
    void shade( const std::function<float( Vector3, Vector3 )> &occlusion, float direct );

    /**
     * @brief The meshes, uploaded to the GPU; the builder is empty afterwards.
     * @return one mesh per 65000 vertices
     */
    std::vector<Mesh> build();

    /**
     * @brief The meshes cut into square tiles on the ground plan, uploaded to the GPU; the builder is empty
     *        afterwards. A level's walls as one long run of chunks would be drawn whole wherever a corner of them is
     *        seen (and into both shadow maps); tiles let the frustum leave out what lies elsewhere.
     * @param[in] tile  the tiles' edge in metres (a triangle goes to the tile of its middle)
     * @return one mesh per tile and 65000 vertices
     */
    std::vector<Mesh> build( float tile );

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
 * @brief Smooth value noise in three dimensions.
 * @param[in] p     where
 * @param[in] seed  which noise
 * @return -1 .. 1
 */
float smoothNoise( Vector3 p, unsigned seed );

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
