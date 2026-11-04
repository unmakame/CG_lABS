#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inColor;

layout(set = 0, binding = 0, std140) uniform SceneUniforms {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 ambientColor;
    vec4 directionalDirection;
    vec4 directionalColor;
} scene;

layout(set = 1, binding = 0, std140) uniform ModelUniforms {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedoTint;
    vec4 specularTintAndPower;
} objectData;

layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec2 uv;
layout(location = 3) out vec3 vertexColor;

void main() {
    vec4 world = objectData.model * vec4(inPosition, 1.0);
    worldPosition = world.xyz;
    worldNormal = normalize(mat3(objectData.normalMatrix) * inNormal);
    uv = inUV;
    vertexColor = inColor;
    gl_Position = scene.viewProjection * world;
}
