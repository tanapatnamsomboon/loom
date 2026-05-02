#version 460 core

layout (location = 0) out vec4 oColor;
layout (location = 1) out int oEntityID;

in vec3 vWorldPosition;

uniform vec3  uCameraPosition;
uniform float uMinorScale;
uniform float uMajorScale;
uniform float uLineThickness;
uniform float uFadeStart;
uniform float uFadeEnd;
uniform vec4  uMinorColor;
uniform vec4  uMajorColor;

float grid(vec2 coord, float size) {
    vec2 grid_pattern = abs(fract(coord / size - 0.5) - 0.5) / fwidth(coord / size);
    float line = min(grid_pattern.x, grid_pattern.y);
    return 1.0 - smoothstep(0.0, uLineThickness, line);
}

void main() {
    float dist = distance(vec3(uCameraPosition.x, 0.0, uCameraPosition.z), vWorldPosition);
    float fade = 1.0 - smoothstep(uFadeStart, uFadeEnd, dist);

    if (fade <= 0.0) discard;

    float minor_grid = grid(vWorldPosition.xz, uMinorScale);
    float major_grid = grid(vWorldPosition.xz, uMajorScale);

    vec4 color = uMinorColor;
    color.a *= minor_grid;
    color = mix(color, uMajorColor, major_grid);

    if (abs(vWorldPosition.x) < (uLineThickness * 1.5) * fwidth(vWorldPosition.x)) color = vec4(0.2, 0.3, 0.8, 1.0);
    if (abs(vWorldPosition.z) < (uLineThickness * 1.5) * fwidth(vWorldPosition.z)) color = vec4(0.8, 0.2, 0.2, 1.0);

    color.a *= fade;
    if (color.a <= 0.01) discard;

    oColor = color;
    oEntityID = -1;
}