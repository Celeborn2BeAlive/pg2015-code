#pragma once

#include <bonez/types.hpp>

namespace BnZ {

struct CubeMapUtils {
    enum {
        POSITIVE_X = 0,
        NEGATIVE_X = 1,
        POSITIVE_Y = 2,
        NEGATIVE_Y = 3,
        POSITIVE_Z = 4,
        NEGATIVE_Z = 5
    };

    static void computeFaceProjMatrices(float zNear, float zFar, Mat4f projMatrices[6]);
};

}
