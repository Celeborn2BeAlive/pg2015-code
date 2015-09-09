#pragma once

#include "Camera.hpp"
#include <bonez/maths/maths.hpp>

namespace BnZ {

class EnvironmentCamera: public Camera {
public:
    EnvironmentCamera() = default;

    EnvironmentCamera(const Mat4f& viewMatrix): Camera(viewMatrix) {
    }

//    Ray doGetRay(const Vec2f& ndcPosition) const override {
//        auto uv = ndcToUV(ndcPosition);
//        auto localDir = Vec4f(sphericalMapping(uv), 0.f);
//        return Ray(getPosition(), Vec3f(getRcpViewMatrix() * localDir));
//    }

//    bool getNDC(const Vec3f& P, Vec2f& ndc) const override {
//        auto globalDir = normalize(P - getPosition());
//        auto localDir = Vec3f(getViewMatrix() * Vec4f(globalDir, 0));
//        auto uv = rcpSphericalMapping(localDir);
//        ndc = uvToNDC(uv);
//        return true;
//    }

//    float surfaceToImageFactor(const SurfacePoint& point) const override {
//        return 0.f;
//    }
};

}
