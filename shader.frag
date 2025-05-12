#version 450
#define MAX_TEXTURES 1024


layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) flat in int fragTexIndex;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D textures[MAX_TEXTURES];

void main() {
    if (fragTexIndex == -1) {
        outColor = vec4(fragColor, 1.0);
    } else {
        vec4 texColor = texture(textures[fragTexIndex], fragUV);
        texColor.rgb = pow(texColor.rgb, vec3(1.0 / 2.2)); 
        outColor = texColor;
    }
}
