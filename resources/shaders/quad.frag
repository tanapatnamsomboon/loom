#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int oEntityID;

in vec4 vColor;
in vec2 vTexCoord;
flat in float vTexIndex;
in float vTilingFactor;
flat in int vEntityID;

uniform sampler2D uTextures[32];

void main() {
    oColor = texture(uTextures[int(vTexIndex)], vTexCoord * vTilingFactor) * vColor;
    // Discard near-transparent fragments so they don't write to depth (which
    // would punch invisible holes blocking later draws like the editor grid)
    // and so mip-averaged transparent halos around alpha-cutout icons drop out
    // instead of bleeding as a white ring at distance.
    if (oColor.a < 0.05) discard;
    // Linearize sRGB-authored RGB (sprite textures + inspector color picker)
    // so the unified tonemap pass produces the intended display brightness
    // instead of double-gamma-encoding to a washed-out look.
    oColor.rgb = pow(oColor.rgb, vec3(2.2));
    oEntityID = vEntityID;
}
