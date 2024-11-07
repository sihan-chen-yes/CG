/*
    This file is part of Nori, a simple educational ray tracer

    Copyright (c) 2015 by Wenzel Jakob

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

#include <nori/bsdf.h>
#include <nori/frame.h>

NORI_NAMESPACE_BEGIN

/// Ideal dielectric BSDF
class Dielectric : public BSDF {
public:
    Dielectric(const PropertyList &propList) {
        /* Interior IOR (default: BK7 borosilicate optical glass) */
        m_intIOR = propList.getFloat("intIOR", 1.5046f);

        /* Exterior IOR (default: air) */
        m_extIOR = propList.getFloat("extIOR", 1.000277f);
    }

    virtual Color3f eval(const BSDFQueryRecord &) const override {
        /* Discrete BRDFs always evaluate to zero in Nori */
        return Color3f(0.0f);
    }

    virtual float pdf(const BSDFQueryRecord &) const override {
        /* Discrete BRDFs always evaluate to zero in Nori */
        return 0.0f;
    }

    virtual Color3f sample(BSDFQueryRecord &bRec, const Point2f &sample) const override {
        Vector3f n = Vector3f(0.0f, 0.0f, 1.0f);
        float cosThetaI, cosThetaT, sinThetaI, sinThetaT;
        cosThetaI = Frame::cosTheta(bRec.wi);
        // consider the incident direction
        if (Frame::cosTheta(bRec.wi) < 0) {
            n = -n;
            cosThetaI = -cosThetaI;
            //relative IOR: incident / outgoing
            bRec.eta = m_intIOR / m_extIOR;
        } else {
            //relative IOR: incident / outgoing
            bRec.eta = m_extIOR / m_intIOR;
        }
        sinThetaI = sqrt(1 - cosThetaI * cosThetaI);

        // fill in other bRec properties
        bRec.measure = EDiscrete;

        float reflected_coeff = fresnel(Frame::cosTheta(bRec.wi), m_extIOR, m_intIOR);
        float refracted_coeff = 1.0f - reflected_coeff;
        if (sample.x() < reflected_coeff) {
            //reflection event
            bRec.wo = Vector3f(
                    -bRec.wi.x(),
                    -bRec.wi.y(),
                    bRec.wi.z()
            );
            // don't forget the pdf of event happening!
            return Color3f(reflected_coeff) / reflected_coeff;
        } else {
            //refraction event: fixed wo according to Snell's law
            sinThetaT = bRec.eta * sinThetaI;
            //total internal reflection is processed in fresnel calculation
            cosThetaT = sqrt(1 - sinThetaT * sinThetaT);
            Vector3f t = bRec.wi - bRec.wi.dot(n) * n;
            // unit tangent vector
            t.normalize();
            bRec.wo = -cosThetaT * n - sinThetaT * t;
            // don't forget the pdf of event happening!
            return bRec.eta * bRec.eta * Color3f(refracted_coeff) / refracted_coeff;
        }
    }

    virtual std::string toString() const override {
        return tfm::format(
            "Dielectric[\n"
            "  intIOR = %f,\n"
            "  extIOR = %f\n"
            "]",
            m_intIOR, m_extIOR);
    }
private:
    float m_intIOR, m_extIOR;
};

NORI_REGISTER_CLASS(Dielectric, "dielectric");
NORI_NAMESPACE_END
