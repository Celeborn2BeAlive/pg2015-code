#version 430

layout(local_size_x = 1024) in;

uniform mat4 uFaceProjMatrix[6];

// input
layout(std430, binding = 0) buffer MVMatrixBuffer {
    mat4 bMVMatrixBuffer[];
};

uniform uint uMVCount; // number of matrix to process

// output
layout(std430, binding = 1) buffer MVPMatrixBuffer {
    mat4 bMVPMatrixBuffer[];
};

void main() {
    uint threadID = gl_GlobalInvocationID.x;
    if(threadID >= uMVCount) {
        return;
    }
    uint offset = threadID * 6;
    for (uint f = 0u; f < 6u; ++f) {
        bMVPMatrixBuffer[offset] = uFaceProjMatrix[f] * bMVMatrixBuffer[threadID];
        ++offset;
    }
}
