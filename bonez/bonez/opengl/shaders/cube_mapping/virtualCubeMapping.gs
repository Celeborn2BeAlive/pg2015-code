#version 430 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

in vec3 vPosition[];
in vec3 vNormal[];
in vec2 vTexCoords[];
flat in int vInstanceID[];

out vec3 gPosition;
out vec3 gNormal;
out vec2 gTexCoords;

layout(std430, binding = 1) buffer MVPMatrixBuffer {
    mat4 bMVPMatrixBuffer[]; // Contains 6 * number of layer matrices
};

void main() {
    for(int face = 0; face < 6; ++face) {
        int mvpMatrix = 6 * vInstanceID[0] + face;
        gl_Layer = vInstanceID[0];
        gl_ViewportIndex = face;
        for(int i = 0; i < 3; ++i) {
            gl_Position = bMVPMatrixBuffer[mvpMatrix] * gl_in[i].gl_Position;

            gPosition = vPosition[i];
            gNormal = vNormal[i];
            gTexCoords = vTexCoords[i];

            EmitVertex();
        }
        EndPrimitive();
    }
}
