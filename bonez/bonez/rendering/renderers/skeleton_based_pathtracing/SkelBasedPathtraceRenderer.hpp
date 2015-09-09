#pragma once

#include "../TileProcessingRenderer.hpp"
#include "../paths.hpp"

#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/utils/DijkstraAlgorithm.hpp>

#include <bonez/opengl/debug/GLDebugStream.hpp>

namespace BnZ {

class SkelBasedPathtraceRenderer: public TileProcessingRenderer {
public:
    PowerBasedLightSampler m_Sampler;

    uint32_t m_nMaxPathDepth = 2;
    float m_fSkeletonStrength = 1.f;

    void preprocess() override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void doExposeIO(GUI& gui) override;

    void doLoadSettings(const tinyxml2::XMLElement& xml) override;

    void doStoreSettings(tinyxml2::XMLElement& xml) const override;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void initFramebuffer() override;

    void drawGLData(const ViewerData& viewerData) override;

    GraphNodeIndex getSkeletonNode(const PathVertex& vertex) const;

    Sample3f sampleSkeleton(ThreadRNG& rng, const PathVertex& vertex, GraphNodeIndex nodeIdx,
                            Vec3f& throughtput, uint32_t& sampledEvent) const;

    float pdfSkeleton(PathVertex& vertex, const Vec3f& outDir, GraphNodeIndex nodeIdx) const;

    Sample3f sampleBSDF(ThreadRNG& rng, const PathVertex& vertex,
                        Vec3f& throughtput, uint32_t& sampledEvent) const;

    float pdfBSDF(PathVertex& vertex, const Vec3f& outDir) const;

    enum FramebufferTarget {
        FINAL_RENDER,
        IS_DELTA,
        DEPTH1
    };

    std::vector<Vec3f> m_PointLightPower; // Power of each point light
    using ImportancePointVector = std::vector<Vec3f>;
    std::vector<ImportancePointVector> m_PointLightImportancePoints; // For each point light, the importance point of each skeleton node to go towards the light

    GLDebugStreamData m_GLDebugStream; // For drawing importance points with opengl
};


}
