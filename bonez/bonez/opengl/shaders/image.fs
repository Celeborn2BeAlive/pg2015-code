#version 330 core

in vec2 vTexCoords;

uniform sampler2D uImage;
uniform float uGamma;

uniform bool uDisplayAlphaChannel;
uniform bool uDivideByAlpha;

out vec3 fFragColor;

void main() {
    if(uDisplayAlphaChannel) {
        fFragColor = pow(texture(uImage, vTexCoords).aaa, vec3(1.f / uGamma));
    } else {
        vec4 color = texture(uImage, vTexCoords);
        if(uDivideByAlpha && color.a != 0) {
            color /= color.a;
        }
        fFragColor = pow(color.rgb, vec3(1.f / uGamma));
    }
}
