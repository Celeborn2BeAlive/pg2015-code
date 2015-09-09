#include "IGIImportanceCachingRenderer.hpp"
#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void IGIImportanceCachingRenderer::computeImportanceRecords() {
    m_ImportanceRecordContainer.clear();

    auto& importanceRecordBuffer = m_ImportanceRecordContainer;

    // Compute importance caches
    Vec2u importanceCacheGridSize = max(Vec2u(1), Vec2u(Vec2f(getFramebuffer().getSize()) * m_fImportanceRecordsDensity));
    importanceRecordBuffer.reserve(importanceCacheGridSize.x * importanceCacheGridSize.y);

    JitteredDistribution2D sampler2D(importanceCacheGridSize.x, importanceCacheGridSize.y);

    float screenSpaceRadius = max(1.f / importanceCacheGridSize.x, 1.f / importanceCacheGridSize.y);
    float screenSpaceArea = sqr(screenSpaceRadius) * pi<float>();

    auto k = 0u;
    for(auto j = 0u; j < importanceCacheGridSize.y; ++j) {
        for(auto i = 0u; i < importanceCacheGridSize.x; ++i) {
            auto s2D = sampler2D.getSample(k, getFloat2(0u));

            RaySample raySample;
            Intersection I;
            float positionPdf, directionPdf, sampledPointToIncidentDirectionJacobian,
                    intersectionPdfWrtArea;

            getSensor().sampleExitantRay(getScene(), Vec2f(0.5f), s2D, raySample, positionPdf, directionPdf, I,
                                         sampledPointToIncidentDirectionJacobian, intersectionPdfWrtArea);
            auto ray = raySample.value;

            if(I) {
                // We have intersectionPdfWrtArea = directionPdf / dirToSurfaceJacobian
                // and directionPdf = imagePointPdf / imageToDirJacobian
                // So intersectionPdfWrtArea = imagePointPdf / (imageToDirJacobian * dirToSurfaceJacobian)
                // imagePointPdf == 1 since the sampling is uniform on the image plane
                // Then intersectionPdfWrtArea = 1 / (imageToDirJacobian * dirToSurfaceJacobian) = 1 / imageToSurfaceJacobian
                auto imageToSurfaceJacobian =  1.f / intersectionPdfWrtArea;
                // The cameraRayFootprint on surfaces can be approximated by the screen space area spanning a grid cell times the imageToSurfaceJacobian
                auto cameraRayFootprint = screenSpaceArea * imageToSurfaceJacobian;
                // Finally the radius of influence is the radius of the disk of area cameraRayFootprint
                auto radiusOfInfluence = sqrt(cameraRayFootprint * one_over_pi<float>());

                importanceRecordBuffer.emplace_back(I, BSDF(-ray.dir, I, getScene()), radiusOfInfluence);
            }

            ++k;
        }
    }

    m_ImportanceRecordContainer.buildImportanceRecordsKdTree();
}

void IGIImportanceCachingRenderer::preprocess() {
    m_LightSampler.initFrame(getScene());
    m_fImportanceRecordOrientationTradeOff = 0.5f / length(getScene().getBBox().size()); // As mentionned in the article
}

