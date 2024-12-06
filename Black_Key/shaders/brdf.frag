#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"
#include "helper_functions.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inFragPos;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec3 inPos;
layout (location = 5) in vec4 inViewPos;
layout (location = 6) in mat3 inTBN;

layout(set = 0, binding = 2) uniform sampler2DArray shadowMap;
layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;
#define CASCADE_COUNT 4 

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);


float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
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

float filterPCF(vec4 sc, uint cascadeIndex)
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

float TestShadow(vec4 shadowCoord, int layer)
{
    vec3 samplePos = shadowCoord.xyz / shadowCoord.w;
    samplePos.y = -samplePos.y;

    // set shadow to 1.0 if samplePos.z > 1.0 here

    // Remap xy to [0.0, 1.0]
    samplePos.xy = samplePos.xy * 0.5 + 0.5;

    float currentDepth = samplePos.z;
    float closestDepth = texture(shadowMap, vec3(samplePos.xy, layer)).r;

    float shadow = currentDepth - 0.005 > closestDepth ? 0.3 : 1.0;
    return shadow;
}

float ShadowCalculation(vec3 fragPosWorldSpace)
{
    //select cascade layer
    vec4 fragPosViewSpace = sceneData.view * vec4(fragPosWorldSpace, 1.0);
    float depthValue = abs(fragPosViewSpace.z);
    int layer = -1;
    for (int i = 0; i < CASCADE_COUNT; ++i)
    {
        if (depthValue < sceneData.distances[i])
        {
            layer = i;
            break;
        }
    }
    if(layer == -1)
    {
       layer == CASCADE_COUNT;
    }

    vec4 fragPosLightSpace = sceneData.lightMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    //projCoords.y *= -1;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;

    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if (currentDepth > 1.0)
    {
        return 1.0;
    }

    // calculate bias (based on depth map resolution and slope)
    vec3 normal = normalize(inNormal);
    float bias = max(0.05 * (1.0 - dot(normal, sceneData.sunlightDirection.xyz)), 0.005);
    const float biasModifier = 0.5f;

    if (layer == CASCADE_COUNT)
    {
        bias *= 1 / (sceneData.cascadeConfigData.y * biasModifier);
    }
    else
    {
        bias *= 1 / (sceneData.cascadeDistances[layer] * biasModifier);
    }

    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, layer)).r;
            shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    //float shadow = texture(shadowMap, vec3(projCoords.xy, layer)).r;
    return shadow;
}

vec3 CalculateNormalFromMap()
{
    vec3 tangentNormal = texture(normalTex,inUV).rgb * 2.0 - 1.0;
    return normalize(inTBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
    vec3 L = normalize(-sceneData.sunlightDirection.xyz);
    vec3 H = normalize(V + L);
    vec3 radiance = sceneData.sunlightColor.xyz;

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    //reflectance equation
    vec3 Lo = vec3(0.0);

    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;
        
    // kS is equal to Fresnel
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;	 

    float NdotL = max(dot(N, L), 0.0);        

    // add to outgoing radiance Lo
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * albedo;
    
    vec3 color = ambient + Lo;

    float depthValue = inViewPos.z;
    int layer = 0;
	for(int i = 0; i < 4 - 1; ++i) {
		if(inViewPos.z < sceneData.cascadeDistances[i]) {	
			layer = i + 1;
		}
	}
    //if(layer == -1)
    //{
      //  layer = CASCADE_COUNT;
    //}
   
    //vec4 shadowCoord = (biasMat * sceneData.lightMatrices[3]) * vec4(inPos,1.0f);
    //float shadow = ShadowCalculation(inFragPos);
    //float shadow = filterPCF(shadowCoord/shadowCoord.w, layer);
    //float shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0), layer);

    vec4 shadowCoord = (biasMat * sceneData.lightMatrices[layer]) * vec4(inPos,1.0f);
    float shadow = TestShadow(shadowCoord, layer);
    //float shadow = 1.0f;
    color = neutral(color);
    color *= (shadow);
    
    // HDR tonemapping
    //color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));
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

