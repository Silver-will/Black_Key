#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

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

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;
#define CASCADE_COUNT 4 

vec3 CalculateNormalFromMap()
{
    vec3 tangentNormal = texture(normalTex,inUV).rgb * 2.0 - 1.0;
    vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent.xyz);
	vec3 B = normalize(cross(N, T)) * inTangent.w;
	mat3 TBN = mat3(T, B, N);
	return normalize(TBN * tangentNormal);
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

float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(preFilterMap, R, lodf).rgb;
	vec3 b = textureLod(preFilterMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, F0);		
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);		
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);			
		color += (kD * pow(texture(colorTex, inUV).rgb,vec3(2.2)) / PI + spec) * dotNL;
	}

	return color;
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
    /*
    vec4 colorVal = texture(colorTex, inUV).rgba;
    vec3 albedo =  pow(colorVal.rgb,vec3(2.2));
    float ao = colorVal.a;

    vec2 metallicRough  = texture(metalRoughTex, inUV).gb;
    float roughness = metallicRough.x;
    float metallic = metallicRough.y;
    
    vec3 N = CalculateNormalFromMap();
    //N.y *= -1;
    
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
    vec3 diffuse = texture(irradianceMap,N).rgb;
    //vec3 diffuse = vec3(0.03);
    diffuse = vec3(1.0) - exp(-diffuse * 2.0f);
        
    // kS is equal to Fresnel
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;	 

    diffuse = diffuse * kD * albedo;

	float lightValue = max(dot(inNormal, vec3(0.3f,1.f,0.3f)), 0.1f);

    float NdotL = max(dot(N, L), 0.0);        

    // add to outgoing radiance Lo
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.01) + diffuse;
    
    vec3 color = (ambient + Lo) * ao;
    */

    vec4 colorVal = texture(colorTex, inUV).rgba;
    vec3 albedo =  pow(colorVal.rgb,vec3(2.2));
    float ao = colorVal.a;

    vec2 metallicRough  = texture(metalRoughTex, inUV).gb;
    float roughness = metallicRough.x;
    float metallic = metallicRough.y;
    
    vec3 N = CalculateNormalFromMap();
    //vec3 N = inNormal;

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
	//vec3 specular = reflection * (F * brdf.x + brdf.y);

	// Ambient part
	vec3 kD = 1.0 - F;
	kD *= 1.0 - metallic;	  
	vec3 ambient = (kD * diffuse);
	
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