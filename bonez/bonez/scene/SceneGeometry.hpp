#pragma once

#include <bonez/types.hpp>
#include <bonez/maths/BBox.hpp>
#include <bonez/sys/files.hpp>
#include <bonez/utils/itertools/Range.hpp>

#include <algorithm>

#include "Intersection.hpp"
#include "Ray.hpp"
#include "shading/Material.hpp"

struct aiScene;

namespace tinyxml2 {
    class XMLElement;
}

namespace BnZ {

struct TriangleMesh {
    struct Vertex {
        Vec3f position;
        Vec3f normal;
        Vec2f texCoords;

        Vertex() {
        }

        Vertex(const Vec3f& p, const Vec3f& n, const Vec2f& texCoords) :
            position(p), normal(n), texCoords(texCoords) {
        }
    };

    struct Triangle {
        uint32_t v0, v1, v2;

        Triangle() {
        }

        Triangle(uint32_t v0, uint32_t v1, uint32_t v2) :
            v0(v0), v1(v1), v2(v2) {
        }
    };

    std::vector<Vertex> m_Vertices;
    std::vector<Triangle> m_Triangles;
    uint32_t m_MaterialID;

    BBox3f m_BBox;

    int32_t m_nLightID = -1;

    const Vertex& getVertex(uint32_t index) const {
        return m_Vertices[index];
    }

    const Triangle& getTriangle(uint32_t primID) const {
        return m_Triangles[primID];
    }

    const std::vector<Triangle>& getTriangles() const {
        return m_Triangles;
    }

    size_t getTriangleCount() const {
        return m_Triangles.size();
    }

    float getTriangleArea(uint32_t primID) const {
        const auto& triangle = m_Triangles[primID];
        auto e0 = Vec3f(m_Vertices[triangle.v1].position - m_Vertices[triangle.v0].position);
        auto e1 = Vec3f(m_Vertices[triangle.v2].position - m_Vertices[triangle.v0].position);
        return 0.5f * length(cross(e0, e1));
    }

    float getArea() const {
        auto area = 0.f;
        for(auto i = 0u; i < m_Triangles.size(); ++i) {
            area += getTriangleArea(i);
        }
        return area;
    }

    void transform(const Mat4f& localToWorldMatrix) {
        auto normalMatrix = transpose(inverse(localToWorldMatrix));
        for(auto i = 0u; i < m_Vertices.size(); ++i) {
            auto& vertex = m_Vertices[i];
            vertex.position = Vec3f(localToWorldMatrix * Vec4f(vertex.position, 1.f));
            vertex.normal = normalize(Vec3f(normalMatrix * Vec4f(vertex.normal, 0.f)));

            if(i == 0u) {
                m_BBox = BBox3f(vertex.position);
            } else {
                m_BBox.grow(vertex.position);
            }
        }
    }
};

Sample<TriangleMesh::Vertex> sample(const TriangleMesh& mesh, float triangleSample,
                                    const Vec2f& pointSample, std::size_t* pTriangleID = nullptr);

Sample<TriangleMesh::Vertex> sampleTriangle(const TriangleMesh& mesh, size_t triangleIdx, const Vec2f& s2D);

TriangleMesh::Vertex computeTriangleCenter(const TriangleMesh& mesh, size_t triangleIdx);

class SceneGeometry {
    std::vector<TriangleMesh> m_TriangleMeshs;
    std::vector<Material> m_Materials;
    std::vector<uint32_t> m_EmissiveMeshs; // Store the index of every mesh that have an emissive material
	
    friend void loadAssimpScene(const aiScene* aiscene, const std::string& filepath, SceneGeometry& geometry);

    friend SceneGeometry loadMesh(const FilePath& path, const tinyxml2::XMLElement& meshDescription);

    friend SceneGeometry loadGeometry(const FilePath& path, const tinyxml2::XMLElement& geometryDescription);

    friend SceneGeometry loadSphere(const FilePath& path, const tinyxml2::XMLElement& sphereDescription);

    friend SceneGeometry loadDisk(const FilePath& path, const tinyxml2::XMLElement& diskDescription);

    friend SceneGeometry loadQuad(const FilePath& path, const tinyxml2::XMLElement& quadDescription);

