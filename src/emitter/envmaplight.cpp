#include <nori/emitter.h>
#include <filesystem/resolver.h>
#include <fstream>
#include <nori/bitmap.h>
#include <nori/dpdf.h>

NORI_NAMESPACE_BEGIN

class EnvMapLight : public Emitter {
public:
    EnvMapLight(const PropertyList & propList) {
        m_lightToWorld = propList.getTransform("toWorld", Transform());
        m_worldToLight = m_lightToWorld.getInverseMatrix();

        filesystem::path texture_path =
                getFileResolver()->resolve(propList.getString("filename"));

        std::ifstream is(texture_path.str());
        if (is.fail())
            throw NoriException("Unable to open env map file \"%p_total\"!", texture_path);

        m_file = texture_path.str();

        if (!readEnvMap(m_file)) {
            throw NoriException("Unable to read env map file \"%p_total\"!", texture_path);
        }

        m_radiance_scale = propList.getFloat("radiance_scale", 1.0f);

        /*
         * Warning: p(u, v) = L(theta, phi) * sin(theta)
         * We stored the p(u, v) in the grid
         */
        ///////
        // precompute m_radiance with sin(theta) term
        m_luminance.resize(m_radiance.size());
        for (int y = 0; y < m_height; ++y) {
            // prevent sin(theta) equals 0.f
            float sinTheta = abs(sin((y + 0.5f) * (M_PI / m_height)));
            for (int x = 0; x < m_width; ++x) {
                m_luminance[y * m_width + x] = m_radiance[y * m_width + x].getLuminance() * sinTheta;
            }
        }

        // preprocess for sampling the env map light
        // calculate the CDF struct of marginal p(theta)
        m_marginal = DiscretePDF (m_height);
        for (int y = 0; y < m_height; ++y) {
            float rowSum = 0.0f;
            for (int x = 0; x < m_width; ++x) {
                rowSum += m_luminance[y * m_width + x]; // Sum of all radiance in row `y`
            }
            m_marginal.append(rowSum); // Append the row sum to the marginal PDF
        }
        m_marginal.normalize(); // Normalize the marginal PDF

        // calculate the CDF struct of conditional probability
        m_conditionals.resize(m_height);
        for (int y = 0; y < m_height; ++y) {
            // p(phi|theta)
            DiscretePDF conditional(m_width);
            for (int x = 0; x < m_width; ++x) {
                // p(phi|theta) = p(phi, theta) / p(theta)
                conditional.append(m_luminance[y * m_width + x]); // Append value for each column
            }
            conditional.normalize(); // Normalize the conditional PDF for row `y`
            m_conditionals[y] = conditional; // Store the CDF struct
        }
        ///////

        cout << "initialized env map class" << endl;

//        /*
//         *  for pdf debugging
//         */
//        float p_total = 0.0f;
//        float p_vertical = 0.0f;
//        for (int y = 0; y < m_height; ++y) {
//            float p_horizontal = 0.0f;
//            float p1 = m_marginal[y];
//            p_vertical += p1;
//            for (int x = 0; x < m_width; ++x) {
//                float p2 = m_conditionals[y][x];
//                p_horizontal += p2;
//                p_total += p1 * p2;
//            }
//            if (p_horizontal != 1.0f) {
//                cout << "p_horizontal:" << p_horizontal << endl;
//                cout << y << endl;
//            }
//        }
//        if (p_vertical != 1.0f) {
//            cout << "p_vertical:" << p_vertical << endl;
//        }
//        if (p_total != 1.0f) {
//            cout << "p_total:" << p_total << endl;
//        }


    }

    virtual std::string toString() const override {
        return tfm::format(
                "EnvMapLight[\n"
                "  radiance_scale = %f,\n"
                "  env map file = %s,\n"
                "  WxH = %dx%d\n"
                "]",
                m_radiance_scale,
                m_file,
                m_width, m_height
        );
    }

    virtual Color3f eval(const EmitterQueryRecord & lRec) const override {
        Vector3f w = (m_worldToLight * lRec.wi).normalized();

        Point2f res = sphericalCoordinates(w);
        float theta = res.x();
        float phi = res.y();
        float sinTheta = std::sin(theta);
        // 0 probability
        if (sinTheta < Epsilon) {
            return 0.0f;
        }

        // u: theta [0, pi]
        // v: phi[0, 2 * pi]
        // uv in range [0, 1]
        float u = theta / M_PI;
        float v = phi / (2 * M_PI);
        // find the corresponding grid to return value
        int y = round(u * (m_height - 1));
        int x = round(v * (m_width - 1));

        return m_radiance[y * m_width + x];
    }

