#include <nori/integrator.h>
#include <nori/scene.h>
#include <nori/warp.h>
#include <nori/bsdf.h>

NORI_NAMESPACE_BEGIN

class DirectMISIntegrator : public Integrator {
public:
    DirectMISIntegrator(const PropertyList &props) {
        // no properties
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its1;
        if (!scene->rayIntersect(ray, its1))
            return Color3f(0.0f); // No intersections, then return black

        //outgoing radiance
        Color3f Lo(0.0f);

        if (its1.mesh->isEmitter()) {
            EmitterQueryRecord eRec(ray.o, its1.p, its1.shFrame.n);
            // Le self emission
            Lo += its1.mesh->getEmitter()->eval(eRec);
        }

        // Multi Importance Sampling
        // Emitter Sampling
        EmitterQueryRecord eRec1(its1.p);
        // uniform random emitter sampling for speeding up
        const Emitter * emitter = scene->getRandomEmitter(sampler->next1D());
        int emitterCount = scene->getLights().size();

        // incident radiance
        Color3f Li = emitter->sample(eRec1, sampler->next2D());
        // Ems contribution following direct_ems
        if (!scene->rayIntersect(eRec1.shadowRay)) {
            // no occlusion with emitter
            // uniform sampling on emitters first
            float pdfEms = eRec1.pdf / emitterCount;

            // remember to local frame for wi and wo
            BSDFQueryRecord bRec1(its1.shFrame.toLocal(-ray.d), its1.shFrame.toLocal(eRec1.wi), EMeasure::ESolidAngle, its1.uv);
            float pdfMats = its1.mesh->getBSDF()->pdf(bRec1);
            float wEms = 1.0f;
            // when pdfEms = 0, use default weight as 1
            // otherwise use formula to calculate
            if (pdfEms >= Epsilon) {
                wEms = pdfEms / (pdfEms + pdfMats);
            }
            Color3f bsdfValue = its1.mesh->getBSDF()->eval(bRec1);
            // f(x) / uniform pdf => f(x) * cnt
            Lo += wEms * bsdfValue * Li * its1.shFrame.n.dot(eRec1.wi) * emitterCount;
        }

        // BSDF Sampling
        BSDFQueryRecord bRec2(its1.shFrame.toLocal(-ray.d), its1.uv);
        // included cosine term already
        Color3f cosBsdfValue = its1.mesh->getBSDF()->sample(bRec2, sampler->next2D());
        // eRec ref
        EmitterQueryRecord eRec2(its1.p);
        // convention not the same for eRec and bRec2!
        eRec2.wi = its1.shFrame.toWorld(bRec2.wo);
        eRec2.shadowRay = Ray3f(its1.p, eRec2.wi);
        eRec2.shadowRay.mint = Epsilon;

        Intersection its2;
        // intersection with emitter
        if (scene->rayIntersect(eRec2.shadowRay, its2) && its2.mesh->isEmitter()) {
            float pdfMats = its1.mesh->getBSDF()->pdf(bRec2);

            // fill in properties of eRecMats
            // light source
            eRec2.p = its2.p;
            eRec2.n = its2.shFrame.n;
            // uniform sampling on emitters first
            float pdfEms = its2.mesh->getEmitter()->pdf(eRec2) / emitterCount;
            float wMats = 1.0f;
            // when pdfMats = 0, use default weight as 1
            // otherwise use formula to calculate
            if (pdfMats >= Epsilon) {
                wMats = pdfMats / (pdfEms + pdfMats);
            }
            // incident radiance
            Color3f Li = its2.mesh->getEmitter()->eval(eRec2);
            Lo += wMats * cosBsdfValue * Li;
        }

        return Lo;
    }

    std::string toString() const {
        return tfm::format(
                "DirectMISIntegrator[]");
    }

};

NORI_REGISTER_CLASS(DirectMISIntegrator, "direct_mis");
NORI_NAMESPACE_END