#version 330

// 1 / 32.f
#define INVDEPTH 0.03125

//Position of the origin of the bounding box of the mesh
uniform vec3 origBBox;

//Number of color buffers
uniform int numRenderTargets;

//Length of a voxel
uniform float voxelSize;

//Resolution of the voxelization
uniform int numVoxels;

//Position of the vextex in object coordinates
in vec3 vertex0;
in vec3 vertex1;
in vec3 vertex2;

//Maximum depth range
in vec2 voxDepthRange;

//Store voxels positions in a texture
out ivec4 color[8];

bool planeBoxOverlap(in vec3 normal, in float d, in float maxVox)
{
        vec3 vMin, vMax;
        if(normal.x > 0.0){
                vMin.x = -maxVox;
                vMax.x = maxVox;
        }
        else{
                vMin.x = maxVox;
                vMax.x = -maxVox;
        }
        if(normal.y > 0.0){
                vMin.y = -maxVox;
                vMax.y = maxVox;
        }
        else{
                vMin.y = maxVox;
                vMax.y = -maxVox;
        }
        if(normal.z > 0.0){
                vMin.z = -maxVox;
                vMax.z = maxVox;
        }
        else{
                vMin.z = maxVox;
                vMax.z = -maxVox;
        }
        if (dot(normal, vMin) + d > 0.0) return false;
        if (dot(normal, vMax) + d >= 0.0) return true;
        return false;
}

