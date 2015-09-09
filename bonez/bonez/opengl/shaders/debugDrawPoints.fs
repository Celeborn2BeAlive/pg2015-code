#version 330 core

in vec3 vColor;

uniform uvec4 uObjectID;

layout(location = 0) out vec3 fColor;
layout(location = 1) out uvec4 fObjectID;

void main() {
    fColor = vColor;
    fObjectID = uObjectID;
}
