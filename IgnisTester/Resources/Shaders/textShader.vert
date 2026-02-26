// TODO: make this work for sdf
#version 450

// layout(location = 0) in vec2 inposition;
// layout(location = 1) in vec2 insize;
// layout(location = 2) in vec4 intexcoord; // uv
// layout(location = 3) in uint intexid;
// layout(location = 4) in vec3 incolor;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in uint inTexId;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out flat uint fragTexId;

layout(set = 1, binding = 0) uniform uniformbufferobject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragTexId = inTexId;
}
