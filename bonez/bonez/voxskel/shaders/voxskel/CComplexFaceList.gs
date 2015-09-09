#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 12) out;

in ivec4 vFace[];
in vec3 vFaceColorXY[];
in vec3 vFaceColorYZ[];
in vec3 vFaceColorXZ[];

uniform mat4 uMVPMatrix;

flat out ivec3 gVoxelCoords;
flat out vec3 gColor;
out vec2 gTexCoords;

void main() {
    gVoxelCoords = vFace[0].xyz;

    int ccType = vFace[0].w;

    if((ccType & 16) == 16){ // has XYFACE
        gColor = vFaceColorXY[0];

        gl_Position = uMVPMatrix * gl_in[0].gl_Position;
        gTexCoords = vec2(0, 0);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(0, 1, 0, 0));
        gTexCoords = vec2(1, 0);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(1, 0, 0, 0));
        gTexCoords = vec2(0, 1);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(1, 1, 0, 0));
        gTexCoords = vec2(1, 1);
        EmitVertex();

        EndPrimitive();
    }
    if((ccType & 32) == 32){ // has YZFACE
        gColor = vFaceColorYZ[0];

        gl_Position = uMVPMatrix * gl_in[0].gl_Position;
        gTexCoords = vec2(0, 0);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(0, 1, 0, 0));
        gTexCoords = vec2(1, 0);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(0, 0, 1, 0));
        gTexCoords = vec2(0, 1);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(0, 1, 1, 0));
        gTexCoords = vec2(1, 1);
        EmitVertex();

        EndPrimitive();
    }
    if((ccType & 64) == 64){ // has XZFACE
        gColor = vFaceColorXZ[0];

        gl_Position = uMVPMatrix * gl_in[0].gl_Position;
        gTexCoords = vec2(0, 0);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(0, 0, 1, 0));
        gTexCoords = vec2(1, 0);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(1, 0, 0, 0));
        gTexCoords = vec2(0, 1);
        EmitVertex();
        gl_Position = uMVPMatrix * (gl_in[0].gl_Position + vec4(1, 0, 1, 0));
        gTexCoords = vec2(1, 1);
        EmitVertex();

        EndPrimitive();
    }
}
