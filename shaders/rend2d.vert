#version 450

layout(push_constant) uniform Push2d {
    mat4 proj;
    vec4 modColor;
    vec2 origin, relSize;
    int useTexture;
    float zIndex;
} pc;

layout(location = 0) in vec2 inPosition; //Because in the quad tex coords are the same as vertex positions in local space

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 modColor;
layout(location = 2) out int useTexture;

void main() {
    gl_Position = pc.proj * vec4(inPosition, pc.zIndex, 1);

    fragTexCoord = pc.origin + inPosition * pc.relSize;
    modColor = pc.modColor;
    useTexture = pc.useTexture;
}