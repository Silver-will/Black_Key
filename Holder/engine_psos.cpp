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
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

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
	mat.shadowImage = vkutil::create_image_empty(VkExtent3D(1024, 1024, 1), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, engine, VK_IMAGE_VIEW_TYPE_2D_ARRAY, false, engine->shadows.getCascadeLevels());
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

	VkDescriptorSetLayout layouts[] = { engine->_skyboxDescriptorLayout};

	VkPipelineLayoutCreateInfo sky_layout_info = vkinit::pipeline_layout_create_info();
	sky_layout_info.setLayoutCount = 1;
	sky_layout_info.pSetLayouts = layouts;
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

	pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(engine->_depthImage.imageFormat);

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


void BloomBlurPipelineObject::build_pipelines(VulkanEngine* engine)
{

}

void BloomBlurPipelineObject::clear_resources(VkDevice device)
{

}

MaterialInstance BloomBlurPipelineObject::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance newmat;
	return newmat;
}


void RenderImagePipelineObject::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule HDRVertexShader;
	if (!vkutil::load_shader_module("shaders/hdr.vert.spv", engine->_device, &HDRVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule HDRFragmentShader;
	if (!vkutil::load_shader_module("shaders/hdr.frag.spv", engine->_device, &HDRFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayout layouts[] = { engine->_drawImageDescriptorLayout };

	VkPipelineLayoutCreateInfo image_layout_info = vkinit::pipeline_layout_create_info();
	image_layout_info.setLayoutCount = 1;
	image_layout_info.pSetLayouts = layouts;
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

	pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);

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


void EarlyDepthPipelineObject::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule depthVertexShader;
	if (!vkutil::load_shader_module("shaders/depth_pass.vert.spv", engine->_device, &depthVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule depthFragmentShader;
	if (!vkutil::load_shader_module("shaders/cascaded_shadows.frag.spv", engine->_device, &depthFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}


	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayout layouts[] = { engine->_gpuSceneDataDescriptorLayout};

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 1;
	mesh_layout_info.pSetLayouts = layouts;
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

	pipelineBuilder.set_depth_format(engine->_shadowDepthImage.imageFormat);

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



void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule meshFragShader;
	if (!vkutil::load_shader_module("shaders/pbr_cluster.frag.spv", engine->_device, &meshFragShader)) {
		fmt::println("Error when building the triangle fragment shader module");
	}

	VkShaderModule meshVertexShader;
	if (!vkutil::load_shader_module("shaders/pbr_cluster.vert.spv", engine->_device, &meshVertexShader)) {
		fmt::println("Error when building the triangle vertex shader module");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	DescriptorLayoutBuilder layoutBuilder;
#if USE_BINDLESS
	constexpr uint32_t MAX_BINDLESS_RESOURCES = 256;
	layoutBuilder.add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES);
	layoutBuilder.add_binding(11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_BINDLESS_RESOURCES);
	materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT);
#else
	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
#endif


	VkDescriptorSetLayout layouts[] = { engine->_gpuSceneDataDescriptorLayout,
		materialLayout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 2;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	opaquePipeline.layout = newLayout;
	transparentPipeline.layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_level(engine->msaa_samples);
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	//render format
	pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(engine->_depthImage.imageFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
	opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	// create the transparent variant
	pipelineBuilder.enable_blending_additive();

	pipelineBuilder.enable_depthtest(false, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::set_material_properties(const MaterialPass pass)
{
	MaterialInstance matData;
	matData.passType = pass;
	if (pass == MaterialPass::Transparent) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}
	return matData;
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance matData;
	matData.passType = pass;
	if (pass == MaterialPass::Transparent) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(device, materialLayout);


	writer.clear();
	writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(3, resources.normalImage.imageView, resources.normalSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	if (resources.separate_occ_texture)
	{
		writer.write_image(4, resources.occlusionImage.imageView, resources.occlusionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}
	writer.update_set(device, matData.materialSet);

	return matData;
}

void GLTFMetallic_Roughness::write_material_array(VkDevice device, LoadedGLTF& file, std::vector< GLTFMetallic_Roughness::MaterialResources>& bindless_resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	writer.clear();
	for (int i = 0; i < bindless_resources.size(); i++)
	{
		int offset = i * 4;
		//writer.write_buffer(i + offset, bindless_resources[i].dataBuffer, sizeof(MaterialConstants), bindless_resources[i].dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		//writer.write_image(i + offset + 1, bindless_resources[i].colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		//writer.write_image(i + offset + 2, bindless_resources[i].metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		//writer.write_image(i + offset + 3, bindless_resources[i].normalImage.imageView, resources.normalSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	}
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);

	vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
	vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
}
