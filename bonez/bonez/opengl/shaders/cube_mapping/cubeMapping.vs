#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

out vec3 vPosition;
out vec3 vNormal;
out vec2 vTexCoords;
flat out int vInstanceID;

layout(std430, binding = 1) buffer MVMatrixBuffer {
    mat4 bMVMatrixBuffer[]; // Contains number of layer matrices
};

void main() {
    gl_Position = vec4(aPosition, 1);

    vPosition = (bMVMatrixBuffer[gl_InstanceID] * gl_Position).xyz;
    vNormal = aNormal;
    vTexCoords = aTexCoords;
    vInstanceID = gl_InstanceID;
}
