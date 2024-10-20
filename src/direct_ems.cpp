#include <nori/integrator.h>
#include <nori/scene.h>
#include <nori/warp.h>
#include <nori/bsdf.h>

NORI_NAMESPACE_BEGIN

class DirectEMSIntegrator : public Integrator {
public:
    DirectEMSIntegrator(const PropertyList &props) {
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

        EmitterQueryRecord eRec(its.p);
        // uniform random emitter sampling for speeding up
        const Emitter * emitter = scene->getRandomEmitter(sampler->next1D());
        int emitterCount = scene->getLights().size();

        // incident radiance
        Color3f Li = emitter->sample(eRec, sampler->next2D());
        if (!scene->rayIntersect(eRec.shadowRay)) {
            // no occlusion with emitter
            // remember to local frame for wi and wo
            BSDFQueryRecord bRec(its.shFrame.toLocal(-ray.d), its.shFrame.toLocal(eRec.wi), EMeasure::ESolidAngle, its.uv);
            Color3f bsdfValue = its.mesh->getBSDF()->eval(bRec);
            // f(x) / uniform pdf => f(x) * cnt
            Lo += bsdfValue * Li * its.shFrame.n.dot(eRec.wi) * emitterCount;
        }
        return Lo;
    }

    std::string toString() const {
        return tfm::format(
                "DirectEMSIntegrator[]");
    }

};

NORI_REGISTER_CLASS(DirectEMSIntegrator, "direct_ems");
NORI_NAMESPACE_END