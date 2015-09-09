#pragma once

#include <ostream>
#include <bonez/types.hpp>

namespace BnZ {

template<typename T>
struct Sample {
    T value;
    float pdf = 0.f;

    Sample() = default;

    template<typename U>
    Sample(U&& value, float pdf): value(std::forward<U>(value)), pdf(pdf) {
    }

    /*
    explicit operator bool() const {
        return pdf > 0.f;
    }*/
};

using Sample1f = Sample<float>;
using Sample2f = Sample<Vec2f>;
using Sample3f = Sample<Vec3f>;
using Sample1u = Sample<uint32_t>;
using Sample2u = Sample<Vec2u>;

template<typename T>
inline std::ostream& operator <<(std::ostream& out, const Sample<T>& s) {
    out << "[ " << s.value << ", pdf = " << s.pdf << " ] ";
    return out;
}

}
