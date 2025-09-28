#version 450

layout (location = 0) in vec3 inUVW;

layout (binding = 1) uniform samplerCube samplerCubeMap;

layout (location = 0) out vec4 outColorSpecular;
layout (location = 1) out vec4 outPosition;
layout (location = 2) out vec4 outNormal;
layout (location = 3) out uint outPicking;

void main() {
	outColorSpecular = vec4(texture(samplerCubeMap, inUVW).rgb, 0);
	outNormal = vec4(0);
	outPicking = 0xFFFFFFFF;
}