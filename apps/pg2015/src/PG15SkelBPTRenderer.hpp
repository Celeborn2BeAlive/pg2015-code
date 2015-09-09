#pragma once

#include "PG15Renderer.hpp"

#include <bonez/rendering/renderers/skeleton_connection/SkeletonVisibilityDistributions.hpp>

namespace BnZ {

struct PG15SkelBPTSettings {
    bool useNodeRadianceWeight = true;
    bool useNodeDistanceWeight = true;
    float nodeFilteringFactor = 0.5f;
};

class PG15SkelBPTRenderer: public PG15Renderer {
public:
    PG15SkelBPTRenderer(
            const PG15RendererParams& params,
            const PG15SharedData& sharedBuffers,
            const PG15SkelBPTSettings& settings):
        PG15Renderer(params, sharedBuffers),
        m_pSkel(getScene().getCurvSkeleton()),
        m_fRadianceScale(1.f / getLightPathCount()),
        m_bUseNodeRadianceWeight(settings.useNodeRadianceWeight),
        m_bUseNodeDistanceWeight(settings.useNodeDistanceWeight),
        m_fNodeFilteringFactor(settings.nodeFilteringFactor) {
        initFramebuffer();

        m_EyeVertexCountPerNode.resize(m_pSkel->size());
        std::fill(begin(m_EyeVertexCountPerNode), end(m_EyeVertexCountPerNode), 0u);

        m_EyeVertexCountPerNodePerThread.resize(m_pSkel->size(), getSystemThreadCount());
        m_FilteredNodeIndex.resize(m_pSkel->size());
    }

    void render() {
        {
            auto timer = m_BuildSkelDistributionsTimer.start(0u);

            computeFilteredNodes();
            buildDistributions(m_SkeletonVisibilityDistributions, getScene(), m_FilteredNodes, getEmissionVertexBufferPtr(),
                               getLightVertexBuffer(), getResamplingLightPathCount(),
                               m_bUseNodeRadianceWeight, m_bUseNodeDistanceWeight, getSystemThreadCount());
        }

        std::fill(begin(m_EyeVertexCountPerNodePerThread), end(m_EyeVertexCountPerNodePerThread), 0u);

        processTiles([&](uint32_t threadID, uint32_t tileID, const Vec4u& viewport) {
            renderTile(threadID, tileID, viewport);
        });

        ++m_nIterationCount;
    }

    void computeFilteredNodes() {
        updateEyeVertexCountPerNode();

        uint64_t sum;
        double mean, variance;
        computeNodeMappingStatistics(sum, mean, variance);

        computeFilteredNodesList(mean, m_FilteredNodes, m_FilteredNodeIndex);

        m_nTotalFilteredNodeCount += m_FilteredNodes.size();
    }

    void updateEyeVertexCountPerNode() {
        // Count the number of eye vertices mapped to each node
        for(auto nodeID: range(m_pSkel->size())) {
            auto count = size_t(0);
            for(auto threadID: range(getSystemThreadCount())) {
                count += m_EyeVertexCountPerNodePerThread(nodeID, threadID);
            }
            m_EyeVertexCountPerNode[nodeID] += count;
        }
    }

    void computeNodeMappingStatistics(
            uint64_t& sum, double& mean, double& variance) const {
        sum = 0;
        for(auto value: m_EyeVertexCountPerNode) {
            sum += value;
            variance += sqr(value);
        }
        mean = double(sum) / size(m_EyeVertexCountPerNode);
        variance = variance / size(m_EyeVertexCountPerNode) - sqr(mean);
    }