void IGIImportanceCachingRenderer::beginFrame() {
    if(m_nPathCount > 0u && m_nMaxPathDepth > 2u) {
        computeImportanceRecords();

        m_ImportanceCache.setEnabledDistributions(m_sDistributionSelector);

        auto maxLightPathDepth = getMaxLightPathDepth();
        m_SurfaceVPLBuffer.resize(maxLightPathDepth * m_nPathCount);
        m_EmissionVPLBuffer.resize(m_nPathCount);

        m_fPowerScale = 1.f / m_nPathCount;

        for(auto i = 0u; i < m_nPathCount; ++i) {
            auto pLightPath = m_SurfaceVPLBuffer.data() + i * maxLightPathDepth;
            std::for_each(pLightPath, pLightPath + maxLightPathDepth, [&](SurfaceVPL& v) { v.pdf = 0.f; });

            ThreadRNG rng(*this, 0u);

            auto lightSample = rng.getFloat();
            auto positionSample = rng.getFloat2();
            auto dirSample = rng.getFloat2();
            const Light* pLight = nullptr;
            float lightPdf, positionPdfGivenLight;
            auto primaryVertex = samplePrimaryLightVertex(getScene(), m_LightSampler, lightSample, positionSample,
                                                          dirSample, pLight, lightPdf, positionPdfGivenLight);

            m_EmissionVPLBuffer[i] = EmissionVPL(pLight, lightPdf, positionSample);

            for(const auto& vertex: makePath(getScene(), primaryVertex, rng)) {
                pLightPath->init(vertex);
                if(vertex.length() == maxLightPathDepth) {
                    break;
                }
                ++pLightPath;
            }
        }

        // The total number of VPL is the number of emissive VPL (m_nPathCount) + the number of surface VPL (maxLightPathDepth * m_nPathCount)
        m_nVPLCount = m_nPathCount + maxLightPathDepth * m_nPathCount;

        auto evalContrib = [&](uint32_t vplIdx, uint32_t importanceRecordIdx) {
            const auto& importanceRecord = m_ImportanceRecordContainer[importanceRecordIdx];
            if(vplIdx < m_nPathCount) {
                // EmissionVPL
                const auto& vpl = m_EmissionVPLBuffer[vplIdx];
                return vpl.lightPdf == 0.f ? 0.f : luminance(evalVPLContribution(vpl, importanceRecord.m_Intersection, importanceRecord.m_BSDF));
            }
            // SurfaceVPL
            vplIdx -= m_nPathCount;
            const auto& vpl = m_SurfaceVPLBuffer[vplIdx];
            return vpl.pdf == 0.f ? 0.f : luminance(evalVPLContribution(vpl, importanceRecord.m_Intersection, importanceRecord.m_BSDF));
        };
        auto evalUnoccluded = [&](uint32_t vplIdx, uint32_t importanceRecordIdx) {
            const auto& importanceRecord = m_ImportanceRecordContainer[importanceRecordIdx];
            if(vplIdx < m_nPathCount) {
                // EmissionVPL
                const auto& vpl = m_EmissionVPLBuffer[vplIdx];
                return vpl.lightPdf == 0.f ? 0.f : luminance(evalUnoccludedVPLContribution(vpl, importanceRecord.m_Intersection, importanceRecord.m_BSDF));
            }
            // SurfaceVPL
            vplIdx -= m_nPathCount;
            const auto& vpl = m_SurfaceVPLBuffer[vplIdx];
            return vpl.pdf == 0.f ? 0.f : luminance(evalUnoccludedVPLContribution(vpl, importanceRecord.m_Intersection, importanceRecord.m_BSDF));
        };
        auto evalBounded = [&](uint32_t vplIdx, uint32_t importanceRecordIdx) {
            const auto& importanceRecord = m_ImportanceRecordContainer[importanceRecordIdx];
            if(vplIdx < m_nPathCount) {
                // EmissionVPL
                const auto& vpl = m_EmissionVPLBuffer[vplIdx];
                return vpl.lightPdf == 0.f ? 0.f : luminance(evalBoundedVPLContribution(vpl, importanceRecord.m_Intersection, importanceRecord.m_fRegionOfInfluenceRadius,
                                                                                   importanceRecord.m_BSDF));
            }
            // SurfaceVPL
            vplIdx -= m_nPathCount;
            const auto& vpl = m_SurfaceVPLBuffer[vplIdx];
            return vpl.pdf == 0.f ? 0.f : luminance(evalBoundedVPLContribution(vpl, importanceRecord.m_Intersection, importanceRecord.m_fRegionOfInfluenceRadius,
                                                        importanceRecord.m_BSDF));
        };
        auto evalConservative = [&](uint32_t vplIdx, uint32_t importanceRecordIdx) {
            if(vplIdx < m_nPathCount) {
                // EmissionVPL
                const auto& vpl = m_EmissionVPLBuffer[vplIdx];
                return vpl.lightPdf == 0.f ? 0.f : 1.f;
            } else {
                // SurfaceVPL
                vplIdx -= m_nPathCount;
                const auto& vpl = m_SurfaceVPLBuffer[vplIdx];
                return vpl.pdf == 0.f ? 0.f : 1.f;
            }
        };

        auto bOptimize = m_bUseAlphaMaxHeuristic && m_bUseDistributionWeightingOptimization;
        m_ImportanceCache.buildDistributions(m_ImportanceRecordContainer.size(), m_nVPLCount, evalContrib, evalUnoccluded, evalBounded,
                                             evalConservative, m_AlphaConfidenceValues, getThreadCount(), bOptimize);
    }

    m_ShadingPointIRBuffer.resize(m_nIRCountPerShadingPoint, getThreadCount());
    m_ShadingPointUnfilteredIRBuffer.resize(getUnfilteredIRCountPerShadingPoint(), getThreadCount());
}

void IGIImportanceCachingRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    processTileSamples(viewport, [this, threadID](uint32_t x, uint32_t y, uint32_t pixelID, uint32_t sampleID) {
        processSample(threadID, pixelID, sampleID, x, y);
    });
}

void IGIImportanceCachingRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const {
    for(auto i = 0u; i < getFramebufferChannelCount(); ++i) {
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
        accumulate(0, pixelID, Vec4f(contrib, 0));
        accumulate(1, pixelID, Vec4f(contrib, 0));
        return;
    }

    Vec3f L = Vec3f(0.f);

    if(acceptPathDepth(1)) {
        accumulate(1, pixelID, Vec4f(sourceVertex.intersection().Le, 0.f));
        L += sourceVertex.power() * sourceVertex.intersection().Le;
    }

    auto irCount = m_ImportanceRecordContainer.getNearestImportanceRecords(sourceVertex.intersection(),
                                                                 getUnfilteredIRCountPerShadingPoint(),
                                                                 m_ShadingPointUnfilteredIRBuffer.getSlicePtr(threadID),
                                                                 m_nIRCountPerShadingPoint,
                                                                 m_ShadingPointIRBuffer.getSlicePtr(threadID),
                                                                 m_fImportanceRecordOrientationTradeOff);

    if(irCount > 0u) {
        auto pImportanceRecords = m_ShadingPointIRBuffer.getSlicePtr(threadID);
        auto nearestIR = pImportanceRecords[0];
        accumulate(NEAREST_IMPORTANCE_RECORD, pixelID, Vec4f(getColor(nearestIR), 0.f));

        // VPL illumination
        for(auto distribIndex: range(m_ImportanceCache.getEnabledDistributionCount())) {
            auto vplSample = m_ImportanceCache.sampleDistribution(distribIndex, irCount, pImportanceRecords, getFloat(threadID));

            auto weight = 0.f;
            if(vplSample.pdf) {
                if(m_bUseAlphaMaxHeuristic) {
                    weight = m_ImportanceCache.misAlphaMaxHeuristic(vplSample, distribIndex, irCount, pImportanceRecords);
                } else {
                    weight = m_ImportanceCache.misBalanceHeuristic(vplSample, distribIndex, irCount, pImportanceRecords);
                }
            }

            if(weight > 0.f) {
                uint32_t pathDepth;
                auto computeContrib = [&]() {
                    if(vplSample.value < m_nPathCount) {
                        // Emission VPL contribution
                        auto idx = vplSample.value;
                        const auto& vpl = m_EmissionVPLBuffer[idx];
                        pathDepth = 1 + sourceVertex.length();
                        if(pathDepth <= m_nMaxPathDepth && acceptPathDepth(pathDepth)) {
                            return evalVPLContribution(vpl, sourceVertex.intersection(), sourceVertex.bsdf());
                        }
                    } else {
                        // Surface VPL contribution
                        auto idx = vplSample.value - m_nPathCount;
                        const auto& vpl = m_SurfaceVPLBuffer[idx];
                        pathDepth = vpl.depth + 1 + sourceVertex.length();
                        if(pathDepth <= m_nMaxPathDepth && acceptPathDepth(pathDepth)) {
                            return evalVPLContribution(vpl, sourceVertex.intersection(), sourceVertex.bsdf());
                        }
                    }
                    return zero<Vec3f>();
                };

                auto contrib = m_fPowerScale * weight * sourceVertex.power() * computeContrib() / vplSample.pdf;
                L += contrib;
                accumulate(FINAL_RENDER_DEPTH1 + pathDepth - 1u, pixelID, Vec4f(contrib, 0.f));
                accumulate(DIST0_CONTRIB + distribIndex, pixelID, Vec4f(contrib, 0.f));
            }
        }
    }

    accumulate(FINAL_RENDER, pixelID, Vec4f(L, 0.f));
}

