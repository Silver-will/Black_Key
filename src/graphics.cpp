#include "graphics.h"
#include "camera.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_engine.h"
#include "vk_pipelines.h"
#include "vk_device.h"

#define M_PI       3.14159265358979323846
struct PushBlock {
	glm::mat4 mvp;
	VkDeviceAddress vertexBuffer;
	float roughness;
	uint32_t numSamples = 32u;
};

struct PushParams {
	glm::vec2 mips;
	float roughness;
};
void black_key::build_clusters(VulkanEngine* engine, PipelineCreationInfo& info, DescriptorAllocator& descriptorAllocator)
{
	/*
	VkPipelineLayoutCreateInfo ClusterLayoutInfo = {};
	ClusterLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	ClusterLayoutInfo.pNext = nullptr;
	ClusterLayoutInfo.pSetLayouts = info.layouts.data();
	ClusterLayoutInfo.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(clusterParams);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	ClusterLayoutInfo.pPushConstantRanges = &pushConstant;
	ClusterLayoutInfo.pushConstantRangeCount = 1;

	VkPipelineLayout buildClusterLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &ClusterLayoutInfo, nullptr, &buildClusterLayout));

	VkShaderModule buildClusterShader;
	if (!vkutil::load_shader_module("shaders/cluster_shader.spv", engine->_device, &buildClusterShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = buildClusterShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = buildClusterLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VkPipeline clusterPipeline;
	//default colors

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &clusterPipeline));

	VkDescriptorSet globalDescriptor = engine->globalDescriptorAllocator.allocate(engine->_device, info.layouts[0]);

	engine->immediate_submit([&](VkCommandBuffer cmd)
		{
			DescriptorWriter writer;
			writer.write_buffer(0, engine->ClusterValues.AABBVolumeGridSSBO.buffer, engine->ClusterValues.numClusters * sizeof(VolumeTileAABB), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			writer.write_buffer(1, engine->ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			writer.update_set(engine->_device, globalDescriptor);

			clusterParams clusterData;
			clusterData.zNear = engine->mainCamera.getNearClip();
			clusterData.zFar = engine->mainCamera.getFarClip();
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, clusterPipeline);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, buildClusterLayout, 0, 1, &globalDescriptor, 0, nullptr);

			vkCmdPushConstants(cmd, buildClusterLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(clusterParams), &clusterData);
			vkCmdDispatch(cmd, 16, 9, 24);
		});
	*/
}
bool black_key::is_visible(const RenderObject& obj, const glm::mat4& viewproj) {
	std::array<glm::vec3, 8> corners{
		glm::vec3 { 1, 1, 1 },
		glm::vec3 { 1, 1, -1 },
		glm::vec3 { 1, -1, 1 },
		glm::vec3 { 1, -1, -1 },
		glm::vec3 { -1, 1, 1 },
		glm::vec3 { -1, 1, -1 },
		glm::vec3 { -1, -1, 1 },
		glm::vec3 { -1, -1, -1 },
	};

	glm::mat4 matrix = viewproj * obj.transform;

	glm::vec3 min = { 1.5, 1.5, 1.5 };
	glm::vec3 max = { -1.5, -1.5, -1.5 };

	for (int c = 0; c < 8; c++) {
		// project each corner into clip space
		glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

		// perspective correction
		v.x = v.x / v.w;
		v.y = v.y / v.w;
		v.z = v.z / v.w;

		min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
		max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
	}

	// check the clip space box is within the view
	if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
		return false;
	}
	else {
		return true;
	}
}

