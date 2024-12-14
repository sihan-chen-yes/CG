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
#include <nori/medium.h>

NORI_NAMESPACE_BEGIN

class VolPathMATSIntegrator : public Integrator {
public:
    VolPathMATSIntegrator(const PropertyList &props) {
        m_max_depth = props.getInteger("max_depth", 12);
        m_rr_depth = props.getInteger("m_rr_depth", 3);
    }

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
        while (bounces < m_max_depth) {
            if (!scene->rayIntersect(shadowRay, its)) {
                // direct to camera
                if (envMapLight != nullptr) {
                    EmitterQueryRecord eRec(shadowRay.o);
                    eRec.wi = shadowRay.d.normalized();
                    Color3f Li = envMapLight->eval(eRec);
                    // ignore failed eval
                    if (envMapLight->pdf(eRec) > 0.f) {
                        Lo += t * Li;
                    }
                }
                break;
            }

            // update the length of ray
            float length = (its.p - shadowRay.o).norm();
            shadowRay = Ray3f(shadowRay, Epsilon, length - Epsilon);

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
                if (shadowRay.medium) {
                    // account Tr for attenuation if inside a medium
                    Lo += t * shadowRay.medium->Tr(shadowRay) * Li;
                } else {
                    // not inside a medium, no attenuation
                    Lo += t * Li;
                }
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

            bool sampleMedium = false;
            // m_sigma_s * Tr / pdf_t
            Color3f distanceValue(1.f);
            // medium interaction point
            Interaction ita;
            if (shadowRay.medium) {
                // ray associated with medium
                // sample t to decide how far to go
                distanceValue = shadowRay.medium->sample(shadowRay, sampler, ita);
                // still inside the volume not touching the boundary
                sampleMedium = ita.isValid();
            }

            if (sampleMedium) {
                // scattering inside a medium
                // self emission contribution
                Lo += t * shadowRay.medium->Le(shadowRay);

                // wi for pRec
                PhaseFunctionQueryRecord pRec(-shadowRay.d);
                // sample wo for pRec
                float phaseValue = ita.phase->sample(pRec, sampler->next2D());

                // update throughout for NEE
                t *= distanceValue * phaseValue;

                // new shadowRay
                // still use the old medium
                shadowRay = Ray3f(ita.p, pRec.wo, shadowRay.medium);

            } else {
                //path-reuse
                BSDFQueryRecord bRec(its.shFrame.toLocal(-shadowRay.d), its.uv);
                // included cosine term already
                Color3f bsdfValue = its.mesh->getBSDF()->sample(bRec, sampler->next2D());

                // update throughout for NEE
                t *= bsdfValue;

                // new shadowRay
                Vector3f wo = its.shFrame.toWorld(bRec.wo);
                shadowRay = Ray3f(its.p, wo);

                if (wo.dot(its.shFrame.n) < 0.f) {
                    // goes into a shape, use associated medium
                    shadowRay.medium = its.mesh->getMedium();
                } else {
                    // goes into a shape, use associated medium
                    shadowRay.medium = nullptr;
                }

            }

            bounces++;
        }
        return Lo;
    }

    std::string toString() const {
        return tfm::format(
                "VolPathMATSIntegrator[\n"
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

NORI_REGISTER_CLASS(VolPathMATSIntegrator, "vol_path_mats");
NORI_NAMESPACE_END