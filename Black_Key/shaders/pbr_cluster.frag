#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"
#include "lights.glsl"

layout(early_fragment_tests) in;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inFragPos;
layout (location = 3) in vec3 inViewPos;
layout (location = 4) in vec3 inPos;
layout (location = 5) in vec2 inUV;
layout (location = 6) in vec4 inTangent;
layout (location = 7) in mat3 inTBN;

layout(set = 0, binding = 2) uniform sampler2DArray shadowMap;
layout(set = 0, binding = 3) uniform samplerCube irradianceMap;
layout(set = 0, binding = 4) uniform sampler2D BRDFLUT;
layout(set = 0, binding = 5) uniform samplerCube preFilterMap;

#define NUM_OF_LIGHTS 4

layout (std430, set = 0, binding = 6) buffer screenToView{
    mat4 inverseProjection;
    uvec4 tileSizes;
    uvec2 screenDimensions;
    float scale;
    float bias;
};
layout (std430, set = 0, binding = 7) buffer lightSSBO{
    PointLight pointLight[];
};
layout (std430, set = 0, binding = 8) buffer lightIndexSSBO{
    uint globalLightIndexList[];
};
layout (std430, set = 0, binding = 9) buffer lightGridSSBO{
    LightGrid lightGrid[];
};

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;
#define CASCADE_COUNT 4 

float linearDepth(float depthSample);

vec3 CalculateNormalFromMap()
{
    vec3 tangentNormal = normalize(texture(normalTex,inUV).rgb * 2.0 - vec3(1.0));
    vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent.xyz);
	vec3 B = cross(N, T) * inTangent.w;
	mat3 TBN = mat3(T, B, N);
	return normalize(TBN * tangentNormal);
}

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float textureProj(vec4 shadowCoord, vec2 offset, int cascadeIndex)
{
	float shadow = 1.0;
	float bias = 0.005;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		float dist = texture(shadowMap, vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
			shadow = 0.3;
		}
	}
	return shadow;

}

float filterPCF(vec4 sc, int cascadeIndex)
{
	ivec2 texDim = textureSize(shadowMap, 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

void main() 
{
    vec4 colorVal = texture(colorTex, inUV).rgba;
    vec3 albedo =  pow(colorVal.rgb,vec3(2.2));
    float ao = colorVal.a;

    vec2 metallicRough  = texture(metalRoughTex, inUV).gb;
    float roughness = metallicRough.x;
    float metallic = metallicRough.y;
    
    vec3 N = CalculateNormalFromMap();
	
    vec3 V = normalize(vec3(sceneData.cameraPos.xyz) - inFragPos);
    vec3 R = reflect(-V,N);

    vec3 F0 = vec3(0.04); 
	F0 = mix(F0, albedo, metallic);

	vec3 Lo = vec3(0.0);
    vec3 L = normalize(-sceneData.sunlightDirection.xyz);
    
    Lo += specularContribution(L, V, N, F0, metallic, roughness);

    vec2 brdf = texture(BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 reflection = prefilteredReflection(R, roughness).rgb;	
	vec3 irradiance = texture(irradianceMap, N).rgb;

    vec3 diffuse = irradiance * albedo;

    vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

	// Specular reflectance
	vec3 specular = reflection * (F * brdf.x + brdf.y);

	// Ambient part
	vec3 kD = 1.0 - F;
	kD *= 1.0 - metallic;	  
	vec3 ambient = (kD * diffuse + specular);
	
	vec3 color = ambient + Lo;

    vec4 fragPosViewSpace = sceneData.view * vec4(inFragPos,1.0f);
    //float depthValue = inViewPos.z;
    float depthValue = fragPosViewSpace.z;
    int layer = 0;
	for(int i = 0; i < 4 - 1; ++i) {
		if(depthValue < sceneData.distances[i]) {	
			layer = i + 1;
		}
	}

    vec4 shadowCoord = (biasMat * sceneData.lightMatrices[layer]) * vec4(inFragPos, 1.0);	

    float shadow = filterPCF(shadowCoord/shadowCoord.w,layer);
    color *= shadow;
    
    if(sceneData.cascadeConfigData.z == 1.0f)
    {
        switch(layer){
            case 0 : 
				color.rgb *= vec3(1.0f, 0.25f, 0.25f);
				break;
			case 1 : 
				color.rgb *= vec3(0.25f, 1.0f, 0.25f);
				break;
			case 2 : 
				color.rgb *= vec3(0.25f, 0.25f, 1.0f);
				break;
			case 3 : 
				color.rgb *= vec3(1.0f, 1.0f, 0.25f);
				break;
        }

    }
    outFragColor = vec4(color, 1.0);  
}

float linearDepth(float depthSample){
	float zNear = sceneData.cascadeConfigData.x;
	float zFar  = sceneData.cascadeConfigData.y;
    float depthRange = 2.0 * depthSample - 1.0;
    // Near... Far... wherever you are...
    float linear = 2.0 * zNear * zFar / (zFar + zNear - depthRange * (zFar - zNear));
    return linear;
}