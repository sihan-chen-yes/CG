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
#include <nori/bsdf.h>

NORI_NAMESPACE_BEGIN

class AlbedoIntegrator : public Integrator {
public:
    AlbedoIntegrator(const PropertyList &props) {
        /* No parameters this time */
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Color3f(0.0f);

        /* Retrieve the diffuse reflectance (albedo) from the material */
        const BSDF *bsdf = its.mesh->getBSDF();
        if (bsdf) {
            /* Assuming the BSDF has a method to query diffuse reflectance */
            Vector3f wi = its.toLocal(-ray.d);
            BSDFQueryRecord bRec(wi, its.uv);
            return bsdf->getAlbedo(bRec);
        }

        /* If no BSDF is associated, return black */
        return Color3f(0.0f);
    }

    std::string toString() const {
        return "AlbedoIntegrator[]";
    }
};

NORI_REGISTER_CLASS(AlbedoIntegrator, "albedo");
NORI_NAMESPACE_END
