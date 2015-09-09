#pragma once

#include "types.hpp"
#include "common.hpp"
#include "constants.hpp"
#include "mappings/image_mappings.hpp"
#include "mappings/paraboloid_mappings.hpp"
#include "mappings/dual_paraboloid_mappings.hpp"
#include "mappings/spherical_mappings.hpp"
#include "mappings/hemispherical_mappings.hpp"

namespace BnZ {

inline Vec3f getColor(uint32_t n) {
    return fract(
                sin(
                    float(n + 1) * Vec3f(12.9898, 78.233, 56.128)
                    )
                * 43758.5453f
                );
}

/**
 * @brief cartesianToSpherical
 * @param direction a unit length 3D vector
 * @return Spherical coordinates (phi, theta) of direction with phi in [0, 2pi[ and theta in [-pi, pi]
 */
inline Vec2f cartesianToSpherical(const Vec3f& direction) {
    return Vec2f(atan2(direction.x, direction.z), asin(direction.y));
}

/**
 * @brief sphericalToCartesian
 * @param angles The spherical angles: angles.x is phi and angles.y is theta
 * @return Cartesian coordinates
 */
inline Vec3f sphericalToCartesian(const Vec2f& angles) {
    auto cosTheta = cos(angles.y);
    return Vec3f(sin(angles.x) * cosTheta,
                     sin(angles.y),
                     cos(angles.x) * cosTheta);
}

inline Vec3f getOrthogonalUnitVector(const Vec3f& v) {
    if(abs(v.y) > abs(v.x)) {
        float rcpLength = 1.f / length(Vec2f(v.x, v.y));
        return rcpLength * Vec3f(v.y, -v.x, 0.f);
    }
    float rcpLength = 1.f / length(Vec2f(v.x, v.z));
    return rcpLength * Vec3f(v.z, 0.f, -v.x);
}

inline Mat3f frameY(const Vec3f& yAxis) {
    Mat3f matrix;

    matrix[1] = Vec3f(yAxis);
    if(abs(yAxis.y) > abs(yAxis.x)) {
        float rcpLength = 1.f / length(Vec2f(yAxis.x, yAxis.y));
        matrix[0] = rcpLength * Vec3f(yAxis.y, -yAxis.x, 0.f);
    } else {
        float rcpLength = 1.f / length(Vec2f(yAxis.x, yAxis.z));
        matrix[0] = rcpLength * Vec3f(yAxis.z, 0.f, -yAxis.x);
    }
    matrix[2] = cross(matrix[0], yAxis);
    return matrix;
}

// zAxis should be normalized
inline Mat3f frameZ(const Vec3f& zAxis) {
    Mat3f matrix;

    matrix[2] = Vec3f(zAxis);
    if(abs(zAxis.y) > abs(zAxis.x)) {
        float rcpLength = 1.f / length(Vec2f(zAxis.x, zAxis.y));
        matrix[0] = rcpLength * Vec3f(zAxis.y, -zAxis.x, 0.f);
    } else {
        float rcpLength = 1.f / length(Vec2f(zAxis.x, zAxis.z));
        matrix[0] = rcpLength * Vec3f(zAxis.z, 0.f, -zAxis.x);
    }
    matrix[1] = cross(zAxis, matrix[0]);
    return matrix;
}

inline Mat4f frameY(const Vec3f& origin, const Vec3f& yAxis) {
    Mat4f matrix;
    matrix[3] = Vec4f(origin, 1);

    matrix[1] = Vec4f(yAxis, 0);
    if(abs(yAxis.y) > abs(yAxis.x)) {
        float rcpLength = 1.f / length(Vec2f(yAxis.x, yAxis.y));
        matrix[0] = rcpLength * Vec4f(yAxis.y, -yAxis.x, 0.f, 0.f);
    } else {
        float rcpLength = 1.f / length(Vec2f(yAxis.x, yAxis.z));
        matrix[0] = rcpLength * Vec4f(yAxis.z, 0.f, -yAxis.x, 0.f);
    }
    matrix[2] = Vec4f(cross(Vec3f(matrix[0]), yAxis), 0);
    return matrix;
}

inline void faceForward(const Vec3f& wi, Vec3f& N) {
    if(dot(wi, N) < 0.f) {
        N = -N;
    }
}

inline float reduceMax(const Vec3f& v) {
    return max(v.x, max(v.y, v.z));
}

inline bool reduceLogicalOr(const Vec3b& v) {
    return v.x || v.y || v.z;
}

inline bool reduceLogicalOr(const Vec4b& v) {
    return v.x || v.y || v.z || v.w;
}

// Return the index of the maximal component of the vector
inline uint32_t maxComponent(const Vec3f& v) {
    auto m = reduceMax(v);
    if(m == v.x) {
        return 0u;
    }
    if(m == v.y) {
        return 1u;
    }
    return 2u;
}

inline bool isInvalidMeasurementEstimate(const Vec3f& c) {
    return c.r < 0.f || c.g < 0.f || c.b < 0.f ||
            reduceLogicalOr(isnan(c)) || reduceLogicalOr(isinf(c));
}

inline bool isInvalidMeasurementEstimate(const Vec4f& c) {
    return isInvalidMeasurementEstimate(Vec3f(c));
}

inline float luminance(const Vec3f& color) {
    return 0.212671f * color.r + 0.715160f * color.g + 0.072169f * color.b;
}

template<typename T> inline T sin2cos ( const T& x )  {
    return sqrt(max(T(0.f), T(1.f) - x * x));
}

template<typename T> inline T cos2sin ( const T& x )  {
    return sin2cos(x);
}

inline Mat4f getViewMatrix(const Vec3f& origin) {
    return glm::translate(glm::mat4(1.f), -origin);
}

// Return a view matrix located at origin and looking toward up direction
inline Mat4f getViewMatrix(const Vec3f& origin, const Vec3f& up) {
    auto frame = frameZ(-up);
    Mat4f m(frame);
    m[3] = Vec4f(origin, 1);
    return inverse(m);
}

inline bool viewportContains(const Vec2f& position, const Vec4f& viewport) {
    return position.x >= viewport.x && position.y >= viewport.y &&
            position.x < viewport.x + viewport.z && position.y < viewport.y + viewport.w;
}

inline bool viewportContains(const Vec2u& position, const Vec4u& viewport) {
    return position.x >= viewport.x && position.y >= viewport.y &&
            position.x < viewport.x + viewport.z && position.y < viewport.y + viewport.w;
}

inline Vec3f refract(const Vec3f& I, const Vec3f& N, float eta) {
    auto dotI = dot(I, N);

    auto sqrDotO = 1 - sqr(eta) * (1 - sqr(dotI));
    if(sqrDotO < 0.f) {
        return zero<Vec3f>();
    }
    auto k = sqrt(sqrDotO);
    return -eta * I + (eta * dotI - sign(dotI) * k) * N;
}

inline Vec3f reflect(const Vec3f& I, const Vec3f& N) {
    return glm::reflect(-I, N);
}

// Same convention as OpenGL:
enum class CubeFace {
    POS_X = 0,
    NEG_X = 1,
    POS_Y = 2,
    NEG_Y = 3,
    POS_Z = 4,
    NEG_Z = 5,
    FACE_COUNT = 6
};

inline CubeFace getCubeFace(const Vec3f& wi) {
    Vec3f absWi = abs(wi);

    float maxComponent = max(absWi.x, max(absWi.y, absWi.z));

    if(maxComponent == absWi.x) {
        if(wi.x > 0) {
            return CubeFace::POS_X;
        }
        return CubeFace::NEG_X;
    }

    if(maxComponent == absWi.y) {
        if(wi.y >= 0) {
            return CubeFace::POS_Y;
        }
        return CubeFace::NEG_Y;
    }

    if(maxComponent == absWi.z) {
        if(wi.z > 0) {
            return CubeFace::POS_Z;
        }
    }

    return CubeFace::NEG_Z;
}

}
