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

#include <nori/warp.h>
#include <nori/medium.h>

NORI_NAMESPACE_BEGIN

/**
 * \brief implementation of HenyeyGreenstein Phase Function
 */

// HenyeyGreenstein Constructor
HenyeyGreenstein::HenyeyGreenstein(const PropertyList &propList) {
    g = propList.getFloat("g", 0.0f);
}

float HenyeyGreenstein::sample(PhaseFunctionQueryRecord &pRec, const Point2f &sample) const {
    // Compute $\cos \theta$ for Henyey--Greenstein sample
    float cosTheta;
    if (std::abs(g) < 1e-3)
        cosTheta = 1 - 2 * sample.x();
    else {
        float sqrTerm = (1 - g * g) / (1 + g - 2 * g * sample.x());
        cosTheta = -(1 + g * g - sqrTerm * sqrTerm) / (2 * g);
    }

    // Compute direction _wo_ for Henyey--Greenstein sample
    float sinTheta = std::sqrt(std::max((float)0, 1 - cosTheta * cosTheta));
    float phi = 2 * M_PI * sample.y();
    Vector3f v1, v2;
    coordinateSystem(pRec.wi, v1, v2);
    pRec.wo = sphericalDirection(sinTheta, cosTheta, phi, v1, v2, pRec.wi);
    pRec.measure = ESolidAngle;
    // don't need to consider cosine term for phase function
//    return PhaseHG(cosTheta, g) / pdf(pRec);
    // cancel out, directly return 1
    return 1.f;
}

float HenyeyGreenstein::pdf(const PhaseFunctionQueryRecord &pRec) const {
    return PhaseHG(pRec.wi.dot(pRec.wo), g);
}

std::string HenyeyGreenstein::toString() const {
    return tfm::format(
            "HenyeyGreenstein[\n"
            "  g = %f\n"
            "]",
            g
    );
}

NORI_REGISTER_CLASS(HenyeyGreenstein, "hg");
NORI_NAMESPACE_END
