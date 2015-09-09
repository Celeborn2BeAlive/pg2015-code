#version 430

layout(std430, binding = 0) buffer SphereMapBuffer {
    vec4 bSphereMapBuffer[];  // Output
};

uniform uint uSphereMapWidth;
uniform uint uSphereMapHeight;
uniform uint uSphereMapCount;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 4) in;

vec4 textureSphere(uint mapIdx, vec3 wi);
vec3 sphericalMapping(vec2 uv);

void main() {
    uvec3 size = uvec3(uSphereMapWidth, uSphereMapHeight, uSphereMapCount);

    if(any(greaterThanEqual(gl_GlobalInvocationID, size))) {
        return;
    }

    uvec2 pixel = gl_GlobalInvocationID.xy;

    vec3 wi = sphericalMapping((vec2(pixel) + vec2(0.5)) / vec2(size.xy));
    //vec4 value = texture(uCubeMap, vec4(wi, gl_GlobalInvocationID.z));
    vec4 value = textureSphere(gl_GlobalInvocationID.z, wi);

    bSphereMapBuffer[pixel.x + uSphereMapWidth * pixel.y +
        uSphereMapWidth * uSphereMapHeight * gl_GlobalInvocationID.z] = value;
}
