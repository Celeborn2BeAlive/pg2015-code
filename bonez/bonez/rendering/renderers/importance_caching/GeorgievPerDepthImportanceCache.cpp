#include "GeorgievPerDepthImportanceCache.hpp"
#include <bonez/sys/threads.hpp>
#include <bonez/maths/maths.hpp>

namespace BnZ {

void GeorgievPerDepthImportanceCache::setEnabledDistributions(const std::string& distributionSelector) {
    std::fill(begin(m_EnabledDistributions), end(m_EnabledDistributions), nullptr);
    std::fill(begin(m_IsDistributionEnabled), end(m_IsDistributionEnabled), false);

    m_nEnabledDistributionCount = 0u;
    for(auto i: range(min(distributionSelector.size(), size(m_EnabledDistributions)))) {
        switch(distributionSelector[i]) {
        case 'F':
            if(!m_IsDistributionEnabled[DistributionIndex::F]) {
                m_EnabledDistributions[i] = &m_FullContributionDistribution;
                m_IsDistributionEnabled[DistributionIndex::F] = true;
                ++m_nEnabledDistributionCount;
            }
            break;
        case 'U':
            if(!m_IsDistributionEnabled[DistributionIndex::U]) {
                m_EnabledDistributions[i] = &m_UnoccludedContributionDistribution;
                m_IsDistributionEnabled[DistributionIndex::U] = true;
                ++m_nEnabledDistributionCount;
            }
            break;
        case 'B':
            if(!m_IsDistributionEnabled[DistributionIndex::B]) {
                m_EnabledDistributions[i] = &m_BoundedContributionDistribution;
                m_IsDistributionEnabled[DistributionIndex::B] = true;
                ++m_nEnabledDistributionCount;
            }
            break;
        case 'C':
            if(!m_IsDistributionEnabled[DistributionIndex::C]) {
                m_EnabledDistributions[i] = &m_ConservativeDistribution;
                m_IsDistributionEnabled[DistributionIndex::C] = true;
                ++m_nEnabledDistributionCount;
            }
            break;
        default:
            std::cerr << "GeorgievPerDepthImportanceCache::extractEnabledDistributionsFromSelector : Unreconized "
                         "distribution " << std::string({ distributionSelector[i] }) << std::endl;
            break;
        }
    }
}

void GeorgievPerDepthImportanceCache::optimizeAlphaDistributions(std::size_t importanceRecordID) {
    auto computeAlphaMaxHeuristic = [&](uint32_t distribIndex, uint32_t depth, uint32_t pathIdx) {
        Vec4f pdfs = zero<Vec4f>();
        float maxAlphaPdf = 0.f;
        for(auto j = 0u; j < m_nEnabledDistributionCount; ++j) {
            pdfs[j] = pdfDiscreteDistribution1D(m_EnabledDistributions[j]->getSlicePtr(importanceRecordID) + distributionOffset(depth), pathIdx);
            maxAlphaPdf = max(maxAlphaPdf, m_AlphaConfidenceFactors[j] * pdfs[j]);
        }

        if(pdfs[distribIndex] < maxAlphaPdf) {
            return 0.f;
        } else {
            for(auto j = 0u; j < distribIndex; ++j) {
                if(pdfs[j] >= maxAlphaPdf) {
                    return 0.f;
                }
            }
        }

        return 1.f;
    };

    for(auto distribIndex = 0u; distribIndex < m_nEnabledDistributionCount; ++distribIndex) {
        for(auto depth: range(m_nMaxDepth + 1)) {
            auto distribOffset = distributionOffset(depth);
            buildDistribution1D([&](uint32_t pathIdx) {
                return computeAlphaMaxHeuristic(distribIndex, depth, pathIdx) *
                        pdfDiscreteDistribution1D(m_EnabledDistributions[distribIndex]->getSlicePtr(importanceRecordID) + distribOffset, pathIdx);
            }, m_EnabledDistributions[distribIndex]->getSlicePtr(importanceRecordID) + distribOffset, m_nPathCount);
        }
    }
}

Sample1u GeorgievPerDepthImportanceCache::sampleDistribution(
        std::size_t depth,
        std::size_t enabledDistributionIndex,
        std::size_t importanceRecordCount,
        const uint32_t* pImportanceRecordIDs,
        float s1D) const {
    return sampleCombinedDiscreteDistribution(importanceRecordCount, [&](uint32_t idx) {
        return m_EnabledDistributions[enabledDistributionIndex]->getSlicePtr(pImportanceRecordIDs[idx]) + distributionOffset(depth);
    }, m_nPathCount, s1D);
}

float GeorgievPerDepthImportanceCache::misAlphaMaxHeuristic(
        const Sample1u& vertexSample,
        std::size_t depth,
        std::size_t distribIndex,
        std::size_t importanceRecordCount,
        const uint32_t* pImportanceRecordIDs) const {
    Vec4f pdfs = zero<Vec4f>();
    float maxAlphaPdf = 0.f;
    for(auto j = 0u; j < m_nEnabledDistributionCount; ++j) {
        auto getDistrib = [&](uint32_t idx) {
            return m_EnabledDistributions[j]->getSlicePtr(pImportanceRecordIDs[idx]) + distributionOffset(depth);
        };
        pdfs[j] = pdfCombinedDiscreteDistribution1D(importanceRecordCount, getDistrib, vertexSample.value);
        maxAlphaPdf = max(maxAlphaPdf, m_AlphaConfidenceFactors[j] * pdfs[j]);
    }

    if(vertexSample.pdf < maxAlphaPdf) {
        return 0.f;
    } else {
        for(auto j = 0u; j < distribIndex; ++j) {
            if(pdfs[j] >= maxAlphaPdf) {
                return 0.f;
            }
        }
    }

    return 1.f;
}

float GeorgievPerDepthImportanceCache::misBalanceHeuristic(
        const Sample1u& vertexSample,
        std::size_t depth,
        std::size_t distribIndex,
        std::size_t importanceRecordCount,
        const uint32_t* pImportanceRecordIDs) const {
    auto rcpWeight = 0.f;
    auto rcpPdf = 1.f / vertexSample.pdf;
    for(auto j = 0u; j < m_nEnabledDistributionCount; ++j) {
        auto getDistrib = [&](uint32_t idx) {
            return m_EnabledDistributions[j]->getSlicePtr(pImportanceRecordIDs[idx]) + distributionOffset(depth);
        };

        rcpWeight += rcpPdf * pdfCombinedDiscreteDistribution1D(importanceRecordCount, getDistrib, vertexSample.value);
    }
    return 1.f / rcpWeight;
}

}