    BBox3f m_BBox;
public:
    void clear() {
        m_TriangleMeshs.clear();
        m_Materials.clear();
        m_BBox = BBox3f(Vec3f(0));
    }

    void transform(const Mat4f& localToWorldMatrix) {
        for(auto i = 0u; i < m_TriangleMeshs.size(); ++i) {
            auto& mesh = m_TriangleMeshs[i];
            mesh.transform(localToWorldMatrix);
            if(i == 0u) {
                m_BBox = mesh.m_BBox;
            } else {
                m_BBox.grow(mesh.m_BBox);
            }
        }
    }

    const TriangleMesh& getMesh(uint32_t geomID) const {
        return m_TriangleMeshs[geomID];
    }

    TriangleMesh& getMesh(uint32_t geomID) {
        return m_TriangleMeshs[geomID];
    }

    const std::vector<TriangleMesh>& getMeshs() const {
        return m_TriangleMeshs;
    }

    const std::vector<uint32_t>& getEmissiveMeshIndices() const {
        return m_EmissiveMeshs;
    }

    size_t getMeshCount() const {
        return m_TriangleMeshs.size();
    }

    const Material& getMaterial(uint32_t matID) const {
        return m_Materials[matID];
    }

    const Material& getMaterial(const TriangleMesh& mesh) const {
        return m_Materials[mesh.m_MaterialID];
    }

    Material& getMaterial(uint32_t matID) {
        return m_Materials[matID];
    }

    int getMaterialID(const std::string& name) const {
        const auto it = std::find_if(std::begin(m_Materials), std::end(m_Materials), [&name](const Material& material) {
            return material.getName() == name;
        });
        if(it == std::end(m_Materials)) {
            return -1;
        }
        return int(it - std::begin(m_Materials));
    }

    const std::vector<Material>& getMaterials() const {
        return m_Materials;
    }

    size_t getMaterialCount() const {
        return m_Materials.size();
    }

    const BBox3f& getBBox() const {
        return m_BBox;
    }

    void postIntersect(const Ray& ray, Intersection& I) const;

    void getSurfacePoint(uint32_t meshID, uint32_t triangleID,
                         float u, float v, SurfacePoint& point) const;

    void append(SceneGeometry geometry) {
        if(m_TriangleMeshs.empty()) {
            m_BBox = geometry.m_BBox;
        } else {
            m_BBox.grow(geometry.m_BBox);
        }
        auto materialOffset = m_Materials.size();
        for(auto& material: geometry.m_Materials) {
            m_Materials.emplace_back(std::move(material));
        }
        for(auto& mesh: geometry.m_TriangleMeshs) {
            m_TriangleMeshs.emplace_back(std::move(mesh));
            m_TriangleMeshs.back().m_MaterialID += uint32_t(materialOffset);
//            if(isEmissive(m_Materials[m_TriangleMeshs.back().m_MaterialID])) {
//                m_EmissiveMeshs.emplace_back(m_TriangleMeshs.size() - 1);
//            }
        }
    }

    void append(TriangleMesh mesh) {
        if(m_TriangleMeshs.empty()) {
            m_BBox = mesh.m_BBox;
        }  else {
            m_BBox.grow(mesh.m_BBox);
        }

        m_TriangleMeshs.emplace_back(std::move(mesh));
//        if(isEmissive(m_Materials[m_TriangleMeshs.back().m_MaterialID])) {
//            m_EmissiveMeshs.emplace_back(m_TriangleMeshs.size() - 1);
//        }
    }

    void extractEmissiveMeshes() {
        m_EmissiveMeshs.clear();
        for(auto index: range(m_TriangleMeshs.size())) {
            if(isEmissive(m_Materials[m_TriangleMeshs[index].m_MaterialID])) {
                m_EmissiveMeshs.emplace_back(index);
            }
        }
    }

    uint32_t addMaterial(Material material) {
        m_Materials.emplace_back(material);
        return m_Materials.size() - 1u;
    }

    void setMeshLight(uint32_t meshID, int32_t lightID) {
        m_TriangleMeshs[meshID].m_nLightID = lightID;
    }
};

// Load a model with format handled by assimp
SceneGeometry loadModel(const std::string& filepath);

// Load geometry from a XML description
SceneGeometry loadGeometry(const FilePath& path, const tinyxml2::XMLElement& geometryDescription);

}
