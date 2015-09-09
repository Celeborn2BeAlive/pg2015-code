#version 330 core

flat in ivec3 gVoxelCoords;
flat in int gType;
flat in vec3 gColor;

out vec3 color;
layout(location = 1) out uvec4 fObjectID;

void main() {
    color = gColor;
    fObjectID = uvec4(42, gVoxelCoords);
}
