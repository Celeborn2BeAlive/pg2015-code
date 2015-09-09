#include "discrete_functions.hpp"
#include <bonez/sys/memory.hpp>
#include <bonez/sys/threads.hpp>
#include <bonez/maths/maths.hpp>
#include <queue>

namespace BnZ {

void dilateLine(uint32_t* radiusLine, Vec3u* centerLine, int stride, uint32_t size, Ball26* pBuffer) {
    DeQueue<Ball26> queue(pBuffer);

    // Invariants for the queue:
    // queue[i].radius > queue[i+1].radius (1)
    // queue[i+1].pixelIndex + queue[i+1].radius > queue[i].pixelIndex + queue[i].radius (2)

    for(auto i = 0u; i < size; ++i) {
        int offset = int(i) * stride;
        Ball26 currentBall = { i, radiusLine[offset], centerLine[offset] };

        if(currentBall.radius == 0u) {
            // We are outside the object, reset the dilation from there
            queue.clear();
        } else if(!queue.empty() && queue.front().endOfRange() == i) {
            // We reach the first pixel outside the range
            queue.pop_front();
        }

        if(queue.empty()) {
            queue.push_back(currentBall);
        } else {
            if(currentBall.radius >= queue.front().radius) {
                // If the current range is bigger than the first one to dilate, it becomes the new to dilate
                // and all the stored range are discarded
                queue.clear();
                queue.push_back(currentBall);
            } else {
                // Check if the current pixel need to be dilated outside the stored ranges
                while(currentBall.radius >= queue.back().radius) {
                    // Remove the ranges lower than the current pixel one
                    queue.pop_back();
                }

                // The queue cannot be empty because we have at least pixelRange.rangeRadius < queue.front().rangeRadius
                if (queue.back().endOfRange() < currentBall.endOfRange()) {
                    queue.push_back(currentBall);
                }
            }
        }

        // Dilate
        radiusLine[offset] = queue.front().radius;
        centerLine[offset] = queue.front().center;
    }
}

Grid3D<uint32_t> computeOpeningMap26(Grid3D<uint32_t> distanceMap, Grid3D<Vec3u>& centerMap) {
    auto width = distanceMap.width();
    auto height = distanceMap.height();
    auto depth = distanceMap.depth();

    centerMap = Grid3D<Vec3u>(width, height, depth);
    for(auto z = 0u; z < depth; ++z) {
        for(auto y = 0u; y < height; ++y) {
            for(auto x = 0u; x < width; ++x) {
                centerMap(x, y, z) = Vec3u(x, y, z);
            }
        }
    }

    auto rangeListSize = max(max(width, height), depth) + 1;
    auto rangeList = makeUniqueArray<Ball26>(rangeListSize);

    //On parcourt les lignes... de gauche à droite, puis droite à gauche
    for(auto k = 0u; k < depth; k++) {
        for(auto j = 0u; j < height; j++)
        {
            dilateLine(distanceMap.data() + j * width + k * width * height,
                       centerMap.data() + j * width + k * width * height,
                       1, width, rangeList.get());
            dilateLine(distanceMap.data() + j * width + k * width * height + width - 1,
                       centerMap.data() + j * width + k * width * height + width - 1,
                       -1, width, rangeList.get());
        }
    }

    //On parcourt les colonnes, haut n bas, puis bas en haut
    for(auto k = 0u; k < depth; k++) {
        for(auto i = 0u; i < width; i++)
        {
            dilateLine(distanceMap.data() + i + k * width * height,
                       centerMap.data() + i + k * width * height,
                       width, height, rangeList.get());
            dilateLine(distanceMap.data() + i + k * width * height + (height - 1) * width,
                       centerMap.data() + i + k * width * height + (height - 1) * width,
                       -(int)width, height, rangeList.get());
        }
    }

    //Parcours des profondeurs, de avant vers arriere, puis inversement
    for(auto j = 0u; j < height; j++) {
        for(auto i = 0u; i < width; i++)
        {
            dilateLine(distanceMap.data() + i + j * width,
                       centerMap.data() + i + j * width,
                       width * height, depth, rangeList.get());
            dilateLine(distanceMap.data() + i + j * width + (depth - 1) * width * height,
                       centerMap.data() + i + j * width + (depth - 1) * width * height,
                       -(int)(width * height), depth, rangeList.get());
        }
    }

    return distanceMap;
}

Grid3D<uint32_t> parallelComputeOpeningMap26(Grid3D<uint32_t> distanceMap, Grid3D<Vec3u>& centerMap) {
    auto width = distanceMap.width();
    auto height = distanceMap.height();
    auto depth = distanceMap.depth();

    auto threadCount = getSystemThreadCount();

    centerMap = Grid3D<Vec3u>(width, height, depth);

    processTasks(width * height * depth, [&](uint32_t voxelID, uint32_t threadID) {
        auto voxel = centerMap.coords(voxelID);
        centerMap[voxelID] = voxel;
    }, threadCount);

    auto rangeListSize = max(max(width, height), depth) + 1;
    auto rangeList = makeUniqueArray<Ball26>(rangeListSize * threadCount);

    processTasks(depth * height, [&](uint32_t lineID, uint32_t threadID) {
        auto j = lineID % height;
        auto k = lineID / height;

        //auto pRangeList = rangeList.get() + threadID * rangeListSize;
        auto pBuffer = rangeList.get() + threadID * rangeListSize;

        dilateLine(distanceMap.data() + j * width + k * width * height,
            centerMap.data() + j * width + k * width * height,
            1, width, pBuffer);
        dilateLine(distanceMap.data() + j * width + k * width * height + width - 1,
            centerMap.data() + j * width + k * width * height + width - 1,
            -1, width, pBuffer);
    }, getSystemThreadCount());


    processTasks(height * width, [&](uint32_t lineID, uint32_t threadID) {
        auto i = lineID % width;
        auto j = lineID / width;

        auto pBuffer = rangeList.get() + threadID * rangeListSize;

        dilateLine(distanceMap.data() + i + j * width,
            centerMap.data() + i + j * width,
            width * height, depth, pBuffer);
        dilateLine(distanceMap.data() + i + j * width + (depth - 1) * width * height,
            centerMap.data() + i + j * width + (depth - 1) * width * height,
            -(int)(width * height), depth, pBuffer);
    }, getSystemThreadCount());

    processTasks(depth * width, [&](uint32_t lineID, uint32_t threadID) {
        auto i = lineID % width;
        auto k = lineID / width;

        auto pBuffer = rangeList.get() + threadID * rangeListSize;

        dilateLine(distanceMap.data() + i + k * width * height,
            centerMap.data() + i + k * width * height,
            width, height, pBuffer);
        dilateLine(distanceMap.data() + i + k * width * height + (height - 1) * width,
            centerMap.data() + i + k * width * height + (height - 1) * width,
            -(int)width, height, pBuffer);
    }, getSystemThreadCount());



    return distanceMap;
}

}
