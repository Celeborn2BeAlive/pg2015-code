#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 12) out;

in ivec3 vVoxelCoords[];

uniform mat4 MVP;
uniform isampler3D uCubicalComplex;

flat out ivec3 gVoxelCoords;
flat out int gType;
flat out vec3 gColor;

void main() {
    gVoxelCoords = vVoxelCoords[0];

    int ccType = texelFetch(uCubicalComplex, vVoxelCoords[0], 0).x;

    if((ccType & 16) == 16){ // has XYFACE
        gType = 2;
        gColor = vec3(1, 1, 0);

        gl_Position = MVP * gl_in[0].gl_Position;
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 0, 0));
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 0, 0));
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 1, 0, 0));
        EmitVertex();

        EndPrimitive();
    }
    if((ccType & 32) == 32){ // has YZFACE
        gType = 2;
        gColor = vec3(0, 1, 1);

        gl_Position = MVP * gl_in[0].gl_Position;
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 0, 0));
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 0, 1, 0));
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 1, 0));
        EmitVertex();

        EndPrimitive();
    }
    if((ccType & 64) == 64){ // has XZFACE
        gType = 2;
        gColor = vec3(1, 0, 1);

        gl_Position = MVP * gl_in[0].gl_Position;
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 0, 1, 0));
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 0, 0));
        EmitVertex();
        gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 1, 0));
        EmitVertex();

        EndPrimitive();
    }
}
