#version 460 core

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require


layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 skyMat;
	vec4 cameraPos;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec3 cascadeConfigData;
	uint lightCount;
	vec4 distances;
	vec4 debugInfo;
} sceneData;

struct VoxelVertex {
	vec4 position;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	VoxelVertex vertices[];
};

struct ObjectData{
	mat4 model;
	vec4 spherebounds;
	uint texture_index;
    uint firstIndex;
    uint indexCount;
	uint firstVertex;
	uint vertexCount;
	uint firstInstance;
	VertexBuffer vertexBuffer;
	vec4 pad;
}; 

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
	VoxelVertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position =  v.position;
	gl_Position =  position;
}