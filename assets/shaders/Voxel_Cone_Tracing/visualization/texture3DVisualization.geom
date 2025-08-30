#version 460

#extension GL_EXT_debug_printf : require

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;


layout(location = 0) out vec4 outColor;

#include "visualization.glsl"

const float EPSILON = 0.00001;

vec3 toWorld(vec3 p, vec3 offset)
{
	return (p + offset);
}

void createQuad(vec4 v0, vec4 v1, vec4 v2, vec4 v3)
{
	gl_Position = v0;
	//outUV = vec2(0.0, 0.0);
	EmitVertex();
	gl_Position = v1;
	//outUV = vec2(0.0, 1.0);
	EmitVertex();
	gl_Position = v3;
	//outUV = vec2(1.0, 0.0);
	EmitVertex();
	gl_Position = v2;
	//outUV = vec2(1.0, 1.0);
	EmitVertex();
	EndPrimitive();
}

void main()
{
	vec3 pos = gl_in[0].gl_Position.xyz;
	ivec3 samplePos = ivec3(pos);
	
	//vec4 colors[6] = vec4[](vec4(0.0), vec4(0.0), vec4(0.0), vec4(0.0), vec4(0.0), vec4(0.0));
	vec4 color = imageLoad(voxel_radiance, samplePos);

	//for (int i = 0; i < u_faceCount; ++i)
	//{
		//colors[i] = texelFetch(voxel_radiance, samplePos + ivec3(u_resolution.x * i, 0, 0), 0);
	//}
	
	vec4 v0 = tex_3d_data.viewproj * vec4(toWorld(pos, vec3(0.0)), 1.0);
	vec4 v1 = tex_3d_data.viewproj * vec4(toWorld(pos, vec3(1, 0, 0)), 1.0);
	vec4 v2 = tex_3d_data.viewproj * vec4(toWorld(pos, vec3(0, 1, 0)), 1.0);
	vec4 v3 = tex_3d_data.viewproj * vec4(toWorld(pos, vec3(1, 1, 0)), 1.0);
	vec4 v4 = tex_3d_data.viewproj * vec4(toWorld(pos, vec3(0, 0, 1)), 1.0);
	vec4 v5 = tex_3d_data.viewproj * vec4(toWorld(pos, vec3(1, 0, 1)), 1.0);
	vec4 v6 = tex_3d_data.viewproj * vec4(toWorld(pos, vec3(0, 1, 1)), 1.0);
	vec4 v7 = tex_3d_data.viewproj * vec4(toWorld(pos, vec3(1, 1, 1)), 1.0);
	
	
	//Create 6 quads modelling a cube
	debugPrintfEXT("Texture 3D color = %v4f", color);
	
	if(color != vec4(0,0,0,1))
	{
		outColor = color;
		gl_Position = v4;
		EmitVertex();
		gl_Position = v1;
		EmitVertex();
		gl_Position = v2;
		EmitVertex();
		EndPrimitive();
	}
	//createQuad(v0, v2, v6, v4);
	//createQuad(v1, v5, v7, v3);
	//createQuad(v0, v4, v5, v1);
	//createQuad(v2, v3, v7, v6);
	//createQuad(v0, v1, v3, v2);
	//createQuad(v4, v6, v7, v5);
}
