#include <nori/integrator.h>
#include <nori/scene.h>
#include <nori/warp.h>
NORI_NAMESPACE_BEGIN

class AverageVisibilityIntegrator : public Integrator {
public:
    AverageVisibilityIntegrator(const PropertyList &props) {
        m_length = props.getFloat("length");
        std::cout << "Parameter value was : " << m_length << std::endl;
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Color3f(1.0f);  // No intersection, return fully visible (white)

        // Sample a new direction on the hemisphere with respect to the surface normal
        Vector3f d = Warp::sampleUniformHemisphere(sampler, its.shFrame.n);

        // Construct a second ray in the sampled direction with a user-defined length
        Ray3f secondRay(its.p, d, Epsilon, m_length);

        // Check if the shadow ray intersects any object in the scene
        Intersection secondIts;
        if (!scene->rayIntersect(secondRay, secondIts))
            return Color3f(1.0f); // No intersection, return fully visible (white)

        return Color3f(0.0f);  // Ray is occluded (black)
    }

    std::string toString() const {
        return tfm::format(
                "AverageVisibilityIntegrator[\n"
                "    length = %f, \n"
                "]",
                m_length);
    }

protected:
    float m_length;
};

NORI_REGISTER_CLASS(AverageVisibilityIntegrator, "av");
NORI_NAMESPACE_END