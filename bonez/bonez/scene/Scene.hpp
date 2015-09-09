#pragma once

#include <vector>

#include "Ray.hpp"
#include "Intersection.hpp"
#include "SceneGeometry.hpp"
#include "raytracing/RTScene.hpp"

#include "topology/CurvilinearSkeleton.hpp"

#include "sensors/Camera.hpp"
#include "lights/LightContainer.hpp"
#include "lights/EnvironmentLight.hpp"

#include <bonez/opengl/GLScene.hpp>
#include <bonez/opengl/GLShaderManager.hpp>

#include <bonez/parsing/parsing.hpp>

#include <bonez/sampling/distribution1d.h>

namespace BnZ {

class Scene {
public:
    Scene(const tinyxml2::XMLElement* pSceneDescription,
          const FilePath& sceneDescriptionFileDir,
          const GLShaderManager& shaderManager);

    Scene(SceneGeometry geometry) :
        m_Geometry(std::move(geometry)) {
        buildAccel();
        extractAreaLights();
        buildSamplingDistribution();
    }

    ~Scene();

    const Material& getMaterial(uint32_t idx) const {
        return m_Geometry.getMaterial(idx);
    }

    const SceneGeometry& getGeometry() const {
        return m_Geometry;
    }

    Intersection postIntersect(const Ray& ray, const RTScene::Hit& hit) const;

    Intersection intersect(const Ray& ray) const;

    bool occluded(const Ray& ray) const;

    const BBox3f& getBBox() const {
        return m_Geometry.getBBox();
    }

    const Shared<const CurvilinearSkeleton>& getCurvSkeleton() const {
        return m_pCurvSkel;
    }

    void setCurvSkeleton(CurvilinearSkeleton curvSkel) {
        m_pCurvSkel = makeUnique<CurvilinearSkeleton>(std::move(curvSkel));
    }

    const Shared<const GLScene>& getGLScene() const {
        return m_pGLScene;
    }

    void setGLScene(const Shared<GLScene>& glScene) {
        m_pGLScene = glScene;
    }

    const LightContainer& getLightContainer() const {
        return m_LightContainer;
    }

    LightContainer& getLightContainer() {
        return m_LightContainer;
    }

    void addLight(const Shared<Light>& pLight) {
        m_LightContainer.addLight(pLight);
        if(std::dynamic_pointer_cast<EnvironmentLight>(pLight)) {
            m_EnvironmentLightIndices.emplace_back(m_LightContainer.size() - 1);
        }
    }

    void clearLights() {
        m_LightContainer.clear();
        m_EnvironmentLightIndices.clear();

        extractAreaLights();
    }

    void uniformSampleSurfacePoints(uint32_t count,
                                    const float* s1DMeshBuffer, // Used to sample a mesh
                                    const float* s1DTriangleBuffer, // Used to sample a triangle
                                    const float* s1DTriangleSideBuffer, // Used to sample the side of the mesh
                                    const Vec2f* s2DTriangleBuffer, // Used to sample 2D area of the chosen triangle
                                    SurfacePointSample* sampledPointsBuffer) const;

    struct SurfacePointSampleParams {
        float meshSample; // Select the mesh
        float triangleSample; // Select the triangle
        float sideSample; // Select the side of the triangle
        Vec2f positionSample; // Select the position on the triangle
    };

    template<typename SampleGeneratorFunction, typename OutputIterator>
    void sampleSurfacePointsWrtArea(uint32_t count, OutputIterator it, SampleGeneratorFunction getSample) const {
        for(auto i = 0u; i < count; ++i) {
            auto sample = getSample(i);

            auto sampledMesh = sampleDiscreteDistribution1D(m_MeshSamplingDistribution.data(),
                                                            m_Geometry.getMeshCount(),
                                                            sample.meshSample);
            if(sampledMesh.pdf > 0.f) {
                const auto& mesh = m_Geometry.getMesh(sampledMesh.value);
                auto sampledTriangle = sampleDiscreteDistribution1D(m_TriangleSamplingDistributions.data() + m_MeshDistributionOffsets[sampledMesh.value],
                                                                    mesh.getTriangleCount(),
                                                                    sample.triangleSample);
                if(sampledTriangle.pdf > 0.f) {
                    const auto& triangle = mesh.getTriangle(sampledTriangle.value);
                    auto sampledUV = uniformSampleTriangleUVs(sample.positionSample.x, sample.positionSample.y,
                                                              mesh.getVertex(triangle.v0).position,
                                                              mesh.getVertex(triangle.v1).position,
                                                              mesh.getVertex(triangle.v2).position);
                    auto area = mesh.getTriangleArea(sampledTriangle.value);

                    SurfacePoint point;
                    m_Geometry.getSurfacePoint(sampledMesh.value, sampledTriangle.value,
                                               sampledUV.x, sampledUV.y, point);
                    auto pdf = 0.5f * sampledTriangle.pdf * sampledMesh.pdf / area; // 0.5f stands for the side selection

                    if(sample.sideSample <= 0.5f) {
                        point.Ng = -point.Ng;
                        point.Ns = -point.Ns;
                    }

                    *it++ = SurfacePointSample { point, pdf };

                } else {
                    *it++ = {};
                }
            } else {
                *it++ = {};
            }
        }
    }

    float getArea() const {
        return m_fTotalArea;
    }

    const RTScene& getRTScene() const {
        return m_RTScene;
    }

    // Return radiance emitted by environment lights along an incident direction
    Vec3f Le(const Vec3f& wi) const;

    const std::vector<std::size_t>& getEnvironmentLightIndices() const {
        return  m_EnvironmentLightIndices;
    }

private:
    void buildAccel();

    void extractAreaLights();

    void buildSamplingDistribution();

    void computeDiscreteData(uint32_t resolution,
                             bool segmented,
                             const GLShaderManager& shaderManager);

    SceneGeometry m_Geometry;

    RTScene m_RTScene;
    LightContainer m_LightContainer;
    std::vector<std::size_t> m_EnvironmentLightIndices;

    Shared<const CurvilinearSkeleton> m_pCurvSkel;
    Shared<const GLScene> m_pGLScene;

    std::vector<float> m_MeshSamplingDistribution;
    std::vector<uint32_t> m_MeshDistributionOffsets;
    std::vector<float> m_TriangleSamplingDistributions;

    float m_fTotalArea = 0.f;
};

inline DiscreteDistribution computePowerBasedDistribution(const Scene& scene) {
    const auto& lightContainer = scene.getLightContainer();
    std::vector<float> powers;
    powers.reserve(lightContainer.size());
    for(const auto& pLight: lightContainer) {
        powers.emplace_back(luminance(pLight->getPowerUpperBound(scene)));
    }
    return DiscreteDistribution(powers.size(), powers.data());
}

inline int getMaterialID(const Scene& scene, const SurfacePoint& point) {
    return scene.getGeometry().getMesh(point.meshID).m_MaterialID;
}

inline const Material& getMaterial(const Scene& scene, const SurfacePoint& point) {
    return scene.getGeometry().getMaterial(scene.getGeometry().getMesh(point.meshID));
}

inline float computeSceneDiag(const Scene& scene) {
    return length(scene.getBBox().size());
}

class DefaultSurfacePointSampleGenerator {
public:
    Scene::SurfacePointSampleParams operator ()(size_t index) {
        return {
            m_Rng.getFloat(),
            m_Rng.getFloat(),
            m_Rng.getFloat(),
            m_Rng.getFloat2()
        };
    }

private:
    RandomGenerator m_Rng;
};

}
