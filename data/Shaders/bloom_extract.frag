#version 150

uniform sampler2D tex0;
uniform vec2 uResolution;
uniform float threshold; // brightness threshold
out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    uv.y = 1.0 - uv.y;
    vec3 col = texture(tex0, uv).rgb;
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    // Extract bright parts only
    float t = max(lum - threshold, 0.0) / max(1.0 - threshold, 0.0001);
    fragColor = vec4(col * t, 1.0);
}
