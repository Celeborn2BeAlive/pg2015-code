#pragma once

#include "../common.hpp"
#include "image_mappings.hpp"

namespace BnZ {

inline Vec2f getHemisphericalAngles(const Vec2f& uv) {
    return uv * Vec2f(two_pi<float>(), 0.5f * pi<float>());
}

inline Vec3f hemisphericalMapping(const Vec2f& uv, float &sinTheta) {
    auto phiTheta = getHemisphericalAngles(uv);
    sinTheta = sin(phiTheta.y);

    return Vec3f(cos(phiTheta.x) * sinTheta,
                 sin(phiTheta.x) * sinTheta,
                 cos(phiTheta.y));
}

inline Vec3f hemisphericalMapping(const Vec2f& uv) {
    float sinTheta;
    return hemisphericalMapping(uv, sinTheta);
}

inline float hemisphericalMappingJacobian(const Vec2f& uv) {
    auto phiTheta = getHemisphericalAngles(uv);
    return sin(phiTheta.y) * sqr(pi<float>());
}

inline Vec2f rcpHemisphericalMapping(const Vec3f& wi, float& sinTheta) {
    sinTheta = sqrt(sqr(wi.x) + sqr(wi.y));
    Vec2f phiTheta;

    phiTheta.x = atan2(wi.y / sinTheta, wi.x / sinTheta);
    phiTheta.y = atan2(sinTheta, wi.z);

    if(phiTheta.x < 0.f) {
        phiTheta.x += two_pi<float>();
    }

    if(phiTheta.y < 0.f) {
        phiTheta.y += two_pi<float>();
    }

    return phiTheta * Vec2f(one_over_two_pi<float>(), two_over_pi<float>());
}

inline float rcpHemisphericalMappingJacobian(const Vec3f& wi) {
    auto sinTheta = sqrt(sqr(wi.x) + sqr(wi.y));
    if(sinTheta == 0.f) {
        return 0.f;
    }
    return 1.f / (sinTheta * sqr(pi<float>()));
}

inline Vec2f rcpHemisphericalMapping(const Vec3f& wi) {
    float sinTheta;
    return rcpHemisphericalMapping(wi, sinTheta);
}

}
