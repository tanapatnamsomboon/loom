#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(std140, binding = 0) uniform Camera {
    mat4 uViewProjection;
};

uniform mat4 uModel;
uniform mat4 uLightVP;       // identity when shadows are disabled
uniform int  uShadowsEnabled;

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vTexCoord;
out vec4 vLightSpacePos;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPos  = world.xyz;

    // Proper normal transform - handles non-uniform scale.
    mat3 normal_matrix = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normal_matrix * aNormal);

    vTexCoord = aTexCoord;

    // Light-space position for shadow sampling. Computed even when shadows are off
    // (cheap, and avoids a branch divergence between vertices).
    vLightSpacePos = uLightVP * world;

    gl_Position = uViewProjection * world;
}
