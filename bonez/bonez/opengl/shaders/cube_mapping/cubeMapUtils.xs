#version 330 core

// Compute the face index of an incident direction
int getFaceIdx(vec3 wi) {
    vec3 absWi = abs(wi);

    float maxComponent = max(absWi.x, max(absWi.y, absWi.z));

    if(maxComponent == absWi.x) {
        if(wi.x > 0) {
            return 0;
        }
        return 1;
    }

    if(maxComponent == absWi.y) {
        if(wi.y >= 0) {
            return 2;
        }
        return 3;
    }

    if(maxComponent == absWi.z) {
        if(wi.z > 0) {
            return 4;
        }
        return 5;
    }

    return -1;
}

// Return a different color for each face, useful for debugging
vec3 getFaceColor(int face) {
    switch(face) {
    case 0: // +X
        return vec3(1, 0, 0);
    case 1: // -X
        return vec3(0, 1, 0);
    case 2: // +Y
        return vec3(0, 0, 1);
    case 3: // -Y
        return vec3(1, 1, 0);
    case 4: // +Z
        return vec3(1, 0, 1);
    case 5: // -Z
        return vec3(0, 1, 1);
    }
    return vec3(0, 0, 0);
}

vec3 sphericalMapping(vec2 texCoords) {
    float PI = 3.14;
    float HALF_PI = 0.5 * PI;
    float QUARTER_PI = 0.25 * PI;

    float phi = texCoords.x * 2 * PI;
    float theta = texCoords.y * PI;

    return vec3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
}

vec3 cubeFaceMapping(vec2 texCoords) {
    texCoords = 2 * texCoords - vec2(1);

    if(texCoords.x <= -0.5 && abs(texCoords.y) <= 0.33) {
        // +Z
        float x = -1 + 4 * (1 + texCoords.x);
        float y = -3 * texCoords.y;
        float z = 1;

        return vec3(x, y, z);
    }

    if(texCoords.x <= 0.0 && abs(texCoords.y) <= 0.33) {
        // -X
        float x = -1;
        float y = -3 * texCoords.y;
        float z = -1 + 4 * (0.5 + texCoords.x);
        return vec3(x, y, z);
    }

    if(texCoords.x <= 0.5 && abs(texCoords.y) <= 0.33) {
        // -Z
        float x = -(-1 + 4 * (0.0 + texCoords.x));
        float y = -3 * texCoords.y;
        float z = -1;
        return vec3(x, y, z);
    }

    if(texCoords.x <= 1.0 && abs(texCoords.y) <= 0.33) {
        // +X
        float x = 1;
        float y = -3 * texCoords.y;
        float z = -(-1 + 4 * (-0.5 + texCoords.x));
        return vec3(x, y, z);
    }

    if(texCoords.x > 0.0 && texCoords.x <= 0.5 && texCoords.y > 0.33) {
        // +Y
        float x = -1 + 4 * texCoords.x;
        float y = 1;
        float z = -1 + 3 * (texCoords.y - 0.33);
        return vec3(x, y, z);
    }

    if(texCoords.x > 0.0 && texCoords.x <= 0.5 && texCoords.y <= -0.33) {
        // -Y
        float x = -1 + 4 * texCoords.x;
        float y = -1;
        float z = -(-1 + 3 * (texCoords.y + 0.99));
        return vec3(x, y, z);
    }

    return vec3(0, 0, 0);
}

