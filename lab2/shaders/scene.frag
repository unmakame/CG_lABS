#version 450

struct PointLight {
    vec4 position;
    vec4 colorIntensity;
};
struct SpotLight {
    vec4 position;
    vec4 directionInnerCos;
    vec4 colorIntensity;
    vec4 outerCosAndPadding;
};

layout(set = 0, binding = 0, std140) uniform SceneUniforms {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 ambientColor;
    vec4 directionalDirection;
    vec4 directionalColor;
} scene;
layout(set = 0, binding = 1, std430) readonly buffer PointLights {
    uint count;
    PointLight values[];
} pointLights;
layout(set = 0, binding = 2, std430) readonly buffer SpotLights {
    uint count;
    SpotLight values[];
} spotLights;
layout(set = 1, binding = 0, std140) uniform ModelUniforms {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedoTint;
    vec4 specularTintAndPower;
} objectData;
layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D specularMap;
layout(set = 1, binding = 3) uniform sampler2D emissiveMap;
layout(push_constant) uniform MaterialOptions {
    float time;
    uint animatedSampling;
    uint showVertexColor;
    uint unlit;
} options;

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 vertexColor;
layout(location = 0) out vec4 outColor;

vec3 safeNormalize(vec3 value) {
    float lengthSquared = dot(value, value);
    return lengthSquared > 1e-8 ? value * inversesqrt(lengthSquared) : vec3(0.0, 1.0, 0.0);
}

vec3 blinnPhong(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, vec3 specularMask, float power) {
    float nDotL = max(dot(N, L), 0.0);
    vec3 H = safeNormalize(L + V);
    float spec = nDotL > 0.0 ? pow(max(dot(N, H), 0.0), max(power, 1.0)) : 0.0;
    return radiance * (albedo * nDotL + specularMask * spec);
}

float surfaceGrid() {
    if ((options.showVertexColor & 2u) == 0u) return 1.0;
    vec2 grid = uv * vec2(12.0, 8.0);
    vec2 distanceToLine = abs(fract(grid - 0.5) - 0.5) / max(fwidth(grid), vec2(0.0001));
    return mix(0.25, 1.0, clamp(min(distanceToLine.x, distanceToLine.y), 0.0, 1.0));
}
void main() {
    vec3 albedo = objectData.albedoTint.rgb;
    vec3 specularMask = objectData.specularTintAndPower.rgb;
    if (options.unlit == 2u) {
        outColor = vec4(albedo,1.0);
        return;
    }
    vec3 N = safeNormalize(worldNormal);
    vec3 V = safeNormalize(scene.cameraPosition.xyz - worldPosition);
    vec3 lit = scene.ambientColor.rgb * albedo;

    vec3 Ld = safeNormalize(-scene.directionalDirection.xyz);
    lit += blinnPhong(N, V, Ld, scene.directionalColor.rgb * scene.directionalColor.a,
                      albedo, specularMask, objectData.specularTintAndPower.a);

    for (uint i = 0; i < pointLights.count; ++i) {
        vec3 delta = pointLights.values[i].position.xyz - worldPosition;
        float distance2 = max(dot(delta, delta), 0.01);
        vec3 L = delta * inversesqrt(max(dot(delta, delta), 0.01));
        vec3 radiance = pointLights.values[i].colorIntensity.rgb *
                        pointLights.values[i].colorIntensity.a / distance2;
        lit += blinnPhong(N, V, L, radiance, albedo, specularMask,
                          objectData.specularTintAndPower.a);
    }

    for (uint i = 0; i < spotLights.count; ++i) {
        SpotLight light = spotLights.values[i];
        vec3 delta = light.position.xyz - worldPosition;
        float distance2 = max(dot(delta, delta), 0.01);
        vec3 L = delta * inversesqrt(max(dot(delta, delta), 0.01));
        float theta = dot(safeNormalize(-L), safeNormalize(light.directionInnerCos.xyz));
        float cone = (light.directionInnerCos.w > light.outerCosAndPadding.x + 1e-6 ? smoothstep(light.outerCosAndPadding.x, light.directionInnerCos.w, theta) : step(light.outerCosAndPadding.x, theta));
        vec3 radiance = light.colorIntensity.rgb * light.colorIntensity.a * cone / distance2;
        lit += blinnPhong(N, V, L, radiance, albedo, specularMask,
                          objectData.specularTintAndPower.a);
    }

    outColor = vec4(lit, 1.0);
}
