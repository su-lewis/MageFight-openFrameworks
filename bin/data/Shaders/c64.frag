#version 150

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D tex0;
uniform float uTime;
uniform vec2 uResolution;
uniform float uScanlineIntensity;
uniform float uPixelSize;

// Authentic Commodore 64 16-color palette (Pepto's Palette in sRGB)
const vec3 c64Palette[16] = vec3[16](
    vec3(0.000, 0.000, 0.000), // 0: Black
    vec3(1.000, 1.000, 1.000), // 1: White
    vec3(0.533, 0.149, 0.169), // 2: Red
    vec3(0.439, 0.784, 0.792), // 3: Cyan
    vec3(0.541, 0.243, 0.580), // 4: Purple
    vec3(0.341, 0.651, 0.259), // 5: Green
    vec3(0.200, 0.157, 0.541), // 6: Blue
    vec3(0.710, 0.784, 0.439), // 7: Yellow
    vec3(0.541, 0.329, 0.161), // 8: Orange
    vec3(0.259, 0.200, 0.000), // 9: Brown
    vec3(0.761, 0.490, 0.482), // 10: Light Red
    vec3(0.263, 0.263, 0.263), // 11: Dark Gray
    vec3(0.420, 0.420, 0.420), // 12: Medium Gray
    vec3(0.600, 0.882, 0.569), // 13: Light Green
    vec3(0.420, 0.388, 0.831), // 14: Light Blue
    vec3(0.580, 0.580, 0.580)  // 15: Light Gray
);

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

// FIX: Pure Euclidean distance prevents dark grays from hue-shifting into brown/yellow
float colorDistance(vec3 c1, vec3 c2) {
    vec3 diff = c1 - c2;
    return dot(diff, diff); 
}

vec3 findClosestPalette(vec3 c) {
    float bestDist = 1000.0;
    int bestI = 0;
    for (int i = 0; i < 16; ++i) {
        float d = colorDistance(c, c64Palette[i]);
        if (d < bestDist) { 
            bestDist = d; 
            bestI = i; 
        }
    }
    return c64Palette[bestI];
}

void main() {
    vec2 uv = vTexCoord;
    vec4 col = texture(tex0, uv);
    vec3 color = col.rgb;

    // Optional pixelation
    float pixelSize = max(1.0, uPixelSize);
    if (pixelSize > 1.5) {
        vec2 pxUv = floor(uv * uResolution / pixelSize) * pixelSize / uResolution;
        color = texture(tex0, pxUv).rgb;
    }

    // FIX: Reduced dither strength from 0.15 to 0.08 for a cleaner image
    float dither = (bayer4(gl_FragCoord.xy) - 0.5) * 0.08;
    
    // Snap to the absolute C64 palette
    vec3 mapped = findClosestPalette(clamp(color + dither, 0.0, 1.0));

    // Original working scanlines
    float scan = sin((gl_FragCoord.y + uTime * 30.0) * 1.2) * 0.5 + 0.5;
    mapped *= mix(1.0, 1.0 - uScanlineIntensity * 0.2, scan);

    // Subtle CRT Vignette (Darker corners)
    vec2 crtUV = uv * 2.0 - 1.0;
    float vignette = 1.0 - dot(crtUV, crtUV) * 0.15;
    mapped *= vignette;

    fragColor = vec4(mapped, col.a);
}