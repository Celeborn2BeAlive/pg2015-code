#version 330 core

layout(points) in;
layout(points, max_vertices = 1) out;

in ivec3 vVoxelCoords[];

uniform mat4 MVP;
uniform isampler3D uCubicalComplex;

flat out ivec3 gVoxelCoords;
flat out int gType;
flat out vec3 gColor;

void main() {
    gVoxelCoords = vVoxelCoords[0];

    int ccType = texelFetch(uCubicalComplex, vVoxelCoords[0], 0).x;

    if((ccType & 1) == 1){ // has POINT
        gl_Position = MVP * gl_in[0].gl_Position;

        gl_PointSize = min(5, 10 * gl_Position.w / gl_Position.z);

        gType = 0;
        gColor = vec3(1);

        EmitVertex();
        EndPrimitive();
    }
}
