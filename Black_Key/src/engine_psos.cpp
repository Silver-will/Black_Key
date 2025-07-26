#include "engine_psos.h"
#include "vk_pipelines.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include <ktx.h>
#include <print>

void ShadowPipelineResources::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule shadowVertexShader;
	if (!vkutil::load_shader_module("shaders/cascaded_shadows.vert.spv", engine->_device, &shadowVertexShader)) {
		std::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule shadowFragmentShader;
	if (!vkutil::load_shader_module("shaders/cascaded_shadows.frag.spv", engine->_device, &shadowFragmentShader)) {
		std::print("Error when building the shadow fragment shader module\n");
	}

	VkShaderModule shadowGeometryShader;
	if (!vkutil::load_shader_module("shaders/cascaded_shadows.geom.spv", engine->_device, &shadowGeometryShader)) {
		std::print("Error when building the shadow geometry shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//VkDescriptorSetLayout layouts[] = { engine->cascaded_shadows_descriptor_layout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 1;
	mesh_layout_info.pSetLayouts = info.layouts.data();
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	shadowPipeline.layout = newLayout;

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(shadowVertexShader, shadowFragmentShader, shadowGeometryShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	//pipelineBuilder.set_color_attachment_format();
	pipelineBuilder.set_depth_format(info.depthFormat);

	pipelineBuilder._pipelineLayout = newLayout;

	shadowPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, shadowVertexShader, nullptr);
	vkDestroyShaderModule(engine->_device, shadowFragmentShader, nullptr);
	vkDestroyShaderModule(engine->_device, shadowGeometryShader, nullptr);
}

ShadowPipelineResources::MaterialResources ShadowPipelineResources::AllocateResources(VulkanEngine* engine)
{
	//MaterialResources mat;
	//mat.shadowImage = vkutil::create_image_empty(VkExtent3D(1024, 1024, 1), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, engine, VK_IMAGE_VIEW_TYPE_2D_ARRAY, false, engine->shadows.getCascadeLevels());
	//mat.shadowSampler = engine->defaultSamplerLinear;
	//return mat;
	MaterialResources mat;
	return mat;
}

void ShadowPipelineResources::write_material(VkDevice device, vkutil::MaterialPass pass, VulkanEngine* engine, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
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


void SkyBoxPipelineResources::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule skyVertexShader;
	if (!vkutil::load_shader_module("shaders/skybox.vert.spv", engine->_device, &skyVertexShader)) {
		std::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule skyFragmentShader;
	if (!vkutil::load_shader_module("shaders/skybox.frag.spv", engine->_device, &skyFragmentShader)) {
		std::print("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	///VkDescriptorSetLayout layouts[] = { engine->_skyboxDescriptorLayout };

	VkPipelineLayoutCreateInfo sky_layout_info = vkinit::pipeline_layout_create_info();
	sky_layout_info.setLayoutCount = 1;
	sky_layout_info.pSetLayouts = info.layouts.data();
	sky_layout_info.pPushConstantRanges = &matrixRange;
	sky_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &sky_layout_info, nullptr, &skyPipeline.layout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(skyVertexShader, skyFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_level(engine->msaa_samples);
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder._pipelineLayout = skyPipeline.layout;

	pipelineBuilder.set_color_attachment_format(info.imageFormat);
	pipelineBuilder.set_depth_format(info.depthFormat);

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

MaterialInstance SkyBoxPipelineResources::write_material(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance newmat;
	return newmat;
}


void BloomBlurPipelineObject::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{

}

void BloomBlurPipelineObject::clear_resources(VkDevice device)
{

}

MaterialInstance BloomBlurPipelineObject::write_material(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance newmat;
	return newmat;
}


void RenderImagePipelineObject::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule HDRVertexShader;
	if (!vkutil::load_shader_module("shaders/hdr.vert.spv", engine->_device, &HDRVertexShader)) {
		std::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule HDRFragmentShader;
	if (!vkutil::load_shader_module("shaders/hdr.frag.spv", engine->_device, &HDRFragmentShader)) {
		std::print("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PostProcessPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

//	VkDescriptorSetLayout layouts[] = { engine->_drawImageDescriptorLayout };

	VkPipelineLayoutCreateInfo image_layout_info = vkinit::pipeline_layout_create_info();
	image_layout_info.setLayoutCount = 1;
	image_layout_info.pSetLayouts = info.layouts.data();
	image_layout_info.pPushConstantRanges = &matrixRange;
	image_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &image_layout_info, nullptr, &renderImagePipeline.layout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(HDRVertexShader, HDRFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder._pipelineLayout = renderImagePipeline.layout;

	pipelineBuilder.set_color_attachment_format(info.imageFormat);

	renderImagePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, HDRVertexShader, nullptr);
	vkDestroyShaderModule(engine->_device, HDRFragmentShader, nullptr);

}

void RenderImagePipelineObject::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, renderImagePipeline.layout, nullptr);

	vkDestroyPipeline(device, renderImagePipeline.pipeline, nullptr);
}


void UpsamplePipelineObject::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule HDRVertexShader;
	if (!vkutil::load_shader_module("shaders/bloom.vert.spv", engine->_device, &HDRVertexShader)) {
		std::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule UpsampleFrag;
	if (!vkutil::load_shader_module("shaders/upsample.frag.spv", engine->_device, &UpsampleFrag)) {
		std::print("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(BloomUpsamplePushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo image_layout_info = vkinit::pipeline_layout_create_info();
	image_layout_info.setLayoutCount = 1;
	image_layout_info.pSetLayouts = info.layouts.data();
	image_layout_info.pPushConstantRanges = &matrixRange;
	image_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &image_layout_info, nullptr, &renderImagePipeline.layout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(HDRVertexShader, UpsampleFrag);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();

	VkPipelineColorBlendAttachmentState colorState;
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	colorState.blendEnable = VK_TRUE;
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;
	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;
	pipelineBuilder.set_custom_blending_configuration(colorState);
	//pipelineBuilder.enable_blending_additive();
	
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder._pipelineLayout = renderImagePipeline.layout;
	pipelineBuilder.set_color_attachment_format(info.imageFormat);

	renderImagePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, HDRVertexShader, nullptr);
	vkDestroyShaderModule(engine->_device, UpsampleFrag, nullptr);
}


void UpsamplePipelineObject::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, renderImagePipeline.layout, nullptr);

	vkDestroyPipeline(device, renderImagePipeline.pipeline, nullptr);
}


void EarlyDepthPipelineObject::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule depthVertexShader;
	if (!vkutil::load_shader_module("shaders/depth_pass.vert.spv", engine->_device, &depthVertexShader)) {
		std::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule depthFragmentShader;
	if (!vkutil::load_shader_module("shaders/cascaded_shadows.frag.spv", engine->_device, &depthFragmentShader)) {
		std::print("Error when building the shadow fragment shader module\n");
	}


	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//VkDescriptorSetLayout layouts[] = { engine->gpu_scene_data_descriptor_layout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 1;
	mesh_layout_info.pSetLayouts = info.layouts.data();
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	earlyDepthPipeline.layout = newLayout;

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(depthVertexShader, depthFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_level(engine->msaa_samples);
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder.set_depth_format(info.depthFormat);

	pipelineBuilder._pipelineLayout = newLayout;

	earlyDepthPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, depthVertexShader, nullptr);
	vkDestroyShaderModule(engine->_device, depthFragmentShader, nullptr);
}

void EarlyDepthPipelineObject::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, earlyDepthPipeline.layout, nullptr);

	vkDestroyPipeline(device, earlyDepthPipeline.pipeline, nullptr);
}