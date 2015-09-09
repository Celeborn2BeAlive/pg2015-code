#include "ThinningProcessDGCI2013.hpp"
#include "discrete_functions.hpp"

#include <bonez/sys/threads.hpp>
#include <bonez/sys/time.hpp>

namespace BnZ {

// Initialize the thinning process of a cubical complex
void ThinningProcessDGCI2013::init(CubicalComplex3D& cc,
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

    {
        Timer timer(true);
        std::clog << "Build border" << std::endl;

        m_BorderFlags = Grid3D<bool>(cc.width(), cc.height(), cc.depth(), false);
        for(auto& borderList: m_Borders) {
            borderList.clear();
        }

        for (auto z = 0u; z < m_nDepth; ++z){
            for (auto y = 0u; y < m_nHeight; ++y){
                for (auto x = 0u; x < m_nWidth; ++x){
                    testForBorder(x, y, z);
                }
            }
        }
    }

    {
        Timer timer(true);
        std::clog << "Update birth map" << std::endl;

        updateBirthMap();
    }
}

// Apply the collapse operation to the cubical complex in each of the 6 directions
//
// \iterCount The number of collapse to apply. If -1, collapse till no free pair exists in the complex
// \return false if the cubical complex is already thin.
bool ThinningProcessDGCI2013::parallelDirectionalCollapse(int iterCount) {
    while((iterCount < 0 || iterCount > 0) && !borderIsEmpty()) {
        parallelProcessBorder(m_Borders[0], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[0], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[0], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[0], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XEDGE>, this, std::placeholders::_1));

        parallelProcessBorder(m_Borders[1], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[1], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[1], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[1], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XEDGE>, this, std::placeholders::_1));

        parallelProcessBorder(m_Borders[2], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[2], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[2], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[2], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YEDGE>, this, std::placeholders::_1));

        parallelProcessBorder(m_Borders[3], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[3], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[3], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[3], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YEDGE>, this, std::placeholders::_1));

        parallelProcessBorder(m_Borders[4], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[4], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[4], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[4], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::ZEDGE>, this, std::placeholders::_1));

        parallelProcessBorder(m_Borders[5], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[5], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[5], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE>, this, std::placeholders::_1));
        parallelProcessBorder(m_Borders[5], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::ZEDGE>, this, std::placeholders::_1));

        ++m_nIterationCount;
        updateBirthMap();

        updateBorder();

        --iterCount;
    }

    if(borderIsEmpty()) {
        return true;
    }

    return false;
}

bool ThinningProcessDGCI2013::directionalCollapse(int iterCount) {
    while((iterCount < 0 || iterCount > 0) && !borderIsEmpty()) {
        processBorder(m_Borders[0], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        processBorder(m_Borders[0], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[0], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[0], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XEDGE>, this, std::placeholders::_1));

        processBorder(m_Borders[1], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        processBorder(m_Borders[1], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[1], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[1], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XEDGE>, this, std::placeholders::_1));

        processBorder(m_Borders[2], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        processBorder(m_Borders[2], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[2], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[2], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YEDGE>, this, std::placeholders::_1));

        processBorder(m_Borders[3], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        processBorder(m_Borders[3], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[3], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[3], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YEDGE>, this, std::placeholders::_1));

        processBorder(m_Borders[4], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        processBorder(m_Borders[4], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[4], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[4], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::ZEDGE>, this, std::placeholders::_1));

        processBorder(m_Borders[5], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE>, this, std::placeholders::_1));
        processBorder(m_Borders[5], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[5], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE>, this, std::placeholders::_1));
        processBorder(m_Borders[5], std::bind(&ThinningProcessDGCI2013::collapse<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::ZEDGE>, this, std::placeholders::_1));

        ++m_nIterationCount;

        updateBirthMap();

        --iterCount;
    }

    if(borderIsEmpty()) {
        return true;
    }

    return false;
}

void ThinningProcessDGCI2013::updateBorder() {
    VoxelList updatedBorder;

    updateBorderXNEG(updatedBorder);
    updateBorderXPOS(updatedBorder);
    updateBorderYNEG(updatedBorder);
    updateBorderYPOS(updatedBorder);
    updateBorderZNEG(updatedBorder);
    updateBorderZPOS(updatedBorder);

    clearBorder();
    clearBorderFlags(updatedBorder);

    computeUpdatedBorder(updatedBorder);
}

