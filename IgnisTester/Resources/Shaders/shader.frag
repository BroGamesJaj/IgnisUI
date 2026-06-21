#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 uv;
layout(location = 1) in flat uint textureId;
layout(location = 2) in flat uint color;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures[];


vec3 hueToRGB(float h) {
    float r = abs(h * 6.0 - 3.0) - 1.0;
    float g = 2.0 - abs(h * 6.0 - 2.0);
    float b = 2.0 - abs(h * 6.0 - 4.0);
    return clamp(vec3(r,g,b), 0.0, 1.0);
}

void main() {

    float r = float((color >> 16) & 0xFF) / 255.0;
    float g = float((color >> 8)  & 0xFF) / 255.0;
    float b = float((color)       & 0xFF) / 255.0;

    if (textureId > 0) {
        vec4 texColor = texture(textures[nonuniformEXT(textureId)],uv);
        outColor = texColor* vec4(r,g,b,1.0);

    } else {
        outColor = vec4(r,g,b,1.0);
    }
}

//vec3 funnyColor = hueToRGB(pc.color);
//outColor = texColor * vec4(funnyColor, 1.0);

