#include "ThinningProcessDGCI2013_2.hpp"
#include "discrete_functions.hpp"

#include <bonez/sys/threads.hpp>
#include <bonez/sys/time.hpp>

namespace BnZ {

// Initialize the thinning process of a cubical complex
void ThinningProcessDGCI2013_2::init(CubicalComplex3D& cc,
                                   const Grid3D<uint32_t>* pDistanceMap,
                                   const Grid3D<uint32_t>* pOpeningMap,
                                   const CubicalComplex3D* pConstrainedCC) {
    m_pCC = &cc;
    m_pDistanceMap = pDistanceMap;
    m_pOpeningMap = pOpeningMap;
    m_pConstrainedCC = pConstrainedCC;
    m_nWidth = cc.width();
    m_nHeight = cc.height();
    m_nDepth = cc.depth();

    m_nIterationCount = 0u;

    if(m_pDistanceMap && m_pOpeningMap) {
        m_BirthMap = Grid3D<Vec3i>(cc.width(), cc.height(), cc.depth(), Vec3i(-1));
        m_EdgeDistanceMap = Grid3D<Vec3i>(cc.width(), cc.height(), cc.depth(), Vec3i(0));
        m_EdgeOpeningMap = Grid3D<Vec3i>(cc.width(), cc.height(), cc.depth(), Vec3i(0));

        foreachVoxel(cc.resolution(), [&](const Vec3i& voxel) {
            if(cc(voxel).containsSome(CC3DFaceBits::XEDGE)) {
                auto maxDist = (*m_pDistanceMap)(voxel);
                auto maxOpening = (*m_pOpeningMap)(voxel);

                if(voxel.y > 0) {
                    maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x, voxel.y - 1, voxel.z)));
                    maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x, voxel.y - 1, voxel.z)));

                    if(voxel.z > 0) {
                        maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x, voxel.y - 1, voxel.z - 1)));
                        maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x, voxel.y - 1, voxel.z - 1)));
                    }
                }

                if(voxel.z > 0) {
                    maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x, voxel.y, voxel.z - 1)));
                    maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x, voxel.y, voxel.z - 1)));
                }

                m_EdgeDistanceMap(voxel)[XEDGE_IDX] = maxDist;
                m_EdgeOpeningMap(voxel)[XEDGE_IDX] = maxOpening;
            }
            if(cc(voxel).containsSome(CC3DFaceBits::YEDGE)) {
                auto maxDist = (*m_pDistanceMap)(voxel);
                auto maxOpening = (*m_pOpeningMap)(voxel);

                if(voxel.x > 0) {
                    maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x - 1, voxel.y, voxel.z)));
                    maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x - 1, voxel.y, voxel.z)));

                    if(voxel.z > 0) {
                        maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x - 1, voxel.y, voxel.z - 1)));
                        maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x - 1, voxel.y, voxel.z - 1)));
                    }
                }

                if(voxel.z > 0) {
                    maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x, voxel.y, voxel.z - 1)));
                    maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x, voxel.y, voxel.z - 1)));
                }

                m_EdgeDistanceMap(voxel)[YEDGE_IDX] = maxDist;
                m_EdgeOpeningMap(voxel)[YEDGE_IDX] = maxOpening;
            }
            if(cc(voxel).containsSome(CC3DFaceBits::ZEDGE)) {
                auto maxDist = (*m_pDistanceMap)(voxel);
                auto maxOpening = (*m_pOpeningMap)(voxel);

                if(voxel.x > 0) {
                    maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x - 1, voxel.y, voxel.z)));
                    maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x - 1, voxel.y, voxel.z)));

                    if(voxel.y > 0) {
                        maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x - 1, voxel.y - 1, voxel.z)));
                        maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x - 1, voxel.y - 1, voxel.z)));
                    }
                }

                if(voxel.y > 0) {
                    maxDist = max(maxDist, (*m_pDistanceMap)(Vec3i(voxel.x, voxel.y - 1, voxel.z)));
                    maxOpening = max(maxOpening, (*m_pOpeningMap)(Vec3i(voxel.x, voxel.y - 1, voxel.z)));
                }

                m_EdgeDistanceMap(voxel)[ZEDGE_IDX] = maxDist;
                m_EdgeOpeningMap(voxel)[ZEDGE_IDX] = maxOpening;
            }
        });
    }

    // Compute border
    m_BorderFlags = Grid3D<bool>(cc.width(), cc.height(), cc.depth(), false);
    m_Border.clear();

    foreachVoxel(cc.resolution(), [&](const Vec3i& voxel) {
        auto face = cc(voxel);
        auto x = voxel.x;
        auto y = voxel.y;
        auto z = voxel.z;
        if(shouldBeOnBorder(x, y, z, face)) {
            m_BorderFlags(x, y, z) = true;
            m_Border.emplace_back(Vec3i(x, y, z));
        }
    });

    // Compute birth map
    updateBirthMap();
}

