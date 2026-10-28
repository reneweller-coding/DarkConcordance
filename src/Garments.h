/**
 * @file Garments.h
 * @brief Clothes over the skinned bodies (Quaternius' base bodies): coats, hoods, scarves, packs, bags, belts, vests,
 *        collars, caps, aprons, knee pads and pockets. Each piece is made in the body's rest pose, fitted to the
 *        body's own outline (measured from its mesh at load), and rides on one bone as the hair does on the head.
 * @author Rene Weller
 */
#pragma once

#include "Skin.h"

#include <raylib.h>

#include <vector>

namespace spaceopera
{

/** @brief What a person can wear over the shader's clothes. */
enum class GarmentE : int
{
    COAT,       ///< a long coat: its skirt behind, a flap on each thigh, a stand-up collar
    HOOD,       ///< a hood down on the back
    HOOD_UP,    ///< a hood over the head (the hair hidden)
    SCARF,      ///< wound round the neck, an end on the chest
    BACKPACK,   ///< a pack with a bedroll, straps over the shoulders
    SATCHEL,    ///< a bag on the left hip, its strap across the chest
    TOOLBELT,   ///< a broad belt with pouches
    VEST,       ///< a work vest with pockets, open in front
    COLLAR,     ///< a jacket's stand-up collar
    CAP,        ///< a knitted cap (long hair hidden)
    APRON,      ///< an apron with a bib
    KNEEPADS,   ///< pads on the knees
    POCKETS,    ///< cargo pockets on the thighs
    BOOTS,      ///< boots over the feet (the body's toes hidden)
    COUNT       ///< how many
};

/** @brief Which colour a piece takes. */
enum class GarmentColourE : int
{
    UPPER,     ///< the jacket's (the shader's colour above the belt)
    LOWER,     ///< the trousers'
    ACCENT,    ///< the person's second colour (a scarf, a cap, a vest)
    LEATHER,   ///< brown leather
    DARK,      ///< straps, buckles, pads
    COUNT      ///< how many
};

/** @brief One piece: a mesh in the body's rest pose on a bone of it. */
struct GarmentPiece
{
    Mesh           m_mesh;     ///< in the body's rest pose (model space)
    int            m_bone;     ///< the body's bone it rides on
    GarmentColourE m_colour;   ///< its colour
};

/**
 * @brief Makes every garment for a body.
 * @param[in] body  the body (its rest pose and its mesh give the sizes)
 * @return per garment (GarmentE) its pieces, uploaded
 */
std::vector<std::vector<GarmentPiece>> buildGarments( const SkinModel &body );

/** @return whether the garment hides long hair (a cap or a hood up) */
bool hidesHair( GarmentE g );

}   // namespace spaceopera
