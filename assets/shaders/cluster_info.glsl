struct VolumeTileAABB{
    vec4 minPoint;
    vec4 maxPoint;
};

layout (set = 0, binding = 0) buffer clusterAABB{
    VolumeTileAABB cluster[ ];
};

layout (set = 0, binding = 1) buffer screenToView{
    mat4 inverseProjection;
    vec4 tileSizes;
    vec2 screenDimensions;
    float sliceFactor;
    float sliceBiasFactor;
};