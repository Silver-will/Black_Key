#version 460 core

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

struct ObjectData{
	vec4 spherebounds;
	uint texture_index;
    uint firstIndex;
    uint indexCount;
    mat4 model;
	VertexBuffer vertexBuffer;
	vec3 pad;
}; 

layout(std140,set = 0, binding = 5) readonly buffer ObjectBuffer{   
	ObjectData objects[];
} objectBuffer;


//push constants block
layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
	uint material_index;
} PushConstants;

invariant gl_Position;

void main()
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	ObjectData obj = objectBuffer.objects[gl_BaseInstance];
	vec4 position = vec4(v.position, 1.0f);
	vec4 fragPos = obj.model * position;
	gl_Position =  sceneData.viewproj * fragPos;
}