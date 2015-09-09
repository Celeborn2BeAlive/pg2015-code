#pragma once

#include "PG15Renderer.hpp"

namespace BnZ {

class PG15BPTRenderer: public PG15Renderer {
public:
    PG15BPTRenderer(
            const PG15RendererParams& params,
            const PG15SharedData& sharedBuffers):
        PG15Renderer(params, sharedBuffers) {
        initFramebuffer();
    }

    void render() {
        processTiles([&](uint32_t threadID, uint32_t tileID, const Vec4u& viewport) {
            renderTile(threadID, tileID, viewport);
        });

        ++m_nIterationCount;
    }

    void renderTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) {
        PG15Renderer::processTilePixels(viewport, [&](uint32_t x, uint32_t y) {
            auto pixelID = getPixelIndex(x, y);

            // Add a new sample to the pixel
            for(auto i = 0u; i <= getFramebuffer().getChannelCount(); ++i) {
                accumulate(i, pixelID, Vec4f(0, 0, 0, 1));
            }

            processSample(threadID, tileID, pixelID, x, y);
        });

        // Compute contributions for 1-length eye paths (connection to sensor)
        connectLightVerticesToSensor(threadID, tileID, viewport);
    }

    void processSample(uint32_t threadID, uint32_t tileID, uint32_t pixelID,
                           uint32_t x, uint32_t y) {
        auto mis = [&](float v) {
            return Mis(v);
        };
        auto rng = getThreadRNG(threadID);

        PixelSensor pixelSensor(getSensor(), Vec2u(x, y), getFramebufferSize());

        auto pixelSample = getFloat2(threadID);
        auto sensorSample = getFloat2(threadID);

        auto fImportanceScale = 1.f;
        BDPTPathVertex eyeVertex(getScene(), pixelSensor, sensorSample, pixelSample, getLightPathCount(), mis);

        if(eyeVertex.m_Power == zero<Vec3f>() || !eyeVertex.m_fPathPdf) {
            return;
        }

        // The light path to connect with the eye path
        auto pLightPath = getLightPathPtr(pixelID);

        auto maxEyePathDepth = getMaxEyePathDepth();
        auto maxLightPathDepth = getMaxLightPathDepth();


        auto extendEyePath = [&]() -> bool {
            return eyeVertex.m_nDepth < maxEyePathDepth &&
                eyeVertex.extend(eyeVertex, getScene(),
                                 1,
                                 false, rng, mis);
        };

        // Iterate over an eye path
        do {
            // Intersection with light source
            auto totalLength = eyeVertex.m_nDepth;
            if(totalLength <= getMaxDepth()) {
                auto contrib = fImportanceScale * computeEmittedRadiance(eyeVertex, getScene(),
                                                                         getLightSampler(),
                                                                         mis);

//                if(isInvalidMeasurementEstimate(contrib)) {
//                    reportInvalidContrib(threadID, tileID, pixelID, [&]() {
//                        debugLog() << "s = " << 0 << ", t = " << (eyeVertex.m_nDepth + 1) << std::endl;
//                    });
//                }

                accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, 0u);
                accumulate(BPT_STRATEGY_s0_t2 + strategyOffset, pixelID, Vec4f(contrib, 0));
            }

            // Connections
            if(eyeVertex.m_Intersection) {
                // Direct illumination
                auto totalLength = eyeVertex.m_nDepth + 1;
                if(totalLength <= getMaxDepth()) {
                    float lightPdf;
                    auto pLight = getLightSampler().sample(getScene(), getFloat(threadID), lightPdf);
                    auto s2D = getFloat2(threadID);
                    auto contrib = fImportanceScale * connectVertices(eyeVertex, EmissionVertex(pLight, lightPdf, s2D),
                                                                      getScene(),
                                                                      mis);

//                    if(isInvalidMeasurementEstimate(contrib)) {
//                        reportInvalidContrib(threadID, tileID, pixelID, [&]() {
//                            debugLog() << "s = " << 1 << ", t = " << (eyeVertex.m_nDepth + 1) << std::endl;
//                        });
//                    }

                    accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                    accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                    auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, 1);
                    accumulate(BPT_STRATEGY_s0_t2 + strategyOffset, pixelID, Vec4f(contrib, 0));
                }

                // Connection with each light vertex
                for(auto j = 0u; j < maxLightPathDepth; ++j) {
                    auto pLightVertex = pLightPath + j;
                    auto totalLength = eyeVertex.m_nDepth + pLightVertex->m_nDepth + 1;

                    if(pLightVertex->m_fPathPdf > 0.f && totalLength <= getMaxDepth()) {
                        auto contrib = fImportanceScale * connectVertices(eyeVertex, *pLightVertex, getScene(),
                                                                          mis);

//                        if(isInvalidMeasurementEstimate(contrib)) {
//                            reportInvalidContrib(threadID, tileID, pixelID, [&]() {
//                                debugLog() << "s = " << (pLightVertex->m_nDepth + 1) << ", t = " << (eyeVertex.m_nDepth + 1) << std::endl;
//                            });
//                        }

                        accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                        accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                        auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, pLightVertex->m_nDepth + 1);
                        accumulate(BPT_STRATEGY_s0_t2 + strategyOffset, pixelID, Vec4f(contrib, 0));
                    }
                }
            }
        } while(extendEyePath());
    }

    void connectLightVerticesToSensor(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) {
        auto rcpPathCount = 1.f / getLightPathCount();

        auto mis = [&](float v) {
            return Mis(v);
        };

        for(const auto& directImportanceSample: getDirectImportanceSamples(tileID)) {
            auto pLightVertex = getLightVertexPtr(directImportanceSample.m_nLightVertexIndex);

            auto totalDepth = 1 + pLightVertex->m_nDepth;
            if(totalDepth > getMaxDepth()) {
                continue;
            }

            auto pixel = directImportanceSample.m_Pixel;
            auto pixelID = BnZ::getPixelIndex(pixel, getFramebufferSize());

            PixelSensor sensor(getSensor(), pixel, getFramebufferSize());

            auto contrib = rcpPathCount * connectVertices(*pLightVertex,
                                           SensorVertex(&sensor, directImportanceSample.m_LensSample),
                                           getScene(),
                                           mis);

//            if(isInvalidMeasurementEstimate(contrib)) {
//                reportInvalidContrib(threadID, tileID, pixelID, [&]() {
//                    debugLog() << "s = " << (pLightVertex->m_nDepth + 1) << ", t = " << 1 << std::endl;
//                });
//            }

            accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0.f));
            accumulate(FINAL_RENDER_DEPTH1 + totalDepth - 1u, pixelID, Vec4f(contrib, 0.f));

            auto strategyOffset = computeBPTStrategyOffset(totalDepth + 1, pLightVertex->m_nDepth + 1);
            accumulate(BPT_STRATEGY_s0_t2 + strategyOffset, pixelID, Vec4f(contrib, 0));
        }
    }

    void storeSettings(tinyxml2::XMLElement& xml) const {
        serialize(xml, "maxDepth", getMaxDepth());
    }

    void storeStatistics(tinyxml2::XMLElement& xml) const {
        // do nothing
    }

    void initFramebuffer() {
        BPT_STRATEGY_s0_t2 = FINAL_RENDER_DEPTH1 + getMaxDepth();

        addFramebufferChannel("final_render");
        for(auto i : range(getMaxDepth())) {
            addFramebufferChannel("depth_" + toString(i + 1));
        }
        for(auto i : range(getMaxDepth())) {
            auto depth = i + 1;
            auto vertexCount = depth + 1; // number of vertices in a path of depth i
            for(auto s : range(vertexCount + 1)) {
                addFramebufferChannel("strategy_s_" + toString(s) + "_t_" + toString(vertexCount - s));
            }
        }
    }

    template<typename MisFunctor>
    inline Vec3f computeEmittedRadiance(
            const BDPTPathVertex& eyeVertex,
            const Scene& scene,
            const PowerBasedLightSampler& lightSampler,
            MisFunctor&& mis) {
        if(eyeVertex.m_Intersection.Le == zero<Vec3f>()) {
            return zero<Vec3f>();
        }

        auto contrib = eyeVertex.m_Power * eyeVertex.m_Intersection.Le;

        // Compute MIS weight
        auto rcpWeight = 1.f;

        if(eyeVertex.m_nDepth > 1u) { // Quick fix since we dont connect emission vertex with sensor vertex
            if(!eyeVertex.m_Intersection) {
                // Hit on environment map (if present) should be handled here
                float pointPdfWrtArea, directionPdfWrtSolidAngle;
                lightSampler.computeEnvironmentLightsPdf(scene, -eyeVertex.m_BSDF.getIncidentDirection(), pointPdfWrtArea, directionPdfWrtSolidAngle);

                rcpWeight += mis(pointPdfWrtArea) * (eyeVertex.m_fdVCM + mis(directionPdfWrtSolidAngle) * eyeVertex.m_fdVC);
            } else {
                auto meshID = eyeVertex.m_Intersection.meshID;
                auto lightID = scene.getGeometry().getMesh(meshID).m_nLightID;

                const auto& pLight = scene.getLightContainer().getLight(lightID);
                const auto* pAreaLight = static_cast<const AreaLight*>(pLight.get());

                // Evaluate the pdf of sampling the ray (eyeVertex.m_Intersection, eyeVertex.m_BSDF.getIncidentDirection()) on the area light:
                float pointPdfWrtArea, directionPdfWrtSolidAngle;
                pAreaLight->pdf(eyeVertex.m_Intersection, eyeVertex.m_BSDF.getIncidentDirection(), scene,
                                pointPdfWrtArea, directionPdfWrtSolidAngle);

                pointPdfWrtArea *= lightSampler.pdf(lightID); // Scale by the pdf of chosing the area light

                rcpWeight += mis(pointPdfWrtArea) * (eyeVertex.m_fdVCM + mis(directionPdfWrtSolidAngle) * eyeVertex.m_fdVC);
            }
        }

        auto weight = 1.f / rcpWeight;

        return weight * contrib;
    }

    template<typename MisFunctor>
    inline Vec3f connectVertices(
            const BDPTPathVertex& eyeVertex,
            const EmissionVertex& lightVertex,
            const Scene& scene,
            MisFunctor&& mis) {
        if(!lightVertex.m_pLight || !lightVertex.m_fLightPdf) {
            return zero<Vec3f>();
        }

        float sampledPointPdfWrtArea, sampledPointToIncidentDirectionJacobian, revPdfWrtArea;
        RaySample shadowRay;
        auto Le = lightVertex.m_pLight->sampleDirectIllumination(scene, lightVertex.m_PositionSample, eyeVertex.m_Intersection, shadowRay,
                                                                 sampledPointToIncidentDirectionJacobian, sampledPointPdfWrtArea, revPdfWrtArea);

        if(Le == zero<Vec3f>() || shadowRay.pdf == 0.f) {
            return zero<Vec3f>();
        }

        shadowRay.pdf *= lightVertex.m_fLightPdf;
        sampledPointPdfWrtArea *= lightVertex.m_fLightPdf;

        Le /= shadowRay.pdf;

        float cosAtEyeVertex;
        float eyeDirPdf, eyeRevPdf;
        auto fr = eyeVertex.m_BSDF.eval(shadowRay.value.dir, cosAtEyeVertex, &eyeDirPdf, &eyeRevPdf);

        if(fr == zero<Vec3f>() || scene.occluded(shadowRay.value)) {
            return zero<Vec3f>();
        }

        auto contrib = eyeVertex.m_Power * Le * fr * abs(cosAtEyeVertex);

        auto rcpWeight = 1.f;

        {
            // Evaluate the pdf of sampling the light vertex by sampling the brdf of the eye vertex
           auto pdfWrtArea = eyeDirPdf * sampledPointToIncidentDirectionJacobian;
           rcpWeight += mis(pdfWrtArea / sampledPointPdfWrtArea);
        }

        rcpWeight += mis(revPdfWrtArea) * (eyeVertex.m_fdVCM + mis(eyeRevPdf) * eyeVertex.m_fdVC);

        auto weight = 1.f / rcpWeight;

        return weight * contrib;
    }

    template<typename MisFunctor>
    inline Vec3f connectVertices(
            const BDPTPathVertex& eyeVertex,
            const BDPTPathVertex& lightVertex,
            const Scene& scene,
            MisFunctor&& mis) {
        Vec3f incidentDirection;
        float dist;
        auto G = geometricFactor(eyeVertex.m_Intersection, lightVertex.m_Intersection, incidentDirection, dist);
        if(dist == 0.f) {
            return zero<Vec3f>();
        }
        Ray incidentRay(eyeVertex.m_Intersection, lightVertex.m_Intersection, incidentDirection, dist);

        float cosAtLightVertex, cosAtEyeVertex;
        float lightDirPdf, lightRevPdf;
        float eyeDirPdf, eyeRevPdf;

        auto M = lightVertex.m_BSDF.eval(-incidentDirection, cosAtLightVertex, &lightDirPdf, &lightRevPdf) *
                eyeVertex.m_BSDF.eval(incidentDirection, cosAtEyeVertex, &eyeDirPdf, &eyeRevPdf);

        if(G > 0.f && M != zero<Vec3f>() && !scene.occluded(incidentRay)) {
            auto rcpWeight = 1.f;

            {
                auto pdfWrtArea = eyeDirPdf * abs(cosAtLightVertex) / sqr(dist);
                // The division by getResamplingLightPathCount() is a trick to remove the extra factor introduced in
                // lightVertex.m_fdVCM and lightVertex.m_fdVC since light vertices are shared with techniques
                // that perform resampling.
                rcpWeight += mis(pdfWrtArea / getResamplingLightPathCount()) * (lightVertex.m_fdVCM + mis(lightRevPdf) * lightVertex.m_fdVC);
            }

            {
                auto pdfWrtArea = lightDirPdf * abs(cosAtEyeVertex) / sqr(dist);
                rcpWeight += mis(pdfWrtArea) * (eyeVertex.m_fdVCM + mis(eyeRevPdf) * eyeVertex.m_fdVC);
            }

            auto weight = 1.f / rcpWeight;

            auto contrib = eyeVertex.m_Power * lightVertex.m_Power * G * M;

            return weight * contrib;
        }

        return zero<Vec3f>();
    }

    template<typename MisFunctor>
    inline Vec3f connectVertices(
            const BDPTPathVertex& lightVertex,
            const SensorVertex& eyeVertex,
            const Scene& scene,
            MisFunctor&& mis) {
        RaySample shadowRaySample;
        float revPdfWrtArea;
        auto We = eyeVertex.m_pSensor->sampleDirectImportance(scene, eyeVertex.m_PositionSample, lightVertex.m_Intersection,
                                                shadowRaySample, nullptr, nullptr, nullptr,
                                                &revPdfWrtArea, nullptr);

        if(shadowRaySample.pdf == 0.f || We == zero<Vec3f>()) {
            return zero<Vec3f>();
        }

        float cosThetaOutDir;
        float bsdfRevPdfW;
        const auto fr = lightVertex.m_BSDF.eval(shadowRaySample.value.dir, cosThetaOutDir, nullptr, &bsdfRevPdfW);

        if(cosThetaOutDir == 0.f || fr == zero<Vec3f>() || scene.occluded(shadowRaySample.value)) {
            return zero<Vec3f>();
        }

        We /= shadowRaySample.pdf;
        // The division by getResamplingLightPathCount() is a trick to remove the extra factor introduced in
        // lightVertex.m_fdVCM and lightVertex.m_fdVC since light vertices are shared with techniques
        // that perform resampling.
        const float wLight = mis(revPdfWrtArea / (getResamplingLightPathCount() * getLightPathCount())) *
                (lightVertex.m_fdVCM + mis(bsdfRevPdfW) * lightVertex.m_fdVC);
        const float misWeight = 1.f / (wLight + 1.f);

        return misWeight * lightVertex.m_Power * fr * abs(cosThetaOutDir) * We;
    }

    // Framebuffer targets
    const std::size_t FINAL_RENDER = 0u;
    const std::size_t FINAL_RENDER_DEPTH1 = 1u;
    std::size_t BPT_STRATEGY_s0_t2;
};

}
