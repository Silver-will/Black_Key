#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_atomic_float : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : require


#include "voxelizationFrag.glsl"
#include "../brdf.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inFragPos;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inNormPos;
layout (location = 4) flat in uint materialIn;
layout (location = 5) flat in uint matBufIn;


layout(set = 0, binding = 3, r32ui) uniform volatile uimage3D voxel_radiance;

const float voxel_size = 15.0f;
const float voxel_resolution = 128.0f;

void imageAtomicRGBA8Avg(ivec3 coords, vec4 value);

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

vec3 CalculateLightContribution(vec3 posW,vec3 L, vec3 V, vec3 N, float shadow)
{
    float nDotL = clamp(dot(N, L), 0.01, 1.0);

    //ToDo add point light contribution to GI

    vec3 lightContribution = vec3(0);
    
    lightContribution += sceneData.sunlightColor.rgb * sceneData.sunlightDirection.a * nDotL * shadow;

    return lightContribution;
}

void main()
{
   vec3 posW = inFragPos;
    //if(failsPreConditions(posW))
      //  discard;

    float lod;

    ivec3 im_size = imageSize(voxel_radiance);
    
    //Grab current objects material information
    GLTFMaterialData mat_data = object_material_description.material_data[matBufIn];
    //Store emissive component within voxel structure
    if(mat_data.has_emission == 1 || mat_data.emission_color != vec4(0.0))
    {
        vec4 emission = mat_data.emission_color;

        //lod = log2(float(textureSize(material_textures[nonuniformEXT(materialIn + 4)], 0).x) / voxelConfigData.clipmapResolution);
        //emission.rgb += textureLod(material_textures[nonuniformEXT(materialIn + 4)], inUV, lod).rgb;
        emission.rgb += texture(material_textures[nonuniformEXT(materialIn + 4)], inUV).rgb;
        
        
        emission.rgb = clamp(emission.rgb, 0.0, 1.0);
        //storeVoxelColorAtomicRGBA8Avg6Faces(voxel_radiance, posW, emission);
        emission.a = 1;

        ivec3 coords = ComputeVoxelizationCoordinate(inNormPos, im_size);
        
        imageAtomicRGBA8Avg(coords, emission);
       
    }
    else
    {
        //Sample diffuse texture
        vec4 color = vec4(0.0);
        
        // Clipmap specific texture sampling
        color.rgba += texture(material_textures[nonuniformEXT(materialIn)], inUV).rgba;
        //color.a = 1.0f;
        vec3 N = normalize(inNormal);
        vec3 lightContribution = vec3(0.0);

        vec3 L = normalize(-sceneData.sunlightDirection.xyz);
	    vec3 V = normalize(vec3(sceneData.cameraPos.xyz) - posW);
         
        //Evaluate shadow term
	    vec4 fragPosViewSpace = sceneData.view * vec4(posW,1.0f);
        float depthValue = fragPosViewSpace.z;
        int layer = 0;
	    for(int i = 0; i < 4 - 1; ++i) {
		    if(depthValue < sceneData.distances[i]) {	
		    	layer = i + 1;
	    	}
	    }  
        layer = 1;
        vec4 shadowCoord = (biasMat * shadowData.shadowMatrices[layer]) * vec4(posW, 1.0);		
        float shadow = filterPCF(shadowCoord/shadowCoord.w,layer);
        
        lightContribution += CalculateLightContribution(posW,L, V, N, shadow);
        
        //debugPrintfEXT("View space position = %v4f", fragPosViewSpace);
        
        if (layer != 0)
        {
            //debugPrintfEXT("view space depth = %f", depthValue);
        }

        
		vec3 radiance = lightContribution * color.rgb * color.a; 
        radiance = clamp(lightContribution, 0.0, 1.0);
		
        ivec3 coords = ComputeVoxelizationCoordinate(inNormPos, im_size);
        
        //ivec3 coords = ComputeImageCoords(inFragPos, im_size);
        if(any(greaterThan(coords,vec3(127))))
           discard;

        if(any(lessThan(coords,vec3(0))))
           discard;

        imageAtomicRGBA8Avg(coords, vec4(color.rgb * shadow,1.0));
    }
}



void imageAtomicRGBA8Avg(ivec3 coords, vec4 value)
{
    value.rgb *= 255.0;                 // optimize following calculations
    uint newVal = convertVec4ToRGBA8(value);
    uint prevStoredVal = 0;
    uint curStoredVal;
	int i = 0;
	const int maxIterations = 100;

    while((curStoredVal = imageAtomicCompSwap(voxel_radiance, coords, prevStoredVal, newVal)) != prevStoredVal)
    {
        prevStoredVal = curStoredVal;
        vec4 rval = convertRGBA8ToVec4(curStoredVal);        
        rval.rgb = (rval.rgb * rval.a); // Denormalize
        vec4 curValF = rval + value;    // Add
        curValF.rgb /= curValF.a;       // Renormalize
        newVal = convertVec4ToRGBA8(curValF);
		++i;
    }
}

#endif