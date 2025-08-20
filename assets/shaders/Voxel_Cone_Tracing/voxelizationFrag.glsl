#ifndef VOXELIZATION_FRAG_GLSL
#define VOXELIZATION_FRAG_GLSL


#extension GL_GOOGLE_include_directive : require

ivec3 ComputeVoxelizationCoordinate(vec3 posW, ivec3 image_dim)
{
	// Output lighting to 3D texture.
	vec3 voxel = 0.5f * posW + vec3(0.5f);
	return ivec3(image_dim * voxel);
	//vec4 res = alpha * vec4(vec3(color), 1);
}

bool isInsideCube(const vec3 p, float e) { return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e; }


bool failsPreConditions(vec3 posW)
{
	//return isInsideDownsampleRegion(posW) || isOutsideVoxelizationRegion(posW);
	return isInsideCube(posW, 0);
}
