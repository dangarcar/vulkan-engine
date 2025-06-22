#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;


void main() {
    vec3 textureColor = vec3(texture(texSampler, fragTexCoord));
    outColor = vec4(textureColor * fragColor, 1);
}