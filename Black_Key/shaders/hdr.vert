#version 450 core

#extension GL_EXT_buffer_reference : require

layout (location = 0)out vec2 TexCoords;

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

//push constants block
layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main()
{	
	/*Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position,1.0f);
	position.y *= -1;
	TexCoords = vec2(v.uv_x,v.uv_y);
	gl_Position = position;
	*/

	TexCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(TexCoords * 2.0f + -1.0f, 0.0f, 1.0f);
}