bool ThinningProcessDGCI2013_2::directionalCollapse(int iterCount) {
    m_bHasChanged = true;
    while((iterCount < 0 || iterCount > 0) && m_bHasChanged) {
        m_bHasChanged = false;

        collapseAll<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>();
        collapseAll<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE>();
        collapseAll<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE>();
        collapseAll<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XEDGE>();

        collapseAll<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>();
        collapseAll<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE>();
        collapseAll<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE>();
        collapseAll<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XEDGE>();

        collapseAll<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>();
        collapseAll<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE>();
        collapseAll<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE>();
        collapseAll<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YEDGE>();

        collapseAll<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>();
        collapseAll<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE>();
        collapseAll<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE>();
        collapseAll<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YEDGE>();

        collapseAll<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>();
        collapseAll<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE>();
        collapseAll<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE>();
        collapseAll<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::ZEDGE>();

        collapseAll<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>();
        collapseAll<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE>();
        collapseAll<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE>();
        collapseAll<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::ZEDGE>();

        ++m_nIterationCount;

        updateBirthMap(); // Must be called before updating the border
        updateBorder();

        --iterCount;
    }

    if(!m_bHasChanged) {
        return true;
    }

    return false;
}

void ThinningProcessDGCI2013_2::updateBorder() {
    const auto& cc = *m_pCC;

    for(const auto& voxel: m_Border) {
        m_BorderFlags(voxel) = false;
    }

    m_PotentialBorder.clear();

    for(const auto& voxel: m_Border) {
        auto face = cc(voxel);
        auto x = voxel.x;
        auto y = voxel.y;
        auto z = voxel.z;
        if(shouldBeOnBorder(x, y, z, face)) {
            addToPotentialBorder(x, y, z);
        }
        foreach26Neighbour(cc.resolution(), voxel, [&](const Vec3i& neighbour) {
            auto face = cc(neighbour);
            auto x = neighbour.x;
            auto y = neighbour.y;
            auto z = neighbour.z;

            if(shouldBeOnBorder(x, y, z, face)) {
                addToPotentialBorder(x, y, z);
            }
        });
    }

    std::swap(m_Border, m_PotentialBorder);
}

bool ThinningProcessDGCI2013_2::isConstrainedEdge(int x, int y, int z, int edgeIdx) const {
    auto birthDate = m_BirthMap(x, y, z)[edgeIdx];
    if(birthDate >= 0) {
        auto lifespan = (int) m_nIterationCount - birthDate;
        //return lifespan > (int)(*m_pOpeningMap)(x, y, z) - 2 * (int)(*m_pDistanceMap)(x, y, z) + birthDate;
        //return lifespan > (int)(*m_pOpeningMap)(x, y, z) - 2 * (int)(*m_pDistanceMap)(x, y, z) + birthDate;
        return lifespan > m_EdgeOpeningMap(x, y, z)[edgeIdx] - m_EdgeDistanceMap(x, y, z)[edgeIdx] + birthDate;
        //return lifespan > m_EdgeOpeningMap(x, y, z)[edgeIdx] - 2 * m_EdgeDistanceMap(x, y, z)[edgeIdx] + birthDate;
        //return lifespan > (int) (*m_pOpeningMap)(x, y, z) - (int) (*m_pDistanceMap)(x, y, z);
        //return (int) m_D1Map(x, y, z) == 1u;
    }

    return false;
}

