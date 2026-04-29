#version 460 core

layout (location = 0) out vec4 oColor;
layout (location = 1) out int  oEntityID;

in vec3 vTexCoords;

void main() {
    vec3 dir = normalize(vTexCoords);

    vec3 top_color     = vec3(0.20, 0.45, 0.75);
    vec3 horizon_color = vec3(0.75, 0.82, 0.88);
    vec3 ground_color  = vec3(0.18, 0.18, 0.18);

    float sky_t = pow(max(dir.y, 0.0), 0.5);
    vec3 sky_gradient = mix(horizon_color, top_color, sky_t);

    float horizon_blur = smoothstep(-0.02, 0.02, dir.y);

    vec3 final_color = mix(ground_color, sky_gradient, horizon_blur);

    oColor = vec4(final_color, 1.0);
    oEntityID = -1;
}