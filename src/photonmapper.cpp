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

#include <nori/integrator.h>
#include <nori/sampler.h>
#include <nori/emitter.h>
#include <nori/bsdf.h>
#include <nori/scene.h>
#include <nori/photon.h>

NORI_NAMESPACE_BEGIN

class PhotonMapper : public Integrator {
public:
    /// Photon map data structure
    typedef PointKDTree<Photon> PhotonMap;

    PhotonMapper(const PropertyList &props) {
        /* Lookup parameters */
        m_photonCount  = props.getInteger("photonCount", 1000000);
        m_photonRadius = props.getFloat("photonRadius", 0.0f /* Default: automatic */);
    }

    virtual void preprocess(const Scene *scene) override {
        cout << "Gathering " << m_photonCount << " photons .. ";
        cout.flush();

        /* Create a sample generator for the preprocess step */
        Sampler *sampler = static_cast<Sampler *>(
            NoriObjectFactory::createInstance("independent", PropertyList()));

        /* Allocate memory for the photon map */
        m_photonMap = std::unique_ptr<PhotonMap>(new PhotonMap());
        m_photonMap->reserve(m_photonCount);

		/* Estimate a default photon radius */
		if (m_photonRadius == 0)
			m_photonRadius = scene->getBoundingBox().getExtents().norm() / 500.0f;

        int emitterCount = scene->getLights().size();
        m_emittedPhotonCount = 0;

        while (m_photonMap->size() < size_t(m_photonCount)) {
            Ray3f ray;
            Color3f phi_p = scene->getRandomEmitter(sampler->next1D())->samplePhoton(ray, sampler->next2D(), sampler->next2D());
            // changed d, need update dRcp!!
            ray.update();
            // emitted a new photon and start tracing
            m_emittedPhotonCount++;

            // consider the pdf of random sampling emitter
            phi_p *= emitterCount;

            int bounces = 0;
            // trace Photon
            while (m_photonMap->size() < size_t(m_photonCount)) {
                Intersection its;
                if (!scene->rayIntersect(ray, its)) {
                    break;
                }

                if (its.mesh->getBSDF()->isDiffuse()) {
                    // store photon if is diffuse
                    m_photonMap->push_back(Photon(its.p, -ray.d, phi_p));
                }

                // start Russian Roulette after 3 bounces
                if (bounces > 3) {
                    float success = std::min(std::max(phi_p.x(), std::max(phi_p.y(), phi_p.z())), 0.99f);

                    if (sampler->next1D() < success) {
                        phi_p /= success;
                    } else {
                        break;
                    }
                }

                // shoot shadow ray to next if not diffuse bsdf
                BSDFQueryRecord bRec(its.shFrame.toLocal(-ray.d), its.uv);
                Color3f bsdfValue = its.mesh->getBSDF()->sample(bRec, sampler->next2D());
                // construct directly, don't need to update dRcp
                ray = Ray3f(its.p, its.shFrame.toWorld(bRec.wo));
                ray.mint = Epsilon;

                // update throughout
                phi_p *= bsdfValue;
                bounces++;
            }
        }

		/* Build the photon map */
        m_photonMap->build();
    }

    virtual Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &_ray) const override {
        //outgoing radiance
        Color3f Lo(0.0f);
        Color3f t(1.0f);
        Intersection its;
        Ray3f shadowRay(_ray);
        int bounces = 0;
        while (true) {
            if (!scene->rayIntersect(shadowRay, its)) {
                break;
            }

            if (its.mesh->isEmitter()) {
                // eRec ref
                EmitterQueryRecord eRec(shadowRay.o);
                // convention not the same for eRec and bRec!
                eRec.wi = shadowRay.d;
                eRec.shadowRay = shadowRay;
                eRec.shadowRay.mint = Epsilon;

                // fill in properties of eRec
                // light source
                eRec.p = its.p;
                eRec.n = its.shFrame.n;
                // incident radiance
                Color3f Li = its.mesh->getEmitter()->eval(eRec);
                Lo += t * Li;
            }

            if (its.mesh->getBSDF()->isDiffuse()) {
                std::vector<uint32_t> results;
                m_photonMap->search(its.p, m_photonRadius, results);
                Color3f photon_radiance(0.0f);
                for (uint32_t i : results) {
                    const Photon &photon = (*m_photonMap)[i];
                    BSDFQueryRecord bRec(its.shFrame.toLocal(-shadowRay.d), its.shFrame.toLocal(photon.getDirection()),
                                         EMeasure::ESolidAngle, its.uv);
                    Color3f bsdfValue = its.mesh->getBSDF()->eval(bRec);
                    photon_radiance += bsdfValue * (photon.getPower() / m_emittedPhotonCount) / (M_PI * m_photonRadius * m_photonRadius);
                }
                Lo += t * photon_radiance;
                break;
            }

            // start Russian Roulette after 3 bounces
            if (bounces > 3) {
                float success = std::min(std::max(t.x(), std::max(t.y(), t.z())), 0.99f);

                if (sampler->next1D() < success) {
                    t /= success;
                } else {
                    break;
                }
            }

            //path-reuse
            BSDFQueryRecord bRec(its.shFrame.toLocal(-shadowRay.d), its.uv);
            // included cosine term already
            Color3f bsdfValue = its.mesh->getBSDF()->sample(bRec, sampler->next2D());

            // update throughout for NEE
            t *= bsdfValue;
            shadowRay = Ray3f(its.p, its.shFrame.toWorld(bRec.wo));
            bounces++;
        }

        // outgoing radiance
        return Lo;
    }

    virtual std::string toString() const override {
        return tfm::format(
            "PhotonMapper[\n"
            "  photonCount = %i,\n"
            "  photonRadius = %f\n"
            "]",
            m_photonCount,
            m_photonRadius
        );
    }
private:
    /* 
     * Important: m_photonCount is the total number of photons deposited in the photon map,
     * NOT the number of emitted photons. You will need to keep track of those yourself.
     */ 
    int m_photonCount;
    int m_emittedPhotonCount;
    float m_photonRadius;
    std::unique_ptr<PhotonMap> m_photonMap;
};

NORI_REGISTER_CLASS(PhotonMapper, "photonmapper");
NORI_NAMESPACE_END