void black_key::generate_irradiance_cube(VulkanEngine* engine, IBLData& ibl)
{
	/*
	//Created irradiance cubemap mage
	VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
	uint32_t dim = 64;
	uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	ibl._irradianceCube = vkutil::create_cubemap_image(VkExtent3D{ dim,dim,1 }, engine, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);

	//Create image sampler
	VkSamplerCreateInfo cubeSampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	cubeSampl.magFilter = VK_FILTER_LINEAR;
	cubeSampl.minFilter = VK_FILTER_LINEAR;
	cubeSampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	cubeSampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubeSampl.addressModeV = cubeSampl.addressModeU;
	cubeSampl.addressModeW = cubeSampl.addressModeU;
	cubeSampl.mipLodBias = 0.0f;
	cubeSampl.minLod = 0.0f;
	cubeSampl.maxLod = numMips;
	cubeSampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &ibl._irradianceCubeSampler);

	//Draw target Image
	AllocatedImage drawImage;
	VkExtent3D drawImageExtent{
		dim,
		dim,
		1
	};
	drawImage.imageExtent = drawImageExtent;
	drawImage.imageFormat = format;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent, 1);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(engine->_allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);
	vmaSetAllocationName(engine->_allocator, drawImage.allocation, "irradiance pass image");

	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	VK_CHECK(vkCreateImageView(engine->_device, &view_info, nullptr, &drawImage.imageView));

	VkDescriptorSetLayout irradianceSetLayout;
	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	irradianceSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);

	//Pipeline setup
	VkShaderModule irradianceVertexShader;
	if (!vkutil::load_shader_module("shaders/filter_cube.vert.spv", engine->_device, &irradianceVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule irradianceFragmentShader;
	if (!vkutil::load_shader_module("shaders/irradiance_cube.frag.spv", engine->_device, &irradianceFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PushBlock);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayout layouts[] = {irradianceSetLayout};

	VkPipelineLayoutCreateInfo irradiance_layout_info = vkinit::pipeline_layout_create_info();
	irradiance_layout_info.setLayoutCount = 1;
	irradiance_layout_info.pSetLayouts = layouts;
	irradiance_layout_info.pPushConstantRanges = &matrixRange;
	irradiance_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout irradianceLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &irradiance_layout_info, nullptr, &irradianceLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(irradianceVertexShader, irradianceFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder._pipelineLayout = irradianceLayout;
	pipelineBuilder.set_color_attachment_format(drawImage.imageFormat);

	auto irradiancePipeline = pipelineBuilder.build_pipeline(engine->_device);

	std::vector<glm::mat4> matrices = {
		// POSITIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
	};
	engine->loadedScenes["cube"]->Draw(glm::mat4{ 1.f }, engine->skyDrawCommands);

	//begin drawing

	VkDescriptorSet globalDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, irradianceSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, engine->_skyImage.imageView, engine->cubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, globalDescriptor);

	auto cmd = vk_device::create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY,engine->_frames[0]._commandPool,engine);

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//> draw_first
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, engine->IBL._irradianceCube.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(drawImage.imageView,nullptr, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(VkExtent2D{ dim,dim }, &colorAttachment,nullptr);

	auto r = engine->skyDrawCommands.OpaqueSurfaces[0];
	
	{
		VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
		
		for (uint32_t m = 0; m < numMips; m++) {
			for (uint32_t f = 0; f < 6; f++)
			{
				vkCmdBeginRendering(cmd, &renderInfo);

				VkViewport viewport = {};
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmd, 0, 1, &viewport);

				VkRect2D scissor = {};
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = dim;
				scissor.extent.height = dim;
				vkCmdSetScissor(cmd, 0, 1, &scissor);

				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, irradiancePipeline);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, irradianceLayout, 0, 1, &globalDescriptor, 0, NULL);

				if (r.indexBuffer != lastIndexBuffer)
				{
					lastIndexBuffer = r.indexBuffer;
					vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
				}
				
				PushBlock pushBlock;
				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
				pushBlock.mvp[1][1] *= -1;
				pushBlock.vertexBuffer = r.vertexBufferAddress;
				vkCmdPushConstants(cmd, irradianceLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushBlock), &pushBlock);
				vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);

				vkCmdEndRendering(cmd);
				vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

				blitRegion.srcOffsets[1].x = viewport.width;
				blitRegion.srcOffsets[1].y = viewport.height;
				blitRegion.srcOffsets[1].z = 1;

				blitRegion.dstOffsets[1].x = viewport.width;
				blitRegion.dstOffsets[1].y = viewport.height;
				blitRegion.dstOffsets[1].z = 1;

				blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blitRegion.srcSubresource.baseArrayLayer = 0;
				blitRegion.srcSubresource.layerCount = 1;
				blitRegion.srcSubresource.mipLevel = 0;

				blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blitRegion.dstSubresource.baseArrayLayer = f;
				blitRegion.dstSubresource.layerCount = 1;
				blitRegion.dstSubresource.mipLevel = m;
				VkExtent2D copyExtent{
					dim,
					dim
				};
				vkutil::copy_image_to_image(cmd, drawImage.image, engine->IBL._irradianceCube.image,copyExtent,copyExtent,&blitRegion);
				vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}
		engine->skyDrawCommands.OpaqueSurfaces.clear();

	}
	vkutil::transition_image(cmd, engine->IBL._irradianceCube.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vk_device::flush_command_buffer(cmd, engine->_graphicsQueue, engine->_frames[0]._commandPool, engine);

	vkDestroyImage(engine->_device, drawImage.image, nullptr);
	vkDestroyImageView(engine->_device, drawImage.imageView, nullptr);
	vkDestroyPipeline(engine->_device, irradiancePipeline, nullptr);
	vkDestroyPipelineLayout(engine->_device, irradianceLayout, nullptr);
	vkDestroyDescriptorSetLayout(engine->_device, irradianceSetLayout, nullptr);
	
	*/
	//Compute shader version
	//Completely identical visuals
	/*VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
	uint32_t dim = 64;
	uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	engine->IBL._irradianceCube = vkutil::create_cubemap_image(VkExtent3D{ dim,dim,1 }, engine, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, true);


	VkSamplerCreateInfo cubeSampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	cubeSampl.magFilter = VK_FILTER_LINEAR;
	cubeSampl.minFilter = VK_FILTER_LINEAR;
	cubeSampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	cubeSampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubeSampl.addressModeV = cubeSampl.addressModeU;
	cubeSampl.addressModeW = cubeSampl.addressModeU;
	cubeSampl.mipLodBias = 0.0f;
	cubeSampl.minLod = 0.0f;
	cubeSampl.maxLod = numMips;
	cubeSampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &engine->IBL._irradianceCubeSampler); 

	VkDescriptorSetLayout IBL_Layout;
	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	IBL_Layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkPipelineLayoutCreateInfo irradianceLayoutInfo = {};
	irradianceLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	irradianceLayoutInfo.pNext = nullptr;
	irradianceLayoutInfo.pSetLayouts = &IBL_Layout;
	irradianceLayoutInfo.setLayoutCount = 1;

	VkPipelineLayout irradianceLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &irradianceLayoutInfo, nullptr, &irradianceLayout));

	VkShaderModule irradianceShader;
	if (!vkutil::load_shader_module("shaders/irradiance_cube.spv", engine->_device, &irradianceShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = irradianceShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = irradianceLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VkPipeline preFilterPipeline;
	//default colors

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &preFilterPipeline));

	VkDescriptorSet globalDescriptor = engine->globalDescriptorAllocator.allocate(engine->_device, IBL_Layout);

	auto cmd = vk_device::create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, engine->_frames[0]._commandPool, engine);
	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//> draw_first
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
	vkutil::transition_image(cmd, engine->_skyImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, engine->IBL._irradianceCube.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	DescriptorWriter writer;
	writer.write_image(0, engine->_skyImage.imageView, engine->_cubeMapSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, engine->IBL._irradianceCube.imageView, engine->IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(engine->_device, globalDescriptor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, preFilterPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, irradianceLayout, 0, 1, &globalDescriptor,0,nullptr);

	//vkCmdPushConstants(cmd, preFilterLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(PushParams),&)
	vkCmdDispatch(cmd, dim / 16, dim / 16, 6);

	vkutil::transition_image(cmd, engine->IBL._irradianceCube.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkutil::transition_image(cmd, engine->_skyImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::generate_mipmaps(cmd, engine->IBL._irradianceCube.image, VkExtent2D{ dim,dim },6);

	vk_device::flush_command_buffer(cmd, engine->_graphicsQueue, engine->_frames[0]._commandPool, engine);
	*/
}

