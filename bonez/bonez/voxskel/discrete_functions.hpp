#pragma once

#include <bonez/scene/VoxelGrid.hpp>

#include <bonez/sys/memory.hpp>
#include <bonez/maths/maths.hpp>
#include <queue>
#include <stack>
#include <unordered_map>

namespace BnZ {

// TODO: fix computeDistanceGrid6 and merge the two versions
template<typename GridType, typename Predicate>
Grid3D<uint32_t> computeDistanceGrid6Connexity(const GridType& grid, Predicate isInObject) {
    auto w = grid.width(), h = grid.height(), d = grid.depth();
    auto maxDist = w + h + d;
    Grid3D<uint32_t> distanceGrid(w, h, d, maxDist);

    using DistType = decltype(0u);

    auto processVoxel = [&distanceGrid, &grid, &isInObject](DistType& dist, int x, int y, int z) {
        if(!isInObject(x, y, z, grid)) {
            dist = 0u;
        } else {
            ++dist;
        }
        if(dist < distanceGrid(x, y, z)) {
            distanceGrid(x, y, z) = dist;
        } else {
            dist = distanceGrid(x, y, z);
        }
    };

    for(auto y = 0u; y < h; ++y) {
        for(auto x = 0u; x < w; ++x) {
            auto dist = 0u;
            for(int z = 0; z < d; ++z) {
                processVoxel(dist, x, y, z);
            }
            dist = 0u;
            for(int z = d - 1; z >= 0; --z) {
                processVoxel(dist, x, y, z);
            }
        }
    }

    for(auto z = 0u; z < d; ++z) {
        for(auto x = 0u; x < w; ++x) {
            auto dist = 0u;
            for(int y = 0; y < h; ++y) {
                processVoxel(dist, x, y, z);
            }
            dist = 0u;
            for(int y = h - 1; y >= 0; --y) {
                processVoxel(dist, x, y, z);
            }
        }
    }

    for(auto z = 0u; z < d; ++z) {
        for(auto y = 0u; y < h; ++y) {
            auto dist = 0u;
            for(int x = 0; x < w; ++x) {
                processVoxel(dist, x, y, z);
            }
            dist = 0u;
            for(int x = w - 1; x >= 0; --x) {
                processVoxel(dist, x, y, z);
            }
        }
    }

    return distanceGrid;
}

template<typename GridType, typename Predicate>
Grid3D<uint32_t> computeDistanceGrid6Connexity_outsideIsInObject(const GridType& grid, Predicate isInObject) {
    auto w = grid.width(), h = grid.height(), d = grid.depth();
    auto maxDist = w + h + d;
    Grid3D<uint32_t> distanceGrid(w, h, d, maxDist);

    using DistType = decltype(maxDist);

    auto processVoxel = [&distanceGrid, &grid, &isInObject, maxDist](DistType& dist, int x, int y, int z) {
        if(!isInObject(x, y, z, grid)) {
            dist = 0u;
        } else if(dist != maxDist) {
            ++dist;
        }
        if(dist < distanceGrid(x, y, z)) {
            distanceGrid(x, y, z) = dist;
        } else {
            dist = distanceGrid(x, y, z);
        }
    };

    for(auto y = 0u; y < h; ++y) {
        for(auto x = 0u; x < w; ++x) {
            auto dist = maxDist;
            for(auto z = 0; z < d; ++z) {
                processVoxel(dist, x, y, z);
            }
            dist = maxDist;
            for(int z = d - 1; z >= 0; --z) {
                processVoxel(dist, x, y, z);
            }
        }
    }

    for(auto z = 0u; z < d; ++z) {
        for(auto x = 0u; x < w; ++x) {
            auto dist = maxDist;
            for(auto y = 0; y < h; ++y) {
                processVoxel(dist, x, y, z);
            }
            dist = maxDist;
            for(int y = h - 1; y >= 0; --y) {
                processVoxel(dist, x, y, z);
            }
        }
    }

    for(auto z = 0u; z < d; ++z) {
        for(auto y = 0u; y < h; ++y) {
            auto dist = maxDist;
            for(auto x = 0; x < w; ++x) {
                processVoxel(dist, x, y, z);
            }
            dist = maxDist;
            for(int x = w - 1; x >= 0; --x) {
                processVoxel(dist, x, y, z);
            }
        }
    }

    return distanceGrid;
}


template<typename GridType, typename Predicate>
Grid3D<uint32_t> computeDistanceMap26(const GridType& grid, Predicate isInObject, bool outsideIsObject) {
    int w = grid.width(), h = grid.height(), d = grid.depth();
    uint32_t maxDist = std::numeric_limits<uint32_t>::max();
    Grid3D<uint32_t> distanceGrid(w, h, d, maxDist);

    static const auto MASK_SIZE = 13;

    auto processVoxel = [&distanceGrid, &grid, &isInObject, maxDist, outsideIsObject, w, h, d](const Vec3i* mask, int x, int y, int z) {
        auto voxel = Vec3i(x, y, z);
        if(!isInObject(x, y, z, grid)) {
            distanceGrid(voxel) = 0u;
        } else if(!outsideIsObject && (x == 0 || y == 0 || z == 0 || x == w - 1 || y == h - 1 || z == d - 1)) {
            distanceGrid(voxel) = 1u;
        }
        auto currentDist = distanceGrid(voxel);
        if(currentDist != maxDist) {
            auto dist = currentDist + 1;
            for(auto i = 0u; i < MASK_SIZE; ++i) {
                auto offset = mask[i];
                auto neighbour = voxel + offset;

                if(grid.contains(neighbour) && isInObject(neighbour.x, neighbour.y, neighbour.z, grid) && dist < distanceGrid(neighbour)) {
                    distanceGrid(neighbour) = dist;
                }
            }
        }
    };

    static const Vec3i masks[][MASK_SIZE] = {
        {
            Vec3i(1, 0, 0),
            Vec3i(1, 1, 0),
            Vec3i(0, 1, 0),
            Vec3i(-1, 1, 0),
            Vec3i(0, 0, 1),
            Vec3i(1, 0, 1),
            Vec3i(1, 1, 1),
            Vec3i(0, 1, 1),
            Vec3i(-1, 1, 1),
            Vec3i(-1, 0, 1),
            Vec3i(-1, -1, 1),
            Vec3i(0, -1, 1),
            Vec3i(1, -1, 1),
        },
        {
            -Vec3i(1, 0, 0),
            -Vec3i(1, 1, 0),
            -Vec3i(0, 1, 0),
            -Vec3i(-1, 1, 0),
            -Vec3i(0, 0, 1),
            -Vec3i(1, 0, 1),
            -Vec3i(1, 1, 1),
            -Vec3i(0, 1, 1),
            -Vec3i(-1, 1, 1),
            -Vec3i(-1, 0, 1),
            -Vec3i(-1, -1, 1),
            -Vec3i(0, -1, 1),
            -Vec3i(1, -1, 1),
        }
    };

    for(auto z = 0; z < d; ++z) {
        for(auto y = 0; y < h; ++y) {
            for(auto x = 0; x < w; ++x) {
                processVoxel(masks[0], x, y, z);
            }
        }
    }

    for(auto z = d - 1; z >= 0; --z) {
        for(auto y = h - 1; y >= 0; --y) {
            for(auto x = w - 1; x >= 0; --x) {
                processVoxel(masks[1], x, y, z);
            }
        }
    }

    return distanceGrid;
}

template<typename GridType, typename Predicate>
Grid3D<uint32_t> computeDistanceMap26_WithQueue(const GridType& grid, Predicate isInObject, bool outsideIsObject) {
    auto w = grid.width(), h = grid.height(), d = grid.depth();
    uint32_t maxDist = std::numeric_limits<uint32_t>::max();
    Grid3D<uint32_t> distanceGrid(w, h, d, maxDist);

    std::queue<Vec4i> queue;

    for(auto z = 0u; z < d; ++z) {
        for(auto y = 0u; y < h; ++y) {
            for(auto x = 0u; x < w; ++x) {
                auto voxel = Vec3i(x, y, z);
                if(!isInObject(x, y, z, grid)) {
                    queue.push(Vec4i(voxel, 0u));
                } else if(!outsideIsObject && (x == 0 || y == 0 || z == 0 || x == w - 1 || y == h - 1 || z == d - 1)) {
                    queue.push(Vec4i(voxel, 1u));
                }
            }
        }
    }

    while(!queue.empty()) {
        auto voxel = Vec3i(queue.front());
        auto dist = queue.front().w;
        queue.pop();

        if(dist < distanceGrid(voxel)) {
            distanceGrid(voxel) = dist;

            foreach26Neighbour(grid.resolution(), voxel, [&](const Vec3i& neighbour) {
                auto newDist = dist + 1;
                if(newDist < distanceGrid(neighbour)) {
                    queue.push(Vec4i(neighbour, newDist));
                }
            });
        }
    }

    return distanceGrid;
}

Grid3D<uint32_t> computeOpeningMap26(Grid3D<uint32_t> distanceMap26, Grid3D<Vec3u>& centerMap);

inline Grid3D<uint32_t> computeOpeningMap26(Grid3D<uint32_t> distanceMap26) {
    Grid3D<Vec3u> centerMap;
    return computeOpeningMap26(distanceMap26, centerMap);
}

Grid3D<uint32_t> parallelComputeOpeningMap26(Grid3D<uint32_t> distanceMap26, Grid3D<Vec3u>& centerMap);

inline Grid3D<uint32_t> parallelComputeOpeningMap26(Grid3D<uint32_t> distanceMap26) {
    Grid3D<Vec3u> centerMap;
    return parallelComputeOpeningMap26(distanceMap26, centerMap);
}

struct Ball26 {
    uint32_t index; // The index of the center in the current line
    uint32_t radius; // The radius of the ball
    Vec3u center; // The center of the ball

    // Return the index of the first voxel outside the ball
    inline uint32_t endOfRange() const {
        return index + radius;
    }
};

template<typename T>
class DeQueue {
    T* m_pBuffer = nullptr;
    T* m_pBegin = nullptr;
    T* m_pEnd = nullptr;
public:
    DeQueue(T* pBuffer): m_pBuffer(pBuffer) {
        clear();
    }

    void clear() {
        m_pBegin = m_pEnd = m_pBuffer;
    }

    bool empty() const {
        return m_pBegin == m_pEnd;
    }

    template<typename U>
    void push_back(U&& value) {
        *m_pEnd = std::forward<U>(value);
        ++m_pEnd;
    }

    const T& front() const {
        assert(!empty());
        return *m_pBegin;
    }

    const T& back() const {
        assert(!empty());
        return *(m_pEnd - 1);
    }

    void pop_front() {
        assert(!empty());
        ++m_pBegin;
    }

    void pop_back() {
        assert(!empty());
        --m_pEnd;
    }
};

void dilateLine(uint32_t* radiusLine, Vec3u* centerLine, int stride, uint32_t size, Ball26* pBuffer);

}
