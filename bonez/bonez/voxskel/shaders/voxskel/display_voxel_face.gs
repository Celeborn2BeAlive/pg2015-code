#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 24) out;

in ivec3 vVoxelCoords[];

uniform mat4 MVP;
uniform isampler3D uCubicalComplex;
uniform sampler3D uColorSampler;
uniform bool uUseNeighborColors = true;

flat out ivec3 gVoxelCoords;
flat out vec3 gColor;
out vec2 gTexCoords;

void main() {
    ivec3 voxelCoords = vVoxelCoords[0];

    int ccType = texelFetch(uCubicalComplex, vVoxelCoords[0], 0).x;

    if(!uUseNeighborColors) {
        gColor = texelFetch(uColorSampler, voxelCoords, 0).xyz;
    }

    if(ccType == 1) {
        gVoxelCoords = ivec3(voxelCoords.x, voxelCoords.y, voxelCoords.z - 1);
        if(!bool(texelFetch(uCubicalComplex, gVoxelCoords, 0).x)) {
            if(uUseNeighborColors) {
                gColor = texelFetch(uColorSampler, gVoxelCoords, 0).xyz;
            }

            gl_Position = MVP * gl_in[0].gl_Position;
            gTexCoords = vec2(0, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 0, 0));
            gTexCoords = vec2(1, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 0, 0));
            gTexCoords = vec2(0, 1);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 1, 0, 0));
            gTexCoords = vec2(1, 1);
            EmitVertex();

            EndPrimitive();
        }

        gVoxelCoords = ivec3(voxelCoords.x - 1, voxelCoords.y, voxelCoords.z);
        if(!bool(texelFetch(uCubicalComplex, gVoxelCoords, 0).x)) {
            if(uUseNeighborColors) {
                gColor = texelFetch(uColorSampler, gVoxelCoords, 0).xyz;
            }

            gl_Position = MVP * gl_in[0].gl_Position;
            gTexCoords = vec2(0, 0);
            EmitVertex();


            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 0, 0));
            gTexCoords = vec2(1, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 0, 1, 0));
            gTexCoords = vec2(0, 1);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 1, 0));
            gTexCoords = vec2(1, 1);
            EmitVertex();

            EndPrimitive();
        }

        gVoxelCoords = ivec3(voxelCoords.x, voxelCoords.y - 1, voxelCoords.z);
        if(!bool(texelFetch(uCubicalComplex, gVoxelCoords, 0).x)) {
            if(uUseNeighborColors) {
                gColor = texelFetch(uColorSampler, gVoxelCoords, 0).xyz;
            }

            gl_Position = MVP * gl_in[0].gl_Position;
            gTexCoords = vec2(0, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 0, 1, 0));
            gTexCoords = vec2(1, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 0, 0));
            gTexCoords = vec2(0, 1);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 1, 0));
            gTexCoords = vec2(1, 1);
            EmitVertex();

            EndPrimitive();
        }

        gVoxelCoords = ivec3(voxelCoords.x + 1, voxelCoords.y, voxelCoords.z);
        if(!bool(texelFetch(uCubicalComplex, gVoxelCoords, 0).x)) {
            if(uUseNeighborColors) {
                gColor = texelFetch(uColorSampler, gVoxelCoords, 0).xyz;
            }

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 0, 0));
            gTexCoords = vec2(0, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 1, 0, 0));
            gTexCoords = vec2(1, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 1, 0));
            gTexCoords = vec2(0, 1);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 1, 1, 0));
            gTexCoords = vec2(1, 1);
            EmitVertex();

            EndPrimitive();
        }

        gVoxelCoords = ivec3(voxelCoords.x, voxelCoords.y, voxelCoords.z + 1);
        if(!bool(texelFetch(uCubicalComplex, gVoxelCoords, 0).x)) {
            if(uUseNeighborColors) {
                gColor = texelFetch(uColorSampler, gVoxelCoords, 0).xyz;
            }

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 0, 1, 0));
            gTexCoords = vec2(0, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 1, 1, 0));
            gTexCoords = vec2(1, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 0, 1, 0));
            gTexCoords = vec2(0, 1);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 1, 0));
            gTexCoords = vec2(1, 1);
            EmitVertex();

            EndPrimitive();
        }

        gVoxelCoords = ivec3(voxelCoords.x, voxelCoords.y + 1, voxelCoords.z);
        if(!bool(texelFetch(uCubicalComplex, gVoxelCoords, 0).x)) {
            if(uUseNeighborColors) {
                gColor = texelFetch(uColorSampler, gVoxelCoords, 0).xyz;
            }

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 1, 0));
            gTexCoords = vec2(0, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 1, 1, 0));
            gTexCoords = vec2(1, 0);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(0, 1, 0, 0));
            gTexCoords = vec2(0, 1);
            EmitVertex();

            gl_Position = MVP * (gl_in[0].gl_Position + vec4(1, 1, 0, 0));
            gTexCoords = vec2(1, 1);
            EmitVertex();

            EndPrimitive();
        }
    }
}
