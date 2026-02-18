#version 150

uniform mat4 uLightVP;
uniform mat4 uModel;

in vec4 position;

void main() {
    gl_Position = uLightVP * uModel * position;
}
