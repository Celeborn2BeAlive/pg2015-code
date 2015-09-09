#pragma once

#include <bonez/maths/maths.hpp>
#include <bonez/types.hpp>
#include <bonez/sampling/Sample.hpp>
#include "SurfacePoint.hpp"

namespace BnZ {

struct Ray {
    using PrimID = Vec2i;

    // Those two primitives are avoided when doing intersection and occlusion
    // to avoid self intersection
    PrimID orgPrim; // meshID + triangleID
    PrimID dstPrim; // meshID + triangleID
    Vec3f org;
    Vec3f dir;
    float tnear;
    float tfar;

    Ray() = default;

    Ray(const PrimID& orgPrim, const PrimID& dstPrim, const Vec3f& org, const Vec3f& dir, float n = 0.f, float f = FLT_MAX) :
        orgPrim(orgPrim), dstPrim(dstPrim), org(org), dir(dir), tnear(n), tfar(f) {
    }

    Ray(const Vec3f& org, const Vec3f& dir, float n = 0.f, float f = FLT_MAX):
        Ray(PrimID(-1), PrimID(-1), org, dir, n, f) {
    }

    Ray(const SurfacePoint& point, const Vec3f& dir, float f = FLT_MAX) :
        Ray(PrimID(point.meshID, point.triangleID), PrimID(-1), point.P, dir, 0, f) {
    }

    Ray(const SurfacePoint& point, const PrimID& dstPrim, const Vec3f& dir, float f = FLT_MAX) :
        Ray(PrimID(point.meshID, point.triangleID), dstPrim, point.P, dir, 0, f) {
    }

    // Constructors for shadow rays
    Ray(const Vec3f& org, const SurfacePoint& dst, const Vec3f& dir, float f) :
        Ray(PrimID(-1), PrimID(dst.meshID, dst.triangleID), org, dir, 0, f) {
    }

    Ray(const SurfacePoint& org, const SurfacePoint& dst, const Vec3f& dir, float f) :
        Ray(PrimID(org.meshID, org.triangleID), PrimID(dst.meshID, dst.triangleID), org.P, dir, 0, f) {
    }

    Ray(const SurfacePoint& org, const SurfacePoint& dst):
        orgPrim(org.meshID, org.triangleID),
        dstPrim(dst.meshID, dst.triangleID),
        org(org.P),
        dir(dst.P - org.P),
        tnear(0.f),
        tfar(length(dir)) {
        dir /= tfar; // Normalize the direction
    }

    explicit operator bool() const {
        return tnear != tfar;
    }
};

typedef Sample<Ray> RaySample;

inline Ray secondaryRay(const Vec3f& P, const Vec3f& dir, float tfar = FLT_MAX) {
    return Ray(P, dir, 0, tfar);
}

inline Ray secondaryRay(const SurfacePoint& point, const Vec3f& dir, float tfar = FLT_MAX) {
    return Ray(point, dir, tfar);
}

inline Ray shadowRay(const SurfacePoint& org, const SurfacePoint& dst, const Vec3f& dir, float tfar) {
    return Ray(org, dst, dir, tfar);
}

}
