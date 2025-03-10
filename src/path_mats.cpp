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

class PathMATSIntegrator : public Integrator {
public:
    PathMATSIntegrator(const PropertyList &props) {
        m_max_depth = props.getInteger("max_depth", std::numeric_limits<int>::max());
        m_rr_depth = props.getInteger("m_rr_depth", 3);
    }

//    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
//        /* Find the surface that is visible in the requested direction */
//        Intersection its;
//        if (!scene->rayIntersect(ray, its))
//            return Color3f(0.0f); // No intersections, then return black
//
//        //outgoing radiance
//        Color3f Lo(0.0f);
//
//        if (its.mesh->isEmitter()) {
//            EmitterQueryRecord eRec(ray.o, its.p, its.shFrame.n);
//            // Le self emission
//            Lo += its.mesh->getEmitter()->eval(eRec);
//        }
//
//        Color3f t = Color3f(1.0f);
//        Vector3f wi = -ray.d;
//        int bounces = 0;
//
//        while (true) {
//            if (bounces >= 3) {
//                float success = std::min(std::max(t.x(), std::max(t.y(), t.z())), 0.99f);
//
//                if (sampler->next1D() < success) {
//                    t /= success;
//                } else {
//                    break;
//                }
//            }
//
//            BSDFQueryRecord bRec(its.shFrame.toLocal(wi), its.uv);
//            // included cosine term already
//            Color3f bsdfValue = its.mesh->getBSDF()->sample(bRec, sampler->next2D());
//            // eRec ref
//            EmitterQueryRecord eRec(its.p);
//            // convention not the same for eRec and bRec!
//            eRec.wi = its.shFrame.toWorld(bRec.wo);
//            eRec.shadowRay = Ray3f(its.p, eRec.wi);
//            eRec.shadowRay.mint = Epsilon;
//            // intersection with emitter
//            // direct illumination
//            Intersection emitterIts;
//            if (scene->rayIntersect(eRec.shadowRay, emitterIts) && emitterIts.mesh->isEmitter()) {
//                // fill in properties of eRec
//                // light source
//                eRec.p = emitterIts.p;
//                eRec.n = emitterIts.shFrame.n;
//                // incident radiance
//                Color3f Li = emitterIts.mesh->getEmitter()->eval(eRec);
//                Lo += t * bsdfValue * Li;
//            }
//
//            // NEE sampling
//            BSDFQueryRecord NEEbRec(its.shFrame.toLocal(wi), its.uv);
//            Color3f NEEBsdfValue = its.mesh->getBSDF()->sample(NEEbRec, sampler->next2D());
//            Ray3f shadowRay = Ray3f(its.p, its.shFrame.toWorld(NEEbRec.wo));
//            shadowRay.mint = Epsilon;
//            // update next its
//            if (!scene->rayIntersect(shadowRay, its)) {
//                break;
//            }
//            // update throughout
//            t *= NEEBsdfValue;
//            wi = -shadowRay.d;
//            bounces++;
//        }
//
//        return Lo;
//    }

