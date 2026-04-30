#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aLocalPosition;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aThickness;
layout(location = 4) in float aFade;
layout(location = 5) in int aEntityID;

layout(std140, binding = 0) uniform Camera {
    mat4 uViewProjection;
};

out vec3     vLocalPosition;
out vec4     vColor;
out float    vThickness;
out float    vFade;
flat out int vEntityID;

void main() {
    vLocalPosition = aLocalPosition;
    vColor = aColor;
    vThickness = aThickness;
    vFade = aFade;
    vEntityID = aEntityID;

    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}