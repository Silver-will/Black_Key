#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

#include "resource.glsl"
layout(early_fragment_tests) in;

layout (location = 0) in vec3 inNormal;
layout (location = 1) flat in uint inMaterialIndex;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inFragPos;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) in vec3 inPos;
layout (location = 6) in vec2 inUV;
layout (location = 7) in vec4 inTangent;
layout (location = 8) in mat3 inTBN;


layout (location = 0) out vec4 outFragColor;
//vec3 Radiance(vec3 albedo, vec3 N, vec3 V, vec3 F0, float metallic, float roughness, float alphaRoughness, LightData light);
float linearDepth(float depthSample);
vec3 prefilteredReflection(vec3 R, float roughness);
vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 C, vec3 F0, float metallic, float roughness);
vec3 CalcDiffuseContribution(vec3 W, vec3 N, PointLight light);
vec3 PointLightContribution(vec3 W, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, PointLight light);
vec3 CalculateNormalFromMap();
float textureProj(vec4 shadowCoord, vec2 offset, int cascadeIndex);
float filterPCF(vec4 sc, int cascadeIndex);

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

const vec4 colors[] = vec4[](
vec4(1,0,0,1),
vec4(0,1,0,1),
vec4(0,0,1,1),
vec4(1,1,0,1),
vec4(1,0,1,1)
);


layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
	uint inMaterialIndex;
} PushConstants;


void main() 
{
	float linDepth = linearDepth(gl_FragCoord.z);
	uint zTile = uint(max(log2(linDepth) * scale + bias, 0.0));
	float sliceCountX = tileSizes.x;
	float sliceCountY = tileSizes.y;
	vec2 tileSize =
		vec2(screenDimensions.x / sliceCountX,
			 screenDimensions.y / sliceCountY);
	uvec3 cluster = uvec3(
		gl_FragCoord.x / tileSize.x,
		gl_FragCoord.y / tileSize.y,
		zTile);
	uint clusterIdx =
		cluster.x +
		cluster.y * int(tileSizes.x) +
		cluster.z * int(tileSizes.x) * int(tileSizes.y);
	uint lightCount = lightGrid[clusterIdx].count;
	uint lightIndexOffset = lightGrid[clusterIdx].offset;

    //vec4 colorVal = texture(colorTex, inUV).rgba;
    vec4 colorVal = texture(material_textures[nonuniformEXT(inMaterialIndex)],inUV).rgba;
	
    vec3 albedo =  pow(colorVal.rgb,vec3(2.2));
    float ao = colorVal.a;

    vec2 metallicRough  = texture(material_textures[nonuniformEXT(inMaterialIndex+1)],inUV).gb;
    //vec2 metallicRough  = texture(metalRoughTex, inUV).gb;
    
	float roughness = metallicRough.x;
    float metallic = metallicRough.y;
    
    vec3 N = CalculateNormalFromMap();
	
    vec3 V = normalize(vec3(sceneData.cameraPos.xyz) - inFragPos);
    vec3 R = reflect(-V,N);

    vec3 F0 = vec3(0.04); 
	F0 = mix(F0, albedo, metallic);

	vec3 Lo = vec3(0.0);
    vec3 L = normalize(-sceneData.sunlightDirection.xyz);
	vec3 Ld = vec3(1.0);
    
    Lo += specularContribution(L, V, N,sceneData.sunlightColor.xyz, F0, metallic, roughness);

	//Calculate point lights
	for(int i = 0; i < lightCount; i++)
	{
		uint lightVectorIndex = globalLightIndexList[lightIndexOffset + i];
		PointLight light = pointLight[lightVectorIndex];
		Lo += PointLightContribution(inFragPos, V, N, F0, metallic, roughness,light);
		Ld += CalcDiffuseContribution(inFragPos,N,light);
	}

    vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);
    vec2 brdf = texture(BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 reflection = prefilteredReflection(R, roughness).rgb;	
	vec3 irradiance = texture(irradianceMap, N).rgb;

    vec3 diffuse = irradiance * albedo * Ld;

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
	int blend = 0;
    int layer = 0;
	for(int i = 0; i < 4 - 1; ++i) {
		if(depthValue < sceneData.distances[i]) {	
			layer = i + 1;

		}
	}

    vec4 shadowCoord = (biasMat * shadowData.shadowMatrices[layer]) * vec4(inFragPos, 1.0);	

    float shadow = filterPCF(shadowCoord/shadowCoord.w,layer);
    //float shadow = textureProj(shadowCoord/shadowCoord.w, vec2(0.0), layer);
	color *= shadow;

	if(sceneData.debugInfo.x == 1.0f)
	{
		float r_off = float(lightCount) * 0.1;
		color.r += r_off;
	}

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
    outFragColor = vec4(color,1.0);  
	//uint color_index = cluster.z % 5;
	//outFragColor = colors[color_index];
    
}

float linearDepth(float depthSample){
	float zNear = sceneData.cascadeConfigData.x;
	float zFar  = sceneData.cascadeConfigData.y;
    //float depthRange = 2.0 * depthSample - 1.0;
    float linear = zNear * zFar / (zFar +  depthSample * (zNear - zFar));
    return linear;
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

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 C, vec3 F0, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	// Light color fixed
	//vec3 lightColor = vec3(1.0);

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
		color += (kD * pow(texture(material_textures[nonuniformEXT(inMaterialIndex)],inUV).rgb,vec3(2.2)) / PI + spec) * dotNL;
		color *= C;
	}

	return color;
}

vec3 CalcDiffuseContribution(vec3 W, vec3 N, PointLight light)
{
	 vec3 L = light.position.xyz - W;
	 float distance = length(L);
	 L = normalize(L);
	 float diff = max(dot(N, L), 0.0);
	 float attenuation = max(1.0 - (distance / light.range), 0.0) / pow(distance, 1.0f);
	 vec3 diffuse = light.color.xyz * diff * attenuation;
	 return diffuse;
}

vec3 PointLightContribution(vec3 W, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, PointLight light)
{
	vec3 L = light.position.xyz - W;
	float distance = length(L);
	L = normalize(L);
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	//float attenuation = 1.0/(distance * distance);
	float attenuation = max(1.0 - (distance / light.range), 0.0) / pow(distance, 1.0f);
	vec3 radiance = light.color.xyz * attenuation;

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
		color += (kD * pow(texture(material_textures[nonuniformEXT(inMaterialIndex)],inUV).rgb,vec3(2.2)) / PI + spec) * radiance * dotNL;
	}

	return color;
}

vec3 CalculateNormalFromMap()
{
    vec3 tangentNormal = normalize(texture(material_textures[nonuniformEXT(inMaterialIndex+2)],inUV).rgb * 2.0 - vec3(1.0));
    vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent.xyz);
	vec3 B = cross(N, T) * inTangent.w;
	mat3 TBN = mat3(T, B, N);
	return normalize(TBN * tangentNormal);
}

float textureProj(vec4 shadowCoord, vec2 offset, int cascadeIndex)
{
	float shadow = 1.0;
	float bias = 0.005;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		float dist = texture(shadowMap, vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
			shadow = 0.05;
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
