#version 460 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uViewProjection;
uniform mat4 uTransform;

out vec3 vWorldPosition;

void main() {
    vec4 world_position = uTransform * vec4(aPosition, 1.0);
    vWorldPosition = world_position.xyz;
    gl_Position = uViewProjection * world_position;
}