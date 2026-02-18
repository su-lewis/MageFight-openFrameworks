#version 150

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uNormalMatrix; // model->view normal matrix
uniform mat4 uLightVP;

in vec4 position;
in vec3 normal;
in vec2 texcoord;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vLightSpacePos;

void main() {
    vec4 worldPos = uModel * position;
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uNormalMatrix) * normal;
    vTexCoord = texcoord;
    vLightSpacePos = uLightVP * worldPos;
    gl_Position = uViewProj * worldPos;
}
