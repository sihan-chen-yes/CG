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

class DirectIntegrator : public Integrator {
public:
    DirectIntegrator(const PropertyList &props) {
        // no properties
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Color3f(0.0f); // No intersections, then return black

        Color3f Lo(0.0f);
        //emitter list
        const std::vector<Emitter *> emitters = scene->getLights();

        for (Emitter *emitter: emitters) {
            EmitterQueryRecord eRec(its.p);

            // incident radiance
            // will fill in corresponding properties of eRec in sample function
            Color3f Li = emitter->sample(eRec, sampler->next2D());

            // intersections test
            if (!scene->rayIntersect(eRec.shadowRay)) {
                // no occlusion with emitter
                // remember to local frame for wi and wo
                BSDFQueryRecord bRec(its.shFrame.toLocal(-ray.d), its.shFrame.toLocal(eRec.wi), EMeasure::ESolidAngle, its.uv);
                Color3f bsdf_value = its.mesh->getBSDF()->eval(bRec);
                Lo += bsdf_value * Li * abs(its.shFrame.n.dot(eRec.wi));
            }
        }
        return Lo; // outgoing radiance Lo
    }

    std::string toString() const {
        return tfm::format(
                "DirectIntegrator[]");
    }

};

NORI_REGISTER_CLASS(DirectIntegrator, "direct");
NORI_NAMESPACE_END