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