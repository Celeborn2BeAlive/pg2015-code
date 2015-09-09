#pragma once

#define GLM_SIMD_ENABLE_XYZW_UNION
#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/simd_vec4.hpp>
#include <glm/gtx/bit.hpp>
#include <glm/gtc/round.hpp>

namespace BnZ {

template<typename T>
struct VecTraits;

template<typename T, glm::precision p>
struct VecTraits<glm::tvec1<T, p>> {
    static const auto dim = 1u;
    typedef T Scalar;
};

template<typename T, glm::precision p>
struct VecTraits<glm::tvec2<T, p>> {
    static const auto dim = 2u;
    typedef T Scalar;
};

template<typename T, glm::precision p>
struct VecTraits<glm::tvec3<T, p>> {
    static const auto dim = 3u;
    typedef T Scalar;
};

template<typename T, glm::precision p>
struct VecTraits<glm::tvec4<T, p>> {
    static const auto dim = 4u;
    typedef T Scalar;
};

using glm::distance;
using glm::length;
using glm::radians;
using glm::degrees;
using glm::dot;
using glm::cross;
using glm::cos;
using glm::sin;
using glm::tan;
using glm::abs;
using glm::floor;
using glm::sign;
using glm::max;
using glm::min;
using glm::translate;
using glm::rotate;
using glm::scale;
using glm::perspective;
using glm::ortho;
using glm::lookAt;
using glm::clamp;
using glm::normalize;
using glm::inverse;
using glm::fract;
using glm::pow;
using glm::sqrt;
using glm::isnan;
using glm::isinf;
using glm::isfinite;
using glm::isPowerOfTwo;
using glm::value_ptr;
using glm::determinant;

}
