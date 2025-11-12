layout(set = 0, binding = 12) uniform sampler3D voxel_radiance;


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

const float MIN_STEP_FACTOR = 0.2;
const float MAX_TRACE_DISTANCE = 20.0;
const float MIN_SPECULAR_APERTURE = 0.05;
#define TSQRT2 2.828427
#define SQRT2 1.414213
#define ISQRT2 0.707106
#define MIPMAP_HARDCAP 6.0f 

// Scales and bias a given vector (i.e. from [-1, 1] to [0, 1]).
vec3 scaleAndBias(const vec3 p) { return 0.5f * p + vec3(0.5f); }


vec3 worldToVoxelUV(vec3 p)
{
    vec3 samplePos =  (p - vxgiConfigUB.region_min.xyz) / (vxgiConfigUB.region_max.xyz - vxgiConfigUB.region_min.xyz);
	return samplePos;
}


float CastShadowCone(vec3 from, vec3 direction, float aperture, float maxDistance)
{
	direction = normalize(direction);

	float voxelSize = vxgiConfigUB.voxel_size;

    float distance = voxelSize;
    float occlusion = 0.0f;

    vec3 result = vec3(0.0f);

    float coneIncline = tan(radians(aperture) / 2.0);
	//float maxDistance = MAX_TRACE_DISTANCE;
	//vec3 posW = 0.5f * from + vec3(0.5f);

	    while (occlusion < 1.0f && distance <= maxDistance) {
        vec3 currentPos = from + direction * distance;

        //vec3 uvw = (currentPos + voxelGridSize * 0.5) / voxelGridSize;
		vec3 uvw = worldToVoxelUV(currentPos);

        if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0))))
            break;

        float diameter = 2.0 * distance * coneIncline;
        float lod = log2(diameter / voxelSize);

        vec4 voxelSample = textureLod(voxel_radiance, uvw, min(lod,MIPMAP_HARDCAP ));

		occlusion += (1.0 - occlusion) *  voxelSample.a * 2.25;

        distance += voxelSize;
    }
    return clamp(1.0 - occlusion, 0.0, 1.0);
}

vec4 CastCone(vec3 from, vec3 direction, float aperture)
{
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

        if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0))))
            break;

        float diameter = distance * coneIncline;
        float lod = log2(diameter / voxelSize);

        vec4 voxelSample = textureLod(voxel_radiance, uvw, min(lod,MIPMAP_HARDCAP ));

		float att = 1.0 / (distance * distance);
		

		//result += voxelSample.rgb * att;
        //if (distance > voxelSize)
        result += (1.0 - occlusion) * voxelSample.rgb * voxelSample.a;
		occlusion += (1.0 - occlusion) *  voxelSample.a;
        
		distance += max(diameter, voxelSize);
    }
	occlusion = min(occlusion, 1.0);
	occlusion = 1.0 - occlusion;
    return vec4(result,occlusion);
}


vec3 CastSpecularCone(vec3 from, vec3 direction, float roughness)
{
	direction = normalize(direction);

	float voxelSize = vxgiConfigUB.voxel_size;

	const float OFFSET = 2 * voxelSize;
	const float STEP = voxelSize;

	//from += OFFSET * inNormal;
	
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
