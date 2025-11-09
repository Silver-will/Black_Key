#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : require


#include "../brdf.glsl"
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

#define TSQRT2 2.828427
#define SQRT2 1.414213
#define ISQRT2 0.707106
#define MIPMAP_HARDCAP 6.0f 

float linearDepth(float depthSample);
vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 C, vec3 F0, float metallic, float roughness);
vec3 CalcDiffuseContribution(vec3 W, vec3 N, PointLight light);
vec3 PointLightContribution(vec3 W, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, PointLight light);
vec3 CalculateNormalFromMap();
float textureProj(vec4 shadowCoord, vec2 offset, int cascadeIndex);
float filterPCF(vec4 sc, int cascadeIndex);
vec3 CastDiffuseCone(vec3 from, vec3 direction, float aperture);
vec3 CastSpecularCone(vec3 from, vec3 direction,float roughness);
float CastShadowCone();
vec4 CastCone(vec3 startPos, vec3 direction, float aperture, float maxDistance);

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
const float MAX_TRACE_DISTANCE = 20.0;
const float MIN_SPECULAR_APERTURE = 0.05;

#ifdef USE_32_CONES
// 32 Cones for higher quality (16 on average per hemisphere)
const int DIFFUSE_CONE_COUNT = 32;
const float DIFFUSE_CONE_APERTURE = 0.628319;

const vec3 DIFFUSE_CONE_DIRECTIONS[32] = {
    vec3(0.898904, 0.435512, 0.0479745),
    vec3(0.898904, -0.435512, -0.0479745),
    vec3(0.898904, 0.0479745, -0.435512),
    vec3(0.898904, -0.0479745, 0.435512),
    vec3(-0.898904, 0.435512, -0.0479745),
    vec3(-0.898904, -0.435512, 0.0479745),
    vec3(-0.898904, 0.0479745, 0.435512),
    vec3(-0.898904, -0.0479745, -0.435512),
    vec3(0.0479745, 0.898904, 0.435512),
    vec3(-0.0479745, 0.898904, -0.435512),
    vec3(-0.435512, 0.898904, 0.0479745),
    vec3(0.435512, 0.898904, -0.0479745),
    vec3(-0.0479745, -0.898904, 0.435512),
    vec3(0.0479745, -0.898904, -0.435512),
    vec3(0.435512, -0.898904, 0.0479745),
    vec3(-0.435512, -0.898904, -0.0479745),
    vec3(0.435512, 0.0479745, 0.898904),
    vec3(-0.435512, -0.0479745, 0.898904),
    vec3(0.0479745, -0.435512, 0.898904),
    vec3(-0.0479745, 0.435512, 0.898904),
    vec3(0.435512, -0.0479745, -0.898904),
    vec3(-0.435512, 0.0479745, -0.898904),
    vec3(0.0479745, 0.435512, -0.898904),
    vec3(-0.0479745, -0.435512, -0.898904),
    vec3(0.57735, 0.57735, 0.57735),
    vec3(0.57735, 0.57735, -0.57735),
    vec3(0.57735, -0.57735, 0.57735),
    vec3(0.57735, -0.57735, -0.57735),
    vec3(-0.57735, 0.57735, 0.57735),
    vec3(-0.57735, 0.57735, -0.57735),
    vec3(-0.57735, -0.57735, 0.57735),
    vec3(-0.57735, -0.57735, -0.57735)
};
#else // 16 cones for lower quality (8 on average per hemisphere)
const int DIFFUSE_CONE_COUNT = 16;
const float DIFFUSE_CONE_APERTURE = 0.872665;

const vec3 DIFFUSE_CONE_DIRECTIONS[16] = {
    vec3(0.57735, 0.57735, 0.57735),
    vec3(0.57735, -0.57735, -0.57735),
    vec3(-0.57735, 0.57735, -0.57735),
    vec3(-0.57735, -0.57735, 0.57735),
    vec3(-0.903007, -0.182696, -0.388844),
    vec3(-0.903007, 0.182696, 0.388844),
    vec3(0.903007, -0.182696, 0.388844),
    vec3(0.903007, 0.182696, -0.388844),
    vec3(-0.388844, -0.903007, -0.182696),
    vec3(0.388844, -0.903007, 0.182696),
    vec3(0.388844, 0.903007, -0.182696),
    vec3(-0.388844, 0.903007, 0.182696),
    vec3(-0.182696, -0.388844, -0.903007),
    vec3(0.182696, 0.388844, -0.903007),
    vec3(-0.182696, 0.388844, 0.903007),
    vec3(0.182696, -0.388844, 0.903007)
};
#endif