void black_key::generate_brdf_lut(VulkanEngine* engine, IBLData& ibl)
{
	VkFormat format = VK_FORMAT_R16G16_SFLOAT;
	uint32_t dim = 512;
	uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	ibl._lutBRDF = vkutil::create_image_empty(VkExtent3D{ dim,dim,1 }, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,engine);

	//Create image sampler
	VkSamplerCreateInfo cubeSampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	cubeSampl.magFilter = VK_FILTER_LINEAR;
	cubeSampl.minFilter = VK_FILTER_LINEAR;
	cubeSampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	cubeSampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubeSampl.addressModeV = cubeSampl.addressModeU;
	cubeSampl.addressModeW = cubeSampl.addressModeU;
	cubeSampl.mipLodBias = 0.0f;
	cubeSampl.minLod = 0.0f;
	cubeSampl.maxLod = 1.0f;
	cubeSampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &ibl._lutBRDFSampler);

	VkDescriptorSetLayout lutBRDFSetLayout;
	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	lutBRDFSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);

	//Pipeline setup
	VkShaderModule brdfLutVertexShader;
	if (!vkutil::load_shader_module("shaders/gen_brdf_lut.vert.spv", engine->_device, &brdfLutVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule brdfLutFragmentShader;
	if (!vkutil::load_shader_module("shaders/gen_brdf_lut.frag.spv", engine->_device, &brdfLutFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}

	VkDescriptorSetLayout layouts[] = { lutBRDFSetLayout };

	VkPipelineLayoutCreateInfo lut_brdf_layout_info = vkinit::pipeline_layout_create_info();
	lut_brdf_layout_info.setLayoutCount = 1;
	lut_brdf_layout_info.pSetLayouts = layouts;
	lut_brdf_layout_info.pPushConstantRanges = nullptr;
	lut_brdf_layout_info.pushConstantRangeCount = 0;

	VkPipelineLayout lutBRDFLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &lut_brdf_layout_info, nullptr, &lutBRDFLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(brdfLutVertexShader, brdfLutFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder._pipelineLayout = lutBRDFLayout;
	pipelineBuilder.set_color_attachment_format(ibl._lutBRDF.imageFormat);

	auto brdfPipeline = pipelineBuilder.build_pipeline(engine->_device);

	VkClearValue clearValues[1];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	
	engine->immediate_submit([&](VkCommandBuffer cmd) {

		vkutil::transition_image(cmd, ibl._lutBRDF.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(ibl._lutBRDF.imageView, nullptr, clearValues, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkRenderingInfo renderInfo = vkinit::rendering_info(VkExtent2D{ dim,dim }, &colorAttachment, nullptr);

		vkCmdBeginRendering(cmd, &renderInfo);
		VkViewport viewport = {};
		viewport.width = dim;
		viewport.height = dim;

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = dim;
		scissor.extent.height = dim;

		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, brdfPipeline);
		vkCmdDraw(cmd, 3, 1, 0, 0);
		vkCmdEndRendering(cmd);

		vkutil::transition_image(cmd, ibl._lutBRDF.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});
	
	/*auto cmd = vk_device::create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, engine->_frames[0]._commandPool, engine);
	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	//> draw_first
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	vkutil::transition_image(cmd, engine->IBL._lutBRDF.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(engine->IBL._lutBRDF.imageView, nullptr, clearValues, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(VkExtent2D{ dim,dim }, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);
	VkViewport viewport = {};
	viewport.width = dim;
	viewport.height = dim;

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = dim;
	scissor.extent.height = dim;

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, brdfPipeline);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	vkCmdEndRendering(cmd);

	vkutil::transition_image(cmd, engine->IBL._lutBRDF.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	vk_device::flush_command_buffer(cmd, engine->_graphicsQueue, engine->_frames[0]._commandPool, engine);
	*/

	vkDestroyPipeline(engine->_device, brdfPipeline, nullptr);
	vkDestroyPipelineLayout(engine->_device, lutBRDFLayout, nullptr);
	vkDestroyDescriptorSetLayout(engine->_device, lutBRDFSetLayout, nullptr);
}

void black_key::generate_prefiltered_cubemap(VulkanEngine* engine, IBLData& ibl)
{
	/*
	VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
	uint32_t dim = 512;
	uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	ibl._preFilteredCube = vkutil::create_cubemap_image(VkExtent3D{ dim,dim,1 }, engine, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);

	//Draw target Image
	AllocatedImage drawImage;
	VkExtent3D drawImageExtent{
		dim,
		dim,
		1
	};
	drawImage.imageExtent = drawImageExtent;
	drawImage.imageFormat = format;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent, 1);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(engine->_allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);
	vmaSetAllocationName(engine->_allocator, drawImage.allocation, "Pre filtered cube draw image");

	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	VK_CHECK(vkCreateImageView(engine->_device, &view_info, nullptr, &drawImage.imageView));

	VkDescriptorSetLayout irradianceSetLayout;
	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	irradianceSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);

	//Pipeline setup
	VkShaderModule preFilterVertexShader;
	if (!vkutil::load_shader_module("shaders/filter_cube.vert.spv", engine->_device, &preFilterVertexShader)) {
		fmt::print("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule preFilterFragmentShader;
	if (!vkutil::load_shader_module("shaders/pre_filter_envmap.frag.spv", engine->_device, &preFilterFragmentShader)) {
		fmt::print("Error when building the shadow fragment shader module\n");
	}


	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PushBlock);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayout layouts[] = { irradianceSetLayout };

	VkPipelineLayoutCreateInfo irradiance_layout_info = vkinit::pipeline_layout_create_info();
	irradiance_layout_info.setLayoutCount = 1;
	irradiance_layout_info.pSetLayouts = layouts;
	irradiance_layout_info.pPushConstantRanges = &matrixRange;
	irradiance_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout irradianceLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &irradiance_layout_info, nullptr, &irradianceLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(preFilterVertexShader, preFilterFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder._pipelineLayout = irradianceLayout;
	pipelineBuilder.set_color_attachment_format(drawImage.imageFormat);

	auto irradiancePipeline = pipelineBuilder.build_pipeline(engine->_device);

	std::vector<glm::mat4> matrices = {
		// POSITIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
	};
	engine->loadedScenes["cube"]->Draw(glm::mat4{ 1.f }, engine->skyDrawCommands);

	VkDescriptorSet globalDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, irradianceSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, engine->_skyImage.imageView, engine->IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, globalDescriptor);

	//begin drawing
	auto cmd = vk_device::create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, engine->_frames[0]._commandPool, engine);

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//> draw_first
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, engine->IBL._preFilteredCube.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(drawImage.imageView, nullptr, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(VkExtent2D{ dim,dim }, &colorAttachment, nullptr);

	VkViewport viewport = {};
	viewport.width = dim;
	viewport.height = dim;
	
	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = dim;
	scissor.extent.height = dim;

	auto r = engine->skyDrawCommands.OpaqueSurfaces[0];
	//vkCmdBeginRendering(cmd, &renderInfo);
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	{
		VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
		PushBlock pushBlock;
		for (uint32_t m = 0; m < numMips; m++) {
			pushBlock.roughness = (float)m / (float)(numMips - 1);
			for (uint32_t f = 0; f < 6; f++)
			{
				vkCmdBeginRendering(cmd, &renderInfo);
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmd, 0, 1, &viewport);
				
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, irradiancePipeline);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, irradianceLayout, 0, 1, &globalDescriptor, 0, NULL);

				if (r.indexBuffer != lastIndexBuffer)
				{
					lastIndexBuffer = r.indexBuffer;
					vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
				}
				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
				pushBlock.mvp[1][1] *= -1;
				pushBlock.vertexBuffer = r.vertexBufferAddress;
				vkCmdPushConstants(cmd, irradianceLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);
				
				vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
				vkCmdEndRendering(cmd);
	
				vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

				blitRegion.srcOffsets[1].x = viewport.width;
				blitRegion.srcOffsets[1].y = viewport.height;
				blitRegion.srcOffsets[1].z = 1;

				blitRegion.dstOffsets[1].x = viewport.width;
				blitRegion.dstOffsets[1].y = viewport.height;
				blitRegion.dstOffsets[1].z = 1;

				blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blitRegion.srcSubresource.baseArrayLayer = 0;
				blitRegion.srcSubresource.layerCount = 1;
				blitRegion.srcSubresource.mipLevel = 0;

				blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blitRegion.dstSubresource.baseArrayLayer = f;
				blitRegion.dstSubresource.layerCount = 1;
				blitRegion.dstSubresource.mipLevel = m;
				VkExtent2D copyExtent{
					dim,
					dim
				};

				vkutil::copy_image_to_image(cmd, drawImage.image, engine->IBL._preFilteredCube.image, copyExtent, copyExtent, &blitRegion);
				vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}
		engine->skyDrawCommands.OpaqueSurfaces.clear();
	}
	vkutil::transition_image(cmd, engine->IBL._preFilteredCube.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vk_device::flush_command_buffer(cmd, engine->_graphicsQueue, engine->_frames[0]._commandPool, engine);
	vkDestroyImage(engine->_device, drawImage.image, nullptr);
	vkDestroyImageView(engine->_device, drawImage.imageView, nullptr);
	vkDestroyPipeline(engine->_device, irradiancePipeline, nullptr);
	vkDestroyPipelineLayout(engine->_device, irradianceLayout, nullptr);
	vkDestroyDescriptorSetLayout(engine->_device, irradianceSetLayout, nullptr);
	*/
}