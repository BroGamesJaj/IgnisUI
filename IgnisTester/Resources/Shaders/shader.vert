#version 450

layout(push_constant) uniform PushConstants {
    mat4 rot;
    float offsetSize;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in uint inTexId;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out flat uint fragTexId;

layout(set = 0, binding = 0) uniform uniformbufferobject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position =(vec4(inPosition, 1.0)* pc.rot + vec4(pc.offsetSize, 0.0, 0.0, 0.0));
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragTexId = inTexId;
}
