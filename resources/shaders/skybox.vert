#version 460 core

layout (location = 0) in vec3 aPosition;

out vec3 vDirection;

uniform mat4 uViewProjection;

void main() {
    vDirection  = aPosition;
    // Render the cube at a large finite scale (well inside the camera's far
    // plane of 1000). Drawn before any scene geometry, so it fills the depth
    // buffer's initial 1.0 with ~0.99x; subsequent scene geometry at closer
    // depth overrides it via normal GL_LESS testing. Avoids the xyww + LEQUAL
    // trick (which would need a depth-func swap).
    gl_Position = uViewProjection * vec4(aPosition * 500.0, 1.0);
}
