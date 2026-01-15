#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in uint inTexId;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out uint fragTexId;

layout(location = 4) in vec3 inGlyphPosition;
layout(location = 5) in vec3 inGlyphSize;
layout(location = 6) in vec4 inUvRect;
layout(location = 7) in uint inPageId;

layout(location = 3) out uint fragPageId; 


layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
void main() {
    // Detect rendering mode by checking if glyph data is valid
    if (inGlyphPosition.x != inGlyphPosition.y || inGlyphSize.x > 0.0) {  // Glyph mode
        // Generate quad vertex from instance data (4 verts per glyph)
        vec3 localPos;
        if (gl_VertexIndex == 0) localPos = vec3(-1,  1, 0);  // TL
        else if (gl_VertexIndex == 1) localPos = vec3( 1,  1, 0);  // TR
        else if (gl_VertexIndex == 2) localPos = vec3(-1, -1, 0);  // BL
        else localPos = vec3( 1, -1, 0);  // BR
        
        vec3 worldPos = inGlyphPosition 
            + inGlyphSize 
            * 0.5 
            * localPos;
        
        // Compute UV from uvRect (u0,v0,u1,v1)
        vec2 localUv;
        if (gl_VertexIndex == 0) localUv = vec2(0, 0);
        else if (gl_VertexIndex == 1) localUv = vec2(1, 0);
        else if (gl_VertexIndex == 2) localUv = vec2(0, 1);
        else localUv = vec2(1, 1);
        
        fragTexCoord = inUvRect.xy + inUvRect.zw * localUv;
        fragColor = vec3(1.0);  // White by default (override with push const)
        fragTexId = inPageId;
        fragPageId = inPageId;
        
        gl_Position = vec4(worldPos, 1.0);  // Screen space
    } else {  // Regular geometry mode
        gl_Position = vec4(inPosition, 1.0);//ubo.proj * ubo.view * ubo.model * 
        fragColor = inColor;
        fragTexCoord = inTexCoord;
        fragTexId = inTexId;
        fragPageId = 0;
    }
}
