#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
#include "voxelizatonGeom.glsl"

in Vertex
{
    vec3 normal;
    vec2 uv;
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
        EmitVertex();
    }

    EndPrimitive();
}
