#version 450

struct Character {
    mat4 proj;
    vec4 color;
    vec4 texCoords;
};

layout(std140, binding = 0) readonly buffer CharacterSSBO {
    Character chars[ ];
};

layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 color;
layout(location = 2) out vec2 fragPosition;

void main() {
    gl_Position = chars[gl_InstanceIndex].proj * vec4(inPosition, 0, 1);
    
    fragTexCoord = chars[gl_InstanceIndex].texCoords.xy;
    if(inPosition.x > 0.5) {
        fragTexCoord.x = chars[gl_InstanceIndex].texCoords.z;
    }
    if(inPosition.y > 0.5) {
        fragTexCoord.y = chars[gl_InstanceIndex].texCoords.w;
    }
    
    color = chars[gl_InstanceIndex].color;
    fragPosition = inPosition;
}