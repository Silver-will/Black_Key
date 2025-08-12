#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
#include "voxelizatonGeom.glsl"

layout(location = 1) flat out uint materialIn;
layout(location = 2) flat out uint matBufIn;
in Vertex
{
    vec3 normal;
	uint material_texture_index;
	vec2 uv;
	uint material_buffer_index;
} In[3];

out Geometry
{
    vec3 normalW;
    vec2 uv;
} Out;

void main()
{
    int idx = getDominantAxisIdx(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);
	gl_ViewportIndex = idx;
	
	for (int i = 0; i < 3; ++i)
    {
        gl_Position = matrixData.viewProj[idx] * gl_in[i].gl_Position;
        Out.uv = In[i].uv;
		Out.posW = gl_in[i].gl_Position.xyz;
        Out.normalW = In[i].normal;
        materialIn = In[i].material_texure_index;
        matBufIn = In[i].material_buffer_index;
        EmitVertex();
    }

    EndPrimitive();
}
