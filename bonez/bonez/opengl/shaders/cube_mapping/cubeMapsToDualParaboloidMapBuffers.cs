#version 430

layout(std430, binding = 0) buffer DualParaboloidMapBuffer {
    vec4 bDualParaboloidMapBuffer[];  // Output
};

uniform uint uParaboloidMapWidth;
uniform uint uParaboloidMapHeight;
uniform uint uDualParaboloidMapCount;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 4) in;

vec4 textureSphere(uint mapIdx, vec3 wi);
vec3 sphericalMapping(vec2 uv);

vec2 getUV(uvec2 pixel, uvec2 imageSize) {
    return (vec2(pixel) + vec2(0.5)) / vec2(imageSize);
}

vec2 getNDC(vec2 uv) {
    return vec2(-1) + 2 * uv;
}

vec3 dualParaboloidMapping(vec2 uv) {
    if(uv.x < 0.5) {
        vec2 ndc = getNDC(uv / vec2(0.5, 1));
        vec3 N = vec3(ndc.x, ndc.y, 1.f);
        float scale = 2.f / dot(N, N);
        return scale * N - vec3(0.f, 0.f, 1.f);
    }
    vec2 ndc = getNDC((uv - vec2(0.5, 0)) / vec2(0.5, 1));
    ndc.x = -ndc.x; // Reverse x to get continuity along the edge
    vec3 N = vec3(ndc.x, ndc.y, -1.f);
    float scale = 2.f / dot(N, N);
    return scale * N - vec3(0.f, 0.f, -1.f);
}

void main() {
    uvec3 size = uvec3(2 * uParaboloidMapWidth, uParaboloidMapHeight, uDualParaboloidMapCount);

    if(any(greaterThanEqual(gl_GlobalInvocationID, size))) {
        return;
    }

    uvec2 pixel = gl_GlobalInvocationID.xy;
    uvec2 mapSize = uvec2(2 * uParaboloidMapWidth, uParaboloidMapHeight);

    vec3 wi = dualParaboloidMapping(getUV(pixel, mapSize));

    vec4 value = textureSphere(gl_GlobalInvocationID.z, wi);

    bDualParaboloidMapBuffer[pixel.x + mapSize.x * pixel.y +
        mapSize.x * mapSize.y * gl_GlobalInvocationID.z] = value;
}
