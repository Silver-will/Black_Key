#version 450

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projview;
} ubo;

layout (location = 0) out vec3 outUVW;

void main()
{
	outUVW = inPos;
	outUVW.xy *= -1.0f;

	gl_Position = ubo.projview *  vec4(inPos,1.0f);
}