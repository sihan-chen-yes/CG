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

#include <nori/mesh.h>
#include <nori/timer.h>
#include <filesystem/resolver.h>
#include <unordered_map>
#include <fstream>
#include <Eigen/Dense>

NORI_NAMESPACE_BEGIN

/**
 * \brief Loader for Wavefront OBJ triangle meshes
 */
class WavefrontOBJ : public Mesh {
public:
    WavefrontOBJ(const PropertyList &propList) {
        typedef std::unordered_map<OBJVertex, uint32_t, OBJVertexHash> VertexMap;

        filesystem::path filename =
            getFileResolver()->resolve(propList.getString("filename"));

        std::ifstream is(filename.str());
        if (is.fail())
            throw NoriException("Unable to open OBJ file \"%s\"!", filename);
        Transform trafo = propList.getTransform("toWorld", Transform());

        cout << "Loading \"" << filename << "\" .. \n";
        cout.flush();
        Timer timer;

        std::vector<Vector3f>   positions;
        std::vector<Vector2f>   texcoords;
        std::vector<Vector3f>   normals;
        std::vector<uint32_t>   indices;
        std::vector<OBJVertex>  vertices;
        VertexMap vertexMap;

        std::string line_str;
        while (std::getline(is, line_str)) {
            std::istringstream line(line_str);

            std::string prefix;
            line >> prefix;

            if (prefix == "v") {
                Point3f p;
                line >> p.x() >> p.y() >> p.z();
                p = trafo * p;
                m_bbox.expandBy(p);
                positions.push_back(p);
            } else if (prefix == "vt") {
                Point2f tc;
                line >> tc.x() >> tc.y();
                texcoords.push_back(tc);
            } else if (prefix == "vn") {
                Normal3f n;
                line >> n.x() >> n.y() >> n.z();
                normals.push_back((trafo * n).normalized());
            } else if (prefix == "f") {
                std::string v1, v2, v3, v4;
                line >> v1 >> v2 >> v3 >> v4;
                OBJVertex verts[6];
                int nVertices = 3;

                verts[0] = OBJVertex(v1);
                verts[1] = OBJVertex(v2);
                verts[2] = OBJVertex(v3);

                if (!v4.empty()) {
                    /* This is a quad, split into two triangles */
                    verts[3] = OBJVertex(v4);
                    verts[4] = verts[0];
                    verts[5] = verts[2];
                    nVertices = 6;
                }
                /* Convert to an indexed vertex list */
                for (int i=0; i<nVertices; ++i) {
                    const OBJVertex &v = verts[i];
                    VertexMap::const_iterator it = vertexMap.find(v);
                    if (it == vertexMap.end()) {
                        vertexMap[v] = (uint32_t) vertices.size();
                        indices.push_back((uint32_t) vertices.size());
                        vertices.push_back(v);
                    } else {
                        indices.push_back(it->second);
                    }
                }
            }
        }

        m_F.resize(3, indices.size()/3);
        memcpy(m_F.data(), indices.data(), sizeof(uint32_t)*indices.size());

        m_V.resize(3, vertices.size());
        for (uint32_t i=0; i<vertices.size(); ++i)
            m_V.col(i) = positions.at(vertices[i].p-1);

        if (!normals.empty()) {
            m_N.resize(3, vertices.size());
            for (uint32_t i=0; i<vertices.size(); ++i)
                m_N.col(i) = normals.at(vertices[i].n-1);
        }

        if (!texcoords.empty()) {
            m_UV.resize(2, vertices.size());
            for (uint32_t i=0; i<vertices.size(); ++i)
                m_UV.col(i) = texcoords.at(vertices[i].uv-1);
        }

        size_t meshSize = m_F.size() * sizeof(uint32_t) +
            sizeof(float) * (m_V.size() + m_N.size() + m_UV.size());

        if (meshSize == 0) {
            cout << endl;
            throw NoriException("OBJ file \"%s\" contains no data! Make sure you have Git LFS installed", filename);
        }

        // calculate TBN map
        if (!texcoords.empty()) {
            compute_pervertex_TBN();
            cout << "per vertex TBN map constructed" << endl;
        }

        m_name = filename.str();
        cout << "done. (V=" << m_V.cols() << ", F=" << m_F.cols() << ", took "
            << timer.elapsedString() << " and "
            << memString(meshSize)
            << ")" << endl;
    }

protected:
    /// Vertex indices used by the OBJ format
    struct OBJVertex {
        uint32_t p = (uint32_t) -1;
        uint32_t n = (uint32_t) -1;
        uint32_t uv = (uint32_t) -1;

