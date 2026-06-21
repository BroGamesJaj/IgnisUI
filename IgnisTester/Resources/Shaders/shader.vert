#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint isInstance;

layout(location = 0) out vec2 uv;
layout(location = 1) out flat uint textureId;
layout(location = 2) out flat uint color;

struct InstanceData {
    mat4 model;
    vec4 uv;        // xy = offset, zw = size
    uint textureId;
    uint color;
};

layout(set = 1, binding = 0) uniform uniformbufferobject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(std430, set = 1, binding = 1) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

vec2 corners[4] = vec2[](
    vec2(0.0, 0.0),  // top-left
    vec2(1.0, 0.0),  // top-right
    vec2(1.0, 1.0),  // bottom-right
    vec2(0.0, 1.0)   // bottom-left
);

void main() {

    //basicly if its ui, maybe will change this to bool later
    if(isInstance == 1){
        InstanceData instance = instances[gl_InstanceIndex];

        vec2 corner = corners[gl_VertexIndex % 4];
        uv = instance.uv.xy + corner * instance.uv.zw;
        textureId = instance.textureId;
        color = instance.color;
        gl_Position = instance.model* vec4(inPosition, 1.0);
    }

}
