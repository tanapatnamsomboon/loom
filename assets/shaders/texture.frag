#version 460 core

layout(location = 0) out vec4 color;

in vec4 vColor;
in vec2 vTexCoord;
uniform sampler2D uTexture;

void main() {
    color = texture(uTexture, vTexCoord) * vColor;
}