#include "BDPTImportanceCachingRenderer.hpp"
#include <bonez/scene/lights/AreaLight.hpp>

#include "../paths.hpp"

#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void BDPTImportanceCachingRenderer::preprocess() {
    m_LightSampler.initFrame(getScene());
    m_fImportanceRecordOrientationTradeOff = 0.5f / length(getScene().getBBox().size());

    m_BeginFrameTimer = TaskTimer({
                                      "TraceLightPaths",
                                      "AssignLightVerticesToTiles",
                                      "ComputeImportanceRecords",
                                      "BuildKdTree",
                                      "ComputeIRDistributions"
                                  }, 1u);
    m_TileProcessingTimer = TaskTimer({
                                  "TraceEyePaths",
                                  "EvalContribution",
                                  "KdTreeLookup",
                                  "ResampleLightVertex",
                                  "ConnectLightVerticesToSensor",
                                  "EvalResamplingMISWeight"
                              }, getThreadCount());
}

void BDPTImportanceCachingRenderer::beginFrame() {
    m_nLightPathCount = getFramebuffer().getPixelCount();

    m_EmissionVertexBuffer.resize(m_nLightPathCount);
    m_LightPathBuffer.resize(getMaxLightPathDepth(), m_nLightPathCount);

    auto maxLightPathDepth = getMaxLightPathDepth();

    auto mis = [&](float v) {
        return Mis(v);
    };

    {
        auto timer = m_BeginFrameTimer.start(0);

        // Sample light paths
        processTasksDeterminist(m_nLightPathCount, [&](uint32_t pathID, uint32_t threadID) {
            ThreadRNG rng(*this, threadID);
            auto pLightPath = m_LightPathBuffer.getSlicePtr(pathID);
            m_EmissionVertexBuffer[pathID] = sampleLightPath(pLightPath, maxLightPathDepth, getScene(),
                            m_LightSampler, getSppCount(), mis, rng);
        }, getThreadCount());
    }

    {
        auto timer = m_BeginFrameTimer.start(1);

        // Partition light vertices according to framebuffer tiles
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
                        return m_LightPathBuffer[i].m_fPathPdf > 0.f;
                    },
                    rng);
    }

    computeImportanceRecords();

    {
        auto timer = m_BeginFrameTimer.start(4);
        computeLightVertexDistributions();
    }

    m_ShadingPointIRBuffer.resize(m_nIRCountPerShadingPoint, getThreadCount());
    m_ShadingPointUnfilteredIRBuffer.resize(getUnfilteredIRCountPerShadingPoint(), getThreadCount());
}

void BDPTImportanceCachingRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
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

    {
        auto timer = m_TileProcessingTimer.start(4, threadID);
        connectLightVerticesToCamera(threadID, tileID, viewport);
    }
}

void BDPTImportanceCachingRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
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

    auto maxEyePathDepth = getMaxEyePathDepth();
    auto maxLightPathDepth = getMaxLightPathDepth();

    auto extendEyePath = [&]() -> bool {
        auto traceEyePathTimer = m_TileProcessingTimer.start(0, threadID);
        return eyeVertex.m_nDepth < maxEyePathDepth && eyeVertex.extend(eyeVertex, getScene(), getSppCount(), false, rng, mis);
    };

    // Iterate over an eye path
    do {
        // Intersection with light source
        {
            auto totalLength = eyeVertex.m_nDepth;
            if(acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
                auto evalContribTimer = m_TileProcessingTimer.start(1, threadID);

                auto contrib = fImportanceScale * computeEmittedRadiance(eyeVertex, getScene(), m_LightSampler, getSppCount(), mis);

                evalContribTimer.storeDuration();

                accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));
                auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, 0u);
                accumulate(FINAL_RENDER_DEPTH1 + m_nMaxDepth + strategyOffset, pixelID, Vec4f(contrib, 0));
            }
        }

        // Connections
        if(eyeVertex.m_Intersection) {
            // Get nearest importance records and their number:
            auto kdTreeLookupTimer = m_TileProcessingTimer.start(2, threadID);

            auto irCount = m_ImportanceRecordContainer.getNearestImportanceRecords(
                        eyeVertex.m_Intersection,
                        getUnfilteredIRCountPerShadingPoint(),
                        m_ShadingPointUnfilteredIRBuffer.getSlicePtr(threadID),
                        m_nIRCountPerShadingPoint,
                        m_ShadingPointIRBuffer.getSlicePtr(threadID),
                        m_fImportanceRecordOrientationTradeOff);

            kdTreeLookupTimer.storeDuration();

            if(irCount > 0u) {
                auto pImportanceRecords = m_ShadingPointIRBuffer.getSlicePtr(threadID);

                if(eyeVertex.m_nDepth == 1u) {
                    auto nearestIR = pImportanceRecords[0];
                    accumulate(NEAREST_IMPORTANCE_RECORD, pixelID, Vec4f(getColor(nearestIR), 0.f));
                }

                for(auto j = 0u; j <= maxLightPathDepth; ++j) {
                   auto lightPathDepth = j;
                   auto totalLength = eyeVertex.m_nDepth + lightPathDepth + 1;

                   if(acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
                       //for(auto i = 0u; i < m_ImportanceCache.getEnabledDistributionCount(); ++i) {
                           auto resamplingTimer = m_TileProcessingTimer.start(3, threadID);

                           auto stategyIndex = clamp(size_t(getFloat(threadID) * m_ImportanceCache.getEnabledDistributionCount()), size_t(0),
                                          size_t(m_ImportanceCache.getEnabledDistributionCount()) - 1);
                           float strategyPdf = 1.f / m_ImportanceCache.getEnabledDistributionCount();

                           auto lightVertexSample = [&]() {
                               auto result = m_ImportanceCache.sampleDistribution(lightPathDepth, stategyIndex, irCount, pImportanceRecords, getFloat(threadID));
                               return result;
                           }();

                           resamplingTimer.storeDuration();

                           if(lightVertexSample.pdf) {
                                auto timer = m_TileProcessingTimer.start(5, threadID);
                                auto weight = [&]() {
                                    if(m_bUseAlphaMaxHeuristic) {
                                        return m_ImportanceCache.misAlphaMaxHeuristic(lightVertexSample, lightPathDepth, stategyIndex, irCount, pImportanceRecords);
                                    }
                                    return m_ImportanceCache.misBalanceHeuristic(lightVertexSample, lightPathDepth, stategyIndex, irCount, pImportanceRecords);
                                }();
                                timer.storeDuration();

                                if(weight > 0.f) {
                                    if(lightPathDepth == 0u) {
                                        auto evalContribTimer = m_TileProcessingTimer.start(1, threadID);

                                       // EmissionVertex
                                       const auto& emissionVertex = m_EmissionVertexBuffer[lightVertexSample.value];
                                       auto contrib = (1.f / m_nPerImportanceCacheLightPathCount) * fImportanceScale * connectVertices(eyeVertex, emissionVertex, getScene(), getSppCount(), mis);

                                       contrib *= weight / (strategyPdf * lightVertexSample.pdf);

                                       evalContribTimer.storeDuration();

                                       accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                                       accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));
                                       accumulate(DIST0_CONTRIB + stategyIndex, pixelID, Vec4f(contrib, 0));
                                       auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, lightPathDepth + 1);
                                       accumulate(FINAL_RENDER_DEPTH1 + m_nMaxDepth + strategyOffset, pixelID, Vec4f(contrib, 0));
                                   } else {
                                       auto evalContribTimer = m_TileProcessingTimer.start(1, threadID);

                                       // SurfaceVertex
                                       auto firstPathOffset = lightPathDepth - 1;
                                       auto pLightPath = &m_LightPathBuffer[firstPathOffset + lightVertexSample.value * getMaxLightPathDepth()];

                                       auto contrib = (1.f / m_nPerImportanceCacheLightPathCount) * fImportanceScale * connectVertices(eyeVertex, *pLightPath, getScene(), getSppCount(), mis);

                                       contrib *= weight / (strategyPdf * lightVertexSample.pdf);

                                       evalContribTimer.storeDuration();

                                       accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                                       accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));
                                       accumulate(DIST0_CONTRIB + stategyIndex, pixelID, Vec4f(contrib, 0));
                                       auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, lightPathDepth + 1);
                                       accumulate(FINAL_RENDER_DEPTH1 + m_nMaxDepth + strategyOffset, pixelID, Vec4f(contrib, 0));
                                   }
                               }
                           }
                       //}
                   }
                }
            } else {
                // TODO: must fallback to a conservative distribution
            }
        }

    } while(extendEyePath());
}

