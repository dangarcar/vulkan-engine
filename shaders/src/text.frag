#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 fragPosition;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;


void main() {
    outColor = color * texture(texSampler, fragTexCoord);

    if(abs(0.5 - fragPosition.x) > 0.49 || abs(0.5 - fragPosition.y) > 0.49) {
        outColor = vec4(0);
    }
}