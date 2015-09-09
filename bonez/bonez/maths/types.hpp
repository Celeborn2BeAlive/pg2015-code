#pragma once

#include "glm.hpp"

namespace BnZ {

using Vec2b = glm::bvec2;
using Vec3b = glm::bvec3;
using Vec4b = glm::bvec4;
using Vec2u = glm::uvec2;
using Vec3u = glm::uvec3;
using Vec4u = glm::uvec4;
using Vec2i = glm::ivec2;
using Vec3i = glm::ivec3;
using Vec4i = glm::ivec4;
using Vec2f = glm::vec2;
using Vec3f = glm::vec3;
using Vec4f = glm::vec4;
using Vec2d = glm::dvec2;
using Vec3d = glm::dvec3;
using Vec4d = glm::dvec4;
using Mat2f = glm::mat2;
using Mat3f = glm::mat3;
using Mat4f = glm::mat4;
using Mat2x3f = glm::mat2x3;
using Mat3x2f = glm::mat3x2;
using Mat2x4f = glm::mat2x4;
using Mat4x2f = glm::mat4x2;
using Mat3x4f = glm::mat3x4;
using Mat4x3f = glm::mat4x3;

struct Col3f: public Vec3f {
    Col3f() = default;

    template<typename T>
    Col3f(T&& v): Vec3f(std::move(v)) {
    }
};

struct Col4f: public Vec4f {
    Col4f() = default;

    template<typename T>
    Col4f(T&& v): Vec4f(std::move(v)) {
    }
};

inline Vec3f badColor() {
    return Vec3f(199, 21, 133) / 255.f;
}

}
