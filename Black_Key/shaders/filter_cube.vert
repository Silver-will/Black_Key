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

layout(push_constant) uniform constants {
	layout (offset = 0) mat4 mvp;
	VertexBuffer vertexBuffer;
} PushConstants;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	outUVW = v.position.xyz;
	gl_Position = PushConstants.mvp * vec4(outUVW, 1.0);
}
