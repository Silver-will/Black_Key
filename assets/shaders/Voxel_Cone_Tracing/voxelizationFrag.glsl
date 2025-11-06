#ifndef VOXELIZATION_FRAG_GLSL
#define VOXELIZATION_FRAG_GLSL
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require

#include "conversion.glsl"

struct VoxelData{
	float r;
	float g;
	float b;
	uint voxel_num;
};

ivec3 ComputeImageCoords(vec3 posW, ivec3 image_dim)
{
	float voxel_size = 1 /128;
	float c = voxel_size * 0.25;
	vec3 region_min = vec3(-15);
	vec3 region_max = vec3(15);
	posW = clamp(posW, region_min + c, region_max - c);

	vec3 clipCoords = transformPosWToClipUVW(posW, region_max - region_min);

	return ivec3(clipCoords * image_dim);
}

ivec3 ComputeVoxelizationCoordinate(vec3 posW, ivec3 image_dim)
{
	vec3 voxel = 0.5f * posW + vec3(0.5f);
	return ivec3(image_dim * voxel);
	//posW = clamp(posW, vec3(-64), vec3(64));
	vec3 clipCoords = transformPosWToClipUVW(posW, 128);

	vec3 region_max = vec3(16);
	vec3 region_min = vec3(-16);
	vec3 texCoord =  (posW - region_min)  / (region_max - region_min);
	//texCoord *= 128;
	ivec3 voxelCoord = ivec3(floor(texCoord * image_dim));
	//return voxelCoord;
	//return ivec3(clipCoords * 128);
	// Calculate 3D texture co-ordinates.
	//return ivec3(posW * image_dim);
}

bool isInsideCube(const vec3 p, float e) { return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e; }


bool failsPreConditions(vec3 posW)
{
	//return isInsideDownsampleRegion(posW) || isOutsideVoxelizationRegion(posW);
	return isInsideCube(posW, 0);
}