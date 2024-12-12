#include <nori/integrator.h>
#include <nori/scene.h>
#include <nori/warp.h>
#include <nori/bsdf.h>

NORI_NAMESPACE_BEGIN

class VolPathMISIntegrator : public Integrator {
public:
    VolPathMISIntegrator(const PropertyList &props) {
        m_max_depth = props.getInteger("max_depth", std::numeric_limits<int>::max());
        m_rr_depth = props.getInteger("m_rr_depth", 3);
    }

//     path reuse version and optimized loop
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
        int emitterCount = scene->getLights().size();
        int bounces = 0;
        Intersection its, its_last;
        Ray3f shadowRay = Ray3f(ray);
        BSDFQueryRecord bRec(its.shFrame.toLocal(-shadowRay.d), its.uv);

        while (bounces < m_max_depth) {
            // Multi Importance Sampling
            // Mats sampling
            if (!scene->rayIntersect(shadowRay, its)) {
                // direct to camera
                if (envMapLight != nullptr) {
                    EmitterQueryRecord eRec(shadowRay.o);
                    eRec.wi = shadowRay.d.normalized();
                    Color3f Li = envMapLight->eval(eRec);

                    // failed bsdf sampling
                    float wMats = 0.0f;

                    if (bounces == 0) {
                        // no its_last, use Mats
                        wMats = 1.0f;
                    } else {
                        float pdfMats = its_last.mesh->getBSDF()->pdf(bRec);

                        // fill in properties of eRecMats
                        // uniform sampling on emitters first
                        float pdfEms = envMapLight->pdf(eRec) / emitterCount;

                        // special handling for Discrete bsdf
                        if (bRec.measure == EDiscrete) {
                            wMats = 1.0f;
                        } else if (pdfMats >= Epsilon){
                            // continuous cases
                            // Solid angle measure
                            // successful bsdf sampling
                            wMats = pdfMats / (pdfEms + pdfMats);
                            //otherwise failed material sampling in continuous cases: no contribution, wMats => 0
                        }
                    }
                    Lo += t * wMats * Li;
                }
                break;
            }

            // update the length of ray
            float length = (its.p - shadowRay.o).norm();
            shadowRay = Ray3f(shadowRay, Epsilon, length - Epsilon);

            // material sampling
            // intersection with emitter
            if (its.mesh->isEmitter()) {
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
                    float wMats = 0.0f;

                    if (bounces == 0) {
                        // not its_last, use Mats
                        wMats = 1.0f;
                    } else {
                        float pdfEms = its.mesh->getEmitter()->pdf(eRec) / emitterCount;

                        float pdfMats = its_last.mesh->getBSDF()->pdf(bRec);

                        // uniform sampling on emitters first
                        // failed bsdf sampling
                        // special handling for Discrete bsdf
                        if (bRec.measure == EDiscrete) {
                            wMats = 1.0f;
                        } else if (pdfMats >= Epsilon){
                            // continuous cases
                            // Solid angle measure
                            // successful bsdf sampling
                            wMats = pdfMats / (pdfEms + pdfMats);
                            //otherwise failed material sampling in continuous cases: no contribution, wMats => 0
                        }
                    }

                    // not inside a medium, no attenuation
                    Lo += t * wMats * Li;
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
            // sigma_s * Tr / pdf_t
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
                // outside a medium
                // Emitter Sampling
                // w.r.t its.p!
                EmitterQueryRecord eRec(its.p);
                const Emitter * emitter = scene->getRandomEmitter(sampler->next1D());

                Color3f Li = emitter->sample(eRec, sampler->next2D());

                // Ems contribution
                if (!scene->rayIntersect(eRec.shadowRay)) {
                    // no occlusion with emitter
                    // uniform sampling on emitters first
                    float pdfEms = eRec.pdf / emitterCount;

                    // remember to local frame for wi and wo
                    BSDFQueryRecord bRecEms(its.shFrame.toLocal(-shadowRay.d), its.shFrame.toLocal(eRec.wi), EMeasure::ESolidAngle, its.uv);
                    float pdfMats = its.mesh->getBSDF()->pdf(bRecEms);
                    // failed emitter sampling
                    float wEms = 0.0f;
                    // Discrete emitter case: pdf eval => 0, eRec.pdf => 1
                    if (emitter->isDelta()) {
                        wEms = 1.0f;
                    } else if (pdfEms >= Epsilon) {
                        // continuous cases
                        // successful emitter sampling
                        wEms = pdfEms / (pdfEms + pdfMats);
                        //otherwise failed emitter sampling in continuous cases: no contribution, wEms => 0
                    }
                    // bsdf distanceValue for Ems
                    Color3f bsdfValueEms = its.mesh->getBSDF()->eval(bRecEms);
                    Lo += t * wEms * bsdfValueEms * Li * its.shFrame.n.dot(eRec.wi) * emitterCount;
                }

                // NEE to get new its
                bRec = BSDFQueryRecord(its.shFrame.toLocal(-shadowRay.d), its.uv);
                // included cosine term already
                Color3f bsdfValue = its.mesh->getBSDF()->sample(bRec, sampler->next2D());

                // update throughout
                t *= bsdfValue;

                // new shadowRay
                Vector3f wo = its.shFrame.toWorld(bRec.wo);
                shadowRay = Ray3f(its.p, wo);

                if (wo.dot(its.shFrame.n) < 0.f) {
                    // goes into a shape, use associated medium
                    shadowRay.medium = its.mesh->getMedium();
                } else {
                    // goes out of a shape, use associated medium
                    shadowRay.medium = nullptr;
                }

            }

            // update throughout for NEE
            // record its for next event
            its_last = its;

            bounces++;
        }
        return Lo;
    }

    std::string toString() const {
        return tfm::format(
                "VolPathMISIntegrator[\n"
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

NORI_REGISTER_CLASS(VolPathMISIntegrator, "vol_path_mis");
NORI_NAMESPACE_END