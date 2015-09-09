#pragma once

#include <memory>

namespace BnZ {

template<typename T>
using Shared = std::shared_ptr<T>;

template<typename T>
using Unique = std::unique_ptr<T>;

template<typename T, typename... Ts>
Shared<T> makeShared(Ts&&... ts) {
    return std::make_shared<T>(std::forward<Ts>(ts)...);
}

template<typename T, typename... Ts>
Unique<T> makeUnique(Ts&&... ts) {
    return Unique<T>(new T(std::forward<Ts>(ts)...));
}

template<typename T>
Unique<T[]> makeUniqueArray(size_t size) {
    return Unique<T[]>(new T[size]);
}

}
