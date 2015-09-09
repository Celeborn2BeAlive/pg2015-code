#pragma once

#include <bonez/utils/itertools/itertools.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>
#include <bonez/scene/topology/CurvilinearSkeleton.hpp>
#include <bonez/sampling/distribution1d.h>

#include "../recursive_mis_bdpt.hpp"

namespace BnZ {

class SkeletonVisibilityDistributions;

void buildDistributions(
        SkeletonVisibilityDistributions& distributions,
        const Scene& scene,
        const std::vector<GraphNodeIndex>& skeletonNodes,
        const EmissionVertex* pEmissionVertexArray,
        const Array2d<BDPTPathVertex>& surfaceLightVertexArray,
        std::size_t lightPathCount,
        bool useNodeRadianceScale,
        bool useNodeDistanceScale,
        std::size_t threadCount);

void buildDistributions(
        SkeletonVisibilityDistributions& distributions,
        const Scene& scene,
        const std::vector<GraphNodeIndex>& skeletonNodes,
        const SurfacePointSample* surfacePointSamples,
        std::size_t surfacePointSampleCount,
        bool useNodeDistanceScale,
        std::size_t threadCount);

class SkeletonVisibilityDistributions {
public:
    template<typename EvalDefaultConservativeWeightFunctor,
             typename... EvalNodeWeightFunctors>
    void buildDistributions(const CurvilinearSkeleton& skel,
                            const std::vector<GraphNodeIndex>& skeletonNodes,
                            std::size_t pathCount,
                            std::size_t maxDepth,
                            std::size_t threadCount,
                            EvalDefaultConservativeWeightFunctor&& evalDefaultConservativeWeight,
                            EvalNodeWeightFunctors&&... evalNodeWeightFunctors) {
        init(pathCount, maxDepth, sizeof...(evalNodeWeightFunctors), evalDefaultConservativeWeight);

        buildNodeDistributions(0, maxDepth, threadCount, skel, skeletonNodes, evalNodeWeightFunctors...);

        optimizeDistributions(maxDepth, skeletonNodes.size(), threadCount);
    }

    Sample1u sample(std::size_t distribIdx, GraphNodeIndex nodeIdx,
                    std::size_t depth, float lightVertexSample) const;

    Sample1u sampleCombined(std::size_t distribIdx, GraphNodeIndex* pNodeIndexBuffer, std::size_t nodeCount,
                            std::size_t depth, float lightVertexSample) const;

    Sample1u sampleDefaultDistribution(std::size_t depth, float lightVertexSample) const;

    float evalResamplingMISWeight(std::size_t distribIdx, GraphNodeIndex nodeIdx, std::size_t depth,
                                  const Sample1u& lightVertexSample) const;

    float evalCombinedResamplingMISWeight(std::size_t distribIdx, GraphNodeIndex* pNodeIndexBuffer, std::size_t nodeCount, std::size_t depth,
                                          const Sample1u& lightVertexSample) const;

    std::size_t distributionCount() const {
        return m_PerDepthPerNodeDistributions.size();
    }

private:
    template<typename EvalDefaultConservativeWeightFunctor>
    void init(std::size_t pathCount,
              std::size_t maxDepth,
              std::size_t distributionCount,
              EvalDefaultConservativeWeightFunctor&& evalDefaultConservativeWeight) {
        m_nLightPathCount = pathCount;
        m_nDistributionSize = getDistribution1DBufferSize(pathCount);

        // BUILD DEFAULT DISTRIBUTIONS
        m_PerDepthDefaulConservativeDistributionsArray.resize(m_nDistributionSize, 1 + maxDepth);
        // Build the default distribution for each light sub-path depth
        for(auto depth : range(maxDepth + 1)) {
            buildDistribution1D([&](uint32_t pathIdx) {
                return evalDefaultConservativeWeight(depth, pathIdx);
            }, m_PerDepthDefaulConservativeDistributionsArray.getSlicePtr(depth), pathCount);
        }

        m_PerDepthPerNodeDistributions.resize(distributionCount);
    }

    template<typename EvalWeightFunctor>
    void buildNodeDistribution(
            std::size_t distributionIndex,
            std::size_t maxDepth,
            const CurvilinearSkeleton& skel,
            const std::vector<GraphNodeIndex>& skeletonNodes,
            EvalWeightFunctor&& evalWeight,
            std::size_t threadCount) {
        m_PerDepthPerNodeDistributions[distributionIndex].resize(m_nDistributionSize, 1 + maxDepth, skeletonNodes.size());
        processTasksDeterminist(skeletonNodes.size(), [&](uint32_t indirectNodeIndex, uint32_t threadID) {
            auto nodeIndex = skeletonNodes[indirectNodeIndex];
            auto nodePos = skel.getNode(nodeIndex).P;
            auto nodeRadius = skel.getNode(nodeIndex).maxball;

            for(auto depth : range(maxDepth + 1)) {
                buildDistribution1D([&](uint32_t pathIdx) {
                    return evalWeight(depth, pathIdx, nodePos, nodeRadius);
                }, m_PerDepthPerNodeDistributions[distributionIndex].getSlicePtr(depth, indirectNodeIndex), m_nLightPathCount);
            }
        }, threadCount);
    }

    void buildNodeDistributions(
            std::size_t distributionIndex,
            std::size_t maxDepth,
            std::size_t threadCount,
            const CurvilinearSkeleton& skel,
            const std::vector<GraphNodeIndex>& skeletonNodes) {
        // do nothing
    }

    template<typename EvalWeightFunctor, typename... EvalNodeWeightFunctors>
    void buildNodeDistributions(
            std::size_t distributionIndex,
            std::size_t maxDepth,
            std::size_t threadCount,
            const CurvilinearSkeleton& skel,
            const std::vector<GraphNodeIndex>& skeletonNodes,
            EvalWeightFunctor&& evalWeight,
            EvalNodeWeightFunctors&&... evalWeights) {
        buildNodeDistribution(distributionIndex, maxDepth, skel, skeletonNodes, evalWeight, threadCount);
        buildNodeDistributions(distributionIndex + 1, maxDepth, threadCount, skel, skeletonNodes, evalWeights...);
    }

    void optimizeDistributions(
            std::size_t maxDepth,
            std::size_t skeletonNodeCount,
            std::size_t threadCount) {
        processTasksDeterminist(skeletonNodeCount, [&](uint32_t indirectNodeIndex, uint32_t threadID) {
            for(auto distribIndex : range(m_PerDepthPerNodeDistributions.size())) {
                for(auto depth: range(maxDepth + 1)) {
                    buildDistribution1D([&](uint32_t pathIdx) {
                        auto pdf = pdfDiscreteDistribution1D(m_PerDepthPerNodeDistributions[distribIndex].getSlicePtr(depth, indirectNodeIndex), pathIdx);
                        Sample1u lightVertexSample(pathIdx, pdf);
                        return evalResamplingMISWeight(distribIndex, indirectNodeIndex, depth, lightVertexSample) * pdf;
                    }, m_PerDepthPerNodeDistributions[distribIndex].getSlicePtr(depth, indirectNodeIndex), m_nLightPathCount);
                }
            }
        }, threadCount);
    }


    std::size_t m_nLightPathCount = 0u;

    std::vector<Array3d<float>> m_PerDepthPerNodeDistributions;
    Array2d<float> m_PerDepthDefaulConservativeDistributionsArray;
    std::size_t m_nDistributionSize; // Size of a distribution for a given depth and a given node
};

}
