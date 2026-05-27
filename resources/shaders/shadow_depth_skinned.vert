#version 460 core

layout(location = 0) in vec3  aPosition;
layout(location = 4) in ivec4 aJoints;
layout(location = 5) in vec4  aWeights;

// Bones UBO must match mesh_skinned.vert's layout byte-for-byte; Renderer3D
// uploads the same skin matrices once per skinned-mesh draw call.
layout(std140, binding = 1) uniform Bones {
    mat4 uBoneMatrices[128];
};

uniform mat4 uLightVP;
uniform mat4 uModel;

void main() {
    mat4 skin = aWeights.x * uBoneMatrices[aJoints.x]
              + aWeights.y * uBoneMatrices[aJoints.y]
              + aWeights.z * uBoneMatrices[aJoints.z]
              + aWeights.w * uBoneMatrices[aJoints.w];

    gl_Position = uLightVP * uModel * skin * vec4(aPosition, 1.0);
}
