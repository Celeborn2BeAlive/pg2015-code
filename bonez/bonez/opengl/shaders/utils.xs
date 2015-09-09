#version 330 core

float linearDepth(float depth, float near, float far) {
    return (2.f * near) / (far + near - depth * (far - near));
}
