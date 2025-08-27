layout(set = 0, binding = 0) uniform Texture3DData{
    mat4 viewproj;
    vec3 resolution;
    float texel_size;
    vec3 position;
    float padding;
    vec3 border_color;
    float border_alpha;    
} tex_3d_data;

layout(set = 0, binding = 1, r32ui) uniform volatile uimage3D voxel_radiance;