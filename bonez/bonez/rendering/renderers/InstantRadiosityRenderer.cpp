#include "InstantRadiosityRenderer.hpp"

#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void InstantRadiosityRenderer::preprocess() {
    m_Sampler.initFrame(getScene());
}

void InstantRadiosityRenderer::beginFrame() {
    auto lightPathDepth = m_nMaxDepth - 2;

    m_EmissionVPLBuffer.clear();
    m_EmissionVPLBuffer.reserve(m_nLightPathCount);

    m_SurfaceVPLBuffer.clear();
    m_SurfaceVPLBuffer.reserve(m_nLightPathCount * lightPathDepth);

    for(auto i: range(m_nLightPathCount)) {
        auto power = Vec3f(1.f / m_nLightPathCount);
        EmissionVPL vpl;
        vpl.pLight = m_Sampler.sample(getScene(), getFloat(0u), vpl.lightPdf);
        if(vpl.pLight && vpl.lightPdf) {
            float emissionVertexPdf;
            RaySample exitantRaySample;
            vpl.emissionVertexSample = getFloat2(0u);
            auto Le = vpl.pLight->sampleExitantRay(getScene(), vpl.emissionVertexSample, getFloat2(0u), exitantRaySample, emissionVertexPdf);

            m_EmissionVPLBuffer.emplace_back(vpl);

            if(Le != zero<Vec3f>() && exitantRaySample.pdf) {
                auto intersection = getScene().intersect(exitantRaySample.value);
                if(intersection) {
                    power *= Le / (vpl.lightPdf * exitantRaySample.pdf);
                    m_SurfaceVPLBuffer.emplace_back(intersection, -exitantRaySample.value.dir, getScene(), power);

                    for(auto depth = 1u; depth < lightPathDepth; ++depth) {
                        Sample3f outgoingDir;
                        float cosThetaOutDir;
                        auto fs = m_SurfaceVPLBuffer.back().bsdf.sample(getFloat3(0u), outgoingDir, cosThetaOutDir,
                                                                 nullptr, true);

                        if(fs != zero<Vec3f>() && outgoingDir.pdf) {
                            Ray ray(m_SurfaceVPLBuffer.back().intersection, outgoingDir.value);
                            auto intersection = getScene().intersect(ray);
                            if(intersection) {
                                power *= fs * abs(cosThetaOutDir) / outgoingDir.pdf;
                                m_SurfaceVPLBuffer.emplace_back(intersection, -ray.dir, getScene(), power);
                            }
                        }
                    }
                }
            }
        }
    }
}

void InstantRadiosityRenderer::processPixel(uint32_t threadID, uint32_t tileID, const Vec2u& pixel) const {
    PixelSensor pixelSensor(getSensor(), pixel, getFramebufferSize());
    RaySample raySample;
    float positionPdf, directionPdf;
    auto We = pixelSensor.sampleExitantRay(getScene(), getFloat2(threadID), getFloat2(threadID), raySample, positionPdf, directionPdf);

    auto L = zero<Vec3f>();

    if(We != zero<Vec3f>() && raySample.pdf) {
        auto I = getScene().intersect(raySample.value);
        BSDF bsdf(-raySample.value.dir, I, getScene());
        if(I) {
            // Direct illumination
            for(auto& vpl: m_EmissionVPLBuffer) {
                RaySample shadowRay;
                auto Le = vpl.pLight->sampleDirectIllumination(getScene(), vpl.emissionVertexSample, I, shadowRay);

                if(shadowRay.pdf) {
                    auto contrib = We * Le * bsdf.eval(shadowRay.value.dir) * abs(dot(I.Ns, shadowRay.value.dir)) /
                            (vpl.lightPdf * shadowRay.pdf * m_nLightPathCount * raySample.pdf);

                    if(contrib != zero<Vec3f>()) {
                        if(!getScene().occluded(shadowRay.value)) {
                            L += contrib;
                        }
                    }
                }
            }

            // Indirect illumination
            for(auto& vpl: m_SurfaceVPLBuffer) {
                auto dirToVPL = vpl.intersection.P - I.P;
                auto dist = length(dirToVPL);

                if(dist > 0.f) {
                    dirToVPL /= dist;

                    auto fs1 = bsdf.eval(dirToVPL);
                    auto fs2 = vpl.bsdf.eval(-dirToVPL);
                    auto geometricFactor = abs(dot(I.Ns, dirToVPL)) * abs(dot(vpl.intersection.Ns, -dirToVPL)) / sqr(dist);

                    auto contrib = We * vpl.power * fs1 * fs2 * geometricFactor / raySample.pdf;
                    if(contrib != zero<Vec3f>()) {
                        Ray shadowRay(I, vpl.intersection, dirToVPL, dist);
                        if(!getScene().occluded(shadowRay)) {
                            L += contrib;
                        }
                    }
                }
            }
        }
    }

    accumulate(0, getPixelIndex(pixel.x, pixel.y), Vec4f(L, 1.f));
}

void InstantRadiosityRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    TileProcessingRenderer::processTilePixels(viewport, [&](auto x, auto y) {
        this->processPixel(threadID, tileID, Vec2u(x, y));
    });
}

void InstantRadiosityRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_nLightPathCount));
}

void InstantRadiosityRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "lightPathCount", m_nLightPathCount);
}

void InstantRadiosityRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "lightPathCount", m_nLightPathCount);
}

void InstantRadiosityRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");

}

}
