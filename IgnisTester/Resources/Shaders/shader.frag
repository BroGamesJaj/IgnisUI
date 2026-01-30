#version 450

#extension GL_EXT_nonuniform_qualifier : require

#define MAX_TEXTURES 1028

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in flat uint fragTexId;

layout(set = 0, binding = 1) uniform sampler2D textures[];

layout(location = 0) out vec4 outColor;

void main() {
    if (fragTexId < MAX_TEXTURES && fragTexId > 0) {
        vec4 texColor = texture(textures[nonuniformEXT(fragTexId)], fragTexCoord);
        outColor = texColor * vec4(fragColor, 1.0);
    } else {
        outColor = vec4(fragColor, 1.0);
    }
}

