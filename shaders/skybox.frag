#version 450

layout (location = 0) in vec3 inUVW;

layout (binding = 1) uniform samplerCube samplerCubeMap;

layout (location = 0) out vec4 outColor;
layout (location = 2) out vec4 outNormal;

void main() {
	outColor = texture(samplerCubeMap, inUVW);
	outNormal = vec4(0);
}