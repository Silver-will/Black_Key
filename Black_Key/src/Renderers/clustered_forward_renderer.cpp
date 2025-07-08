#include "clustered_forward_renderer.h"
#include "../graphics.h"
#include "../UI.h"

#include <VkBootstrap.h>

#include <chrono>
#include <thread>
#include <iostream>
#include <random>

#include "../../../tracy/public/tracy/Tracy.hpp"

#include <string>
#include <glm/glm.hpp>
using namespace std::literals::string_literals;

#include <vma/vk_mem_alloc.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>


void ClusteredForwardRenderer::Init(VulkanEngine* engine)
{
	mainCamera.type = Camera::CameraType::firstperson;
	//mainCamera.flipY = true;
	mainCamera.movementSpeed = 2.5f;
	mainCamera.setPerspective(60.0f, (float)_windowExtent.width / (float)_windowExtent.height, 1.0f, 300.0f);
	mainCamera.setPosition(glm::vec3(-0.12f, -5.14f, -2.25f));
	mainCamera.setRotation(glm::vec3(-17.0f, 7.0f, 0.0f));
	// only one engine initialization is allowed with the application.
	assert(loaded_engine == nullptr);
	this->loaded_engine = engine;

	glfwSetWindowUserPointer(loaded_engine->window, this);
	glfwSetFramebufferSizeCallback(loaded_engine->window, framebuffer_resize_callback);
	glfwSetKeyCallback(loaded_engine->window, KeyCallback);
	glfwSetCursorPosCallback(loaded_engine->window, CursorCallback);
	glfwSetInputMode(loaded_engine->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	InitSwapchain();

	InitRenderTargets();

	InitCommands();

	InitSyncStructures();

	InitDescriptors();

	InitDefaultData();

	InitBuffers();

	InitPipelines();

	InitImgui();

	LoadAssets();

	PreProcessPass();
	_isInitialized = true;
}

void ClusteredForwardRenderer::InitEngine()
{
	//vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features.dynamicRendering = true;
	features.synchronization2 = true;
	//vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;
	features12.runtimeDescriptorArray = true;
	features12.descriptorBindingPartiallyBound = true;
	features12.descriptorBindingSampledImageUpdateAfterBind = true;
	features12.descriptorBindingUniformBufferUpdateAfterBind = true;
	features12.descriptorBindingStorageImageUpdateAfterBind = true;
	features12.shaderSampledImageArrayNonUniformIndexing = true;
	features12.descriptorBindingUpdateUnusedWhilePending = true;
	features12.descriptorBindingVariableDescriptorCount = true;
	features12.samplerFilterMinmax = true;


	VkPhysicalDeviceVulkan11Features features11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	features11.shaderDrawParameters = true;

	VkPhysicalDeviceFeatures baseFeatures{};
	baseFeatures.geometryShader = true;
	baseFeatures.samplerAnisotropy = true;
	baseFeatures.sampleRateShading = true;
	baseFeatures.drawIndirectFirstInstance = true;
	baseFeatures.multiDrawIndirect = true;

	loaded_engine->init_vulkan(baseFeatures, features11, features12, features);
}

void ClusteredForwardRenderer::InitSwapchain()
{
	CreateSwapchain(_windowExtent.width, _windowExtent.height);
}

void ClusteredForwardRenderer::InitRenderTargets()
{
	VkExtent3D drawImageExtent = {
	_windowExtent.width,
	_windowExtent.height,
	1
	};

	//Allocate images larger than swapchain to avoid 
	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	/*VkExtent3D drawImageExtent = {
		mode->width,
		mode->height,
		1
	};*/

	//hardcoding the draw format to 16 bit float
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage.imageExtent = drawImageExtent;

	_resolveImage = _drawImage;
	_hdrImage = _drawImage;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	VkImageUsageFlags resolveImageUsages{};
	resolveImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	resolveImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	resolveImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	resolveImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;


	VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent, 1, msaa_samples);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);
	vmaSetAllocationName(_allocator, _drawImage.allocation, "Draw image");

	//Create resolve image for multisampling
	VkImageCreateInfo resolve_img_info = vkinit::image_create_info(_resolveImage.imageFormat, resolveImageUsages, drawImageExtent, 1);
	vmaCreateImage(_allocator, &resolve_img_info, &rimg_allocinfo, &_resolveImage.image, &_resolveImage.allocation, nullptr);
	vmaSetAllocationName(_allocator, _resolveImage.allocation, "resolve image");

	vmaCreateImage(_allocator, &resolve_img_info, &rimg_allocinfo, &_hdrImage.image, &_hdrImage.allocation, nullptr);
	vmaSetAllocationName(_allocator, _hdrImage.allocation, "hdr image");

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	VkImageViewCreateInfo resolve_view_info = vkinit::imageview_create_info(_resolveImage.imageFormat, _resolveImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	VkImageViewCreateInfo hdr_view_info = vkinit::imageview_create_info(_hdrImage.imageFormat, _hdrImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));
	VK_CHECK(vkCreateImageView(_device, &resolve_view_info, nullptr, &_resolveImage.imageView));
	VK_CHECK(vkCreateImageView(_device, &hdr_view_info, nullptr, &_hdrImage.imageView));


	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo dresolve_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages | VK_IMAGE_USAGE_SAMPLED_BIT, drawImageExtent, 1);

	//allocate and create the image
	vmaCreateImage(_allocator, &dresolve_info, &rimg_allocinfo, &_depthResolveImage.image, &_depthResolveImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo dRview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthResolveImage.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(_device, &dRview_info, nullptr, &_depthResolveImage.imageView));

	depthImageUsages |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent, 1, msaa_samples);

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

	//add to deletion queues
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

		vkDestroyImageView(_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);

		vkDestroyImageView(_device, _resolveImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _resolveImage.image, _resolveImage.allocation);

		vkDestroyImageView(_device, _hdrImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _hdrImage.image, _hdrImage.allocation);

		vkDestroyImageView(_device, _depthResolveImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _depthResolveImage.image, _depthResolveImage.allocation);
		});
}


