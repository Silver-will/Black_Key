#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outFragPos;
layout (location = 3) out vec2 outUV;
layout (location = 4) out mat3 outTBN;

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
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	vec4 fragPos = PushConstants.render_matrix * position;
	gl_Position =  sceneData.viewproj * fragPos;	

	//Note: Change this to transpose of inverse of render mat
	mat3 normalMatrix = mat3(PushConstants.render_matrix);
	vec3 T = normalize(normalMatrix * vec3(v.tangent.xyz));
	vec3 N = normalize(normalMatrix * v.normal);
	T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

	outTBN = mat3(T, B, N);
	outNormal = v.normal;
	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outFragPos = vec3(fragPos.xyz);
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;

}
