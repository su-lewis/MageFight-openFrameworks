#version 150

uniform sampler2D tex0;
uniform vec2 uResolution;
out vec4 fragColor;

void main() {
    // gl_FragCoord is in window pixels, origin bottom-left.
    // openFrameworks 2D screen space is top-left, so flip Y to match.
    vec2 uv = gl_FragCoord.xy / max(uResolution, vec2(1.0));
    uv.y = 1.0 - uv.y;


    fragColor = texture(tex0, uv);
}
