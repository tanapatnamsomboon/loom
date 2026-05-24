#version 460 core
// Identical to bloom_downsample.vert — fullscreen triangle. Kept as a
// separate file because the AssetManager loads shaders by base name and
// pairs each .frag with its sibling .vert.
out vec2 vUV;
void main() {
    vec2 uv     = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUV         = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
