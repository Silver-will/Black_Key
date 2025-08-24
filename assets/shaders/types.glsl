#ifndef TYPES_GLSL
#define TYPES_GLSL
struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	vec4 tangent;
}; 


layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

struct GLTFMaterialData{   
	vec4 colorFactors;
	vec4 metal_rough_factors;
	int has_metalRough;
	int has_occlusion_tex;
	int has_emission;
	int pad;
	vec4 emission_color;
};

struct ObjectData{
    mat4 model;
	vec4 spherebounds;
	uint texture_index;
    uint firstIndex;
    uint indexCount;
	vec3 pad;
	VertexBuffer vertexBuffer;
}; 
#endif