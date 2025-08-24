#ifndef GLOBAL_RESOURCE_GLSL
#define GLOBAL_RESOURCE_GLSL
#include "types.glsl"
#define MAX_MATERIAL_COUNT 65536

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



layout(set = 1, binding = 0) uniform GLTFMaterialDataBuffer{
	GLTFMaterialData material_data[MAX_MATERIAL_COUNT];	
}object_material_description;
layout(set = 1, binding = 1) uniform sampler2D material_textures[];
layout(rgba8, set = 1, binding = 2) uniform image2D storage_image[];

#endif