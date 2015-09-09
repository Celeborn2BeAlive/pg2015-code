#pragma once

#include <array>

#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>

#include "GeorgievImportanceCache.hpp"
#include "GeorgievImportanceRecordContainer.hpp"

#include "../TileProcessingRenderer.hpp"
#include "../paths.hpp"

namespace BnZ {

class IGIImportanceCachingRenderer: public TileProcessingRenderer {
private:
    struct EmissionVPL {
        const Light* pLight = nullptr;
        float lightPdf;
        Vec2f positionSample;

        EmissionVPL() = default;

        EmissionVPL(const Light* pLight, float lightPdf, const Vec2f& positionSample):
            pLight(pLight), lightPdf(lightPdf), positionSample(positionSample) {
        }
    };

    struct SurfaceVPL {
        Intersection lastVertex;
        BSDF lastVertexBSDF;
        float pdf; // pdf of the complete path
        float pdfLastVertex; // pdf of the last vertex, conditional to the previous
        Vec3f power;
        uint32_t depth; // number of edges of the path

        void init(const BidirPathVertex& vertex) {
            lastVertex = vertex.intersection();
            lastVertexBSDF = vertex.bsdf();
            pdf = vertex.pathPdf();
            pdfLastVertex = vertex.pdfWrtArea();
            power = vertex.power();
            depth = vertex.length();
        }
    };

    void preprocess() override;

    void beginFrame() override;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void doExposeIO(GUI& gui) override;

    void doLoadSettings(const tinyxml2::XMLElement& xml) override;

    void doStoreSettings(tinyxml2::XMLElement& xml) const override;

    uint32_t getMaxLightPathDepth() const;

     void initFramebuffer() override;

    float getCosMaxIncidentAngle() const {
        return cos(radians(m_fMinimalIncidentAngleToVPL));
    }

    uint32_t getUnfilteredIRCountPerShadingPoint() const {
        return m_nUnfilteredIRCountPerShadingPointFactor * m_nIRCountPerShadingPoint;
    }

    Vec3f evalVPLContribution(const SurfaceVPL& vpl, const SurfacePoint& I, const BSDF& bsdf) const;

    Vec3f evalUnoccludedVPLContribution(const SurfaceVPL& vpl, const SurfacePoint& I, const BSDF& bsdf) const;

    float evalBoundedGeometricFactor(const SurfaceVPL& vpl, const SurfacePoint& I, float radiusOfInfluence, Vec3f& wi, float& dist) const;

    Vec3f evalBoundedVPLContribution(const SurfaceVPL& vpl, const SurfacePoint& I, float radiusOfInfluence, const BSDF& bsdf) const;

    Vec3f evalVPLContribution(const EmissionVPL& vpl, const SurfacePoint& I, const BSDF& bsdf) const;

    Vec3f evalUnoccludedVPLContribution(const EmissionVPL& vpl, const SurfacePoint& I, const BSDF& bsdf) const;

    Vec3f evalBoundedVPLContribution(const EmissionVPL& vpl, const SurfacePoint& I, float radiusOfInfluence, const BSDF& bsdf) const;

    void extractEnabledDistributionsFromSelector();

    void computeImportanceRecords();

    PowerBasedLightSampler m_LightSampler;

    // User parameters
    uint32_t m_nMaxPathDepth = 2u;
    uint32_t m_nPathCount = 1u;
    std::string m_sDistributionSelector = "FUBC";
//     A string that select the distributions to use "F", "U", "B", "C" are used to identify the distributions
//     For example: "FBC" use the 3 distributions F, B and C. For each shading point, a sample is used from each of this distributions and MIS is applied
//     A maximum of 4 distributions are used.
    Vec4f m_AlphaConfidenceValues = Vec4f(1.f);
//     The confidence value associated to each enabled distribution.
//     This is used for alpha-max heuristic, as described in the paper.
    bool m_bUseAlphaMaxHeuristic = true;
    bool m_bUseDistributionWeightingOptimization = true;
    float m_fImportanceRecordsDensity = 0.2f;
//     Control the number of importance records
//     We compute m_fImportanceRecordsDensity * framebufferWidth * m_fImportanceCacheDensity * framebufferHeight importance caches
    uint32_t m_nIRCountPerShadingPoint = 4u; // Number of importance records to use for each shading point
    uint32_t m_nUnfilteredIRCountPerShadingPointFactor = 3;

    // Precomputed parameters
    float m_fMinimalIncidentAngleToVPL = 90.f; // degrees
    uint32_t m_nVPLCount = 0u; // Maximal number of VPLs
    float m_fPowerScale = 0.f; // 1.f / m_nPathCount after beginFrame()
    float m_fImportanceRecordOrientationTradeOff = 0.f;

    // Data stored in memory per frame
    std::vector<EmissionVPL> m_EmissionVPLBuffer;
    std::vector<SurfaceVPL> m_SurfaceVPLBuffer;

    GeorgievImportanceRecordContainer m_ImportanceRecordContainer;
    GeorgievImportanceCache m_ImportanceCache;

    // Temporary buffers for each thread
    mutable Array2d<uint32_t> m_ShadingPointIRBuffer; // A buffer to store the IRs associated to each shading point
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
};

}
