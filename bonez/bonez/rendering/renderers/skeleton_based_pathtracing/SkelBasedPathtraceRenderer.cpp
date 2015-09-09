#include "SkelBasedPathtraceRenderer.hpp"
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/sys/DebugLog.hpp>

#include <bonez/scene/sensors/PixelSensor.hpp>

#include <bonez/opengl/debug/GLDebugRenderer.hpp>

namespace BnZ {

void SkelBasedPathtraceRenderer::preprocess() {
    m_Sampler.initFrame(getScene());

    const auto& skeleton = *getScene().getCurvSkeleton();
    getScene().getLightContainer().forEach<PointLight>([&](std::size_t lightIdx, const PointLight& light) {
        auto rootNodeIndex = skeleton.getNearestNode(light.m_Position);

        if(rootNodeIndex != UNDEFINED_NODE) {
            m_PointLightPower.emplace_back(light.getPowerUpperBound(getScene()));
            auto shortestPaths = DijkstraAlgorithm().computeShortestPaths(
                        skeleton.getGraph(),
                        rootNodeIndex,
                        [&](GraphNodeIndex n1, GraphNodeIndex n2) {
                            return 1;
                        });

            m_PointLightImportancePoints.emplace_back();
            for(auto nodeIdx: range(skeleton.size())) {
                auto currentNodeIdx = nodeIdx;
                auto currentNodePosition = skeleton.getNode(currentNodeIdx).P;
                auto sumPositions = currentNodePosition;
                auto countPositions = 1u;

                auto nextNodeIdx = shortestPaths[nodeIdx].predecessor;
                if(nextNodeIdx != UNDEFINED_NODE) {
                    auto nextNodePosition = skeleton.getNode(nextNodeIdx).P;

                    auto makePredecessorShadowRay = [&]() {
                        auto dir = nextNodePosition - currentNodePosition;
                        auto l = length(dir);
                        dir /= l;
                        return Ray(currentNodePosition, dir, 0, l);
                    };

                    while(nextNodeIdx != currentNodeIdx && !getScene().occluded(makePredecessorShadowRay())) {
                        sumPositions += nextNodePosition;
                        ++countPositions;

                        currentNodeIdx = nextNodeIdx;
                        nextNodeIdx = shortestPaths[currentNodeIdx].predecessor;
                        nextNodePosition = skeleton.getNode(nextNodeIdx).P;
                    }
                }

                auto barycenter = sumPositions / float(countPositions);
                m_PointLightImportancePoints.back().emplace_back(barycenter);
            }
        }
    });
}

void SkelBasedPathtraceRenderer::drawGLData(const ViewerData& viewerData) {
    m_GLDebugStream.clearObjects();
    viewerData.debugRenderer.addStream(&m_GLDebugStream);

    if(!m_PointLightImportancePoints.empty()) {
        const auto& skeleton = *getScene().getCurvSkeleton();
        for(auto i: range(skeleton.size())) {
            auto nodePosition = skeleton.getNode(i).P;
            for(auto& importancePoints: m_PointLightImportancePoints) {
                m_GLDebugStream.addLine(nodePosition, importancePoints[i], getColor(i), Vec3f(1,0,0), 5);
            }
        }
    }
}

void SkelBasedPathtraceRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const {
    for(auto i: range(getFramebufferChannelCount())) {
        accumulate(i, pixelID, Vec4f(0.f, 0.f, 0.f, 1.f));
    }

    PixelSensor pixelSensor(getSensor(), Vec2u(x, y), getFramebufferSize());

    auto pixelSample = getPixelSample(threadID, sampleID);
    auto lensSample = getFloat2(threadID);

    auto vertex = BnZ::samplePrimaryEyeVertex<PathVertex>(getScene(), pixelSensor,
        lensSample, pixelSample);

    if (vertex.length() == 0u && acceptPathDepth(1)) {
        auto contrib = vertex.power();
        // Hit on environment light, if exists
        accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
        accumulate(DEPTH1, pixelID, Vec4f(contrib, 0));
        return;
    }

    Vec3f L = Vec3f(0.f);

    if(acceptPathDepth(1)) {
        accumulate(DEPTH1, pixelID, Vec4f(vertex.intersection().Le, 0.f));
        L += vertex.power() * vertex.intersection().Le;
    }

    float lightPdf;
    auto pLight = m_Sampler.sample(getScene(), getFloat(threadID), lightPdf);

    ThreadRNG rng(*this, threadID);
    while(vertex.length() < m_nMaxPathDepth) {
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

        if(!vertex.intersection()) {
            // Cannot extend from infinity
            vertex.invalidate();
            break;
        }

        uint32_t sampledEvent;
        Sample3f woSample;
        Vec3f power;

        auto nodeIdx = getSkeletonNode(vertex);
        if(nodeIdx != UNDEFINED_NODE) {
            auto pSkel = 0.5f;
            auto pBSDF = 1.f - pSkel;
            if(rng.getFloat() < pSkel) {
                // Use skeleton
                Vec3f throughput;
                woSample = sampleSkeleton(rng, vertex, nodeIdx, throughput, sampledEvent);
                auto weight = pSkel * woSample.pdf / (pSkel * woSample.pdf + pBSDF * pdfBSDF(vertex, woSample.value)); // MIS
                power = weight * vertex.power() * throughput / (pSkel * woSample.pdf);


            } else {
                // Use BSDF
                Vec3f throughput;
                woSample = sampleBSDF(rng, vertex, throughput, sampledEvent);
                auto weight = pBSDF * woSample.pdf / (pBSDF * woSample.pdf + pSkel * pdfSkeleton(vertex, woSample.value, nodeIdx));  // MIS
                power = weight * vertex.power() * throughput / (pBSDF * woSample.pdf);
            }
        } else {
            if(vertex.length() == 1)
                accumulate(IS_DELTA, pixelID, Vec4f(1, 1, 1, 0.f));

            Vec3f throughput;
            woSample = sampleBSDF(rng, vertex, throughput, sampledEvent);
            power = vertex.power() * throughput / woSample.pdf;
        }

        if(woSample.pdf == 0.f || power == zero<Vec3f>()) {
            vertex.invalidate();
            break;
        }

        // Extend path
        auto I = getScene().intersect(Ray(vertex.intersection(), woSample.value));
        if(!I) {
            vertex = PathVertex {
                I,
                BSDF(),
                sampledEvent,
                power * I.Le,
                vertex.length() + 1u,
                false
            };
        } else {
            vertex = PathVertex(I,
                                BSDF(-woSample.value, I, getScene()),
                                sampledEvent,
                                power,
                                vertex.length() + 1u,
                                false);
        }
    }

    accumulate(FINAL_RENDER, pixelID, Vec4f(L, 0.f));
}

GraphNodeIndex SkelBasedPathtraceRenderer::getSkeletonNode(const PathVertex& vertex) const {
    if(vertex.bsdf().isDelta()) {
        return UNDEFINED_NODE;
    }

    if(m_PointLightImportancePoints.empty()) {
        return UNDEFINED_NODE;
    }

    return getScene().getCurvSkeleton()->getNearestNode(vertex.intersection(), vertex.bsdf().getIncidentDirection());
}

Sample3f SkelBasedPathtraceRenderer::sampleSkeleton(ThreadRNG& rng, const PathVertex& vertex, GraphNodeIndex nodeIdx,
                        Vec3f& throughtput, uint32_t& sampledEvent) const {
    auto importancePointSetIdx =
            clamp(size_t(rng.getFloat() * m_PointLightImportancePoints.size()),
                  size_t(0),
                  m_PointLightImportancePoints.size() - 1);
    auto importancePoint = m_PointLightImportancePoints[importancePointSetIdx][nodeIdx];
    auto importanceDirection = normalize(importancePoint - vertex.intersection().P);

    sampledEvent = ScatteringEvent::Other;
    auto woSample = powerCosineSampleHemisphere(rng.getFloat(), rng.getFloat(), importanceDirection, m_fSkeletonStrength);
    float cosThetaOutDir;
    throughtput = vertex.bsdf().eval(woSample.value, cosThetaOutDir);
    throughtput *= abs(cosThetaOutDir);

    return woSample;
}

float SkelBasedPathtraceRenderer::pdfSkeleton(PathVertex& vertex, const Vec3f& outDir, GraphNodeIndex nodeIdx) const {
    auto importancePointSetIdx =
            clamp(size_t(0),
                  size_t(0),
                  m_PointLightImportancePoints.size() - 1);
    auto importancePoint = m_PointLightImportancePoints[importancePointSetIdx][nodeIdx];
    auto importanceDirection = normalize(importancePoint - vertex.intersection().P);

    return powerCosineSampleHemispherePDF(outDir, importanceDirection, m_fSkeletonStrength);
}

Sample3f SkelBasedPathtraceRenderer::sampleBSDF(ThreadRNG& rng, const PathVertex& vertex,
                    Vec3f& throughtput, uint32_t& sampledEvent) const {
    Sample3f woSample;
    float cosThetaOutDir;
    throughtput = vertex.bsdf().sample(Vec3f(rng.getFloat(), rng.getFloat2()), woSample, cosThetaOutDir, &sampledEvent);
    throughtput *= abs(cosThetaOutDir);

    return woSample;
}

float SkelBasedPathtraceRenderer::pdfBSDF(PathVertex& vertex, const Vec3f& outDir) const {
    return vertex.bsdf().pdf(outDir);
}

void SkelBasedPathtraceRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxPathDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_fSkeletonStrength));
}

void SkelBasedPathtraceRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
    serialize(xml, "skeletonStrength", m_fSkeletonStrength);
}

void SkelBasedPathtraceRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
    serialize(xml, "skeletonStrength", m_fSkeletonStrength);
}

void SkelBasedPathtraceRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    processTileSamples(viewport, [this, threadID](uint32_t x, uint32_t y, uint32_t pixelID, uint32_t sampleID) {
        processSample(threadID, pixelID, sampleID, x, y);
    });
}

void SkelBasedPathtraceRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    addFramebufferChannel("isDelta");
    for(auto i : range(m_nMaxPathDepth)) {
        addFramebufferChannel("depth_" + toString(i + 1));
    }
}

}
