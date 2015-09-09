#pragma once

#include <bonez/types.hpp>
#include "bonez/sampling/Sample.hpp"
#include "bonez/maths/maths.hpp"

namespace BnZ {

struct SurfacePoint {
    Vec3f P;
    Vec3f Ng, Ns;
    Vec2f uv;
    Vec2f texCoords;
    uint32_t meshID, triangleID;

    SurfacePoint() = default;

    SurfacePoint(const Vec3f& P, const Vec3f& N):
        P(P), Ng(N), Ns(N), uv(0.f), texCoords(0.f), meshID(-1), triangleID(-1) {
    }
};

using SurfacePointSample = Sample<SurfacePoint>;

inline float geometricFactor(const SurfacePoint& x, const SurfacePoint& y,
                             Vec3f& incidentDirectionOnX, float& distance) {
    incidentDirectionOnX = y.P - x.P;
    distance = length(incidentDirectionOnX);
    if(distance == 0.f) {
        return 0.f;
    }
    incidentDirectionOnX /= distance;
    return abs(dot(x.Ns, incidentDirectionOnX)) * abs(dot(y.Ns, -incidentDirectionOnX))
            / sqr(distance);
}

inline float geometricFactor(const SurfacePoint& x, const SurfacePoint& y) {
    Vec3f wi;
    float d;
    return geometricFactor(x, y, wi, d);
}

}