void ThinningProcessDGCI2013::computeUpdatedBorder(const VoxelList& updatedBorder){
    for (auto i = 0u; i < updatedBorder.size(); i++){
        Vec3i p = updatedBorder[i];
        testForBorder(p.x, p.y, p.z);
    }
}

void ThinningProcessDGCI2013::updateBorderXNEG(VoxelList& updatedBorder) {
   for (auto i = 0u; i < m_Borders[0].size(); i++){
        Vec3i p = m_Borders[0][i];

        if (!isInBorder(p.x, p.y, p.z)){
            updatedBorder.emplace_back(p);
            setBorderFlag(p.x, p.y, p.z);
        }

        // Testing neighbourhood
        if (p.x > 0 && !isInBorder(p.x - 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x - 1, p.y, p.z));
            setBorderFlag(p.x - 1, p.y, p.z);
        }

        if (p.y > 0 && !isInBorder(p.x, p.y - 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y - 1, p.z));
            setBorderFlag(p.x, p.y - 1, p.z);
        }

        if (p.z > 0 && !isInBorder(p.x, p.y, p.z - 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z - 1));
            setBorderFlag(p.x, p.y, p.z - 1);
        }

        if (p.y < int(m_nHeight) - 1 && !isInBorder(p.x, p.y + 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y + 1, p.z));
            setBorderFlag(p.x, p.y + 1, p.z);
        }

        if (p.z < int(m_nDepth) - 1 && !isInBorder(p.x, p.y, p.z + 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z + 1));
            setBorderFlag(p.x, p.y, p.z + 1);
        }
    }
}

void ThinningProcessDGCI2013::updateBorderXPOS(VoxelList& updatedBorder) {
   for (auto i = 0u; i < m_Borders[1].size(); i++){
        Vec3i p = m_Borders[1][i];

        if (!isInBorder(p.x, p.y, p.z)){
            updatedBorder.emplace_back(p);
            setBorderFlag(p.x, p.y, p.z);
        }

        // Testing neighbourhood
        if (p.x < int(m_nWidth) - 1 && !isInBorder(p.x + 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x + 1, p.y, p.z));
            setBorderFlag(p.x + 1, p.y, p.z);
        }

        if (p.y > 0 && !isInBorder(p.x, p.y - 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y - 1, p.z));
            setBorderFlag(p.x, p.y - 1, p.z);
        }

        if (p.y < int(m_nHeight) - 1 && !isInBorder(p.x, p.y + 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y + 1, p.z));
            setBorderFlag(p.x, p.y + 1, p.z);
        }

        if (p.z > 0 && !isInBorder(p.x, p.y, p.z - 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z - 1));
            setBorderFlag(p.x, p.y, p.z - 1);
        }

        if (p.z < int(m_nDepth) - 1 && !isInBorder(p.x, p.y, p.z + 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z + 1));
            setBorderFlag(p.x, p.y, p.z + 1);
        }

    }
}

void ThinningProcessDGCI2013::updateBorderYNEG(VoxelList& updatedBorder) {
    for (auto i = 0u; i < m_Borders[2].size(); i++){
        Vec3i p = m_Borders[2][i];

        if (!isInBorder(p.x, p.y, p.z)){
            updatedBorder.emplace_back(p);
            setBorderFlag(p.x, p.y, p.z);
        }

        // Testing neighbourhood
        if (p.x > 0 && !isInBorder(p.x - 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x - 1, p.y, p.z));
            setBorderFlag(p.x - 1, p.y, p.z);
        }

        if (p.x < int(m_nWidth) - 1 && !isInBorder(p.x + 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x + 1, p.y, p.z));
            setBorderFlag(p.x + 1, p.y, p.z);
        }

        if (p.y > 0 && !isInBorder(p.x, p.y - 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y - 1, p.z));
            setBorderFlag(p.x, p.y - 1, p.z);
        }

        if (p.z > 0 && !isInBorder(p.x, p.y, p.z - 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z - 1));
            setBorderFlag(p.x, p.y, p.z - 1);
        }

        if (p.z < int(m_nDepth) - 1 && !isInBorder(p.x, p.y, p.z + 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z + 1));
            setBorderFlag(p.x, p.y, p.z + 1);
        }
    }

}

