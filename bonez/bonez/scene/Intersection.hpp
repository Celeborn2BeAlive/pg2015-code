#pragma once

#include "SurfacePoint.hpp"

namespace BnZ {

struct Intersection: public SurfacePoint {
    float distance = 0.f;

    // Emitted radiance towards incident direction
    Vec3f Le = Vec3f(0.f);

    using SurfacePoint::SurfacePoint;

    explicit operator bool() const {
        return distance > 0.f;
    }

private:
    friend class Scene;
};



}
