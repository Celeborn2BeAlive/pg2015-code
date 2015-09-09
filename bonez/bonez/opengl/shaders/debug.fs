#version 330 core

in vec3 vPosition_vs;
in vec3 vNormal_vs;
in vec3 vColor;
flat in uvec4 vObjectID;

layout(location = 0) out vec3 fColor;
layout(location = 1) out uvec4 fObjectID;

void main() {
    fColor = abs(dot(normalize(vNormal_vs), normalize(-vPosition_vs))) * vColor;
    fObjectID = vObjectID;
}
