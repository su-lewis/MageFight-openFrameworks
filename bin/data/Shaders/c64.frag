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

// (saturation boost removed) -- keep colors closer to the original

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

    // Slightly stronger dither amplitude
    vec3 mapped = findClosestPalette(color + (t - 0.5) * 0.03);

    // Increase palette replacement strength so C64 look is more visible
    mapped = mix(color, mapped, 0.45);

    // No extra saturation boost; preserve mapped color

    // Avoid altering very bright pixels at all (keeps highlights intact)
    float lum = dot(color, vec3(0.299, 0.587, 0.114));
    if (lum > 0.92) {
        mapped = color;
    } else {
        int nearest = findClosestPaletteIndex(color);
        // If the nearest palette entry is black but the pixel isn't dark,
        // prefer the original color to avoid black replacement for moderately bright pixels.
        if (nearest == 0 && lum > 0.09) {
            mapped = color;
        }
    }

    // scanlines
    float scan = sin((gl_FragCoord.y + uTime * 30.0) * 1.2) * 0.5 + 0.5;
    // Increase scanline slightly to strengthen the retro feel, while keeping it controlled
    mapped *= mix(1.0, 1.0 - uScanlineIntensity * 0.28, scan);

    fragColor = vec4(mapped, col.a);
}
