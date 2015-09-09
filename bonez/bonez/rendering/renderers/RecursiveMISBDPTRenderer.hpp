#pragma once

#include <bonez/rendering/renderers/TileProcessingRenderer.hpp>
#include <bonez/sampling/distribution1d.h>
#include <bonez/sampling/shapes.hpp>
#include <bonez/sampling/patterns.hpp>
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>

#include "recursive_mis_bdpt.hpp"
#include "DirectImportanceSampleTilePartionning.hpp"

namespace BnZ {

class RecursiveMISBDPTRenderer: public TileProcessingRenderer {
    using PathVertex = BDPTPathVertex;

    void preprocess() override;

    void beginFrame() override;

    void sampleLightPaths();

    void buildDirectImportanceSampleTilePartitionning();

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void processSample(uint32_t threadID, uint32_t tileID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void connectLightVerticesToSensor(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const;

    uint32_t getMaxLightPathDepth() const;

    uint32_t getMaxEyePathDepth() const;

    float Mis(float pdf) const {
        return balanceHeuristic(pdf);
    }

    virtual void doExposeIO(GUI& gui);

    virtual void doLoadSettings(const tinyxml2::XMLElement& xml);

    virtual void doStoreSettings(tinyxml2::XMLElement& xml) const;

    void initFramebuffer() override;

    // User parameters
    uint32_t m_nMaxDepth = 3;

    // Per frame precomputed parameters
    uint32_t m_nLightPathCount;

    // Per scene data
    PowerBasedLightSampler m_LightSampler;

    // Per frame data
    Array2d<PathVertex> m_LightPathBuffer;
    DirectImportanceSampleTilePartionning m_DirectImportanceSampleTilePartitionning;

    // Framebuffer targets
    const std::size_t FINAL_RENDER = 0u;
    const std::size_t FINAL_RENDER_DEPTH1 = 1u;
    std::size_t BPT_STRATEGY_s0_t2;
};

}
