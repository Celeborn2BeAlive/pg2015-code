#version 330

struct VirtualCubeMapContainer {
    samplerCube indirectionTexture;
    sampler2DArray texture;
};

uniform VirtualCubeMapContainer uVirtualCubeMapContainer;

vec4 textureSphere(uint texIdx, vec3 wi) {
    vec2 uv = texture(uVirtualCubeMapContainer.indirectionTexture, wi).rg;
    return texture(uVirtualCubeMapContainer.texture, vec3(uv, texIdx));
}
