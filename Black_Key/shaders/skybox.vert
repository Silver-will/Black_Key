#version 460 core


#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"


layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
} PushConstants;

layout (location = 0) out vec3 outUVW;


void main()
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	outUVW = v.position.xyz;
	outUVW.y *= -1.0f;
	vec3 pos = outUVW;
	//pos.y *= -1.0f;
	vec4 position = sceneData.skyMat * vec4(pos,1.0f);
	gl_Position = position.xyww;
}