void ThinningProcessDGCI2013::updateBorderYPOS(VoxelList& updatedBorder) {
   for (auto i = 0u; i < m_Borders[3].size(); i++){
        Vec3i p = m_Borders[3][i];

        if (!isInBorder(p.x, p.y, p.z)){
            updatedBorder.emplace_back(p);
            setBorderFlag(p.x, p.y, p.z);
        }

        // Testing neighbourhood
        if (p.x > 0 && !isInBorder(p.x - 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x - 1, p.y, p.z));
            setBorderFlag(p.x - 1, p.y, p.z);
        }

        if (p.x < int(m_nWidth) - 1 && !isInBorder(p.x + 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x + 1, p.y, p.z));
            setBorderFlag(p.x + 1, p.y, p.z);
        }

        if (p.y < int(m_nHeight) - 1 && !isInBorder(p.x, p.y + 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y + 1, p.z));
            setBorderFlag(p.x, p.y + 1, p.z);
        }

        if (p.z > 0 && !isInBorder(p.x, p.y, p.z - 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z - 1));
            setBorderFlag(p.x, p.y, p.z - 1);
        }

        if (p.z < int(m_nDepth) - 1 && !isInBorder(p.x, p.y, p.z + 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z + 1));
            setBorderFlag(p.x, p.y, p.z + 1);
        }

    }
}

void ThinningProcessDGCI2013::updateBorderZNEG(VoxelList& updatedBorder) {
    for (auto i = 0u; i < m_Borders[4].size(); i++){
        Vec3i p = m_Borders[4][i];

        if (!isInBorder(p.x, p.y, p.z)){
            updatedBorder.emplace_back(p);
            setBorderFlag(p.x, p.y, p.z);
        }

        // Testing neighbourhood
        if (p.x > 0 && !isInBorder(p.x - 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x - 1, p.y, p.z));
            setBorderFlag(p.x - 1, p.y, p.z);
        }

        if (p.x < int(m_nWidth) - 1 && !isInBorder(p.x + 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x + 1, p.y, p.z));
            setBorderFlag(p.x + 1, p.y, p.z);
        }

        if (p.y > 0 && !isInBorder(p.x, p.y - 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y - 1, p.z));
            setBorderFlag(p.x, p.y - 1, p.z);
        }

        if (p.y < int(m_nHeight) - 1 && !isInBorder(p.x, p.y + 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y + 1, p.z));
            setBorderFlag(p.x, p.y + 1, p.z);
        }

        if (p.z > 0 && !isInBorder(p.x, p.y, p.z - 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z - 1));
            setBorderFlag(p.x, p.y, p.z - 1);
        }
    }
}

void ThinningProcessDGCI2013::updateBorderZPOS(VoxelList& updatedBorder) {
   for (auto i = 0u; i < m_Borders[5].size(); i++){
        Vec3i p = m_Borders[5][i];

        if (!isInBorder(p.x, p.y, p.z)){
            updatedBorder.emplace_back(p);
            setBorderFlag(p.x,p.y,p.z);
        }

        // Testing neighbourhood
        if (p.x > 0 && !isInBorder(p.x - 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x - 1, p.y, p.z));
            setBorderFlag(p.x - 1, p.y, p.z);
        }

        if (p.x < int(m_nWidth) - 1 && !isInBorder(p.x + 1, p.y, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x + 1, p.y, p.z));
            setBorderFlag(p.x + 1, p.y, p.z);
        }

        if (p.y > 0 && !isInBorder(p.x, p.y - 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y - 1, p.z));
            setBorderFlag(p.x, p.y - 1, p.z);
        }

        if (p.y < int(m_nHeight) - 1 && !isInBorder(p.x, p.y + 1, p.z)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y + 1, p.z));
            setBorderFlag(p.x, p.y + 1, p.z);
        }

        if (p.z < int(m_nDepth) - 1 && !isInBorder(p.x, p.y, p.z + 1)){
            updatedBorder.emplace_back(Vec3i(p.x, p.y, p.z + 1));
            setBorderFlag(p.x, p.y, p.z + 1);
        }
    }
}

