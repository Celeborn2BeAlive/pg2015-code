#pragma once

#include <bonez/sys/easyloggingpp.hpp>
#include <GL/glew.h>

namespace BnZ {
    // Must be called after context creation
    bool initGL();
}
