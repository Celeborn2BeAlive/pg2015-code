#pragma once

#include <vector>
#include <cstdint>

#include "bonez/sys/files.hpp"
#include <bonez/utils/Grid3D.hpp>

namespace BnZ {

typedef Grid3D<uint8_t> VoxelGrid;

VoxelGrid loadVoxelGridFromPGM(const FilePath& filepath);

inline bool isInObject(int x, int y, int z, const VoxelGrid& grid) {
    return grid(x, y, z);
}

inline bool isInComplementary(int x, int y, int z, const VoxelGrid& grid) {
    return !grid(x, y, z);
}

VoxelGrid inverse(VoxelGrid voxelGrid);

VoxelGrid deleteEmptyBorder(const VoxelGrid& voxelGrid, Mat4f gridToWorldMatrix, Mat4f& newGridToWorldMatrix);

}
