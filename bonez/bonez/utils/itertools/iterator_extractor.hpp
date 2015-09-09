#ifndef _BONEZ_ITERATOREXTRACTOR_HPP_
#define _BONEZ_ITERATOREXTRACTOR_HPP_

namespace BnZ {

template <typename T>
struct iterator_extractor { typedef typename T::iterator type; };

template <typename T>
struct iterator_extractor<T const> { typedef typename T::const_iterator type; };

}

#endif