void ThinningProcessDGCI2013::clearBorderFlags(const VoxelList& updatedBorder) {
    for (auto i = 0u; i < updatedBorder.size(); ++i) {
        Vec3i p = updatedBorder[i];
        setBorderFlag(p.x, p.y, p.z, false);
    }
}

void ThinningProcessDGCI2013::clearBorder() {
    for (auto i = 0u; i < 6u; i++) {
        m_Borders[i].clear();
    }
}

void ThinningProcessDGCI2013::testForBorder(int x, int y, int z) {
    if ((*m_pCC)(x,y,z).exists()) {
        if (isFreeXNEG(x,y,z))
            m_Borders[0].emplace_back(Vec3i(x,y,z)); //dir = 0 or = 0

        if (isFreeXPOS(x,y,z))
            m_Borders[1].emplace_back(Vec3i(x,y,z)); //dir = 0 or = 1

        if (isFreeYNEG(x,y,z))
            m_Borders[2].emplace_back(Vec3i(x,y,z)); //dir = 1 or = 0

        if (isFreeYPOS(x,y,z))
            m_Borders[3].emplace_back(Vec3i(x,y,z)); //dir = 1 or = 1

        if (isFreeZNEG(x,y,z))
            m_Borders[4].emplace_back(Vec3i(x,y,z)); //dir = 2 or = 0

        if (isFreeZPOS(x,y,z))
            m_Borders[5].emplace_back(Vec3i(x,y,z)); //dir = 2 or = 1
    }
}

bool ThinningProcessDGCI2013::isConstrainedEdge(int x, int y, int z, int edgeIdx) const {
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

void ThinningProcessDGCI2013::updateBirthMap() {
    foreachVoxel(m_pCC->resolution(), [&](const Vec3i& voxel) {
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
    });

//    for(const auto& borderList: m_Borders) {
//        for(const auto& voxel: borderList) {
//            auto x = voxel.x;
//            auto y = voxel.y;
//            auto z = voxel.z;

//            if(m_BirthMap(voxel)[XEDGE_IDX] < 0 &&
//                (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XEDGE) &&
//               !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE)) &&
//               !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE)) &&
//               (y == 0 || !((*m_pCC)(voxel.x, voxel.y - 1, voxel.z).containsSome(CC3DFaceBits::XYFACE))) &&
//               (z == 0 || !((*m_pCC)(voxel.x, voxel.y, voxel.z - 1).containsSome(CC3DFaceBits::XZFACE)))) {
//                m_BirthMap(voxel)[XEDGE_IDX] = m_nIterationCount;
//            }

//            if(m_BirthMap(voxel)[YEDGE_IDX] < 0 &&
//                (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YEDGE) &&
//               !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE)) &&
//               !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YZFACE)) &&
//               (x == 0 || !((*m_pCC)(voxel.x - 1, voxel.y, voxel.z).containsSome(CC3DFaceBits::XYFACE))) &&
//               (z == 0 || !((*m_pCC)(voxel.x, voxel.y, voxel.z - 1).containsSome(CC3DFaceBits::YZFACE)))) {
//                m_BirthMap(voxel)[YEDGE_IDX] = m_nIterationCount;
//            }

//            if(m_BirthMap(voxel)[ZEDGE_IDX] < 0 &&
//                (*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::ZEDGE) &&
//               !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE)) &&
//               !((*m_pCC)(voxel.x, voxel.y, voxel.z).containsSome(CC3DFaceBits::YZFACE)) &&
//               (x == 0 || !((*m_pCC)(voxel.x - 1, voxel.y, voxel.z).containsSome(CC3DFaceBits::XZFACE))) &&
//               (y == 0 || !((*m_pCC)(voxel.x, voxel.y - 1, voxel.z).containsSome(CC3DFaceBits::YZFACE)))) {
//                m_BirthMap(voxel)[ZEDGE_IDX] = m_nIterationCount;
//            }
//        }
//    }
}

}
