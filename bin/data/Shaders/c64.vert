#version 150

in vec4 position;
in vec2 texcoord;
out vec2 vTexCoord;

uniform mat4 modelViewProjectionMatrix;

void main() {
    vTexCoord = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}