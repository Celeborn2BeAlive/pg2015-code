#pragma once

#include <vector>
#include <cstdint>
#include <bonez/types.hpp>

#include "GLVoxelFramebuffer.hpp"

namespace BnZ {

class VoxelBuffer {
private:
    std::vector<Vec4i> m_data;
    uint32_t m_width = 0, m_height = 0, m_depth = 0, m_numRenderTarget = 0;
    static int s_bitmask[32];

public:
    void init(const GLVoxelFramebuffer& framebuffer);

    bool operator()(uint32_t x, uint32_t y, uint32_t z);

    template<typename Functor>
    void processVoxels(Functor f, bool processComplementary = false) {
        for(auto texture = 0u; texture < m_numRenderTarget; ++texture) {
            auto* pData = m_data.data() + texture *  m_width * m_height;
            //Convert data in voxel position
            for (auto i = 0u; i <  m_width * m_height; i++) {
                auto x = i % m_width;
                auto y = (i - x) / m_width;
                for(auto j = 0u; j < 4; j++){
                    for(auto k = 0u; k < 32; k++) {
                        auto z = texture * 128 + j * 32 + k;
                        if (z < m_depth && (
                                (!processComplementary && (pData[i][j] & s_bitmask[k]) != 0) ||
                                    (processComplementary && (pData[i][j] & s_bitmask[k]) == 0)
                                    )) {
                            f(x, y, z);
                        }
                    }
                }
            }
        }
    }
};

}
