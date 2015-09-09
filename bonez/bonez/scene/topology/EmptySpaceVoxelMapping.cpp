#include "EmptySpaceVoxelMapping.hpp"

namespace BnZ {

EmptySpaceVoxelMapping::EmptySpaceVoxelMapping() {
    {
        auto k = 0u;
        for(auto i = -1; i <= 1; ++i) {
            for(auto j = -1; j <= 1; ++j) {
                m_FaceOffsets[int(CubeFace::POS_X)][k++] = Vec3i(1, i, j);
            }
        }
    }

    {
        auto k = 0u;
        for(auto i = -1; i <= 1; ++i) {
            for(auto j = -1; j <= 1; ++j) {
                m_FaceOffsets[int(CubeFace::NEG_X)][k++] = Vec3i(-1, i, j);
            }
        }
    }

    {
        auto k = 0u;
        for(auto i = -1; i <= 1; ++i) {
            for(auto j = -1; j <= 1; ++j) {
                m_FaceOffsets[int(CubeFace::POS_Y)][k++] = Vec3i(i, 1, j);
            }
        }
    }

    {
        auto k = 0u;
        for(auto i = -1; i <= 1; ++i) {
            for(auto j = -1; j <= 1; ++j) {
                m_FaceOffsets[int(CubeFace::NEG_Y)][k++] = Vec3i(i, -1, j);
            }
        }
    }

    {
        auto k = 0u;
        for(auto i = -1; i <= 1; ++i) {
            for(auto j = -1; j <= 1; ++j) {
                m_FaceOffsets[int(CubeFace::POS_Z)][k++] = Vec3i(i, j, 1);
            }
        }
    }

    {
        auto k = 0u;
        for(auto i = -1; i <= 1; ++i) {
            for(auto j = -1; j <= 1; ++j) {
                m_FaceOffsets[int(CubeFace::NEG_Z)][k++] = Vec3i(i, j, -1);
            }
        }
    }
}

}
