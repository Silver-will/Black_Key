#include "voxelizer.h"

void Voxelizer::InitializeVoxelDebugger()
{
    auto res = voxel_texture_resolution;
    std::size_t vertexCount = std::size_t(res * res * res);

    std::vector<VoxelVertex> vertices;
    vertices.reserve(vertexCount);

    for (uint16_t z = 0; z < res; ++z)
    {
        for (uint16_t y = 0; y < res; ++y)
        {
            for (uint16_t x = 0; x < res; ++x)
            {
                vertices.push_back(VoxelVertex(x, y, z));
            }
        }
    }
}