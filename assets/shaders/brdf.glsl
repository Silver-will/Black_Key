#include "resource.glsl"

#define MEDIUMP_FLT_MAX    65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
const float PI = 3.14159265359;

float V_SmithGGXCorrelated(float NoV, float NoL, float roughness);
float D_Burley(float roughness, float LoH, float NoL, float NoV);
float D_GGX_IMPROVED(float roughness, float NoH, const vec3 n, const vec3 h);
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness);



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

// ----------------------------------------------------------------------------
//vec3 fresnelSchlick(float cosTheta, vec3 F0)
//{
  //  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
//}


// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}


float F_Schlick(float u, float f0, float f90) {
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 EvaluateIBL(vec3 N, vec3 R, vec3 diffuseColor, vec3 f0, vec3 f90, float roughness, float NoV)
{
    vec3 Ld = texture(irradianceMap, R).rgb * diffuseColor;
    //float lod = computeLODFromRoughness(perceptualRoughness);
    //vec3 Lld = preFilteredReflection(r, perceptualRoughness);
    //vec2 Ldfg = textureLod(BRDFLut, vec2(NoV, perceptualRoughness), 0.0).xy;
    //vec3 Lr =  (f0 * Ldfg.x + f90 * Ldfg.y) * Lld;
    //return Ld + Lr;
    //vec3 F = F_SchlickR(NoV, f0, roughness);
    //vec2 brdf = texture(BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	//vec3 reflection = prefilteredReflection(R, roughness).rgb;	
	//vec3 irradiance = texture(irradianceMap, N).rgb;
    
    //diffuseColor = diffuseColor * irradiance;
    return vec3(0.0);
}

float RcpSinFromCos(float cosTheta)
{
    return inversesqrt(max(0.0,1.0 - (cosTheta * cosTheta)));
}

vec3 GetViewClampedNormal(vec3 N, vec3 V, inout float NdotV)
{
    NdotV = dot(N,V);

    if(NdotV < 0)
    {
        N = (N - NdotV * V) * RcpSinFromCos(NdotV);
        NdotV = 0;
    }
    return N;
}

vec3 GetViewReflectedNormal(vec3 N, vec3 V, inout float NdotV)
{
    NdotV = dot(N,V);
    N += (2.0 * clamp(-NdotV, 0.0, 1.0)) * V;
    NdotV = abs(NdotV);
    return N;
}

vec3 StandardSurfaceShading(vec3 N, vec3 V, vec3 L, vec3 albedo, vec2 metal_rough)
{
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
    vec3 diffuse = (1.0 - metallic) * albedo;
	
    float D = D_GGX_IMPROVED(roughness, NoH, N, H);
    //float D = DistributionGGX(N, H, roughness);
    vec3 F = F_Schlick(LoH, F0);

    //This uses 2 square roots
    //float V_S = V_SmithGGXCorrelated(NoV, NoL, roughness);
    float V_S = GeometrySmith(N, V, L, roughness);

    float disney_D = D_Burley(roughness, LoH, NoL, NoV);
    vec3 Fs = (D * V_S) * F;
    vec3 Fd = diffuse * disney_D;

    //Evaluate IBL Terms
    vec3 F_IBL = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);
    vec2 brdf = texture(BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 reflection = prefilteredReflection(R, roughness).rgb;	
    vec3 texCoord = vec3(N.x, N.y, N.z);
	vec3 irradiance = texture(irradianceMap, texCoord).rgb;

    vec3 specular = reflection * (F_IBL * brdf.x + brdf.y);

    return (vec3(Fd * irradiance + Fs * specular) * sceneData.sunlightDirection.w) * sceneData.sunlightColor.xyz;
}


float D_Lambert()
{
    return 1.0/PI;
}
float D_Burley(float roughness, float LoH, float NoL, float NoV)
{
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(NoL, 1.0, f90);
    float viewScatter = F_Schlick(NoV, 1.0, f90);
    return lightScatter * viewScatter * (1.0 / PI);
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

float D_GGX_IMPROVED(float roughness, float NoH, const vec3 n, const vec3 h)
{
    vec3 NxH = cross(n, h);
    float a = NoH * roughness;
    float k = roughness / (dot(NxH, NxH) + a * a);
    float d = k * k * (1.0 / PI);
    return saturateMediump(d);
}
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}


float V_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    float a2 = roughness * roughness;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
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
