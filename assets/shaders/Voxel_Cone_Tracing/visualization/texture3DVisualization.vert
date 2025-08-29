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
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);
	
	debugPrintfEXT("Transformed position = %v4f", position);

	gl_Position =  position;
}