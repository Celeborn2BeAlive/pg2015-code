#include "RTScene.hpp"

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>

namespace BnZ {

RTScene::EmbreeInitHandle RTScene::s_EmbreeHandle;

RTScene::EmbreeInitHandle::EmbreeInitHandle() {
    rtcInit("threads=1"); // Init embree with only one thread because we don't want the acceleration data structures to depend on task ordering
}

RTScene::EmbreeInitHandle::~EmbreeInitHandle() {
    rtcExit();
}

static void fillRTCRay(const Ray& ray, RTCRay& rtcRay) {
    rtcRay.org[0] = ray.org.x;
    rtcRay.org[1] = ray.org.y;
    rtcRay.org[2] = ray.org.z;
    rtcRay.dir[0] = ray.dir.x;
    rtcRay.dir[1] = ray.dir.y;
    rtcRay.dir[2] = ray.dir.z;
    rtcRay.tnear = ray.tnear;
    rtcRay.tfar = ray.tfar;
    rtcRay.geomID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.primID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.instID = RTC_INVALID_GEOMETRY_ID;
    rtcRay.mask = 0xFFFFFFFF;
    rtcRay.time = 0.0f;
}

struct RTCRayHandle {
    RTCRay rtcRay;
    const Ray& ray;
    uint32_t threadID;
    const RTScene::FilterFunction* pFilterFunction;

