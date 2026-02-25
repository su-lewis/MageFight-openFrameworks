#version 150

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D tex0;
uniform float uTime;
uniform vec2 uResolution;
uniform float uScanlineIntensity; // 0..1
uniform float uPixelSize; // 1.0 = native, >1 pixelates

// Commodore 64-ish 16-color palette (approximate in sRGB)
vec3 c64Palette[16] = vec3[16](
    vec3(0.0, 0.0, 0.0),       // black
    vec3(1.0, 1.0, 1.0),       // white
    vec3(0.666, 0.333, 0.0),   // red-ish
    vec3(1.0, 0.666, 0.0),     // cyan-ish (approx)
    vec3(0.333, 0.666, 0.0),   // purple-ish
    vec3(0.0, 0.666, 0.0),     // green
    vec3(0.666, 0.666, 0.0),   // blue-ish
    vec3(0.333, 0.333, 0.333), // yellow/dark gray
    vec3(0.666, 0.333, 0.666), // orange/med
    vec3(0.0, 0.333, 0.666),   // brown/med
    vec3(0.333, 0.333, 0.0),   // light red
    vec3(0.666, 0.666, 0.333), // light cyan
    vec3(0.333, 0.666, 0.666), // light purple
    vec3(0.666, 0.333, 0.333), // light green
    vec3(0.333, 0.666, 0.333), // light blue
    vec3(0.666, 0.666, 0.666)  // light gray
);

// 2x2 ordered Bayer matrix for simple dithering
int bayer2[4] = int[4](0, 2, 3, 1);

float bayerDither(vec2 uv, float scale) {
    ivec2 p = ivec2(floor(uv * scale));
    int idx = (p.x % 2) + (p.y % 2) * 2;
    float threshold = float(bayer2[idx]) / 4.0;
    return threshold;
}

vec3 findClosestPalette(vec3 c) {
    float bestDist = 1000.0;
    int bestI = 0;
    for (int i = 0; i < 16; ++i) {
        float d = distance(c, c64Palette[i]);
        if (d < bestDist) { bestDist = d; bestI = i; }
    }
    return c64Palette[bestI];
}

int findClosestPaletteIndex(vec3 c) {
    float bestDist = 1000.0;
    int bestI = 0;
    for (int i = 0; i < 16; ++i) {
        float d = distance(c, c64Palette[i]);
        if (d < bestDist) { bestDist = d; bestI = i; }
    }
    return bestI;
}

void main() {
    vec2 uv = vTexCoord;
    vec4 col = texture(tex0, uv);
    vec3 color = col.rgb;


    // optional low-res pixelation to emulate C64 low resolution
    float pixelSize = max(1.0, uPixelSize);
    if (pixelSize > 1.5) {
        vec2 pxUv = floor(uv * uResolution / pixelSize) * pixelSize / uResolution;
        color = texture(tex0, pxUv).rgb;
    }

    // apply slight palette mapping with dithering
    float scale = 6.0; // dither scale
    float t = bayerDither(gl_FragCoord.xy / uResolution, scale);
    vec3 mapped = findClosestPalette(color + (t - 0.5) * 0.04);

    // preserve more of the original color so the board doesn't get strongly
    // replaced with black; blend original and mapped colors (35% original)
    mapped = mix(color, mapped, 0.65);

    // Avoid mapping relatively bright pixels to pure black — if the closest
    // palette index is black but the original luminance is above a small
    // threshold, bias back toward the original color to keep texture detail.
    float lum = dot(color, vec3(0.299, 0.587, 0.114));
    int nearest = findClosestPaletteIndex(color);
    if (nearest == 0 && lum > 0.08) {
        mapped = mix(mapped, color, 0.85);
    }

    // scanlines
    float scan = sin((gl_FragCoord.y + uTime * 30.0) * 1.2) * 0.5 + 0.5;
    // reduce max darkening from scanlines so board colors don't go very black
    mapped *= mix(1.0, 1.0 - uScanlineIntensity * 0.5, scan);

    fragColor = vec4(mapped, col.a);
}
