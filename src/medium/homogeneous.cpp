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

#include <nori/sampler.h>
#include <nori/medium.h>
#include <string>

NORI_NAMESPACE_BEGIN

class HomogeneousMedium : public Medium {
public:
    HomogeneousMedium(const PropertyList &props): m_phaseFunction(nullptr) {
        sigma_a = props.getColor("sigma_a", Color3f(0.5));
        sigma_s = props.getColor("sigma_s", Color3f(0.5));
        m_sampleEmitter = props.getBoolean("sampleEmitter", true);

        sigma_t = sigma_a + sigma_s;
    }

    ~HomogeneousMedium() {
        if (m_phaseFunction != nullptr) {
            delete m_phaseFunction;
        }
    }

    virtual void activate() override {
        if(!m_phaseFunction) {
            /* If no material was assigned, instantiate an isotropic HenyeyGreenstein Phase Function */
            PropertyList l;
            l.setFloat("g", 0.0f);
            m_phaseFunction = static_cast<PhaseFunction *>(NoriObjectFactory::createInstance("hg", l));
            m_phaseFunction->activate();
        }
    }

    virtual void addChild(NoriObject *obj) override {
        switch (obj->getClassType()) {
            case EPhaseFunction:
                if (m_phaseFunction)
                    throw NoriException("There is already a phase function defined!");
                m_phaseFunction = static_cast<PhaseFunction *>(obj);
                break;

            default:
                throw NoriException("HomogeneousMedium::addChild(<%s>) is not supported!",
                                    classTypeName(obj->getClassType()));
        }
    }

    bool sampleEmitter() const override {
        return m_sampleEmitter;
    }

    Color3f Tr(const Ray3f &ray) const override {
        return Exp(-sigma_t * std::min(ray.maxt * ray.d.norm(), std::numeric_limits<float>::max()));
    }

    Color3f sample(const Ray3f &ray, Sampler *sampler,
                   Interaction &mi) const override {
        // Sample a channel and distance along the ray
        int channel = std::min((int)(sampler->next1D() * 3), 2);
        float dist = -std::log(1 - sampler->next1D()) / sigma_t[channel];
        float t = std::min(dist / ray.d.norm(), ray.maxt);
        bool sampledMedium = t < ray.maxt;
        if (sampledMedium) {
            mi = Interaction((ray)(t), ray.reverse().d, m_phaseFunction);
        }

        // Compute the transmittance and sampling density
        Color3f Tr = Exp(-sigma_t * std::min(t, std::numeric_limits<float>::max()) * ray.d.norm());

        // Return weighting factor for scattering from homogeneous medium
        Color3f density = sampledMedium ? (sigma_t * Tr) : Tr;
        float pdf = 0;
        for (int i = 0; i < 3; ++i) pdf += density[i];
        pdf *= 1.f / 3.f;
        if (pdf == 0.f) {
            // CHECK(Tr.IsBlack());
            if (!Tr.isZero(SEpsilon)) {
                throw NoriException("Tr is not Zero! when pdf is 0.f");
            }
            pdf = 1.f;
        }

        if (sampledMedium) {
            return Tr * sigma_s / pdf;
        } else {
            return Tr / pdf;
        }
    }

    virtual std::string toString() const override {
        return tfm::format(
                "HomogeneousMedium[\n"
                "  sigma_a = %s,\n"
                "  sigma_s = %s,\n"
                "  sigma_t = %s,\n"
                "  sampleEmitter = %d,\n"
                "  phaseFunction = %s\n"
                "]",
                sigma_a.toString(),
                sigma_s.toString(),
                sigma_t.toString(),
                m_sampleEmitter,
                m_phaseFunction ? indent(m_phaseFunction->toString()) : std::string("null")
        );
    }

private:
    Color3f sigma_a;
    Color3f sigma_s;
    // sigma_t = sigma_a + sigma_s
    Color3f sigma_t;
    bool m_sampleEmitter;
    PhaseFunction *m_phaseFunction;
};

NORI_REGISTER_CLASS(HomogeneousMedium, "homogeneous");
NORI_NAMESPACE_END