bool triBoxOverlap(in vec3 voxCenter, in float voxHalfSize, in vec3 vertex0, in vec3 vertex1, in vec3 vertex2)
{
        vec3 v0, v1, v2, e0, e1, e2, fe0, fe1, fe2, normal;
        float minValue, maxValue, p0, p1, p2, rad, d;

        v0 = vertex0 - voxCenter;
        v1 = vertex1 - voxCenter;
        v2 = vertex2 - voxCenter;
        e0 = v1 - v0;
        e1 = v2 - v1;
        e2 = v0 - v2;
        fe0 = abs(e0);

        // AXISTEST_X01(e0.z, e0.y, fe0.z, fe0.y)
        p0 = e0.z * v0.y - e0.y * v0.z;
        p2 = e0.z * v2.y - e0.y * v2.z;
        if (p0 < p2){
                minValue = p0;
                maxValue = p2;
        }
        else{
                minValue = p2;
                maxValue = p0;
        }
        rad = fe0.z * voxHalfSize + fe0.y * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;

        // AXISTEST_Y02(e0.z, e0.x, fe0.z, fe0.x)
        p0 = -e0.z * v0.x + e0.x * v0.z;
        p2 = -e0.z * v2.x + e0.x * v2.z;
        if (p0 < p2){
                minValue = p0;
                maxValue = p2;
        }
        else{
                minValue = p2;
                maxValue = p0;
        }
        rad = fe0.z * voxHalfSize + fe0.x * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;

        // AXISTEST_Z12(e0.y, e0.x, fe0.y, fe0.x)
        p1 = e0.y * v1.x - e0.x * v1.y;
        p2 = e0.y * v2.x - e0.x * v2.y;
        if (p2 < p1){
                minValue = p2;
                maxValue = p1;
        }
        else{
                minValue = p1;
                maxValue = p2;
        }
        rad = fe0.y * voxHalfSize + fe0.x * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;
        fe1 = abs(e1);

        // AXISTEST_X01(e1.z, e1.y, fe1.z, fe1.y)
        p0 = e1.z * v0.y - e1.y * v0.z;
        p2 = e1.z * v2.y - e1.y * v2.z;
        if (p0 < p2){
                minValue = p0;
                maxValue = p2;
        }
        else{
                minValue = p2;
                maxValue = p0;
        }
        rad = fe1.z * voxHalfSize + fe1.y * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;

        // AXISTEST_Y02(e1.z, e1.x, fe1.z, fe1.x)
        p0 = -e1.z * v0.x + e1.x * v0.z;
        p2 = -e1.z * v2.x + e1.x * v2.z;
        if (p0 < p2){
                minValue = p0;
                maxValue = p2;
        }
        else{
                minValue = p2;
                maxValue = p0;
        }
        rad = fe1.z * voxHalfSize + fe1.x * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;

        // AXISTEST_Z0(e1.y, e1.x, fe1.y, fe1.x)
        p0 = e1.y * v0.x - e1.x * v0.y;
        p1 = e1.y * v1.x - e1.x * v1.y;
        if (p0 < p1){
                minValue = p0;
                maxValue = p1;
        }
        else{
                minValue = p1;
                maxValue = p0;
        }
        rad = fe1.y * voxHalfSize + fe1.x * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;
        fe2 = abs(e2);

        // AXISTEST_X2(e2.z, e2.y, fe2.z, fe2.y)
        p0 = e2.z * v0.y - e2.y * v0.z;
        p1 = e2.z * v1.y - e2.y * v1.z;
        if (p0 < p1){
                minValue = p0;
                maxValue = p1;
        }
        else{
                minValue = p1;
                maxValue = p0;
        }
        rad = fe2.z * voxHalfSize + fe2.y * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;

        // AXISTEST_Y1(e2.z, e2.x, fe2.z, fe2.x)
        p0 = -e2.z * v0.x + e2.x * v0.z;
        p1 = -e2.z * v1.x + e2.x * v1.z;
        if (p0 < p1){
                minValue = p0;
                maxValue = p1;
        }
        else{
                minValue = p1;
                maxValue = p0;
        }
        rad = fe2.z * voxHalfSize + fe2.x * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;

        // AXISTEST_Z12(e2.y, e2.x, fe2.y, fe2.x)
        p0 = e2.y * v1.x - e2.x * v1.y;
        p1 = e2.y * v2.x - e2.x * v2.y;
        if (p0 < p1){
                minValue = p0;
                maxValue = p1;
        }
        else{
                minValue = p1;
                maxValue = p0;
        }
        rad = fe2.y * voxHalfSize + fe2.x * voxHalfSize;
        if (minValue > rad || maxValue < -rad) return false;

        // FINDMINMAX(v0.z, v1.z, v2.z, minValue, maxValue)
        minValue = maxValue = v0.z;
        minValue = min(min(minValue, v1.z), v2.z);
        maxValue = max(max(maxValue, v1.z), v2.z);
        if (minValue > voxHalfSize || maxValue < -voxHalfSize) return false;

        // FINDMINMAX(v0.x, v1.x, v2.x, minValue, maxValue)
        minValue = maxValue = v0.x;
        minValue = min(min(minValue, v1.x), v2.x);
        maxValue = max(max(maxValue, v1.x), v2.x);
        if (minValue > voxHalfSize || maxValue < -voxHalfSize) return false;

        // FINDMINMAX(v0.y, v1.y, v2.y, minValue, maxValue)
        minValue = maxValue = v0.y;
        minValue = min(min(minValue, v1.y), v2.y);
        maxValue = max(max(maxValue, v1.y), v2.y);
        if (minValue > voxHalfSize || maxValue < -voxHalfSize) return false;

        normal = cross(e0, e1);
        d = -dot(normal, v0);
        if (!planeBoxOverlap(normal, d, voxHalfSize) ) return false;
        return true;
}


void main()
{
        vec3 voxCenter;
        float targetDepth;
        float halfVoxelSize = voxelSize * 0.5;


        for (int index = 0; index < numRenderTargets; ++index){
            color[index] = ivec4(0,0,0,0);
        }

        voxCenter.xy = origBBox.xy + vec2(voxelSize) * gl_FragCoord.xy;

        for (int index = 0; index <= numVoxels; ++index){
        //for (float index = voxDepthRange.x; index <= voxDepthRange.y; ++index){
            voxCenter.z = origBBox.z + voxelSize * (index + 0.5);
            if (triBoxOverlap(voxCenter, halfVoxelSize, vertex0, vertex1, vertex2)){
                    targetDepth = index * INVDEPTH;

                    //Find the colorBuffer where the voxel position will be stored
                    int colorBuffer = int(floor(targetDepth*0.25));

                    //Find the bit corresponding to the depth of the voxel
                    int pos = int(32 * fract(targetDepth));

                    //Find on which integer the data of the voxel will be stored
                    int vector = int(mod(floor(targetDepth),4));

                    //Write it on the corresponding colorBuffer in the right integer
                    color[colorBuffer][vector] |= (1 << pos);
            }
        }
}

