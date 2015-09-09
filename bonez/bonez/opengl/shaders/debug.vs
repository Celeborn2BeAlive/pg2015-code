#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in mat4 aMVPMatrix;
layout(location = 7) in mat4 aMVMatrix;
layout(location = 11) in uvec4 aObjectID;

out vec3 vPosition_vs;
out vec3 vNormal_vs;
out vec3 vColor;
flat out uvec4 vObjectID;

void main() {
    vec4 pos = vec4(aPosition, 1);

    vPosition_vs = vec3(aMVMatrix * pos);
    vNormal_vs = vec3(aMVMatrix * vec4(aNormal, 0)); // Suppose only uniform scaling
    gl_Position = aMVPMatrix * pos;
    vColor = aColor;
    vObjectID = aObjectID;
}
