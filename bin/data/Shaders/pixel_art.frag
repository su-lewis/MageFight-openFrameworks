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

// Safely downsamples while preserving dark 1px outlines AND bright 1px UI highlights
vec3 getFatPixel(vec2 center) {
    vec2 texelNative = 1.0 / uResolution;
    
    // Check all 4 native pixels inside this 2x2 downscaled fat pixel
    vec3 c0 = texture(tex0, center + vec2(-0.5, -0.5) * texelNative).rgb;
    vec3 c1 = texture(tex0, center + vec2( 0.5, -0.5) * texelNative).rgb;
    vec3 c2 = texture(tex0, center + vec2(-0.5,  0.5) * texelNative).rgb;
    vec3 c3 = texture(tex0, center + vec2( 0.5,  0.5) * texelNative).rgb;
    
    float l0 = lum(c0);
    float l1 = lum(c1);
    float l2 = lum(c2);
    float l3 = lum(c3);
    
    float minL = min(min(l0, l1), min(l2, l3));
    float maxL = max(max(l0, l1), max(l2, l3));
    
    // If there is high contrast (an outline), always keep the "odd one out" pixel
    if (maxL - minL > 0.15) {
        float avgL = (l0 + l1 + l2 + l3) / 4.0;
        
        // If the average is closer to the max, it's a dark line on a bright background
        if (maxL - avgL < avgL - minL) {
            if (minL == l0) return c0;
            if (minL == l1) return c1;
            if (minL == l2) return c2;
            return c3;
        } 
        // If the average is closer to the min, it's a bright line on a dark background (like UI deck highlights)
        else {
            if (maxL == l0) return c0;
            if (maxL == l1) return c1;
            if (maxL == l2) return c2;
            return c3;
        }
    }
    
    // Otherwise, point sample to keep the flat pixel art blockiness
    return c0;
}

void main() {
    vec2 uv = vTexCoord;
    
    // Pixel-center sample in low-res grid
    vec2 texel = 1.0 / uLowRes;
    vec2 lowCoord = floor(uv * uLowRes) / uLowRes + 0.5 / uLowRes;
    
    vec3 base = getFatPixel(lowCoord);

    // --- Milder Color Modifications ---
    float luma = lum(base);
    base = mix(vec3(luma), base, 1.15); // Slight Saturation boost
    base *= 1.05; // Slight Brightness boost
    base = clamp(base, 0.0, 1.0);

    // Color Banding / Posterization
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
    float l = lum(getFatPixel(lowCoord + vec2(-texel.x, 0)));
    float r = lum(getFatPixel(lowCoord + vec2(texel.x, 0)));
    float u = lum(getFatPixel(lowCoord + vec2(0, -texel.y)));
    float d = lum(getFatPixel(lowCoord + vec2(0, texel.y)));
    
    float diffL = abs(c - l);
    float diffR = abs(c - r);
    float diffU = abs(c - u);
    float diffD = abs(c - d);

    float edge = max(max(diffL, diffR), max(diffU, diffD));

    // Threshold to detect crisp outlines on 3D models
    float edgeFactor = step(0.12, edge) * max(edgeStrength, 0.4);

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