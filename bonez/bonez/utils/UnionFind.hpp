#pragma once

#include <vector>
#include "itertools/Range.hpp"

namespace BnZ {

class UnionFind {
public:
    UnionFind() = default;

    UnionFind(size_t size):
        m_ParentArray(size), m_RankArray(size, 0) {
        for(auto i: range(size)) {
            m_ParentArray[i] = i;
        }
    }

    size_t size() const {
        return m_ParentArray.size();
    }

    size_t parent(size_t index) const {
        assert(index < size());
        return m_ParentArray[index];
    }

    size_t rank(size_t index) const {
        assert(index < size());
        return m_RankArray[index];
    }

    size_t find(size_t index) const {
        assert(index < size());
        auto p = parent(index);
        if(index == p) {
            return index;
        }
        return find(p);
    }

    size_t link(size_t x, size_t y) {
        assert(x < size() && y < size());
        if(rank(x) > rank(y)) {
            std::swap(x, y);
        }
        if(rank(x) == rank(y)) {
            ++m_RankArray[y];
        }
        m_ParentArray[x] = y;
        return y;
    }

private:
    std::vector<size_t> m_ParentArray;
    std::vector<size_t> m_RankArray;
};

}
