#include "GeorgievImportanceRecordContainer.hpp"

namespace BnZ {

void GeorgievImportanceRecordContainer::buildImportanceRecordsKdTree() {
    m_ImportanceRecordsKdTree.build(size(), [&](uint32_t idx) {
        return (*this)[idx].m_Intersection.P;
    },
    [&](uint32_t idx) {
        return (*this)[idx].m_fRegionOfInfluenceRadius > 0.f;
    });
}

std::size_t GeorgievImportanceRecordContainer::getNearestImportanceRecords(
        const SurfacePoint& point,
        std::size_t unfilteredIRCount,
        NearestUnfilteredImportanceRecord* pUnfilteredIRBuffer,
        std::size_t filteredIRCount,
        std::uint32_t* pIRIndexBuffer,
        float orientationTradeOff) const {
    // Two step filtering approach to select the IRs
    auto irCount = std::size_t(0);

    // The predicate only choose nearest IR with the same normal orientation
    // However sometimes it produces 0 matchs
//    auto predicate = [&](uint32_t irIdx) {
//        return dot(point.Ns, (*this)[irIdx].m_Intersection.Ns) > 0.f;
//    };

    // First select IR according to distance
    m_ImportanceRecordsKdTree.searchKNearestNeighbours(
                point.P, unfilteredIRCount,
                [&](uint32_t irIdx, const Vec3f& irPosition, float distSquaredToIR) {
                    auto orientationTerm = abs(1 - dot(point.Ns, (*this)[irIdx].m_Intersection.Ns));
                    auto metric = sqrt(distSquaredToIR) + orientationTradeOff * sqrt(orientationTerm);
                    pUnfilteredIRBuffer[irCount] = { irIdx, metric };
                    ++irCount;
                }
    );

    // Then choose the ones that minimize distance + m_fImportanceRecordOrientationTradeOff * sqrt(1 - dot(I.Ns, normal))
    std::sort(pUnfilteredIRBuffer, pUnfilteredIRBuffer + irCount);

    irCount = min(irCount, filteredIRCount);
    for(auto i: range(irCount)) {
        pIRIndexBuffer[i] = pUnfilteredIRBuffer[i].m_nIndex;
    }

    return irCount;
}

}
