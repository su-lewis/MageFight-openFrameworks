#version 150

uniform sampler2D tex0;
uniform vec2 uResolution;
out vec4 fragColor;

void main() {
    // Pass-through post shader: no vignette/colour grading
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    uv.y = 1.0 - uv.y;
    fragColor = texture(tex0, uv);
}
    vec4 color = texture(tex0, uv);
