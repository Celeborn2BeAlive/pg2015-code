#ifndef _BONEZ_ITERTOOLS_HPP
#define _BONEZ_ITERTOOLS_HPP

#include <algorithm>
#include "Indexer.hpp"
#include "Range.hpp"

namespace BnZ {

template<typename Functor>
inline void repeat(size_t count, Functor f) {
    while(count--) {
        f();
    }
}

template<typename InputContainer, typename OutputContainer, typename UnaryOperator>
inline void transform(InputContainer&& in, OutputContainer&& out, UnaryOperator&& op) {
    std::transform(begin(in), end(in), begin(out), op);
}

template<typename Container, typename Generator>
inline void emplace_n(Container&& c, size_t count, Generator&& generator) {
    c.reserve(c.size() + count);
    while(count--) {
        c.emplace_back(generator());
    }
}

template<typename Container, typename Generator>
inline void emplace_n_indexed(Container&& c, size_t count, Generator&& generator) {
    c.reserve(c.size() + count);
    for(auto i = 0u; i < count; ++i) {
        c.emplace_back(generator(i));
    }
}

}

#endif // _BONEZ_ITERTOOLS_HPP
