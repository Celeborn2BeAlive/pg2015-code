#include "IGIRenderer.hpp"
#include "paths.hpp"

#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void IGIRenderer::preprocess() {
    m_LightSampler.initFrame(getScene());
}

uint32_t IGIRenderer::getMaxLightPathDepth() const {
    return m_nMaxPathDepth - 2;
}

void IGIRenderer::beginFrame() {
    // Sample all VPLs, allocs buffers

    if(m_nPathCount > 0u && m_nMaxPathDepth > 2u) {
        auto maxLightPathDepth = getMaxLightPathDepth();
        m_SurfaceVPLBuffer.resize(getMaxLightPathDepth() * m_nPathCount);

        float scaleCoeff = 1.f / m_nPathCount;

        for(auto i = 0u; i < m_nPathCount; ++i) {
            auto pLightPath = m_SurfaceVPLBuffer.data() + i * maxLightPathDepth;
            std::for_each(pLightPath, pLightPath + maxLightPathDepth, [&](SurfaceVPL& v) { v.pdf = 0.f; });

            ThreadRNG rng(*this, 0u);
            for(const auto& vertex: makeLightPath(getScene(), m_LightSampler, rng)) {
                pLightPath->init(vertex);
                pLightPath->power *= scaleCoeff;

                if(vertex.length() == maxLightPathDepth) {
                    break;
                }
                ++pLightPath;
            }
        }
    }
}

void IGIRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const {
    for(auto i = 0u; i <= getFramebufferChannelCount(); ++i) {
        accumulate(i, pixelID, Vec4f(0.f, 0.f, 0.f, 1.f));
    }

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

    auto ray = raySample.value;

    Vec3f L = Vec3f(0.f);
    Vec3f wo = -ray.dir;
    auto currentDepth = 1u;
    Vec3f throughput(1.f);

    if(!I) {
        L += I.Le;
        accumulate(FINAL_RENDER_DEPTH1, pixelID, Vec4f(I.Le, 0.f));
    } else {
        auto bsdf = BSDF(wo, I, getScene());

//        while(bsdf.isDelta()) {
//            // Follow specular path
//            Sample3f wi;
//            float cosThetaOutDir;
//            auto fs = bsdf.sample(Vec3f(getFloat(threadID), getFloat2(threadID)), wi, cosThetaOutDir);
//            if(fs != zero<Vec3f>() && wi.pdf > 0.f) {
//                throughput *= fs * cosThetaOutDir / wi.pdf;
//                ++currentDepth;
//                I = getScene().intersect(Ray(I, wi.value));
//                if(I) {
//                    wo = -wi.value;
//                    bsdf = BSDF(wo, I, getScene());;
//                } else {
//                    break;
//                }
//            } else {
//                break;
//            }
//        }

        if(m_nMaxPathDepth >= 1u && acceptPathDepth(1u)) {
            L += I.Le;
            accumulate(FINAL_RENDER_DEPTH1, pixelID, Vec4f(I.Le, 0.f));
        }

        // Direct illumination
        if(1 + currentDepth <= m_nMaxPathDepth && acceptPathDepth(1 + currentDepth)) {
            float lightPdf;
            auto pLight = m_LightSampler.sample(getScene(), getFloat(threadID), lightPdf);
            if(pLight && lightPdf) {
                RaySample shadowRay;
                auto Le = pLight->sampleDirectIllumination(getScene(), getFloat2(threadID), I, shadowRay);

                if(Le != zero<Vec3f>() && shadowRay.pdf) {
                    float cosThetaOutDir;
                    auto fr = bsdf.eval(shadowRay.value.dir, cosThetaOutDir);
                    auto contrib = Le * fr * abs(cosThetaOutDir) / shadowRay.pdf;
                    if(contrib != zero<Vec3f>() && !getScene().occluded(shadowRay.value)) {
                        L += contrib;
                        accumulate(FINAL_RENDER_DEPTH1 + currentDepth, pixelID, Vec4f(contrib, 0.f));
                    }
                }
            }
        }

        // VPL illumination
        for(const auto& vpl: m_SurfaceVPLBuffer) {
            if(vpl.pdf) {
                auto pathDepth = vpl.depth + 1 + currentDepth;

                if(pathDepth <= m_nMaxPathDepth && acceptPathDepth(pathDepth)) {
                    auto contrib = evalVPLContribution(vpl, I, bsdf);
                    L += contrib;
                    accumulate(FINAL_RENDER_DEPTH1 + pathDepth - 1u, pixelID, Vec4f(contrib, 0.f));
                }
            }
        }
    }

    accumulate(FINAL_RENDER, pixelID, Vec4f(L, 0.f));
}

Vec3f IGIRenderer::evalVPLContribution(const SurfaceVPL& vpl, const Intersection& I, const BSDF& bsdf) const {
    Vec3f wi;
    float dist;
    auto G = geometricFactor(I, vpl.lastVertex, wi, dist);
    if(G > 0.f) {
        float cosThetaOutVPL, costThetaOutBSDF;
        auto M = bsdf.eval(wi, costThetaOutBSDF) * vpl.lastVertexBSDF.eval(-wi, cosThetaOutVPL);
        if(M != zero<Vec3f>()) {
            Ray shadowRay(I, vpl.lastVertex, wi, dist);
            if(!getScene().occluded(shadowRay)) {
                return M * G * vpl.power;
            }
        }
    }
    return zero<Vec3f>();
}

void IGIRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxPathDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_nPathCount));
}

void IGIRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
    serialize(xml, "pathCount", m_nPathCount);
}

void IGIRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
    serialize(xml, "pathCount", m_nPathCount);
}

void IGIRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    processTileSamples(viewport, [this, threadID](uint32_t x, uint32_t y, uint32_t pixelID, uint32_t sampleID) {
        processSample(threadID, pixelID, sampleID, x, y);
    });
}

void IGIRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    for(auto i : range(m_nMaxPathDepth)) {
        addFramebufferChannel("depth_" + toString(i + 1));
    }
}

}
