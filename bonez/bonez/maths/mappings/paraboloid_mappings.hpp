#pragma once

#include "../common.hpp"
#include "image_mappings.hpp"

namespace BnZ {

inline Vec3f getParaboloidNormal(const Vec2f& uv) {
    return Vec3f(getNDC(uv, Vec2f(1.f, 1.f)), 1.f);
}

inline Vec3f paraboloidMapping(const Vec2f& uv) {
    auto N = getParaboloidNormal(uv);
    auto scale = 2.f / dot(N, N);
    return scale * N - Vec3f(0, 0, N.z);
}

inline Vec2f rcpParaboloidMapping(const Vec3f& wi) {
    auto scaledN = wi + Vec3f(0, 0, 1);
    auto N = scaledN / scaledN.z;
    return Vec2f(0.5f * (N.x + 1.f), 0.5f * (N.y + 1.f));
}

inline float paraboloidMappingJacobian(const Vec2f& uv) {
    auto N = getParaboloidNormal(uv);
    auto d = dot(N, N);
    return 16.f / (d * d);
}

}
