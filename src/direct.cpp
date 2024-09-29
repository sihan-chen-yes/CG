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

            // incident radience
            // will fill in corresponding properties of eRec in sample function
            Color3f Li = emitter->sample(eRec, sampler->next2D());

            // intersections test
            if (!scene->rayIntersect(eRec.shadowRay)) {
                // no occlusion with emitter
                // remember to local frame for wi and wo
                BSDFQueryRecord bRec(its.shFrame.toLocal(eRec.wi), its.shFrame.toLocal(-ray.d), EMeasure::ESolidAngle, its.uv);
                Color3f bsdf_value = its.mesh->getBSDF()->eval(bRec);
                Lo += bsdf_value * Li * abs(its.shFrame.n.dot(eRec.wi));
            }
        }
        return Lo; // outgoing radience Lo
    }

    std::string toString() const {
        return tfm::format(
                "DirectIntegrator[]");
    }

};

NORI_REGISTER_CLASS(DirectIntegrator, "direct");
NORI_NAMESPACE_END