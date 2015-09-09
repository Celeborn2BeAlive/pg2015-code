#version 330

in ivec3 aVoxelCoords;

out ivec3 vVoxelCoords;

void main()
{
    vVoxelCoords = aVoxelCoords;
    gl_Position = vec4(vec3(aVoxelCoords), 1.f);
}
