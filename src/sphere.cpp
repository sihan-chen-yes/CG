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

#include <nori/shape.h>
#include <nori/bsdf.h>
#include <nori/emitter.h>
#include <nori/warp.h>

NORI_NAMESPACE_BEGIN

class Sphere : public Shape {
public:
    Sphere(const PropertyList & propList) {
        m_position = propList.getPoint3("center", Point3f());
        m_radius = propList.getFloat("radius", 1.f);

        m_bbox.expandBy(m_position - Vector3f(m_radius));
        m_bbox.expandBy(m_position + Vector3f(m_radius));
    }

    virtual BoundingBox3f getBoundingBox(uint32_t index) const override { return m_bbox; }

    virtual Point3f getCentroid(uint32_t index) const override { return m_position; }

    virtual bool rayIntersect(uint32_t index, const Ray3f &ray, float &u, float &v, float &t) const override {
        Vector3f O = ray.o;
        Vector3f d = ray.d;
        Vector3f C = getCentroid(index);
        // determinant delta = b^2 - 4ac
        float a = d.dot(d);
        float b = 2 * d.dot(O - C);
        float c = (O - C).dot(O - C) - pow(m_radius, 2);
        float delta_square = pow(b, 2) - 4 * a * c;
        float t_min = ray.mint;
        float t_max = ray.maxt;
        // no intersections
        bool intersect = false;
        if (delta_square == 0) {
            //only one intersection
            float t1 = -b / (2 * a);
            //should in [mint, maxt] and bigger than 0
            if (t1 >= 0 && t1 >= t_min && t1 <= t_max) {
                t = t1;
                intersect = true;
            }
        } else if (delta_square > 0) {
            // two intersections t1 < t2
            float t1 = (-b - sqrt(delta_square)) / (2 * a);
            float t2 = (-b + sqrt(delta_square)) / (2 * a);
            // check smaller one first
            if (t1 >= 0 && t1 >= t_min && t1 <= t_max) {
                t = t1;
                intersect = true;
            } else if (t2 >= 0 && t2 >= t_min && t2 <= t_max) {
                // check bigger one then
                t = t2;
                intersect = true;
            }
        }

        if (intersect) {
            // set UV
            Point3f p = ray.o + ray.d * t;
            // map sphere coordinate to uv coordinate
            Vector3f rel_p = (p - getCentroid(index));
            // x = rsin(theta)cos(phi), y = rsin(theta)sin(phi) z = rcos(theta)
            //[0, pi] need to handle the precision
            float theta = acos(std::clamp(rel_p.z() / m_radius, -1.0f, 1.0f));
            //[-pi, pi]
            float phi = atan2(rel_p.y(), rel_p.x());

            // map to [0, 1]
            u = phi / (2 * M_PI);
            v = 1 - theta / M_PI;

            // check UV
//            if (!std::isfinite(u) || std::isnan(u)) {
//                cout << "U out of range: " << u << endl;
//            }
//            if (!std::isfinite(v) || std::isnan(v)) {
//                cout << "V out of range: " << v << endl;
//            }
        }
        return intersect;
    }

    virtual void setHitInformation(uint32_t index, const Ray3f &ray, Intersection & its) const override {
        float u = 0.0;
        float v = 0.0;
        float t = 0.0;
        // get intersection point
        bool res = rayIntersect(index, ray, u, v, t);
        assert(res == true);
        Point3f p = ray.o + ray.d * t;

        Vector3f normal = (p - getCentroid(index)).normalized();

        // fill its properties
        // intersection point
        its.p = p;
        // same geo and sh frame for sphere
        its.geoFrame = Frame(normal);
        its.shFrame = Frame(normal);

        its.uv.x() = u;
        its.uv.y() = v;
    }

    virtual void sampleSurface(ShapeQueryRecord & sRec, const Point2f & sample) const override {
        Vector3f q = Warp::squareToUniformSphere(sample);
        sRec.p = m_position + m_radius * q;
        sRec.n = q;
        sRec.pdf = std::pow(1.f/m_radius,2) * Warp::squareToUniformSpherePdf(Vector3f(0.0f,0.0f,1.0f));
    }
    virtual float pdfSurface(const ShapeQueryRecord & sRec) const override {
        return std::pow(1.f/m_radius,2) * Warp::squareToUniformSpherePdf(Vector3f(0.0f,0.0f,1.0f));
    }


    virtual std::string toString() const override {
        return tfm::format(
                "Sphere[\n"
                "  center = %s,\n"
                "  radius = %f,\n"
                "  bsdf = %s,\n"
                "  emitter = %s\n"
                "]",
                m_position.toString(),
                m_radius,
                m_bsdf ? indent(m_bsdf->toString()) : std::string("null"),
                m_emitter ? indent(m_emitter->toString()) : std::string("null"));
    }

protected:
    Point3f m_position;
    float m_radius;
};

NORI_REGISTER_CLASS(Sphere, "sphere");
NORI_NAMESPACE_END
