#include "GeorgievImportanceCache.hpp"

#include <bonez/maths/maths.hpp>

namespace BnZ {

void GeorgievImportanceCache::setEnabledDistributions(const std::string& distributionSelector) {
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
            std::cerr << "GeorgievImportanceCache::extractEnabledDistributionsFromSelector : Unreconized "
                         "distribution " << std::string({ distributionSelector[i] }) << std::endl;
            break;
        }
    }
}

void GeorgievImportanceCache::optimizeAlphaDistributions(std::size_t importanceRecordID) {
    auto computeAlphaMaxHeuristic = [&](uint32_t distribIndex, uint32_t vertexIndex) {
        Vec4f pdfs = zero<Vec4f>();
        float maxAlphaPdf = 0.f;
        for(auto j = 0u; j < m_nEnabledDistributionCount; ++j) {
            pdfs[j] = pdfDiscreteDistribution1D(m_EnabledDistributions[j]->getSlicePtr(importanceRecordID), vertexIndex);
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
        buildDistribution1D([&](uint32_t i) {
            return computeAlphaMaxHeuristic(distribIndex, i) * pdfDiscreteDistribution1D(m_EnabledDistributions[distribIndex]->getSlicePtr(importanceRecordID), i);
        }, m_EnabledDistributions[distribIndex]->getSlicePtr(importanceRecordID), m_nVertexCount);
    }
}

Sample1u GeorgievImportanceCache::sampleDistribution(
        std::size_t enabledDistributionIndex,
        std::size_t importanceRecordCount,
        const uint32_t* pImportanceRecordIDs,
        float s1D) const {
    return sampleCombinedDiscreteDistribution(importanceRecordCount, [&](uint32_t idx) {
        return m_EnabledDistributions[enabledDistributionIndex]->getSlicePtr(pImportanceRecordIDs[idx]);
    }, m_nVertexCount, s1D);
}

float GeorgievImportanceCache::misAlphaMaxHeuristic(const Sample1u& vertexSample,
                           std::size_t distribIndex,
                           std::size_t importanceRecordCount,
                           const uint32_t* pImportanceRecordIDs) const {
    Vec4f pdfs = zero<Vec4f>();
    Vec4f alphaPdfs = zero<Vec4f>();
    float maxAlphaPdf = 0.f;
    for(auto j = 0u; j < m_nEnabledDistributionCount; ++j) {
        auto getDistrib = [&](uint32_t idx) {
            return m_EnabledDistributions[j]->getSlicePtr(pImportanceRecordIDs[idx]);
        };
        pdfs[j] = pdfCombinedDiscreteDistribution1D(importanceRecordCount, getDistrib, vertexSample.value);;
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

float GeorgievImportanceCache::misBalanceHeuristic(const Sample1u& vertexSample,
                          std::size_t distribIndex,
                          std::size_t importanceRecordCount,
                          const uint32_t* pImportanceRecordIDs) const {
    auto rcpWeight = 0.f;
    auto rcpPdf = 1.f / vertexSample.pdf;
    for(auto j = 0u; j < m_nEnabledDistributionCount; ++j) {
        auto getDistrib = [&](uint32_t idx) {
            return m_EnabledDistributions[j]->getSlicePtr(pImportanceRecordIDs[idx]);
        };

        rcpWeight += rcpPdf * pdfCombinedDiscreteDistribution1D(importanceRecordCount, getDistrib, vertexSample.value);
    }
    return 1.f / rcpWeight;
}

}
