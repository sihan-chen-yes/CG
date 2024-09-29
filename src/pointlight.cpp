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
        throw NoriException("To implement...");
    }

    virtual Color3f sample(EmitterQueryRecord & lRec, const Point2f & sample) const override {
        float distanceSquared = (m_position - lRec.ref).squaredNorm();

        // fill properties of query record before return
        lRec.pdf = 1.0f;
        lRec.p = m_position;
        lRec.wi = (m_position - lRec.ref).normalized();
        lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, (lRec.p - lRec.ref).norm() - Epsilon);

        // radience Li
        return m_power / (4 * M_PI * distanceSquared);
    }

    virtual float pdf(const EmitterQueryRecord &lRec) const override {
        return 1.0f;
    }

protected:
    Point3f m_position;
    Color3f m_power;
};

NORI_REGISTER_CLASS(PointLight, "point");
NORI_NAMESPACE_END
