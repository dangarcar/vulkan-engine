#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 color;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;


void main() {
    outColor = color * texture(texSampler, fragTexCoord).r;
}