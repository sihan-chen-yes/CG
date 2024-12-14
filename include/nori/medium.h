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

#if !defined(__NORI_MEDIUM_H)
#define __NORI_MEDIUM_H

#include <nori/object.h>
#include <nori/interaction.h>

NORI_NAMESPACE_BEGIN

/**
 * \brief Convenience data structure used to pass multiple
 * parameters to the evaluation and sampling routines in \ref PhaseFunction
 */
struct PhaseFunctionQueryRecord {
    /// Incident direction (in the local frame)
    Vector3f wi;

    /// Outgoing direction (in the local frame)
    Vector3f wo;

    /// Measure associated with the sample
    EMeasure measure;

    /// Create a new record for sampling the PhaseFunction
    PhaseFunctionQueryRecord(const Vector3f &wi)
            : wi(wi), measure(EUnknownMeasure) { }

    /// Create a new record for querying the PhaseFunction
    PhaseFunctionQueryRecord(const Vector3f &wi,
                    const Vector3f &wo, EMeasure measure)
            : wi(wi), wo(wo), measure(measure) { }

};

/**
 * \brief Superclass of all bidirectional phase functions
 */
class PhaseFunction : public NoriObject {
public:
    virtual float sample(PhaseFunctionQueryRecord &pRec, const Point2f &sample) const = 0;

    virtual float pdf(const PhaseFunctionQueryRecord &pRec) const = 0;

    /**
     * \brief Return the type of object (i.e. Mesh/BSDF/PhaseFunction/etc.)
     * provided by this instance
     * */
    virtual EClassType getClassType() const override { return EPhaseFunction; }
};

inline float PhaseHG(float cosTheta, float g) {
    // cosTheta means wo.dot(wi) here
    float denom = 1 + g * g + 2 * g * cosTheta;
    return INV_FOURPI * (1 - g * g) / (denom * std::sqrt(denom));
}

class HenyeyGreenstein : public PhaseFunction {
public:
    // HenyeyGreenstein Public Methods
    HenyeyGreenstein(const PropertyList &propList);

    virtual float sample(PhaseFunctionQueryRecord &pRec, const Point2f &sample) const override;

    virtual float pdf(const PhaseFunctionQueryRecord &pRec) const override;

    virtual std::string toString() const override;

private:
    float g;
};

class Medium : public NoriObject {
public:
    // Medium Interface
    virtual bool sampleEmitter() const = 0;
    virtual Color3f Tr(const Ray3f &ray) const = 0;
    virtual Color3f Le(const Ray3f &ray) const = 0;
    virtual Color3f sample(const Ray3f &ray, Sampler *sampler,
                           Interaction &mi) const = 0;

    /**
     * \brief Return the type of object (i.e. Mesh/BSDF/PhaseFunction/Medium/etc.)
     * provided by this instance
     * */
    virtual EClassType getClassType() const override { return EMedium; }
};

NORI_NAMESPACE_END

#endif /* __NORI_MEDIUM_H */