Vec3f IGIImportanceCachingRenderer::evalVPLContribution(const SurfaceVPL& vpl, const SurfacePoint& I, const BSDF& bsdf) const {
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

Vec3f IGIImportanceCachingRenderer::evalUnoccludedVPLContribution(const SurfaceVPL& vpl, const SurfacePoint& I, const BSDF& bsdf) const {
    Vec3f wi;
    float dist;
    auto G = geometricFactor(I, vpl.lastVertex, wi, dist);
    if(G > 0.f) {
        float cosThetaOutVPL, costThetaOutBSDF;
        auto M = bsdf.eval(wi, costThetaOutBSDF) * vpl.lastVertexBSDF.eval(-wi, cosThetaOutVPL);
        return M * G * vpl.power;
    }
    return zero<Vec3f>();
}

float IGIImportanceCachingRenderer::evalBoundedGeometricFactor(const SurfaceVPL& vpl, const SurfacePoint& I, float radiusOfInfluence, Vec3f& wi, float& dist) const {
    wi = vpl.lastVertex.P - I.P;
    auto distToVPL = length(wi);

    if(distToVPL == 0.f) {
        return 0.f;
    }

    wi /= distToVPL;

    auto distMin = max(0.f, distToVPL - radiusOfInfluence);
    if(distMin == 0.f) {
        return 0.f;
    }
    auto maxAngleChange = -asin(clamp(radiusOfInfluence / distToVPL, 0.f, 1.f)); // clamp to avoid nan
    auto vplAngleToI = acos(clamp(dot(vpl.lastVertex.Ns, -wi), -1.f, 1.f)); // clamp to avoid nan

    return abs(float(cos(vplAngleToI + maxAngleChange))) * abs(getCosMaxIncidentAngle()) / sqr(distMin);
}

Vec3f IGIImportanceCachingRenderer::evalBoundedVPLContribution(const SurfaceVPL& vpl, const SurfacePoint& I, float radiusOfInfluence, const BSDF& bsdf) const {
    Vec3f wi;
    float dist;
    auto maxG = evalBoundedGeometricFactor(vpl, I, radiusOfInfluence, wi, dist);
    float cosThetaOutVPL, costThetaOutBSDF;
    auto M = bsdf.eval(wi, costThetaOutBSDF) * vpl.lastVertexBSDF.eval(-wi, cosThetaOutVPL);
    return M * maxG * vpl.power;
}

Vec3f IGIImportanceCachingRenderer::evalVPLContribution(const EmissionVPL& vpl, const SurfacePoint& I, const BSDF& bsdf) const {
    RaySample shadowRay;
    auto Le = vpl.pLight->sampleDirectIllumination(getScene(), vpl.positionSample, I, shadowRay);

    if(Le != zero<Vec3f>() && shadowRay.pdf) {
        shadowRay.pdf *= vpl.lightPdf;
        float cosThetaOutDir;
        auto fr = bsdf.eval(shadowRay.value.dir, cosThetaOutDir);
        auto contrib = Le * fr * abs(cosThetaOutDir) / shadowRay.pdf;
        if(contrib != zero<Vec3f>() && !getScene().occluded(shadowRay.value)) {
            return contrib;
        }
    }

    return zero<Vec3f>();
}

Vec3f IGIImportanceCachingRenderer::evalUnoccludedVPLContribution(const EmissionVPL& vpl, const SurfacePoint& I, const BSDF& bsdf) const {
    RaySample shadowRay;
    auto Le = vpl.pLight->sampleDirectIllumination(getScene(), vpl.positionSample, I, shadowRay);

    if(Le != zero<Vec3f>() && shadowRay.pdf) {
        shadowRay.pdf *= vpl.lightPdf;
        float cosThetaOutDir;
        auto fr = bsdf.eval(shadowRay.value.dir, cosThetaOutDir);
        return Le * fr * abs(cosThetaOutDir) / shadowRay.pdf;
    }

    return zero<Vec3f>();
}

Vec3f IGIImportanceCachingRenderer::evalBoundedVPLContribution(const EmissionVPL& vpl, const SurfacePoint& I, float radiusOfInfluence, const BSDF& bsdf) const {
    Ray shadowRay;
    auto LeMax = vpl.pLight->evalBoundedDirectIllumination(getScene(), vpl.positionSample, I, radiusOfInfluence, shadowRay);
    return bsdf.eval(shadowRay.dir) * abs(getCosMaxIncidentAngle()) * LeMax / vpl.lightPdf;
}

uint32_t IGIImportanceCachingRenderer::getMaxLightPathDepth() const {
    // The maximal light path depth is the maximal path depth minus 2: one for the ray from camera and one from the connection ray
    return m_nMaxPathDepth - 2;
}

void IGIImportanceCachingRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxPathDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_nPathCount));
    gui.addVarRW(BNZ_GUI_VAR(m_fImportanceRecordOrientationTradeOff));
    gui.addValue("ImportanceRecordsCount", m_ImportanceRecordContainer.size());
    gui.addVarRW(BNZ_GUI_VAR(m_fImportanceRecordsDensity));
    gui.addVarRW(BNZ_GUI_VAR(m_nIRCountPerShadingPoint));
    gui.addVarRW(BNZ_GUI_VAR(m_nUnfilteredIRCountPerShadingPointFactor));
    gui.addVarRW(BNZ_GUI_VAR(m_sDistributionSelector));
    gui.addVarRW(BNZ_GUI_VAR(m_AlphaConfidenceValues));
    m_AlphaConfidenceValues = clamp(m_AlphaConfidenceValues, Vec4f(0.f), Vec4f(1.f));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseAlphaMaxHeuristic));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseDistributionWeightingOptimization));
}

void IGIImportanceCachingRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
    serialize(xml, "pathCount", m_nPathCount);
    serialize(xml, "importanceRecordsDensity", m_fImportanceRecordsDensity);
    serialize(xml, "irCountPerShadingPoint", m_nIRCountPerShadingPoint);
    serialize(xml, "unfilteredIRCountPerShadingPointFactor", m_nUnfilteredIRCountPerShadingPointFactor);
    serialize(xml, "distributionSelector", m_sDistributionSelector);
    m_AlphaConfidenceValues = Vec4f(1);
    serialize(xml, "alphaConfidenceValue", m_AlphaConfidenceValues);
    m_AlphaConfidenceValues = clamp(m_AlphaConfidenceValues, Vec4f(0.f), Vec4f(1.f));
    serialize(xml, "useAlphaMaxHeuristic", m_bUseAlphaMaxHeuristic);
    serialize(xml, "useDistributionWeightingOptimization", m_bUseDistributionWeightingOptimization);
}

void IGIImportanceCachingRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
    serialize(xml, "pathCount", m_nPathCount);
    serialize(xml, "importanceRecordsDensity", m_fImportanceRecordsDensity);
    serialize(xml, "irCountPerShadingPoint", m_nIRCountPerShadingPoint);
    serialize(xml, "unfilteredIRCountPerShadingPointFactor", m_nUnfilteredIRCountPerShadingPointFactor);
    serialize(xml, "distributionSelector", m_sDistributionSelector);
    serialize(xml, "alphaConfidenceValue", m_AlphaConfidenceValues);
    serialize(xml, "useAlphaMaxHeuristic", m_bUseAlphaMaxHeuristic);
    serialize(xml, "useDistributionWeightingOptimization", m_bUseDistributionWeightingOptimization);
}

void IGIImportanceCachingRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    addFramebufferChannel("nearest_importance_record");
    addFramebufferChannel("dist0_contrib");
    addFramebufferChannel("dist1_contrib");
    addFramebufferChannel("dist2_contrib");
    addFramebufferChannel("dist3_contrib");
    for(auto i : range(m_nMaxPathDepth)) {
        addFramebufferChannel("depth_" + toString(i + 1));
    }
}

}
