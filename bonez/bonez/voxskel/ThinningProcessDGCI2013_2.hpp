#pragma once

#include <bonez/types.hpp>
#include <bonez/utils/Grid3D.hpp>
#include <bonez/sys/threads.hpp>

#include "CubicalComplex3D.hpp"

namespace BnZ {

class ThinningProcessDGCI2013_2 {
public:
    // Initialize the thinning process of a cubical complex
    void init(CubicalComplex3D& cc,
              const Grid3D<uint32_t>* pDistanceMap,
              const Grid3D<uint32_t>* pOpeningMap,
              const CubicalComplex3D* pConstrainedCC = nullptr);

    // Apply the collapse operation to the cubical complex in each of the 6 directions
    //
    // \iterCount The number of collapse to apply. If -1, collapse till no free pair exists in the complex
    // \return true if the cubical complex ends up being thin.
    bool directionalCollapse(int iterCount = -1);

    const Grid3D<Vec3i>& getBirthMap() const {
        return m_BirthMap;
    }

public:
    using VoxelList = std::vector<Vec3i>;

    CubicalComplex3D* m_pCC = nullptr;
    const CubicalComplex3D* m_pConstrainedCC = nullptr;
    uint32_t m_nWidth = 0u;
    uint32_t m_nHeight = 0u;
    uint32_t m_nDepth = 0u;

    VoxelList m_Border;
    VoxelList m_PotentialBorder;
    Grid3D<bool> m_BorderFlags; // Flag true for each voxel in the border

    VoxelList m_FreePairs;

    bool m_bHasChanged = true;

    enum EdgeIndex {
        XEDGE_IDX = 0,
        YEDGE_IDX = 1,
        ZEDGE_IDX = 2,
    };

    Grid3D<Vec3i> m_BirthMap; // Contains the birth date of the edge of each voxel
    Grid3D<Vec3i> m_EdgeDistanceMap;
    Grid3D<Vec3i> m_EdgeOpeningMap;
    uint32_t m_nIterationCount = 0u; // Number of thinning iterations already done

    const Grid3D<uint32_t>* m_pDistanceMap; // Distance to border for each voxel
    const Grid3D<uint32_t>* m_pOpeningMap; // Opening radius for each voxel (based on the distance map)

    void updateBorder();

    void updateBirthMap();

    bool isConstrainedEdge(int x, int y, int z, int edgeIdx) const;

    bool isFreeXNEG(int x, int y, int z) const {
        return isFree<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XEDGE>(Vec3i(x, y, z));
    }

    bool isFreeXPOS(int x, int y, int z) const {
        return isFree<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XEDGE>(Vec3i(x, y, z));
    }

    bool isFreeYNEG(int x, int y, int z) const {
        return isFree<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YEDGE>(Vec3i(x, y, z));
    }

    bool isFreeYPOS(int x, int y, int z) const {
        return isFree<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YEDGE>(Vec3i(x, y, z));
    }

    bool isFreeZNEG(int x, int y, int z) const {
        return isFree<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::ZEDGE>(Vec3i(x, y, z));
    }

    bool isFreeZPOS(int x, int y, int z) const {
        return isFree<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE>(Vec3i(x, y, z)) ||
                isFree<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::ZEDGE>(Vec3i(x, y, z));
    }

    bool isFree(int x, int y, int z) const {
        return isFreeXNEG(x, y, z) || isFreeXPOS(x, y, z) || isFreeYNEG(x, y, z) ||
                isFreeYPOS(x, y, z) || isFreeZNEG(x, y, z) || isFreeZPOS(x, y, z);
    }

    template<CC3DDirection Dir, CC3DOrientation Orient, CC3DFaceElement Face>
    struct EdgeConstraintHelper {
        static bool isEdgeConstrained(const ThinningProcessDGCI2013_2& thinningProcess,
                               const Vec3i& voxel) {
            bool result = false;
            CC3DFreePairHelper<Dir, Orient, Face>::forEach(voxel, [&](const Vec3i& v, int bits) {
                if((bits & CC3DFaceBits::XEDGE) && thinningProcess.isConstrainedEdge(v.x, v.y, v.z, XEDGE_IDX)) {
                    result = true;
                }
                if((bits & CC3DFaceBits::YEDGE) && thinningProcess.isConstrainedEdge(v.x, v.y, v.z, YEDGE_IDX)) {
                    result = true;
                }
                if((bits & CC3DFaceBits::ZEDGE) && thinningProcess.isConstrainedEdge(v.x, v.y, v.z, ZEDGE_IDX)) {
                    result = true;
                }
            });
            return result;
        }
    };

    template<CC3DDirection Dir, CC3DOrientation Orient>
    struct EdgeConstraintHelper<Dir, Orient, CC3DFaceElement::XEDGE> {
        static bool isEdgeConstrained(const ThinningProcessDGCI2013_2& thinningProcess,
                               const Vec3i& voxel) {
            bool result = false;
            CC3DFreePairHelper<Dir, Orient, CC3DFaceElement::XEDGE>::forEach(voxel, [&](const Vec3i& v, int bits) {
                if((bits & CC3DFaceBits::XEDGE) && thinningProcess.isConstrainedEdge(v.x, v.y, v.z, XEDGE_IDX)) {
                    result = true;
                }
            });
            return result;
        }
    };

