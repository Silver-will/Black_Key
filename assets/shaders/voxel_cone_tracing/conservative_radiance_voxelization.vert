#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : require

#include "../types.glsl"
#include "../global_resources.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outFragPos;
layout (location = 2) out vec2 outUV;
layout (location = 3) flat out uint material_tex_In;
layout (location = 4) flat out uint material_buff_In;

layout(set = 0, binding = 1) readonly buffer ObjectBuffer{   
	ObjectData objects[];
} objectBuffer;

//push constants block
layout( push_constant ) uniform constants
{
	vec4 min_bounding_box;
    vec4 max_bounding_box;
	VertexBuffer vertexBuffer;
} PushConstants;

invariant gl_Position;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	ObjectData obj = objectBuffer.objects[gl_BaseInstance];

	//debugPrintfEXT("indirect world position = %v3f", test.position.xyz);
	
	material_tex_In = obj.texture_index;
	material_buff_In = obj.texture_index / 5;
	vec4 position = vec4(v.position, 1.0f);
	vec4 fragPos = obj.model * position;
	
	mat3 normalMatrix = mat3(transpose(inverse(obj.model)));
	outNormal = normalMatrix * v.normal;

	outFragPos = vec3(fragPos.xyz);
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	//vec4 region_max = obj.model * PushConstants.max_bounding_box;
	//vec4 region_min = obj.model * PushConstants.min_bounding_box;
	vec4 region_max = vec4(12);
	vec4 region_min = vec4(-12);
	region_max.x = 15;
	
	vec3 fragPosCorrected = fragPos.xyz;
	vec3 fragPosNorm =  (fragPosCorrected.xyz - region_min.xyz)  / (region_max.xyz - region_min.xyz);
	fragPosNorm = 2.0f * fragPosNorm - 1.0f;
	//debugPrintfEXT("transformed region max = %v4f", region_max);
	//fragPosNorm *= 1.4f;
	gl_Position =  vec4(fragPosNorm,1);
}

