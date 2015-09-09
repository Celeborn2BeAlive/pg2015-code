#pragma once

#include <cstdint>
#include <vector>
#include <bonez/types.hpp>
#include <bonez/sys/memory.hpp>

namespace BnZ {

enum class Grid3DCutAxis {
    XAxis = 0,
    YAxis = 1,
    ZAxis = 2
};

template<typename T>
class Grid3D: std::vector<T> {
    typedef std::vector<T> Base;
public:
    using value_type = typename Base::value_type;
    using reference = typename Base::reference;
    using const_reference = typename Base::const_reference;
    using difference_type = typename Base::difference_type;
    using size_type = typename Base::size_type;
    using iterator = typename Base::iterator;
    using const_iterator = typename Base::const_iterator;
    using Base::data;
    using Base::size;
    using Base::empty;
    using Base::begin;
    using Base::end;
    using Base::operator [];

    Grid3D():
        m_nWidth(0),
        m_nHeight(0),
        m_nDepth(0),
        m_nSliceSize(0) {
    }

    Grid3D(size_t width, size_t height, size_t depth):
        Base(width * height * depth),
        m_nWidth(width),
        m_nHeight(height),
        m_nDepth(depth),
        m_nSliceSize(width * height) {
    }

    Grid3D(size_t width, size_t height, size_t depth, T value):
        Base(width * height * depth, value),
        m_nWidth(width),
        m_nHeight(height),
        m_nDepth(depth),
        m_nSliceSize(width * height) {
    }

    Grid3D(const Vec3u& resolution): Grid3D(resolution.x, resolution.y, resolution.z) {
    }

    Grid3D(const Vec3u &resolution, T value): Grid3D(resolution.x, resolution.y, resolution.z, value) {
    }

    uint32_t offset(uint32_t x, uint32_t y, uint32_t z) const {
        return x + y * m_nWidth + z * m_nSliceSize;
    }

    uint32_t offset(const Vec3i& coords) const {
        return offset(coords.x, coords.y, coords.z);
    }

    Vec3i coords(uint32_t offset) const {
        Vec3i c;
        c.x = offset % m_nWidth;
        c.y = ((offset - c.x) / m_nWidth) % m_nHeight;
        c.z = ((offset - c.x - c.y * m_nWidth)) / m_nSliceSize;
        return c;
    }

    value_type operator ()(uint32_t x, uint32_t y, uint32_t z) const {
        return (*this)[offset(x, y, z)];
    }

    reference operator ()(uint32_t x, uint32_t y, uint32_t z) {
        return (*this)[offset(x, y, z)];
    }

    value_type operator ()(const Vec3i& coords) const {
        return (*this)(coords.x, coords.y, coords.z);
    }

    reference operator ()(const Vec3i& coords) {
        return (*this)(coords.x, coords.y, coords.z);
    }

    bool contains(int x, int y, int z) const {
        return x >= 0 &&
                y >= 0 &&
                z >= 0 &&
                x < (int)m_nWidth &&
                y < (int)m_nHeight &&
                z < (int)m_nDepth;
    }

    bool contains(const Vec3i& coords) const {
        return contains(coords.x, coords.y, coords.z);
    }

    size_t width() const {
        return m_nWidth;
    }

    size_t height() const {
        return m_nHeight;
    }

    size_t depth() const {
        return m_nDepth;
    }

    Vec3u resolution() const {
        return Vec3u(m_nWidth, m_nHeight, m_nDepth);
    }

    template<typename Functor>
    void forEach(const Functor& f) const {
        auto idx = 0u;
        for(auto z = 0u; z < m_nDepth; ++z)  {
            for(auto y = 0u; y < m_nHeight; ++y) {
                for(auto x = 0u; x < m_nWidth; ++x) {
                    f(x, y, z, (*this)[idx++]);
                }
            }
        }
    }

    template<typename Functor>
    void forEach(const Functor& f) {
        auto idx = 0u;
        for(auto z = 0u; z < m_nDepth; ++z)  {
            for(auto y = 0u; y < m_nHeight; ++y) {
                for(auto x = 0u; x < m_nWidth; ++x) {
                    f(x, y, z, (*this)[idx++]);
                }
            }
        }
    }

private:
    size_t m_nWidth, m_nHeight, m_nDepth;
    size_t m_nSliceSize;
};

template<>
class Grid3D<bool> : std::vector<bool>{
    typedef std::vector<bool> Base;
public:
    typedef Base::value_type value_type;
    typedef Base::reference reference;
    typedef Base::iterator iterator;
    typedef Base::const_iterator const_iterator;
    using Base::size;
    using Base::empty;
    using Base::begin;
    using Base::end;
    using Base::operator [];

    Grid3D() :
        m_nWidth(0),
        m_nHeight(0),
        m_nDepth(0),
        m_nSliceSize(0) {
    }

    Grid3D(size_t width, size_t height, size_t depth) :
        Base(width * height * depth),
        m_nWidth(width),
        m_nHeight(height),
        m_nDepth(depth),
        m_nSliceSize(width * height) {
    }

    Grid3D(size_t width, size_t height, size_t depth, bool value) :
        Base(width * height * depth, value),
        m_nWidth(width),
        m_nHeight(height),
        m_nDepth(depth),
        m_nSliceSize(width * height) {
    }

    Grid3D(const Vec3u& resolution): Grid3D(resolution.x, resolution.y, resolution.z) {
    }

    Grid3D(const Vec3u &resolution, bool value): Grid3D(resolution.x, resolution.y, resolution.z, value) {
    }

    uint32_t offset(uint32_t x, uint32_t y, uint32_t z) const {
        return uint32_t(x + y * m_nWidth + z * m_nSliceSize);
    }

