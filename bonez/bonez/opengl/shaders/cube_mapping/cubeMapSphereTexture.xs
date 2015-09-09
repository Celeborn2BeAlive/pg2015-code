#version 400

struct CubeMapContainer {
    samplerCubeArray texture;
};

uniform CubeMapContainer uCubeMapContainer;

vec4 textureSphere(uint texIdx, vec3 wi) {
    return texture(uCubeMapContainer.texture, vec4(wi, texIdx));
}
