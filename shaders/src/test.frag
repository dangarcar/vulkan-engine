#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float gamma;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;


void main() {
    outColor = vec4(texture(texSampler, fragTexCoord) * vec4(fragColor, 1));
    
    outColor.r = pow(outColor.r, 1.0/gamma);
    outColor.g = pow(outColor.g, 1.0/gamma);
    outColor.b = pow(outColor.b, 1.0/gamma);
}