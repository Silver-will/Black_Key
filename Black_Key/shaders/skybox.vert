#version 450

layout (location = 0) in vec3 inPos;

layout ( push_constant ) uniform constants 
{
	mat4 projview;
} PushConstants;

layout (location = 0) out vec3 outUVW;

void main()
{
	outUVW = inPos;
	outUVW.xy *= -1.0f;

	gl_Position = PushConstants.projview *  vec4(inPos,1.0f);
}