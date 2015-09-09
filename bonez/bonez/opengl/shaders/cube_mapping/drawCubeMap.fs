#version 330 core

uniform uint uMapIndex;
uniform float uZNear;
uniform float uZFar;
uniform bool uDrawDepth;

in vec2 vTexCoords;

out vec3 fColor;

vec3 sphericalMapping(vec2 texCoords);
float linearDepth(float, float, float);
vec4 textureSphere(uint texIdx, vec3 wi);

void main() {
    vec3 wi = sphericalMapping(vTexCoords);

    if(wi == vec3(0, 0, 0)) {
        fColor = wi;
    } else {
        vec4 value = textureSphere(uMapIndex, wi);
        if(uDrawDepth) {
            float depth = value.r;
            fColor = vec3(linearDepth(depth, uZNear, uZFar));
        } else {
            fColor = value.rgb;
        }
    }
}
