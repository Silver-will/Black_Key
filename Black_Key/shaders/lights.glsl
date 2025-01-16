struct PointLight{
    vec4 position;
    vec4 color;
    uint enabled;
    float intensity;
    float range;
};

struct LightGrid{
    uint offset;
    uint count;
};

struct VolumeTileAABB{
    vec4 minPoint;
    vec4 maxPoint;
};

layout (std430, binding = 0) buffer clusterAABB{
    VolumeTileAABB cluster[ ];
};

layout (std430, binding = 1) buffer screenToView{
    mat4 inverseProjection;
    uvec4 tileSizes;
    uvec2 screenDimensions;
};