#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "../resource.glsl"

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
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
	uint inMaterialIndex;
} PushConstants;

invariant gl_Position;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	ObjectData obj = objectBuffer.objects[gl_BaseInstance];
	
	material_tex_In = obj.texture_index;
	material_buff_In = obj.texture_index / 5;
	vec4 position = vec4(v.position, 1.0f);
	vec4 fragPos = obj.model * position;
	gl_Position =  sceneData.viewproj * fragPos;
	
	mat3 normalMatrix = mat3(transpose(inverse(obj.model)));
	outNormal = normalMatrix * v.normal;
	outFragPos = vec3(fragPos.xyz);
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
}

