#version 150

uniform sampler2D albedoTex;
uniform sampler2D normalTex; // optional
uniform sampler2D shadowMap;
uniform vec3 uViewPos;
uniform vec3 lightDir; // in world space (normalized, from light to scene)
uniform vec3 lightColor;
uniform float metallic;
uniform float roughness;
uniform int useAlbedoTex;
uniform int useNormalTex;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vLightSpacePos;

out vec4 fragColor;

// Schlick Fresnel
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float shadowPCF(sampler2D shadowMap, vec3 projCoords) {
    // projCoords: (x,y,z) where x,y in [0,1], z = depth
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float bias = 0.005;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float closestDepth = texture(shadowMap, projCoords.xy + offset).r;
            float currentDepth = projCoords.z - bias;
            if (currentDepth > closestDepth) shadow += 1.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec3 N = normalize(vNormal);
    if (useNormalTex == 1) {
        vec3 nmap = texture(normalTex, vTexCoord).rgb;
        nmap = nmap * 2.0 - 1.0;
        N = normalize(nmap);
    }

    vec3 albedo = vec3(1.0);
    if (useAlbedoTex == 1) {
        albedo = texture(albedoTex, vTexCoord).rgb;
    }

    // Direct lighting (single directional light)
    vec3 L = normalize(-lightDir); // lightDir is from light to scene; use opposite
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // Cook-Torrance terms
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(VdotH, F0);

    float a = roughness * roughness;
    float a2 = a*a;
    float denom = (NdotH*NdotH)*(a2-1.0) + 1.0;
    float D = a2 / (3.14159265 * denom * denom + 1e-6);

    float k = (a + 1.0)*(a + 1.0) / 8.0; // geometry Schlick-GGX
    float G_V = NdotV / (NdotV * (1.0 - k) + k);
    float G_L = NdotL / (NdotL * (1.0 - k) + k);
    float G = G_V * G_L;

    vec3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 1e-6;
    vec3 specular = numerator / denominator;

    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / 3.14159265;

    // Shadow
    vec3 proj = vLightSpacePos.xyz / vLightSpacePos.w;
    proj = proj * 0.5 + 0.5; // transform to 0..1
    float shadow = 0.0;
    if (proj.x >= 0.0 && proj.x <= 1.0 && proj.y >= 0.0 && proj.y <= 1.0 && proj.z <= 1.0) {
        shadow = shadowPCF(shadowMap, proj);
    }

    vec3 Lo = (diffuse + specular) * lightColor * NdotL * (1.0 - shadow);

    // Ambient (simple)
    vec3 ambient = vec3(0.03) * albedo;

    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0)); // simple tonemap
    color = pow(color, vec3(1.0/2.2));
    fragColor = vec4(color, 1.0);
}
