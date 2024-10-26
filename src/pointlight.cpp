#include <nori/emitter.h>

NORI_NAMESPACE_BEGIN

class PointLight : public Emitter {
public:
    PointLight(const PropertyList & propList) {
        m_position = propList.getPoint3("position", Point3f ());
        m_power = propList.getColor("power", Color3f ());
    }

    virtual std::string toString() const override {
        return tfm::format(
                "PointLight[\n"
                "  position = %s,\n"
                "  power = %s,\n"
                "]",
                m_position.toString(),
                m_power.toString()
        );
    }

    virtual Color3f eval(const EmitterQueryRecord & lRec) const override {
        float distanceSquared = (m_position - lRec.ref).squaredNorm();
        // radiance Li
        return m_power / (4 * M_PI * distanceSquared);
    }

    virtual Color3f sample(EmitterQueryRecord & lRec, const Point2f & sample) const override {
        // fill properties of query record before return
        lRec.p = m_position;
        lRec.wi = (m_position - lRec.ref).normalized();
        lRec.pdf = pdf(lRec);
        lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, (lRec.p - lRec.ref).norm() - Epsilon);

        // radiance Li
        return eval(lRec);
    }

    virtual float pdf(const EmitterQueryRecord &lRec) const override {
        return 0.0f;
    }

protected:
    Point3f m_position;
    Color3f m_power;
};

NORI_REGISTER_CLASS(PointLight, "point");
NORI_NAMESPACE_END
