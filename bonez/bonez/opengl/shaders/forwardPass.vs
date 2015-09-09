#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTexCoords;

uniform mat4 uMVPMatrix;

out vec3 vNormal;
out vec2 vTexCoords;

void main() {
    gl_Position = uMVPMatrix * vec4(aPosition, 1);
    vNormal = aNormal;
    vTexCoords = aTexCoords.xy;
}
