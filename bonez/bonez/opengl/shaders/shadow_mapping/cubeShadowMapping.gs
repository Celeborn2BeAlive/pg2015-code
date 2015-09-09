#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

uniform mat4 uFaceMVPMatrices[6];

void main() {
    for(int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for(int i = 0; i < 3; ++i) {
            gl_Position = uFaceMVPMatrices[face] * gl_in[i].gl_Position;
            EmitVertex();
        }
        EndPrimitive();
    }
}
