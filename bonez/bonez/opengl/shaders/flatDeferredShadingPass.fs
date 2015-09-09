#version 330 core

struct GBuffer {
    sampler2D normalDepthSampler;
    sampler2D diffuseSampler;
    sampler2D glossyShininessSampler;
};

in vec3 vFarPoint_vs;

uniform GBuffer uGBuffer;
uniform bool uDoWhiteRendering;
uniform bool uDoLightingFromCamera;
//uniform mat4 uViewMatrix;

layout(location = 0) out vec3 fColor;
layout(location = 1) out uvec4 fObjectID;

const float PI = 3.14159265358979323846264;

void main() {
    ivec2 pixelCoords = ivec2(gl_FragCoord.xy);

    vec3 color;
    if(uDoWhiteRendering) {
        color = vec3(1);
    } else {
        vec3 Kd = texelFetch(uGBuffer.diffuseSampler, pixelCoords, 0).rgb;
        vec4 KdShininess = texelFetch(uGBuffer.glossyShininessSampler, pixelCoords, 0);
        vec3 reflectance = PI * Kd + 2 * PI * KdShininess.rgb;

        if(reflectance != vec3(0)) {
            color = reflectance;
        } else {
            color = vec3(1);
        }
    }

    if(uDoLightingFromCamera) {
        vec4 normalDepth = texelFetch(uGBuffer.normalDepthSampler, pixelCoords, 0);
        vec3 N_vs = normalDepth.xyz;
        //vec3 N_vs = normalize(vec3(uViewMatrix * vec4(N_ws, 0)));
        vec3 P_vs = normalDepth.w * vFarPoint_vs;
        vec3 viewDir = normalize(-P_vs);
        color *= abs(dot(N_vs, viewDir));
    }

    fColor = color;
    fObjectID = uvec4(0);
}
