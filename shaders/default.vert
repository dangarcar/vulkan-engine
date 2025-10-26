#version 450

layout(push_constant) uniform PushDefault {
    mat4 model;
    mat4 projView;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;


layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;


void main() {
    gl_Position = pc.projView * pc.model * vec4(inPosition, 1.0);

    fragTexCoord = inTexCoord;
    fragPos = inPosition;
    fragNormal = normalize(mat3(transpose(inverse(pc.model))) * inNormal);
}