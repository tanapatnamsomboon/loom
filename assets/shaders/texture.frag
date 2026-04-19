#version 460 core

layout(location = 0) out vec4 color;

in vec4 vColor;
in vec2 vTexCoord;
in float vTexIndex;
in float vTilingFactor;

uniform sampler2D uTextures[32];

void main() {
    color = texture(uTextures[int(vTexIndex)], vTexCoord * vTilingFactor) * vColor;
}