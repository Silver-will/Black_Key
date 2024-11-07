#include "engine_psos.h"
#include "vk_pipelines.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include <ktx.h>

void ShadowPipelineResources::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule shadowVertexShader;
	if (!vkutil::load_shader_module("../../shaders/cascaded_shadow.vert.spv", engine->_device, &shadowVertexShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	}
	else {
		fmt::print("Shadow vertex shader succesfully loaded");
	}
	VkShaderModule shadowFragmentShader;
	if (!vkutil::load_shader_module("../../shaders/cascaded_shadow.frag.spv", engine->_device, &shadowFragmentShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	}
	else {
		fmt::print("Shadow fragment shader succesfully loaded");
	}
	VkShaderModule shadowGeometryShader;
	if (!vkutil::load_shader_module("../../shaders/cascaded_shadow.geom.spv", engine->_device, &shadowGeometryShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	}
	else {
		fmt::print("Shadow geometry shader succesfully loaded");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { engine->_gpuSceneDataDescriptorLayout,
		materialLayout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 2;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	shadowPipeline.layout = newLayout;

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(shadowVertexShader, shadowFragmentShader, shadowGeometryShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	pipelineBuilder._pipelineLayout = newLayout;

	shadowPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, shadowVertexShader,nullptr);
	vkDestroyShaderModule(engine->_device, shadowFragmentShader, nullptr);
	vkDestroyShaderModule(engine->_device, shadowGeometryShader, nullptr);
}

void ShadowPipelineResources::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	matData.passType = pass;
	matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

	writer.clear();
	writer.write_buffer(0, resources.dataBuffer, sizeof(ShadowMatrices), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	writer.update_set(device, matData.materialSet);
}

void ShadowPipelineResources::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, shadowPipeline.layout, nullptr);

	vkDestroyPipeline(device, shadowPipeline.pipeline, nullptr);
}