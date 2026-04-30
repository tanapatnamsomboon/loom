#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntityID;

in vec3 vLocalPosition;
in vec4 vColor;
in float vThickness;
in float vFade;
flat in int vEntityID;

void main() {
    float distance = 1.0 - length(vLocalPosition.xy);

    float circle = smoothstep(0.0, vFade, distance);
    circle *= smoothstep(vThickness + vFade, vThickness, distance);

    if (circle == 0.0)
    discard;

    oColor = vColor;
    oColor.a *= circle;

    oEntityID = vEntityID;
}