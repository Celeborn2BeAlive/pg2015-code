#include "BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer.hpp"
#include <bonez/scene/lights/AreaLight.hpp>

#include "../paths.hpp"
#include "skel_utils.hpp"

#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::preprocess() {
    m_LightSampler.initFrame(getScene());

    m_pSkel = getScene().getCurvSkeleton();

    if(!m_pSkel) {
        throw std::runtime_error("BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer - No skeleton for the scene");
    }

    m_BeginFrameTimer = TaskTimer({
                                      "TraceLightPaths",
                                      "AssignLightVerticesToTiles",
                                      "BuildSkeletonDistributions"
                                  }, 1u);
    m_TileProcessingTimer = TaskTimer({
                                  "TraceEyePaths",
                                  "EvalContribution",
                                  "SkeletonMapping",
                                  "ResampleLightVertex",
                                  "ConnectLightVerticesToSensor"
                              }, getThreadCount());

    m_EyeVertexCountPerNode.resize(m_pSkel->size());
    std::fill(begin(m_EyeVertexCountPerNode), end(m_EyeVertexCountPerNode), 0u);

    m_EyeVertexCountPerNodePerThread.resize(m_pSkel->size(), getThreadCount());
    m_FilteredNodeIndex.resize(m_pSkel->size());
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::beginFrame() {
    m_nLightPathCount = getFramebuffer().getPixelCount();

    m_fImportanceScale = 1.f / getSppCount();
    m_fRadianceScale = 1.f / m_nLightPathCount;

    {
        auto timer = m_BeginFrameTimer.start(0u);
        sampleLightPaths();
    }

    {
        auto timer = m_BeginFrameTimer.start(1u);
        buildDirectImportanceSampleTilePartitionning();
    }


    {
        auto timer = m_BeginFrameTimer.start(2u);
        computeFilteredNodes();
        buildDistributions(m_SkeletonVisibilityDistributions, getScene(), m_FilteredNodes, m_EmissionVertexBuffer.data(), m_LightPathBuffer, m_nPerNodeLightPathCount,
                           m_bUseNodeRadianceWeight, m_bUseNodeDistanceWeight, getThreadCount());
    }

    m_PerThreadNodeBuffer.resize(m_nMaxNodeCount, getThreadCount());
    m_PerThreadFilteredNodeBuffer.resize(m_nMaxNodeCount, getThreadCount());
    std::fill(begin(m_EyeVertexCountPerNodePerThread), end(m_EyeVertexCountPerNodePerThread), 0u);
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::sampleLightPaths() {
    auto maxLightPathDepth = getMaxLightPathDepth();

    auto mis = [&](float v) {
        return Mis(v);
    };

    m_EmissionVertexBuffer.resize(m_nLightPathCount);
    m_LightPathBuffer.resize(maxLightPathDepth, m_nLightPathCount);

    processTasksDeterminist(m_nLightPathCount, [&](uint32_t pathID, uint32_t threadID) {
        ThreadRNG rng(*this, threadID);
        auto pLightPath = m_LightPathBuffer.getSlicePtr(pathID);
        m_EmissionVertexBuffer[pathID] = sampleLightPath(pLightPath, maxLightPathDepth, getScene(),
                        m_LightSampler, getSppCount(), mis, rng);
    }, getThreadCount());
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::buildDirectImportanceSampleTilePartitionning() {
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

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::computeFilteredNodes() {
    updateEyeVertexCountPerNode();

    uint64_t sum;
    double mean, variance;
    computeNodeMappingStatistics(sum, mean, variance);

    computeFilteredNodesList(mean, m_FilteredNodes, m_FilteredNodeIndex);

    m_nTotalFilteredNodeCount += m_FilteredNodes.size();
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::updateEyeVertexCountPerNode() {
    // Count the number of eye vertices mapped to each node
    for(auto nodeID: range(m_pSkel->size())) {
        auto count = size_t(0);
        for(auto threadID: range(getThreadCount())) {
            count += m_EyeVertexCountPerNodePerThread(nodeID, threadID);
        }
        m_EyeVertexCountPerNode[nodeID] += count;
    }
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::computeNodeMappingStatistics(
        uint64_t& sum, double& mean, double& variance) const {
    sum = 0;
    for(auto value: m_EyeVertexCountPerNode) {
        sum += value;
        variance += sqr(value);
    }
    mean = double(sum) / size(m_EyeVertexCountPerNode);
    variance = variance / size(m_EyeVertexCountPerNode) - sqr(mean);
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::computeFilteredNodesList(
        double mappedVertexCountMean,
        std::vector<GraphNodeIndex>& filteredNodes,
        std::vector<std::size_t>& filteredNodeIndex) const {
    filteredNodes.clear();

    // Filter the nodes according to the number of eye vertices mapped
    const double percentageOfMeanEyeVertexCount = m_fNodeFilteringFactor * mappedVertexCountMean;
    for(auto nodeIndex: range(size(m_EyeVertexCountPerNode))) {
        auto value = m_EyeVertexCountPerNode[nodeIndex];
        if(value > percentageOfMeanEyeVertexCount) {
            filteredNodeIndex[nodeIndex] = m_FilteredNodes.size();
            filteredNodes.emplace_back(nodeIndex);
        } else {
            // Reject the node for the next frame
            filteredNodeIndex[nodeIndex] = std::numeric_limits<size_t>::max();
        }
    }
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
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

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                       uint32_t x, uint32_t y) const {
    auto traceEyePathTimer = m_TileProcessingTimer.start(0, threadID);

    auto maxEyePathDepth = getMaxEyePathDepth();
    auto maxLightPathDepth = getMaxLightPathDepth();

    auto mis = [&](float v) {
        return Mis(v);
    };

    ThreadRNG rng(*this, threadID);

    PixelSensor pixelSensor(getSensor(), Vec2u(x, y), getFramebufferSize());

    auto pixelSample = getPixelSample(threadID, sampleID);
    auto sensorSample = getFloat2(threadID);

    auto fImportanceScale = 1.f / getSppCount();
    BDPTPathVertex eyeVertex(getScene(), pixelSensor, sensorSample, pixelSample, m_nLightPathCount, mis);

    traceEyePathTimer.storeDuration();

    if(eyeVertex.m_Power == zero<Vec3f>() || !eyeVertex.m_fPathPdf) {
        return;
    }

    auto pNodeBuffer = m_PerThreadNodeBuffer.getSlicePtr(threadID);
    auto pFilteredNodeBuffer = m_PerThreadFilteredNodeBuffer.getSlicePtr(threadID);

    auto extendEyePath = [&]() -> bool {
        auto traceEyePathTimer = m_TileProcessingTimer.start(0, threadID);
        return eyeVertex.m_nDepth < maxEyePathDepth && eyeVertex.extend(eyeVertex, getScene(), getSppCount(), false, rng, mis);
    };

    // Iterate over an eye path
    do {
        // Intersection with light source
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

        // Connections
        if(eyeVertex.m_Intersection) {
            auto mappingTimer = m_TileProcessingTimer.start(2, threadID);

            auto nodeCount = skeletonMapping(eyeVertex, pNodeBuffer);

            // Output selected mapped nodes on framebuffer
            if(eyeVertex.m_nDepth == 1u) {
                for(auto i : range(nodeCount)) {
                    accumulate(SELECTED_NODE0 + i, pixelID, Vec4f(getColor(pNodeBuffer[i]), 0));
                }
            }

            // Sample a unique mapped node for this eye vertex (in case we don't use combined resampling)
            auto sampledNode = [&]() -> GraphNodeIndex {
                if(nodeCount > 0u && !m_bUseCombinedResampling) {
                    auto idx = clamp(size_t(getFloat(threadID) * nodeCount),
                                     size_t(0),
                                     nodeCount - 1);
                    return pNodeBuffer[idx];
                }
                return UNDEFINED_NODE;
            }();

            std::size_t filteredNodeIndex = std::numeric_limits<std::size_t>::max();
            auto filteredNodeCount = [&]() {
                if(m_bUseCombinedResampling) {
                    auto filteredNodeCount = size_t(0);
                    for(auto i: range(nodeCount)) {
                        auto nodeID = pNodeBuffer[i];
                        ++m_EyeVertexCountPerNodePerThread(nodeID, threadID);
                        if(m_FilteredNodeIndex[nodeID] != std::numeric_limits<size_t>::max()) {
                            pFilteredNodeBuffer[filteredNodeCount] = m_FilteredNodeIndex[nodeID];
                            ++filteredNodeCount;
                        }
                    }
                    return filteredNodeCount;
                }
                if(sampledNode != UNDEFINED_NODE) {
                    ++m_EyeVertexCountPerNodePerThread(sampledNode, threadID);
                    if(m_FilteredNodeIndex[sampledNode] != std::numeric_limits<size_t>::max()) {
                        filteredNodeIndex = m_FilteredNodeIndex[sampledNode];
                        return size_t(1);
                    }
                }
                return size_t(0);
            }();

            mappingTimer.storeDuration();

            // Connection with a light vertex of each depth
            for(auto j = 0u; j <= maxLightPathDepth; ++j) {
                auto lightPathDepth = j;
                auto totalLength = eyeVertex.m_nDepth + lightPathDepth + 1;

                if(acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
                    if(!filteredNodeCount) {
                        auto lightVertexSample = m_SkeletonVisibilityDistributions.sampleDefaultDistribution(lightPathDepth, getFloat(threadID));
                        if(lightVertexSample.pdf > 0.f) {
                            auto evalContribTimer = m_TileProcessingTimer.start(1, threadID);

                            auto contrib = evalLightVertexContrib(eyeVertex, lightPathDepth, lightVertexSample, 1.f);

                            evalContribTimer.storeDuration();

                            accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                            accumulate(DEFAULT_DISTRIB_CONTRIBUTION, pixelID, Vec4f(contrib, 0));
                            accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                            auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, lightPathDepth + 1);
                            accumulate(FINAL_RENDER_DEPTH1 + m_nMaxDepth + strategyOffset, pixelID, Vec4f(contrib, 0));
                        }
                    } else {
                        auto resamplingTimer = m_TileProcessingTimer.start(3, threadID);

                        auto distribIdx = clamp(size_t(getFloat(threadID) * m_SkeletonVisibilityDistributions.distributionCount()), size_t(0),
                                                m_SkeletonVisibilityDistributions.distributionCount() - 1);
                        auto distribPdf = 1.f / m_SkeletonVisibilityDistributions.distributionCount();

                        auto lightPathDiscreteSample = [&]() -> Sample1u {
                            if(m_bUseCombinedResampling) {
                                return m_SkeletonVisibilityDistributions.sampleCombined(distribIdx, pFilteredNodeBuffer, filteredNodeCount, lightPathDepth, getFloat(threadID));
                            }
                            return m_SkeletonVisibilityDistributions.sample(distribIdx, filteredNodeIndex, lightPathDepth, getFloat(threadID));
                        }();
                        lightPathDiscreteSample.pdf *= distribPdf;

                        resamplingTimer.storeDuration();

                        if(lightPathDiscreteSample.pdf > 0.f) {
                            auto weight = [&]() -> float {
                                 if(m_bUseCombinedResampling) {
                                    m_SkeletonVisibilityDistributions.evalCombinedResamplingMISWeight(distribIdx, pFilteredNodeBuffer, filteredNodeCount,
                                                                                                      lightPathDepth, lightPathDiscreteSample);
                                 }
                                 return 1.f;
                            }();

                            auto evalContribTimer = m_TileProcessingTimer.start(1, threadID);

                            auto contrib = evalLightVertexContrib(eyeVertex, lightPathDepth, lightPathDiscreteSample, weight);

                            evalContribTimer.storeDuration();

                            accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                            accumulate(DISTRIB0_CONTRIBUTION + distribIdx, pixelID, Vec4f(contrib, 0));
                            accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                            auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, lightPathDepth + 1);
                            accumulate(FINAL_RENDER_DEPTH1 + m_nMaxDepth + strategyOffset, pixelID, Vec4f(contrib, 0));
                        }
                    }
                }
            }
        }
    } while(extendEyePath());
}

Vec3f BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::evalLightVertexContrib(const PathVertex& eyeVertex, std::size_t lightPathDepth, const Sample1u& lightVertexSample,
                             float resamplingMISWeight) const {
    auto mis = [&](float v) {
        return Mis(v);
    };

    if(resamplingMISWeight == 0.f) {
        return zero<Vec3f>();
    }

    auto connexionContrib = [&]() {
        if(!lightPathDepth) {
            const auto& emissionVertex = m_EmissionVertexBuffer[lightVertexSample.value];
            if(!emissionVertex.m_pLight || !emissionVertex.m_fLightPdf) {
                return zero<Vec3f>();
            }
            return connectVertices(eyeVertex, emissionVertex, getScene(), getSppCount(), mis);
        }
        auto& lightPath = m_LightPathBuffer(lightPathDepth - 1, lightVertexSample.value);
        if(lightPath.m_fPathPdf > 0.f) {
            return connectVertices(eyeVertex, lightPath, getScene(), getSppCount(), mis);
        }
        return zero<Vec3f>();
    }();
    auto rcpPdf = 1.f / lightVertexSample.pdf;
    return resamplingMISWeight * rcpPdf * (1.f / m_nPerNodeLightPathCount) * m_fImportanceScale * connexionContrib;
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::connectLightVerticesToCamera(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
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

        auto contrib = m_fRadianceScale * connectVertices(*pLightVertex,
                                       SensorVertex(&sensor, directImportanceSample.m_LensSample),
                                       getScene(), m_nLightPathCount, mis);

        accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0.f));
        accumulate(FINAL_RENDER_DEPTH1 + totalDepth - 1u, pixelID, Vec4f(contrib, 0.f));

        auto strategyOffset = computeBPTStrategyOffset(totalDepth + 1, pLightVertex->m_nDepth + 1);
        accumulate(FINAL_RENDER_DEPTH1 + m_nMaxDepth + strategyOffset, pixelID, Vec4f(contrib, 0));
    }
}

size_t BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::skeletonMapping(const PathVertex& eyeVertex, GraphNodeIndex* nodeBuffer) const {
    if(m_nMaxNodeCount == 0u) {
        return 0u;
    }
    *nodeBuffer = m_pSkel->getNearestNode(eyeVertex.m_Intersection, eyeVertex.m_BSDF.getIncidentDirection());
    if(*nodeBuffer == UNDEFINED_NODE) {
        return 0u;
    }

    if(m_nMaxNodeCount > 1) {
        auto i = 1u;
        for(auto neighbourNode: m_pSkel->neighbours(*nodeBuffer)) {
            nodeBuffer[i] = neighbourNode;
            ++i;
            if(i == m_nMaxNodeCount) {
                break;
            }
        }
        return i;
    }

    return 1u;
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxDepth));
    //gui.addVarRW(BNZ_GUI_VAR(m_nLightPathCount));
    gui.addVarRW(BNZ_GUI_VAR(m_nPerNodeLightPathCount));
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxNodeCount));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseCombinedResampling));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseNodeRadianceWeight));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseNodeDistanceWeight));
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "perNodeLightPathCount", m_nPerNodeLightPathCount);
    serialize(xml, "maxNodeCount", m_nMaxNodeCount);
    serialize(xml, "useCombinedResampling", m_bUseCombinedResampling);
    serialize(xml, "useNodeRadianceWeight", m_bUseNodeRadianceWeight);
    serialize(xml, "useNodeDistanceWeight", m_bUseNodeDistanceWeight);
    serialize(xml, "nodeFilteringFactor", m_fNodeFilteringFactor);
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "perNodeLightPathCount", m_nPerNodeLightPathCount);
    serialize(xml, "maxNodeCount", m_nMaxNodeCount);
    serialize(xml, "useCombinedResampling", m_bUseCombinedResampling);
    serialize(xml, "useNodeRadianceWeight", m_bUseNodeRadianceWeight);
    serialize(xml, "useNodeDistanceWeight", m_bUseNodeDistanceWeight);
    serialize(xml, "nodeFilteringFactor", m_fNodeFilteringFactor);
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::doStoreStatistics() const {
    if(auto pStats = getStatisticsOutput()) {
        serialize(*pStats, "BeginFrameTime", m_BeginFrameTimer);
        serialize(*pStats, "TileProcessingTime", (const TaskTimer&) m_TileProcessingTimer);

        uint64_t sum;
        double mean, variance;
        computeNodeMappingStatistics(sum, mean, variance);

        auto numberOfNodeWithZeroEyeVertex = std::count_if(
                    begin(m_EyeVertexCountPerNode), end(m_EyeVertexCountPerNode),
                    [&](uint64_t value) {
                        return !value;
                    });

        serialize(*pStats, "NumberOfMappedEyeVertex", (const size_t&) sum);
        serialize(*pStats, "MeanOfMappedEyeVertex", (const double&) mean);
        serialize(*pStats, "VarianceOfMappedEyeVertex", (const double&) variance);
        serialize(*pStats, "StdDevOfMappedEyeVertex", (const double&) sqrt(variance));
        serialize(*pStats, "NumberOfNodeWithZeroEyeVertex", (const size_t&) numberOfNodeWithZeroEyeVertex);
        serialize(*pStats, "PercentageOfNodeWithZeroEyeVertex", 100.0 * numberOfNodeWithZeroEyeVertex / m_pSkel->size());

        serialize(*pStats, "MappedEyeVertexRequired", m_fNodeFilteringFactor * mean);

        std::vector<GraphNodeIndex> filteredNodes;
        std::vector<std::size_t> filteredNodeIndex(m_pSkel->size());

        computeFilteredNodesList(mean, filteredNodes, filteredNodeIndex);

        serialize(*pStats, "NumberOfFilteredNodesLastFrame", filteredNodes.size());
        serialize(*pStats, "PercentageOfFilteredNodesLastFrame", 100. * filteredNodes.size() / m_pSkel->size());
        serialize(*pStats, "NumberOfRejectedNodeLastFrame", m_pSkel->size() - filteredNodes.size());
        serialize(*pStats, "PercentageOfRejectedNodesLastFrame", 100. * (m_pSkel->size() - filteredNodes.size()) / m_pSkel->size());

        sum = 0;
        for(auto nodeID: filteredNodes) {
            auto value = m_EyeVertexCountPerNode[nodeID];
            sum += value;
            variance += sqr(value);
        }
        mean = double(sum) / size(filteredNodes);
        variance = variance / size(filteredNodes) - sqr(mean);

        serialize(*pStats, "MeanOfMappedEyeVertexForFilteredNodes", (const double&) mean);
        serialize(*pStats, "VarianceOfMappedEyeVertexForFilteredNodes", (const double&) variance);
        serialize(*pStats, "StdDevOfMappedEyeVertexForFilteredNodes", (const double&) sqrt(variance));

        serialize(*pStats, "MeanOfUseNodePerFrame", m_nTotalFilteredNodeCount / getIterationCount());
    }
}

void BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    addFramebufferChannel("node0");
    addFramebufferChannel("node1");
    addFramebufferChannel("node2");
    addFramebufferChannel("node3");
    addFramebufferChannel("dist0_contrib");
    addFramebufferChannel("dist1_contrib");
    addFramebufferChannel("dist2_contrib");
    addFramebufferChannel("dist3_contrib");
    addFramebufferChannel("unmapped_contrib");
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