void BDPTImportanceCachingRenderer::connectLightVerticesToCamera(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
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

        accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0.f));
        accumulate(FINAL_RENDER_DEPTH1 + totalDepth - 1u, pixelID, Vec4f(contrib, 0.f));
        auto strategyOffset = computeBPTStrategyOffset(totalDepth + 1, pLightVertex->m_nDepth + 1);
        accumulate(FINAL_RENDER_DEPTH1 + m_nMaxDepth + strategyOffset, pixelID, Vec4f(contrib, 0));
    }
}

void BDPTImportanceCachingRenderer::computeLightVertexDistributions() {
    auto maxLightPathDepth = getMaxLightPathDepth();

    m_ImportanceCache.setEnabledDistributions(m_sDistributionSelector);

    auto evalContrib = [&](uint32_t pathIdx, std::size_t depth, uint32_t importanceRecordID) {
        const auto& importanceRecord = m_ImportanceRecordContainer[importanceRecordID];

        if(depth == size_t(0)) {
            // EmissionVertex
            const auto& vertex = m_EmissionVertexBuffer[pathIdx];
            return vertex.m_fLightPdf == 0.f ? 0.f : luminance(evalContribution(vertex, importanceRecord.m_Intersection, importanceRecord.m_BSDF));
        }

        auto vertexIdx = depth - 1 + pathIdx * maxLightPathDepth;
        if(m_LightPathBuffer[vertexIdx].m_fPathPdf == 0.f) {
            return 0.f;
        }

        return luminance(evalContribution(m_LightPathBuffer[vertexIdx], importanceRecord.m_Intersection, importanceRecord.m_BSDF));
    };

    auto evalUnoccluded = [&](uint32_t pathIdx, std::size_t depth, uint32_t importanceRecordID) {
        const auto& importanceRecord = m_ImportanceRecordContainer[importanceRecordID];

        if(depth == size_t(0)) {
            // EmissionVertex
            const auto& vertex = m_EmissionVertexBuffer[pathIdx];
            return vertex.m_fLightPdf == 0.f ? 0.f : luminance(evalUnoccludedContribution(vertex, importanceRecord.m_Intersection, importanceRecord.m_BSDF));
        }

        auto vertexIdx = depth - 1 + pathIdx * maxLightPathDepth;
        if(m_LightPathBuffer[vertexIdx].m_fPathPdf == 0.f) {
            return 0.f;
        }

        return luminance(evalUnoccludedContribution(m_LightPathBuffer[vertexIdx], importanceRecord.m_Intersection, importanceRecord.m_BSDF));
    };

    auto evalBounded = [&](uint32_t pathIdx, std::size_t depth, uint32_t importanceRecordID) {
        const auto& importanceRecord = m_ImportanceRecordContainer[importanceRecordID];

        if(depth == size_t(0)) {
            // EmissionVertex
            const auto& vertex = m_EmissionVertexBuffer[pathIdx];
            return vertex.m_fLightPdf == 0.f ? 0.f : luminance(evalBoundedContribution(vertex, importanceRecord.m_Intersection, importanceRecord.m_fRegionOfInfluenceRadius, importanceRecord.m_BSDF));
        }

        auto vertexIdx = depth - 1 + pathIdx * maxLightPathDepth;
        if(m_LightPathBuffer[vertexIdx].m_fPathPdf == 0.f) {
            return 0.f;
        }

        return luminance(evalBoundedContribution(m_LightPathBuffer[vertexIdx], importanceRecord.m_Intersection, importanceRecord.m_fRegionOfInfluenceRadius, importanceRecord.m_BSDF));
    };

    auto evalConservative = [&](uint32_t pathIdx, std::size_t depth, uint32_t importanceRecordID) {
        if(depth == size_t(0)) {
            // EmissionVertex
            const auto& vertex = m_EmissionVertexBuffer[pathIdx];
            return vertex.m_fLightPdf == 0.f ? 0.f : 1.f;
        }

        auto vertexIdx = depth - 1 + pathIdx * maxLightPathDepth;
        if(m_LightPathBuffer[vertexIdx].m_fPathPdf == 0.f) {
            return 0.f;
        }

        return 1.f;
    };

    auto bOptimize = m_bUseAlphaMaxHeuristic && m_bUseDistributionWeightingOptimization;

    m_ImportanceCache.buildDistributions(m_ImportanceRecordContainer.size(),
                                         m_nPerImportanceCacheLightPathCount,
                                         getMaxLightPathDepth(),
                                         evalContrib,
                                         evalUnoccluded,
                                         evalBounded,
                                         evalConservative,
                                         m_AlphaConfidenceValues,
                                         getThreadCount(),
                                         bOptimize);
}

