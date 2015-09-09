#pragma once

#include <bonez/rendering/renderers/TileProcessingRenderer.hpp>
#include <bonez/sampling/multiple_importance_sampling.hpp>
#include <bonez/opengl/GLGBuffer.hpp>
#include <bonez/sampling/distribution1d.h>
#include <bonez/sampling/shapes.hpp>
#include <bonez/sampling/patterns.hpp>
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/utils/KdTree.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>
#include <array>

#include <bonez/opengl/debug/GLDebugRenderer.hpp>
#include <bonez/opengl/debug/GLDebugStream.hpp>

#include "../recursive_mis_bdpt.hpp"
#include "../DirectImportanceSampleTilePartionning.hpp"
#include "GeorgievImportanceRecordContainer.hpp"
#include "GeorgievPerDepthImportanceCache.hpp"

namespace BnZ {

// TODO: debug this: NaN on cornell box with area light and glass sphere (the glass sphere maybe ?)
class BDPTImportanceCachingRenderer: public TileProcessingRenderer {
private:
    using PathVertex = BDPTPathVertex;

    void preprocess() override;

    void beginFrame() override;

    void computeImportanceRecords();

    void computeLightVertexDistributions();

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void connectLightVerticesToCamera(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const;

    Vec3f evalContribution(const PathVertex& lightVertex, const SurfacePoint& I, const BSDF& bsdf) const;

    Vec3f evalUnoccludedContribution(const PathVertex& lightVertex, const SurfacePoint& I, const BSDF& bsdf) const;

    float evalBoundedGeometricFactor(const PathVertex& lightVertex, const SurfacePoint& I, float radiusOfInfluence, Vec3f& wi) const;

    Vec3f evalBoundedContribution(const PathVertex& lightVertex, const SurfacePoint& I, float radiusOfInfluence, const BSDF& bsdf) const;

    Vec3f evalContribution(const EmissionVertex& vertex, const SurfacePoint& I, const BSDF& bsdf) const;

    Vec3f evalUnoccludedContribution(const EmissionVertex& vertex, const SurfacePoint& I, const BSDF& bsdf) const;

    Vec3f evalBoundedContribution(const EmissionVertex& vertex, const SurfacePoint& I, float radiusOfInfluence, const BSDF& bsdf) const;

    uint32_t getMaxLightPathDepth() const {
        return m_nMaxDepth - 1;
    }

    uint32_t getMaxEyePathDepth() const {
        return m_nMaxDepth;
    }

    uint32_t getUnfilteredIRCountPerShadingPoint() const {
        return m_nUnfilteredIRCountPerShadingPointFactor * m_nIRCountPerShadingPoint;
    }

    float Mis(float pdf) const {
        return pdf; // Balance heuristic
    }

    void doExposeIO(GUI& gui) override;

    void doLoadSettings(const tinyxml2::XMLElement& xml) override;

    void doStoreSettings(tinyxml2::XMLElement& xml) const override;

    void doStoreStatistics() const override;

    void drawGLData(const ViewerData& viewerData) override;

    void initFramebuffer() override;

    std::vector<EmissionVertex> m_EmissionVertexBuffer;
    Array2d<PathVertex> m_LightPathBuffer;

    DirectImportanceSampleTilePartionning m_DirectImportanceSampleTilePartitionning;

    PowerBasedLightSampler m_LightSampler;

    uint32_t m_nMaxDepth = 3;
    uint32_t m_nLightPathCount;
    uint32_t m_nPerImportanceCacheLightPathCount = 1024;

    float m_fImportanceRecordsDensity = 0.2f;
    // Control the number of importance records
    // We compute m_fImportanceRecordsDensity * framebufferWidth * m_fImportanceCacheDensity * framebufferHeight importance caches

    float m_fMinimalIncidentAngleToVPL = 0.f; // degrees

    float getCosMaxIncidentAngle() const {
        return cos(radians(m_fMinimalIncidentAngleToVPL));
    }

    float m_fImportanceRecordOrientationTradeOff = 0.f;

    GeorgievImportanceRecordContainer m_ImportanceRecordContainer;
    std::vector<std::size_t> m_ImportanceRecordDepths;
    bool m_bUseUniformImportanceRecordSampling = false;
    uint32_t m_nUniformImportanceRecordCount = 32000;

    GeorgievPerDepthImportanceCache m_ImportanceCache;

    std::string m_sDistributionSelector = "FUBC";
    // A string that select the distributions to use "F", "U", "B", "C" are used to identify the distributions
    // For example: "FBC" use the 3 distributions F, B and C. For each shading point, a sample is used from each of this distributions and MIS is applied
    // A maximum of 4 distributions are used.

    Vec4f m_AlphaConfidenceValues = Vec4f(1.f);

    bool m_bUseAlphaMaxHeuristic = true;
    bool m_bUseDistributionWeightingOptimization = true;

    uint32_t m_nIRCountPerShadingPoint = 4u; // Number of importance records to use for each shading point
    mutable Array2d<uint32_t> m_ShadingPointIRBuffer; // A buffer to store the IRs associated to each shading point

    uint32_t m_nUnfilteredIRCountPerShadingPointFactor = 3;

    mutable Array2d<GeorgievImportanceRecordContainer::NearestUnfilteredImportanceRecord> m_ShadingPointUnfilteredIRBuffer;

    enum FramebufferTarget {
        FINAL_RENDER,
        NEAREST_IMPORTANCE_RECORD,
        DIST0_CONTRIB,
        DIST1_CONTRIB,
        DIST2_CONTRIB,
        DIST3_CONTRIB,
        FINAL_RENDER_DEPTH1
    };

    GLDebugStreamData m_GLDebugStream;
    uint32_t m_nImportanceRecordDisplayDepth = 1u;

    TaskTimer m_BeginFrameTimer;
    mutable TaskTimer m_TileProcessingTimer;
};

}
