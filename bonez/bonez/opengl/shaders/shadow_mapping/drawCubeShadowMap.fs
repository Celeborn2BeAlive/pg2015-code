#version 330 core

uniform samplerCube uTexture;
uniform float uZNear;
uniform float uZFar;

in vec2 vTexCoords;

out vec3 fColor;

vec3 texCoordsToDirection1(vec2 texCoords);
float linearDepth(float, float, float);

void main() {
    vec3 d = texCoordsToDirection1(vTexCoords);

    if(d == vec3(0, 0, 0)) {
        fColor = d;
    } else {
        float depth = texture(uTexture, d).r;
        fColor = vec3(linearDepth(depth, uZNear, uZFar));
    }
}
