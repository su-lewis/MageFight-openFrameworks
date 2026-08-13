#version 150

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D tex0;
uniform int levels;        
uniform int useDither;     
uniform vec2 uResolution;  
uniform vec2 uLowRes;      
uniform vec3 edgeColor;    
uniform float edgeStrength;

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

float lum(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    // 1. Calculate the exact center of the "chunky" pixel grid
    vec2 texel = 1.0 / uLowRes;
    vec2 lowCoord = floor(vTexCoord * uLowRes) / uLowRes + (texel * 0.5);
    
    // 2. Sample the scene exactly at the center of our retro pixel.
    // This entirely removes the "shimmering" caused by the old getFatPixel function.
    vec3 base = texture(tex0, lowCoord).rgb;

    // --- Milder Color Modifications ---
    float luma = lum(base);
    base = mix(vec3(luma), base, 1.15); // Slight Saturation boost
    base *= 1.05; // Slight Brightness boost
    base = clamp(base, 0.0, 1.0);

    // --- Color Banding / Posterization ---
    float L = max(1.0, float(levels));
    vec3 q = base;
    
    if (L > 1.0) {
        if (useDither == 1) {
            float d = (bayer4(gl_FragCoord.xy) - 0.5) * 0.045; 
            q = floor((q + d) * L) / L;
        } else {
            q = floor(q * L) / L;
        }
    }

    // --- Center-Relative Edge Detection ---
    float c = lum(base);
    float l = lum(texture(tex0, lowCoord + vec2(-texel.x, 0)).rgb);
    float r = lum(texture(tex0, lowCoord + vec2(texel.x, 0)).rgb);
    float u = lum(texture(tex0, lowCoord + vec2(0, -texel.y)).rgb);
    float d = lum(texture(tex0, lowCoord + vec2(0, texel.y)).rgb);
    
    float diffL = abs(c - l);
    float diffR = abs(c - r);
    float diffU = abs(c - u);
    float diffD = abs(c - d);

    float edge = max(max(diffL, diffR), max(diffU, diffD));

    // FIX: Using smoothstep instead of step(). 
    // This gives edges a tiny bit of anti-aliasing internally so they don't 
    // flicker on and off abruptly when the camera pans across sub-pixels.
    float edgeFactor = smoothstep(0.08, 0.18, edge) * max(edgeStrength, 0.4);

    vec3 finalCol;
    
    // --- UI OVERWRITE FIX ---
    // Protect dark UI outlines AND bright UI highlights from being grayed out by the edge detector
    if (lum(q) < 0.15 || lum(q) > 0.8) {
        finalCol = q;
    } else {
        finalCol = mix(q, edgeColor, edgeFactor * 0.50);
    }

    fragColor = vec4(clamp(finalCol, 0.0, 1.0), 1.0);
}