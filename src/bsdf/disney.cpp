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
#include <nori/frame.h>
#include <nori/warp.h>

NORI_NAMESPACE_BEGIN

class Disney : public BSDF {
public:
    Disney(const PropertyList &propList) {
        // for constant baseColor
        if(propList.has("baseColor")) {
            PropertyList l;
            l.setColor("value", propList.getColor("baseColor"));
            m_albedo = static_cast<Texture<Color3f> *>(NoriObjectFactory::createInstance("constant_color", l));
        }

        m_metallic = propList.getFloat("metallic", 0.f);
        m_subsurface = propList.getFloat("subsurface", 0.f);
        m_specular = propList.getFloat("specular", 0.5f);
        m_specularTint = propList.getFloat("specularTint", 0.f);
        m_roughness = propList.getFloat("roughness", 0.5f);
        m_anisotropic = propList.getFloat("anisotropic", 0.f);
        m_sheen = propList.getFloat("sheen", 0.f);
        m_sheenTint = propList.getFloat("sheenTint", 0.f);
        m_clearcoat = propList.getFloat("clearcoat", 0.f);
        m_clearcoatGloss = propList.getFloat("clearcoatGloss", 0.f);
        // map specualr to fresnel then to eta
        m_eta = 2.f / (1.f - sqrt(0.08f * m_specular)) - 1.f;

        m_aspect = sqrtf(1 - 0.9 * m_anisotropic);
        m_alpha_min = 0.001f;
        m_alpha_g = (1.0f - m_clearcoatGloss) * 0.1 + m_clearcoatGloss * 0.001;

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
                } else if (obj->getIdName() == "metallicmap") {
                    if (m_metallic_map) {
                        throw NoriException("There is already a metallicmap defined!");
                    }
                    m_metallic_map = static_cast<Texture<float> *>(obj);
                } else if (obj->getIdName() == "roughnessmap") {
                    if (m_roughness_map) {
                        throw NoriException("There is already a roughnessmap defined!");
                    }
                    m_roughness_map = static_cast<Texture<float> *>(obj);
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

    virtual void activate() override {
        // default albedo
        if(!m_albedo) {
            PropertyList l;
            l.setColor("value", Color3f(0.5f));
            m_albedo = static_cast<Texture<Color3f> *>(NoriObjectFactory::createInstance("constant_color", l));
            m_albedo->activate();
        }
    }

    virtual ~Disney() {
        if (m_albedo != nullptr)
            delete m_albedo;
        if (m_normalmap != nullptr)
            delete m_normalmap;
        if (m_roughness_map != nullptr)
            delete m_roughness_map;
        if (m_metallic_map != nullptr)
            delete m_metallic_map;
    }

    float lambda_m(const BSDFQueryRecord &bRec, Vector3f w) const {
        float xSquare = pow(w.x() * get_alpha_x(bRec), 2);
        float ySquare = pow(w.y() * get_alpha_y(bRec), 2);
        return (sqrtf(1 + (xSquare + ySquare) / (w.z() * w.z())) - 1) / 2;
    }

    // smith GGX anisotropic for metal
    float g_m(const BSDFQueryRecord &bRec, Vector3f w) const {
        return 1.0f / (1 + lambda_m(bRec, w));
    }

    // Metallic geometry term
    float G_m(const BSDFQueryRecord &bRec) const {
        return g_m(bRec, bRec.wi) * g_m(bRec, bRec.wo);
    }

    float lambda_c(Vector3f w) const {
        float xSquare = w.x() * 0.25 * w.x() * 0.25;
        float ySquare = w.y() * 0.25 * w.y() * 0.25;
        return (sqrtf(1 + ((xSquare + ySquare) / (w.z() * w.z()))) - 1) / 2;
    }

    // smith GGX for clearcoat
    float g_c(Vector3f w) const {
        return 1.0f / (1 + lambda_c(w));
    }

    // Clearcoat geometry term
    float G_c(Vector3f wi, Vector3f wo) const {
        return g_c(wi) * g_c(wo);
    }

    /*
     * Diffuse lobe: cosine weighted hemisphere
     * parameter:
     * roughness
     * subsurface
     * fake sheen lobe parameters:
         * sheen
         * sheenTint
     */
    void sampleDiffuse(BSDFQueryRecord &bRec, const Point2f &sample) const {
        // sample the wo directly
        Vector3f wo = Warp::squareToCosineHemisphere(sample);
        bRec.wo = wo;
    }

    float pdfDiffuse(const BSDFQueryRecord &bRec) const {
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return 0.0f;

        return Warp::squareToCosineHemispherePdf(bRec.wo);
    }

    /*
     * Metallic(Specular) lobes: GTR2
     * parameter:
     * metallic
     * specular
     * specularTint
     * anisotropic
     */

    void sampleMetal(BSDFQueryRecord &bRec, const Point2f &sample) const {
        // sampling the half vector wh
        Vector3f wh = Warp::squareToGTR2(sample, get_alpha_x(bRec), get_alpha_y(bRec));
        bRec.wo = 2 * (bRec.wi.dot(wh)) * wh - bRec.wi;
    }

    float pdfSpecular(const BSDFQueryRecord &bRec) const {
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return 0.0f;

        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        // transform pdf_h to pdf_wo
        return Warp::squareToGTR2Pdf(wh, get_alpha_x(bRec), get_alpha_y(bRec)) / (4 * abs(wh.dot(bRec.wo)));
    }

    /*
     * Clearcoat lobes: GTR1
     * clearcoat
     * clearcoatGloss
     */

    void sampleClearcoat(BSDFQueryRecord &bRec, const Point2f &sample) const {
        // sampling the half vector wh
        Vector3f wh = Warp::squareToGTR1(sample, m_alpha_g);
        bRec.wo = 2 * (bRec.wi.dot(wh)) * wh - bRec.wi;
    }

    float pdfClearcoat(const BSDFQueryRecord &bRec) const {
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return 0.0;

        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        // transform pdf_h to pdf_wo
        return Warp::squareToGTR1Pdf(wh, m_alpha_g) / (4 * abs(wh.dot(bRec.wo)));
    }

    float SchlickFresnel(float cosTheta) const {
        float m = clamp(1 - cosTheta, 0.f, 1.f);
        return pow(m, 5);
    }

    // disney studio version
    virtual Color3f eval(const BSDFQueryRecord &bRec) const override {
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        float cosTheta_i = Frame::cosTheta(bRec.wi);
        float cosTheta_o = Frame::cosTheta(bRec.wo);
        Normal3f wh = (bRec.wi + bRec.wo).normalized();
        Color3f baseColor = m_albedo->eval(bRec.uv);
        Color3f Ctint(1.f);
        if (baseColor.getLuminance() > 0) {
            Ctint = baseColor / baseColor.getLuminance();
        }
        Color3f Cspec0 = lerp(getMetallic(bRec), m_specular * 0.08 * lerp(m_specularTint, Color3f(1.f), Ctint), baseColor);
        Color3f Csheen = lerp(m_sheenTint, Color3f(1), Ctint);

        // diffuse component
        float wh_dot_wo = wh.dot(bRec.wo);
        float FO = SchlickFresnel(cosTheta_o); // FL
        float FI = SchlickFresnel(cosTheta_i); // FV
        float Fd90 = 0.5f + 2 * getRoughness(bRec) * wh_dot_wo * wh_dot_wo;
        float Fd = lerp(FO, 1.f, Fd90) * lerp(FI, 1.f, Fd90);

        // subsurface component
        float Fss90 = getRoughness(bRec) * wh_dot_wo * wh_dot_wo;
        float Fss = lerp(FO, 1.f, Fss90) * lerp(FI, 1.f, Fss90);
        float ss = 1.25 * (Fss * (1.f / (cosTheta_i + cosTheta_o) - 0.5) + 0.5);

        // specular component
        // include cosTheta_h already, need to cancel out
        float Ds = Warp::squareToGTR2Pdf(wh, get_alpha_x(bRec), get_alpha_y(bRec)) / Frame::cosTheta(wh);
        float FH = SchlickFresnel(wh_dot_wo);
        Color3f Fs = lerp(FH, Cspec0, Color3f(1.f));

        float Gs = G_m(bRec);
        Color3f Fsheen = FH * m_sheen * Csheen;

        // clearcoat component
        // include cosTheta_h already, need to cancel out
        float Dr = Warp::squareToGTR1Pdf(wh, m_alpha_g) / Frame::cosTheta(wh);
        float Fr = lerp(FH, 0.04, 1.0);
        float Gr = G_c(bRec.wi, bRec.wo);

        float diffuseWeight = 1 - getMetallic(bRec);
        float metallicWeight = 1;
        float clearcoatWeight = 0.25 * m_clearcoat;
        // normalize like microfacet
        return diffuseWeight * (INV_PI * lerp(m_subsurface, Fd, ss) * baseColor + Fsheen)
               + metallicWeight * (Gs * Fs * Ds) / (4.f * cosTheta_o * cosTheta_i)
               + clearcoatWeight * (Gr * Fr * Dr) / (4.f * cosTheta_o * cosTheta_i);
    }

    virtual float pdf(const BSDFQueryRecord &bRec) const override {
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
        return 0.0f;

        float diffuseWeight = 1 - getMetallic(bRec);
        float metallicWeight = 1;
        float clearcoatWeight = 0.25 * m_clearcoat;
        float total = diffuseWeight + metallicWeight + clearcoatWeight;
        Vector3f weight(diffuseWeight / total, metallicWeight / total, clearcoatWeight / total);
        // blending pdf should in [0, 1]
        return weight[0] * pdfDiffuse(bRec) + weight[1] * pdfSpecular(bRec) + weight[2] * pdfClearcoat(bRec);
    }

    virtual Color3f sample(BSDFQueryRecord &bRec, const Point2f &_sample) const override {
        bRec.measure = ESolidAngle;

        float diffuseWeight = 1 - getMetallic(bRec);
        float metallicWeight = 1;
        float clearcoatWeight = 0.25 * m_clearcoat;
        float total = diffuseWeight + metallicWeight + clearcoatWeight;

        // prepare the pdf for importance sampling lobes
        Vector3f lobePdf(diffuseWeight / total, metallicWeight / total, clearcoatWeight / total);
        // need to fill in the bRec.wo in each sample component
        // need to sample reuse(re-normalize)

        float diffusePdf = lobePdf[0];
        float metallicPdf = lobePdf[1];
        float clearcoatPdf = lobePdf[2];

        Point2f sample(_sample);
        // importance sampling according to the pdf
        if (_sample.x() < diffusePdf) {
            // diffuse part
            sample.x() /= diffusePdf;
            sampleDiffuse(bRec, sample);
        } else if (_sample.x() < diffusePdf + metallicPdf) {
            // metallic part
            sample.x() = (_sample.x() - diffusePdf) / metallicPdf;
            sampleMetal(bRec, sample);
        } else {
            // clearcoat part
            sample.x() = (_sample.x() - diffusePdf - metallicPdf) / clearcoatPdf;
            sampleClearcoat(bRec, sample);
        }

        float pdfValue = pdf(bRec);
        if (pdfValue < SEpsilon) {
            return Color3f(0.0f);
        }
        return eval(bRec) * Frame::cosTheta(bRec.wo) / pdfValue;
    }

    float getMetallic(const BSDFQueryRecord &bRec) const {
        if (m_metallic_map == nullptr) {
            return m_metallic;
        }
        return m_metallic_map->eval(bRec.uv);
    }

    float getRoughness(const BSDFQueryRecord &bRec) const {
        if (m_roughness_map == nullptr) {
            return m_roughness;
        }
        return m_roughness_map->eval(bRec.uv);
    }

    float get_alpha_x(const BSDFQueryRecord &bRec) const {
        return std::max(m_alpha_min, getRoughness(bRec) * getRoughness(bRec) / m_aspect);
    }

    float get_alpha_y(const BSDFQueryRecord &bRec) const {
        return std::max(m_alpha_min, getRoughness(bRec) * getRoughness(bRec) * m_aspect);
    }

    Color3f getAlbedo(const BSDFQueryRecord &bRec) const {
        return m_albedo->eval(bRec.uv);
    }

    virtual std::string toString() const override {
        return tfm::format(
            "Disney[\n"
            "  baseColor = %s\n"
            "  normalmap = %s\n"
            "  metallic = %f\n"
            "  metallic_map = %s\n"
            "  subsurface = %f\n"
            "  specular = %f\n"
            "  specularTint = %f\n"
            "  roughness = %f\n"
            "  roughness_map = %s\n"
            "  anisotropic = %f\n"
            "  sheen = %f\n"
            "  sheenTint = %f\n"
            "  clearcoat = %f\n"
            "  clearcoatGloss = %f\n"
            "  eta = %f\n"
            "]",
            m_albedo ? indent(m_albedo->toString()) : std::string("null"),
            m_normalmap ? indent(m_normalmap->toString()) : std::string("null"),
            m_metallic,
            m_metallic_map ? indent(m_metallic_map->toString()) : std::string("null"),
            m_subsurface,
            m_specular,
            m_specularTint,
            m_roughness,
            m_roughness_map ? indent(m_roughness_map->toString()) : std::string("null"),
            m_anisotropic,
            m_sheen,
            m_sheenTint,
            m_clearcoat,
            m_clearcoatGloss,
            m_eta
        );
    }
private:
    // 1. constant baseColor
    // 2. image texture
    Texture<Color3f> * m_albedo = nullptr;
    Texture<Normal3f> * m_normalmap = nullptr;
    // disney bsdf parameters
    float m_metallic;
    Texture<float> *m_metallic_map = nullptr;
    float m_subsurface;
    float m_specular;
    float m_specularTint;
    float m_roughness;
    Texture<float> *m_roughness_map = nullptr;
    float m_anisotropic;
    float m_sheen;
    float m_sheenTint;
    float m_clearcoat;
    float m_clearcoatGloss;

    float m_aspect;
    float m_alpha_min;
    float m_alpha_g;
    float m_eta;

};

NORI_REGISTER_CLASS(Disney, "disney");
NORI_NAMESPACE_END
