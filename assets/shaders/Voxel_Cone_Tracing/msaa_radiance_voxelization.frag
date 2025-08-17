#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_atomic_float : require

#include "voxelizationFrag.glsl"
#include "../brdf.glsl"


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
        //storeVoxelColorAtomicRGBA8Avg6Faces(voxel_radiance, posW, emission);
        emission.a = 1;
        imageAtomicAdd(voxel_radiance, posW, emission);
    }
    else
    {
        //Sample diffuse texture
        vec4 color = vec4(0.0);
        
        //Clipmap specific texture sampling
        //lod = log2(float(textureSize(material_textures[nonuniformEXT(materialIn)], 0).x) / voxelConfigData.clipmapResolution);
        //color.rgb += textureLod(material_textures[nonuniformEXT(materialIn)], In.uv, lod).rgb;
        color.rgb += texture(material_textures[nonuniformEXT(materialIn)], In.uv).rgb;

        //Sample metallic texture
        vec2 metallicRough = texture(material_textures[nonuniformEXT(materialIn + 1)], In.uv).gb;
        
        vec3 N = normalize(In.normalW);

        vec3 lightContribution = vec3(0.0);
        vec3 L = normalize(-sceneData.sunlightDirection.xyz);
	    vec3 V = normalize(vec3(sceneData.cameraPos.xyz) - inFragPos);
         
        vec3 R = reflect(-V,N);
	    vec3 H = normalize(V + L);
        //float NoV = abs(dot(N, V)) + 1e-5;
	    float NoL = clamp(dot(N, L), 0.0, 1.0);
	    float NoH = clamp(dot(N, H), 0.0, 1.0);
        float LoH = clamp(dot(L, H), 0.0, 1.0);

        float roughness = metal_rough.x;
        float metallic = metal_rough.y;
        float NoV;
        N = GetViewReflectedNormal(N, V,NoV);

        roughness = roughness;
        vec3 F0 = vec3(0.04); 
	    F0 = mix(F0, albedo, metallic);

        vec3 radiance = vec3(0.0f);
        lightContribution += CalculateDirectionalLightContribution(N,V,L,albedo,F0,metal_rough,shadow,NoV,NoH,NoL);

        
        if (all(equal(lightContribution, vec3(0.0))))
            discard;
        
		vec3 radiance = lightContribution * color.rgb * color.a;
        radiance = clamp(lightContribution, 0.0, 1.0);
		
		ivec3 faceIndices = computeVoxelFaceIndices(-normal);
        //storeVoxelColorAtomicRGBA8Avg(u_voxelRadiance, posW, vec4(radiance, 1.0), faceIndices, abs(normal));
	    imageAtomicAdd(voxel_radiance, posW, vec4(radiance,1.0));
    }
   
}
