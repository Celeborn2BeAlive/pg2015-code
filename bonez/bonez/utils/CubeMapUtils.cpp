#include "CubeMapUtils.hpp"

#include <bonez/maths/maths.hpp>

namespace BnZ {

void CubeMapUtils::computeFaceProjMatrices(float zNear, float zFar,
                                           Mat4f projMatrices[6]) {
    Mat4f projectionMatrix = perspective(radians(90.f), 1.f, zNear, zFar);

    projMatrices[0] = rotate(rotate(projectionMatrix, -radians(90.f), Vec3f(0, 1, 0)), radians(180.f), Vec3f(0, 0, 1)); // +X
    projMatrices[1] = rotate(rotate(projectionMatrix, -radians(270.f), Vec3f(0, 1, 0)), radians(180.f), Vec3f(0, 0, 1)); // -X
    projMatrices[2] = rotate(projectionMatrix, -radians(90.f), Vec3f(1, 0, 0)); // +Y
    projMatrices[3] = rotate(projectionMatrix, radians(90.f), Vec3f(1, 0, 0)); // -Y
    projMatrices[4] = rotate(rotate(projectionMatrix, -radians(180.f), Vec3f(0, 1, 0)), radians(180.f), Vec3f(0, 0, 1)); // +Z
    projMatrices[5] = rotate(projectionMatrix, radians(180.f), Vec3f(0, 0, 1)); // -Z
}

}
