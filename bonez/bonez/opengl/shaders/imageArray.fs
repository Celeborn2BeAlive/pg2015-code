#version 330 core

in vec2 vTexCoords;

uniform sampler2DArray uTextureArray;
uniform uint uLayer;
uniform float uNear;
uniform float uFar;
uniform bool uDrawDepth;

out vec3 fFragColor;

float linearDepth(float, float, float);

void main() {
    vec4 color = texture(uTextureArray, vec3(vTexCoords, uLayer));

    if(uDrawDepth) {
            fFragColor = vec3(linearDepth(color.r, uNear, uFar));
    } else {
            fFragColor = color.rgb;
    }
}
