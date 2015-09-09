#version 330 core

layout(points) in;
layout(line_strip, max_vertices = 6) out;

in ivec3 vVoxelCoords[];

uniform mat4 MVP;
uniform isampler3D uCubicalComplex;

flat out ivec3 gVoxelCoords;
flat out int gType;
flat out vec3 gColor;

void main() {
    gVoxelCoords = vVoxelCoords[0];

    int ccType = texelFetch(uCubicalComplex, vVoxelCoords[0], 0).x;

    if((ccType & 2) == 2){ // has XEDGE
        gType = 1;
        gColor = vec3(1, 0, 0);

        gl_Position = MVP * gl_in[0].gl_Position;
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(0.9, 0, 0, 0));
        EmitVertex();
        EndPrimitive();
    }
    if((ccType & 4) == 4){ // has YEDGE
        gType = 1;
        gColor = vec3(0, 1, 0);

        gl_Position = MVP * gl_in[0].gl_Position;
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 0.9, 0,  0));
        EmitVertex();
        EndPrimitive();
    }
    if((ccType & 8) == 8){ // has ZEDGE
        gType = 1;
        gColor = vec3(0, 0, 1);

        gl_Position = MVP * gl_in[0].gl_Position;
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 0, 0.9, 0));
        EmitVertex();
        EndPrimitive();
    }
}
