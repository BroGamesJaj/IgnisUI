#version 450

#extension GL_EXT_nonuniform_qualifier : require


layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) out vec4 outColor;

vec3 hueToRGB(float h) {
    float r = abs(h * 6.0 - 3.0) - 1.0;
    float g = 2.0 - abs(h * 6.0 - 2.0);
    float b = 2.0 - abs(h * 6.0 - 4.0);
    return clamp(vec3(r,g,b), 0.0, 1.0);
}

void main() {

    if (true) {
        //vec4 texColor = texture(textures[nonuniformEXT(fragTexId)], fragTexCoord);

        outColor = vec4(1.0,1.0,1.0,1.0);

    } else {
        outColor = vec4(1.0,1.0,1.0,1.0);
    }
}

//vec3 funnyColor = hueToRGB(pc.color);
//outColor = texColor * vec4(funnyColor, 1.0);

