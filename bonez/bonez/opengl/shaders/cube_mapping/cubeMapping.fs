#version 330 core

in vec3 gPosition;
in vec3 gNormal;
in vec2 gTexCoords;

layout(location = 0) out vec4 fNormalDist;
layout(location = 1) out vec2 fTexCoords;

void main() {
    fNormalDist  = vec4(normalize(gNormal), length(gPosition));
    fTexCoords = gTexCoords;
}