layout(set = 0, binding = 1) uniform  VXGIConfigData{   
	float indirectDiffuseIntensity;
	float indirectSpecularIntensity;
	float voxel_resolution;
	float ambientOcclusionFactor;
	float traceStartOffset;
	float voxel_size;
	float step_factor;
	float volumeCenter;
    vec4 region_min;
    vec4 region_max;
} vxgiConfigUB;

layout(set = 0, binding = 12) uniform sampler3D voxel_radiance;

const float VOXEL_SIZE = (1/128);
const float MIN_STEP_FACTOR = 0.2;

// Scales and bias a given vector (i.e. from [-1, 1] to [0, 1]).
vec3 scaleAndBias(const vec3 p) { return 0.5f * p + vec3(0.5f); }


vec3 worldToVoxelUV(vec3 p)
{
	vec3 bounds = vxgiConfigUB.region_max.xyz - vxgiConfigUB.region_min.xyz;
	float extent = max(bounds.x,max(bounds.y, bounds.z));

	//vec3 samplePos = fract(p / extent);
	//return samplePos;
    vec3 samplePos =  (p - vxgiConfigUB.region_min.xyz) / (vxgiConfigUB.region_max.xyz - vxgiConfigUB.region_min.xyz);
	return samplePos;

}

vec4 CastCone(vec3 startPos, vec3 direction, float aperture, float maxDistance)
{
	// Accumulated radiance and opacity
    vec4 dst = vec4(0.0);

    // Cone expansion coefficient
    float coneCoeff = 2.0 * tan(aperture * 0.5);

	float voxelSize = vxgiConfigUB.voxel_size;
	//voxelSize = 0.25;
    // Offset trace start to avoid sampling inside the surface
    startPos += direction * voxelSize * vxgiConfigUB.traceStartOffset;

	float s = 0.0;                   // ray distance along the cone
    float diameter = max(s * coneCoeff, voxelSize);      // initial cone diameter
    float occlusion = 0.0;           // accumulated opacity
    float lastS = 0.0;
	float curSegmentLength = voxelSize;
	float stepFactor = vxgiConfigUB.step_factor;
	
	vec4 color_ret = vec4(0);
	//debugPrintfEXT("Start trace pos = %v3f, voxelSize: %f", startPos, voxelSize);
    while (s < maxDistance && occlusion < 1.0)
    {
        // Current world position along cone
        vec3 position = startPos + direction * s;

        // Convert world coordinate → voxel texture coordinate (0–1)
        // You'll replace this with your own world→voxel transform.
		vec3 uvw = worldToVoxelUV(position);

		if(any(greaterThan(uvw,vec3(1))))
		{
			break;
		}
    
		float lod = log2(diameter / voxelSize);

        // Read radiance (RGBA)
        vec4 radiance = texture(voxel_radiance, uvw,  min(lod,MIPMAP_HARDCAP ));
        float opacity = radiance.a;
		
        // Compute radiance/opacity correction for cone step size
		float correction = curSegmentLength / voxelSize;

		float att = 1.0 / (1.0 + s * s * 0.1); 
        // Radiance grows proportionally to how many voxels you cross
        radiance.rgb *= correction * att;

        // Opacity corrected like Beer-Lambert
        opacity = clamp(1.0 - pow(1.0 - opacity, correction), 0.0, 1.0);

        vec4 src = vec4(radiance.rgb, opacity);
        // Front-to-back compositing
        dst += clamp((1.0 - dst.a),0.0,1.0) * src;

        // Occlusion accumulation (soft shadow behavior)
        occlusion += (1.0 - occlusion) * opacity / (1.0 + (s + voxelSize));

        // Advance along the cone
        lastS = s;

        // Step size grows with the cone diameter
       
		s += max(diameter, voxelSize) * stepFactor;

        // Cone diameter grows linearly with distance
        diameter = s * coneCoeff;
		curSegmentLength = s - lastS;
        
    }
	//return color_ret;
    return clamp(vec4(dst.rgb, 1.0 - occlusion), 0.0, 1.0);
}

