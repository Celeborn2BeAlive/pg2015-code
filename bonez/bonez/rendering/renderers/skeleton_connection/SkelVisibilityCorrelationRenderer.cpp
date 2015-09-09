#include "SkelVisibilityCorrelationRenderer.hpp"
#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void SkelVisibilityCorrelationRenderer::preprocess() {
    m_pSkeleton = getScene().getCurvSkeleton().get();

    if(!m_pSkeleton) {
        throw std::runtime_error("SkelVisibilityCorrelationRenderer: No skeleton attached to the scene.");
    }

    auto nodeRange = range(m_pSkeleton->size());
    m_FilteredNodes.clear();
    std::copy(begin(nodeRange), end(nodeRange), std::back_inserter(m_FilteredNodes));
}

void SkelVisibilityCorrelationRenderer::beginFrame() {
    m_SurfacePointSamples.resize(m_nSurfacePointSampleCount);

    auto sampleGenerator = [&](uint32_t i) -> Scene::SurfacePointSampleParams {
        return {
            getFloat(0u), // meshSample
            getFloat(0u), // triangleSample
            getFloat(0u), // sideSample
            getFloat2(0u) // positionSample
        };
    };

    getScene().sampleSurfacePointsWrtArea(m_nSurfacePointSampleCount,
                                         begin(m_SurfacePointSamples),
                                         sampleGenerator);

    buildDistributions(m_SkeletonVisibilityDistributions,
                       getScene(),
                       m_FilteredNodes,
                       m_SurfacePointSamples.data(),
                       m_SurfacePointSamples.size(),
                       m_bUseNodeDistanceWeight,
                       getThreadCount());
}

void SkelVisibilityCorrelationRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    auto spp = getSppCount();

    TileProcessingRenderer::processTilePixels(viewport, [&](uint32_t x, uint32_t y) {
        auto pixelID = getPixelIndex(x, y);

        for(auto i = 0u; i < getFramebufferChannelCount(); ++i) {
            accumulate(i, pixelID, Vec4f(0, 0, 0, 1));
        }

        for(auto sampleID = 0u; sampleID < spp; ++sampleID) {
            processSample(threadID, pixelID, sampleID, x, y);
        }
    });
}

void SkelVisibilityCorrelationRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const {
    PixelSensor sensor(getSensor(), Vec2u(x, y), getFramebufferSize());

    auto pixelSample = getPixelSample(threadID, sampleID);
    auto lensSample = getFloat2(threadID);

    RaySample raySample;
    Intersection I;

    sampleExitantRay(
                sensor,
                getScene(),
                lensSample,
                pixelSample,
                raySample,
                I);

    if(I) {
        auto nodeIdx = m_pSkeleton->getNearestNode(I, -raySample.value.dir);
        if(nodeIdx != UNDEFINED_NODE) {
            accumulate(MAPPED_NODE, pixelID, Vec4f(getColor(nodeIdx), 0.f));

//            auto node = m_pSkeleton->getNode(nodeIdx);

//            float visibilityCorrelation = 0.f;
//            std::size_t count = 0u;
//            for(const auto& pointSample: m_SurfacePointSamples) {
//                auto fromPointShadowRay = Ray(I, pointSample.value);
//                float isVisibleFromPoint = !getScene().occluded(fromPointShadowRay);

//                auto dir = pointSample.value.P - node.P;
//                auto l = length(dir);
//                dir /= l;

//                auto fromNodeShadowRay = Ray(node.P, pointSample.value, dir, l);

//                float isVisibleFromNode = !getScene().occluded(fromNodeShadowRay);

//                visibilityCorrelation += 1.f - sqr(isVisibleFromNode - isVisibleFromPoint);
//                ++count;
//            }
//            if(count) {
//                visibilityCorrelation /= count;
//            }

            auto pointReSample = m_SkeletonVisibilityDistributions.sample(0u, nodeIdx, 0u, getFloat(threadID));
            if(pointReSample.pdf) {
                const auto& pointSample = m_SurfacePointSamples[pointReSample.value];
                auto fromPointShadowRay = Ray(I, pointSample.value);
                float isVisibleFromPoint = !getScene().occluded(fromPointShadowRay);

                auto correlation = 1.f - sqr(1.f - isVisibleFromPoint);

                accumulate(FINAL_RENDER, pixelID, Vec4f(Vec3f(correlation), 0.f));
            }
        }
    }
}

void SkelVisibilityCorrelationRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nSurfacePointSampleCount));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseNodeDistanceWeight));
}

void SkelVisibilityCorrelationRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "surfacePointSampleCount", m_nSurfacePointSampleCount);
    serialize(xml, "useNodeDistanceWeight", m_bUseNodeDistanceWeight);
}

void SkelVisibilityCorrelationRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "surfacePointSampleCount", m_nSurfacePointSampleCount);
    serialize(xml, "useNodeDistanceWeight", m_bUseNodeDistanceWeight);
}

void SkelVisibilityCorrelationRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    addFramebufferChannel("mapped_node");
}

}