    // path reuse version
//    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
//        /* Find the surface that is visible in the requested direction */
//        Intersection its;
//        if (!scene->rayIntersect(ray, its))
//            return Color3f(0.0f); // No intersections, then return black
//
//        //outgoing radiance
//        Color3f Lo(0.0f);
//
//        if (its.mesh->isEmitter()) {
//            EmitterQueryRecord eRec(ray.o, its.p, its.shFrame.n);
//            // Le self emission
//            Lo += its.mesh->getEmitter()->eval(eRec);
//        }
//
//        Color3f t = Color3f(1.0f);
//        Vector3f wi = -ray.d;
//        int bounces = 0;
//        while (true) {
//            if (bounces >= 3) {
//                float success = std::min(std::max(t.x(), std::max(t.y(), t.z())), 0.99f);
//
//                if (sampler->next1D() < success) {
//                    t /= success;
//                } else {
//                    break;
//                }
//            }
//
//            //path-reuse
//            BSDFQueryRecord bRec(its.shFrame.toLocal(wi), its.uv);
//            // included cosine term already
//            Color3f bsdfValue = its.mesh->getBSDF()->sample(bRec, sampler->next2D());
//            // eRec ref
//            EmitterQueryRecord eRec(its.p);
//            // convention not the same for eRec and bRec!
//            eRec.wi = its.shFrame.toWorld(bRec.wo);
//            eRec.shadowRay = Ray3f(its.p, eRec.wi);
//            eRec.shadowRay.mint = Epsilon;
//            if (!scene->rayIntersect(eRec.shadowRay, its)) {
//                break;
//            } else {
//                // update throughout
//                t *= bsdfValue;
//                // for NEE
//                wi = -eRec.shadowRay.d;
//                // intersection with emitter
//                // direct illumination
//                if (its.mesh->isEmitter()) {
//                    // fill in properties of eRec
//                    // light source
//                    eRec.p = its.p;
//                    eRec.n = its.shFrame.n;
//                    // incident radiance
//                    Color3f Li = its.mesh->getEmitter()->eval(eRec);
//                    Lo += t * Li;
//                }
//            bounces++;
//            }
//        }
//
//        return Lo;
//    }

    // path reuse version and optimized loop
    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        // search the env map light
        Emitter *envMapLight = nullptr;
        for (int i = 0; i < scene->getLights().size(); ++i) {
            Emitter * light = scene->getLights().at(i);
            if (light->isEnvMapLight()) {
                envMapLight = light;
                break;
            }
        }

        //outgoing radiance
        Color3f Lo(0.0f);

        Color3f t = Color3f(1.0f);
        int bounces = 0;
        Intersection its;
        Ray3f shadowRay = Ray3f(ray);
        while (true) {
            if (!scene->rayIntersect(shadowRay, its)) {
                // direct to camera
                if (envMapLight != nullptr) {
                    EmitterQueryRecord eRec(shadowRay.o);
                    eRec.wi = shadowRay.d.normalized();
                    Color3f Li = envMapLight->eval(eRec);
                    Lo += t * Li;
                }
                break;
            }
            // intersection with emitter
            // direct illumination
            if (its.mesh->isEmitter()) {
                // eRec ref
                EmitterQueryRecord eRec(shadowRay.o);
                // convention not the same for eRec and bRec!
                eRec.wi = shadowRay.d;
                eRec.shadowRay = shadowRay;
                eRec.shadowRay.mint = Epsilon;

                // fill in properties of eRec
                // light source
                eRec.p = its.p;
                eRec.n = its.shFrame.n;
                // incident radiance
                Color3f Li = its.mesh->getEmitter()->eval(eRec);
                Lo += t * Li;
            }

            // start Russian Roulette after m_rr_depth bounces
            if (bounces > m_rr_depth) {
                float success = std::min(std::max(t.x(), std::max(t.y(), t.z())), 0.99f);

                if (sampler->next1D() < success) {
                    t /= success;
                } else {
                    break;
                }
            }

            //path-reuse
            BSDFQueryRecord bRec(its.shFrame.toLocal(-shadowRay.d), its.uv);
            // included cosine term already
            Color3f bsdfValue = its.mesh->getBSDF()->sample(bRec, sampler->next2D());

            // update throughout for NEE
            t *= bsdfValue;
            shadowRay = Ray3f(its.p, its.shFrame.toWorld(bRec.wo));
            bounces++;
        }
        return Lo;
    }

    std::string toString() const {
        return tfm::format(
                "PathMATSIntegrator[\n"
                "  m_max_depth = %d\n"
                "  m_rr_depth = %d\n"
                "]",
                m_max_depth,
                m_rr_depth
        );
    }

protected:
    int m_max_depth;
    int m_rr_depth;
};

NORI_REGISTER_CLASS(PathMATSIntegrator, "path_mats");
NORI_NAMESPACE_END