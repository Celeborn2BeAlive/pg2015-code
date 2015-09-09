#pragma once

#include "TileProcessingRenderer.hpp"
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>

namespace BnZ {

class PathtraceRenderer: public TileProcessingRenderer {
public:
    PowerBasedLightSampler m_Sampler;

    uint32_t m_nMaxPathDepth = 2;

    void preprocess() override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void doExposeIO(GUI& gui) override;

    void doLoadSettings(const tinyxml2::XMLElement& xml) override;

    void doStoreSettings(tinyxml2::XMLElement& xml) const override;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void initFramebuffer() override;

    enum FramebufferTarget {
        FINAL_RENDER,
        DEPTH1
    };
};


}
