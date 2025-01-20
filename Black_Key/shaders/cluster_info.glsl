struct VolumeTileAABB{
    vec4 minPoint;
    vec4 maxPoint;
};

layout (std430, binding = 0) buffer clusterAABB{
    VolumeTileAABB cluster[ ];
};

layout (std430, binding = 1) buffer screenToView{
    mat4 inverseProjection;
    vec4 tileSizes;
    uvec2 screenDimensions;
    float sliceFactor;
    float sliceBiasFactor;
};