#version 460 core

layout (location = 0) in vec3 aPosition;

out vec3 vTexCoords;

uniform mat4 uViewProjection;

void main() {
    vTexCoords = aPosition;
    vec4 pos = uViewProjection * vec4(aPosition * 500.0, 1.0);
    gl_Position = pos;
}