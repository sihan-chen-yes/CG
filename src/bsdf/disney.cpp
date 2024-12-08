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
#include <nori/warp.h>

NORI_NAMESPACE_BEGIN

/// Ideal dielectric BSDF
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
        m_alpha_min = 0.0001f;
        m_alpha_x = std::max(m_alpha_min, m_roughness * m_roughness / m_aspect);
        m_alpha_y = std::max(m_alpha_min, m_roughness * m_roughness * m_aspect);
        m_alpha_g = (1.0f - m_clearcoatGloss) * 0.1 + m_clearcoatGloss * 0.001;


        diffuseWeight = 1 - m_metallic;
        metalWeight = m_metallic;
        clearcoatWeight = 0.25 * m_clearcoat;
//        sheenWeight = (1 - m_metallic) * m_sheen;

//        diffuseWeight = 1 - m_metallic;
//        metalWeight = 1;
//        clearcoatWeight = 0.25 * m_clearcoat;
//        sheenWeight = (1 - m_metallic) * m_sheen;

        // not used
//        glassWeight = (1 - m_metallic) * m_specularTransmission;
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
                    if (m_roughtness_map) {
                        throw NoriException("There is already a roughnessmap defined!");
                    }
                    m_roughtness_map = static_cast<Texture<float> *>(obj);
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


    /*
     * Disney bsdf components evaluations
     */

    /*
     * Basediffuse component
     */

    float F_D90(Vector3f wh, Vector3f wo) const {
        // half vector dot wo
        float cosTheta = abs(wh.dot(wo));
        return 0.5f + 2 * m_roughness * cosTheta * cosTheta;
    }

    float F_D(Vector3f w, Vector3f wh, Vector3f wo) const {
        float cosTheta = abs(Frame::cosTheta(w));
        return 1.0f + (F_D90(wh, wo) - 1.0f) * pow(1 - cosTheta, 5);
    }

    Color3f evalBaseDiffuse(const BSDFQueryRecord &bRec) const {
        //return zero if the measure is wrong, or when queried for illumination on the backside
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        Vector3f wo = bRec.wo;
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        return m_albedo->eval(bRec.uv) * INV_PI * F_D(bRec.wi, wh, wo) * F_D(bRec.wo, wh, wo);
    }

    /*
     * Subsurface component
     */

    float F_SS90(Vector3f wh, Vector3f wo) const {
        // half vector dot wo
        float cosTheta = abs(wh.dot(wo));
        return m_roughness * cosTheta * cosTheta;
    }

    float F_SS(Vector3f w, Vector3f wh, Vector3f wo) const {
        float cosTheta = abs(Frame::cosTheta(w));
        return 1.0f + (F_SS90(wh, wo) - 1.0f) * pow(1.0f - cosTheta, 5);
    }

    Color3f evalSubsurface(const BSDFQueryRecord &bRec) const {
        //return zero if the measure is wrong, or when queried for illumination on the backside
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        Vector3f wo = bRec.wo;
        Vector3f wi = bRec.wi;
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        float cosTheta_i = abs(Frame::cosTheta(wi));
        float cosTheta_o = abs(Frame::cosTheta(wo));
        return 1.25 * m_albedo->eval(bRec.uv) * INV_PI *
        (F_SS(wi, wh, wo) * F_SS(wo, wh, wo) * (1.0f / (cosTheta_i + cosTheta_o) - 0.5) + 0.5);
    }

    /*
     * Diffuse component including baseDiffuse and subsurface components
     * params:
     * m_roughness
     * m_subsurface
     */

    Color3f evalDiffuse(const BSDFQueryRecord &bRec) const {
        //return zero if the measure is wrong, or when queried for illumination on the backside
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        return (1.0f - m_subsurface) * evalBaseDiffuse(bRec) + m_subsurface * evalSubsurface(bRec);
    }

    /*
     * Metal components:
     * params:
     *  m_anistropic
     *  m_roughness
     */

    Color3f C_0(const BSDFQueryRecord &bRec) const {
        Color3f baseColor = m_albedo->eval(bRec.uv);
        return m_specular * R_0(m_eta) * (1 - m_metallic) * K_s(bRec) + m_metallic * baseColor;
    }

    Color3f K_s(const BSDFQueryRecord &bRec) const {
        return (1 - m_specularTint) + m_specularTint * C_tint(bRec);
    }

    Color3f F_m(const BSDFQueryRecord &bRec) const {
        Color3f baseColor = m_albedo->eval(bRec.uv);
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        float cosTheta = wh.dot(bRec.wo);
        // modified F_m_tilde
        return baseColor + (1 - baseColor) * pow(1 - cosTheta, 5);
    }

    Color3f F_m_tilde(const BSDFQueryRecord &bRec) const {
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        float cosTheta = wh.dot(bRec.wo);
        Color3f c0 = C_0(bRec);
        // modified F_m_tilde
        return c0 + (1 - c0) * pow(1 - cosTheta, 5);
    }


    // Anisotropic GGX Microfacet Distribution Function
    float D_m(Vector3f wh) const {
        float h = (wh.x() / m_alpha_x) * (wh.x() / m_alpha_x) + (wh.y() / m_alpha_y) * (wh.y() / m_alpha_y) + wh.z() * wh.z();
        return 1.0f / (M_PI * m_alpha_x * m_alpha_y * h * h);
    }

    float lambda_m(Vector3f w) const {
        float xSquare = w.x() * m_alpha_x * w.x() * m_alpha_x;
        float ySquare = w.y() * m_alpha_y * w.y() * m_alpha_y;
        return (sqrtf(1 + (xSquare + ySquare) / (w.z() * w.z())) - 1) / 2;
    }

    float g_m(Vector3f w) const {
        return 1.0f / (1 + lambda_m(w));
    }

    // Metallic geometry term
    float G_m(Vector3f wi, Vector3f wo) const {
        return g_m(wi) * g_m(wo);
    }

    Color3f evalMetal(const BSDFQueryRecord &bRec) const {
        //return zero if the measure is wrong, or when queried for illumination on the backside
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        Vector3f wo = bRec.wo;
        Vector3f wi = bRec.wi;
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        float cosTheta_i = abs(Frame::cosTheta(wi));
        float cosTheta_o = abs(Frame::cosTheta(wo));
        return F_m_tilde(bRec) * D_m(wh) * G_m(wi, wo) / (4 * cosTheta_i * cosTheta_o);
    }

    /*
     * Clearcoat components:
     * params:
     * m_clearcoatGloss
     * m_clearcoat
     */

    float R_0(float eta) const {
        return (eta - 1.0f) * (eta - 1.0f) / ((eta + 1.0f) * (eta + 1.0f));
    }

    float F_c(Vector3f wh, Vector3f wo) const {
        float cosTheta = abs(wh.dot(wo));
        // default eta, specular = 0.5
        return R_0(1.5) + (1.0f - R_0(1.5)) * pow(1 - cosTheta, 5);
    }

    float D_c(Vector3f wh) const {
        float alphaSquare = m_alpha_g * m_alpha_g;
        return (alphaSquare - 1) / (M_PI * log(alphaSquare) * (1 + (alphaSquare - 1) * wh.z() * wh.z()));
    }

    float lambda_c(Vector3f w) const {
        float xSquare = w.x() * 0.25 * w.x() * 0.25;
        float ySquare = w.y() * 0.25 * w.y() * 0.25;
        return (sqrtf(1 + ((xSquare + ySquare) / (w.z() * w.z()))) - 1) / 2;
    }

    float g_c(Vector3f w) const {
        return 1.0f / (1 + lambda_c(w));
    }

    // Clearcoat geometry term
    float G_c(Vector3f wi, Vector3f wo) const {
        return g_c(wi) * g_c(wo);
    }

    Color3f evalClearcoat(const BSDFQueryRecord &bRec) const {
        //return zero if the measure is wrong, or when queried for illumination on the backside
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        Vector3f wo = bRec.wo;
        Vector3f wi = bRec.wi;
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        float cosTheta_i = abs(Frame::cosTheta(wi));
        float cosTheta_o = abs(Frame::cosTheta(wo));
        return F_c(wh, wo) * D_c(wh) * G_c(wi, wo) / (4 * cosTheta_i * cosTheta_o);
    }

    /* 2015 version, Burley added a dielectric lobe
     * Glass components:
     * params:
     * m_
     */

