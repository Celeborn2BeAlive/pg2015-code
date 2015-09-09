#version 330 core

layout(location = 0) in vec2 aPosition;

uniform float uVerticalScale = 1;

out vec2 vTexCoords;

void main() {
    vTexCoords = 0.5 * vec2(1 + aPosition.x, 1 + uVerticalScale * aPosition.y);
    gl_Position = vec4(aPosition, 0.f, 1.f);
}
