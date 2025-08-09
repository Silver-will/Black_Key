#ifndef VOXELIZATION_GEOM_GLSL
#define VOXELIZATION_GEOM_GLSL

#extension GL_GOOGLE_include_directive : require

#include "/voxelConeTracing/voxelization.glsl"

layout(set = 0, binding = 0) uniform  MatrixData{   
	mat4 viewProj[3];
	mat4 viewProjInv[3];
	vec2 viewPortSizes[3];
} matrixData;



#ifdef CONSERVATIVE_VOXELIZATION
out ConservativeVoxelizationFragmentInput
{
	vec3 posW;
    vec3 posClip;
    flat vec4 triangleAABB;
	flat vec3[3] trianglePosW;
	flat int faceIdx;
} out_Frag;


void RasterizeToMostVisibleAxis(out vec4 positionsClip[3])
{
	int idx = getDominantAxisIdx(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);
	gl_ViewportIndex = idx;
    
    positionsClip = vec4[3](
        matrixData.viewProj[idx] * gl_in[0].gl_Position,
        matrixData.viewProj[idx] * gl_in[1].gl_Position,
        matrixData.viewProj[idx] * gl_in[2].gl_Position
    );

    vec2 pixel_size = 1.0 / matrixData.viewportSizes[idx];
	vec3 triangleNormalClip = normalize(cross(positionsClip[1].xyz - positionsClip[0].xyz, positionsClip[2].xyz - positionsClip[0].xyz));

	out_Frag.faceIdx = idx * 2;
	if (triangleNormalClip.z > 0.0)
		out_Frag.faceIdx += 1;
	
	// Using the original triangle for the intersection tests introduces a slight underestimation
	out_Frag.trianglePosW[0] = gl_in[0].gl_Position.xyz;
	out_Frag.trianglePosW[1] = gl_in[1].gl_Position.xyz;
	out_Frag.trianglePosW[2] = gl_in[2].gl_Position.xyz;
}

void cvEmitVertex(vec4 posClip)
{
	gl_Position = posClip;
	out_Frag.posW = (matrixData.viewProjInv[gl_ViewportIndex] * posClip).xyz;
	out_Frag.posClip = posClip.xyz;
	
	EmitVertex();
}
#endif // CONSERVATIVE_VOXELIZATION

#endif // VOXELIZATION_GEOM_GLSL