        inline OBJVertex() { }

        inline OBJVertex(const std::string &string) {
            std::vector<std::string> tokens = tokenize(string, "/", true);

            if (tokens.size() < 1 || tokens.size() > 3)
                throw NoriException("Invalid vertex data: \"%s\"", string);

            p = toUInt(tokens[0]);

            if (tokens.size() >= 2 && !tokens[1].empty())
                uv = toUInt(tokens[1]);

            if (tokens.size() >= 3 && !tokens[2].empty())
                n = toUInt(tokens[2]);
        }

        inline bool operator==(const OBJVertex &v) const {
            return v.p == p && v.n == n && v.uv == uv;
        }
    };

    /// Hash function for OBJVertex
    struct OBJVertexHash {
        std::size_t operator()(const OBJVertex& v) const {
            size_t hash = std::hash<uint32_t>()(v.p);
            hash = hash * 37 + std::hash<uint32_t>()(v.uv);
            hash = hash * 37 + std::hash<uint32_t>()(v.n);
            return hash;
        }
    };

    void compute_pervertex_TBN() {
        // column first for Eigen
        // Initialize m_T and m_B to have the same size as m_V (3, N)
        m_T.resize(3, m_V.cols());
        m_T.setZero();
        m_B.resize(3, m_V.cols());
        m_B.setZero();

        // Iterate over each face to compute tangents and bitangents
        for (int i = 0; i < m_F.cols(); ++i) {
            // Get vertex indices for the current face
            int idx0 = m_F(0, i);
            int idx1 = m_F(1, i);
            int idx2 = m_F(2, i);

            // Get positions and UVs for the current face
            Vector3f p0 = m_V.col(idx0);
            Vector3f p1 = m_V.col(idx1);
            Vector3f p2 = m_V.col(idx2);

            Vector2f uv0 = m_UV.col(idx0);
            Vector2f uv1 = m_UV.col(idx1);
            Vector2f uv2 = m_UV.col(idx2);

            // Compute edges and delta UVs
            Vector3f edge1 = p1 - p0;
            Vector3f edge2 = p2 - p0;
            Vector2f deltaUV1 = uv1 - uv0;
            Vector2f deltaUV2 = uv2 - uv0;

            // Compute tangent and bitangent for this face
            float denom = deltaUV1.x() * deltaUV2.y() - deltaUV1.y() * deltaUV2.x();
            // careful with degenerate triangle
            float r = 1.0f / denom;
            // weighting the degenerate TB with area
            Vector3f T = r * (deltaUV2.y() * edge1 - deltaUV1.y() * edge2);
            Vector3f B = r * (-deltaUV2.x() * edge1 + deltaUV1.x() * edge2);

            // Accumulate the results to the vertices
            m_T.col(idx0) += T;
            m_T.col(idx1) += T;
            m_T.col(idx2) += T;

            m_B.col(idx0) += B;
            m_B.col(idx1) += B;
            m_B.col(idx2) += B;
        }

        // Normalize accumulated tangents and bitangents
        for (int i = 0; i < m_V.cols(); ++i) {
            m_T.col(i).normalize();
            m_B.col(i).normalize();
        }

        for (int i = 0; i < m_V.cols(); ++i) {
            Vector3f N = m_N.col(i).normalized(); // Per-vertex normal
            Vector3f T = m_T.col(i);
            Vector3f B = m_B.col(i);

            // Re-orthogonalize tangent and compute corrected bitangent
            T = (T - N.dot(T) * N).normalized();
            B = N.cross(T).normalized();

            // Store back the orthogonalized results
            m_T.col(i) = T;
            m_B.col(i) = B;
        }
    }
};

NORI_REGISTER_CLASS(WavefrontOBJ, "obj");
NORI_NAMESPACE_END
