/*
    This file is part of Nori, a simple educational ray tracer

    Copyright (c) 2024 by CGL, ETH Zurich

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

#include <nori/texture.h>

NORI_NAMESPACE_BEGIN

template <typename T>
class UVTexture : public Texture<T> {
public:
    UVTexture(const PropertyList& props) { }

    virtual T eval(const Point2f & uv) override {
        const float C = 8 * 2 * M_PI;
        float u = uv.x();
        float v = uv.y();

        float r = abs(fmod(u, 1.f));
        float g = abs(fmod(v, 1.f));
        float b = 0.5;

        // Adding cyclic pattern
        r *= (1.f - 0.2 * std::sin(C * u) * std::sin(C * v));
        g *= (1.f - 0.2 * std::cos(C * u) * std::sin(C * v));

        return Color3f(r, g, b);
    }

    virtual std::string toString() const override {
        return "UVTexture[]";
    }
};

NORI_REGISTER_TEMPLATED_CLASS(UVTexture, Color3f, "uvtexture")

NORI_NAMESPACE_END