    virtual Color3f sample(EmitterQueryRecord & lRec, const Point2f & sample) const override {
        // fill properties of query record before return
        // p is at infinite far place, ignore shadowRay and p
        // sample xi uniformly in [0, 1] then get u, v
        float xi1 = sample.x();
        float xi2 = sample.y();
        float p_u = 0.0f, p_v = 0.0f;
        // importance sampling
        // xy: corresponding grid index
        int y = m_marginal.sample(xi1, p_u);
        int x = m_conditionals[y].sample(xi2, p_v);
        // get corresponding uv domain values [0, 1]
        float u = float(y) / m_height;
        float v = float(x) / m_width;
        // map uv to theta phi domain
        float theta = u * M_PI;
        float phi = v * 2 * M_PI;
        Vector3f wi = sphericalDirection(theta, phi);
        lRec.wi = m_lightToWorld * wi;
        lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, INFINITY);
        lRec.pdf = pdf(lRec);

        // check pdf
        if (lRec.pdf < Epsilon) {
            // failed smapling
            return Color3f(0.0f);
        }

        /*
         * for debugging
         */
//        Color3f test1 = eval(lRec);
//        Color3f test2 = m_radiance[y * m_width + x];
//        float test_p1 = pdf(lRec);
//        float test_p2 = p_u * p_v / sin(theta);
//        assert(test1 = test2);
//        assert(test_p1 == test_p2);

        // radiance Li
        return eval(lRec) / lRec.pdf;
    }

    virtual float pdf(const EmitterQueryRecord &lRec) const override {
        Vector3f w = (m_worldToLight * lRec.wi).normalized();
        // u:theta v:phi
        Point2f res = sphericalCoordinates(w);
        float theta = res.x();
        float phi = res.y();
        float sinTheta = std::sin(theta);
        // 0 probability
        if (sinTheta < Epsilon) {
            return 0.0f;
        }

        float u = theta / M_PI;
        float v = phi / (2 * M_PI);
        assert(u >= 0 && u <= 1.0f);
        assert(v >= 0 && v <= 1.0f);

        // map to corresponding grid
        int y = round(u * (m_height - 1));
        int x = round(v * (m_width - 1));

        //p(u)
        float p_u = m_marginal[y];
        //p(v|u)
        float p_v = m_conditionals.at(y)[x];

        // joint distribution for p(u, v)
        float p_joint = p_u * p_v;

        /*
         * for debugging, should be very few points
         */
//        if (p_u * m_height < Epsilon) {
//            cout << p_u * m_height<< endl;
//            cout << "HxW:" << y << "x" << x << endl;
//        }
//        if (p_v * m_width < Epsilon) {
//            cout << p_v * m_width << endl;
//            cout << "HxW:" << y << "x" << x << endl;
//
//        }

        // return solid measure, need consider two jacobians!
        return p_joint * (m_width * m_height) / (sinTheta * 2 * M_PI * M_PI);
    }

    virtual bool isEnvMapLight() const override {
        return true;
    }

protected:
    std::vector<Color3f> m_radiance;
    std::vector<float> m_luminance;
    DiscretePDF m_marginal;
    std::vector<DiscretePDF> m_conditionals;

    // used for adjust convention
    Transform m_worldToLight;
    Transform m_lightToWorld;

    float m_radiance_scale;
    int m_width;
    int m_height;
    std::string m_file;

    bool readEnvMap(std::string const filename) {
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos) {
            return false; // No extension found
        }
        std::string extension = filename.substr(pos + 1);

        if (extension == "exr") {
            std::cout << "Processing EXR file: " << filename << std::endl;
            // using bit map
            Bitmap bitmap(filename);
            m_height = bitmap.rows();
            m_width = bitmap.cols();
            for (int x = 0; x < m_width; ++x) {
                for (int y = 0; y < m_height; ++y) {
                    m_radiance.push_back(bitmap(y, x));
                }
            }
        } else {
            std::cerr << "Only support exr, unsupported file format: " << filename << std::endl;
            return false;
        }

        if (m_radiance.size() != 0) {
            std::cout << "Loaded image: " << filename << ", Size:" << m_width << "x" << m_height << std::endl;
            return true;
        }

        return false;
    }

};

NORI_REGISTER_CLASS(EnvMapLight, "envmap");
NORI_NAMESPACE_END