#ifndef VOXELIZATION_GLSL
#define VOXELIZATION_GLSL

#extension GL_GOOGLE_include_directive : require

#include "common.glsl"
#include "intersection.glsl"
#include "conversion.glsl"
#include "atomicOperations.glsl"
#include "settings.glsl"

layout(set = 0, binding = 0) uniform  VoxelConfigurationData{   
	vec3 regionMin;
	uint clipmapLevel;
	vec3 regionMax;
	uint clipmapResolution;
	vec3 prevRegionMin;
	uint clipMapResolutionWithBorder;
	vec3 prevRegionMax;
	float downsampleTransitionRegionSize;
	float maxExtent;
	float voxelSize;
} voxelConfigData;


ivec3 computeImageCoords(vec3 posW)
{
	// Avoid floating point imprecision issues by clamping to narrowed bounds
	float c = u_voxelSize * 0.25; // Error correction constant
	posW = clamp(posW, u_regionMin + c, u_regionMax - c);
	
	vec3 clipCoords = transformPosWToClipUVW(posW, u_maxExtent);

    // The & (u_clipmapResolution - 1) (aka % u_clipmapResolution) is important here because
    // clipCoords can be in [0,1] and thus cause problems at the border (value of 1) of the physical
    // clipmap since the computed value would be 1 * u_clipmapResolution and thus out of bounds.
    // The reason is that in transformPosWToClipUVW the frac() operation is used and due to floating point
    // precision limitations the operation can return 1 instead of the mathematically correct fraction.
	ivec3 imageCoords = ivec3(clipCoords * u_clipmapResolution) & (u_clipmapResolution - 1);
    
#ifdef VOXEL_TEXTURE_WITH_BORDER
	imageCoords += ivec3(BORDER_WIDTH);
#endif

	// Target the correct clipmap level
	imageCoords.y += u_clipmapResolutionWithBorder * u_clipmapLevel;
	
	return imageCoords;
}

void storeVoxelColorAtomicRGBA8Avg6Faces(layout(r32ui) volatile uimage3D image, vec3 posW, vec4 color)
{
	ivec3 imageCoords = computeImageCoords(posW);
	
	for (int i = 0; i < 6; ++i)
		imageAtomicRGBA8Avg(image, ivec3(imageCoords + vec3(u_clipmapResolutionWithBorder * i, 0, 0)), color);
}

void storeVoxelColorR32UIRGBA8(layout(r32ui) uimage3D image, vec3 posW, vec4 color)
{
	ivec3 imageCoords = computeImageCoords(posW);
	
	for (int i = 0; i < 6; ++i)	
		imageStore(image, imageCoords + ivec3(u_clipmapResolutionWithBorder * i, 0, 0), uvec4(convertVec4ToRGBA8(color * 255)));
}

void storeVoxelColorRGBA8(layout(rgba8ui) uimage3D image, vec3 posW, vec4 color)
{
	ivec3 imageCoords = computeImageCoords(posW);
	
	for (int i = 0; i < 6; ++i)
		imageStore(image, imageCoords + ivec3(u_clipmapResolutionWithBorder * i, 0, 0), uvec4(color * 255));
}

void storeVoxelColorRGBA8(layout(rgba8) writeonly image3D image, vec3 posW, vec4 color)
{
	ivec3 imageCoords = computeImageCoords(posW);
	
	for (int i = 0; i < 6; ++i)
		imageStore(image, imageCoords + ivec3(u_clipmapResolutionWithBorder * i, 0, 0), color);
}

void storeVoxelColorAtomicRGBA8Avg(layout(r32ui) volatile uimage3D image, vec3 posW, vec4 color, ivec3 faceIndices, vec3 weight) // layout(r32ui) volatile coherent 
{
	ivec3 imageCoords = computeImageCoords(posW);
	
	imageAtomicRGBA8Avg(image, imageCoords + ivec3(faceIndices.x * u_clipmapResolutionWithBorder, 0, 0), vec4(color.rgb * weight.x, 1.0));
	imageAtomicRGBA8Avg(image, imageCoords + ivec3(faceIndices.y * u_clipmapResolutionWithBorder, 0, 0), vec4(color.rgb * weight.y, 1.0));
	imageAtomicRGBA8Avg(image, imageCoords + ivec3(faceIndices.z * u_clipmapResolutionWithBorder, 0, 0), vec4(color.rgb * weight.z, 1.0));
}

//void storeVoxelColorRGBA8(uimage3D image, vec3 posW, vec4 color) // layout(rgba8ui) volatile coherent 
//{
//	ivec3 imageCoords = computeImageCoords(posW);
//	
//	for (int i = 0; i < 6; ++i)
//		imageStore(image, imageCoords + ivec3(u_clipmapResolutionWithBorder * i, 0, 0), uvec4(color * 255));
//}

#endif