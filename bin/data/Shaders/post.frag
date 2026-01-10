#version 150

uniform sampler2D tex0;
uniform vec2 uResolution;
out vec4 fragColor;

void main() {
    // 1. Calculate UV coordinates
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    uv.y = 1.0 - uv.y;

    // 2. Get the game color
    vec4 color = texture(tex0, uv);

    // 3. APPLY VIGNETTE (Darken the corners)
    // Calculate distance from center of screen (0.5, 0.5)
    float dist = distance(uv, vec2(0.5, 0.5));
    
    // Smoothstep creates a soft gradient circle.
    // 0.8 is the outer edge (dark), 0.2 is the inner edge (bright).
    float vignette = smoothstep(0.8, 0.2, dist);

    // Multiply the color by the vignette (Darkens edges, keeps center bright)
    color.rgb *= vignette;

    // Optional: Slight Gamma Correction to make mid-tones pop
    color.rgb = pow(color.rgb, vec3(1.0 / 1.1));

    fragColor = color;
}