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

#include <nori/integrator.h>
#include <nori/scene.h>
#include <nori/warp.h>
#include <nori/bsdf.h>

NORI_NAMESPACE_BEGIN
class MaterialIntegrator : public Integrator {
public:
    MaterialIntegrator(const PropertyList &props) {
        // no properties
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Color3f(0.0f);  // No intersection

        // fixed outgoing direction
        Vector3f wo(0.f, 0.f, 1.f);
        // convert direction from world frame to local frame
        // remember to flip direction
        Vector3f wi = its.toLocal(-ray.d);
        // construct bsdf query record
        BSDFQueryRecord bRec(wi, wo, EMeasure::ESolidAngle, its.uv);

        const BSDF *bsdf = its.mesh->getBSDF();
        // return bsdf val
        return bsdf->eval(bRec);
    }

    std::string toString() const {
        return "MaterialIntegrator[]";
    }
};

NORI_REGISTER_CLASS(MaterialIntegrator, "material");
NORI_NAMESPACE_END