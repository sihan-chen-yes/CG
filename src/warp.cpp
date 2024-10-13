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

#include <nori/warp.h>
#include <nori/vector.h>
#include <nori/frame.h>

NORI_NAMESPACE_BEGIN

Vector3f Warp::sampleUniformHemisphere(Sampler *sampler, const Normal3f &pole) {
    // Naive implementation using rejection sampling
    Vector3f v;
    do {
        v.x() = 1.f - 2.f * sampler->next1D();
        v.y() = 1.f - 2.f * sampler->next1D();
        v.z() = 1.f - 2.f * sampler->next1D();
    } while (v.squaredNorm() > 1.f);

    if (v.dot(pole) < 0.f)
        v = -v;
    v /= v.norm();

    return v;
}

Point2f Warp::squareToUniformSquare(const Point2f &sample) {
    return sample;
}

float Warp::squareToUniformSquarePdf(const Point2f &sample) {
    return ((sample.array() >= 0).all() && (sample.array() <= 1).all()) ? 1.0f : 0.0f;
}

Point2f Warp::squareToUniformDisk(const Point2f &sample) {
    Point2f disk_sample;
    float r = sqrt(sample.x());
    float theta = 2 * M_PI * sample.y();
    disk_sample.x() = r * cos(theta);
    disk_sample.y() = r * sin(theta);
    return disk_sample;
}

float Warp::squareToUniformDiskPdf(const Point2f &p) {
    // inside or on
    return (p.squaredNorm() <= 1) ? INV_PI : 0.0f;
}

Vector3f Warp::squareToUniformSphereCap(const Point2f &sample, float cosThetaMax) {
    Vector3f v;
    // theta = 0 => z = 1
    v.z() = 1 - sample.x() * (1 - cosThetaMax);
    float r = sqrt(1 - v.z() * v.z());
    float phi = 2 * M_PI * sample.y();
    v.x() = r * cos(phi);
    v.y() = r * sin(phi);
    return v;
}

float Warp::squareToUniformSphereCapPdf(const Vector3f &v, float cosThetaMax) {
    // on sphere (assuming unit)
    // 1. on the sphere 2.constraint for z
    if (abs(v.norm() - 1.0f) < Epsilon && (v.z() >= cosThetaMax && v.z() <= 1)) {
        // 1 divided by surface area
        return 1.0f / (2 * M_PI * (1 - cosThetaMax));
    }
    return 0.0f;
}

Vector3f Warp::squareToUniformSphere(const Point2f &sample) {
    //using squareToUniformSphereCap with theta = PI
    float cosThetaMax = -1.0f;
    return squareToUniformSphereCap(sample, cosThetaMax);
}

float Warp::squareToUniformSpherePdf(const Vector3f &v) {
    //using squareToUniformSphereCapPdf with theta = PI
    float cosThetaMax = -1.0f;
    return squareToUniformSphereCapPdf(v, cosThetaMax);
}

Vector3f Warp::squareToUniformHemisphere(const Point2f &sample) {
    //using squareToUniformSphereCap with theta = PI / 2
    float cosThetaMax = 0.0f;
    return squareToUniformSphereCap(sample, cosThetaMax);
}

float Warp::squareToUniformHemispherePdf(const Vector3f &v) {
    //using squareToUniformSphereCapPdf with theta = PI / 2
    float cosThetaMax = 0.0f;
    return squareToUniformSphereCapPdf(v, cosThetaMax);
}

Vector3f Warp::squareToCosineHemisphere(const Point2f &sample) {
    Vector3f v;

    // Malley’s method
    // inside or on the disk
    Point2f disk_sample = squareToUniformDisk(sample);
    // project onto the hemisphere
    float r = disk_sample.norm();
    v.z() = sqrt(1 - r * r);
    v.x() = disk_sample.x();
    v.y() = disk_sample.y();
    return v;

//    naive method
//    float theta = asin(sqrt(sample.x()));
//    float phi = 2 * M_PI * sample.y();
//    v.x() = sin(theta) * cos(phi);
//    v.y() = sin(theta) * sin(phi);
//    v.z() = cos(theta);
//    return v;
}

float Warp::squareToCosineHemispherePdf(const Vector3f &v) {
    //assuming unit hemisphere
    // 1. on the sphere 2. appropriate orientation
    if (abs(v.norm() - 1.0f) < Epsilon && v.z() >= 0) {
        // theta:[0, pi / 2]
        float cos_theta = v.z();
        return INV_PI * cos_theta;
    }
    return 0.0f;
}

Vector3f Warp::squareToBeckmann(const Point2f &sample, float alpha) {
    Vector3f v;
    float theta = atan(alpha * sqrt(-log(1 - sample.x())));
    float phi = 2 * M_PI * sample.y();
    //assuming unit
    v.x() = sin(theta) * cos(phi);
    v.y() = sin(theta) * sin(phi);
    v.z() = cos(theta);
    return v;
}

float Warp::squareToBeckmannPdf(const Vector3f &m, float alpha) {
    //assuming unit hemisphere
    if (abs(m.norm() - 1.0f) < Epsilon && m.z() >= 0) {
        float theta = acos(m.z());
        float cos_theta = cos(theta);
        float tan_theta = tan(theta);

        return exp(- (tan_theta * tan_theta) / (alpha * alpha)) / (M_PI * alpha * alpha * cos_theta * cos_theta * cos_theta);
    }
    return 0.0f;
}

Vector3f Warp::squareToUniformTriangle(const Point2f &sample) {
    float su1 = sqrtf(sample.x());
    float u = 1.f - su1, v = sample.y() * su1;
    return Vector3f(u,v,1.f-u-v);
}

NORI_NAMESPACE_END
