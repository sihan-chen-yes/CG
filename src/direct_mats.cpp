#include <nori/integrator.h>
#include <nori/scene.h>
#include <nori/warp.h>
#include <nori/bsdf.h>

NORI_NAMESPACE_BEGIN

class DirectMATSIntegrator : public Integrator {
public:
    DirectMATSIntegrator(const PropertyList &props) {
        // no properties
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Color3f(0.0f); // No intersections, then return black

        //outgoing radiance
        Color3f Lo(0.0f);

        if (its.mesh->isEmitter()) {
            EmitterQueryRecord eRec(ray.o, its.p, its.shFrame.n);
            // Le self emission
            Lo += its.mesh->getEmitter()->eval(eRec);
        }

        BSDFQueryRecord bRec(its.shFrame.toLocal(-ray.d), its.uv);
        // included cosine term already
        Color3f cosBsdfValue = its.mesh->getBSDF()->sample(bRec, sampler->next2D());
        // eRec ref
        EmitterQueryRecord eRec(its.p);
        // convention not the same for eRec and bRec!
        eRec.wi = its.shFrame.toWorld(bRec.wo);
        eRec.shadowRay = Ray3f(its.p, eRec.wi);
        eRec.shadowRay.mint = Epsilon;

        // intersection with emitter
        if (scene->rayIntersect(eRec.shadowRay, its) && its.mesh->isEmitter()) {
            // fill in properties of eRec
            // light source
            eRec.p = its.p;
            eRec.n = its.shFrame.n;
            // incident radiance
            Color3f Li = its.mesh->getEmitter()->eval(eRec);
            Lo += cosBsdfValue * Li;
        }
        return Lo;
    }

    std::string toString() const {
        return tfm::format(
                "DirectMATSIntegrator[]");
    }

};

NORI_REGISTER_CLASS(DirectMATSIntegrator, "direct_mats");
NORI_NAMESPACE_END