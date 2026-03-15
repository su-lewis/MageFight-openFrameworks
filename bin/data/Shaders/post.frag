#version 150

uniform sampler2D tex0;
uniform vec2 uResolution;
out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    // Keep original orientation
    uv.y = 1.0 - uv.y;

    // Simple passthrough with subtle vignette and mild color tweak
    vec3 col = texture(tex0, uv).rgb;

    // Subtle vignette
    vec2 centered = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution;
    float radius = length(centered);
    float vig = smoothstep(0.98, 0.5, radius);

    // Mild color/contrast adjustment
    col = pow(col, vec3(0.98));
    col = mix(col * 0.99, col, 0.01);

    fragColor = vec4(col * vig, 1.0);
}
