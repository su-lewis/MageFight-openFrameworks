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
    vec2 texelLow = 1.0 / uLowRes;
    vec2 gridCell = floor(vTexCoord * uLowRes);
    vec2 lowCoord = (gridCell + 0.5) * texelLow;
    
    // 4-tap box filter within the low-res cell
    // Preserves thin 1px/2px outlines and prevents flickering dropouts during movement
    vec2 offset = texelLow * 0.25;
    vec3 base = (
        texture(tex0, lowCoord + vec2(-offset.x, -offset.y)).rgb +
        texture(tex0, lowCoord + vec2( offset.x, -offset.y)).rgb +
        texture(tex0, lowCoord + vec2(-offset.x,  offset.y)).rgb +
        texture(tex0, lowCoord + vec2( offset.x,  offset.y)).rgb
    ) * 0.25;

    // --- Color Adjustments ---
    float luma = lum(base);
    base = mix(vec3(luma), base, 1.15); // Saturation
    base *= 1.05; // Brightness
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

    // --- Outline / Edge Preservation ---
    if (edgeStrength > 0.001) {
        float c = lum(base);
        float l = lum(texture(tex0, lowCoord + vec2(-texelLow.x, 0.0)).rgb);
        float r = lum(texture(tex0, lowCoord + vec2( texelLow.x, 0.0)).rgb);
        float u = lum(texture(tex0, lowCoord + vec2(0.0, -texelLow.y)).rgb);
        float d = lum(texture(tex0, lowCoord + vec2(0.0,  texelLow.y)).rgb);
        
        float edge = max(max(abs(c - l), abs(c - r)), max(abs(c - u), abs(c - d)));
        float edgeFactor = smoothstep(0.10, 0.22, edge) * edgeStrength;

        if (lum(q) >= 0.12 && lum(q) <= 0.85) {
            q = mix(q, edgeColor, edgeFactor * 0.5);
        }
    }

    fragColor = vec4(clamp(q, 0.0, 1.0), 1.0);
}