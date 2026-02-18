#version 150

// Empty fragment shader - depth written to depth attachment
out vec4 fragColor;
void main() {
    // gl_FragDepth is written automatically from gl_Position
    fragColor = vec4(0.0);
}
