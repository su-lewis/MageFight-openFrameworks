#version 150

uniform sampler2D tex0;
uniform int levels;        // posterize levels (1 = off)
uniform int useDither;     // 0/1
uniform vec2 uResolution;  // full-screen resolution
uniform vec2 uLowRes;      // low-res FBO size (optional)
uniform vec3 edgeColor;    // color tint for edges
uniform float edgeStrength;// how strong edge darkening is

out vec4 fragColor;

// Lightweight Bayer 4x4 matrix
float bayer4(vec2 p) {
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    int idx = x + y * 4;
    float m[16] = float[16](
        0.0, 8.0, 2.0, 10.0,
        12.0, 4.0, 14.0, 6.0,
        3.0, 11.0, 1.0, 9.0,
        15.0, 7.0, 13.0, 5.0
    );
    return (m[idx] + 0.5) / 16.0;
}

// Compute luminance
float lum(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    // normalized uv (0..1) with origin at top-left like texture sampling expects
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    uv.y = 1.0 - uv.y;

    // Determine low-res grid to sample from, prefer provided uLowRes when valid
    vec2 lowRes = (uLowRes.x > 1.0 && uLowRes.y > 1.0) ? uLowRes : vec2(max(1.0, floor(uResolution.x / 4.0)), max(1.0, floor(uResolution.y / 4.0)));
    vec2 texel = 1.0 / lowRes;

    // Pixel-center sample in low-res grid (crisp pixel-art sampling)
    vec2 lowCoord = floor(uv * lowRes) / lowRes + 0.5 / lowRes;
    vec3 base = texture(tex0, lowCoord).rgb;

    // Posterize (per-channel) controlled by `levels`
    float L = max(1.0, float(levels));
    vec3 q = base;
    if (L > 1.0) {
        if (useDither == 1) {
            // gentle Bayer dither depending on screen coords
            float d = bayer4(gl_FragCoord.xy) * 0.45;
            q.r = floor(q.r * L + d) / L;
            q.g = floor(q.g * L + d) / L;
            q.b = floor(q.b * L + d) / L;
        } else {
            q = floor(q * L) / L;
        }
        // mix posterized result with original so the effect isn't too harsh
        q = mix(base, q, 0.6);
    }

    // edge detection on the low-res grid (Sobel-ish)
    float c = lum(base);
    float l = lum(texture(tex0, lowCoord + vec2(-texel.x, 0)).rgb);
    float r = lum(texture(tex0, lowCoord + vec2(texel.x, 0)).rgb);
    float u = lum(texture(tex0, lowCoord + vec2(0, -texel.y)).rgb);
    float d = lum(texture(tex0, lowCoord + vec2(0, texel.y)).rgb);
    float gx = (r - l);
    float gy = (d - u);
    float edge = sqrt(gx * gx + gy * gy);

    // Smooth the edge response and allow edgeStrength to control it
    float threshold = 0.08;
    float edgeFactor = smoothstep(threshold, threshold * max(edgeStrength, 1.0), edge);

    // Compose final color: apply subtle edge darkening towards edgeColor
    vec3 edgeTint = mix(q, edgeColor, 0.0); // keep tint optional; default 0
    vec3 colorWithEdge = mix(q, edgeTint * 0.85, edgeFactor * 0.9);
    colorWithEdge *= mix(1.0, 0.9, edgeFactor * 0.6); // slight darkening

    // Avoid additional gamma boosting — use the composed color directly
    // (removing the previous pow(...) which made the image too bright)
    vec3 finalCol = colorWithEdge * 0.96; // slight overall tone-down

    fragColor = vec4(clamp(finalCol, 0.0, 1.0), 1.0);
}
