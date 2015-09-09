#include "VoxelGrid.hpp"

#include <fstream>
#include <stdexcept>
#include <sstream>

#include <bonez/maths/BBox.hpp>

using namespace std;

namespace BnZ {

VoxelGrid loadVoxelGridFromPGM(const FilePath& filepath) {
    ifstream in(filepath.c_str());

    if (!in) {
        throw runtime_error("Unable to open the file " + filepath.str() + ".");
    }

    std::string ext = filepath.ext();

    if (ext != "ppm" && ext != "pgm") {
        throw runtime_error("Invalid format for the file " + filepath.str() +
            ". Recognized formats are .ppm / .pgm");
    }

    string line;
    getline(in, line, '\n');

    // Read file code
    if ((line.find("P5") == string::npos)) {
        throw std::runtime_error("PPM / PGM invalid file (error in the code).");
    }

    // Skip commentaries
    do {
        getline(in, line, '\n');
    }
    while (line.at(0) == '#');

    size_t w, h, d;
    {
        istringstream iss(line);

        // Read the size of the grid
        iss >> w;
        iss >> h;
        iss >> d;
    }
    getline(in, line, '\n'); // contains max grey value = 255
    if(line != "255") {
        throw std::runtime_error("PPM / PGM invalid file (doesn't contains chars').");
    }

    VoxelGrid grid(w, h, d);
    in.read((char*) grid.data(), grid.size() * sizeof(VoxelGrid::value_type));

    return grid;
}

VoxelGrid inverse(VoxelGrid voxelGrid) {
    foreachVoxel(voxelGrid.resolution(), [&](const Vec3i& voxel) {
        voxelGrid(voxel) = VoxelGrid::value_type(!voxelGrid(voxel));
    });
    return voxelGrid;
}

VoxelGrid deleteEmptyBorder(const VoxelGrid& voxelGrid, Mat4f gridToWorldMatrix, Mat4f& newGridToWorldMatrix) {
    BBox3i filledVoxelBBox;
    bool bboxIsInit = false;

    foreachVoxel(voxelGrid.resolution(), [&](const Vec3i& voxel) {
        if (voxelGrid(voxel)) {
            if (bboxIsInit) {
                filledVoxelBBox.grow(voxel);
            }
            else {
                filledVoxelBBox = BBox3i(voxel);
                bboxIsInit = true;
            }
        }
    });

    filledVoxelBBox.upper += Vec3i(1);

    VoxelGrid newGrid(Vec3u(filledVoxelBBox.size()));

    foreachVoxel(newGrid.resolution(), [&](const Vec3i& voxel) {
        newGrid(voxel) = voxelGrid(voxel + filledVoxelBBox.lower);
    });

    Mat4f newGridToOldGrid = translate(Mat4f(1), Vec3f(filledVoxelBBox.lower));
    newGridToWorldMatrix = gridToWorldMatrix * newGridToOldGrid;

    return newGrid;
}

}
