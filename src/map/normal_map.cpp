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

#include <nori/object.h>
#include <nori/texture.h>
#include "stb_image.h"
#include <nori/bitmap.h>

#include <filesystem/resolver.h>
#include <fstream>

NORI_NAMESPACE_BEGIN

template <typename T>
class NormalMap : public Texture<T> {
public:
    NormalMap(const PropertyList &props);

    virtual std::string toString() const override;

    virtual T eval(const Point2f & uv) override {
        // map UV in any range to [0, 1]
        Point2f normalized_uv;
        normalized_uv.x() = uv.x() - std::floor(uv.x());
        normalized_uv.y() = uv.y() - std::floor(uv.y());
        // flip V to align convention
        // UV coord set origin at left bottom
        // image coord set origin at left top
        normalized_uv.y() = 1.0f - normalized_uv.y();

        assert(!(normalized_uv.x() < 0.f && normalized_uv.x() > 1.0f));
        assert(!(normalized_uv.y() < 0.f && normalized_uv.y() > 1.0f));

        //change to image space
        // swap x and y
        float y = normalized_uv.x() / m_scale.x() * (m_width - 1);
        float x = normalized_uv.y() / m_scale.y() * (m_height - 1);

        // integer part
        int x_int = int(std::floor(x));
        int y_int = int(std::floor(y));

        float x_frac = x - float(x_int);
        float y_frac = y - float(y_int);

        assert(!(x_frac < 0.0f && x_frac > 1.0f));
        assert(!(y_frac < 0.0f && y_frac > 1.0f));

        // repeat pattern: careful to circle back
        x_int = x_int % m_width;
        y_int = y_int % m_height;

        T v00 = getPixel(x_int, y_int);
        T v10 = getPixel((x_int + 1) % m_width, y_int);
        T v01 = getPixel(x_int, (y_int + 1) % m_height);
        T v11 = getPixel((x_int + 1) % m_width, (y_int + 1) % m_height);


        // bilinear interpolation
        T interpolated_value =
                (1 - x_frac) * (1 - y_frac) * v00 +
                x_frac * (1 - y_frac) * v10 +
                (1 - x_frac) * y_frac * v01 +
                x_frac * y_frac * v11;

        return interpolated_value;
    }

    ~NormalMap() {
        if (m_normal.size() != 0) {
            m_normal.clear();
        }
    }


protected:
    //sRGB format
    std::vector<Normal3f> m_normal;

    Vector2f m_scale;
    int m_width;
    int m_height;
    std::string m_file;

    T getPixel(int x, int y) {
        assert(x >= 0 && x < m_width && y >= 0 && y < m_height);

        return m_normal[y * m_width + x];
    }

    bool readImage(std::string const filename);
};

template <>
bool NormalMap<Normal3f>::readImage(std::string const filename) {
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos) {
        return false; // No extension found
    }
    std::string extension = filename.substr(pos + 1);

    // TODO
    if (extension == "exr") {
        std::cout << "Processing EXR file: " << filename << std::endl;
        // using bit map
        Bitmap bitmap(filename);
        m_height = bitmap.rows();
        m_width = bitmap.cols();
        float max = -INFINITY;
        float min = INFINITY;
        for (int x = 0; x < m_width; ++x) {
            for (int y = 0; y < m_height; ++y) {
                Color3f rgb = bitmap(y, x);

                if (rgb.maxCoeff() > max) {
                    max = rgb.maxCoeff();
                }

                if (rgb.minCoeff() < min) {
                    min = rgb.minCoeff();
                }

                // nori has left hand side frame
                Normal3f n = 2 * Normal3f(rgb.x(), rgb.y(), rgb.z()) - Normal3f(1.0f);
                m_normal.push_back(n.normalized());
            }
        }
        std::cout << "image range: [" << min << ", " << max << "]" << std::endl;
    } else if (extension == "png" || extension == "jpg" || extension == "jpeg") {
        std::cout << "Processing jpg/png file: " << filename << std::endl;
        // using stbi library
        // 1.RGBA/RGB 2.column first order lay out
        int channels, desired_channels;
        if (extension == "png") {
            desired_channels = 4;
        } else {
            // for jpg jpeg
            desired_channels = 3;
        }
        unsigned char* data = stbi_load(filename.c_str(), &m_width, &m_height, &channels, desired_channels);
        if (!data) {
            std::cerr << "Failed to load jpg/png image: " << filename << std::endl;
            return false;
        }
        m_normal.reserve(m_width * m_height);
        float max = -INFINITY;
        float min = INFINITY;
        for (int x = 0; x < m_width; ++x) {
            for (int y = 0; y < m_height; ++y) {
                //careful with channels!
                float R = data[channels * (y * m_width + x)] / 255.0f;
                float G = data[channels * (y * m_width + x) + 1] / 255.0f;
                float B = data[channels * (y * m_width + x) + 2] / 255.0f;
                max = std::max(max, std::max(R, std::max(G, B)));
                min = std::min(min, std::min(R, std::min(G, B)));
                // map [0, 1] to [-1, 1]
                Normal3f n = 2.0 * Normal3f(R, G, B) - Normal3f(1.0f);
                m_normal.push_back(n.normalized());
            }
        }
        //release mem
        stbi_image_free(data);

        std::cout << "image range: [" << min << ", " << max << "]" << std::endl;
//        assert(max <= 1.0f);
//        assert(min >= 0.0f);
    } else {
        std::cerr << "Unsupported file format: " << filename << std::endl;
        return false;
    }

    if (m_normal.size() != 0) {
        std::cout << "Loaded image: " << filename << ", Size:" << m_width << "x" << m_height << std::endl;
        return true;
    }

    return false;
}

template <>
NormalMap<Normal3f>::NormalMap(const PropertyList &props) {
    filesystem::path texture_path =
            getFileResolver()->resolve(props.getString("filename"));

    std::ifstream is(texture_path.str());
    if (is.fail())
        throw NoriException("Unable to open normal map file \"%s\"!", texture_path);

    m_file = texture_path.str();

    if (!readImage(m_file)) {
        throw NoriException("Unable to read normal map file \"%s\"!", texture_path);
    }

    m_scale = props.getVector2("scale", Vector2f(1));
}



template <>
std::string NormalMap<Normal3f>::toString() const {
    return tfm::format(
            "NormalMap[\n"
            "  scale = %s,\n"
            "  normal_map = %s,\n"
            "]",
            m_scale.toString(),
            m_file
    );
}

NORI_REGISTER_TEMPLATED_CLASS(NormalMap, Normal3f, "image_normal")
NORI_NAMESPACE_END
