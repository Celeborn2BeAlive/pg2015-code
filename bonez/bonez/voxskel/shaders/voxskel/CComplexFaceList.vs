#version 330 core

layout(location = 0) in ivec4 aFace;
layout(location = 1) in vec3 aFaceColorXY;
layout(location = 2) in vec3 aFaceColorYZ;
layout(location = 3) in vec3 aFaceColorXZ;

out ivec4 vFace;
out vec3 vFaceColorXY;
out vec3 vFaceColorYZ;
out vec3 vFaceColorXZ;

void main()
{
    vFace = aFace;
    vFaceColorXY = aFaceColorXY;
    vFaceColorYZ = aFaceColorYZ;
    vFaceColorXZ = aFaceColorXZ;

    gl_Position = vec4(vec3(aFace), 1.f);
}
