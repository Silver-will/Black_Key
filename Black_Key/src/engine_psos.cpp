#include "engine_psos.h"
#include "vk_pipelines.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include <ktx.h>

void ShadowPipelineResources::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule shadowVertexShader;
	if (!vkutil::load_shader_module("shaders/cascaded_shadows.vert.spv", engine->_device, &shadowVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule shadowFragmentShader;
	if (!vkutil::load_shader_module("shaders/cascaded_shadows.frag.spv", engine->_device, &shadowFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}
	
	VkShaderModule shadowGeometryShader;
	if (!vkutil::load_shader_module("shaders/cascaded_shadows.geom.spv", engine->_device, &shadowGeometryShader)) {
		fmt::print("Error when building the shadow geometry shader module\n");
	}
	
	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//DescriptorLayoutBuilder layoutBuilder;
	//layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	//materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);

	VkDescriptorSetLayout layouts[] = { engine->_gpuSceneDataDescriptorLayout/*,
		materialLayout*/};

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 1;
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
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//pipelineBuilder.set_color_attachment_format();
	pipelineBuilder.set_depth_format(engine->_shadowDepthImage.imageFormat);

	pipelineBuilder._pipelineLayout = newLayout;

	shadowPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, shadowVertexShader,nullptr);
	vkDestroyShaderModule(engine->_device, shadowFragmentShader, nullptr);
	vkDestroyShaderModule(engine->_device, shadowGeometryShader, nullptr);
}

ShadowPipelineResources::MaterialResources ShadowPipelineResources::AllocateResources(VulkanEngine* engine)
{
	MaterialResources mat;
	mat.shadowImage = vkutil::create_image_empty(VkExtent3D(1024, 1024, 1), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, engine, VK_IMAGE_VIEW_TYPE_2D_ARRAY, false, engine->shadows.getCascadeLevels().size());
	mat.shadowSampler = engine->_defaultSamplerLinear;
	return mat;
}

void ShadowPipelineResources::write_material(VkDevice device, MaterialPass pass, VulkanEngine* engine, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	matData.passType = pass;
	matData.materialSet = descriptorAllocator.allocate(device, materialLayout);
	
	auto materialResource = AllocateResources(engine);

	writer.clear();
	writer.write_image(1, materialResource.shadowImage.imageView, materialResource.shadowSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(device, matData.materialSet);
}

void ShadowPipelineResources::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, shadowPipeline.layout, nullptr);

	vkDestroyPipeline(device, shadowPipeline.pipeline, nullptr);
}


void SkyBoxPipelineResources::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule skyVertexShader;
	if (!vkutil::load_shader_module("shaders/skybox.vert.spv", engine->_device, &skyVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule skyFragmentShader;
	if (!vkutil::load_shader_module("shaders/skybox.frag.spv", engine->_device, &skyFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { engine->_gpuSceneDataDescriptorLayout,
		materialLayout };

	VkPipelineLayoutCreateInfo sky_layout_info = vkinit::pipeline_layout_create_info();
	sky_layout_info.setLayoutCount = 2;
	sky_layout_info.pSetLayouts = layouts;
	sky_layout_info.pPushConstantRanges = &matrixRange;
	sky_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &sky_layout_info, nullptr, &skyPipeline.layout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(skyVertexShader, skyFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder._pipelineLayout = skyPipeline.layout;

	pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);

	skyPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, skyVertexShader, nullptr);
	vkDestroyShaderModule(engine->_device, skyFragmentShader, nullptr);
}

void SkyBoxPipelineResources::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, skyPipeline.layout, nullptr);

	vkDestroyPipeline(device, skyPipeline.pipeline, nullptr);
}

MaterialInstance SkyBoxPipelineResources::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance newmat;
	return newmat;
}


void PostProcessingPipelineResources::build_pipelines(VulkanEngine* engine)
{

}

void PostProcessingPipelineResources::clear_resources(VkDevice device)
{

}

MaterialInstance PostProcessingPipelineResources::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance newmat;
	return newmat;
}
