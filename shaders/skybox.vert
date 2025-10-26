#version 450


layout(push_constant) uniform PushSkybox {
	mat4 projection;
	mat4 view;
} pc;

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 outUVW;

void main()  {
	outUVW = inPos.xzy;

	mat4 viewMat = mat4(mat3(pc.view)); //Remove translation
	gl_Position = pc.projection * viewMat * vec4(inPos, 1);
}