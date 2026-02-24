#version 150

uniform sampler2D tex0;
uniform vec2 uResolution;
uniform int horizontal; // legacy flag - kept for compatibility but ignored below
out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    vec2 texel = 1.0 / max(uResolution, vec2(1.0));
    vec3 result = vec3(0.0);

    // Simple 9-tap Gaussian (weights sum ~=1)
    float w0 = 0.2270270270;
    float w1 = 0.1945945946;
    float w2 = 0.1216216216;
    float w3 = 0.0540540541;

    // NOTE: swap axes if the bloom appears stretched on the wrong axis.
    // Some platforms or FBO orientations can invert texture axes; flipping
    // the meaning here will make the first pass blur along the other screen axis.
    // Perform both horizontal and vertical blurs and average them to avoid
    // depending on framebuffer/texture orientation. This makes the bloom
    // isotropic and removes axis-misalignment issues on some platforms.
    vec2 dirH = vec2(texel.x, 0.0);
    vec2 dirV = vec2(0.0, texel.y);

    vec3 resH = vec3(0.0);
    resH += texture(tex0, uv).rgb * w0;
    resH += texture(tex0, uv + dirH * 1.0).rgb * w1;
    resH += texture(tex0, uv - dirH * 1.0).rgb * w1;
    resH += texture(tex0, uv + dirH * 2.0).rgb * w2;
    resH += texture(tex0, uv - dirH * 2.0).rgb * w2;
    resH += texture(tex0, uv + dirH * 3.0).rgb * w3;
    resH += texture(tex0, uv - dirH * 3.0).rgb * w3;

    vec3 resV = vec3(0.0);
    resV += texture(tex0, uv).rgb * w0;
    resV += texture(tex0, uv + dirV * 1.0).rgb * w1;
    resV += texture(tex0, uv - dirV * 1.0).rgb * w1;
    resV += texture(tex0, uv + dirV * 2.0).rgb * w2;
    resV += texture(tex0, uv - dirV * 2.0).rgb * w2;
    resV += texture(tex0, uv + dirV * 3.0).rgb * w3;
    resV += texture(tex0, uv - dirV * 3.0).rgb * w3;

    result = (resH + resV) * 0.5;

    fragColor = vec4(result, 1.0);
}
