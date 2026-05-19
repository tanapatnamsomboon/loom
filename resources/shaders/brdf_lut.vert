#version 460 core

// Fullscreen triangle via gl_VertexID — no vertex buffer required. The
// triangle (x, y) = {(-1,-1), (3,-1), (-1,3)} covers the [-1, 1] viewport
// fully and the off-screen area is clipped away.
out vec2 vUV;

void main() {
    vec2 p = vec2((gl_VertexID == 1) ?  3.0 : -1.0,
                  (gl_VertexID == 2) ?  3.0 : -1.0);
    vUV         = p * 0.5 + 0.5;       // [0..1]
    gl_Position = vec4(p, 0.0, 1.0);
}
