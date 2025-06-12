#version 450


#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"


struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	vec4 tangent;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

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