#version 460 core
// Fullscreen triangle — empty VAO, no VBO. Same gl_VertexID trick as the
// tonemap pass; vUV ranges [0,1] across the full output framebuffer.
out vec2 vUV;
void main() {
    vec2 uv     = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUV         = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
