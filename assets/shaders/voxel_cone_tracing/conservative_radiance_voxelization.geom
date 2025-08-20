#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
#include "common.glsl"


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
    int idx = getDominantAxisIdx(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);
	//gl_ViewportIndex = idx;
	
	for (int i = 0; i < 3; ++i)
    {
        //gl_Position = matrixData.viewProj[idx] * gl_in[i].gl_Position;
        if(idx == 0)
            gl_Position = vec4(gl_in[i].gl_Position.y,gl_in[i].gl_Position.z, 0, 1);
        else if(idx == 1)
            gl_Position = vec4(gl_in[i].gl_Position.x,gl_in[i].gl_Position.z, 0, 1);
        else
            gl_Position = vec4(gl_in[i].gl_Position.x,gl_in[i].gl_Position.y, 0, 1);

        outUV = inUV[i];
		outFragPos = gl_in[i].gl_Position.xyz;
        outNormal = inNormal[i];
        material_tex_out = material_tex_In[i];
        material_buff_out = material_buff_In[i];
        EmitVertex();
    }

    EndPrimitive();
}
