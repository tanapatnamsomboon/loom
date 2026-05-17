#version 460 core

// Depth-only pass: no color writes, gl_FragDepth is filled implicitly.
// The fragment shader is required only to satisfy the OpenGL pipeline;
// the shadow framebuffer has no color attachments so any out vars would
// be dropped anyway.
void main() {
}
