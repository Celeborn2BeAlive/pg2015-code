#version 330 core

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoords;
in vec3 vWorldSpacePosition;
in vec3 vWorldSpaceNormal;

struct Material {
    vec3 Kd;
    vec3 Ks;
    float shininess;
    sampler2D KdSampler;
    sampler2D KsSampler;
    sampler2D shininessSampler;
};

uniform Material uMaterial;

uniform float uZFar;

layout(location = 0) out vec4 fNormalDepth;
layout(location = 1) out vec3 fDiffuse;
layout(location = 2) out vec4 fGlossyShininess;

const float PI = 3.14159265358979323846264;

void main() {
    vec3 nNormal = normalize(vNormal);
    fNormalDepth.xyz = nNormal;
    fNormalDepth.w = -vPosition.z / uZFar; // Normalized depth value

    fDiffuse = uMaterial.Kd * texture(uMaterial.KdSampler, vTexCoords).rgb / PI;

    float shininess = uMaterial.shininess + texture(uMaterial.shininessSampler, vTexCoords).r;
    fGlossyShininess.rgb = uMaterial.Ks * texture(uMaterial.KsSampler, vTexCoords).rgb * (shininess + 2) / (2 * PI);
    fGlossyShininess.a = shininess;
}
