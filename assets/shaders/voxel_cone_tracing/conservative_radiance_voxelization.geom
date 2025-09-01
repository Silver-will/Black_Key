#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : require
#include "common.glsl"
#include "../global_resources.glsl"


layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec3 inFragPos[];
layout (location = 2) in vec2 inUV[];
layout (location = 3) flat in uint material_tex_In[];
layout (location = 4) flat in uint material_buff_In[];



layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outFragPos;
layout (location = 2) out vec2 outUV;
layout (location = 3) flat out uint material_tex_out;
layout (location = 4) flat out uint material_buff_out;

void main()
{
    
    int idx = getDominantAxisIdx(inFragPos[0].xyz, inFragPos[1].xyz, inFragPos[2].xyz);
	//gl_ViewportIndex = idx;
	
	for (int i = 0; i < 3; ++i)
    {
        vec3 worldPosition = gl_in[i].gl_Position.xyz;
        outFragPos = worldPosition;
        
        //gl_Position = matrixData.viewProj[idx] * gl_in[i].gl_Position;
        if(idx == 0)
            gl_Position = vec4(worldPosition.y,worldPosition.z, 1.0, 1);
        else if(idx == 1)
            gl_Position = vec4(worldPosition.x,worldPosition.z, 1.0, 1);
        else
            gl_Position = vec4(worldPosition.x,worldPosition.y, 1.0, 1);

		
        outNormal = inNormal[i];
        material_tex_out = material_tex_In[i];
        material_buff_out = material_buff_In[i];
        outUV = inUV[i];
        EmitVertex();
    }

    EndPrimitive();
}
