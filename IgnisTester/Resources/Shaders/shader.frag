#version 450

#define MAX_TEXTURES 1028

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in flat uint fragTexId;

layout(location = 3) in flat uint fragPageId;

layout(set = 0, binding = 1) uniform sampler2D textures[MAX_TEXTURES];

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor;
    
    // Glyph mode: use pageId as texture index
    if (fragPageId > 0) {
        uint texIndex = fragPageId;
        if (texIndex < MAX_TEXTURES) {
            texColor = texture(textures[texIndex], fragTexCoord);
            // Proper alpha premultiplication for fonts
            outColor = vec4(texColor.rgb * texColor.a * fragColor.rgb, texColor.a);
        } else {
            outColor = vec4(0.0);  // Transparent fallback
        }
    } 
    // Regular geometry: use original texId logic
    else if (fragTexId < MAX_TEXTURES && fragTexId > 0) {
        texColor = texture(textures[fragTexId], fragTexCoord);
        outColor = texColor * vec4(fragColor, 1.0);
    } 
    // No texture case
    else {
        outColor = vec4(fragColor, 1.0);
    }
}

