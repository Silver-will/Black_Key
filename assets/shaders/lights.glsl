#define CASCADE_COUNT 4 


struct PointLight{
    vec4 position;
    vec4 color;
    float enabled;
    float range;
    float intensity;
    float padding;
};

struct LightGrid{
    uint offset;
    uint count;
};


void PBR_LOGL()
{
	//Learn opengl PBR
    /*
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
    vec3 diffuse = texture(irradianceMap,N).rgb;
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
}