vec3 CastDiffuseCone(vec3 from, vec3 direction, float aperture)
{
	/*
	direction = normalize(direction);
	
	const float CONE_SPREAD = 0.325;

	vec4 acc = vec4(0.0f);
	
	float dist = 0.1953125;

	float voxelSize = vxgiConfigUB.voxel_size;

	// Trace.
	while(dist < SQRT2 && acc.a < 1){
		vec3 c = from + dist * direction;
		c = worldToVoxelUV(c);
		c = 0.5f * c + vec3(0.5f);
		//c = scaleAndBias(from + dist * direction);
		float l = (1 + CONE_SPREAD * dist / voxelSize);
		float level = log2(l);
		float ll = (level + 1) * (level + 1);
		vec4 voxel = textureLod(voxel_radiance, c, min(MIPMAP_HARDCAP, level));
		acc += 0.075 * ll * voxel * pow(1 - voxel.a, 2);
		dist += ll * voxelSize * 2;
	}
	return pow(acc.rgb * 2.0, vec3(1.5));
	*/

	
	direction = normalize(direction);

	vec3 bounds = vxgiConfigUB.region_max.xyz - vxgiConfigUB.region_min.xyz;
	float voxelGridSize = max(bounds.x,max(bounds.y, bounds.z));
	
	float voxelSize = vxgiConfigUB.voxel_size;

    float distance = voxelSize;
    float occlusion = 0.0f;

    vec3 result = vec3(0.0f);

    float coneIncline = aperture;
	float maxDistance = MAX_TRACE_DISTANCE;
	//vec3 posW = 0.5f * from + vec3(0.5f);

	    while (occlusion < 1.0f && distance <= maxDistance) {
        vec3 currentPos = from + direction * distance;

        //vec3 uvw = (currentPos + voxelGridSize * 0.5) / voxelGridSize;
		vec3 uvw = worldToVoxelUV(currentPos);

		debugPrintfEXT("UVW = %v3f", uvw);

        if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0))))
            break;

        float diameter = distance * coneIncline;
        float lod = log2(diameter / voxelSize);

        vec4 voxelSample = textureLod(voxel_radiance, uvw, min(lod,MIPMAP_HARDCAP ));

		float att = 1.0 / (distance * distance);
		

		//result += voxelSample.rgb * att;
        if (distance > voxelSize * 2)
            result += (1.0 - occlusion) * voxelSample.rgb * voxelSample.a;
			occlusion += (1.0 - occlusion) *  voxelSample.a * 2;
        distance += max(diameter, voxelSize);
    }
    return result;
}


vec3 CastSpecularCone(vec3 from, vec3 direction, float roughness)
{
	direction = normalize(direction);

	float voxelSize = vxgiConfigUB.voxel_size;

	const float OFFSET = 2 * voxelSize;
	const float STEP = voxelSize;

	from += OFFSET * inNormal;
	
	vec4 acc = vec4(0.0f);
	float distance = OFFSET;
	float maxDistance = MAX_TRACE_DISTANCE;
	
	// Trace.
	while(distance < maxDistance && acc.a < 1){ 
		vec3 currentPos = from + direction * distance;

        //vec3 uvw = (currentPos + voxelGridSize * 0.5) / voxelGridSize;
		vec3 uvw = worldToVoxelUV(currentPos);

		
		float level = 0.1 * roughness * log2(1 + distance / voxelSize);
		vec4 voxel = textureLod(voxel_radiance, uvw, min(level, MIPMAP_HARDCAP));
		float f = 1 - acc.a;
		acc.rgb += 0.25 * (1 + roughness) * voxel.rgb * voxel.a * f;
		acc.a += 0.25 * voxel.a * f;
		distance += STEP * (1.0f + 0.125f * level);
	}
	return 1.0 * pow(roughness + 1, 0.8) * acc.rgb;
}

