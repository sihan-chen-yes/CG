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

#include <nori/bsdf.h>

NORI_NAMESPACE_BEGIN
/**
 * \brief Null BRDF model for participating media
 */
class Null : public BSDF {
public:
    Null(const PropertyList &propList) {
        // no properties
    }

    virtual Color3f eval(const BSDFQueryRecord &bRec) const override {
        /* Discrete BRDFs always evaluate to zero in Nori */
        return Color3f(0.0f);
    }

    virtual float pdf(const BSDFQueryRecord &bRec) const override {
        /* Discrete BRDFs always evaluate to zero in Nori */
        return 0.0f;
    }

    virtual Color3f sample(BSDFQueryRecord &bRec, const Point2f &sample) const override {
        bRec.wo = -bRec.wi;
        bRec.measure = EDiscrete;

        return Color3f(1.f);
    }
    
    virtual std::string toString() const override {
        return tfm::format(
                "Null[]");
    }
};

NORI_REGISTER_CLASS(Null, "null");
NORI_NAMESPACE_END