void BDPTImportanceCachingRenderer::computeImportanceRecords() {
    m_ImportanceRecordContainer.clear();
    m_ImportanceRecordDepths.clear();

    {
        auto timer = m_BeginFrameTimer.start(2);

        if(m_bUseUniformImportanceRecordSampling) {
            std::vector<SurfacePointSample> points;

            auto N = max(1u, uint32_t(sqrt(m_nUniformImportanceRecordCount)));
            m_nUniformImportanceRecordCount = N * N;

            std::mt19937 g(getIterationCount());

            JitteredDistribution1D jittered1D(m_nUniformImportanceRecordCount);
            std::vector<float> meshSamples, triangleSamples, sideSamples;

            for(auto pBuffer: { &meshSamples, &triangleSamples, &sideSamples }) {
                generateDistribution(m_nUniformImportanceRecordCount, jittered1D,
                                     getRandomGenerator().getGenerator(0u), std::back_inserter(*pBuffer));
                std::shuffle(begin(*pBuffer), end(*pBuffer), g);
            }

            JitteredDistribution2D jittered2D(N, N);
            std::vector<Vec2f> positionSamples;
            generateDistribution(m_nUniformImportanceRecordCount, jittered2D,
                                 getRandomGenerator().getGenerator(0u), std::back_inserter(positionSamples));
            std::shuffle(begin(positionSamples), end(positionSamples), g);

            auto sampleGenerator = [&](uint32_t i) -> Scene::SurfacePointSampleParams {
                Scene::SurfacePointSampleParams params;
                params.meshSample = meshSamples[i];
                params.triangleSample = triangleSamples[i];
                params.sideSample = sideSamples[i];
                params.positionSample = positionSamples[i];
                return params;
            };

            getScene().sampleSurfacePointsWrtArea(m_nUniformImportanceRecordCount,
                                                  std::back_inserter(points),
                                                  sampleGenerator);

            for(auto& point: points) {
                auto sceneArea = 1.f / point.pdf;
                auto pointArea = sceneArea / m_nUniformImportanceRecordCount;
                auto radiusOfInfluence = sqrt(pointArea * one_over_pi<float>());
                auto s2D = getFloat2(0u);
                auto wiSample = uniformSampleHemisphere(s2D.x, s2D.y, point.value.Ns);
                m_ImportanceRecordContainer.emplace_back(point.value, BSDF(wiSample.value, point.value, getScene()), radiusOfInfluence);
                m_ImportanceRecordDepths.emplace_back(0u);
            }
        } else {
            // Compute importance caches

            // Compute the importance cache sampling grid in order to get approximately
            // m_fImportanceRecordsDensity * getFramebuffer().getPixelCount() cells
            // and to keep the framebuffer ratio getFramebuffer().getWidth() / getFramebuffer().getHeight()
            auto importanceCacheGridHeight = sqrt(m_fImportanceRecordsDensity) * getFramebuffer().getHeight();
            auto importanceCacheGridWidth = importanceCacheGridHeight * getFramebuffer().getWidth() / getFramebuffer().getHeight();

            Vec2u importanceCacheGridSize = max(Vec2u(1), Vec2u(importanceCacheGridHeight, importanceCacheGridWidth));
            m_ImportanceRecordContainer.reserve(importanceCacheGridSize.x * importanceCacheGridSize.y);

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

                        m_ImportanceRecordContainer.emplace_back(I, BSDF(-ray.dir, I, getScene()), radiusOfInfluence);
                        m_ImportanceRecordDepths.emplace_back(1u);

                        for(auto depth = 1u; depth < getMaxEyePathDepth(); ++depth) {
                            BSDF bsdf(-ray.dir, I, getScene());
                            Sample3f woSample;
                            float cosThetaOut;
                            auto fr = bsdf.sample(getFloat3(0u), woSample, cosThetaOut);
                            if(fr == zero<Vec3f>() || woSample.pdf == 0.f) {
                                break;
                            }

                            ray = Ray(I, woSample.value);
                            auto nextI = getScene().intersect(ray);

                            if(nextI) {
                                I = nextI;
                                auto area = pi<float>() * sqr(radiusOfInfluence);
                                auto projectedArea = area * abs(cosThetaOut);
                                auto cameraRayFootprint = projectedArea / abs(dot(I.Ns, -woSample.value));
                                radiusOfInfluence = sqrt(cameraRayFootprint * one_over_pi<float>());

                                m_ImportanceRecordContainer.emplace_back(I, BSDF(-ray.dir, I, getScene()), radiusOfInfluence);
                                m_ImportanceRecordDepths.emplace_back(depth + 1);
                            } else {
                                break;
                            }
                        }
                    }

                    ++k;
                }
            }
        }
    }

    {
        auto timer = m_BeginFrameTimer.start(3);
        m_ImportanceRecordContainer.buildImportanceRecordsKdTree();
    }
}

