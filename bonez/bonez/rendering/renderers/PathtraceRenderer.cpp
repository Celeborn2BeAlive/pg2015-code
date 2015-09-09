#include "PathtraceRenderer.hpp"
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/sys/DebugLog.hpp>

#include <bonez/scene/sensors/PixelSensor.hpp>

#include "paths.hpp"

namespace BnZ {

void PathtraceRenderer::preprocess() {
    m_Sampler.initFrame(getScene());
}

void PathtraceRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const {
    for(auto i: range(getFramebufferChannelCount())) {
        accumulate(i, pixelID, Vec4f(0.f, 0.f, 0.f, 1.f));
    }

    PixelSensor pixelSensor(getSensor(), Vec2u(x, y), getFramebufferSize());

    auto pixelSample = getPixelSample(threadID, sampleID);
    auto lensSample = getFloat2(threadID);

    auto sourceVertex = BnZ::samplePrimaryEyeVertex<PathVertex>(getScene(), pixelSensor,
        lensSample, pixelSample);

    if (sourceVertex.length() == 0u && acceptPathDepth(1)) {
        auto contrib = sourceVertex.power();
        // Hit on environment light, if exists
        accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
        accumulate(DEPTH1, pixelID, Vec4f(contrib, 0));
        return;
    }

    Vec3f L = Vec3f(0.f);

    if(acceptPathDepth(1)) {
        accumulate(DEPTH1, pixelID, Vec4f(sourceVertex.intersection().Le, 0.f));
        L += sourceVertex.power() * sourceVertex.intersection().Le;
    }

    float lightPdf;
    auto pLight = m_Sampler.sample(getScene(), getFloat(threadID), lightPdf);

    ThreadRNG rng(*this, threadID);
    for(auto vertex: makePath(getScene(), sourceVertex, rng)) {
        // Next event estimation
        if(vertex.intersection() && vertex.length() < m_nMaxPathDepth && acceptPathDepth(vertex.length() + 1)) {
            RaySample shadowRay;
            auto Le = pLight->sampleDirectIllumination(getScene(), getFloat2(threadID), vertex.intersection(), shadowRay);

            if (Le != zero<Vec3f>() && shadowRay.pdf > 0.f && !getScene().occluded(shadowRay.value)) {
                shadowRay.pdf *= lightPdf;
                auto contrib = vertex.power() * vertex.bsdf().eval(shadowRay.value.dir) * abs(dot(shadowRay.value.dir, vertex.intersection().Ns))
                    * Le / shadowRay.pdf;
                accumulate(DEPTH1 + vertex.length(), pixelID, Vec4f(contrib, 0.f));
                L += contrib;
            }
        }

        // If the scattering was specular, add Le
        if((vertex.sampledEvent() & ScatteringEvent::Specular) && acceptPathDepth(vertex.length())) {
            auto contrib = vertex.power() * vertex.intersection().Le;
            accumulate(DEPTH1 + vertex.length() - 1u, pixelID, Vec4f(contrib, 0.f));
            L += contrib;
        }

        if(vertex.length() == m_nMaxPathDepth) {
            break;
        }
    }

    accumulate(FINAL_RENDER, pixelID, Vec4f(L, 0.f));
}

void PathtraceRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxPathDepth));
}

void PathtraceRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
}

void PathtraceRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
}

void PathtraceRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    processTileSamples(viewport, [this, threadID](uint32_t x, uint32_t y, uint32_t pixelID, uint32_t sampleID) {
        processSample(threadID, pixelID, sampleID, x, y);
    });
}

void PathtraceRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    for(auto i : range(m_nMaxPathDepth)) {
        addFramebufferChannel("depth_" + toString(i + 1));
    }
}

}
