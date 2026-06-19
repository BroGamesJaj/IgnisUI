#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float isInstance;

layout(set = 1, binding = 0) uniform uniformbufferobject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 1, binding = 1) buffer StorageBuffer
{
    vec4 data[];
} ssbo;

void main() {
    gl_Position = vec4(inPosition, 1.0);
}