Vec3f BDPTImportanceCachingRenderer::evalContribution(const PathVertex& lightVertex, const SurfacePoint& I, const BSDF& bsdf) const {
    Vec3f wi;
    float dist;
    auto G = geometricFactor(I, lightVertex.m_Intersection, wi, dist);
    if(G > 0.f) {
        float cosThetaOutVPL, costThetaOutBSDF;
        auto M = bsdf.eval(wi, costThetaOutBSDF) * lightVertex.m_BSDF.eval(-wi, cosThetaOutVPL);
        if(M != zero<Vec3f>()) {
            Ray shadowRay(I, lightVertex.m_Intersection, wi, dist);
            if(!getScene().occluded(shadowRay)) {
                return M * G * lightVertex.m_Power;
            }
        }
    }
    return zero<Vec3f>();
}

Vec3f BDPTImportanceCachingRenderer::evalUnoccludedContribution(const PathVertex& lightVertex, const SurfacePoint& I, const BSDF& bsdf) const {
    Vec3f wi;
    float dist;
    auto G = geometricFactor(I, lightVertex.m_Intersection, wi, dist);
    if(G > 0.f) {
        float cosThetaOutVPL, costThetaOutBSDF;
        auto M = bsdf.eval(wi, costThetaOutBSDF) * lightVertex.m_BSDF.eval(-wi, cosThetaOutVPL);
        return M * G * lightVertex.m_Power;
    }
    return zero<Vec3f>();
}

