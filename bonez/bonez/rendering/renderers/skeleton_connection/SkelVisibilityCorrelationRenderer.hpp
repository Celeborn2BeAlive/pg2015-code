#pragma once

#include <bonez/rendering/renderers/TileProcessingRenderer.hpp>
#include "SkeletonVisibilityDistributions.hpp"

namespace BnZ {

class SkelVisibilityCorrelationRenderer: public TileProcessingRenderer {
private:
    void preprocess() override;

    void beginFrame() override;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void doExposeIO(GUI& gui) override;

    void doLoadSettings(const tinyxml2::XMLElement& xml) override;

    void doStoreSettings(tinyxml2::XMLElement& xml) const override;

    void initFramebuffer() override;

    uint32_t m_nSurfacePointSampleCount = 128;
    bool m_bUseNodeDistanceWeight = true;

    std::vector<SurfacePointSample> m_SurfacePointSamples;
    SkeletonVisibilityDistributions m_SkeletonVisibilityDistributions;

    const CurvilinearSkeleton* m_pSkeleton = nullptr;
    std::vector<GraphNodeIndex> m_FilteredNodes;

    enum FramebufferTarget {
        FINAL_RENDER,
        MAPPED_NODE
    };
};

}
