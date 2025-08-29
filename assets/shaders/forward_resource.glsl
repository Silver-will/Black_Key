//Scene resources to be updated once per frame
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "lights.glsl"
#include "types.glsl"


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
