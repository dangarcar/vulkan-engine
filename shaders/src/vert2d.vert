#version 450

layout(binding = 0) uniform UBO2D {
    mat4 proj;
    mat4 model;
    vec4 modColor;
    int useTexture;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 modColor;
layout(location = 2) out int useTexture;

void main() {
    gl_Position = ubo.proj * ubo.model * vec4(inPosition, 0, 1);
    
    fragTexCoord = inTexCoord;
    modColor = ubo.modColor;
    useTexture = ubo.useTexture;
}