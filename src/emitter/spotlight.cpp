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

#include <nori/emitter.h>

NORI_NAMESPACE_BEGIN

class SpotLight : public Emitter {
public:
    SpotLight(const PropertyList & propList) {
        m_position = propList.getPoint3("position", Point3f (0.0f, 0.0f, 0.0f));
        m_intensity = propList.getColor("intensity", Color3f (1000.0f, 1000.0f, 1000.0f));
        m_lightToWorld = propList.getTransform("toWorld", Transform());
        m_worldToLight = m_lightToWorld.getInverseMatrix();
        m_baseDir = Vector3f (0.0f, 0.0f, -1.0f);
        // change from degrees to radian
        m_thetaMax = propList.getFloat("thetaMax", 30.0) * M_PI / 180.0f;
        m_cosThetaMax = std::cos(m_thetaMax);
        m_thetaFall = propList.getFloat("thetaFall", 5.0) * M_PI / 180.0f;
        m_cosThetaFall = std::cos(m_thetaFall);
        m_baseColor = propList.getColor("baseColor", Color3f(1.0f));
    }

    virtual std::string toString() const override {
        return tfm::format(
                "SpotLight[\n"
                "  position = %s,\n"
                "  intensity = %s,\n"
                "  toWorld = %s,\n"
                "  cosThetaMax = %f,\n"
                "  ThetaMax = %f,\n"
                "  cosThetaFall = %f,\n"
                "  ThetaFall = %f,\n"
                "]",
                m_position.toString(),
                m_intensity.toString(),
                m_lightToWorld.toString(),
                m_cosThetaMax,
                m_thetaMax,
                m_cosThetaFall,
                m_thetaFall
        );
    }

    virtual Color3f eval(const EmitterQueryRecord & lRec) const override {
        // not directly sample from point light
        // delta function set to 0
        return Color3f(0.0f);
    }

    virtual Color3f sample(EmitterQueryRecord & lRec, const Point2f & sample) const override {
        // fill properties of query record before return
        lRec.p = m_position;
        lRec.wi = (m_position - lRec.ref).normalized();
        // always on the point light manually set pdf to 1
        lRec.pdf = 1.0f;
        lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, (lRec.p - lRec.ref).norm() - Epsilon);

        float distanceSquared = (m_position - lRec.ref).squaredNorm();

        Vector3f wi = -(m_worldToLight * lRec.wi).normalized();
        float cosTheta = m_baseDir.dot(wi);

        // out of total width
        if (cosTheta < m_cosThetaMax) {
            return Color3f(0.0f);
        }
        float ratio = 1.0f;
        if (cosTheta < m_cosThetaFall) {
            float theta = std::acos(cosTheta);
            ratio = (m_thetaMax - theta) / (m_thetaMax - m_thetaFall);
        }
        // radiance Li
        return ratio * m_intensity * m_baseColor / distanceSquared;
    }

    virtual float pdf(const EmitterQueryRecord &lRec) const override {
        // not directly sample from point light
        // delta function set to 0
        return 0.0f;
    }

    virtual bool isDelta() const override {
        return true;
    }

protected:
    Point3f m_position;
    Color3f m_intensity;
    Transform m_lightToWorld;
    Transform m_worldToLight;
    float m_cosThetaMax;
    float m_cosThetaFall;
    // in degrees
    float m_thetaMax;
    float m_thetaFall;
    Vector3f m_baseDir;
    Color3f m_baseColor;

};

NORI_REGISTER_CLASS(SpotLight, "spot");
NORI_NAMESPACE_END
