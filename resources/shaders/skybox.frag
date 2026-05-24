#version 460 core

layout (location = 0) out vec4 oColor;
layout (location = 1) out int  oEntityID;

in vec3 vDirection;

uniform samplerCube uSkybox;

// Output linear HDR — the unified post-process tonemap pass (tonemap.frag)
// applies ACES + sRGB once to the whole scene. Same contract as mesh.frag.
void main() {
    oColor    = vec4(texture(uSkybox, normalize(vDirection)).rgb, 1.0);
    oEntityID = -1;
}
