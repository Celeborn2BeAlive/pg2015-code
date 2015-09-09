#version 330 core

in vec2 vTexCoords;
in vec3 vNormal;

struct GLMaterial {
    vec3 Kd;
    vec3 Ks;
    float shininess;
    sampler2D KdSampler;
    sampler2D KsSampler;
    sampler2D shininessSampler;
};

uniform GLMaterial uMaterial;

out vec3 fColor;

void main() {
    fColor = uMaterial.Kd * texture(uMaterial.KdSampler, vTexCoords).rgb;
}
