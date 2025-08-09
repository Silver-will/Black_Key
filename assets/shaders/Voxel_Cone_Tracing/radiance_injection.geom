#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;


in Vertex
{
    vec3 normalW;
    vec2 uv;
} In[3];

out Geometry
{
    vec3 normalW;
    vec2 uv;
} Out;

void main()
{
    vec4 positionsClip[3];
	RasterizeToMostVisibleAxis(positionsClip);
	
	for (int i = 0; i < 3; ++i)
    {
        Out.uv = In[i].uv;
        Out.normalW = In[i].normalW;
        
		EmitAndPassVertex(positionsClip[i]);
    }

    EndPrimitive();
}