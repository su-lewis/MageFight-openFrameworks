#version 150

uniform sampler2D tex0;
uniform int levels;
uniform int useDither;
uniform vec2 uResolution;
uniform vec2 uLowRes; // low-res FBO size in pixels
uniform vec3 edgeColor; // color to use for edges
uniform float edgeStrength; // multiplier for edge smoothstep range
out vec4 fragColor;

// Simple Bayer 4x4 matrix for dithering
float bayer4(vec2 p) {
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    int idx = x + y * 4;
    float m[16] = float[16](
        0.0,  8.0,  2.0, 10.0,
        12.0, 4.0, 14.0, 6.0,
        3.0, 11.0, 1.0, 9.0,
        15.0, 7.0, 13.0, 5.0
    );
    return (m[idx] + 0.5) / 16.0;
}

void main() {
    // Compute screen-space UV (0..1) compatible with post-processing fullscreen quad
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    uv.y = 1.0 - uv.y;
    vec3 col = texture(tex0, uv).rgb;

    // Edge detection (Sobel on luminance) using low-res texel offsets
    vec2 texel = 1.0 / max(uLowRes, vec2(1.0,1.0));
    float lumC = dot(col, vec3(0.299,0.587,0.114));
    float lumL = dot(texture(tex0, uv + vec2(-texel.x, 0)).rgb, vec3(0.299,0.587,0.114));
    float lumR = dot(texture(tex0, uv + vec2(texel.x, 0)).rgb, vec3(0.299,0.587,0.114));
    float lumU = dot(texture(tex0, uv + vec2(0, -texel.y)).rgb, vec3(0.299,0.587,0.114));
    float lumD = dot(texture(tex0, uv + vec2(0, texel.y)).rgb, vec3(0.299,0.587,0.114));
    float gx = lumR - lumL;
    float gy = lumD - lumU;
    float edge = sqrt(gx*gx + gy*gy);

    // Posterize per channel
    float L = max(1.0, float(levels));
    // If no posterize requested, just blend a mild edge tint on original color
    if (L <= 1.0) {
        float edgeThreshold = 0.12;
        float factor = smoothstep(edgeThreshold, edgeThreshold * max(edgeStrength, 1.0), edge);
        // darken edges slightly without introducing color tint
        vec3 edgeTint = col * 0.5;
        fragColor = vec4(mix(col, edgeTint, factor), 1.0);
        return;
    }

    if (useDither == 1) {
        float d = bayer4(gl_FragCoord.xy);
        col.r = floor(col.r * L + d) / L;
        col.g = floor(col.g * L + d) / L;
        col.b = floor(col.b * L + d) / L;
    } else {
        col = floor(col * L) / L;
    }

    float edgeThreshold = 0.12;
    float factor = smoothstep(edgeThreshold, edgeThreshold * max(edgeStrength, 1.0), edge);
    // darken edges slightly without introducing color tint
    vec3 edgeTint = col * 0.5;
    fragColor = vec4(mix(col, edgeTint, factor), 1.0);
}