    RTCRayHandle(const Ray& ray, uint32_t threadID, const RTScene::FilterFunction* pFilterFunction = nullptr) :
        ray(ray), threadID(threadID), pFilterFunction(pFilterFunction) {
        fillRTCRay(ray, rtcRay);
    }
};

static bool autoIntersectFilterFunc(RTCRay& ray) {
    RTCRayHandle* handle = (RTCRayHandle*)&ray; // Find the handle containing the rtcRay

    // Test if the intersected primitive is the origin primitive: avoid self intersections of a given triangle
    if ((ray.geomID == handle->ray.orgPrim.x && ray.primID == handle->ray.orgPrim.y)
        || (ray.geomID == handle->ray.dstPrim.x && ray.primID == handle->ray.dstPrim.y)) {
        ray.geomID = RTC_INVALID_GEOMETRY_ID; // According to embree's API, this cancel this intersection
        return true;
    }
    return false;
}

static void intersectFilterFunc(void* userPtr, RTCRay& ray) {
    if (autoIntersectFilterFunc(ray)) {
        return;
    }

    RTCRayHandle* handle = (RTCRayHandle*)&ray; // Find the handle containing the rtcRay
    if (userPtr) {
        RTScene::FilterFunctions* pFunctions = (RTScene::FilterFunctions*)userPtr;
        if (pFunctions->m_pIntersectionFilter) {
            if ((*pFunctions->m_pIntersectionFilter)(RTScene::Hit(ray), handle->threadID)) {
                ray.geomID = RTC_INVALID_GEOMETRY_ID; // According to embree's API, this cancel this intersection
                return;
            }
        }
    }

    if(handle->pFilterFunction && (*handle->pFilterFunction)(RTScene::Hit(ray), handle->threadID)) {
        ray.geomID = RTC_INVALID_GEOMETRY_ID; // According to embree's API, this cancel this intersection
        return;
    }
}

static void occludedFilterFunc(void* userPtr, RTCRay& ray) {
    if (autoIntersectFilterFunc(ray)) {
        return;
    }

    RTCRayHandle* handle = (RTCRayHandle*)&ray; // Find the handle containing the rtcRay
    if (userPtr) {
        RTScene::FilterFunctions* pFunctions = (RTScene::FilterFunctions*)userPtr;
        if (pFunctions->m_pIntersectionFilter) {
            if ((*pFunctions->m_pOcclusionFilter)(RTScene::Hit(ray), handle->threadID)) {
                ray.geomID = RTC_INVALID_GEOMETRY_ID; // According to embree's API, this cancel this intersection
                return;
            }
        }
    }

    if(handle->pFilterFunction && (*handle->pFilterFunction)(RTScene::Hit(ray), handle->threadID)) {
        ray.geomID = RTC_INVALID_GEOMETRY_ID; // According to embree's API, this cancel this intersection
        return;
    }
}

RTScene::Hit::Hit(const RTCRay& ray):
    m_nInstID(ray.instID),
    m_nMeshID(ray.geomID),
    m_nTriangleID(ray.primID),
    m_UV(Vec2f(ray.u, ray.v)),
    m_Ng(normalize(Vec3f(ray.Ng[0], ray.Ng[1], ray.Ng[2]))),
    m_fDistance(ray.tfar),
    m_P(Vec3f(ray.org[0], ray.org[1], ray.org[2]) + ray.tfar * Vec3f(ray.dir[0], ray.dir[1], ray.dir[2])) {
}

RTScene::RTScene():
    m_RTCScene(rtcNewScene(RTC_SCENE_STATIC, RTC_INTERSECT1)) {
}

RTScene::~RTScene() {
    if (m_RTCScene) {
        rtcDeleteScene(m_RTCScene);
    }
}

RTScene::RTScene(RTScene&& scene) {
    *this = std::move(scene);
}

RTScene& RTScene::operator =(RTScene&& scene) {
    std::swap(m_RTCScene, scene.m_RTCScene);
    return *this;
}

void RTScene::addTriangleMesh(const TriangleMesh& mesh, FilterFunctions* pFilterFunctions) {
    auto geoID = rtcNewTriangleMesh(m_RTCScene, RTC_GEOMETRY_STATIC, mesh.m_Triangles.size(),
        mesh.m_Vertices.size());
    rtcSetBuffer(m_RTCScene, geoID, RTC_VERTEX_BUFFER, (void*)(mesh.m_Vertices.data()), 0, sizeof(TriangleMesh::Vertex));
    rtcSetBuffer(m_RTCScene, geoID, RTC_INDEX_BUFFER, (void*)(mesh.m_Triangles.data()), 0, sizeof(TriangleMesh::Triangle)); // Care: this works because uint32 and int32 are compatible in terms of representation
    rtcSetUserData(m_RTCScene, geoID, pFilterFunctions);
    rtcSetOcclusionFilterFunction(m_RTCScene, geoID, occludedFilterFunc);
    rtcSetIntersectionFilterFunction(m_RTCScene, geoID, intersectFilterFunc);
}

void RTScene::addGeometry(const SceneGeometry& geometry) {
    for (const auto& mesh : geometry.getMeshs()) {
        addTriangleMesh(mesh);
    }
}

void RTScene::addInstance(const RTScene& scene) {
    auto pRTCScene = m_RTCScene;
    auto instID = rtcNewInstance(pRTCScene, scene.m_RTCScene);
    Mat4f identity(1.f);
    rtcSetTransform(pRTCScene, instID, RTC_MATRIX_COLUMN_MAJOR_ALIGNED16, value_ptr(identity));
}

void RTScene::commit() {
    rtcCommit(m_RTCScene);
}

bool RTScene::intersect(const Ray& ray, Hit& hit, uint32_t threadID) const {
    RTCRayHandle rayHandle(ray, threadID);

    rtcIntersect(m_RTCScene, rayHandle.rtcRay);

    if (uint32_t(rayHandle.rtcRay.geomID) == RTC_INVALID_GEOMETRY_ID || rayHandle.rtcRay.tfar <= 0.f) {
        return false;
    }

    hit.m_nInstID = rayHandle.rtcRay.instID;
    hit.m_nMeshID = rayHandle.rtcRay.geomID;
    hit.m_nTriangleID = rayHandle.rtcRay.primID;
    hit.m_UV = Vec2f(rayHandle.rtcRay.u, rayHandle.rtcRay.v);
    hit.m_Ng = normalize(Vec3f(rayHandle.rtcRay.Ng[0], rayHandle.rtcRay.Ng[1], rayHandle.rtcRay.Ng[2]));
    hit.m_fDistance = rayHandle.rtcRay.tfar;
    hit.m_P = ray.org + rayHandle.rtcRay.tfar * ray.dir;

    return true;
}

bool RTScene::occluded(const Ray& ray, uint32_t threadID) const {
    RTCRayHandle rayHandle(ray, threadID);
    rtcOccluded(m_RTCScene, rayHandle.rtcRay);
    return rayHandle.rtcRay.geomID == 0;
}

bool RTScene::intersect(const Ray& ray, Hit& hit, const FilterFunction& filter, uint32_t threadID) const {
    RTCRayHandle rayHandle(ray, threadID, &filter);

    rtcIntersect(m_RTCScene, rayHandle.rtcRay);

    if (uint32_t(rayHandle.rtcRay.geomID) == RTC_INVALID_GEOMETRY_ID || rayHandle.rtcRay.tfar <= 0.f) {
        return false;
    }

    hit.m_nInstID = rayHandle.rtcRay.instID;
    hit.m_nMeshID = rayHandle.rtcRay.geomID;
    hit.m_nTriangleID = rayHandle.rtcRay.primID;
    hit.m_UV = Vec2f(rayHandle.rtcRay.u, rayHandle.rtcRay.v);
    hit.m_Ng = normalize(Vec3f(rayHandle.rtcRay.Ng[0], rayHandle.rtcRay.Ng[1], rayHandle.rtcRay.Ng[2]));
    hit.m_fDistance = rayHandle.rtcRay.tfar;
    hit.m_P = ray.org + rayHandle.rtcRay.tfar * ray.dir;

    return true;
}

bool RTScene::occluded(const Ray& ray, const FilterFunction& filter, uint32_t threadID) const {
    RTCRayHandle rayHandle(ray, threadID, &filter);
    rtcOccluded(m_RTCScene, rayHandle.rtcRay);
    return rayHandle.rtcRay.geomID == 0;
}

}
