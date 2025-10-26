#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D texSampler;


layout (location = 0) out vec4 outColorSpecular;
layout (location = 1) out vec4 outPosition;
layout (location = 2) out vec4 outNormal;
layout (location = 3) out uint outPicking;

void main() {
    vec3 textureColor = vec3(texture(texSampler, fragTexCoord));
    
    outColorSpecular = vec4(textureColor, 0.5);
    outPosition = vec4(fragPos, 1);
    outNormal = vec4(fragNormal, 1);
    outPicking = 0xFFFFFFFF;
}