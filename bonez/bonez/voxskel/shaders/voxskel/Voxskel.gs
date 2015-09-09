#version 330

#define ZNEARCLIP vec3(-1.0)
#define ZFARCLIP vec3(1.0)
#define ZINSIDECLIP 0.0
#define ZOUTSIDECLIP -2.0

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 normal_view[];
in vec4 position_view[];

uniform float halfVoxelSizeNormalized;
uniform mat4 MVP;
uniform vec2 halfPixelSize;
uniform int numVoxels;

out vec3 vertex0;
out vec3 vertex1;
out vec3 vertex2;
out vec2 voxDepthRange;

void main() {
    vec4 triV0, triV1, triV2, AABB;
    vec3 depths;
    float zd1, zd2;

    //Send each vertex position to the Fragment Shader
    vertex0 = gl_in[0].gl_Position.xyz;
    vertex1 = gl_in[1].gl_Position.xyz;
    vertex2 = gl_in[2].gl_Position.xyz;

   //Get each vertex position in screen space
    triV0 = MVP * gl_in[0].gl_Position;
    triV1 = MVP * gl_in[1].gl_Position;
    triV2 = MVP * gl_in[2].gl_Position;

    depths = vec3(triV0.z, triV1.z, triV2.z);
    AABB = triV0.xyxy;

    //if the primitive is out of the viewport, we make sure it will not be displayed
    if (all(lessThan(depths, ZNEARCLIP) ) || all(greaterThan(depths, ZFARCLIP) ) ){
            voxDepthRange = vec2(ZOUTSIDECLIP);
            //Same vertex created four times => a point is displayed
            gl_Position = vec4(AABB.xw, ZOUTSIDECLIP, 1.0);
            EmitVertex();
            gl_Position = vec4(AABB.xy, ZOUTSIDECLIP, 1.0);
            EmitVertex();
            gl_Position = vec4(AABB.zw, ZOUTSIDECLIP, 1.0);
            EmitVertex();
            gl_Position = vec4(AABB.zy, ZOUTSIDECLIP, 1.0);
            EmitVertex();
    }
    //if the primitive is in
    else{
            //keep the least and greatest values of x and y between triV0,triV1 and triV2
            AABB = vec4(min(min(AABB.xy, triV1.xy), triV2.xy), max(max(AABB.zw, triV1.xy), triV2.xy) );

            //Add the value of halfPixelSize for precision
            AABB += vec4(-halfPixelSize, halfPixelSize);

            //Keep the least and greatest value of z between triV0, triV1 and triV2
            //voxDepthRange.xy = vec2(++triV0.z * 0.5);
            //zd1 = ++triV1.z * 0.5;
            //zd2 = ++triV2.z * 0.5;
            //voxDepthRange = vec2(min(min(voxDepthRange.x, zd1), zd2), max(max(voxDepthRange.y, zd1), zd2) );

            //Add the value of halfVoxelSizeNormalized
            //voxDepthRange += vec2(-halfVoxelSizeNormalized, halfVoxelSizeNormalized);

            //Clamp the value between 0.0 and 1.0
            //Multiply by numVoxels
            //Get the biggest integer inferior to this value
            //voxDepthRange = floor(clamp(voxDepthRange, 0.0, 1.0) *numVoxels);

            //Create a quad with those values
            gl_Position = vec4(AABB.xw, ZINSIDECLIP, 1.0);
            EmitVertex();
            gl_Position = vec4(AABB.xy, ZINSIDECLIP, 1.0);
            EmitVertex();
            gl_Position = vec4(AABB.zw, ZINSIDECLIP, 1.0);
            EmitVertex();
            gl_Position = vec4(AABB.zy, ZINSIDECLIP, 1.0);
            EmitVertex();
    }
    EndPrimitive();

}


