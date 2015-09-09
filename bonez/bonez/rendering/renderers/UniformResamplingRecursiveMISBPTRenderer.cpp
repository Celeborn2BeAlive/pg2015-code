#include "UniformResamplingRecursiveMISBPTRenderer.hpp"
#include <bonez/scene/lights/AreaLight.hpp>

#include <bonez/scene/sensors/PixelSensor.hpp>

#include "paths.hpp"

#include <bonez/sys/DebugLog.hpp>

namespace BnZ {

void UniformResamplingRecursiveMISBPTRenderer::preprocess() {
    BPT_STRATEGY_s0_t2 = FINAL_RENDER_DEPTH1 + m_nMaxDepth;
    m_LightSampler.initFrame(getScene());
}

void UniformResamplingRecursiveMISBPTRenderer::beginFrame() {
    sampleLightPaths();
    buildDirectImportanceSampleTilePartitionning();
}

void UniformResamplingRecursiveMISBPTRenderer::sampleLightPaths() {
    m_LightPathBuffer.resize(getMaxLightPathDepth(), m_nLightPathCount);

    auto mis = [&](float v) {
        return Mis(v);
    };

    processTasksDeterminist(m_nLightPathCount, [&](uint32_t pathID, uint32_t threadID) {
        ThreadRNG rng(*this, threadID);
        auto pLightPath = m_LightPathBuffer.getSlicePtr(pathID);
        sampleLightPath(pLightPath, getMaxLightPathDepth(), getScene(),
                        m_LightSampler, getSppCount(), mis, rng);
    }, getThreadCount());
}

void UniformResamplingRecursiveMISBPTRenderer::buildDirectImportanceSampleTilePartitionning() {
    ThreadRNG rng(*this, 0u);
    m_DirectImportanceSampleTilePartitionning.build(
                m_LightPathBuffer.size(),
                getScene(),
                getSensor(),
                getFramebufferSize(),
                getTileSize(),
                getTileCount2(),
                [&](std::size_t i) {
                    return m_LightPathBuffer[i].m_Intersection;
                },
                [&](std::size_t i) {
                    return m_LightPathBuffer[i].m_Intersection && m_LightPathBuffer[i].m_fPathPdf > 0.f;
                },
                rng);
}

void UniformResamplingRecursiveMISBPTRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    auto spp = getSppCount();

    TileProcessingRenderer::processTilePixels(viewport, [&](uint32_t x, uint32_t y) {
        auto pixelID = getPixelIndex(x, y);

        // Add a new sample to the pixel
        for(auto i = 0u; i <= getFramebuffer().getChannelCount(); ++i) {
            accumulate(i, pixelID, Vec4f(0, 0, 0, 1));
        }

        // Process each sample
        for(auto sampleID = 0u; sampleID < spp; ++sampleID) {
            processSample(threadID, tileID, pixelID, sampleID, x, y);
        }
    });

    // Compute contributions for 1-length eye paths (connection to sensor)
    connectLightVerticesToSensor(threadID, tileID, viewport);
}

