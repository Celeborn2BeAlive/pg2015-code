#version 330 core

layout(location = 0) in vec2 aPosition;

out vec3 vFarPoint_vs;
out vec2 vUVCoords;

uniform mat4 uRcpProjMatrix;

void main() {
    // By setting z to 1.f, we get the point corresponding to the vertex on
    // the far plane in view space after inversion of the projection
    vec4 farPointViewSpaceCoords = uRcpProjMatrix * vec4(aPosition, 1.f, 1.f);
    vFarPoint_vs = farPointViewSpaceCoords.xyz / farPointViewSpaceCoords.w;

    vUVCoords = 0.5 * (aPosition + vec2(1));

    gl_Position = vec4(aPosition, 0, 1);
}
