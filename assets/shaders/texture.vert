#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in float aTexIndex;
layout(location = 4) in float aTilingFactor;
layout(location = 5) in int aEntityID;

out vec4 vColor;
out vec2 vTexCoord;
out float vTexIndex;
out float vTilingFactor;
flat out int vEntityID;

uniform mat4 uViewProjection;

void main() {
    vColor = aColor;
    vTexCoord = aTexCoord;
    vTexIndex = aTexIndex;
    vTilingFactor = aTilingFactor;
    vEntityID = aEntityID;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}