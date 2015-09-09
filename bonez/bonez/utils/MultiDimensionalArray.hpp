#pragma once

#include <vector>
#include <array>

namespace BnZ {

template<typename T, std::size_t Dimension, typename Alloc>
class MultiDimensionalArray;

template<typename T, typename Alloc = std::allocator<T>>
using Array2d = MultiDimensionalArray<T, 2, Alloc>;

template<typename T, typename Alloc = std::allocator<T>>
using Array3d = MultiDimensionalArray<T, 3, Alloc>;

// A multidimensional array stored contiguously in memory
template<typename T, std::size_t Dimension, typename Alloc = std::allocator<T>>
class MultiDimensionalArray: std::vector<T, Alloc> {
    using Container = std::vector<T, Alloc>;
public:
    static const std::size_t dimension = Dimension;

    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;

    using Container::begin;
    using Container::end;
    using Container::data;
    using Container::size;
    using Container::empty;
    using Container::operator [];

    MultiDimensionalArray() = default;

    template<typename... Us>
    MultiDimensionalArray(std::size_t size0, Us&&... sizes):
        Container(totalSize(size0, std::forward<Us>(sizes)...)) {
        checkDimension(size0, sizes...);
        setSizes(0u, 1u, size0, std::forward<Us>(sizes)...);
    }

//    template<typename U, typename Alloc2>
//    MultiDimensionalArray(const MultiDimensionalArray<U, dimension, Alloc2>&) = default;

//    //MultiDimensionalArray(MultiDimensionalArray<T, dimension, Alloc>&&) = default;

//    template<typename U, typename Alloc2>
//    MultiDimensionalArray& operator =(const MultiDimensionalArray<U, dimension, Alloc2>&) = default;

    //MultiDimensionalArray& operator =(MultiDimensionalArray<T, dimension, Alloc>&&) = default;

    template<typename... Us>
    void resize(Us&&... sizes) {
        static_assert(sizeof...(sizes) == dimension, "Number of size arguments should be the same as the dimension of the MultiBuffer.");
        if(!sameSize(0u, std::forward<Us>(sizes)...)) {
            Container::clear();
            Container::resize(totalSize(std::forward<Us>(sizes)...));
            setSizes(0u, 1u, std::forward<Us>(sizes)...);
        }
    }

    template<typename... Indices>
    std::size_t offset(Indices&&... indices) const {
        return computeElementIndex(0u, std::forward<Indices>(indices)...);
    }

    template<typename... Indices>
    T& operator ()(Indices&&... indices) {
        static_assert(sizeof...(indices) == dimension, "Number of indices arguments should be the same as the dimension of the MultiBuffer.");
        return (*this)[computeElementIndex(0u, std::forward<Indices>(indices)...)];
    }

    template<typename... Indices>
    const T& operator ()(Indices&&... indices) const {
        static_assert(sizeof...(indices) == dimension, "Number of indices arguments should be the same as the dimension of the MultiBuffer.");
        return (*this)[computeElementIndex(0u, std::forward<Indices>(indices)...)];
    }

    // Return a pointer to a slice of lower dimension.
    // The dimension of the slice is dimension - sizeof...(indices)
    // The returned pointer is the address of the element of index (0, ..., 0, indices_1, ..., indices_k)
    template<typename... Indices>
    const T* getSlicePtr(Indices&&... indices) const {
        static_assert(sizeof...(indices) < dimension, "Number of indices arguments should be less than the dimension of the MultiBuffer.");
        const auto i = dimension - sizeof...(indices);
        return data() + computeElementIndex(i, std::forward<Indices>(indices)...);
    }

    template<typename... Indices>
    T* getSlicePtr(Indices&&... indices) {
        static_assert(sizeof...(indices) < dimension, "Number of indices arguments should be less than the dimension of the MultiBuffer.");
        const auto i = dimension - sizeof...(indices);
        return data() + computeElementIndex(i, std::forward<Indices>(indices)...);
    }

    std::size_t size(std::size_t dimensionIndex) const {
        return m_nSizes[dimensionIndex];
    }

private:
    template<typename... Us>
    void checkDimension(Us&&... sizes) {
        static_assert(sizeof...(sizes) == dimension, "Number of size arguments should be the same as the dimension of the MultiBuffer.");
    }

    inline std::size_t computeElementIndex(std::size_t index) const {
        return std::size_t(0u);
    }

    template<typename Index, typename... Indices>
    inline std::size_t computeElementIndex(std::size_t index, Index&& i1, Indices&&... indices) const {
        auto tmp = computeElementIndex(index + 1, std::forward<Indices>(indices)...);
        return m_nStrides[index] * i1 + tmp;
    }

    inline bool sameSize(std::size_t index) const {
        return true;
    }

    template<typename U, typename... Us>
    inline bool sameSize(std::size_t index, U&& s1, Us&&... s) const {
        return m_nSizes[index] == s1 && sameSize(index + 1, std::forward<Us>(s)...);
    }

    inline std::size_t totalSize() {
        return std::size_t(1u);
    }

    template<typename U, typename... Us>
    inline std::size_t totalSize(U&& s1, Us&&... s) {
        return s1 * totalSize(std::forward<Us>(s)...);
    }

    inline void setSizes(std::size_t index, std::size_t stride) {
    }

    template<typename U, typename... Us>
    inline void setSizes(std::size_t index, std::size_t stride, U&& s1, Us&&... s) {
        m_nSizes[index] = s1;
        m_nStrides[index] = stride;
        setSizes(index + 1, stride * s1, std::forward<Us>(s)...);
    }

    std::array<std::size_t, dimension> m_nSizes = { { 0 } };
    std::array<std::size_t, dimension> m_nStrides = { { 0 } };
};

}