    uint32_t offset(const Vec3i& coords) const {
        return offset(coords.x, coords.y, coords.z);
    }

    Vec3i coords(uint32_t offset) const {
        Vec3i c;
        c.x = offset % m_nWidth;
        c.y = ((offset - c.x) / m_nWidth) % m_nHeight;
        c.z = ((offset - c.x - c.y * m_nWidth)) / m_nSliceSize;
        return c;
    }

    value_type operator ()(uint32_t x, uint32_t y, uint32_t z) const {
        return (*this)[offset(x, y, z)];
    }

    reference operator ()(uint32_t x, uint32_t y, uint32_t z) {
        return (*this)[offset(x, y, z)];
    }

    value_type operator ()(const Vec3i& coords) const {
        return (*this)(coords.x, coords.y, coords.z);
    }

    reference operator ()(const Vec3i& coords) {
        return (*this)(coords.x, coords.y, coords.z);
    }

    bool contains(int x, int y, int z) const {
        return x >= 0 &&
            y >= 0 &&
            z >= 0 &&
            x < (int)m_nWidth &&
            y < (int)m_nHeight &&
            z < (int)m_nDepth;
    }

    bool contains(const Vec3i& coords) const {
        return contains(coords.x, coords.y, coords.z);
    }

    size_t width() const {
        return m_nWidth;
    }

    size_t height() const {
        return m_nHeight;
    }

    size_t depth() const {
        return m_nDepth;
    }

    Vec3u resolution() const {
        return Vec3u(m_nWidth, m_nHeight, m_nDepth);
    }

private:
    size_t m_nWidth, m_nHeight, m_nDepth;
    size_t m_nSliceSize;
};

template<typename Functor>
void processXCut(const Vec3u& gridSize, uint32_t cutPosition, Functor f) {
    for(auto z = 0u; z < gridSize.z; ++z) {
        for(auto y = 0u; y < gridSize.y; ++y) {
            f(y, z, Vec3i(cutPosition, y, z));
        }
    }
}

template<typename Functor>
void processYCut(const Vec3u& gridSize, uint32_t cutPosition, Functor f) {
    for(auto z = 0u; z < gridSize.z; ++z) {
        for(auto x = 0u; x < gridSize.x; ++x) {
            f(z, x, Vec3i(x, cutPosition, z));
        }
    }
}

template<typename Functor>
void processZCut(const Vec3u& gridSize, uint32_t cutPosition, Functor f) {
    for(auto y = 0u; y < gridSize.y; ++y) {
        for(auto x = 0u; x < gridSize.x; ++x) {
            f(x, y, Vec3i(x, y, cutPosition));
        }
    }
}

template<typename Functor>
void processCut(const Vec3u& gridSize, uint32_t cutPosition, Grid3DCutAxis cutAxis, Functor f) {
    switch(cutAxis) {
    case Grid3DCutAxis::XAxis:
        processXCut(gridSize, cutPosition, f);
        break;
    case Grid3DCutAxis::YAxis:
        processYCut(gridSize, cutPosition, f);
        break;
    case Grid3DCutAxis::ZAxis:
        processZCut(gridSize, cutPosition, f);
        break;
    }
}

inline Vec2u getCutSize(const Vec3u& gridSize, Grid3DCutAxis cutAxis) {
    switch(cutAxis) {
    case Grid3DCutAxis::XAxis:
        return Vec2u(gridSize.y, gridSize.z);
    case Grid3DCutAxis::YAxis:
        return Vec2u(gridSize.z, gridSize.x);
    case Grid3DCutAxis::ZAxis:
        return Vec2u(gridSize.x, gridSize.y);
    }
    return Vec2u(0);
}

template<typename Functor>
inline void foreach6Neighbour(const Vec3u& gridSize, const Vec3i& voxel, Functor f) {
    if(voxel.x > 0) {
        f(Vec3i(voxel.x - 1, voxel.y, voxel.z));
    }
    if(voxel.y > 0) {
        f(Vec3i(voxel.x, voxel.y - 1, voxel.z));
    }
    if(voxel.z > 0) {
        f(Vec3i(voxel.x, voxel.y, voxel.z - 1));
    }
    if(voxel.x < int(gridSize.x) - 1) {
        f(Vec3i(voxel.x + 1, voxel.y, voxel.z));
    }
    if(voxel.y < int(gridSize.y) - 1) {
        f(Vec3i(voxel.x, voxel.y + 1, voxel.z));
    }
    if(voxel.z < int(gridSize.z) - 1) {
        f(Vec3i(voxel.x, voxel.y, voxel.z + 1));
    }
}

template<typename Functor>
inline void foreach26Neighbour(const Vec3u& gridSize, const Vec3i& voxel, Functor f) {
    for(auto k = -1; k <= 1; ++k) {
        for(auto j = -1; j <= 1; ++j) {
            for(auto i = -1; i <= 1; ++i) {
                if(i != 0 || j != 0 || k != 0) {
                    Vec3i neighbour = voxel + Vec3i(i, j, k);
                    if(neighbour.x >= 0 && neighbour.y >= 0 && neighbour.z >= 0 &&
                            neighbour.x < int(gridSize.x) && neighbour.y < int(gridSize.y) && neighbour.z < int(gridSize.z)) {
                        f(neighbour);
                    }
                }
            }
        }
    }
}

template<typename Functor>
inline void foreachVoxel(const Vec3u& gridSize, Functor f) {
    for (auto z = 0u; z < gridSize.z; ++z) {
        for (auto y = 0u; y < gridSize.y; ++y) {
            for (auto x = 0u; x < gridSize.x; ++x) {
                f(Vec3i(x, y, z));
            }
        }
    }
}

}
