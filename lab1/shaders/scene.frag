#version 450
// Lab 1: procedural vertex color multiplied by the user-selected model color.
layout(set=1,binding=0,std140) uniform ModelUniforms {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedoTint;
    vec4 specularTintAndPower;
} objectData;
layout(push_constant) uniform MaterialOptions {
    float time;
    uint animatedSampling;
    uint showVertexColor;
    uint unlit;
} options;
layout(location=2) in vec2 uv;
layout(location=3) in vec3 vertexColor;
layout(location=0) out vec4 outColor;
void main() {
    vec3 color=objectData.albedoTint.rgb;
    if ((options.showVertexColor & 1u)!=0u) color*=vertexColor;
    if ((options.showVertexColor & 2u)!=0u) {
        vec2 grid=uv*vec2(12,8);
        vec2 d=abs(fract(grid-0.5)-0.5)/max(fwidth(grid),vec2(0.0001));
        color*=mix(0.25,1.0,clamp(min(d.x,d.y),0.0,1.0));
    }
    outColor=vec4(color,1);
}
