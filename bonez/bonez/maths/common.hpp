#pragma once

#include "glm.hpp"
#include "types.hpp"

namespace BnZ {

template<typename T>
inline T sqr(const T& value) {
    return value * value;
}

inline float distanceSquared(const Vec3f& A, const Vec3f& B) {
    auto v = B - A;
    return dot(v, v);
}

inline float lengthSquared(const Vec3f& v) {
    return dot(v, v);
}

}
