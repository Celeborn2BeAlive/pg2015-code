#pragma once

#include <bonez/rendering/renderers/TileProcessingRenderer.hpp>
#include <bonez/sampling/distribution1d.h>
#include <bonez/sampling/shapes.hpp>
#include <bonez/sampling/patterns.hpp>
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>

#include <bonez/opengl/debug/GLDebugRenderer.hpp>

#include "../paths.hpp"
#include "../recursive_mis_bdpt.hpp"
#include "../DirectImportanceSampleTilePartionning.hpp"

#include "SkeletonVisibilityDistributions.hpp"

namespace BnZ {

class BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer: public TileProcessingRenderer {
private:
    using PathVertex = BDPTPathVertex;

    void preprocess() override;

    void beginFrame() override;

    void sampleLightPaths();

    void buildDirectImportanceSampleTilePartitionning();

    void computeFilteredNodes();

    void updateEyeVertexCountPerNode();

    void computeNodeMappingStatistics(uint64_t& sum, double& mean, double& variance) const;

    void computeFilteredNodesList(double mappedVertexCountMean,
                                  std::vector<GraphNodeIndex> &filteredNodes,
                                  std::vector<std::size_t>& filteredNodeIndex) const;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void connectLightVerticesToCamera(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const;

    Vec3f evalLightVertexContrib(const PathVertex& eyeVertex, std::size_t lightPathDepth, const Sample1u& lightVertexSample,
                                 float resamplingMISWeight) const;

    size_t skeletonMapping(const PathVertex& eyeVertex, GraphNodeIndex* nodeBuffer) const;

    uint32_t getMaxLightPathDepth() const {
        return m_nMaxDepth - 1;
    }

    uint32_t getMaxEyePathDepth() const {
        return m_nMaxDepth;
    }

    float Mis(float pdf) const {
        return balanceHeuristic(pdf);
    }

    void doExposeIO(GUI& gui) override;

    void doLoadSettings(const tinyxml2::XMLElement& xml) override;

    void doStoreSettings(tinyxml2::XMLElement& xml) const override;

    void doStoreStatistics() const override;

    void initFramebuffer() override;

    mutable Array2d<GraphNodeIndex> m_PerThreadNodeBuffer;
    mutable Array2d<GraphNodeIndex> m_PerThreadFilteredNodeBuffer;

    SkeletonVisibilityDistributions m_SkeletonVisibilityDistributions;
    std::vector<GraphNodeIndex> m_FilteredNodes;
    std::vector<uint64_t> m_EyeVertexCountPerNode;
    std::vector<std::size_t> m_FilteredNodeIndex;
    uint64_t m_nTotalFilteredNodeCount;

    std::vector<EmissionVertex> m_EmissionVertexBuffer;
    Array2d<PathVertex> m_LightPathBuffer;

    DirectImportanceSampleTilePartionning m_DirectImportanceSampleTilePartitionning;

    PowerBasedLightSampler m_LightSampler;

    Shared<const CurvilinearSkeleton> m_pSkel;

    uint32_t m_nMaxDepth = 3;

    uint32_t m_nLightPathCount;

    uint32_t m_nPerNodeLightPathCount = 1024;
    uint32_t m_nMaxNodeCount = 4;
    bool m_bUseCombinedResampling = false;
    bool m_bUseNodeRadianceWeight = true;
    bool m_bUseNodeDistanceWeight = true;
    float m_fNodeFilteringFactor = 0.01f;


    float m_fImportanceScale = 0.f; // 1 / (number of eye path per pixel)
    float m_fRadianceScale = 0.f; // 1 / (number of light path per pixel)

    enum FramebufferTarget {
        FINAL_RENDER,
        SELECTED_NODE0,
        SELECTED_NODE1,
        SELECTED_NODE2,
        SELECTED_NODE3,
        DISTRIB0_CONTRIBUTION,
        DISTRIB1_CONTRIBUTION,
        DISTRIB2_CONTRIBUTION,
        DISTRIB3_CONTRIBUTION,
        DEFAULT_DISTRIB_CONTRIBUTION,
        FINAL_RENDER_DEPTH1
    };

    TaskTimer m_BeginFrameTimer;
    mutable TaskTimer m_TileProcessingTimer;

    mutable Array2d<uint64_t> m_EyeVertexCountPerNodePerThread;
};

}