    template<CC3DDirection Dir, CC3DOrientation Orient>
    struct EdgeConstraintHelper<Dir, Orient, CC3DFaceElement::YEDGE> {
        static bool isEdgeConstrained(const ThinningProcessDGCI2013_2& thinningProcess,
                               const Vec3i& voxel) {
            bool result = false;
            CC3DFreePairHelper<Dir, Orient, CC3DFaceElement::YEDGE>::forEach(voxel, [&](const Vec3i& v, int bits) {
                if((bits & CC3DFaceBits::YEDGE) && thinningProcess.isConstrainedEdge(v.x, v.y, v.z, YEDGE_IDX)) {
                    result = true;
                }
            });
            return result;
        }
    };

    template<CC3DDirection Dir, CC3DOrientation Orient>
    struct EdgeConstraintHelper<Dir, Orient, CC3DFaceElement::ZEDGE> {
        static bool isEdgeConstrained(const ThinningProcessDGCI2013_2& thinningProcess,
                               const Vec3i& voxel) {
            bool result = false;
            CC3DFreePairHelper<Dir, Orient, CC3DFaceElement::ZEDGE>::forEach(voxel, [&](const Vec3i& v, int bits) {
                if((bits & CC3DFaceBits::ZEDGE) && thinningProcess.isConstrainedEdge(v.x, v.y, v.z, ZEDGE_IDX)) {
                    result = true;
                }
            });
            return result;
        }
    };

    template<CC3DDirection Dir, CC3DOrientation Orient, CC3DFaceElement Face>
    bool isConstrained(const Vec3i& voxel) const {
        if(m_pConstrainedCC) {
            bool result = false;
            CC3DFreePairHelper<Dir, Orient, Face>::forEach(voxel, [&](const Vec3i& v, int bits) {
                result |= (*m_pConstrainedCC)(v).containsSome(bits);
            });
            if(result) {
                return true;
            }
        }
        if(!m_pDistanceMap || !m_pOpeningMap) {
            return false;
        }
        return EdgeConstraintHelper<Dir, Orient, Face>::isEdgeConstrained(*this, voxel);
    }

    template<CC3DDirection Dir, CC3DOrientation Orient, CC3DFaceElement Face>
    bool isFree(const Vec3i& voxel) const {
        return CC3DFreePairHelper<Dir, Orient, Face>::isFree(*m_pCC, voxel) && !isConstrained<Dir, Orient, Face>(voxel);
    }

    template<CC3DDirection Dir, CC3DOrientation Orient, CC3DFaceElement Face>
    void collapse(const Vec3i& voxel) {
        if(isFree<Dir, Orient, Face>(voxel)) {
            CC3DFreePairHelper<Dir, Orient, Face>::forEach(voxel, [&](const Vec3i& v, int bits) {
                (*m_pCC)(v).remove(bits);
                m_bHasChanged = true;
            });
        }
    }

    template<typename Functor>
    inline void processBorder(const VoxelList& border, Functor f) {
        for(const auto& voxel: border) {
            f(voxel);
        }
    }

    template<CC3DDirection Dir, CC3DOrientation Orient, CC3DFaceElement Face>
    void collapseAll() {
        m_FreePairs.clear();
        for(const auto& voxel: m_Border) {
            if(isFree<Dir, Orient, Face>(voxel)) {
                m_FreePairs.emplace_back(voxel);
            }
        }
        for(const auto& voxel: m_FreePairs) {
            collapse<Dir, Orient, Face>(voxel);
        }
    }

    template<typename Functor>
    inline void parallelProcessBorder(const VoxelList& border, Functor f) {
        processTasks(border.size(), [&](uint32_t taskID, uint32_t threadID) {
            f(border[taskID]);
        }, getSystemThreadCount());
    }

    bool borderIsEmpty() const {
        return m_Border.empty();
    }

    void addToPotentialBorder(int x, int y, int z) {
        if(!m_BorderFlags(x, y, z)) {
            m_BorderFlags(x, y, z) = true;
            m_PotentialBorder.emplace_back(Vec3i(x, y, z));
        }
    }

    bool shouldBeOnBorder(int x, int y, int z, CC3DFace face) const {
        if(!face.exists()) {
            return false;
        }
        if(!face.containsSome(CC3DFaceBits::CUBE)) {
            return true;
        }
        if(x == 0 || y == 0 || z == 0 ||
                !(*m_pCC)(x - 1, y, z).containsSome(CC3DFaceBits::CUBE) ||
                !(*m_pCC)(x, y - 1, z).containsSome(CC3DFaceBits::CUBE) ||
                !(*m_pCC)(x, y, z - 1).containsSome(CC3DFaceBits::CUBE)) {
            return true;
        }
        return false;
    }
};

}
