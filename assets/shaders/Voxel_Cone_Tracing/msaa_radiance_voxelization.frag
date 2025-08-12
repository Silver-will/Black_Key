#version 460 core
#include "voxelizationFrag.glsl"


layout(location = 1) flat in uint materialIn;
layout(location = 2) flat in uint matBufIn;

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
    
    //Grab current objects material information
    GLTFMaterialData mat_data = object_material_description.material_data[matBufIn];
    //Store emissive component within voxel structure
    if(mat_data.has_emission == 1 || mat_data.emission_color != vec4(0.0))
    {
        vec4 emission = mat_data.emission_color;

        lod = log2(float(textureSize(material_textures[nonuniformEXT(materialIn + 4)], 0).x) / voxelConfigData.clipmapResolution);
        emission.rgb += textureLod(material_textures[nonuniformEXT(materialIn + 4)], In.uv, lod).rgb;
        
        
        emission.rgb = clamp(emission.rgb, 0.0, 1.0);
        storeVoxelColorAtomicRGBA8Avg6Faces(voxel_radiance, posW, emission);
    }
    else
    {
        vec4 color = vec4(0.0);

        lod = log2(float(textureSize(material_textures[nonuniformEXT(materialIn)], 0).x) / voxelConfigData.clipmapResolution);
        color.rgb += textureLod(material_textures[nonuniformEXT(materialIn)], In.uv, lod).rgb;
        
        vec3 normal = normalize(In.normalW);


        vec3 lightContribution = vec3(0.0);
        for (int i = 0; i < u_numActiveDirLights; ++i)
        {
            float nDotL = max(0.0, dot(normal, -u_directionalLights[i].direction));
            
            float visibility = 1.0;
            if (u_directionalLightShadowDescs[i].enabled != 0)
            {
                visibility = computeVisibility(posW, u_shadowMaps[i], u_directionalLightShadowDescs[i], u_usePoissonFilter, u_depthBias);
            }
            
            lightContribution += nDotL * visibility * u_directionalLights[i].color * u_directionalLights[i].intensity;
        }
        
        if (all(equal(lightContribution, vec3(0.0))))
            discard;
        
		vec3 radiance = lightContribution * color.rgb * color.a;
        radiance = clamp(radiance, 0.0, 1.0);
		
		ivec3 faceIndices = computeVoxelFaceIndices(-normal);
        storeVoxelColorAtomicRGBA8Avg(u_voxelRadiance, posW, vec4(radiance, 1.0), faceIndices, abs(normal));
	
    }
   
}
