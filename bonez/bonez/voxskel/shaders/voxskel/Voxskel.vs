#version 330

in vec3 position;
in vec3 normal_coords;

uniform mat4 MV;

out vec3 normal_view;
out vec3 position_view;

void main()
{
    gl_Position = vec4(position, 1.f);
    position_view = (MV*vec4(position,1.f)).xyz;
    normal_view = (MV*vec4(normal_coords,0.f)).xyz;
}