void main() 
{
	//Calculate current fragment cluster
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


	//Check material description for available texture data
	GLTFMaterialData mat_description = object_material_description.material_data[nonuniformEXT(inMaterialIndex / 5)];
	
	//PBR material values
    vec4 colorVal = texture(material_textures[nonuniformEXT(inMaterialIndex)],inUV).rgba;
    vec3 albedo =  pow(colorVal.rgb,vec3(2.2));
    vec2 metallicRough  = texture(material_textures[nonuniformEXT(inMaterialIndex+1)],inUV).gb;

	vec3 occlusion = texture(material_textures[nonuniformEXT(inMaterialIndex+3)],inUV).rgb;
	vec3 emission = texture(material_textures[nonuniformEXT(inMaterialIndex+4)],inUV).rgb;

	float roughness = metallicRough.x;
    float metallic = metallicRough.y;
	

    vec3 N = CalculateNormalFromMap();
    vec3 V = normalize(vec3(sceneData.cameraPos.xyz) - inFragPos);
	vec3 L = normalize(-sceneData.sunlightDirection.xyz);
	
	// Compute indirect contribution
    vec4 indirectContribution = vec4(0.0);
	//Indirect Diffuse Contribution

	vec3 startPos = inFragPos + N * vxgiConfigUB.voxel_size * vxgiConfigUB.traceStartOffset;
    
	float cone_aperture =  0.57735;
    float coneTraceCount = 0.0;
    float cosSum = 0.0;
	for (int i = 0; i < DIFFUSE_CONE_COUNT; ++i)
    {
		float cosTheta = dot(N, DIFFUSE_CONE_DIRECTIONS[i]);
        
        if (cosTheta < 0.0)
			   continue;
        
        coneTraceCount += 1.0;
		//indirectContribution += CastCone(startPos, DIFFUSE_CONE_DIRECTIONS[i], DIFFUSE_CONE_APERTURE, MAX_TRACE_DISTANCE) * cosTheta;
		indirectContribution.rgb +=  CastDiffuseCone(startPos, DIFFUSE_CONE_DIRECTIONS[i], cone_aperture) * cosTheta;
	}

	indirectContribution /= coneTraceCount;
    indirectContribution.a *= vxgiConfigUB.ambientOcclusionFactor;
    
	vec3 specularConeDirection = reflect(-V, N);
    vec3 specularContribution = vec3(0.0);
	specularContribution += CastDiffuseCone(startPos, specularConeDirection, 0.1);
	specularContribution =  clamp(specularContribution, 0.0,1.0f);
    //specularContribution += CastSpecularCone(startPos, specularConeDirection, 2.0);
	specularContribution *=  vxgiConfigUB.indirectSpecularIntensity;

	//Evaluate indirect specular 
		//Evaluate shadow term
	vec4 fragPosViewSpace = sceneData.view * vec4(inFragPos,1.0f);
    float depthValue = fragPosViewSpace.z;
    int layer = 0;
	for(int i = 0; i < 4 - 1; ++i) {
		if(depthValue < sceneData.distances[i]) {	
			layer = i + 1;
		}
	}
    vec4 shadowCoord = (biasMat * shadowData.shadowMatrices[layer]) * vec4(inFragPos, 1.0);		

    float shadow = filterPCF(shadowCoord/shadowCoord.w,layer);
	vec3 spec_comp = vec3(0.0f);
	vec3 KD = vec3(0.0f);
	vec3 color = StandardSurfaceShading(N, V, L, albedo, metallicRough,shadow, spec_comp, KD);

	indirectContribution.rgb *= albedo * vxgiConfigUB.indirectDiffuseIntensity * KD;
	indirectContribution = clamp(indirectContribution, 0.0, 1.0);

	specularContribution *= spec_comp;

	if(mat_description.has_emission == 1)
	{
		color += emission;	
	}

	if(mat_description.has_occlusion_tex == 1)
	{
		color *= occlusion;
	}
	color.rgb += indirectContribution.rgb;
	color.rgb += specularContribution.rgb;
	//color.rgb *= indirectContribution.a;

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
	//vec3 indirect_color = (vec3(1.0) + indirectContribution.rgb) / 2.0f; 
    outFragColor = vec4(color.rgb,1);  
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
		//float D = D_GGX(dotNH, roughness); 
		float D = D_GGX_IMPROVED(roughness, dotNH, N, H);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = V_SmithGGXCorrelated(dotNV,dotNL, roughness);
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
    
	return normalize(inTBN * tangentNormal);
}