float BDPTImportanceCachingRenderer::evalBoundedGeometricFactor(const PathVertex& lightVertex, const SurfacePoint& I, float radiusOfInfluence, Vec3f& wi) const {
    wi = lightVertex.m_Intersection.P - I.P;
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
    auto vplAngleToI = acos(clamp(dot(lightVertex.m_Intersection.Ns, -wi), -1.f, 1.f)); // clamp to avoid nan

    return max(0.f, float(cos(vplAngleToI + maxAngleChange))) * max(0.f, getCosMaxIncidentAngle()) / sqr(distMin);
}

Vec3f BDPTImportanceCachingRenderer::evalBoundedContribution(const PathVertex& lightVertex, const SurfacePoint& I, float radiusOfInfluence, const BSDF& bsdf) const {
    Vec3f wi;
    auto maxG = evalBoundedGeometricFactor(lightVertex, I, radiusOfInfluence, wi);
    float cosThetaOutVPL, costThetaOutBSDF;
    auto M = bsdf.eval(wi, costThetaOutBSDF) * lightVertex.m_BSDF.eval(-wi, cosThetaOutVPL);
    return M * maxG * lightVertex.m_Power;
}

Vec3f BDPTImportanceCachingRenderer::evalContribution(const EmissionVertex& vertex, const SurfacePoint& I, const BSDF& bsdf) const {
    RaySample shadowRay;
    auto Le = vertex.m_pLight->sampleDirectIllumination(getScene(), vertex.m_PositionSample, I, shadowRay);

    if(Le != zero<Vec3f>() && shadowRay.pdf) {
        shadowRay.pdf *= vertex.m_fLightPdf;
        float cosThetaOutDir;
        auto fr = bsdf.eval(shadowRay.value.dir, cosThetaOutDir);
        auto contrib = Le * fr * abs(cosThetaOutDir) / shadowRay.pdf;
        if(contrib != zero<Vec3f>() && !getScene().occluded(shadowRay.value)) {
            return contrib;
        }
    }

    return zero<Vec3f>();
}

Vec3f BDPTImportanceCachingRenderer::evalUnoccludedContribution(const EmissionVertex& vertex, const SurfacePoint& I, const BSDF& bsdf) const {
    RaySample shadowRay;
    auto Le = vertex.m_pLight->sampleDirectIllumination(getScene(), vertex.m_PositionSample, I, shadowRay);

    if(Le != zero<Vec3f>() && shadowRay.pdf) {
        shadowRay.pdf *= vertex.m_fLightPdf;
        float cosThetaOutDir;
        auto fr = bsdf.eval(shadowRay.value.dir, cosThetaOutDir);
        return Le * fr * abs(cosThetaOutDir) / shadowRay.pdf;
    }

    return zero<Vec3f>();
}

Vec3f BDPTImportanceCachingRenderer::evalBoundedContribution(const EmissionVertex& vertex, const SurfacePoint& I, float radiusOfInfluence, const BSDF& bsdf) const {
    Ray shadowRay;
    auto LeMax = vertex.m_pLight->evalBoundedDirectIllumination(getScene(), vertex.m_PositionSample, I, radiusOfInfluence, shadowRay);
    return bsdf.eval(shadowRay.dir) * abs(getCosMaxIncidentAngle()) * LeMax / vertex.m_fLightPdf;
}

