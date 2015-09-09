#pragma once

#include <bonez/types.hpp>
#include <bonez/common.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>
#include <bonez/sampling/distribution1d.h>

#include <array>

namespace BnZ {

/**
 * @brief The GeorgievImportanceCache class represents the distributions of importance for each Importance Record
 * built upon Virtual Point Light distributions.
 */
class GeorgievPerDepthImportanceCache {
public:
    struct DistributionIndex {
        enum {
            F,
            U,
            B,
            C
        };
    };

    void setEnabledDistributions(const std::string& distributionSelector);

    template<typename EvalContributionFunctor,
             typename EvalUnoccludedContributionFunctor,
             typename EvalBoundedContributionFunctor,
             typename EvalConservativeConstributionFunctor>
    void buildDistributions(std::size_t importanceRecordCount,
                            std::size_t pathCount,
                            std::size_t maxDepth,
                            EvalContributionFunctor&& evalContrib,
                            EvalUnoccludedContributionFunctor&& evalUnoccluded,
                            EvalBoundedContributionFunctor&& evalBounded,
                            EvalConservativeConstributionFunctor&& evalConservative,
                            const Vec4f& alphaConfidenceFactors,
                            std::size_t threadCount,
                            bool bOptimize) {
        m_AlphaConfidenceFactors = alphaConfidenceFactors;
        m_nPathCount = pathCount;
        m_nMaxDepth = maxDepth;
        m_nDistributionSize = getDistribution1DBufferSize(m_nPathCount); // Each individual distribution is composed of (at most) m_nPathCount vertices

        for(auto pDistribution: m_EnabledDistributions) {
            if(pDistribution) {
                pDistribution->resize(m_nDistributionSize * (m_nMaxDepth + 1), importanceRecordCount);
            }
        }

        processTasksDeterminist(importanceRecordCount, [&](uint32_t importanceRecordID, uint32_t threadID) {
            for(auto depth: range(maxDepth + 1)) {
                auto distribOffset = distributionOffset(depth);

                if(m_IsDistributionEnabled[DistributionIndex::F]) {
                    buildDistribution1D([&](uint32_t pathIdx) {
                        return evalContrib(pathIdx, depth, importanceRecordID);
                    }, m_FullContributionDistribution.getSlicePtr(importanceRecordID) + distribOffset, pathCount);
                }

                if(m_IsDistributionEnabled[DistributionIndex::U]) {
                    buildDistribution1D([&](uint32_t pathIdx) {
                        return evalUnoccluded(pathIdx, depth, importanceRecordID);
                    }, m_UnoccludedContributionDistribution.getSlicePtr(importanceRecordID) + distribOffset, pathCount);
                }

                if(m_IsDistributionEnabled[DistributionIndex::B]) {
                    float sum;
                    buildDistribution1D([&](uint32_t pathIdx) {
                        return evalBounded(pathIdx, depth, importanceRecordID);
                    }, m_BoundedContributionDistribution.getSlicePtr(importanceRecordID) + distribOffset, pathCount, &sum);
                }

                if(m_IsDistributionEnabled[DistributionIndex::C]) {
                    buildDistribution1D([&](uint32_t pathIdx) {
                        return evalConservative(pathIdx, depth, importanceRecordID);
                    }, m_ConservativeDistribution.getSlicePtr(importanceRecordID) + distribOffset, pathCount);
                }
            }

            if(bOptimize) {
                optimizeAlphaDistributions(importanceRecordID);
            }
        }, threadCount);
    }

    std::size_t getEnabledDistributionCount() const {
        return m_nEnabledDistributionCount;
    }

    float misAlphaMaxHeuristic(const Sample1u& vertexSample,
                               std::size_t depth,
                               std::size_t distribIndex,
                               std::size_t importanceRecordCount,
                               const uint32_t* pImportanceRecordIDs) const;

    float misBalanceHeuristic(const Sample1u& vertexSample,
                              std::size_t depth,
                              std::size_t distribIndex,
                              std::size_t importanceRecordCount,
                              const uint32_t* pImportanceRecordIDs) const;

    Sample1u sampleDistribution(std::size_t depth,
                                std::size_t enabledDistributionIndex,
                                std::size_t importanceRecordCount,
                                const uint32_t* pImportanceRecordIDs,
                                float s1D) const;

private:
    void optimizeAlphaDistributions(std::size_t importanceRecordID);

    size_t distributionOffset(std::size_t depth) const {
        return m_nDistributionSize * depth;
    }

    std::size_t m_nDistributionSize = 0u;
    std::size_t m_nPathCount = 0u;
    std::size_t m_nMaxDepth = 0u;

    Array2d<float> m_FullContributionDistribution; // "F"
    Array2d<float> m_UnoccludedContributionDistribution; // "U"
    Array2d<float> m_BoundedContributionDistribution; // "B"
    Array2d<float> m_ConservativeDistribution; // "C"

    std::array<int, 4u> m_IsDistributionEnabled = {{ true, true, true ,true }};
    std::array<Array2d<float>*, 4u> m_EnabledDistributions;
    std::size_t m_nEnabledDistributionCount = 0u;

    Vec4f m_AlphaConfidenceFactors = Vec4f(1.f);
};

}
