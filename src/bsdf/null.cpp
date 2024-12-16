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
        if(propList.has("baseColor")) {
            PropertyList l;
            l.setColor("value", propList.getColor("baseColor"));
            m_albedo = static_cast<Texture<Color3f> *>(NoriObjectFactory::createInstance("constant_color", l));
        }

        m_emissionStrength = propList.getFloat("strength", 1.0f);
    }

    virtual void addChild(NoriObject *obj) override {
        switch (obj->getClassType()) {
            case ETexture:
                if(obj->getIdName() == "baseColor") {
                    if (m_albedo)
                        throw NoriException("There is already an baseColor defined!");
                    m_albedo = static_cast<Texture<Color3f> *>(obj);
                } else if (obj->getIdName() == "normalmap") {
                    if (m_normalmap) {
                        throw NoriException("There is already a normalmap defined!");
                    }
                    m_normalmap = static_cast<Texture<Normal3f> *>(obj);
                } else if (obj->getIdName() == "alphamap") {
                    if (m_alpha_map) {
                        throw NoriException("There is already a alphamap defined!");
                    }
                    m_alpha_map = static_cast<Texture<float> *>(obj);
                } else if (obj->getIdName() == "emissionmap") {
                    if (m_emission_map) {
                        throw NoriException("There is already a emissionmap defined!");
                    }
                    m_emission_map = static_cast<Texture<float> *>(obj);
                }
                else {
                    throw NoriException("The name of this texture does not match any field!");
                }
                break;

            default:
                throw NoriException("Disney::addChild(<%s>) is not supported!",
                                    classTypeName(obj->getClassType()));
        }
    }

    virtual ~Null() {
        if (m_albedo != nullptr)
            delete m_albedo;
        if (m_normalmap != nullptr)
            delete m_normalmap;
        if (m_alpha_map != nullptr)
            delete m_alpha_map;
        if (m_emission_map != nullptr)
            delete m_emission_map;
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

        float alpha = getAlpha(bRec);
        float emission = getEmission(bRec);

        // handled as transparent bsdf
        if (m_albedo == nullptr) {
            return Color3f(1.0f);
        }

        return alpha * emission * m_emissionStrength * m_albedo->eval(bRec.uv) + (1 - alpha) * Color3f(1.0f);
    }

    // for denoising
    Color3f getAlbedo(const BSDFQueryRecord &bRec) const {
        // handled as translucent materials
        if (m_albedo == nullptr) {
            return Color3f(1.0f);
        }
        assert(m_albeo != nullptr && m_alpha_map != nullptr);
        float alpha = getAlpha(bRec);
        return alpha * m_albedo->eval(bRec.uv) + (1 - alpha) * Color3f(0.0f);
    }

    float getAlpha(const BSDFQueryRecord &bRec) const {
        if (m_alpha_map == nullptr) {
            return 1.0f;
        }
        return m_alpha_map->eval(bRec.uv);
    }

    bool isNull() const {
        return true;
    }

    float getEmission(const BSDFQueryRecord &bRec) const {
        if (m_emission_map == nullptr) {
            return 1.0f;
        }
        return m_emission_map->eval(bRec.uv);
    }

    virtual std::string toString() const override {
        return tfm::format(
                "Null[\n"
                "  baseColor = %s\n"
                "  normalmap = %s\n"
                "  alpha_map = %s\n"
                "  emission_map = %s\n"
                "  emissionStrength = %f\n"
                "]",
                m_albedo ? indent(m_albedo->toString()) : std::string("null"),
                m_normalmap ? indent(m_normalmap->toString()) : std::string("null"),
                m_alpha_map ? indent(m_alpha_map->toString()) : std::string("null"),
                m_emission_map ? indent(m_emission_map->toString()) : std::string("null"),
                m_emissionStrength
            );
    }

protected:
    Texture<Color3f> * m_albedo = nullptr;
    Texture<Normal3f> * m_normalmap = nullptr;
    Texture<float> *m_alpha_map = nullptr;
    Texture<float> *m_emission_map = nullptr;
    float m_emissionStrength;
};

NORI_REGISTER_CLASS(Null, "null");
NORI_NAMESPACE_END
