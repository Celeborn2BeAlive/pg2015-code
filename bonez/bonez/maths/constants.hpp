#pragma once

#include "types.hpp"

namespace BnZ {

using glm::zero;
using glm::pi;
using glm::one_over_pi;
using glm::two_over_pi;

template <typename genType>
inline genType two_pi() {
    return genType(2.0 * pi<genType>());
}

template <typename genType>
inline genType four_pi() {
    return genType(4.0 * pi<genType>());
}

template <typename genType>
inline genType one_over_two_pi() {
    return genType(0.5 * one_over_pi<genType>());
}

template <typename genType>
inline genType one_over_four_pi() {
    return genType(0.25 * one_over_pi<genType>());
}

}
