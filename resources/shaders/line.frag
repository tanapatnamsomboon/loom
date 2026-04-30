#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int  oEntityID;

in vec4     vColor;
flat in int vEntityID;

void main() {
    oColor = vColor;
    oEntityID = vEntityID;
}