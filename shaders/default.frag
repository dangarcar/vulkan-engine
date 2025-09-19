#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPosition;

layout(binding = 1) uniform sampler2D texSampler;


layout (location = 0) out vec4 outColorSpecular;
layout (location = 1) out vec4 outPosition;
layout (location = 2) out vec4 outNormal;
layout (location = 3) out uint outPicking;

void main() {
    vec3 textureColor = vec3(texture(texSampler, fragTexCoord));
    
    outColorSpecular = vec4(textureColor * fragColor, 0.5);
    outPosition = vec4(fragPosition, 1);
    outNormal = vec4(1);
    outPicking = 0xFFFFFFFF;
}