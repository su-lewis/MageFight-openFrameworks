#version 150

uniform vec2 uResolution;

in vec4 position;

void main() {
    vec2 res = max(uResolution, vec2(1.0));
    // OF screen-space positions: (0,0) top-left. Convert to OpenGL NDC.
    float x = (position.x / res.x) * 2.0 - 1.0;
    float y = 1.0 - (position.y / res.y) * 2.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
}
#version 150

uniform vec2 uResolution;

in vec4 position;

void main() {
    vec2 res = max(uResolution, vec2(1.0));
    // OF screen-space positions: (0,0) top-left. Convert to OpenGL NDC.
    float x = (position.x / res.x) * 2.0 - 1.0;
    float y = 1.0 - (position.y / res.y) * 2.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
}