//    Color3f evalGlass(const BSDFQueryRecord &bRec) const {
//        //return zero if the measure is wrong, or when queried for illumination on the backside
//        if (bRec.measure != ESolidAngle)
//            return Color3f(0.0f);
//
//        Vector3f wo = bRec.wo;
//        Vector3f wi = bRec.wi;
//        Vector3f wh = (bRec.wi + bRec.wo).normalized();
//        Color3f baseColor = m_albedo->eval(bRec.uv);
//        // TODO
//        float F = fresnel(wh.dot(wi), m_extIOR, m_intIOR);
//        float cosTheta_i = abs(Frame::cosTheta(wi));
//        float eta = m_intIOR / m_extIOR;
//        if (Frame::cosTheta(wi) < 0) {
//            eta = m_extIOR / m_intIOR;
//        }
//        if (Frame::cosTheta(wi) * Frame::cosTheta(wo) > 0) {
//            // D G same as metal
//            return baseColor * F * D_m(wh) * G_m(wi, wo) / (4 * cosTheta_i);
//        }
//        // TODO
//        Color3f sqrtBaseColor = Color3f(sqrtf(baseColor.x()), sqrtf(baseColor.y()), sqrtf(baseColor.z()));
//        return sqrtBaseColor * (1 - F) * D_m(wh) * G_m(wi, wo) * abs(wh.dot(wi) * wh.dot(wo)) /
//               (cosTheta_i * (wh.dot(wi) + eta * wh.dot(wo)) * (wh.dot(wi) + eta * wh.dot(wo)));
//    }


    /* Sheen components:
     * params:
     * m_sheen
     */

    Color3f C_tint(const BSDFQueryRecord &bRec) const {
        Color3f baseColor = m_albedo->eval(bRec.uv);
        if (baseColor.getLuminance() > 0) {
            return baseColor / baseColor.getLuminance();
        }
        return Color3f(1.0f);
    }

    Color3f C_sheen(const BSDFQueryRecord &bRec) const {
        return (1 - m_sheenTint) + m_sheenTint * C_tint(bRec);
    }

    Color3f evalSheen(const BSDFQueryRecord &bRec) const {
        //return zero if the measure is wrong, or when queried for illumination on the backside
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        Vector3f wo = bRec.wo;
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        float cosTheta_o = abs(wh.dot(wo));
        return C_sheen(bRec) * pow(1 - cosTheta_o, 5);
    }

    /*
     * Disney bsdf lobes sampling and pdf
     */

    /*
     * Diffuse lobe: cosine weighted hemisphere
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
     */

    void sampleMetal(BSDFQueryRecord &bRec, const Point2f &sample) const {
        // sampling the half vector wh
        Vector3f wh = Warp::squareToGTR2(sample, m_alpha_x, m_alpha_y);
        bRec.wo = 2 * (bRec.wi.dot(wh)) * wh - bRec.wi;
    }

    float pdfMetal(const BSDFQueryRecord &bRec) const {
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return 0.0f;

        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        float cosTheta_o = abs(Frame::cosTheta(bRec.wo));
        float cosTheta_i = abs(Frame::cosTheta(bRec.wi));
        // transform pdf_h to pdf_wo
        return Warp::squareToGTR2Pdf(wh, m_alpha_x, m_alpha_y) / (4 * abs(wh.dot(bRec.wo)));
        return Warp::squareToGTR2Pdf(wh, m_alpha_x, m_alpha_y) * g_m(bRec.wi) / (4 * cosTheta_i) / 4 / abs(wh.dot(bRec.wo));
        return Warp::squareToGTR2Pdf(wh, m_alpha_x, m_alpha_y) * g_m(bRec.wi) / (4 * cosTheta_i);
        return Warp::squareToGTR2Pdf(wh, m_alpha_x, m_alpha_y) * g_m(bRec.wi) / (4 * abs(wh.dot(bRec.wo)));
    }

    /*
     * Clearcoat lobes: GTR1
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
//        return Warp::squareToGTR1Pdf(wh, m_alpha_g) * abs(Frame::cosTheta(wh)) / (4 * abs(wh.dot(bRec.wo))) / (4 * abs(wh.dot(bRec.wo)));
    }

    /*
     * Glass lobes:
     */


    virtual Color3f eval(const BSDFQueryRecord &bRec) const override {
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        return diffuseWeight * evalDiffuse(bRec)
               + metalWeight * evalMetal(bRec)
               + clearcoatWeight * evalClearcoat(bRec)
               + sheenWeight * evalSheen(bRec);

        return diffuseWeight * evalDiffuse(bRec)
               + metalWeight * evalMetal(bRec)
               + clearcoatWeight * evalClearcoat(bRec)
               + sheenWeight * evalSheen(bRec);
    }


    virtual float pdf(const BSDFQueryRecord &bRec) const override {
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
        return 0.0f;

        Vector3f weights(diffuseWeight, metalWeight, clearcoatWeight);
        weights.normalize();
        // blending pdf should in [0, 1]
        return weights[0] * pdfDiffuse(bRec) + weights[1] * pdfMetal(bRec) + weights[2] * pdfClearcoat(bRec);
    }

    virtual Color3f sample(BSDFQueryRecord &bRec, const Point2f &_sample) const override {
        bRec.measure = ESolidAngle;
        if (Frame::cosTheta(bRec.wi) < 0) {
            //relative IOR: outgoing / incident
            bRec.eta = -m_eta;
        } else {
            //relative IOR: outgoing / incident
            bRec.eta = m_eta;
        }

        Vector3f lobePdf(diffuseWeight, metalWeight, clearcoatWeight);
        lobePdf.normalize();
        // importance sampling
        float xi = _sample.x();
        Point2f sample = Point2f(_sample);
        // need to fill in the bRec.wo in each sample component
        // need to sample reuse(renomalize)
        if (xi < lobePdf[0]) {
            sample.x() /= lobePdf[0];
            sampleDiffuse(bRec, sample);
        } else if (xi < lobePdf[0] + lobePdf[1]) {
            sample.x() = (sample.x() - lobePdf[0]) / lobePdf[1];
            sampleMetal(bRec, sample);
        } else {
            sample.x() = (sample.x() - (lobePdf[0] + lobePdf[1])) / lobePdf[2];
            sampleClearcoat(bRec, sample);
        }

        float pdfValue = pdf(bRec);
        if (pdfValue < SEpsilon) {
            return Color3f(0.0f);
        }
        return eval(bRec) * Frame::cosTheta(bRec.wo) / pdfValue;
    }

    virtual std::string toString() const override {
        return tfm::format(
            "Disney[\n"
            "  baseColor = %s\n"
            "  normalmap = %s\n"
            "  metallic = %f\n"
            "  subsurface = %f\n"
            "  specular = %f\n"
            "  specularTint = %f\n"
            "  roughness = %f\n"
            "  anisotropic = %f\n"
            "  sheen = %f\n"
            "  sheenTint = %f\n"
            "  clearcoat = %f\n"
            "  clearcoatGloss = %f\n"
            "  eta = %f\n"
            "]",
            m_albedo ? indent(m_albedo->toString()) : std::string("null"),
            m_normalmap ? indent(m_albedo->toString()) : std::string("null"),
            m_metallic,
            m_subsurface,
            m_specular,
            m_specularTint,
            m_roughness,
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
    // TODO
    float m_metallic;
    Texture<float> *m_metallic_map;
    float m_subsurface;
    float m_specular;
    float m_specularTint;
    float m_roughness;
    Texture<float> *m_roughtness_map;
    float m_anisotropic;
    float m_sheen;
    float m_sheenTint;
    float m_clearcoat;
    float m_clearcoatGloss;

    float m_aspect;
    float m_alpha_min;
    float m_alpha_x;
    float m_alpha_y;
    float m_alpha_g;
    float m_eta;

    float diffuseWeight;
    float metalWeight;
    float clearcoatWeight;
    // not used
    float glassWeight;
    float sheenWeight;
};

NORI_REGISTER_CLASS(Disney, "disney");
NORI_NAMESPACE_END
