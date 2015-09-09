#include "Scene.hpp"
#include "bonez/maths/maths.hpp"
#include "bonez/sampling/distribution1d.h"
#include "bonez/sampling/shapes.hpp"
#include "lights/AreaLight.hpp"
#include <bonez/sys/threads.hpp>

#include <bonez/scene/topology/VSkelLoader.hpp>

#include <bonez/voxskel/GLVoxelFramebuffer.hpp>
#include <bonez/voxskel/CubicalComplex3D.hpp>
#include <bonez/voxskel/ThinningProcessDGCI2013.hpp>
#include <bonez/voxskel/GLVoxelizerTripiana2009.hpp>
#include <bonez/voxskel/discrete_functions.hpp>

namespace BnZ {

Scene::~Scene() {
}

void Scene::buildAccel() {
    m_RTScene.addGeometry(m_Geometry);
    m_RTScene.commit();
}

void Scene::extractAreaLights() {
    for(auto meshIdx: m_Geometry.getEmissiveMeshIndices()) {
        auto light = makeShared<AreaLight>(meshIdx,
                                           m_Geometry.getMesh(meshIdx),
                                           m_Geometry.getMaterial(m_Geometry.getMesh(meshIdx).m_MaterialID));
        auto lightID = m_LightContainer.addLight(light);
        m_Geometry.setMeshLight(meshIdx, lightID);
    }
}

void Scene::buildSamplingDistribution() {
    m_fTotalArea = 0.f;

    m_MeshSamplingDistribution.resize(getDistribution1DBufferSize(m_Geometry.getMeshCount()));
    m_MeshDistributionOffsets.reserve(m_Geometry.getMeshCount());
    auto offset = 0u;
    for(const auto& mesh: m_Geometry.getMeshs()) {
        m_MeshDistributionOffsets.emplace_back(offset);
        auto distSize = getDistribution1DBufferSize(mesh.getTriangleCount());
        offset += distSize;
    }
    m_TriangleSamplingDistributions.resize(offset);
    auto i = 0u;
    for(const auto& mesh: m_Geometry.getMeshs()) {
        auto totalArea = 0.f;
        auto offset = m_MeshDistributionOffsets[i];
        auto ptr = m_TriangleSamplingDistributions.data() + offset;
        for(auto triangleIndex = 0u; triangleIndex < mesh.getTriangleCount(); ++triangleIndex) {
            auto area = mesh.getTriangleArea(triangleIndex);
            ptr[triangleIndex] = area;
            totalArea += area;
        }

        buildDistribution1D(
            [ptr](uint32_t triangleIndex) {
                return ptr[triangleIndex];
            },
            ptr,
            mesh.getTriangleCount());
        m_MeshSamplingDistribution[i] = totalArea;
        ++i;

        m_fTotalArea += totalArea;
    }
    auto ptr = m_MeshSamplingDistribution.data();
    buildDistribution1D(
        [&](uint32_t meshIndex) {
            return ptr[meshIndex];
        },
        ptr,
        m_Geometry.getMeshCount());

    m_fTotalArea *= 2.f; // Both side of each triangle to take into account
}

Intersection Scene::postIntersect(const Ray& ray, const RTScene::Hit& hit) const {
    Intersection I;
    I.meshID = hit.m_nMeshID;
    I.triangleID = hit.m_nTriangleID;
    I.uv = hit.m_UV;
    I.Ng = hit.m_Ng;
    I.distance = hit.m_fDistance;
    I.P = hit.m_P;

    m_Geometry.postIntersect(ray, I);
    return I;
}

Intersection Scene::intersect(const Ray& ray) const {
    RTScene::Hit hit;
    if (!m_RTScene.intersect(ray, hit)) {
        Intersection I;
        I.Le = Le(ray.dir);
        return I;
    }

    return postIntersect(ray, hit);
}

bool Scene::occluded(const Ray& ray) const {
    return m_RTScene.occluded(ray);
}

void Scene::uniformSampleSurfacePoints(uint32_t count,
                                       const float* s1DMeshBuffer, // Used to sample a mesh
                                       const float* s1DTriangleBuffer, // Used to sample a triangle
                                       const float* s1DTriangleSideBuffer, // Used to sample the side of the mesh
                                       const Vec2f* s2DTriangleBuffer, // Used to sample 2D area of the chosen triangle
                                       SurfacePointSample* sampledPointsBuffer) const {
    for(auto i = 0u; i < count; ++i) {
        auto sampledMesh = sampleDiscreteDistribution1D(m_MeshSamplingDistribution.data(),
                                                        m_Geometry.getMeshCount(),
                                                        s1DMeshBuffer[i]);
        if(sampledMesh.pdf > 0.f) {
            const auto& mesh = m_Geometry.getMesh(sampledMesh.value);
            auto sampledTriangle = sampleDiscreteDistribution1D(m_TriangleSamplingDistributions.data() + m_MeshDistributionOffsets[sampledMesh.value],
                                                                mesh.getTriangleCount(),
                                                                s1DTriangleBuffer[i]);
            if(sampledTriangle.pdf > 0.f) {
                const auto& triangle = mesh.getTriangle(sampledTriangle.value);
                auto sampledUV = uniformSampleTriangleUVs(s2DTriangleBuffer[i].x, s2DTriangleBuffer[i].y,
                                                          mesh.getVertex(triangle.v0).position,
                                                          mesh.getVertex(triangle.v1).position,
                                                          mesh.getVertex(triangle.v2).position);
                auto area = mesh.getTriangleArea(sampledTriangle.value);

                m_Geometry.getSurfacePoint(sampledMesh.value, sampledTriangle.value,
                                           sampledUV.x, sampledUV.y, sampledPointsBuffer[i].value);
                sampledPointsBuffer[i].pdf = 0.5f * sampledTriangle.pdf * sampledMesh.pdf / area; // 0.5f stands for the side selection

                if(s1DTriangleSideBuffer[i] <= 0.5f) {
                    sampledPointsBuffer[i].value.Ng = -sampledPointsBuffer[i].value.Ng;
                    sampledPointsBuffer[i].value.Ns = -sampledPointsBuffer[i].value.Ns;
                }

            } else {
                sampledPointsBuffer[i].pdf = 0.f;
            }
        } else {
            sampledPointsBuffer[i].pdf = 0.f;
        }
    }

}

void Scene::computeDiscreteData(uint32_t resolution, bool segmented, const GLShaderManager& shaderManager) {
    m_pGLScene = makeShared<GLScene>(m_Geometry);

    GLVoxelFramebuffer voxelFramebuffer;
    Mat4f gridToWorldMatrix;
    CubicalComplex3D skeletonCubicalComplex, emptySpaceCubicalComplex;
    GLVoxelizerTripiana2009 voxelizer(shaderManager);
    ThinningProcessDGCI2013 thinningProcess;

    Timer timer(true);
    std::clog << "Compute discrete scene dat at resolution " << resolution << std::endl;

    {
        Timer timer(true);
        std::clog << "Voxelizing the scene" << std::endl;
        voxelizer.initGLState(resolution, getBBox(),
            voxelFramebuffer, gridToWorldMatrix);
        m_pGLScene->render();
        voxelizer.restoreGLState();
    }

    {
        Timer timer(true);
        std::clog << "Convert empty space voxel grid to CC3D" << std::endl;
        emptySpaceCubicalComplex = skeletonCubicalComplex = getCubicalComplexFromVoxelFramebuffer(voxelFramebuffer, true);
    }


    auto distanceMap = [&](){
        Timer timer(true);
        std::clog << "Compute distance map" << std::endl;
        return computeDistanceMap26(emptySpaceCubicalComplex, CubicalComplex3D::isInObject, true);
    }();

    auto openingMap = [&]() {
        Timer timer(true);
        std::clog << "Compute opening map" << std::endl;
        return parallelComputeOpeningMap26(distanceMap);
    }();

    {
        Timer timer(true);
        std::clog << "Initialize the thinning process" << std::endl;
        thinningProcess.init(skeletonCubicalComplex, &distanceMap, &openingMap);
    }

    {
        Timer timer(true);
        std::clog << "Run the thinning process" << std::endl;
        thinningProcess.directionalCollapse();
    }

    if(segmented) {
        Timer timer(true);
        std::clog << "Convert skeleton CC3D to Curvilinear Skeleton (segmented)" << std::endl;
        auto skeleton = getSegmentedCurvilinearSkeleton(skeletonCubicalComplex, emptySpaceCubicalComplex,
                                                        distanceMap, openingMap,
                                                        gridToWorldMatrix);

        setCurvSkeleton(skeleton);
    } else {
        Timer timer(true);
        std::clog << "Convert skeleton CC3D to Curvilinear Skeleton (NOT segmented)" << std::endl;
        auto skeleton = getCurvilinearSkeleton(skeletonCubicalComplex, emptySpaceCubicalComplex,
                                               distanceMap, openingMap,
                                               gridToWorldMatrix);
        setCurvSkeleton(skeleton);
    }

    std::cerr << "Number of nodes = " << m_pCurvSkel->size() << std::endl;
}

Vec3f Scene::Le(const Vec3f& wi) const {
    Vec3f L = zero<Vec3f>();
    for(auto pEnvLightIdx: m_EnvironmentLightIndices) {
        auto pEnvLight = static_cast<EnvironmentLight*>(m_LightContainer.getLight(pEnvLightIdx).get());
        L += pEnvLight->Le(wi);
    }
    return L;
}

Scene::Scene(const tinyxml2::XMLElement* pSceneDescription,
             const FilePath& sceneDescriptionFileDir,
             const GLShaderManager& shaderManager) {
    // Load geometry
    auto pGeometry = pSceneDescription->FirstChildElement("Geometry");
    if(!pGeometry) {
        throw std::runtime_error("No Geometry tag in scene setting file");
    }

    m_Geometry = loadGeometry(sceneDescriptionFileDir, *pGeometry);

    buildAccel();
    extractAreaLights();
    buildSamplingDistribution();

    if(auto pLights = pSceneDescription->FirstChildElement("Lights")) {
        loadLights(*this, pLights);
    }

    // Load skeleton
    if(auto pCurvSkel = pSceneDescription->FirstChildElement("CurvSkel")) {
        const char* curvSkelPath = pCurvSkel->Attribute("path");
        if(curvSkelPath) {
            VSkelLoader loader(sceneDescriptionFileDir);
            setCurvSkeleton(loader.loadCurvilinearSkeleton(curvSkelPath));
        } else {
            auto pVoxelGrid = pCurvSkel->FirstChildElement("VoxelGrid");
            if(!pVoxelGrid) {
                throw std::runtime_error("No VoxelGrid tag (required to build the skeleton)");
            }
            auto pThinningProcess = pCurvSkel->FirstChildElement("ThinningProcess");
            if(!pVoxelGrid) {
                throw std::runtime_error("No ThinningProcess tag (required to build the skeleton)");
            }

            auto resolution = 128u;
            getAttribute(*pVoxelGrid, "resolution", resolution);

            auto segmented = false;
            getAttribute(*pThinningProcess, "segmented", segmented);

            computeDiscreteData(resolution, segmented, shaderManager);
        }
    }
}

}
