#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "resource.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outFragPos;
layout (location = 3) out vec3 outViewPos;
layout (location = 4) out vec3 outPos;
layout (location = 5) out vec2 outUV;
layout (location = 6) out vec4 outTangent;
layout (location = 7) out mat3 outTBN;

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
	vec4 fragPos = PushConstants.render_matrix * position;
	gl_Position =  sceneData.viewproj * fragPos;	

	//Note: Change this to transpose of inverse of render mat
	mat3 normalMatrix = mat3(transpose(inverse(PushConstants.render_matrix)));
	vec3 T = normalize(normalMatrix * vec3(v.tangent.xyz));
	vec3 N = normalize(normalMatrix * v.normal);
	//T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

	outTBN = mat3(T, B, N);
	outNormal = normalMatrix * v.normal;
	//outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outColor = v.color.xyz;
	outFragPos = vec3(fragPos.xyz);
	outViewPos = (sceneData.view * position).xyz;
	outPos = v.position;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outTangent.xyz = normalMatrix * v.tangent.xyz;
	outTangent.w = v.tangent.w;
}