void ClusteredForwardRenderer::InitCommands()
{
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(loaded_engine->_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(loaded_engine->_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(loaded_engine->_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		loaded_engine->_mainDeletionQueue.push_function([=]() { vkDestroyCommandPool(loaded_engine->_device, _frames[i]._commandPool, nullptr); });
	}
}

void ClusteredForwardRenderer::InitSyncStructures()
{
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateFence(loaded_engine->_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

		VK_CHECK(vkCreateSemaphore(loaded_engine->_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(loaded_engine->_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		loaded_engine->_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(loaded_engine->_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(loaded_engine->_device, _frames[i]._swapchainSemaphore, nullptr);
			vkDestroySemaphore(loaded_engine->_device, _frames[i]._renderSemaphore, nullptr);
			});
	}
}

void ClusteredForwardRenderer::InitDescriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
	};

	globalDescriptorAllocator.init_pool(_device, 30, sizes);
	_mainDeletionQueue.push_function(
		[&]() { vkDestroyDescriptorPool(_device, globalDescriptorAllocator.pool, nullptr); });


	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);

	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		gpu_scene_data_descriptor_layout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		cascaded_shadows_descriptor_layout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_skyboxDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		_cullLightsDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		_buildClustersDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 65536);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 65536);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 65536);
		bindless_descriptor_layout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		compute_cull_descriptor_layout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		depth_reduce_descriptor_layout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
	}

	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, gpu_scene_data_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _skyboxDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, cascaded_shadows_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _cullLightsDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _buildClustersDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, bindless_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(_device, compute_cull_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(_device, depth_reduce_descriptor_layout, nullptr);
		});

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor 
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		std::vector<DescriptorAllocator::PoolSizeRatio> bindless_sizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
		_frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);
		_frames[i].bindless_material_descriptor = DescriptorAllocator{};
		_frames[i].bindless_material_descriptor.init_pool(_device, 65536, bindless_sizes);
		_mainDeletionQueue.push_function([&, i]() {
			_frames[i]._frameDescriptors.destroy_pools(_device);
			_frames[i].bindless_material_descriptor.destroy_pool(_device);
			});
	}
}


void ClusteredForwardRenderer::InitPipelines()
{
	metalRoughMaterial.build_pipelines(loaded_engine);
	cascadedShadows.build_pipelines(loaded_engine);
	skyBoxPSO.build_pipelines(loaded_engine);
	HdrPSO.build_pipelines(loaded_engine);
	depthPrePassPSO.build_pipelines(loaded_engine);
	_mainDeletionQueue.push_function([&]()
		{
			depthPrePassPSO.clear_resources(_device);
			metalRoughMaterial.clear_resources(_device);
			cascadedShadows.clear_resources(_device);
			skyBoxPSO.clear_resources(_device);
			HdrPSO.clear_resources(_device);
		});

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _shadowDepthImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _shadowDepthImage.image, _shadowDepthImage.allocation);
		});
}


void ClusteredForwardRenderer::InitComputePipelines()
{
	VkPipelineLayoutCreateInfo cullLightsLayoutInfo = {};
	cullLightsLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	cullLightsLayoutInfo.pNext = nullptr;
	cullLightsLayoutInfo.pSetLayouts = &_cullLightsDescriptorLayout;
	cullLightsLayoutInfo.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(CullData);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	cullLightsLayoutInfo.pPushConstantRanges = &pushConstant;
	cullLightsLayoutInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &cullLightsLayoutInfo, nullptr, &_cullLightsPipelineLayout));

	VkShaderModule cullLightShader;
	if (!vkutil::load_shader_module("shaders/cluster_cull_light_shader.spv", _device, &cullLightShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = cullLightShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _cullLightsPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_cullLightsPipeline));


	VkPipelineLayoutCreateInfo cullObjectsLayoutInfo = {};
	cullObjectsLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	cullObjectsLayoutInfo.pNext = nullptr;
	cullObjectsLayoutInfo.pSetLayouts = &compute_cull_descriptor_layout;
	cullObjectsLayoutInfo.setLayoutCount = 1;

	pushConstant.size = sizeof(vkutil::DrawCullData);

	cullObjectsLayoutInfo.pPushConstantRanges = &pushConstant;
	cullObjectsLayoutInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &cullObjectsLayoutInfo, nullptr, &cull_objects_pso.layout));

	VkShaderModule cullObjectsShader;
	if (!vkutil::load_shader_module("shaders/indirect_cull.comp.spv", _device, &cullObjectsShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkPipelineShaderStageCreateInfo stageinfoObj{};
	stageinfoObj.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfoObj.pNext = nullptr;
	stageinfoObj.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfoObj.module = cullObjectsShader;
	stageinfoObj.pName = "main";

	computePipelineCreateInfo.layout = cull_objects_pso.layout;
	computePipelineCreateInfo.stage = stageinfoObj;

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &cull_objects_pso.pipeline));

	VkPipelineLayoutCreateInfo depthReduceLayoutInfo = {};
	depthReduceLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	depthReduceLayoutInfo.pNext = nullptr;
	depthReduceLayoutInfo.pSetLayouts = &depth_reduce_descriptor_layout;
	depthReduceLayoutInfo.setLayoutCount = 1;

	pushConstant.size = sizeof(glm::vec2);
	depthReduceLayoutInfo.pPushConstantRanges = &pushConstant;
	depthReduceLayoutInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &depthReduceLayoutInfo, nullptr, &depth_reduce_pso.layout));

	VkShaderModule depthReduceShader;
	if (!vkutil::load_shader_module("shaders/depth_reduce.comp.spv", _device, &depthReduceShader)) {
		fmt::print("Error when building the compute shader \n");
	}


	VkPipelineShaderStageCreateInfo depthReduceStageinfo{};
	depthReduceStageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	depthReduceStageinfo.pNext = nullptr;
	depthReduceStageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	depthReduceStageinfo.module = depthReduceShader;
	depthReduceStageinfo.pName = "main";

	VkComputePipelineCreateInfo depthComputePipelineCreateInfo{};
	depthComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	depthComputePipelineCreateInfo.pNext = nullptr;
	depthComputePipelineCreateInfo.layout = depth_reduce_pso.layout;
	depthComputePipelineCreateInfo.stage = depthReduceStageinfo;

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &depthComputePipelineCreateInfo, nullptr, &depth_reduce_pso.pipeline));



	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(_device, depth_reduce_pso.layout, nullptr);
		vkDestroyPipeline(_device, depth_reduce_pso.pipeline, nullptr);
		vkDestroyPipelineLayout(_device, cull_objects_pso.layout, nullptr);
		vkDestroyPipeline(_device, cull_objects_pso.pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _cullLightsPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _cullLightsPipeline, nullptr);
		});
}

