#include "engine_psos.h"
#include "vk_pipelines.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include <ktx.h>
#include <print>


void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule meshFragShader;
	std::string assets_path = ENGINE_ASSET_PATH;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/pbr_test.frag.spv").c_str(), engine->_device, &meshFragShader)) {
		std::println("Error when building the triangle fragment shader module");
	}

	VkShaderModule meshVertexShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/indirect_forward.vert.spv").c_str(), engine->_device, &meshVertexShader)) {
		std::println("Error when building the triangle vertex shader module");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	DescriptorLayoutBuilder layoutBuilder;

	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 2;
	mesh_layout_info.pSetLayouts = info.layouts.data();
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
	//No need to write depth. z-prepass already populated the depth buffer
	pipelineBuilder.enable_depthtest(false, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	//render format
	pipelineBuilder.set_color_attachment_format(info.imageFormat);
	pipelineBuilder.set_depth_format(info.depthFormat);

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

MaterialInstance GLTFMetallic_Roughness::SetMaterialProperties(const vkutil::MaterialPass pass, int mat_index)
{
	MaterialInstance matData;
	matData.passType = pass;
	if (pass == vkutil::MaterialPass::transparency) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	if (mat_index >= 0)
		matData.material_index = mat_index;
	return matData;
}

MaterialInstance GLTFMetallic_Roughness::WriteMaterial(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance matData;
	matData.passType = pass;
	if (pass == vkutil::MaterialPass::transparency) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(device, materialLayout);
	
	writer.clear();
	//writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
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

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);

	vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
	vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
}



void ShadowPipelineResources::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule shadowVertexShader;
	std::string assets_path = ENGINE_ASSET_PATH;

	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/cascaded_shadows.vert.spv").c_str(), engine->_device, &shadowVertexShader)) {
		std::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule shadowFragmentShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/cascaded_shadows.frag.spv").c_str(), engine->_device, &shadowFragmentShader)) {
		std::print("Error when building the shadow fragment shader module\n");
	}

	VkShaderModule shadowGeometryShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/cascaded_shadows.geom.spv").c_str(), engine->_device, &shadowGeometryShader)) {
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
	std::string assets_path = ENGINE_ASSET_PATH;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/skybox.vert.spv").c_str(), engine->_device, &skyVertexShader)) {
		std::print("Error when building the skybox vertex shader module\n");
	}

	VkShaderModule skyFragmentShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/skybox.frag.spv").c_str(), engine->_device, &skyFragmentShader)) {
		std::print("Error when building the skybox fragment shader module\n");
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
	std::string assets_path = ENGINE_ASSET_PATH;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/hdr.vert.spv").c_str(), engine->_device, &HDRVertexShader)) {
		std::print("Error when building the fullscreen quad vertex shader module\n");
	}

	VkShaderModule HDRFragmentShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/hdr.frag.spv").c_str(), engine->_device, &HDRFragmentShader)) {
		std::print("Error when building the fullscreen quad fragment shader module\n");
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
	std::string assets_path = ENGINE_ASSET_PATH;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/bloom.vert.spv").c_str(), engine->_device, &HDRVertexShader)) {
		std::print("Error when building the bloom upsample vertex shader module\n");
	}

	VkShaderModule UpsampleFrag;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/upsample.frag.spv").c_str(), engine->_device, &UpsampleFrag)) {
		std::print("Error when building the bloom upsample fragment shader module\n");
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
	std::string assets_path = ENGINE_ASSET_PATH;

	VkShaderModule depthVertexShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/depth_pass.vert.spv").c_str(), engine->_device, &depthVertexShader)) {
		std::print("Error when building the early depth vertex shader module\n");
	}

	VkShaderModule depthFragmentShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/cascaded_shadows.frag.spv").c_str(), engine->_device, &depthFragmentShader)) {
		std::print("Error when building the early depth fragment shader module\n");
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


