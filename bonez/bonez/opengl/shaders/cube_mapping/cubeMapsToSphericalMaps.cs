#version 430

//uniform samplerCubeArray uCubeMap; // Input
layout(rgba32f) uniform image2DArray uSphereMap; // Output

layout(local_size_x = 16, local_size_y = 16, local_size_z = 4) in;

vec4 textureSphere(uint mapIdx, vec3 wi);
vec3 sphericalMapping(vec2 uv);

void main() {
    uvec3 size = uvec3(imageSize(uSphereMap));

    if(any(greaterThanEqual(gl_GlobalInvocationID, size))) {
        return;
    }

    uvec2 pixel = gl_GlobalInvocationID.xy;

    vec3 wi = sphericalMapping((vec2(pixel) + vec2(0.5)) / vec2(size.xy));
    //vec4 value = texture(uCubeMap, vec4(wi, gl_GlobalInvocationID.z));
    vec4 value = textureSphere(gl_GlobalInvocationID.z, wi);
    imageStore(uSphereMap, ivec3(gl_GlobalInvocationID), value);
}
