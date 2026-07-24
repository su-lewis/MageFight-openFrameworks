#version 150

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D tex0;
uniform float uTime;
uniform vec2 uResolution;
uniform float uScanlineIntensity;
uniform float uPixelSize;

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

void main() {
    vec2 uv = vTexCoord;
    vec4 col = texture(tex0, uv);
    vec3 color = col.rgb;

    // Apply retro pixelation blockiness
    float pixelSize = max(1.0, uPixelSize);
    if (pixelSize > 1.5) {
        vec2 pxUv = floor(uv * uResolution / pixelSize) * pixelSize / uResolution;
        color = texture(tex0, pxUv).rgb;
    }

    // Keep natural lighting colors - NO artificial saturation boost to prevent hue shifts
    color = clamp(color, 0.0, 1.0);

    // Softened dither texture
    float dither = (bayer4(gl_FragCoord.xy) - 0.5) * 0.025;
    
    // 16 levels per channel (12-bit palette / 4,096 colors)
    // Preserves exact color hues: green-yellow walls and blue-purple floors stay accurate!
    vec3 mapped = clamp(color + dither, 0.0, 1.0);
    float levels = 15.0; 
    mapped = floor(mapped * levels + 0.5) / levels;

    // --- PROMINENT CRT SCANLINES ---
    float scan = sin(gl_FragCoord.y * 1.25) * 0.5 + 0.5;
    scan = pow(scan, 1.4);
    float scanlineDarkness = clamp(uScanlineIntensity * 3.5, 0.15, 0.55); 
    mapped *= mix(1.0, 1.0 - scanlineDarkness, scan);

    // Subtle CRT Vignette (Darker corners)
    vec2 crtUV = uv * 2.0 - 1.0;
    float vignette = 1.0 - dot(crtUV, crtUV) * 0.20;
    mapped *= vignette;

    fragColor = vec4(mapped, col.a);
}