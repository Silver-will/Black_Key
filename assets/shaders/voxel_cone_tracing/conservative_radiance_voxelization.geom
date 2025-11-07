#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : require
#include "common.glsl"
#include "../global_resources.glsl"


layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec3 inFragPos[];
layout (location = 2) in vec3 inNormalisedPosition[];
layout (location = 3) in vec2 inUV[];
layout (location = 4) flat in uint material_tex_In[];
layout (location = 5) flat in uint material_buff_In[];



layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outFragPos;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outNormPosition;
layout (location = 4) flat out uint material_tex_out;
layout (location = 5) flat out uint material_buff_out;

const float voxel_size = 15.0f;
const float voxel_resolution = 128.0f;

layout(set = 0, binding = 4) uniform  ProjViewMatUB{   
    mat4 projViewMat[6]; //
} MatrixUB;

void main()
{
    int idx = getDominantAxisIdx(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);
	//gl_ViewportIndex = idx;
	
	for (int i = 0; i < 3; ++i)
    {
        vec4 clipPosition = MatrixUB.projViewMat[idx] * gl_in[i].gl_Position;
        //debugPrintfEXT("Clip space position = %v4f", clipPosition);
        vec3 worldPosition = gl_in[i].gl_Position.xyz;
        outFragPos = inFragPos[i];
        outNormPosition = inNormalisedPosition[i];
        outNormal = inNormal[i];
        material_tex_out = material_tex_In[i];
        material_buff_out = material_buff_In[i];
        outUV = inUV[i];
        gl_Position = clipPosition;
        EmitVertex();		
    }

    EndPrimitive();
}
