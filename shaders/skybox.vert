#version 450


layout (binding = 0) uniform UBO {
	mat4 projection;
	mat4 view;
} ubo;

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 outUVW;

void main()  {
	outUVW = inPos.xzy;

	mat4 viewMat = mat4(mat3(ubo.view)); //Remove translation
	gl_Position = ubo.projection * viewMat * vec4(inPos, 1);
}