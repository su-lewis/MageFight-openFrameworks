#version 150

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D tex0;
uniform float uTime;
uniform vec2 uResolution;
uniform float uScanlineIntensity;
uniform float uPixelSize;

// 100% Accurate C64 "Pepto" Palette (16 Fixed Colors)
const vec3 C64_PALETTE[16] = vec3[16](
    vec3(0.000, 0.000, 0.000), // 0: Black
    vec3(1.000, 1.000, 1.000), // 1: White
    vec3(0.533, 0.000, 0.000), // 2: Red
    vec3(0.667, 1.000, 0.933), // 3: Cyan
    vec3(0.800, 0.267, 0.800), // 4: Purple
    vec3(0.000, 0.800, 0.333), // 5: Green
    vec3(0.000, 0.000, 0.667), // 6: Blue
    vec3(0.933, 0.933, 0.467), // 7: Yellow
    vec3(0.867, 0.533, 0.333), // 8: Orange
    vec3(0.400, 0.267, 0.000), // 9: Brown
    vec3(1.000, 0.467, 0.467), // 10: Light Red
    vec3(0.200, 0.200, 0.200), // 11: Dark Gray
    vec3(0.467, 0.467, 0.467), // 12: Gray
    vec3(0.667, 1.000, 0.400), // 13: Light Green
    vec3(0.000, 0.533, 1.000), // 14: Light Blue
    vec3(0.733, 0.733, 0.733)  // 15: Light Gray
);

// 8x8 Bayer matrix for authentic 8-bit retro dithering
float bayer8(vec2 p) {
    int x = int(mod(p.x, 8.0));
    int y = int(mod(p.y, 8.0));
    const int dither[64] = int[64](
         0, 32,  8, 40,  2, 34, 10, 42,
        48, 16, 56, 24, 50, 18, 58, 26,
        12, 44,  4, 36, 14, 46,  6, 38,
        60, 28, 52, 20, 62, 30, 54, 22,
         3, 35, 11, 43,  1, 33,  9, 41,
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47,  7, 39, 13, 45,  5, 37,
        63, 31, 55, 23, 61, 29, 53, 21
    );
    return float(dither[x + y * 8]) / 64.0 - 0.5;
}

vec3 nearestC64Color(vec3 col) {
    float minDist = 9999.0;
    vec3 bestColor = C64_PALETTE[0];
    
    for (int i = 0; i < 16; i++) {
        vec3 pal = C64_PALETTE[i];
        vec3 diff = col - pal;
        
        float dist = dot(diff, diff); 
        
        // THE MAGIC TRICK: Penalize Grays!
        // This forces the math to use C64 colors (Blue/Purple/Brown) to dither the stone walls
        if (i == 11 || i == 12 || i == 15) {
            dist *= 2.0; 
        }
        
        if (dist < minDist) {
            minDist = dist;
            bestColor = pal;
        }
    }
    return bestColor;
}

void main() {
    vec2 uv = vTexCoord;
    
    // 1. Pixelation (Wide pixels)
    float pxSize = max(1.0, uPixelSize * 2.0);
    vec2 pixelGrid = uResolution / pxSize;
    pixelGrid.x *= 0.85; 
    
    vec2 pxUv = floor(uv * pixelGrid) / pixelGrid;
    
    vec4 col = texture(tex0, pxUv);
    vec3 color = col.rgb;
    
    // Capture raw brightness to find the pitch-black background
    float originalLuma = dot(color, vec3(0.299, 0.587, 0.114));
    
    // Protect the deep black void outside the board
    if (originalLuma < 0.095) {
        color = vec3(0.0);
    } else {
        // 2. Color Grading for Retro Mapping (YOUR FAVORITE MATH!)
        // Changed pow(0.65) to pow(0.75) to make it less bright/washed out
        color = pow(color, vec3(0.75));
        color = clamp((color - 0.05) * 1.15, 0.0, 1.0);
        
        // Saturation boost
        float newLuma = dot(color, vec3(0.299, 0.587, 0.114));
        color = clamp(mix(vec3(newLuma), color, 1.6), 0.0, 1.0); 

        // 3. Ordered Dithering
        float ditherSpread = 0.12; 
        float dither = bayer8(gl_FragCoord.xy / pxSize) * ditherSpread;
        color = clamp(color + vec3(dither), 0.0, 1.0);
    }

    // 4. Map to C64 colors
    vec3 mapped = nearestC64Color(color);

    // 5. VIC-II Composite Artifacts (Horizontal color smearing)
    float smearX = 1.0 / pixelGrid.x;
    vec3 colorLeft = nearestC64Color(texture(tex0, pxUv - vec2(smearX, 0.0)).rgb);
    vec3 colorRight = nearestC64Color(texture(tex0, pxUv + vec2(smearX, 0.0)).rgb);
    mapped = mix(mapped, (mapped * 0.6 + colorLeft * 0.2 + colorRight * 0.2), 0.35);

    // 6. Authentic CRT Scanlines
    float scan = sin(uv.y * uResolution.y * 3.14159) * 0.5 + 0.5;
    scan = pow(scan, 1.5);
    float scanlineDarkness = clamp(uScanlineIntensity * 2.5, 0.1, 0.4); 
    mapped *= mix(1.0, 1.0 - scanlineDarkness, scan);

    // 7. Vintage Monitor Vignette
    vec2 crtUV = uv * 2.0 - 1.0;
    float vignette = 1.0 - dot(crtUV, crtUV) * 0.25;
    mapped *= smoothstep(0.0, 0.6, vignette);

    fragColor = vec4(mapped, col.a);
}