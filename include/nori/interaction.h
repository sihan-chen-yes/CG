/*
    This file is part of Nori, a simple educational ray tracer

    Copyright (c) 2024 by Sihan Chen

    Nori is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Nori is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#if !defined(__NORI_INTERACTION_H)
#define __NORI_INTERACTION_H

#include <nori/object.h>
#include <nori/frame.h>

NORI_NAMESPACE_BEGIN

/**
 * \brief Intersection data structure
 *
 * This data structure records local information about a ray-triangle intersection.
 * This includes the position, traveled ray distance, uv coordinates, as well
 * as well as two local coordinate frames (one that corresponds to the true
 * geometry, and one that is used for shading computations).
 */
struct Intersection {
    /// Position of the surface intersection
    Point3f p;
    /// Unoccluded distance along the ray
    float t;
    /// UV coordinates, if any
    Point2f uv;
    /// Shading frame (based on the shading normal)
    Frame shFrame;
    /// Geometric frame (based on the true geometry)
    Frame geoFrame;
    /// Pointer to the associated shape
    const Shape *mesh;

    /// Create an uninitialized intersection record
    Intersection() : mesh(nullptr) { }

    Intersection(const Point3f &p) : p(p), mesh(nullptr) { }

    /// Transform a direction vector into the local shading frame
    Vector3f toLocal(const Vector3f &d) const {
        return shFrame.toLocal(d);
    }

    /// Transform a direction vector from local to world coordinates
    Vector3f toWorld(const Vector3f &d) const {
        return shFrame.toWorld(d);
    }

    /// Return a human-readable summary of the intersection record
    std::string toString() const;
};

/**
 * \brief Interaction data structure
 *
 * This data structure records local information about a ray-medium interaction.
 */

/**
 * \brief Intersection data structure
 *
 * This data structure records local information about a ray-medium interaction.
 * With ray direction and phase function as extra properties
 */
struct Interaction: public Intersection {

    /// Direction of the ray
    Vector3f d;

    // phase function of the participating media
    const PhaseFunction *phase;

    Interaction() : Intersection(), phase(nullptr) { }

    Interaction(const Point3f &p, const Vector3f &d, const PhaseFunction *phase): Intersection(p), d(d), phase(phase) { }

    bool isValid() const { return phase != nullptr; }

    /// Return a human-readable summary of the interaction record
    std::string toString() const;
};

NORI_NAMESPACE_END

#endif /* __NORI_INTERACTION_H */
