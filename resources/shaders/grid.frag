#version 460 core

layout (location = 0) out vec4 oColor;
layout (location = 1) out int  oEntityID;

in vec3 vWorldPosition;

uniform vec3  uCameraPosition;
uniform float uMinorScale;
uniform float uMajorScale;
uniform float uLineThickness;
uniform float uFadeStart;
uniform float uFadeEnd;
uniform vec4  uMinorColor;
uniform vec4  uMajorColor;

// Returns line coverage in [0, 1] for an axis-aligned grid of `size`-spaced lines.
// fwidth(coord/size) gives the per-pixel size of one grid cell, which keeps lines
// a constant 1-ish pixel wide regardless of zoom (screen-space derivatives).
float gridCoverage(vec2 coord, float size, float thickness) {
    vec2 c            = coord / size;
    vec2 grid_pattern = abs(fract(c - 0.5) - 0.5) / fwidth(c);
    float line        = min(grid_pattern.x, grid_pattern.y);
    return 1.0 - smoothstep(0.0, thickness, line);
}

// Single-axis line coverage using the same fwidth trick. Used for the colored
// world-axis lines along x=0 (Z axis) and z=0 (X axis).
float axisCoverage(float coord, float thickness) {
    return 1.0 - smoothstep(0.0, thickness, abs(coord) / max(fwidth(coord), 1e-6));
}

void main() {
    float dist = distance(vec3(uCameraPosition.x, 0.0, uCameraPosition.z), vWorldPosition);

    // Overall grid distance fade — smoothstep gives a C1-continuous roll-off.
    float fade = 1.0 - smoothstep(uFadeStart, uFadeEnd, dist);
    if (fade <= 0.0) discard;

    // Minor lines fade out earlier than major lines — at long view distances
    // the minor grid would otherwise cluster into solid-color noise. Major
    // lines persist for the full fade range to give the player a spatial anchor.
    float minor_fade = 1.0 - smoothstep(uFadeStart * 0.4, uFadeStart, dist);
    float major_fade = fade;

    float minor = gridCoverage(vWorldPosition.xz, uMinorScale, uLineThickness);
    float major = gridCoverage(vWorldPosition.xz, uMajorScale, uLineThickness);

    // Layer minor underneath, major on top. Where major has any coverage,
    // shift the color toward the major tint proportional to that coverage;
    // final alpha is whichever layer wins, so a major line never gets dimmer
    // than the minor line it overlaps.
    float minor_a = uMinorColor.a * minor * minor_fade;
    float major_a = uMajorColor.a * major * major_fade;
    vec3  rgb     = mix(uMinorColor.rgb, uMajorColor.rgb, clamp(major_a, 0.0, 1.0));
    float alpha   = max(minor_a, major_a);

    // Colored world-axis lines — anti-aliased via the same fwidth pattern as the
    // grid, blended on top of the grid lines rather than overwriting them so
    // there is no visible 1-pixel discontinuity where axis meets nearby grid.
    // Z-axis (line where x=0, running along +Z) = blue; X-axis (z=0, +X) = red.
    const vec3 kZAxisColor = vec3(0.30, 0.55, 0.95);
    const vec3 kXAxisColor = vec3(0.85, 0.25, 0.30);
    float z_axis = axisCoverage(vWorldPosition.x, uLineThickness * 1.5);
    float x_axis = axisCoverage(vWorldPosition.z, uLineThickness * 1.5);

    if (z_axis > 0.0) { rgb = mix(rgb, kZAxisColor, z_axis); alpha = max(alpha, z_axis); }
    if (x_axis > 0.0) { rgb = mix(rgb, kXAxisColor, x_axis); alpha = max(alpha, x_axis); }

    alpha *= fade;
    if (alpha <= 0.01) discard;

    oColor    = vec4(rgb, alpha);
    oEntityID = -1;
}
