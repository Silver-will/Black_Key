#version 460 core
#include "voxelizationFrag.glsl"

layout(set = 0, binding = 2) uimage3D voxel_radiance;
in Geometry
{
	vec3 posW;
    vec3 normalW;
    vec2 uv;
} In;

void main()
{
    vec3 posW = In.posW;
    if(failsPreConditions(posW)
    {
        discard;
    }

    float lod;
}
