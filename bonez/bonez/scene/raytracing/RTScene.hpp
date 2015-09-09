#pragma once

#include <functional>
#include <bonez/scene/Ray.hpp>
#include <bonez/scene/SceneGeometry.hpp>

// Forward declaration of embree type for the scene
struct __RTCScene;
typedef __RTCScene* RTCScene;

struct RTCRay;

namespace BnZ {

class RTScene {
public:
    struct Hit {
        uint32_t m_nInstID;
        uint32_t m_nMeshID;
        uint32_t m_nTriangleID;
        Vec2f m_UV; // Barycentric coordinates
        Vec3f m_Ng; // Geometric normal
        float m_fDistance; // Distance from origin of ray to hit point
        Vec3f m_P; // Position of the hit

        Hit() {
        }

        Hit(const RTCRay& ray);
    };
    using FilterFunction = std::function < bool(const Hit& hit, uint32_t threadID) > ;
    struct FilterFunctions {
        const FilterFunction* m_pIntersectionFilter = nullptr;
        const FilterFunction* m_pOcclusionFilter = nullptr;

        FilterFunctions(const FilterFunction* pInterFilter = nullptr, const FilterFunction* pOcclusionFilter = nullptr) :
            m_pIntersectionFilter(pInterFilter), m_pOcclusionFilter(pOcclusionFilter) {
        }
    };

    RTScene();

    ~RTScene();

    RTScene(const RTScene&) = delete;

    RTScene& operator =(const RTScene&) = delete;

    RTScene(RTScene&&);

    RTScene& operator =(RTScene&&);

    void addTriangleMesh(const TriangleMesh& mesh, FilterFunctions* pFilterFunctions = nullptr);

    void addGeometry(const SceneGeometry& geometry);

    void addInstance(const RTScene& scene);

    void commit();

    bool intersect(const Ray& ray, Hit& hit, uint32_t threadID = 0u) const;

    bool occluded(const Ray& ray, uint32_t threadID = 0u) const;

    bool intersect(const Ray& ray, Hit& hit, const FilterFunction& filter, uint32_t threadID = 0u) const;

    bool occluded(const Ray& ray, const FilterFunction& filter, uint32_t threadID = 0u) const;

private:
    RTCScene m_RTCScene = nullptr; //! Embree scene

    struct EmbreeInitHandle {
        EmbreeInitHandle(); // Init embree

        ~EmbreeInitHandle(); // Exit embree
    };

    static EmbreeInitHandle s_EmbreeHandle;
};

}
