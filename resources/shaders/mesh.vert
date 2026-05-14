#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(std140, binding = 0) uniform Camera {
    mat4 uViewProjection;
};

uniform mat4 uModel;

out vec3 vWorldNormal;
out vec2 vTexCoord;

void main() {
    // Proper normal transform — handles non-uniform scale.
    mat3 normal_matrix = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normal_matrix * aNormal);
    vTexCoord    = aTexCoord;

    gl_Position = uViewProjection * uModel * vec4(aPosition, 1.0);
}
