#version 460 core
// Fullscreen triangle — no VBO needed. Bind an empty VAO and call
// glDrawArrays(GL_TRIANGLES, 0, 3). The triangle overdraws the whole
// NDC [-1,+1] square at every pixel.
void main() {
    vec2 uv     = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
