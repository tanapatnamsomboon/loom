#version 460 core

layout (location = 0) out vec4 oColor;
layout (location = 1) out int oEntityID;

in vec3 vWorldPosition;
uniform vec3 uCameraPosition;

float grid(vec2 coord, float size) {
    vec2 grid_pattern = abs(fract(coord / size - 0.5) - 0.5) / fwidth(coord / size);
    float line = min(grid_pattern.x, grid_pattern.y);
    return 1.0 - min(line, 1.0);
}

void main() {
    float dist = distance(vec3(uCameraPosition.x, 0.0, uCameraPosition.z), vWorldPosition);
    float fade = 1.0 - smoothstep(20.0, 120.0, dist);

    if (fade <= 0.0) discard;

    float minor_grid = grid(vWorldPosition.xz, 1.0);
    float major_grid = grid(vWorldPosition.xz, 10.0);

    vec4 color = vec4(0.6, 0.6, 0.6, minor_grid * 0.4);
    color = mix(color, vec4(1.0, 1.0, 1.0, 0.7), major_grid);

    if (abs(vWorldPosition.x) < 1.5 * fwidth(vWorldPosition.x)) color = vec4(0.2, 0.3, 0.8, 1.0);
    if (abs(vWorldPosition.z) < 1.5 * fwidth(vWorldPosition.z)) color = vec4(0.8, 0.2, 0.2, 1.0);

    color.a *= fade;
    if (color.a <= 0.01) discard;

    oColor = color;
    oEntityID = -1;
}