void BDPTImportanceCachingRenderer::drawGLData(const ViewerData& viewerData) {
    m_GLDebugStream.clearObjects();
    viewerData.debugRenderer.addStream(&m_GLDebugStream);

    for(auto i: range(m_ImportanceRecordContainer.size())) {
        const auto& importanceRecord = m_ImportanceRecordContainer[i];
        if(m_ImportanceRecordDepths[i] == m_nImportanceRecordDisplayDepth) {
            m_GLDebugStream.addSphere(importanceRecord.m_Intersection.P,
                                      importanceRecord.m_fRegionOfInfluenceRadius,
                                      getColor(i));
        }
    }
}

void BDPTImportanceCachingRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_nPerImportanceCacheLightPathCount));

    gui.addVarRW(BNZ_GUI_VAR(m_fImportanceRecordOrientationTradeOff));
    gui.addValue("ImportanceRecordsCount", m_ImportanceRecordContainer.size());
    gui.addVarRW(BNZ_GUI_VAR(m_bUseUniformImportanceRecordSampling));
    if(m_bUseUniformImportanceRecordSampling) {
        gui.addVarRW(BNZ_GUI_VAR(m_nUniformImportanceRecordCount));
    } else {
        gui.addVarRW(BNZ_GUI_VAR(m_fImportanceRecordsDensity));
    }
    gui.addVarRW(BNZ_GUI_VAR(m_nIRCountPerShadingPoint));
    gui.addVarRW(BNZ_GUI_VAR(m_nUnfilteredIRCountPerShadingPointFactor));
    gui.addVarRW(BNZ_GUI_VAR(m_sDistributionSelector));
    gui.addVarRW(BNZ_GUI_VAR(m_AlphaConfidenceValues));
    m_AlphaConfidenceValues = clamp(m_AlphaConfidenceValues, Vec4f(0.f), Vec4f(1.f));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseAlphaMaxHeuristic));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseDistributionWeightingOptimization));
    gui.addSeparator();
    gui.addVarRW(BNZ_GUI_VAR(m_nImportanceRecordDisplayDepth));
}

void BDPTImportanceCachingRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "perImportanceCacheLightPathCount", m_nPerImportanceCacheLightPathCount);

    serialize(xml, "useUniformImportanceRecordSampling", m_bUseUniformImportanceRecordSampling);
    serialize(xml, "uniformImportanceRecordCount", m_nUniformImportanceRecordCount);
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

void BDPTImportanceCachingRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "perImportanceCacheLightPathCount", m_nPerImportanceCacheLightPathCount);

    serialize(xml, "useUniformImportanceRecordSampling", m_bUseUniformImportanceRecordSampling);
    serialize(xml, "uniformImportanceRecordCount", m_nUniformImportanceRecordCount);
    serialize(xml, "importanceRecordsDensity", m_fImportanceRecordsDensity);
    serialize(xml, "irCountPerShadingPoint", m_nIRCountPerShadingPoint);
    serialize(xml, "unfilteredIRCountPerShadingPointFactor", m_nUnfilteredIRCountPerShadingPointFactor);
    serialize(xml, "distributionSelector", m_sDistributionSelector);
    serialize(xml, "alphaConfidenceValue", m_AlphaConfidenceValues);
    serialize(xml, "useAlphaMaxHeuristic", m_bUseAlphaMaxHeuristic);
    serialize(xml, "useDistributionWeightingOptimization", m_bUseDistributionWeightingOptimization);
}

void BDPTImportanceCachingRenderer::doStoreStatistics() const {
    if(auto pStats = getStatisticsOutput()) {
        serialize(*pStats, "ImportanceRecordsCount", m_ImportanceRecordContainer.size());
        serialize(*pStats, "BeginFrameTime", m_BeginFrameTimer);
        serialize(*pStats, "TileProcessingTime", (const TaskTimer&) m_TileProcessingTimer);
    }
}

void BDPTImportanceCachingRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    addFramebufferChannel("nearest_importance_record");
    addFramebufferChannel("dist0_contrib");
    addFramebufferChannel("dist1_contrib");
    addFramebufferChannel("dist2_contrib");
    addFramebufferChannel("dist3_contrib");
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
