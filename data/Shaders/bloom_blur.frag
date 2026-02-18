#version 150

uniform sampler2D tex0;
uniform vec2 uResolution;
uniform int horizontal; // 1 = horizontal, 0 = vertical
out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    uv.y = 1.0 - uv.y;
    vec2 texel = 1.0 / max(uResolution, vec2(1.0));
    vec3 result = vec3(0.0);

    // Simple 9-tap Gaussian (weights sum ~=1)
    float w0 = 0.2270270270;
    float w1 = 0.1945945946;
    float w2 = 0.1216216216;
    float w3 = 0.0540540541;

    vec2 dir = horizontal == 1 ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);

    result += texture(tex0, uv).rgb * w0;
    result += texture(tex0, uv + dir * 1.0).rgb * w1;
    result += texture(tex0, uv - dir * 1.0).rgb * w1;
    result += texture(tex0, uv + dir * 2.0).rgb * w2;
    result += texture(tex0, uv - dir * 2.0).rgb * w2;
    result += texture(tex0, uv + dir * 3.0).rgb * w3;
    result += texture(tex0, uv - dir * 3.0).rgb * w3;

    fragColor = vec4(result, 1.0);
}
