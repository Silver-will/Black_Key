#version 460 core
#extension GL_EXT_buffer_reference : require

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
layout(set = 0, binding = 0) uniform  ShadowData{   
	mat4 shadowMatrices[4];
} shadowData;


layout(set = 0, binding = 1) readonly buffer ObjectBuffer{   
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
	ObjectData obj = objectBuffer.objects[0];
	vec4 position = vec4(v.position, 1.0f);
	gl_Position = obj.model * position;
}