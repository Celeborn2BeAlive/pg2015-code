#pragma once

#include "../common.hpp"
#include "image_mappings.hpp"

namespace BnZ {

inline Vec3f getDualParaboloidNormal(const Vec2f& uv) {
    if(uv.x < 0.5f) {
        return Vec3f(getNDC(uv, Vec2f(0.5f, 1.f)), 1.f);
    }
    auto ndc = getNDC(uv - Vec2f(0.5f, 0.f), Vec2f(0.5f, 1.f));
    ndc.x = -ndc.x;
    return Vec3f(ndc, -1.f);
}

inline Vec3f dualParaboloidMapping(const Vec2f& uv) {
    auto N = getDualParaboloidNormal(uv);
    auto scale = 2.f / dot(N, N);
    return scale * N - Vec3f(0, 0, N.z);
}

inline Vec2f rcpDualParaboloidMapping(const Vec3f& wi) {
    if(wi.z > 0.f) {
        // left
        auto scaledN = wi + Vec3f(0, 0, 1);
        auto N = scaledN / scaledN.z;
        return Vec2f(0.25f * (N.x + 1.f), 0.5f * (N.y + 1.f));
    }
    //right
    auto scaledN = wi + Vec3f(0, 0, -1);
    auto N = -scaledN / scaledN.z;
    return Vec2f(0.5f + 0.25f * (-N.x + 1.f), 0.5f * (N.y + 1.f));
}

}
