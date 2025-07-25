#pragma once 
#include "vk_types.h"

namespace vkutil {

    bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
};

class PipelineBuilder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;
    VkFormat _colorAttachmentformat;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;

    PipelineBuilder() { clear(); }

    void clear();

    VkPipeline build_pipeline(VkDevice device);

    void set_vertex_input_state(VkPipelineVertexInputStateCreateInfo vertexInfo);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode polygon);
    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader, VkShaderModule geometryShader = NULL);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void disable_blending();
    void set_multisampling_none();
    void set_multisampling_level(VkSampleCountFlagBits samples);
    void disable_depthtest();
    void set_depth_format(VkFormat format);
    void set_color_attachment_format(VkFormat format);
    void enable_depthtest(bool depthWriteEnable, bool depthTestEnable, VkCompareOp op);
    void enable_blending_additive();
    void enable_blending_alphablend();
    void set_custom_blending_configuration(VkPipelineColorBlendAttachmentState colorBlendAttachment);
};