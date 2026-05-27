#version 460 core

// Depth-only pass: no color writes, gl_FragDepth is filled implicitly.
// Identical to shadow_depth.frag; the loader pairs <base>.vert + <base>.frag
// by filename so the skinned variant needs its own copy.
void main() {
}
