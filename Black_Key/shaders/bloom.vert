#version 460 core

layout (location = 0)out vec2 TexCoords;

layout(push_constant) uniform UpSampleParams
{
	vec2 screenDimensions;
	float filterRadius;
};
void main()
{	
	TexCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(TexCoords * 2.0f + -1.0f, 0.0f, 1.0f);
}