void ThinningProcessDGCI2013_2::updateBirthMap() {
//    foreachVoxel(m_pCC->resolution(), [&](const Vec3i& voxel) {
//        auto x = voxel.x;
//        auto y = voxel.y;
//        auto z = voxel.z;

//        if(m_BirthMap(voxel)[XEDGE_IDX] < 0 &&
//            (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XEDGE) &&
//           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE)) &&
//           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE)) &&
//           (y == 0 || !((*m_pCC)(voxel.x, voxel.y - 1, voxel.z).containsSome(CC3DFaceBits::XYFACE))) &&
//           (z == 0 || !((*m_pCC)(voxel.x, voxel.y, voxel.z - 1).containsSome(CC3DFaceBits::XZFACE)))) {
//            m_BirthMap(voxel)[XEDGE_IDX] = m_nIterationCount;
//        }

//        if(m_BirthMap(voxel)[YEDGE_IDX] < 0 &&
//            (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YEDGE) &&
//           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE)) &&
//           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YZFACE)) &&
//           (x == 0 || !((*m_pCC)(voxel.x - 1, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE))) &&
//           (z == 0 || !((*m_pCC)(voxel.x, voxel.y, voxel.z - 1).containsSome(CC3DFaceBits::YZFACE)))) {
//            m_BirthMap(voxel)[YEDGE_IDX] = m_nIterationCount;
//        }

//        if(m_BirthMap(voxel)[ZEDGE_IDX] < 0 &&
//            (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::ZEDGE) &&
//           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE)) &&
//           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YZFACE)) &&
//           (x == 0 || !((*m_pCC)(voxel.x - 1, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE))) &&
//           (y == 0 || !((*m_pCC)(voxel.x, voxel.y - 1, voxel.z).containsSome(CC3DFaceBits::YZFACE)))) {
//            m_BirthMap(voxel)[ZEDGE_IDX] = m_nIterationCount;
//        }
//    });

    for(const auto& voxel: m_Border) {
        auto x = voxel.x;
        auto y = voxel.y;
        auto z = voxel.z;

        if(m_BirthMap(voxel)[XEDGE_IDX] < 0 &&
            (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XEDGE) &&
           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE)) &&
           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE)) &&
           (y == 0 || !((*m_pCC)(voxel.x, voxel.y - 1, voxel.z).containsSome(CC3DFaceBits::XYFACE))) &&
           (z == 0 || !((*m_pCC)(voxel.x, voxel.y, voxel.z - 1).containsSome(CC3DFaceBits::XZFACE)))) {
            m_BirthMap(voxel)[XEDGE_IDX] = m_nIterationCount;
        }

        if(m_BirthMap(voxel)[YEDGE_IDX] < 0 &&
            (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YEDGE) &&
           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE)) &&
           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YZFACE)) &&
           (x == 0 || !((*m_pCC)(voxel.x - 1, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE))) &&
           (z == 0 || !((*m_pCC)(voxel.x, voxel.y, voxel.z - 1).containsSome(CC3DFaceBits::YZFACE)))) {
            m_BirthMap(voxel)[YEDGE_IDX] = m_nIterationCount;
        }

        if(m_BirthMap(voxel)[ZEDGE_IDX] < 0 &&
            (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::ZEDGE) &&
           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE)) &&
           !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YZFACE)) &&
           (x == 0 || !((*m_pCC)(voxel.x - 1, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE))) &&
           (y == 0 || !((*m_pCC)(voxel.x, voxel.y - 1, voxel.z).containsSome(CC3DFaceBits::YZFACE)))) {
            m_BirthMap(voxel)[ZEDGE_IDX] = m_nIterationCount;
        }
    }
}

}
