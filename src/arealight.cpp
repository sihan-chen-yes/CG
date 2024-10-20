/*
    This file is part of Nori, a simple educational ray tracer

    Copyright (c) 2015 by Romain Prévost

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
#include <nori/warp.h>
#include <nori/shape.h>

NORI_NAMESPACE_BEGIN

class AreaEmitter : public Emitter {
public:
    AreaEmitter(const PropertyList &props) {
        m_radiance = props.getColor("radiance");
    }

    virtual std::string toString() const override {
        return tfm::format(
                "AreaLight[\n"
                "  radiance = %s,\n"
                "]",
                m_radiance.toString());
    }

    virtual Color3f eval(const EmitterQueryRecord & lRec) const override {
        if(!m_shape)
            throw NoriException("There is no shape attached to this Area light!");
        // reject intersection at back
        if (lRec.n.dot(-lRec.wi) <= 0) {
            return Color3f(0.0f);
        }
        return m_radiance;
    }

    virtual Color3f sample(EmitterQueryRecord & lRec, const Point2f & sample) const override {
        if(!m_shape)
            throw NoriException("There is no shape attached to this Area light!");
        ShapeQueryRecord sRec(lRec.ref);
        // set p n pdf inside
        m_shape->sampleSurface(sRec, sample);
        // fill in properties
        lRec.p = sRec.p;
        lRec.n = sRec.n;
        // remember to normalize
        lRec.wi = (lRec.p - lRec.ref).normalized();
        lRec.pdf = pdf(lRec);
        lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, (lRec.p - lRec.ref).norm() - Epsilon);
        if (lRec.pdf < Epsilon) {
            return Color3f(0.0f);
        }
        return eval(lRec) / lRec.pdf;
    }

    virtual float pdf(const EmitterQueryRecord &lRec) const override {
        if(!m_shape)
            throw NoriException("There is no shape attached to this Area light!");
        if (lRec.n.dot(-lRec.wi) <= 0) {
            return 0.0f;
        }
        ShapeQueryRecord sRec(lRec.ref, lRec.p);
        float p_area = m_shape->pdfSurface(sRec);
        // careful with cos_theta = 0
        float cos_theta = lRec.n.dot(-lRec.wi);
        float d_square = (lRec.p - lRec.ref).squaredNorm();
        // solid angle
        return d_square * p_area / cos_theta;
    }


    virtual Color3f samplePhoton(Ray3f &ray, const Point2f &sample1, const Point2f &sample2) const override {
        throw NoriException("To implement...");
    }


protected:
    Color3f m_radiance;
};

NORI_REGISTER_CLASS(AreaEmitter, "area")
NORI_NAMESPACE_END