#version 450

#extension GL_EXT_nonuniform_qualifier : require

#define MAX_TEXTURES 1028

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in flat uint fragTexId;


layout(push_constant) uniform PushConstants {
    layout(offset = 4) float color;
} pc;

layout(set = 0, binding = 1) uniform sampler2D textures[];

layout(location = 0) out vec4 outColor;

vec3 hueToRGB(float h) {
    float r = abs(h * 6.0 - 3.0) - 1.0;
    float g = 2.0 - abs(h * 6.0 - 2.0);
    float b = 2.0 - abs(h * 6.0 - 4.0);
    return clamp(vec3(r,g,b), 0.0, 1.0);
}

void main() {
    vec3 funnyColor = hueToRGB(pc.color);

    if (fragTexId < MAX_TEXTURES && fragTexId > 0) {
        vec4 texColor = texture(textures[nonuniformEXT(fragTexId)], fragTexCoord);

        outColor = texColor * vec4(funnyColor, 1.0);

    } else {
        outColor = vec4(fragColor, 1.0) * vec4(funnyColor, 1.0);
    }
}

//vec3 funnyColor = hueToRGB(pc.color);
//outColor = texColor * vec4(funnyColor, 1.0);

