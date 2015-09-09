#ifndef _BONEZ_INDEXER_HPP
#define _BONEZ_INDEXER_HPP

#include "iterator_extractor.hpp"
#include <iterator>

namespace BnZ {

template <typename T>
class Range {
public:
    class iterator: public std::iterator<std::forward_iterator_tag, T, std::ptrdiff_t, const T*, const T> {
    public:
        iterator(T value): m_Value(value) {}

        const T& operator*() const { return m_Value; }

        iterator& operator++() { ++m_Value; return *this; }
        iterator operator++(int) { iterator tmp(m_Value); ++*this; return tmp; }

        bool operator ==(iterator const& it) const { return m_Value == it.m_Value; }
        bool operator !=(iterator const& it) const { return !(*this == it); }

    private:
        T m_Value;
    };

    Range(const T& start, const T& end): m_Start(start), m_End(end) {}

    iterator begin() const { return iterator(m_Start); }
    iterator end() const { return iterator(m_End); }

private:
    T m_Start, m_End;
};

template <typename T>
inline Range<T> range(const T& start, const T& end) {
    return Range<T>(std::min(start, end), end);
}

template<typename T>
inline Range<T> range(const T& end) {
    return range(T(), end);
}

}

#endif // _BONEZ_RANGE_HPP
