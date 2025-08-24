//Scene resources to be updated once per frame
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "lights.glsl"
#include "types.glsl"


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
