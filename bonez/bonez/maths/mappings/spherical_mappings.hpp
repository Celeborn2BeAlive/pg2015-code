#pragma once

#include "../common.hpp"
#include "../constants.hpp"
#include "image_mappings.hpp"

namespace BnZ {

inline Vec2f getSphericalAngles(const Vec2f& uv) {
    return uv * Vec2f(two_pi<float>(), pi<float>());
}

// Standard spherical mapping [0,1]x[0,1] -> UnitSphere
inline Vec3f sphericalMapping(const Vec2f& uv, float& sinTheta) {
    auto phiTheta = getSphericalAngles(uv);
    sinTheta = sin(phiTheta.y);

    return Vec3f(cos(phiTheta.x) * sinTheta,
                 sin(phiTheta.x) * sinTheta,
                 cos(phiTheta.y));
}

inline Vec3f sphericalMapping(const Vec2f& uv) {
    float sinTheta;
    return sphericalMapping(uv, sinTheta);
}

inline float sphericalMappingJacobian(const Vec2f& uv, float& sinTheta) {
    auto phiTheta = getSphericalAngles(uv);
    sinTheta = sin(phiTheta.y);
    return abs(two_pi<float>() * pi<float>() * sinTheta);
}

inline float sphericalMappingJacobian(const Vec2f& uv) {
    float sinTheta;
    return sphericalMappingJacobian(uv, sinTheta);
}

inline Vec2f rcpSphericalMapping(const Vec3f& wi, float& sinTheta) {
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

    return phiTheta * Vec2f(one_over_two_pi<float>(), one_over_pi<float>());
}

inline Vec2f rcpSphericalMapping(const Vec3f& wi) {
    float sinTheta;
    return rcpSphericalMapping(wi, sinTheta);
}

inline float rcpSphericalMappingJacobian(const Vec3f& wi, float& sinTheta) {
    sinTheta = sqrt(sqr(wi.x) + sqr(wi.y));
    return sinTheta == 0.f ? 0.f : abs(1.f / (two_pi<float>() * pi<float>() * sinTheta));
}

inline float rcpSphericalMappingJacobian(const Vec3f& wi) {
    float sinTheta;
    return rcpSphericalMappingJacobian(wi, sinTheta);
}

}