void ClusteredForwardRenderer::InitDefaultData()
{
	forward_passes.push_back(vkutil::MaterialPass::forward);
	forward_passes.push_back(vkutil::MaterialPass::transparency);

	directLight = DirectionalLight(glm::normalize(glm::vec4(-20.0f, -50.0f, -20.0f, 1.f)), glm::vec4(1.5f), glm::vec4(1.0f));
	//Create Shadow render target
	_shadowDepthImage = vkutil::create_image_empty(VkExtent3D(shadowMapSize, shadowMapSize, 1), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, this, VK_IMAGE_VIEW_TYPE_2D_ARRAY, false, shadows.getCascadeLevels());
	shadows.SetShadowMapTextureSize(shadowMapSize);

	//Create default images
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = vkutil::create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, this);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = vkutil::create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, this);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = vkutil::create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, this);

	storageImage = vkutil::create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, this);

	depthPyramidWidth = BlackKey::PreviousPow2(_windowExtent.width);
	depthPyramidHeight = BlackKey::PreviousPow2(_windowExtent.height);
	depthPyramidLevels = BlackKey::GetImageMipLevels(depthPyramidWidth, depthPyramidHeight);

	_depthPyramid = vkutil::create_image_empty(VkExtent3D(depthPyramidWidth, depthPyramidHeight, 1), VK_FORMAT_R32_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		this, VK_IMAGE_VIEW_TYPE_2D, true, 1, VK_SAMPLE_COUNT_1_BIT, depthPyramidLevels);

	for (int i = 0; i < depthPyramidLevels; i++)
	{

		VkImageViewCreateInfo level_info = vkinit::imageview_create_info(VK_FORMAT_R32_SFLOAT, _depthPyramid.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
		level_info.subresourceRange.levelCount = 1;
		level_info.subresourceRange.baseMipLevel = i;

		VkImageView pyramid;
		vkCreateImageView(_device, &level_info, nullptr, &pyramid);

		depthPyramidMips[i] = pyramid;
		assert(depthPyramidMips[i]);
	}

	//Populate point light list
	int numOfLights = 4;
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_real_distribution<> distFloat(0.0f, 15.0f);
	for (int i = 0; i < numOfLights; i++)
	{
		pointData.pointLights.push_back(PointLight(glm::vec4(distFloat(rng), 5.0f, distFloat(rng), 1.0f), glm::vec4(1), 12.0f, 1.0f));
	}
	pointData.pointLights.push_back(PointLight(glm::vec4(-257.0f, 130.0f, 5.25f, -256.0f), glm::vec4(1), 15.0f, 1.0f));
	pointData.pointLights.push_back(PointLight(glm::vec4(-0.12f, -5.14f, -5.25f, 1.0f), glm::vec4(1), 15.0f, 1.0f));



	//Prepare Depth Pyramid


	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	errorCheckerboardImage = vkutil::create_image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, this);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_device, &sampl, nullptr, &defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampl, nullptr, &defaultSamplerLinear);

	VkSamplerCreateInfo cubeSampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	cubeSampl.magFilter = VK_FILTER_LINEAR;
	cubeSampl.minFilter = VK_FILTER_LINEAR;
	cubeSampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	cubeSampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubeSampl.addressModeV = cubeSampl.addressModeU;
	cubeSampl.addressModeW = cubeSampl.addressModeU;
	cubeSampl.mipLodBias = 0.0f;
	//cubeSampl.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
	//samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
	cubeSampl.compareOp = VK_COMPARE_OP_NEVER;
	cubeSampl.minLod = 0.0f;
	cubeSampl.maxLod = (float)11;
	cubeSampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(_device, &cubeSampl, nullptr, &cubeMapSampler);

	cubeSampl.maxLod = 1;
	cubeSampl.maxAnisotropy = 1.0f;
	vkCreateSampler(_device, &cubeSampl, nullptr, &depthSampler);

	VkSamplerCreateInfo depthReductionSampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	auto reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;

	depthReductionSampl = cubeSampl;
	depthReductionSampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	depthReductionSampl.minLod = 0.0f;
	depthReductionSampl.maxLod = 16.0f;

	VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

	if (reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT)
	{
		createInfoReduction.reductionMode = reductionMode;

		depthReductionSampl.pNext = &createInfoReduction;
	}

	vkCreateSampler(_device, &depthReductionSampl, nullptr, &depthReductionSampler);

	//< default_img

	_mainDeletionQueue.push_function([=]() {
		destroy_image(_whiteImage);
		destroy_image(_blackImage);
		destroy_image(_greyImage);
		destroy_image(errorCheckerboardImage);
		destroy_image(_skyImage);
		destroy_image(_shadowDepthImage);
		vkDestroySampler(_device, depthReductionSampler, nullptr);
		vkDestroySampler(_device, defaultSamplerLinear, nullptr);
		vkDestroySampler(_device, defaultSamplerNearest, nullptr);
		vkDestroySampler(_device, cubeMapSampler, nullptr);
		vkDestroySampler(_device, depthSampler, nullptr);
		});
}

