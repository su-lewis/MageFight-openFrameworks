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
    vec2 uv = vTexCoord;
    
    // Pixel-center sample in low-res grid (crisp pixel-art sampling)
    vec2 texel = 1.0 / uLowRes;
    vec2 lowCoord = floor(uv * uLowRes) / uLowRes + 0.5 / uLowRes;
    
    vec3 base = texture(tex0, lowCoord).rgb;

    // --- FIX: BRIGHTEN THE IMAGE ---
    // Boost saturation slightly, and add a flat 20% brightness boost so it isn't so dark
    float luma = lum(base);
    base = mix(vec3(luma), base, 1.30); // Saturation boost
    base *= 1.20; // 20% Brightness boost
    base = clamp(base, 0.0, 1.0);

    // Color Banding / Posterization
    float L = max(1.0, float(levels));
    vec3 q = base;
    
    if (L > 1.0) {
        if (useDither == 1) {
            float d = (bayer4(gl_FragCoord.xy) - 0.5) * 0.1;
            q = floor((q + d) * L) / L;
        } else {
            q = floor(q * L) / L;
        }
    }

    // Crisp Edge Detection (Sobel)
    float c = lum(base);
    float l = lum(texture(tex0, lowCoord + vec2(-texel.x, 0)).rgb);
    float r = lum(texture(tex0, lowCoord + vec2(texel.x, 0)).rgb);
    float u = lum(texture(tex0, lowCoord + vec2(0, -texel.y)).rgb);
    float d = lum(texture(tex0, lowCoord + vec2(0, texel.y)).rgb);
    
    float gx = (r - l);
    float gy = (d - u);
    float edge = sqrt(gx * gx + gy * gy);

    // Hard threshold for a crisp 1-pixel dark outline
    float edgeFactor = step(0.15, edge) * max(edgeStrength, 0.5);

    // Apply outline (darken the edge)
    vec3 finalCol = mix(q, edgeColor, edgeFactor * 0.85);

    fragColor = vec4(clamp(finalCol, 0.0, 1.0), 1.0);
}