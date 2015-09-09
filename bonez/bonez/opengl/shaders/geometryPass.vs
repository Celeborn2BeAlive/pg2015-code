#version 330

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

uniform mat4 uMVPMatrix;
uniform mat4 uMVMatrix;

out vec3 vPosition;
out vec3 vNormal;
out vec2 vTexCoords;
out vec3 vWorldSpaceNormal;
out vec3 vWorldSpacePosition;

void main() {
    vWorldSpacePosition = aPosition;
    vWorldSpaceNormal = aNormal;
    vPosition = vec3(uMVMatrix * vec4(aPosition, 1));
    vNormal = vec3(uMVMatrix * vec4(aNormal, 0)); // Works because MVMatrix has no scale
    vTexCoords = aTexCoords;

    gl_Position = uMVPMatrix * vec4(aPosition, 1);
}
