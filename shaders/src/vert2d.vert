#version 450

layout(binding = 0) uniform UBO2D {
    mat4 proj;
    vec4 modColor;
    int useTexture;
} ubo;

layout(location = 0) in vec2 inPosition; //Because in the quad tex coords are the same as vertex positions in local space

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 modColor;
layout(location = 2) out int useTexture;

void main() {
    gl_Position = ubo.proj * vec4(inPosition, 0, 1);
    
    fragTexCoord = inPosition;
    modColor = ubo.modColor;
    useTexture = ubo.useTexture;
}