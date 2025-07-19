//Scene resources to be updated once per frame
#include "lights.glsl"
#define MAX_MATERIAL_COUNT 65536


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


layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 skyMat;
	vec4 cameraPos;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec3 cascadeConfigData;
	uint lightCount;
	vec4 distances;
	vec4 debugInfo;
} sceneData;


layout(set = 0, binding = 2) uniform sampler2DArray shadowMap;
layout(set = 0, binding = 3) uniform samplerCube irradianceMap;
layout(set = 0, binding = 4) uniform sampler2D BRDFLUT;
layout(set = 0, binding = 5) uniform samplerCube preFilterMap;

layout (set = 0, binding = 6) readonly buffer lightSSBO{
    PointLight pointLight[];
};

layout (set = 0, binding = 7) readonly buffer screenToView{
    mat4 inverseProjection;
    vec4 tileSizes;
    vec2 screenDimensions;
    float scale;
    float bias;
};

layout (set = 0, binding = 8) readonly buffer lightIndexSSBO{
    uint globalLightIndexList[];
};
layout (set = 0, binding = 9) readonly buffer lightGridSSBO{
    LightGrid lightGrid[];
};

layout(set = 0, binding = 11) uniform  ShadowData{   
	mat4 shadowMatrices[4];
} shadowData;


struct GLTFMaterialData{   
	vec4 colorFactors;
	vec4 metal_rough_factors;
	int colorTexID;
	int metalRoughTexID;
};

layout(set = 1, binding = 0) uniform GLTFMaterialDataBuffer{
	GLTFMaterialData material_data[MAX_MATERIAL_COUNT];	
};
layout(set = 1, binding = 1) uniform sampler2D material_textures[];
layout(rgba8, set = 1, binding = 2) uniform image2D storage_image[];