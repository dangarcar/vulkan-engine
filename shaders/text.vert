#version 450

struct Character {
    mat4 proj;
    vec4 colorAndZIdx;
    vec4 texCoords;
};

layout(std140, binding = 0) readonly buffer CharacterSSBO {
    Character chars[ ];
};

layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 color;

void main() {
    float zIdx = chars[gl_InstanceIndex].colorAndZIdx.a;
    gl_Position = chars[gl_InstanceIndex].proj * vec4(inPosition, zIdx, 1);
    
    fragTexCoord = chars[gl_InstanceIndex].texCoords.xy;
    if(inPosition.x > 0.5) {
        fragTexCoord.x = chars[gl_InstanceIndex].texCoords.z;
    }
    if(inPosition.y > 0.5) {
        fragTexCoord.y = chars[gl_InstanceIndex].texCoords.w;
    }
    
    color.rgb = chars[gl_InstanceIndex].colorAndZIdx.rgb;
}