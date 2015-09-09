#ifndef _BONEZ_RANGE_HPP
#define _BONEZ_RANGE_HPP

#include "iterator_extractor.hpp"
#include <iterator>

namespace BnZ {

template <typename T>
class Indexer {
public:
    class iterator {
        typedef typename iterator_extractor<T>::type inner_iterator;

        typedef typename std::iterator_traits<inner_iterator>::reference inner_reference;
    public:
        typedef std::pair<size_t, inner_reference> reference;

        iterator(inner_iterator it): _pos(0), _it(it) {}

        reference operator*() const { return reference(_pos, *_it); }

        iterator& operator++() { ++_pos; ++_it; return *this; }
        iterator operator++(int) { iterator tmp(*this); ++*this; return tmp; }

        bool operator==(iterator const& it) const { return _it == it._it; }
        bool operator!=(iterator const& it) const { return !(*this == it); }

    private:
        size_t _pos;
        inner_iterator _it;
    };

    Indexer(T& t): _container(t) {}

    iterator begin() const { return iterator(_container.begin()); }
    iterator end() const { return iterator(_container.end()); }

private:
    T& _container;
}; // class Indexer

template <typename T>
inline Indexer<T> index(T& t) {
    return Indexer<T>(t);
}

}

#endif // _BONEZ_RANGE_HPP
