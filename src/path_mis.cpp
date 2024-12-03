#include <nori/integrator.h>
#include <nori/scene.h>
#include <nori/warp.h>
#include <nori/bsdf.h>

NORI_NAMESPACE_BEGIN

class PathMISIntegrator : public Integrator {
public:
    PathMISIntegrator(const PropertyList &props) {
        // no properties
        m_max_depth = props.getInteger("max_depth", 12);
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
//        int emitterCount = scene->getLights().size();
//        int bounces = 0;
//
//        while (true) {
//            if (bounces > 3) {
//                float success = std::min(std::max(t.x(), std::max(t.y(), t.z())), 0.99f);
//
//                if (sampler->next1D() < success) {
//                    t /= success;
//                } else {
//                    break;
//                }
//            }
//
//            // Multi Importance Sampling
//            // Emitter Sampling
//            EmitterQueryRecord eRec1(its.p);
//            const Emitter * emitter = scene->getRandomEmitter(sampler->next1D());
//
//            Color3f Li = emitter->sample(eRec1, sampler->next2D());
//
//            // Ems contribution
//            if (!scene->rayIntersect(eRec1.shadowRay)) {
//                // no occlusion with emitter
//                // uniform sampling on emitters first
//                float pdfEms = eRec1.pdf / emitterCount;
//
//                // remember to local frame for wi and wo
//                BSDFQueryRecord bRec1(its.shFrame.toLocal(wi), its.shFrame.toLocal(eRec1.wi), EMeasure::ESolidAngle, its.uv);
//                float pdfMats = its.mesh->getBSDF()->pdf(bRec1);
//                // failed emitter sampling
//                float wEms = 0.0f;
//                // successful emitter sampling
//                if (pdfEms >= Epsilon) {
//                    wEms = pdfEms / (pdfEms + pdfMats);
//                }
//                Color3f bsdfValue = its.mesh->getBSDF()->eval(bRec1);
//                // f(x) / uniform pdf => f(x) * cnt
//                Lo += t * wEms * bsdfValue * Li * its.shFrame.n.dot(eRec1.wi) * emitterCount;
//            }
//
//            // Mats contribution
//            // BSDF Sampling
//            BSDFQueryRecord bRec2(its.shFrame.toLocal(wi), its.uv);
//            // included cosine term already
//            Color3f cosBsdfValue = its.mesh->getBSDF()->sample(bRec2, sampler->next2D());
//            // eRec ref
//            EmitterQueryRecord eRec2(its.p);
//            // convention not the same for eRec and bRec2!
//            eRec2.wi = its.shFrame.toWorld(bRec2.wo);
//            eRec2.shadowRay = Ray3f(its.p, eRec2.wi);
//            eRec2.shadowRay.mint = Epsilon;
//
//            Intersection emitterIts;
//            // intersection with emitter
//            if (scene->rayIntersect(eRec2.shadowRay, emitterIts) && emitterIts.mesh->isEmitter()) {
//                float pdfMats = its.mesh->getBSDF()->pdf(bRec2);
//
//                // fill in properties of eRecMats
//                // light source
//                eRec2.p = emitterIts.p;
//                eRec2.n = emitterIts.shFrame.n;
//                // uniform sampling on emitters first
//                float pdfEms = emitterIts.mesh->getEmitter()->pdf(eRec2) / emitterCount;
//                // failed bsdf sampling
//                float wMats = 0.0f;
//                // special handling for Discrete bsdf
//                if (bRec2.measure == EDiscrete) {
//                    wMats = 1.0f;
//                } else if (pdfMats >= Epsilon){
//                    // Solid angle measure
//                    // successful bsdf sampling
//                    wMats = pdfMats / (pdfEms + pdfMats);
//                }
//                // incident radiance
//                Color3f Li = emitterIts.mesh->getEmitter()->eval(eRec2);
//                Lo += t * wMats * cosBsdfValue * Li;
//            }
//
//            // NEE sampling
//            Color3f NEEBsdfValue = its.mesh->getBSDF()->sample(bRec2, sampler->next2D());
//            Ray3f shadowRay = Ray3f(its.p, its.shFrame.toWorld(bRec2.wo));
//            shadowRay.mint = Epsilon;
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

        /* Find the surface that is visible in the requested direction */
        Intersection its, nextIts;
        if (!scene->rayIntersect(ray, its)) {
            // direct to camera
            if (envMapLight != nullptr) {
                EmitterQueryRecord eRec(ray.o);
                eRec.wi = ray.d.normalized();
                Color3f Li = envMapLight->eval(eRec);
                return Li;
            }
            return Color3f(0.0f); // No intersections and no env map light, then return black
        }

        //outgoing radiance
        Color3f Lo(0.0f);

        if (its.mesh->isEmitter()) {
            EmitterQueryRecord eRec(ray.o, its.p, its.shFrame.n);
            // Le self emission
            Lo += its.mesh->getEmitter()->eval(eRec);
        }

        Color3f t = Color3f(1.0f);
        Vector3f wi = -ray.d;
        // exclude env map
        int emitterCount = scene->getLights().size();
        int bounces = 0;

        while (bounces < m_max_depth) {
            if (bounces > 3) {
                float success = std::min(std::max(t.x(), std::max(t.y(), t.z())), 0.99f);

                if (sampler->next1D() < success) {
                    t /= success;
                } else {
                    break;
                }
            }

            // Multi Importance Sampling
            // Emitter Sampling
            EmitterQueryRecord eRec1(its.p);
            const Emitter * emitter = scene->getRandomEmitter(sampler->next1D());

            Color3f Li = emitter->sample(eRec1, sampler->next2D());

            /*
             * for env map importance sampling validation
             */
            if (emitter->isEnvMapLight()) {
                envMapLight->counter();
            }

            // Ems contribution
            if (!scene->rayIntersect(eRec1.shadowRay)) {
                // no occlusion with emitter
                // uniform sampling on emitters first
                float pdfEms = eRec1.pdf / emitterCount;

                // remember to local frame for wi and wo
                BSDFQueryRecord bRec1(its.shFrame.toLocal(wi), its.shFrame.toLocal(eRec1.wi), EMeasure::ESolidAngle, its.uv);
                float pdfMats = its.mesh->getBSDF()->pdf(bRec1);
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
                Color3f bsdfValue = its.mesh->getBSDF()->eval(bRec1);
                Lo += t * wEms * bsdfValue * Li * its.shFrame.n.dot(eRec1.wi) * emitterCount;
            }

            // Mats contribution
            // BSDF Sampling
            BSDFQueryRecord bRec2(its.shFrame.toLocal(wi), its.uv);
            // included cosine term already
            Color3f cosBsdfValue = its.mesh->getBSDF()->sample(bRec2, sampler->next2D());
            // eRec ref
            EmitterQueryRecord eRec2(its.p);
            // convention not the same for eRec and bRec2!
            eRec2.wi = its.shFrame.toWorld(bRec2.wo);
            eRec2.shadowRay = Ray3f(its.p, eRec2.wi);
            eRec2.shadowRay.mint = Epsilon;

            // update throughout
            t *= cosBsdfValue;

            if (!scene->rayIntersect(eRec2.shadowRay, nextIts)) {
                if (envMapLight != nullptr) {
                    float pdfMats = its.mesh->getBSDF()->pdf(bRec2);

                    // fill in properties of eRecMats
                    // uniform sampling on emitters first
                    float pdfEms = envMapLight->pdf(eRec2) / emitterCount;
                    // failed bsdf sampling
                    float wMats = 0.0f;
                    // special handling for Discrete bsdf
                    if (bRec2.measure == EDiscrete) {
                        wMats = 1.0f;
                    } else if (pdfMats >= Epsilon){
                        // continuous cases
                        // Solid angle measure
                        // successful bsdf sampling
                        wMats = pdfMats / (pdfEms + pdfMats);
                        //otherwise failed material sampling in continuous cases: no contribution, wMats => 0
                    }
                    Color3f Li = envMapLight->eval(eRec2);
                    Lo += t * wMats * Li;
                }
                break;
            }

            // intersection with emitter
            if (nextIts.mesh->isEmitter()) {
                float pdfMats = its.mesh->getBSDF()->pdf(bRec2);

                // fill in properties of eRecMats
                // light source
                eRec2.p = nextIts.p;
                eRec2.n = nextIts.shFrame.n;
                // uniform sampling on emitters first
                float pdfEms = nextIts.mesh->getEmitter()->pdf(eRec2) / emitterCount;
                // failed bsdf sampling
                float wMats = 0.0f;
                // special handling for Discrete bsdf
                if (bRec2.measure == EDiscrete) {
                    wMats = 1.0f;
                } else if (pdfMats >= Epsilon){
                    // continuous cases
                    // Solid angle measure
                    // successful bsdf sampling
                    wMats = pdfMats / (pdfEms + pdfMats);
                    //otherwise failed material sampling in continuous cases: no contribution, wMats => 0
                }
                // incident radiance
                Color3f Li = nextIts.mesh->getEmitter()->eval(eRec2);
                Lo += t * wMats * Li;
            }

            wi = -eRec2.wi;
            its = nextIts;
            bounces++;
        }

        return Lo;
    }

    std::string toString() const {
        return tfm::format(
                "PathMISIntegrator[]");
    }

protected:
    int m_max_depth;

};

NORI_REGISTER_CLASS(PathMISIntegrator, "path_mis");
NORI_NAMESPACE_END