    void computeFilteredNodesList(
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
        auto traceEyePathTimer = m_TileProcessingTimer.start(0, threadID);

        auto maxEyePathDepth = getMaxEyePathDepth();
        auto maxLightPathDepth = getMaxLightPathDepth();

        auto mis = [&](float v) {
            return Mis(v);
        };

        auto rng = getThreadRNG(threadID);

        PixelSensor pixelSensor(getSensor(), Vec2u(x, y), getFramebufferSize());

        auto pixelSample = getFloat2(threadID);
        auto sensorSample = getFloat2(threadID);

        auto fImportanceScale = 1.f;
        BDPTPathVertex eyeVertex(getScene(), pixelSensor, sensorSample, pixelSample, getLightPathCount(), mis);

        traceEyePathTimer.storeDuration();

        if(eyeVertex.m_Power == zero<Vec3f>() || !eyeVertex.m_fPathPdf) {
            return;
        }

        auto extendEyePath = [&]() -> bool {
            auto traceEyePathTimer = m_TileProcessingTimer.start(0, threadID);
            return eyeVertex.m_nDepth < maxEyePathDepth && eyeVertex.extend(eyeVertex, getScene(), getResamplingLightPathCount(), false, rng, mis);
        };

        // Iterate over an eye path
        do {
            // Intersection with light source
            auto totalLength = eyeVertex.m_nDepth;
            if(totalLength <= getMaxDepth()) {
                auto evalContribTimer = m_TileProcessingTimer.start(1, threadID);

                auto contrib = fImportanceScale * computeEmittedRadiance(eyeVertex, getScene(), getLightSampler(), 1, mis);

                evalContribTimer.storeDuration();

                accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, 0u);
                accumulate(FINAL_RENDER_DEPTH1 + getMaxDepth() + strategyOffset, pixelID, Vec4f(contrib, 0));
            }

            // Connections
            if(eyeVertex.m_Intersection) {
                auto mappingTimer = m_TileProcessingTimer.start(2, threadID);

                auto sampledNode = skeletonMapping(eyeVertex);

                // Output selected mapped nodes on framebuffer
                if(eyeVertex.m_nDepth == 1u) {
                    accumulate(MAPPED_NODE, pixelID, Vec4f(getColor(sampledNode), 0));
                }

                std::size_t filteredNodeIndex = std::numeric_limits<std::size_t>::max();
                if(sampledNode != UNDEFINED_NODE) {
                    ++m_EyeVertexCountPerNodePerThread(sampledNode, threadID);
                    filteredNodeIndex = m_FilteredNodeIndex[sampledNode];
                }

                mappingTimer.storeDuration();

                // Connection with a light vertex of each depth
                for(auto j = 0u; j <= maxLightPathDepth; ++j) {
                    auto lightPathDepth = j;
                    auto totalLength = eyeVertex.m_nDepth + lightPathDepth + 1;

                    if(totalLength <= getMaxDepth()) {
                        if(filteredNodeIndex == std::numeric_limits<std::size_t>::max()) {
                            auto lightVertexSample = m_SkeletonVisibilityDistributions.sampleDefaultDistribution(lightPathDepth, getFloat(threadID));
                            if(lightVertexSample.pdf > 0.f) {
                                auto evalContribTimer = m_TileProcessingTimer.start(1, threadID);

                                auto contrib = evalLightVertexContrib(eyeVertex, lightPathDepth, lightVertexSample);

                                evalContribTimer.storeDuration();

                                accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                                accumulate(DEFAULT_DISTRIB_CONTRIBUTION, pixelID, Vec4f(contrib, 0));
                                accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                                auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, lightPathDepth + 1);
                                accumulate(FINAL_RENDER_DEPTH1 + getMaxDepth() + strategyOffset, pixelID, Vec4f(contrib, 0));
                            }
                        } else {
                            auto resamplingTimer = m_TileProcessingTimer.start(3, threadID);

                            auto distribIdx = clamp(size_t(getFloat(threadID) * m_SkeletonVisibilityDistributions.distributionCount()), size_t(0),
                                                    m_SkeletonVisibilityDistributions.distributionCount() - 1);
                            auto distribPdf = 1.f / m_SkeletonVisibilityDistributions.distributionCount();

                            auto lightPathDiscreteSample = m_SkeletonVisibilityDistributions.sample(distribIdx, filteredNodeIndex,
                                                                                                    lightPathDepth, getFloat(threadID));
                            lightPathDiscreteSample.pdf *= distribPdf;

                            resamplingTimer.storeDuration();

                            if(lightPathDiscreteSample.pdf > 0.f) {
                                auto evalContribTimer = m_TileProcessingTimer.start(1, threadID);

                                auto contrib = evalLightVertexContrib(eyeVertex, lightPathDepth, lightPathDiscreteSample);

                                evalContribTimer.storeDuration();

                                accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                                accumulate(DISTRIB0_CONTRIBUTION + distribIdx, pixelID, Vec4f(contrib, 0));
                                accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));

                                auto strategyOffset = computeBPTStrategyOffset(totalLength + 1, lightPathDepth + 1);
                                accumulate(FINAL_RENDER_DEPTH1 + getMaxDepth() + strategyOffset, pixelID, Vec4f(contrib, 0));
                            }
                        }
                    }
                }
            }
        } while(extendEyePath());
    }

    Vec3f evalLightVertexContrib(
            const BDPTPathVertex& eyeVertex, std::size_t lightPathDepth, const Sample1u& lightVertexSample) const {
        auto mis = [&](float v) {
            return Mis(v);
        };

        auto connexionContrib = [&]() {
            if(!lightPathDepth) {
                const auto& emissionVertex = getEmissionVertex(lightVertexSample.value);
                if(!emissionVertex.m_pLight || !emissionVertex.m_fLightPdf) {
                    return zero<Vec3f>();
                }
                return connectVertices(eyeVertex, emissionVertex, getScene(), getResamplingLightPathCount(), mis);
            }
            auto& lightPath = getLightVertexBuffer()(lightPathDepth - 1, lightVertexSample.value);
            if(lightPath.m_fPathPdf > 0.f) {
                return connectVertices(eyeVertex, lightPath, getScene(), getResamplingLightPathCount(), mis);
            }
            return zero<Vec3f>();
        }();
        auto rcpPdf = 1.f / lightVertexSample.pdf;
        return rcpPdf * (1.f / getResamplingLightPathCount()) * m_fImportanceScale * connexionContrib;
    }

    GraphNodeIndex skeletonMapping(const BDPTPathVertex& eyeVertex) const {
        return m_pSkel->getNearestNode(eyeVertex.m_Intersection, eyeVertex.m_BSDF.getIncidentDirection());
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
                                           getScene(), getLightPathCount(), mis);

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

    void initFramebuffer() {
        BPT_STRATEGY_s0_t2 = FINAL_RENDER_DEPTH1 + getMaxDepth();

        addFramebufferChannel("final_render");
        addFramebufferChannel("mapped_node");
        addFramebufferChannel("dist0_contrib");
        addFramebufferChannel("dist1_contrib");
        addFramebufferChannel("dist2_contrib");
        addFramebufferChannel("dist3_contrib");
        addFramebufferChannel("unmapped_contrib");
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

    void storeSettings(tinyxml2::XMLElement& xml) const {
        serialize(xml, "maxDepth", getMaxDepth());
        serialize(xml, "resamplingLightPathCount", getResamplingLightPathCount());
        serialize(xml, "useNodeRadianceWeight", m_bUseNodeRadianceWeight);
        serialize(xml, "useNodeDistanceWeight", m_bUseNodeDistanceWeight);
        serialize(xml, "nodeFilteringFactor", m_fNodeFilteringFactor);
    }

    void storeStatistics(tinyxml2::XMLElement& xml) const {
        serialize(xml, "BeginFrameTime", m_BuildSkelDistributionsTimer);
        serialize(xml, "TileProcessingTime", (const TaskTimer&) m_TileProcessingTimer);

        uint64_t sum;
        double mean, variance;
        computeNodeMappingStatistics(sum, mean, variance);

        auto numberOfNodeWithZeroEyeVertex = std::count_if(
                    begin(m_EyeVertexCountPerNode), end(m_EyeVertexCountPerNode),
                    [&](uint64_t value) {
                        return !value;
                    });

        serialize(xml, "NumberOfMappedEyeVertex", (const size_t&) sum);
        serialize(xml, "MeanOfMappedEyeVertex", (const double&) mean);
        serialize(xml, "VarianceOfMappedEyeVertex", (const double&) variance);
        serialize(xml, "StdDevOfMappedEyeVertex", (const double&) sqrt(variance));
        serialize(xml, "NumberOfNodeWithZeroEyeVertex", (const size_t&) numberOfNodeWithZeroEyeVertex);
        serialize(xml, "PercentageOfNodeWithZeroEyeVertex", 100.0 * numberOfNodeWithZeroEyeVertex / m_pSkel->size());

        serialize(xml, "MappedEyeVertexRequired", m_fNodeFilteringFactor * mean);

        std::vector<GraphNodeIndex> filteredNodes;
        std::vector<std::size_t> filteredNodeIndex(m_pSkel->size());

        computeFilteredNodesList(mean, filteredNodes, filteredNodeIndex);

        serialize(xml, "NumberOfFilteredNodesLastFrame", filteredNodes.size());
        serialize(xml, "PercentageOfFilteredNodesLastFrame", 100. * filteredNodes.size() / m_pSkel->size());
        serialize(xml, "NumberOfRejectedNodeLastFrame", m_pSkel->size() - filteredNodes.size());
        serialize(xml, "PercentageOfRejectedNodesLastFrame", 100. * (m_pSkel->size() - filteredNodes.size()) / m_pSkel->size());

        sum = 0;
        for(auto nodeID: filteredNodes) {
            auto value = m_EyeVertexCountPerNode[nodeID];
            sum += value;
            variance += sqr(value);
        }
        mean = double(sum) / size(filteredNodes);
        variance = variance / size(filteredNodes) - sqr(mean);

        serialize(xml, "MeanOfMappedEyeVertexForFilteredNodes", (const double&) mean);
        serialize(xml, "VarianceOfMappedEyeVertexForFilteredNodes", (const double&) variance);
        serialize(xml, "StdDevOfMappedEyeVertexForFilteredNodes", (const double&) sqrt(variance));

        serialize(xml, "MeanOfUseNodePerFrame", m_nTotalFilteredNodeCount / getIterationCount());
    }

    Shared<const CurvilinearSkeleton> m_pSkel;

    SkeletonVisibilityDistributions m_SkeletonVisibilityDistributions;
    std::vector<GraphNodeIndex> m_FilteredNodes;
    std::vector<uint64_t> m_EyeVertexCountPerNode;
    std::vector<std::size_t> m_FilteredNodeIndex;
    uint64_t m_nTotalFilteredNodeCount = 0u;

    bool m_bUseNodeRadianceWeight = true;
    bool m_bUseNodeDistanceWeight = true;
    float m_fNodeFilteringFactor = 0.01f;

    float m_fImportanceScale = 1.f; // 1 / (number of eye path per pixel)
    float m_fRadianceScale = 0.f; // 1 / (number of light path per pixel)

    TaskTimer m_BuildSkelDistributionsTimer = {{ "BuildSkeletonDistributions" }, 1u };
    TaskTimer m_TileProcessingTimer = {
        {
            "TraceEyePaths",
            "EvalContribution",
            "SkeletonMapping",
            "ResampleLightVertex",
            "ConnectLightVerticesToSensor"
        }, getSystemThreadCount()};

    Array2d<uint64_t> m_EyeVertexCountPerNodePerThread;

    // Framebuffer targets
    enum FramebufferTarget {
        FINAL_RENDER,
        MAPPED_NODE,
        DISTRIB0_CONTRIBUTION,
        DISTRIB1_CONTRIBUTION,
        DISTRIB2_CONTRIBUTION,
        DISTRIB3_CONTRIBUTION,
        DEFAULT_DISTRIB_CONTRIBUTION,
        FINAL_RENDER_DEPTH1
    };
    std::size_t BPT_STRATEGY_s0_t2;
};

}
