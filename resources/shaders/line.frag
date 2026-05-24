#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int  oEntityID;

in vec4     vColor;
flat in int vEntityID;

void main() {
    oColor = vColor;
    // Linearize sRGB-authored vColor — see quad.frag for the rationale.
    oColor.rgb = pow(oColor.rgb, vec3(2.2));
    oEntityID = vEntityID;
}