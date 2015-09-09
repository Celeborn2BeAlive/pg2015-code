#version 330 core

flat in ivec3 gVoxelCoords;
flat in vec3 gColor;
in vec2 gTexCoords;

layout(location = 0) out vec3 fColor;
layout(location = 1) out uvec4 fObjectID;

void main() {
    float dist = max(abs(gTexCoords.x - 0.5), abs(gTexCoords.y - 0.5));
    if(dist < 0.45) {
        fColor = gColor;
    } else {
        fColor = gColor * 0.7;
    }
    fObjectID = uvec4(42, gVoxelCoords);
}