void ConservativeVoxelizationPipelineObject::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule voxelization_frag_shader;
	std::string assets_path = ENGINE_ASSET_PATH;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/voxel_cone_tracing/cons_voxelization.frag.spv").c_str(), engine->_device, &voxelization_frag_shader)) {
		std::println("Error when building the voxelization fragment shader module");
	}
	
	VkShaderModule voxelization_geom_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/voxel_cone_tracing/cons_voxelization.geom.spv").c_str(), engine->_device, &voxelization_geom_shader)) {
		std::println("Error when building the voxelization fragment shader module");
	}

	VkShaderModule voxelization_vert_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/voxel_cone_tracing/cons_voxelization.vert.spv").c_str(), engine->_device, &voxelization_vert_shader)) {
		std::println("Error when building the voxelization vertex shader module");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(VoxelizationPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	DescriptorLayoutBuilder layoutBuilder;

	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	layoutBuilder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.add_binding(11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 2;
	mesh_layout_info.pSetLayouts = info.layouts.data();
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	conservative_radiance_pipeline.layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(voxelization_vert_shader, voxelization_frag_shader, voxelization_geom_shader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_level(info.msaa_samples);
	pipelineBuilder.disable_blending();
	pipelineBuilder.disable_color_write();
	pipelineBuilder.disable_depthtest();
	pipelineBuilder.set_stencil_test(VK_TRUE);


	//Conservative rasterization setup
		
	PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(vkGetInstanceProcAddr(engine->_instance, "vkGetPhysicalDeviceProperties2KHR"));
	assert(vkGetPhysicalDeviceProperties2KHR);
	VkPhysicalDeviceProperties2KHR deviceProps2{};
	conservativeRasterProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT;
	deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
	deviceProps2.pNext = &conservativeRasterProps;
	vkGetPhysicalDeviceProperties2KHR(engine->_chosenGPU, &deviceProps2);

	VkPipelineRasterizationConservativeStateCreateInfoEXT conservativeRasterStateCI{};
	conservativeRasterStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;
	conservativeRasterStateCI.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
	conservativeRasterStateCI.extraPrimitiveOverestimationSize = conservativeRasterProps.maxExtraPrimitiveOverestimationSize;
	
	// Conservative rasterization state has to be chained into the pipeline rasterization state create info structure
	pipelineBuilder.set_next_rasterization_state(&conservativeRasterStateCI);
	
	//render format
	pipelineBuilder.set_color_attachment_format(info.imageFormat);
	//pipelineBuilder.set_depth_format(info.depthFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
	conservative_radiance_pipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, voxelization_frag_shader, nullptr);
	vkDestroyShaderModule(engine->_device, voxelization_vert_shader, nullptr);
	vkDestroyShaderModule(engine->_device, voxelization_geom_shader, nullptr);
}

void ConservativeVoxelizationPipelineObject::clear_resources(VkDevice device)
{
	vkDestroyPipelineLayout(device, conservative_radiance_pipeline.layout, nullptr);

	vkDestroyPipeline(device, conservative_radiance_pipeline.pipeline, nullptr);
}


void VoxelizationVisualizationPipelineObject::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule texture_visualization_frag;
	std::string assets_path = ENGINE_ASSET_PATH;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/voxel_cone_tracing/visualization/texture3DVisualization.frag.spv").c_str(), engine->_device, &texture_visualization_frag)) {
		std::println("Error when building the voxel visualization fragment shader module");
	}

	VkShaderModule texture_visualization_geom;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/voxel_cone_tracing/visualization/texture3DVisualization.geom.spv").c_str(), engine->_device, &texture_visualization_geom)) {
		std::println("Error when building the voxel visualization fragment shader module");
	}

	VkShaderModule texture_visualization_vert;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/voxel_cone_tracing/visualization/texture3DVisualization.vert.spv").c_str(), engine->_device, &texture_visualization_vert)) {
		std::println("Error when building the voxel visualization vertex shader module");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	DescriptorLayoutBuilder layoutBuilder;

	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 1;
	mesh_layout_info.pSetLayouts = info.layouts.data();
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	texture_3D_pipeline.layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(texture_visualization_vert, texture_visualization_frag, texture_visualization_geom);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_level(info.msaa_samples);
	pipelineBuilder.enable_blending_alphablend();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);


	//render format
	pipelineBuilder.set_color_attachment_format(info.imageFormat);
	pipelineBuilder.set_depth_format(info.depthFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
	texture_3D_pipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, texture_visualization_frag, nullptr);
	vkDestroyShaderModule(engine->_device, texture_visualization_geom, nullptr);
	vkDestroyShaderModule(engine->_device, texture_visualization_vert, nullptr);
}

void VoxelizationVisualizationPipelineObject::clear_resources(VkDevice device)
{
	vkDestroyPipelineLayout(device, texture_3D_pipeline.layout, nullptr);

	vkDestroyPipeline(device, texture_3D_pipeline.pipeline, nullptr);
}