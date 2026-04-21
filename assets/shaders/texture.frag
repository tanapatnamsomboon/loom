#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntityID;

in vec4 vColor;
in vec2 vTexCoord;
in float vTexIndex;
in float vTilingFactor;
flat in int vEntityID;

uniform sampler2D uTextures[32];

void main() {
    oColor = texture(uTextures[int(vTexIndex)], vTexCoord * vTilingFactor) * vColor;
    oEntityID = vEntityID;
}