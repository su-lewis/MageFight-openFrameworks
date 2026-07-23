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

    // Just a slight saturation boost to make colors pop. NO artificial brightness boost.
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, 1.15); 
    color = clamp(color, 0.0, 1.0);

    // Softer dither
    float dither = (bayer4(gl_FragCoord.xy) - 0.5) * 0.05;
    
    // Uniform quantization (prevents dark grays from shifting yellow/brown)
    // 7.0 gives 8 values per channel (512 colors total)
    vec3 mapped = clamp(color + dither, 0.0, 1.0);
    float levels = 7.0; 
    mapped = floor(mapped * levels + 0.5) / levels;

    // Normal scanlines (less aggressive since the base image is no longer artificially brightened)
    float scan = sin(gl_FragCoord.y * 2.5) * 0.5 + 0.5;
    float scanlineDarkness = clamp(uScanlineIntensity * 1.5, 0.05, 0.35); 
    mapped *= mix(1.0, 1.0 - scanlineDarkness, scan);

    // Subtle CRT Vignette (Darker corners)
    vec2 crtUV = uv * 2.0 - 1.0;
    float vignette = 1.0 - dot(crtUV, crtUV) * 0.20;
    mapped *= vignette;

    fragColor = vec4(mapped, col.a);
}