void UniformResamplingRecursiveMISBPTRenderer::processSample(uint32_t threadID, uint32_t tileID, uint32_t pixelID, uint32_t sampleID,
                       uint32_t x, uint32_t y) const {
    auto mis = [&](float v) {
        return Mis(v);
    };
    ThreadRNG rng(*this, threadID);

    PixelSensor pixelSensor(getSensor(), Vec2u(x, y), getFramebufferSize());

    auto pixelSample = getPixelSample(threadID, sampleID);
    auto sensorSample = getFloat2(threadID);

    auto fImportanceScale = 1.f / getSppCount();
    BDPTPathVertex eyeVertex(getScene(), pixelSensor, sensorSample, pixelSample, m_nLightPathCount, mis);

    if(eyeVertex.m_Power == zero<Vec3f>() || !eyeVertex.m_fPathPdf) {
        return;
    }

    // Sample the light path to connect with the eye path
    auto lightPathIdx = clamp(std::size_t(getFloat(threadID) * m_nLightPathCount),
                              std::size_t(0),
                              m_nLightPathCount - 1);
    auto pLightPath = m_LightPathBuffer.getSlicePtr(lightPathIdx);

    auto maxEyePathDepth = getMaxEyePathDepth();
    auto maxLightPathDepth = getMaxLightPathDepth();


    auto extendEyePath = [&]() -> bool {
        return eyeVertex.m_nDepth < maxEyePathDepth &&
            eyeVertex.extend(eyeVertex, getScene(), getSppCount(), false, rng, mis);
    };

    // Iterate over an eye path
    do {
        // Intersection with light source
        auto totalLength = eyeVertex.m_nDepth;
        if(acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
            auto contrib = fImportanceScale * computeEmittedRadiance(eyeVertex, getScene(), m_LightSampler, getSppCount(), mis);

            if(isInvalidMeasurementEstimate(contrib)) {
                reportInvalidContrib(threadID, tileID, pixelID, [&]() {
                    debugLog() << "s = " << 0 << ", t = " << (eyeVertex.m_nDepth + 1) << std::endl;
                });
            }

            accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
            accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

            auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, 0u);
            accumulate(BPT_STRATEGY_s0_t2 + strategyOffset, pixelID, Vec4f(contrib, 0));
        }

        // Connections
        if(eyeVertex.m_Intersection) {
            // Direct illumination
            auto totalLength = eyeVertex.m_nDepth + 1;
            if(acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
                float lightPdf;
                auto pLight = m_LightSampler.sample(getScene(), getFloat(threadID), lightPdf);
                auto s2D = getFloat2(threadID);
                auto contrib = fImportanceScale * connectVertices(eyeVertex, EmissionVertex(pLight, lightPdf, s2D), getScene(), getSppCount(), mis);

                if(isInvalidMeasurementEstimate(contrib)) {
                    reportInvalidContrib(threadID, tileID, pixelID, [&]() {
                        debugLog() << "s = " << 1 << ", t = " << (eyeVertex.m_nDepth + 1) << std::endl;
                    });
                }

                accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, 1);
                accumulate(BPT_STRATEGY_s0_t2 + strategyOffset, pixelID, Vec4f(contrib, 0));
            }

            // Connection with each light vertex
            for(auto j = 0u; j < maxLightPathDepth; ++j) {
                auto pLightVertex = pLightPath + j;
                auto totalLength = eyeVertex.m_nDepth + pLightVertex->m_nDepth + 1;

                if(pLightVertex->m_fPathPdf > 0.f && acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
                    auto contrib = fImportanceScale * connectVertices(eyeVertex, *pLightVertex, getScene(), getSppCount(), mis);

                    if(isInvalidMeasurementEstimate(contrib)) {
                        reportInvalidContrib(threadID, tileID, pixelID, [&]() {
                            debugLog() << "s = " << (pLightVertex->m_nDepth + 1) << ", t = " << (eyeVertex.m_nDepth + 1) << std::endl;
                        });
                    }

                    accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                    accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                    auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, pLightVertex->m_nDepth + 1);
                    accumulate(BPT_STRATEGY_s0_t2 + strategyOffset, pixelID, Vec4f(contrib, 0));
                }
            }
        }
    } while(extendEyePath());
}

void UniformResamplingRecursiveMISBPTRenderer::connectLightVerticesToSensor(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    auto rcpPathCount = 1.f / m_nLightPathCount;

    auto mis = [&](float v) {
        return Mis(v);
    };

    for(const auto& directImportanceSample: m_DirectImportanceSampleTilePartitionning[tileID]) {
        auto pLightVertex = &m_LightPathBuffer[directImportanceSample.m_nLightVertexIndex];

        auto totalDepth = 1 + pLightVertex->m_nDepth;
        if(!acceptPathDepth(totalDepth) || totalDepth > m_nMaxDepth) {
            continue;
        }

        auto pixel = directImportanceSample.m_Pixel;
        auto pixelID = BnZ::getPixelIndex(pixel, getFramebufferSize());

        PixelSensor sensor(getSensor(), pixel, getFramebufferSize());

        auto contrib = rcpPathCount * connectVertices(*pLightVertex,
                                       SensorVertex(&sensor, directImportanceSample.m_LensSample),
                                       getScene(), m_nLightPathCount, mis);

        if(isInvalidMeasurementEstimate(contrib)) {
            reportInvalidContrib(threadID, tileID, pixelID, [&]() {
                debugLog() << "s = " << (pLightVertex->m_nDepth + 1) << ", t = " << 1 << std::endl;
            });
        }

        accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0.f));
        accumulate(FINAL_RENDER_DEPTH1 + totalDepth - 1u, pixelID, Vec4f(contrib, 0.f));

        auto strategyOffset = computeBPTStrategyOffset(totalDepth + 1, pLightVertex->m_nDepth + 1);
        accumulate(BPT_STRATEGY_s0_t2 + strategyOffset, pixelID, Vec4f(contrib, 0));
    }
}

uint32_t UniformResamplingRecursiveMISBPTRenderer::getMaxLightPathDepth() const {
    return m_nMaxDepth - 1; // We don't allow intersection with sensor so the light paths have one vertex less
}

uint32_t UniformResamplingRecursiveMISBPTRenderer::getMaxEyePathDepth() const {
    return m_nMaxDepth;
}

void UniformResamplingRecursiveMISBPTRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_nLightPathCount));
}

void UniformResamplingRecursiveMISBPTRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "lightPathCount", m_nLightPathCount);
}

void UniformResamplingRecursiveMISBPTRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "lightPathCount", m_nLightPathCount);
}

void UniformResamplingRecursiveMISBPTRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    for(auto i : range(m_nMaxDepth)) {
        addFramebufferChannel("depth_" + toString(i + 1));
    }
    for(auto i : range(m_nMaxDepth)) {
        auto depth = i + 1;
        auto vertexCount = depth + 1; // number of vertices in a path of depth i
        for(auto s : range(vertexCount + 1)) {
            addFramebufferChannel("strategy_s_" + toString(s) + "_t_" + toString(vertexCount - s));
        }
    }
}

}