/*
void ClusteredForwardRenderer::CreateSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ loaded_engine->_chosenGPU,loaded_engine->_device,loaded_engine->_surface };

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();


	_swapchainExtent = vkbSwapchain.extent;
	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
}


void ClusteredForwardRenderer::InitBuffers()
{
	ClusterValues.AABBVolumeGridSSBO = loaded_engine->create_buffer(ClusterValues.numClusters * sizeof(VolumeTileAABB), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	float zNear = mainCamera.getNearClip();
	float zFar = mainCamera.getFarClip();

	ClusterValues.sizeX = (uint16_t)std::ceilf(_aspect_width / (float)ClusterValues.gridSizeX);
	ScreenToView screen;
	auto proj = mainCamera.matrices.perspective;
	//proj[1][1] *= -1;
	screen.inverseProjectionMat = glm::inverse(proj);
	//screen.inverseProjectionMat[1][1] *= -1;
	screen.tileSizes[0] = ClusterValues.gridSizeX;
	screen.tileSizes[1] = ClusterValues.gridSizeY;
	screen.tileSizes[2] = ClusterValues.gridSizeZ;
	screen.tileSizes[3] = ClusterValues.sizeX;
	screen.screenWidth = _aspect_width;
	screen.screenHeight = _aspect_height;

	screen.sliceScalingFactor = (float)ClusterValues.gridSizeZ / std::log2f(zFar / zNear);
	screen.sliceBiasFactor = -((float)ClusterValues.gridSizeZ * std::log2f(zNear) / std::log2f(zFar / zNear));

	ClusterValues.screenToViewSSBO = loaded_engine->create_and_upload(sizeof(ScreenToView), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, &screen);

	ClusterValues.lightSSBO = loaded_engine->create_and_upload(pointData.pointLights.size() * sizeof(PointLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, pointData.pointLights.data());
	auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;
	ClusterValues.lightIndexListSSBO = loaded_engine->create_buffer(sizeof(uint32_t) * totalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	ClusterValues.lightGridSSBO = loaded_engine->create_buffer(ClusterValues.numClusters * sizeof(LightGrid), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	uint32_t val = 0;
	for (uint32_t i = 0; i < 2; i++)
	{
		ClusterValues.lightGlobalIndex[i] = loaded_engine->create_and_upload(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, &val);
	}
	//ClusterValues.lightIndexGlobalCountSSBO = create_and_upload(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,  VMA_MEMORY_USAGE_GPU_ONLY,&val);

	loaded_engine->_mainDeletionQueue.push_function([=]() {
		loaded_engine->destroy_buffer(ClusterValues.lightSSBO);
		loaded_engine->destroy_buffer(ClusterValues.lightGridSSBO);
		loaded_engine->destroy_buffer(ClusterValues.screenToViewSSBO);
		loaded_engine->destroy_buffer(ClusterValues.AABBVolumeGridSSBO);
		loaded_engine->destroy_buffer(ClusterValues.lightIndexListSSBO);
		loaded_engine->destroy_buffer(ClusterValues.lightGlobalIndex[0]);
		loaded_engine->destroy_buffer(ClusterValues.lightGlobalIndex[1]);

		});
}

void ClusteredForwardRenderer::InitImgui()
{

	//// 1: create descriptor pool for IMGUI
	////  the size of the pool is very oversize, but it's copied from imgui demo
	////  itself.
	//VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
	//	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
	//	{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
	//	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
	//	{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
	//	{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
	//	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
	//	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
	//	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
	//	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
	//	{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 } };

	//VkDescriptorPoolCreateInfo pool_info = {};
	//pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	//pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	//pool_info.maxSets = 1000;
	//pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	//pool_info.pPoolSizes = pool_sizes;

	//VkDescriptorPool imguiPool;
	//VK_CHECK(vkCreateDescriptorPool(loaded_engine->_device, &pool_info, nullptr, &imguiPool));

	//// 2: initialize imgui library

	//// this initializes the core structures of imgui
	//ImGui::CreateContext();
	//ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	//// this initializes imgui for SDL
	//ImGui_ImplGlfw_InitForVulkan(window, true);

	//// this initializes imgui for Vulkan
	//ImGui_ImplVulkan_InitInfo init_info = {};
	//init_info.Instance = loaded_engine->_instance;
	//init_info.PhysicalDevice = loaded_engine->_chosenGPU;
	//init_info.Device = loaded_engine->_device;
	//init_info.Queue = loaded_engine->_graphicsQueue;
	//init_info.DescriptorPool = imguiPool;
	//init_info.MinImageCount = 3;
	//init_info.ImageCount = 3;
	//init_info.UseDynamicRendering = true;

	////dynamic rendering parameters for imgui to use
	//init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	//init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	//init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;


	//init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	//ImGui_ImplVulkan_Init(&init_info);

	//ImGui_ImplVulkan_CreateFontsTexture();

	//// add the destroy the imgui created structures
	//loaded_engine->_mainDeletionQueue.push_function([=]() {
	//	ImGui_ImplVulkan_Shutdown();
	//	vkDestroyDescriptorPool(loaded_engine->_device, imguiPool, nullptr);
	//	});
}

void ClusteredForwardRenderer::DestroySwapchain()
{
	vkDestroySwapchainKHR(loaded_engine->_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {

		vkDestroyImageView(loaded_engine->_device, _swapchainImageViews[i], nullptr);
	}
}

void ClusteredForwardRenderer::UpdateScene()
{
	float currentFrame = glfwGetTime();
	float deltaTime = currentFrame - delta.lastFrame;
	delta.lastFrame = currentFrame;
	mainCamera.update(deltaTime);
	mainDrawContext.OpaqueSurfaces.clear();

	//sceneData.view = mainCamera.getViewMatrix();
	sceneData.view = mainCamera.matrices.view;
	auto camPos = mainCamera.position * -1.0f;
	sceneData.cameraPos = glm::vec4(camPos, 1.0f);
	// camera projection
	mainCamera.updateAspectRatio(_aspect_width / _aspect_height);
	sceneData.proj = mainCamera.matrices.perspective;

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	sceneData.proj[1][1] *= -1;
	sceneData.viewproj = sceneData.proj * sceneData.view;
	glm::mat4 model(1.0f);
	model = glm::translate(model, glm::vec3(0, 50, -500));
	model = glm::scale(model, glm::vec3(10, 10, 10));
	//sceneData.skyMat = model;
	sceneData.skyMat = sceneData.proj * glm::mat4(glm::mat3(sceneData.view));

	//some default lighting parameters
	sceneData.ambientColor = glm::vec4(.1f);
	sceneData.sunlightColor = directLight.color;
	sceneData.sunlightDirection = directLight.direction;
	sceneData.lightCount = pointData.pointLights.size();

	void* data = nullptr;
	vmaMapMemory(loaded_engine->_allocator, ClusterValues.lightSSBO.allocation, &data);
	memcpy(data, pointData.pointLights.data(), pointData.pointLights.size() * sizeof(PointLight));

	void* value = nullptr;
	//uint32_t* val = (uint32_t*)value; ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].allocation->GetMappedData();
	vmaMapMemory(loaded_engine->_allocator, ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].allocation, &value);
	uint32_t* val = (uint32_t*)value;
	*val = 0;
	//GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	//*sceneUniformData = sceneData;

	if (mainCamera.updated || directLight.direction != directLight.lastDirection)
	{
		auto cascadeData = shadows.getCascades(loaded_engine);
		memcpy(&sceneData.lightSpaceMatrices, cascadeData.lightSpaceMatrix.data(), sizeof(glm::mat4) * cascadeData.lightSpaceMatrix.size());
		memcpy(&sceneData.cascadePlaneDistances, cascadeData.cascadeDistances.data(), sizeof(float) * cascadeData.cascadeDistances.size());
		sceneData.distances.x = cascadeData.cascadeDistances[0];
		sceneData.distances.y = cascadeData.cascadeDistances[1];
		sceneData.distances.z = cascadeData.cascadeDistances[2];
		sceneData.distances.w = cascadeData.cascadeDistances[3];
		directLight.lastDirection = directLight.direction;
		render_shadowMap = true;
		mainCamera.updated = false;
	}

	if (debugShadowMap)
		sceneData.ConfigData.z = 1.0f;
	else
		sceneData.ConfigData.z = 0.0f;
	sceneData.ConfigData.x = mainCamera.getNearClip();
	sceneData.ConfigData.y = mainCamera.getFarClip();

	//Not an actual api Draw call
	loadedScenes["sponza"]->Draw(glm::mat4{ 1.f }, drawCommands);
	loadedScenes["cube"]->Draw(glm::mat4{ 1.f }, skyDrawCommands);
	loadedScenes["plane"]->Draw(glm::mat4{ 1.f }, imageDrawCommands);
}

void ClusteredForwardRenderer::LoadAssets()
{
	std::string cubePath{ "assets/cube.gltf" };
	auto cubeFile = loadGltf(loaded_engine, cubePath);
	loadedScenes["cube"] = *cubeFile;

	//std::string structurePath{ "assets/SM_Deccer_Cubes_Textured_Complex.gltf" };
	std::string structurePath{ "assets/sponza/Sponza.gltf" };

	auto structureFile = loadGltf(loaded_engine, structurePath, true);
	assert(structureFile.has_value());

	std::string planePath{ "assets/plane.glb" };
	auto planeFile = loadGltf(loaded_engine, planePath);

	loadedScenes["sponza"] = *structureFile;
	loadedScenes["plane"] = *planeFile;
}

void ClusteredForwardRenderer::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto app = reinterpret_cast<ClusteredForwardRenderer*>(glfwGetWindowUserPointer(window));
	app->mainCamera.processKeyInput(window, key, action);
}

void ClusteredForwardRenderer::CursorCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto app = reinterpret_cast<ClusteredForwardRenderer*>(glfwGetWindowUserPointer(window));
	app->mainCamera.processMouseMovement(window, xpos, ypos);
}

void ClusteredForwardRenderer::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<ClusteredForwardRenderer*>(glfwGetWindowUserPointer(window));
	app->resize_requested = true;
	if (width == 0 || height == 0)
		app->stop_rendering = true;
	else
		app->stop_rendering = false;
}

void ClusteredForwardRenderer::PreProcessPass()
{
	black_key::generate_irradiance_cube(loaded_engine);
	black_key::generate_prefiltered_cubemap(loaded_engine);
	black_key::generate_brdf_lut(loaded_engine);
	black_key::build_clusters(loaded_engine);

	loaded_engine->_mainDeletionQueue.push_function([=]() {
		loaded_engine->destroy_image(IBL._irradianceCube);
		loaded_engine->destroy_image(IBL._preFilteredCube);
		loaded_engine->destroy_image(IBL._lutBRDF);

		//destroy_image(_shadowDepthImage);
		vkDestroySampler(loaded_engine->_device, IBL._irradianceCubeSampler, nullptr);
		vkDestroySampler(loaded_engine->_device, IBL._lutBRDFSampler, nullptr);
		});
}

void ClusteredForwardRenderer::Cleanup()
{
	if (_isInitialized)
	{
		vkDeviceWaitIdle(loaded_engine->_device);

		loadedScenes.clear();

		for (auto& frame : _frames) {
			frame._deletionQueue.flush();
		}

		DestroySwapchain();
		loaded_engine->cleanup();
	}
	loaded_engine = nullptr;
}

void ClusteredForwardRenderer::Draw()
{
	ZoneScoped;
	auto start_update = std::chrono::system_clock::now();
	//wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(loaded_engine->_device, 1, &get_current_frame()._renderFence, true, 1000000000));

	auto end_update = std::chrono::system_clock::now();
	auto elapsed_update = std::chrono::duration_cast<std::chrono::microseconds>(end_update - start_update);
	stats.update_time = elapsed_update.count() / 1000.f;

	get_current_frame()._deletionQueue.flush();
	get_current_frame()._frameDescriptors.clear_pools(loaded_engine->_device);

	//request image from the swapchain
	uint32_t swapchainImageIndex;
	VkResult e = vkAcquireNextImageKHR(loaded_engine->_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);

	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}

	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height);
	_drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width);

	VK_CHECK(vkResetFences(loaded_engine->_device, 1, &get_current_frame()._renderFence));

	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//> draw_first
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _shadowDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _hdrImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	DrawMain(cmd);

	DrawPostProcess(cmd);

	//transtion the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, _hdrImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	//vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//< draw_first
	//> imgui_draw
	// execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, _hdrImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
	//vkutil::copy_image_to_image(cmd, _resolveImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

	// set swapchain image layout to Attachment Optimal so we can draw it
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//draw imgui into the swapchain image
	DrawImgui(cmd, _swapchainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can draw it
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);

	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(loaded_engine->_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = vkinit::present_info();

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(loaded_engine->_graphicsQueue, &presentInfo);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}
	//increase the number of frames drawn
	_frameNumber++;

}

void ClusteredForwardRenderer::DrawShadows(VkCommandBuffer cmd)
{
	ZoneScoped;
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = loaded_engine->create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		loaded_engine->destroy_buffer(gpuSceneDataBuffer);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(loaded_engine->_allocator, gpuSceneDataBuffer.allocation,&sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	//GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	*sceneUniformData = sceneData;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(loaded_engine->_device, _gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(loaded_engine->_device, globalDescriptor);

	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	auto draw = [&](const RenderObject& r) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cascadedShadows.shadowPipeline.pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cascadedShadows.shadowPipeline.layout, 0, 1,
			&globalDescriptor, 0, nullptr);

		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)_shadowDepthImage.imageExtent.width;
		viewport.height = (float)_shadowDepthImage.imageExtent.height;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = _shadowDepthImage.imageExtent.width;
		scissor.extent.height = _shadowDepthImage.imageExtent.height;
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		//vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cascadedShadows.shadowPipeline.layout, 1, 1,
			//&cascadedShadows.matData.materialSet, 0, nullptr);
		if (r.indexBuffer != lastIndexBuffer)
		{
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;

		vkCmdPushConstants(cmd, cascadedShadows.shadowPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

		stats.shadow_drawcall_count++;
		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		};
	for (auto& r : draws) {
		draw(drawCommands.OpaqueSurfaces[r]);
	}
}
void ClusteredForwardRenderer::DrawMain(VkCommandBuffer cmd)
{
	ZoneScoped;
	auto main_start = std::chrono::system_clock::now();

	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	VkRenderingInfo earlyDepthRenderInfo = vkinit::rendering_info(_windowExtent, nullptr, &depthAttachment);
	vkCmdBeginRendering(cmd, &earlyDepthRenderInfo);

	DrawEarlyDepth(cmd);

	vkCmdEndRendering(cmd);

	if (render_shadowMap)
	{
		VkRenderingAttachmentInfo shadowDepthAttachment = vkinit::depth_attachment_info(_shadowDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		VkExtent2D shadowExtent{};
		shadowExtent.width = _shadowDepthImage.imageExtent.width;
		shadowExtent.height = _shadowDepthImage.imageExtent.height;
		VkRenderingInfo shadowRenderInfo = vkinit::rendering_info(shadowExtent, nullptr, &shadowDepthAttachment, shadows.getCascadeLevels());
		auto startShadow = std::chrono::system_clock::now();
		vkCmdBeginRendering(cmd, &shadowRenderInfo);

		DrawShadows(cmd);

		vkCmdEndRendering(cmd);
		auto endShadow = std::chrono::system_clock::now();
		auto elapsedShadow = std::chrono::duration_cast<std::chrono::microseconds>(endShadow - startShadow);
		stats.shadow_pass_time = elapsedShadow.count() / 1000.f;
		render_shadowMap = false;
	}

	//Cull lights
	CullLights(cmd);

	VkClearValue geometryClear{ 1.0,1.0,1.0,1.0f };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, &_resolveImage.imageView, &geometryClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment2 = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD);

	vkutil::transition_image(cmd, _shadowDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment2);

	auto start = std::chrono::system_clock::now();
	vkCmdBeginRendering(cmd, &renderInfo);

	DrawGeometry(cmd);

	vkCmdEndRendering(cmd);
	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.mesh_draw_time = elapsed.count() / 1000.f;

	auto main_end = std::chrono::system_clock::now();
	auto main_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(main_end - main_start);
	//stats.frametime = main_elapsed.count() / 1000.f;
	VkRenderingAttachmentInfo colorAttachment2 = vkinit::attachment_info(_drawImage.imageView, &_resolveImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment3 = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_DONT_CARE);
	VkRenderingInfo backRenderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment2, &depthAttachment3);
	vkCmdBeginRendering(cmd, &backRenderInfo);

	DrawBackground(cmd);

	vkCmdEndRendering(cmd);
}
void ClusteredForwardRenderer::DrawPostProcess(VkCommandBuffer cmd)
{
	ZoneScoped;
	vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//vkutil::transition_image(cmd, _presentImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkClearValue clear{ 1.0f, 1.0f, 1.0f, 1.0f };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_hdrImage.imageView, nullptr, &clear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo hdrRenderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &hdrRenderInfo);
	DrawHdr(cmd);
	vkCmdEndRendering(cmd);
}

void ClusteredForwardRenderer::DrawBackground(VkCommandBuffer cmd)
{
	ZoneScoped;
	std::vector<uint32_t> b_draws;
	b_draws.reserve(skyDrawCommands.OpaqueSurfaces.size());
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer skySceneDataBuffer = loaded_engine->create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		loaded_engine->destroy_buffer(skySceneDataBuffer);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(loaded_engine->_allocator, skySceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = sceneData;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(loaded_engine->_device, _skyboxDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, skySceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, _skyImage.imageView, _cubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(loaded_engine->_device, globalDescriptor);

	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
	auto b_draw = [&](const RenderObject& r) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyBoxPSO.skyPipeline.pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyBoxPSO.skyPipeline.layout, 0, 1,
			&globalDescriptor, 0, nullptr);

		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)_windowExtent.width;
		viewport.height = (float)_windowExtent.height;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = _windowExtent.width;
		scissor.extent.height = _windowExtent.height;
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		if (r.indexBuffer != lastIndexBuffer)
		{
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;

		vkCmdPushConstants(cmd, skyBoxPSO.skyPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		};
	b_draw(skyDrawCommands.OpaqueSurfaces[0]);
	skyDrawCommands.OpaqueSurfaces.clear();
}

void ClusteredForwardRenderer::DrawGeometry(VkCommandBuffer cmd)
{
	ZoneScoped;
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = loaded_engine->create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		loaded_engine->destroy_buffer(gpuSceneDataBuffer);
		});


	void* sceneDataPtr = nullptr;
	vmaMapMemory(loaded_engine->_allocator, gpuSceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = sceneData;

	//write the buffer
	//GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	//*sceneUniformData = sceneData;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(loaded_engine->_device, _gpuSceneDataDescriptorLayout);

	static auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(2, _shadowDepthImage.imageView, _depthSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(3, IBL._irradianceCube.imageView, IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(4, IBL._lutBRDF.imageView, IBL._lutBRDFSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(5, IBL._preFilteredCube.imageView, IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_buffer(6, ClusterValues.lightSSBO.buffer, pointData.pointLights.size() * sizeof(PointLight), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(7, ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(8, ClusterValues.lightIndexListSSBO.buffer, totalLightCount * sizeof(uint32_t), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(9, ClusterValues.lightGridSSBO.buffer, ClusterValues.numClusters * sizeof(LightGrid), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	writer.update_set(loaded_engine->_device, globalDescriptor);

	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	auto draw = [&](const RenderObject& r) {
		if (r.material != lastMaterial) {
			lastMaterial = r.material;
			if (r.material->pipeline != lastPipeline) {

				lastPipeline = r.material->pipeline;
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1,
					&globalDescriptor, 0, nullptr);

				VkViewport viewport = {};
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = (float)_windowExtent.width;
				viewport.height = (float)_windowExtent.height;
				viewport.minDepth = 0.f;
				viewport.maxDepth = 1.f;
				vkCmdSetViewport(cmd, 0, 1, &viewport);

				VkRect2D scissor = {};
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = _windowExtent.width;
				scissor.extent.height = _windowExtent.height;

				vkCmdSetScissor(cmd, 0, 1, &scissor);
			}

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
				&r.material->materialSet, 0, nullptr);
		}
		if (r.indexBuffer != lastIndexBuffer) {
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;

		vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

		stats.drawcall_count++;
		stats.triangle_count += r.indexCount / 3;
		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		};

	stats.drawcall_count = 0;
	stats.triangle_count = 0;

	for (auto& r : draws) {
		draw(drawCommands.OpaqueSurfaces[r]);
	}

	for (auto& r : drawCommands.TransparentSurfaces) {
		draw(r);
	}

	// we delete the draw commands now that we processed them
	drawCommands.OpaqueSurfaces.clear();
	drawCommands.TransparentSurfaces.clear();
	draws.clear();
}

void ClusteredForwardRenderer::DrawHdr(VkCommandBuffer cmd)
{
	ZoneScoped;
	std::vector<uint32_t> draws;
	draws.reserve(imageDrawCommands.OpaqueSurfaces.size());

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(loaded_engine->_device, _drawImageDescriptorLayout);

	DescriptorWriter writer;
	writer.write_image(0, _resolveImage.imageView, _defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(loaded_engine->_device, globalDescriptor);

	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
	auto draw = [&](const RenderObject& r) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, HdrPSO.renderImagePipeline.pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, HdrPSO.renderImagePipeline.layout, 0, 1,
			&globalDescriptor, 0, nullptr);

		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)_windowExtent.width;
		viewport.height = (float)_windowExtent.height;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = _windowExtent.width;
		scissor.extent.height = _windowExtent.height;
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		if (r.indexBuffer != lastIndexBuffer)
		{
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;

		vkCmdPushConstants(cmd, HdrPSO.renderImagePipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
		vkCmdDraw(cmd, 3, 1, 0, 0);
		};
	draw(imageDrawCommands.OpaqueSurfaces[0]);

}

void ClusteredForwardRenderer::CullLights(VkCommandBuffer cmd)
{
	CullData culling_information;

	VkDescriptorSet cullingDescriptor = get_current_frame()._frameDescriptors.allocate(loaded_engine->_device, _cullLightsDescriptorLayout);

	//write the buffer
	//auto* pointBuffer = ClusterValues.lightSSBO.allocation->GetMappedData();
	//memcpy(pointBuffer, pointData.pointLights.data(), pointData.pointLights.size() * sizeof(PointLight));

	DescriptorWriter writer;
	writer.write_buffer(0, ClusterValues.AABBVolumeGridSSBO.buffer, ClusterValues.numClusters * sizeof(VolumeTileAABB), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(1, ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(2, ClusterValues.lightSSBO.buffer, pointData.pointLights.size() * sizeof(PointLight), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;
	writer.write_buffer(3, ClusterValues.lightIndexListSSBO.buffer, sizeof(uint32_t) * totalLightCount, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(4, ClusterValues.lightGridSSBO.buffer, ClusterValues.numClusters * sizeof(LightGrid), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(5, ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].buffer, sizeof(uint32_t), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.update_set(loaded_engine->_device, cullingDescriptor);

	culling_information.view = mainCamera.matrices.view;
	culling_information.lightCount = pointData.pointLights.size();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _cullLightsPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _cullLightsPipelineLayout, 0, 1, &cullingDescriptor, 0, nullptr);

	vkCmdPushConstants(cmd, _cullLightsPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullData), &culling_information);
	vkCmdDispatch(cmd, 16, 9, 24);
}

void ClusteredForwardRenderer::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	auto start_imgui = std::chrono::system_clock::now();
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
	auto end_imgui = std::chrono::system_clock::now();
	auto elapsed_imgui = std::chrono::duration_cast<std::chrono::microseconds>(end_imgui - start_imgui);
	stats.ui_draw_time = elapsed_imgui.count() / 1000.f;
}

void ClusteredForwardRenderer::DrawEarlyDepth(VkCommandBuffer cmd)
{
	draws.reserve(drawCommands.OpaqueSurfaces.size());
	for (int i = 0; i < drawCommands.OpaqueSurfaces.size(); i++) {
		if (black_key::is_visible(drawCommands.OpaqueSurfaces[i], sceneData.viewproj)) {
			draws.push_back(i);
		}
	}

	std::sort(draws.begin(), draws.end(), [&](const auto& iA, const auto& iB) {
		const RenderObject& A = drawCommands.OpaqueSurfaces[iA];
		const RenderObject& B = drawCommands.OpaqueSurfaces[iB];
		if (A.material == B.material) {
			return A.indexBuffer < B.indexBuffer;
		}
		else {
			return A.material < B.material;
		}
		});

	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = loaded_engine->create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		loaded_engine->destroy_buffer(gpuSceneDataBuffer);
		});

	void* sceneDataPtr = nullptr;
	vmaMapMemory(loaded_engine->_allocator, gpuSceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = sceneData;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(loaded_engine->_device, _gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(loaded_engine->_device, globalDescriptor);

	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	auto draw = [&](const RenderObject& r) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrePassPSO.earlyDepthPipeline.pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrePassPSO.earlyDepthPipeline.layout, 0, 1,
			&globalDescriptor, 0, nullptr);

		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)_windowExtent.width;
		viewport.height = (float)_windowExtent.height;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = _windowExtent.width;
		scissor.extent.height = _windowExtent.height;
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		if (r.indexBuffer != lastIndexBuffer)
		{
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;

		vkCmdPushConstants(cmd, depthPrePassPSO.earlyDepthPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		};

	for (auto& r : draws) {
		draw(drawCommands.OpaqueSurfaces[r]);
	}
}

void ClusteredForwardRenderer::Run()
{
	bool bQuit = false;


	// main loop
	while (!glfwWindowShouldClose(loaded_engine->window)) {
		auto start = std::chrono::system_clock::now();
		if (resize_requested) {
			ResizeSwapchain();
		}
		// do not draw if we are minimized
		if (stop_rendering) {
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();


		ImGui::NewFrame();

		SetImguiTheme(0.8f);
		DrawUI();
		ImGui::Render();


		auto start_update = std::chrono::system_clock::now();
		UpdateScene();
		auto end_update = std::chrono::system_clock::now();
		auto elapsed_update = std::chrono::duration_cast<std::chrono::microseconds>(end_update - start_update);

		Draw();

		glfwPollEvents();
		auto end = std::chrono::system_clock::now();

		//convert to microseconds (integer), and then come back to miliseconds
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		stats.frametime = elapsed.count() / 1000.f;
	}
	FrameMark;
}

void ClusteredForwardRenderer::ResizeSwapchain()
{
	vkDeviceWaitIdle(loaded_engine->_device);

	DestroySwapchain();

	int w, h;
	glfwGetWindowSize(loaded_engine->window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	_aspect_width = w;
	_aspect_height = h;

	CreateSwapchain(_windowExtent.width, _windowExtent.height);

	//Destroy and recreate render targets
	loaded_engine->destroy_image(_drawImage);
	loaded_engine->destroy_image(_hdrImage);
	loaded_engine->destroy_image(_resolveImage);

	VkExtent3D ImageExtent{
		_aspect_width,
		_aspect_height,
		1
	};
	_drawImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, loaded_engine, VK_IMAGE_VIEW_TYPE_2D, false, 1, VK_SAMPLE_COUNT_4_BIT);
	_hdrImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, loaded_engine);
	_resolveImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, loaded_engine);

	resize_requested = false;
}

void ClusteredForwardRenderer::DrawUI()
{
	// Demonstrate the various window flags. Typically you would just use the default!
	static bool no_titlebar = false;
	static bool no_scrollbar = false;
	static bool no_menu = false;
	static bool no_move = false;
	static bool no_resize = false;
	static bool no_collapse = false;
	static bool no_close = false;
	static bool no_nav = false;
	static bool no_background = false;
	static bool no_bring_to_front = false;
	static bool no_docking = false;
	static bool unsaved_document = false;

	ImGuiWindowFlags window_flags = 0;
	if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
	if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
	if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
	if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
	if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
	if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
	if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
	if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
	if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	//if (no_docking)         window_flags |= ImGuiWindowFlags_NoDocking;
	if (unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;

	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

	bool p_open = true;
	if (!ImGui::Begin("Black key", &p_open, window_flags))
	{
		ImGui::End();
		return;
	}

	if (ImGui::CollapsingHeader("Lighting"))
	{
		std::string index{};
		if (ImGui::TreeNode("Point Lights"))
		{
			for (size_t i = 0; i < pointData.pointLights.size(); i++)
			{
				index = std::to_string(i);
				if (ImGui::TreeNode(("Point "s + index).c_str()))
				{
					if (ImGui::TreeNode("Position"))
					{
						float pos[3] = { pointData.pointLights[i].position.x, pointData.pointLights[i].position.y, pointData.pointLights[i].position.z };
						ImGui::SliderFloat3("x,y,z", pos, -15.0f, 15.0f);
						pointData.pointLights[i].position = glm::vec4(pos[0], pos[1], pos[2], 0.0f);
						ImGui::TreePop();
						ImGui::Spacing();
					}
					if (ImGui::TreeNode("Color"))
					{
						float col[4] = { pointData.pointLights[i].color.x, pointData.pointLights[i].color.y, pointData.pointLights[i].color.z, pointData.pointLights[i].color.w };
						ImGui::ColorEdit4("Light color", col);
						pointData.pointLights[i].color = glm::vec4(col[0], col[1], col[2], col[3]);
						ImGui::TreePop();
						ImGui::Spacing();
					}

					if (ImGui::TreeNode("Attenuation"))
					{
						//move this declaration to a higher scope later
						ImGui::SliderFloat("Range", &pointData.pointLights[i].range, 0.0f, 1.0f);
						ImGui::SliderFloat("Intensity", &pointData.pointLights[i].intensity, 0.0f, 1.0f);
						//ImGui::SliderFloat("quadratic", &points[i].quadratic, 0.0f, 2.0f);
						//ImGui::SliderFloat("radius", &points[i].quadratic, 0.0f, 100.0f);
						ImGui::TreePop();
						ImGui::Spacing();

					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Direct Light"))
		{
			ImGui::SeparatorText("direction");
			float pos[3] = { directLight.direction.x, directLight.direction.y, directLight.direction.z };
			ImGui::SliderFloat3("x,y,z", pos, -7, 7);
			directLight.direction = glm::vec4(pos[0], pos[1], pos[2], 0.0f);

			ImGui::SeparatorText("color");
			float col[4] = { directLight.color.x, directLight.color.y, directLight.color.z, directLight.color.w };
			ImGui::ColorEdit4("Light color", col);
			directLight.color = glm::vec4(col[0], col[1], col[2], col[3]);
			ImGui::TreePop();
		}
	}
	if (ImGui::CollapsingHeader("Debugging"))
	{
		ImGui::Checkbox("Visualize shadow cascades", &debugShadowMap);
	}

	if (ImGui::CollapsingHeader("Engine Stats"))
	{
		ImGui::SeparatorText("Render timings");
		ImGui::Text("FPS %f ", 1000.0f / stats.frametime);
		ImGui::Text("frametime %f ms", stats.frametime);
		ImGui::Text("drawtime %f ms", stats.mesh_draw_time);
		ImGui::Text("triangles %i", stats.triangle_count);
		ImGui::Text("draws %i", stats.drawcall_count);
		ImGui::Text("UI render time %f ms", stats.ui_draw_time);
		ImGui::Text("Update time %f ms", stats.update_time);
		ImGui::Text("Shadow Pass time %f ms", stats.shadow_pass_time);
	}
	ImGui::End();
}
*/