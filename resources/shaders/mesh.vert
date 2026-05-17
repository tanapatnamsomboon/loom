#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(std140, binding = 0) uniform Camera {
    mat4 uViewProjection;
};

uniform mat4 uModel;
// One light-space VP per cascade. Computed every vertex (cheap) so the frag
// shader can pick the right one based on view-space depth.
uniform mat4 uLightVP0;
uniform mat4 uLightVP1;
uniform mat4 uLightVP2;
uniform mat4 uLightVP3;
uniform int  uShadowsEnabled;

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vTexCoord;
out vec4 vLightSpacePos0;
out vec4 vLightSpacePos1;
out vec4 vLightSpacePos2;
out vec4 vLightSpacePos3;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPos  = world.xyz;

    // Proper normal transform - handles non-uniform scale.
    mat3 normal_matrix = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normal_matrix * aNormal);

    vTexCoord = aTexCoord;

    vLightSpacePos0 = uLightVP0 * world;
    vLightSpacePos1 = uLightVP1 * world;
    vLightSpacePos2 = uLightVP2 * world;
    vLightSpacePos3 = uLightVP3 * world;

    gl_Position = uViewProjection * world;
}
