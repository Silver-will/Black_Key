#include "clustered_forward_renderer.h"
#include "../vk_device.h"
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

#define M_PI       3.14159265358979323846


void ClusteredForwardRenderer::Init(VulkanEngine* engine)
{
	assert(engine != nullptr);
	this->engine = engine;

	InitEngine();

	ConfigureRenderWindow();

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

void ClusteredForwardRenderer::ConfigureRenderWindow()
{
	
	glfwSetWindowUserPointer(engine->window, this);
	glfwSetFramebufferSizeCallback(engine->window, FramebufferResizeCallback);
	glfwSetKeyCallback(engine->window, KeyCallback);
	glfwSetCursorPosCallback(engine->window, CursorCallback);
	glfwSetInputMode(engine->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void ClusteredForwardRenderer::InitEngine()
{
	//Request required GPU features and extensions
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
	engine->init(baseFeatures, features11, features12, features);
	resource_manager = std::make_shared<ResourceManager>(engine);
	scene_manager = std::make_shared<SceneManager>();
	scene_manager->Init(resource_manager, engine);
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

	msaa_samples = engine->GetMSAASampleCount();
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
	vmaCreateImage(engine->_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);
	vmaSetAllocationName(engine->_allocator, _drawImage.allocation, "Draw image");

	//Create resolve image for multisampling
	VkImageCreateInfo resolve_img_info = vkinit::image_create_info(_resolveImage.imageFormat, resolveImageUsages, drawImageExtent, 1);
	vmaCreateImage(engine->_allocator, &resolve_img_info, &rimg_allocinfo, &_resolveImage.image, &_resolveImage.allocation, nullptr);
	vmaSetAllocationName(engine->_allocator, _resolveImage.allocation, "resolve image");

	vmaCreateImage(engine->_allocator, &resolve_img_info, &rimg_allocinfo, &_hdrImage.image, &_hdrImage.allocation, nullptr);
	vmaSetAllocationName(engine->_allocator, _hdrImage.allocation, "hdr image");

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	VkImageViewCreateInfo resolve_view_info = vkinit::imageview_create_info(_resolveImage.imageFormat, _resolveImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	VkImageViewCreateInfo hdr_view_info = vkinit::imageview_create_info(_hdrImage.imageFormat, _hdrImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(engine->_device, &rview_info, nullptr, &_drawImage.imageView));
	VK_CHECK(vkCreateImageView(engine->_device, &resolve_view_info, nullptr, &_resolveImage.imageView));
	VK_CHECK(vkCreateImageView(engine->_device, &hdr_view_info, nullptr, &_hdrImage.imageView));


	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo dresolve_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages | VK_IMAGE_USAGE_SAMPLED_BIT, drawImageExtent, 1);

	//allocate and create the image
	vmaCreateImage(engine->_allocator, &dresolve_info, &rimg_allocinfo, &_depthResolveImage.image, &_depthResolveImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo dRview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthResolveImage.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(engine->_device, &dRview_info, nullptr, &_depthResolveImage.imageView));

	depthImageUsages |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent, 1, msaa_samples);

	//allocate and create the image
	vmaCreateImage(engine->_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(engine->_device, &dview_info, nullptr, &_depthImage.imageView));

	//add to deletion queues
	resource_manager->deletionQueue.push_function([=]() {
		vkDestroyImageView(engine->_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _drawImage.image, _drawImage.allocation);

		vkDestroyImageView(engine->_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _depthImage.image, _depthImage.allocation);

		vkDestroyImageView(engine->_device, _resolveImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _resolveImage.image, _resolveImage.allocation);

		vkDestroyImageView(engine->_device, _hdrImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _hdrImage.image, _hdrImage.allocation);

		vkDestroyImageView(engine->_device, _depthResolveImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _depthResolveImage.image, _depthResolveImage.allocation);
		});
}


void ClusteredForwardRenderer::InitCommands()
{
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(engine->_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(engine->_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(engine->_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		resource_manager->deletionQueue.push_function([=]() { vkDestroyCommandPool(engine->_device, _frames[i]._commandPool, nullptr); });
	}
}

void ClusteredForwardRenderer::InitSyncStructures()
{
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateFence(engine->_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

		VK_CHECK(vkCreateSemaphore(engine->_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(engine->_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		resource_manager->deletionQueue.push_function([=]() {
			vkDestroyFence(engine->_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(engine->_device, _frames[i]._swapchainSemaphore, nullptr);
			vkDestroySemaphore(engine->_device, _frames[i]._renderSemaphore, nullptr);
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

	globalDescriptorAllocator.init_pool(engine->_device, 30, sizes);
	_mainDeletionQueue.push_function(
		[&]() { vkDestroyDescriptorPool(engine->_device, globalDescriptorAllocator.pool, nullptr); });


	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_drawImageDescriptorLayout = builder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);
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
		_gpuSceneDataDescriptorLayout = builder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	}
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		cascaded_shadows_descriptor_layout = builder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_skyboxDescriptorLayout = builder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		_cullLightsDescriptorLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		_buildClustersDescriptorLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 65536);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 65536);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 65536);
		resource_manager->bindless_descriptor_layout = builder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		compute_cull_descriptor_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		depth_reduce_descriptor_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
	}

	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(engine->_device, _drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, _gpuSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, _skyboxDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, cascaded_shadows_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, _cullLightsDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, _buildClustersDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, resource_manager->bindless_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, compute_cull_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, depth_reduce_descriptor_layout, nullptr);
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
		_frames[i]._frameDescriptors.init(engine->_device, 1000, frame_sizes);
		_frames[i].bindless_material_descriptor = DescriptorAllocator{};
		_frames[i].bindless_material_descriptor.init_pool(engine->_device, 65536, bindless_sizes);
		_mainDeletionQueue.push_function([&, i]() {
			_frames[i]._frameDescriptors.destroy_pools(engine->_device);
			_frames[i].bindless_material_descriptor.destroy_pool(engine->_device);
			});
	}
}


void ClusteredForwardRenderer::InitPipelines()
{
	PipelineCreationInfo info;
	info.layouts.push_back(_gpuSceneDataDescriptorLayout);
	info.layouts.push_back(resource_manager->bindless_descriptor_layout);
	info.imageFormat = _drawImage.imageFormat;
	info.depthFormat = _depthImage.imageFormat;
	metalRoughMaterial.build_pipelines(engine, info);
	resource_manager->PBRpipeline = &metalRoughMaterial;

	PipelineCreationInfo shadowInfo;
	shadowInfo.layouts.push_back(cascaded_shadows_descriptor_layout);
	shadowInfo.depthFormat = _shadowDepthImage.imageFormat;
	cascadedShadows.build_pipelines(engine, shadowInfo);

	PipelineCreationInfo skyInfo;
	skyInfo.layouts.push_back(_skyboxDescriptorLayout);
	skyInfo.depthFormat = _depthImage.imageFormat;
	skyInfo.imageFormat = _drawImage.imageFormat;
	skyBoxPSO.build_pipelines(engine,skyInfo);

	PipelineCreationInfo HDRinfo;
	HDRinfo.layouts.push_back(_drawImageDescriptorLayout);
	HDRinfo.imageFormat = _drawImage.imageFormat;
	HdrPSO.build_pipelines(engine,HDRinfo);

	PipelineCreationInfo earlyDepthInfo;
	earlyDepthInfo.layouts.push_back(_gpuSceneDataDescriptorLayout);
	earlyDepthInfo.depthFormat = _depthImage.imageFormat;
	depthPrePassPSO.build_pipelines(engine, earlyDepthInfo);
	InitComputePipelines();
	_mainDeletionQueue.push_function([&]()
		{
			depthPrePassPSO.clear_resources(engine->_device);
			metalRoughMaterial.clear_resources(engine->_device);
			cascadedShadows.clear_resources(engine->_device);
			skyBoxPSO.clear_resources(engine->_device);
			HdrPSO.clear_resources(engine->_device);
		});

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(engine->_device, _shadowDepthImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _shadowDepthImage.image, _shadowDepthImage.allocation);
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

	
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &cullLightsLayoutInfo, nullptr, &cull_lights_pso.layout));

	VkShaderModule cullLightShader;
	if (!vkutil::load_shader_module("shaders/cluster_cull_light_shader.spv", engine->_device, &cullLightShader)) {
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
	computePipelineCreateInfo.layout = cull_lights_pso.layout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &cull_lights_pso.pipeline));


	VkPipelineLayoutCreateInfo cullObjectsLayoutInfo = {};
	cullObjectsLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	cullObjectsLayoutInfo.pNext = nullptr;
	cullObjectsLayoutInfo.pSetLayouts = &compute_cull_descriptor_layout;
	cullObjectsLayoutInfo.setLayoutCount = 1;

	pushConstant.size = sizeof(vkutil::DrawCullData);

	cullObjectsLayoutInfo.pPushConstantRanges = &pushConstant;
	cullObjectsLayoutInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &cullObjectsLayoutInfo, nullptr, &cull_objects_pso.layout));

	VkShaderModule cullObjectsShader;
	if (!vkutil::load_shader_module("shaders/indirect_cull.comp.spv", engine->_device, &cullObjectsShader)) {
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

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &cull_objects_pso.pipeline));

	VkPipelineLayoutCreateInfo depthReduceLayoutInfo = {};
	depthReduceLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	depthReduceLayoutInfo.pNext = nullptr;
	depthReduceLayoutInfo.pSetLayouts = &depth_reduce_descriptor_layout;
	depthReduceLayoutInfo.setLayoutCount = 1;

	pushConstant.size = sizeof(glm::vec2);
	depthReduceLayoutInfo.pPushConstantRanges = &pushConstant;
	depthReduceLayoutInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &depthReduceLayoutInfo, nullptr, &depth_reduce_pso.layout));

	VkShaderModule depthReduceShader;
	if (!vkutil::load_shader_module("shaders/depth_reduce.comp.spv", engine->_device, &depthReduceShader)) {
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

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &depthComputePipelineCreateInfo, nullptr, &depth_reduce_pso.pipeline));



	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(engine->_device, depth_reduce_pso.layout, nullptr);
		vkDestroyPipeline(engine->_device, depth_reduce_pso.pipeline, nullptr);
		vkDestroyPipelineLayout(engine->_device, cull_objects_pso.layout, nullptr);
		vkDestroyPipeline(engine->_device, cull_objects_pso.pipeline, nullptr);
		vkDestroyPipelineLayout(engine->_device, cull_lights_pso.layout, nullptr);
		vkDestroyPipeline(engine->_device, cull_lights_pso.pipeline , nullptr);
		});
}

void ClusteredForwardRenderer::InitDefaultData()
{
	forward_passes.push_back(vkutil::MaterialPass::forward);
	forward_passes.push_back(vkutil::MaterialPass::transparency);

	directLight = DirectionalLight(glm::normalize(glm::vec4(-20.0f, -50.0f, -20.0f, 1.f)), glm::vec4(1.5f), glm::vec4(1.0f));
	//Create Shadow render target
	_shadowDepthImage = resource_manager->CreateImageEmpty(VkExtent3D(shadowMapSize, shadowMapSize, 1), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, false, shadows.getCascadeLevels());
	shadows.SetShadowMapTextureSize(shadowMapSize);

	//Create default images
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = resource_manager->CreateImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = resource_manager->CreateImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = resource_manager->CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	storageImage = resource_manager->CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	depthPyramidWidth = BlackKey::PreviousPow2(_windowExtent.width);
	depthPyramidHeight = BlackKey::PreviousPow2(_windowExtent.height);
	depthPyramidLevels = BlackKey::GetImageMipLevels(depthPyramidWidth, depthPyramidHeight);

	_depthPyramid = resource_manager->CreateImageEmpty(VkExtent3D(depthPyramidWidth, depthPyramidHeight, 1), VK_FORMAT_R32_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_IMAGE_VIEW_TYPE_2D, true, 1, VK_SAMPLE_COUNT_1_BIT, depthPyramidLevels);

	mainCamera.type = Camera::CameraType::firstperson;
	//mainCamera.flipY = true;
	mainCamera.movementSpeed = 2.5f;
	mainCamera.setPerspective(45.0f, (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 3000.0f);
	mainCamera.setPosition(glm::vec3(-0.12f, -5.14f, -2.25f));
	mainCamera.setRotation(glm::vec3(-17.0f, 7.0f, 0.0f));

	for (int i = 0; i < depthPyramidLevels; i++)
	{

		VkImageViewCreateInfo level_info = vkinit::imageview_create_info(VK_FORMAT_R32_SFLOAT, _depthPyramid.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
		level_info.subresourceRange.levelCount = 1;
		level_info.subresourceRange.baseMipLevel = i;

		VkImageView pyramid;
		vkCreateImageView(engine->_device, &level_info, nullptr, &pyramid);

		depthPyramidMips[i] = pyramid;
		assert(depthPyramidMips[i]);
	}

	//Populate point light list
	int numOfLights = 3000;
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_real_distribution<> distFloat(-25.0f, 25.0f);
	std::uniform_real_distribution<> distRadius(2.5f, 4.f);
	std::uniform_real_distribution<> distRGB(0, 255.0f);
	
	for (int i = 0; i < 1; i++)
	{
		//pointData.pointLights.push_back(PointLight(glm::vec4(distFloat(rng), (distFloat(rng) + 25.0f)/2.0f, distFloat(rng), 1.0f), glm::vec4(distRGB(rng) / 255.0f, distRGB(rng) / 255.0f, distRGB(rng) / 255.0f, 1.0), distRadius(rng), 10.0f));
		pointData.pointLights.push_back(PointLight(glm::vec4(3, 5, 4.1,1.0), glm::vec4(1.0),10.3f, 10.0f));
	}
	

	
	int light_per_row = 20;
	//Generate Light grid
	for (int x = 1; x < light_per_row; x++)
	{
		for (int y = 1; y < 5; y++)
		{
			for (int z = 1; z < light_per_row; z++)
			{
				auto xpos = ((22.0f / (float)x) / 2) - 22.0f - (22.0f/light_per_row);
				auto ypos = 10.0f / (float)y;
				auto zpos = ((5.0f / (float)z) / 2) - 5;
				pointData.pointLights.push_back(PointLight(glm::vec4(xpos, ypos, zpos, 1.0f), glm::vec4(distRGB(rng) / 255.0f, distRGB(rng) / 255.0f, distRGB(rng) / 255.0f, 1.0), 10.0f, 10.0f));

			}
		}
	}
	

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	errorCheckerboardImage = resource_manager->CreateImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, this);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerLinear);

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
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &cubeMapSampler);

	cubeSampl.maxLod = 1;
	cubeSampl.maxAnisotropy = 1.0f;
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &depthSampler);

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

	vkCreateSampler(engine->_device, &depthReductionSampl, nullptr, &depthReductionSampler);

	//< default_img

	_mainDeletionQueue.push_function([=]() {
		resource_manager->DestroyImage(_skyImage);
		resource_manager->DestroyImage(_shadowDepthImage);
		vkDestroySampler(engine->_device, depthReductionSampler, nullptr);
		vkDestroySampler(engine->_device, defaultSamplerLinear, nullptr);
		vkDestroySampler(engine->_device, defaultSamplerNearest, nullptr);
		vkDestroySampler(engine->_device, cubeMapSampler, nullptr);
		vkDestroySampler(engine->_device, depthSampler, nullptr);
		});
}


void ClusteredForwardRenderer::CreateSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ engine->_chosenGPU,engine->_device,engine->_surface };

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
	ClusterValues.AABBVolumeGridSSBO = resource_manager->CreateBuffer(ClusterValues.numClusters * sizeof(VolumeTileAABB), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	float zNear = mainCamera.getNearClip();
	float zFar = mainCamera.getFarClip();

	ClusterValues.sizeX = (uint16_t)std::ceilf(_aspect_width / (float)ClusterValues.gridSizeX);
	ScreenToView screen;
	auto proj = mainCamera.matrices.perspective;
	proj[1][1] *= -1;
	screen.inverseProjectionMat = glm::inverse(proj);
	screen.tileSizes.x = static_cast<float>(ClusterValues.gridSizeX);
	screen.tileSizes.y = static_cast<float>(ClusterValues.gridSizeY);
	screen.tileSizes.z = static_cast<float>(ClusterValues.gridSizeZ);
	screen.tileSizes.w = static_cast<float>(ClusterValues.sizeX);
	screen.screenDimensions.x = _aspect_width;
	screen.screenDimensions.y = _aspect_height;

	screen.sliceScalingFactor = (float)ClusterValues.gridSizeZ / std::log2f(zFar / zNear);
	screen.sliceBiasFactor = -((float)ClusterValues.gridSizeZ * std::log2f(zNear) / std::log2f(zFar / zNear));

	ClusterValues.screenToViewSSBO = resource_manager->CreateAndUpload(sizeof(ScreenToView), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, &screen);

	ClusterValues.lightSSBO = resource_manager->CreateAndUpload(pointData.pointLights.size() * sizeof(PointLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, pointData.pointLights.data());
	auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;
	ClusterValues.lightIndexListSSBO = resource_manager->CreateBuffer(sizeof(uint32_t) * totalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	ClusterValues.lightGridSSBO = resource_manager->CreateBuffer(ClusterValues.numClusters * sizeof(LightGrid), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	uint32_t val = 0;
	for (uint32_t i = 0; i < 2; i++)
	{
		ClusterValues.lightGlobalIndex[i] = resource_manager->CreateAndUpload(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT , VMA_MEMORY_USAGE_CPU_TO_GPU, &val);
	}
	
}


void ClusteredForwardRenderer::InitImgui()
{

	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(engine->_device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// this initializes imgui for SDL
	ImGui_ImplGlfw_InitForVulkan(engine->window, true);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = engine->_instance;
	init_info.PhysicalDevice = engine->_chosenGPU;
	init_info.Device = engine->_device;
	init_info.Queue = engine->_graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;


	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(engine->_device, imguiPool, nullptr);
		});
}

void ClusteredForwardRenderer::DestroySwapchain()
{
	vkDestroySwapchainKHR(engine->_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {

		vkDestroyImageView(engine->_device, _swapchainImageViews[i], nullptr);
	}
}

void ClusteredForwardRenderer::UpdateScene()
{
	float currentFrame = glfwGetTime();
	float deltaTime = currentFrame - delta.lastFrame;
	delta.lastFrame = currentFrame;
	mainCamera.update(deltaTime);
	mainDrawContext.OpaqueSurfaces.clear();

	scene_data.view = mainCamera.matrices.view;
	auto camPos = mainCamera.position * -1.0f;
	scene_data.cameraPos = glm::vec4(camPos, 1.0f);
	// camera projection
	mainCamera.updateAspectRatio(_aspect_width / _aspect_height);
	scene_data.proj = mainCamera.matrices.perspective;

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	scene_data.proj[1][1] *= -1;
	scene_data.viewproj = scene_data.proj * scene_data.view;
	glm::mat4 model(1.0f);
	model = glm::translate(model, glm::vec3(0, 50, -500));
	model = glm::scale(model, glm::vec3(10, 10, 10));
	//sceneData.skyMat = model;
	scene_data.skyMat = scene_data.proj * glm::mat4(glm::mat3(scene_data.view));

	//some default lighting parameters
	scene_data.sunlightColor = directLight.color;
	scene_data.sunlightDirection = directLight.direction;
	scene_data.lightCount = pointData.pointLights.size();

	void* data = nullptr;
	vmaMapMemory(engine->_allocator, ClusterValues.lightSSBO.allocation, &data);
	memcpy(data, pointData.pointLights.data(), pointData.pointLights.size() * sizeof(PointLight));
	vmaUnmapMemory(engine->_allocator, ClusterValues.lightSSBO.allocation);

	void* val = nullptr;
	uint32_t reset = 0;
	vmaMapMemory(engine->_allocator, ClusterValues.lightGlobalIndex[(_frameNumber) % FRAME_OVERLAP].allocation, &val);
	memcpy(val, &reset, sizeof(uint32_t));
	vmaUnmapMemory(engine->_allocator, ClusterValues.lightGlobalIndex[(_frameNumber) % FRAME_OVERLAP].allocation);


	cascadeData = shadows.getCascades(engine, mainCamera, scene_data);
	if (mainCamera.updated || directLight.direction != directLight.lastDirection)
	{
		memcpy(&shadow_data.lightSpaceMatrices, cascadeData.lightSpaceMatrix.data(), sizeof(glm::mat4) * cascadeData.lightSpaceMatrix.size());
		scene_data.distances.x = cascadeData.cascadeDistances[0];
		scene_data.distances.y = cascadeData.cascadeDistances[1];
		scene_data.distances.z = cascadeData.cascadeDistances[2];
		scene_data.distances.w = cascadeData.cascadeDistances[3];

		//shadow_data.distances = scene_data.distances;
		directLight.lastDirection = directLight.direction;
		render_shadowMap = true;
		mainCamera.updated = false;
	}

	if (debugShadowMap)
		scene_data.ConfigData.z = 1.0f;
	else
		scene_data.ConfigData.z = 0.0f;

	scene_data.debugValues.x = debugLightClustering ? 1.0 : 0.0f;
	scene_data.ConfigData.x = mainCamera.getNearClip();
	scene_data.ConfigData.y = mainCamera.getFarClip();

	//Prepare Render objects
	loadedScenes["sponza"]->Draw(glm::mat4{ 1.f }, drawCommands);
	loadedScenes["cube"]->Draw(glm::mat4{ 1.f }, skyDrawCommands);
	loadedScenes["plane"]->Draw(glm::mat4{ 1.f }, imageDrawCommands);
}

void ClusteredForwardRenderer::LoadAssets()
{
	//Load in skyBox image
	_skyImage = vkutil::load_cubemap_image("assets/textures/hdris/overcast.ktx", VkExtent3D{ 1,1,1 }, engine, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, true);

	std::string structurePath{ "assets/sponza/Sponza.gltf" };
	auto structureFile = resource_manager->loadGltf(engine, structurePath, true);
	assert(structureFile.has_value());

	std::string cubePath{ "assets/cube.gltf" };
	auto cubeFile = resource_manager->loadGltf(engine, cubePath);
	assert(cubeFile.has_value());

	std::string planePath{ "assets/plane.glb" };
	auto planeFile = resource_manager->loadGltf(engine, planePath);
	assert(planeFile.has_value());

	loadedScenes["sponza"] = *structureFile;
	loadedScenes["cube"] = *cubeFile;
	loadedScenes["plane"] = *planeFile;

	loadedScenes["sponza"]->Draw(glm::mat4{ 1.f }, drawCommands);
	scene_manager->RegisterMeshAssetReference("sponza");
	//Register render objects for draw indirect
	scene_manager->RegisterObjectBatch(drawCommands);
	scene_manager->MergeMeshes();
	scene_manager->PrepareIndirectBuffers();
	scene_manager->BuildBatches();
	resource_manager->write_material_array();
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
	GenerateIrradianceCube();
	GeneratePrefilteredCubemap();
	black_key::generate_brdf_lut(engine, IBL);
	PipelineCreationInfo clusterInfo;
	clusterInfo.layouts.push_back(_buildClustersDescriptorLayout);
	BuildClusters();

	resource_manager->deletionQueue.push_function([=]() {
		resource_manager->DestroyImage(IBL._irradianceCube);
		resource_manager->DestroyImage(IBL._preFilteredCube);
		resource_manager->DestroyImage(IBL._lutBRDF);

		vkDestroySampler(engine->_device, IBL._irradianceCubeSampler, nullptr);
		vkDestroySampler(engine->_device, IBL._lutBRDFSampler, nullptr);
		});
}

void ClusteredForwardRenderer::BuildClusters()
{
	VkPipelineLayoutCreateInfo ClusterLayoutInfo = {};
	ClusterLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	ClusterLayoutInfo.pNext = nullptr;
	ClusterLayoutInfo.pSetLayouts = &_buildClustersDescriptorLayout;
	ClusterLayoutInfo.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(clusterParams);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	ClusterLayoutInfo.pPushConstantRanges = &pushConstant;
	ClusterLayoutInfo.pushConstantRangeCount = 1;

	VkPipelineLayout buildClusterLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &ClusterLayoutInfo, nullptr, &buildClusterLayout));
	generate_clusters_pso.layout = buildClusterLayout;

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
	generate_clusters_pso.pipeline = clusterPipeline;
	VkDescriptorSet globalDescriptor = globalDescriptorAllocator.allocate(engine->_device, _buildClustersDescriptorLayout);

	engine->immediate_submit([&](VkCommandBuffer cmd)
		{
			DescriptorWriter writer;
			writer.write_buffer(0, ClusterValues.AABBVolumeGridSSBO.buffer, ClusterValues.numClusters * sizeof(VolumeTileAABB), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			writer.write_buffer(1, ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			writer.update_set(engine->_device, globalDescriptor);

			clusterParams clusterData;
			clusterData.zNear = mainCamera.getNearClip();
			clusterData.zFar = mainCamera.getFarClip();
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, clusterPipeline);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, buildClusterLayout, 0, 1, &globalDescriptor, 0, nullptr);

			vkCmdPushConstants(cmd, buildClusterLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(clusterParams), &clusterData);
			vkCmdDispatch(cmd, 16, 9, 24);
		});
}

void ClusteredForwardRenderer::GeneratePrefilteredCubemap()
{
	VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
	uint32_t dim = 512;
	uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	IBL._preFilteredCube = vkutil::create_cubemap_image(VkExtent3D{ dim,dim,1 }, engine, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);

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
	matrixRange.size = sizeof(black_key::PushBlock);
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
	loadedScenes["cube"]->Draw(glm::mat4{ 1.f }, skyDrawCommands);

	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, irradianceSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, _skyImage.imageView, IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, globalDescriptor);

	//begin drawing
	auto cmd = vk_device::create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, _frames[0]._commandPool, engine);

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//> draw_first
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, IBL._preFilteredCube.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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

	auto r = skyDrawCommands.OpaqueSurfaces[0];
	//vkCmdBeginRendering(cmd, &renderInfo);
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	{
		VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
		black_key::PushBlock pushBlock;
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
				vkCmdPushConstants(cmd, irradianceLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(black_key::PushBlock), &pushBlock);

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

				vkutil::copy_image_to_image(cmd, drawImage.image, IBL._preFilteredCube.image, copyExtent, copyExtent, &blitRegion);
				vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}
		skyDrawCommands.OpaqueSurfaces.clear();
	}
	vkutil::transition_image(cmd, IBL._preFilteredCube.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vk_device::flush_command_buffer(cmd, engine->_graphicsQueue, _frames[0]._commandPool, engine);
	vkDestroyImage(engine->_device, drawImage.image, nullptr);
	vkDestroyImageView(engine->_device, drawImage.imageView, nullptr);
	vkDestroyPipeline(engine->_device, irradiancePipeline, nullptr);
	vkDestroyPipelineLayout(engine->_device, irradianceLayout, nullptr);
	vkDestroyDescriptorSetLayout(engine->_device, irradianceSetLayout, nullptr);

}

void ClusteredForwardRenderer::GenerateIrradianceCube()
{
	VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
	uint32_t dim = 64;
	uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
	IBL._irradianceCube = vkutil::create_cubemap_image(VkExtent3D{ dim,dim,1 }, engine, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);

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
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &IBL._irradianceCubeSampler);

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
	matrixRange.size = sizeof(black_key::PushBlock);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayout layouts[] = { irradianceSetLayout };

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
	loadedScenes["cube"]->Draw(glm::mat4{ 1.f }, skyDrawCommands);

	//begin drawing

	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, irradianceSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, _skyImage.imageView, IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, globalDescriptor);

	auto cmd = vk_device::create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, _frames[0]._commandPool, engine);

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//> draw_first
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, IBL._irradianceCube.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(drawImage.imageView, nullptr, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(VkExtent2D{ dim,dim }, &colorAttachment, nullptr);

	auto r = skyDrawCommands.OpaqueSurfaces[0];

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

				black_key::PushBlock pushBlock;
				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
				pushBlock.mvp[1][1] *= -1;
				pushBlock.vertexBuffer = r.vertexBufferAddress;
				vkCmdPushConstants(cmd, irradianceLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(black_key::PushBlock), &pushBlock);
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
				vkutil::copy_image_to_image(cmd, drawImage.image, IBL._irradianceCube.image, copyExtent, copyExtent, &blitRegion);
				vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}
		skyDrawCommands.OpaqueSurfaces.clear();
	}
	vkutil::transition_image(cmd, IBL._irradianceCube.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vk_device::flush_command_buffer(cmd, engine->_graphicsQueue, _frames[0]._commandPool, engine);

	vkDestroyImage(engine->_device, drawImage.image, nullptr);
	vkDestroyImageView(engine->_device, drawImage.imageView, nullptr);
	vkDestroyPipeline(engine->_device, irradiancePipeline, nullptr);
	vkDestroyPipelineLayout(engine->_device, irradianceLayout, nullptr);
	vkDestroyDescriptorSetLayout(engine->_device, irradianceSetLayout, nullptr);

}

void ClusteredForwardRenderer::Cleanup()
{
	if (_isInitialized)
	{
		vkDeviceWaitIdle(engine->_device);

		loadedScenes.clear();

		for (auto& frame : _frames) {
			frame._deletionQueue.flush();
		}

		DestroySwapchain();
		engine->cleanup();
	}
	engine = nullptr;
}

void ClusteredForwardRenderer::Draw()
{
	ZoneScoped;
	auto start_update = std::chrono::system_clock::now();
	//wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(engine->_device, 1, &get_current_frame()._renderFence, true, 1000000000));


	auto end_update = std::chrono::system_clock::now();
	auto elapsed_update = std::chrono::duration_cast<std::chrono::microseconds>(end_update - start_update);
	stats.update_time = elapsed_update.count() / 1000.f;

	get_current_frame()._deletionQueue.flush();
	get_current_frame()._frameDescriptors.clear_pools(engine->_device);

	//request image from the swapchain
	uint32_t swapchainImageIndex;
	VkResult e = vkAcquireNextImageKHR(engine->_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);

	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}

	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height);
	_drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width);

	VK_CHECK(vkResetFences(engine->_device, 1, &get_current_frame()._renderFence));

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
	vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _depthResolveImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _shadowDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _hdrImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _depthPyramid.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

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

	//draw UI directly into the swapchain image
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
	VK_CHECK(vkQueueSubmit2(engine->_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

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

	VkResult presentResult = vkQueuePresentKHR(engine->_graphicsQueue, &presentInfo);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}
	//increase the number of frames drawn
	_frameNumber++;
}

void ClusteredForwardRenderer::DrawShadows(VkCommandBuffer cmd)
{
	AllocatedBuffer shadowDataBuffer = vkutil::create_buffer(sizeof(shadowData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, engine);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		vkutil::destroy_buffer(shadowDataBuffer,engine);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(engine->_allocator, shadowDataBuffer.allocation, &sceneDataPtr);
	shadowData* ptr = (shadowData*)sceneDataPtr;
	*ptr = shadow_data;
	vmaUnmapMemory(engine->_allocator, shadowDataBuffer.allocation);

	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, cascaded_shadows_descriptor_layout);

	DescriptorWriter writer;
	writer.write_buffer(0, shadowDataBuffer.buffer, sizeof(shadowData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_buffer(1, scene_manager->GetObjectDataBuffer()->buffer,
		sizeof(vkutil::GPUModelInformation) * scene_manager->GetModelCount(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.update_set(engine->_device, globalDescriptor);


	/*
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
	*/
	{
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

		vkCmdBindIndexBuffer(cmd, scene_manager->GetMergedIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);
		//calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.vertexBuffer = *scene_manager->GetMergedDeviceAddress();
		vkCmdPushConstants(cmd, cascadedShadows.shadowPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

		vkCmdDrawIndexedIndirect(cmd, scene_manager->GetMeshPass(vkutil::MaterialPass::shadow_pass)->drawIndirectBuffer.buffer, 0,
			scene_manager->GetMeshPass(vkutil::MaterialPass::shadow_pass)->flat_objects.size(), sizeof(SceneManager::GPUIndirectObject));
	};

}
void ClusteredForwardRenderer::DrawMain(VkCommandBuffer cmd)
{
	ZoneScoped;
	auto main_start = std::chrono::system_clock::now();
	cullBarriers.clear();

	//Begin Compute shader culling passes
	vkutil::cullParams earlyDepthCull;
	earlyDepthCull.viewmat = scene_data.view;
	earlyDepthCull.projmat = scene_data.proj;
	earlyDepthCull.frustrumCull = false;
	earlyDepthCull.occlusionCull = true;
	earlyDepthCull.aabb = false;
	earlyDepthCull.drawDist = mainCamera.getFarClip();
	ExecuteComputeCull(cmd, earlyDepthCull, scene_manager->GetMeshPass(vkutil::MaterialPass::early_depth));

	vkutil::cullParams shadowCull;
	shadowCull.viewmat = cascadeData.lightViewMatrices[1];
	shadowCull.projmat = cascadeData.lightProjMatrices[1];
	shadowCull.frustrumCull = true;
	shadowCull.occlusionCull = false;
	shadowCull.aabb = false;
	glm::vec3 aabbCenter = glm::vec3(0);
	glm::vec3 aabbExtent = glm::vec3(1000);
	shadowCull.aabbmin = aabbCenter - aabbExtent;
	shadowCull.aabbmax = aabbCenter + aabbExtent;
	shadowCull.drawDist = mainCamera.getFarClip();
	ExecuteComputeCull(cmd, shadowCull, scene_manager->GetMeshPass(vkutil::MaterialPass::shadow_pass));


	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, cullBarriers.size(), cullBarriers.data(), 0, nullptr);
	if (readDebugBuffer)
	{
		resource_manager->ReadBackBufferData(cmd, &scene_manager->GetMeshPass(vkutil::MaterialPass::early_depth)->drawIndirectBuffer);
		readDebugBuffer = false;
	}

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

	//GenerateAABB(cmd);
	//Compute shader pass for clustered light culling
	CullLights(cmd);

	VkClearValue geometryClear{ 1.0,1.0,1.0,1.0f };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, &_resolveImage.imageView, &geometryClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.0f;
	VkRenderingAttachmentInfo depthAttachment2 = vkinit::attachment_info(_depthImage.imageView, &_depthResolveImage.imageView, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD);

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
	ReduceDepth(cmd);
}

void ClusteredForwardRenderer::ExecuteComputeCull(VkCommandBuffer cmd, vkutil::cullParams& cullParams, SceneManager::MeshPass* meshPass)
{
	AllocatedBuffer gpuSceneDataBuffer = vkutil::create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,engine);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		vkutil::destroy_buffer(gpuSceneDataBuffer,engine);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(engine->_allocator, gpuSceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(engine->_allocator, gpuSceneDataBuffer.allocation);

	VkDescriptorSet computeCullDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, compute_cull_descriptor_layout);
	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_buffer(1, scene_manager->GetObjectDataBuffer()->buffer, sizeof(vkutil::GPUModelInformation) * scene_manager->GetModelCount(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(2, meshPass->drawIndirectBuffer.buffer, sizeof(SceneManager::GPUIndirectObject) * meshPass->flat_objects.size(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_image(3, _depthPyramid.imageView, depthReductionSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, computeCullDescriptor);

	//cullParams.projmat[1][1] *= -1;
	glm::mat4 projection = cullParams.projmat;
	auto projectionT = glm::transpose(projection);

	glm::vec4 frustumX = BlackKey::NormalizePlane(projectionT[3] + projectionT[0]); // x + w < 0
	glm::vec4 frustumY = BlackKey::NormalizePlane(projectionT[3] + projectionT[1]); // y + w < 0

	vkutil::DrawCullData cullData = {};
	cullData.P00 = projection[0][0];
	cullData.P11 = projection[1][1];
	cullData.znear = mainCamera.getNearClip();
	cullData.zfar = cullParams.drawDist;
	cullData.frustum[0] = frustumX.x;
	cullData.frustum[1] = frustumX.z;
	cullData.frustum[2] = frustumY.y;
	cullData.frustum[3] = frustumY.z;
	cullData.drawCount = static_cast<uint32_t>(meshPass->flat_objects.size());
	cullData.cullingEnabled = cullParams.frustrumCull;
	cullData.lodEnabled = false;
	cullData.occlusionEnabled = cullParams.occlusionCull;
	cullData.lodBase = 10.f;
	cullData.lodStep = 1.5f;
	cullData.pyramidWidth = static_cast<float>(depthPyramidWidth);
	cullData.pyramidHeight = static_cast<float>(depthPyramidHeight);
	cullData.viewMat = cullParams.viewmat;//get_view_matrix();

	cullData.AABBcheck = cullParams.aabb;
	cullData.aabbmin_x = cullParams.aabbmin.x;
	cullData.aabbmin_y = cullParams.aabbmin.y;
	cullData.aabbmin_z = cullParams.aabbmin.z;

	cullData.aabbmax_x = cullParams.aabbmax.x;
	cullData.aabbmax_y = cullParams.aabbmax.y;
	cullData.aabbmax_z = cullParams.aabbmax.z;

	if (cullParams.drawDist > 10000)
	{
		cullData.distanceCheck = false;
	}
	else
	{
		cullData.distanceCheck = true;
	}

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cull_objects_pso.pipeline);

	vkCmdPushConstants(cmd, cull_objects_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vkutil::DrawCullData), &cullData);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cull_objects_pso.layout, 0, 1, &computeCullDescriptor, 0, nullptr);

	vkCmdDispatch(cmd, static_cast<uint32_t>((meshPass->flat_objects.size() / 256) + 1), 1, 1);

	{
		VkBufferMemoryBarrier barrier = vkinit::buffer_barrier(meshPass->drawIndirectBuffer.buffer, engine->_graphicsQueueFamily);
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

		cullBarriers.push_back(barrier);
	}
}

inline uint32_t GetGroupCount(uint32_t threadCount, uint32_t localSize)
{
	return (threadCount + localSize - 1) / localSize;
}

void ClusteredForwardRenderer::ReduceDepth(VkCommandBuffer cmd)
{
	VkImageMemoryBarrier depthReadBarriers[] =
	{
		vkinit::image_barrier(_depthResolveImage.image, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
	};

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, depthReadBarriers);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depth_reduce_pso.pipeline);

	for (int32_t i = 0; i < depthPyramidLevels; ++i)
	{
		VkDescriptorSet depthDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, depth_reduce_descriptor_layout);

		DescriptorWriter writer;

		writer.write_image(0, depthPyramidMips[i], depthReductionSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		if (i == 0)
			writer.write_image(1, _depthResolveImage.imageView, depthReductionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		else
			writer.write_image(1, depthPyramidMips[i - 1], depthReductionSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		writer.update_set(engine->_device, depthDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depth_reduce_pso.layout, 0, 1, &depthDescriptor, 0, nullptr);

		uint32_t levelWidth = depthPyramidWidth >> i;
		uint32_t levelHeight = depthPyramidHeight >> i;
		if (levelHeight < 1) levelHeight = 1;
		if (levelWidth < 1) levelWidth = 1;

		glm::vec2 reduceData = { glm::vec2(levelWidth, levelHeight) };

		vkCmdPushConstants(cmd, depth_reduce_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(reduceData), &reduceData);
		vkCmdDispatch(cmd, GetGroupCount(levelWidth, 32), GetGroupCount(levelHeight, 32), 1);
		VkImageMemoryBarrier reduceBarrier = vkinit::image_barrier(_depthPyramid.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &reduceBarrier);
	}
	VkImageMemoryBarrier depthWriteBarrier = vkinit::image_barrier(_depthResolveImage.image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &depthWriteBarrier);

}

void ClusteredForwardRenderer::DrawPostProcess(VkCommandBuffer cmd)
{
	ZoneScoped;
	vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, _depthResolveImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkClearValue clear{ 1.0f, 1.0f, 1.0f, 1.0f };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_hdrImage.imageView, nullptr, &clear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo hdrRenderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &hdrRenderInfo);
	DrawHdr(cmd);
	vkCmdEndRendering(cmd);
}

void ClusteredForwardRenderer::DrawHdr(VkCommandBuffer cmd)
{
	ZoneScoped;
	std::vector<uint32_t> draws;
	draws.reserve(imageDrawCommands.OpaqueSurfaces.size());

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, _drawImageDescriptorLayout);

	DescriptorWriter writer;
	writer.write_image(0, _resolveImage.imageView, defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(1, _depthResolveImage.imageView, defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, globalDescriptor);

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
		push_constants.material_index = debugDepthTexture ? 1 : 0;

		vkCmdPushConstants(cmd, HdrPSO.renderImagePipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
		vkCmdDraw(cmd, 3, 1, 0, 0);
		};

	draw(imageDrawCommands.OpaqueSurfaces[0]);
}
void ClusteredForwardRenderer::DrawBackground(VkCommandBuffer cmd)
{
	ZoneScoped;
	std::vector<uint32_t> b_draws;
	b_draws.reserve(skyDrawCommands.OpaqueSurfaces.size());
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer skySceneDataBuffer = vkutil::create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, engine);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		vkutil::destroy_buffer(skySceneDataBuffer,engine);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(engine->_allocator, skySceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(engine->_allocator, skySceneDataBuffer.allocation);

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, _skyboxDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, skySceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, _skyImage.imageView, cubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, globalDescriptor);

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
	stats.drawcall_count = 0;
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = vkutil::create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,engine);

	AllocatedBuffer shadowDataBuffer = vkutil::create_buffer(sizeof(shadowData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, engine);

	get_current_frame()._deletionQueue.push_function([=, this]() {
		vkutil::destroy_buffer(gpuSceneDataBuffer,engine);
		vkutil::destroy_buffer(shadowDataBuffer, engine);
		});


	//write the buffer
	void* shadowDataPtr = nullptr;
	vmaMapMemory(engine->_allocator, shadowDataBuffer.allocation, &shadowDataPtr);
	shadowData* ptr = (shadowData*)shadowDataPtr;
	*ptr = shadow_data;
	//memcpy(sceneDataPtr, &shadow_data.lightSpaceMatrices, sizeof(shadowData));
	vmaUnmapMemory(engine->_allocator, shadowDataBuffer.allocation);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	void* sceneDataPtr = nullptr;
	vmaMapMemory(engine->_allocator, gpuSceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(engine->_allocator, gpuSceneDataBuffer.allocation);

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, _gpuSceneDataDescriptorLayout);

	static auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(2, _shadowDepthImage.imageView, depthSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(3, IBL._irradianceCube.imageView, IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(4, IBL._lutBRDF.imageView, IBL._lutBRDFSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(5, IBL._preFilteredCube.imageView, IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_buffer(6, ClusterValues.lightSSBO.buffer, pointData.pointLights.size() * sizeof(PointLight), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(7, ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(8, ClusterValues.lightIndexListSSBO.buffer, totalLightCount * sizeof(uint32_t), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(9, ClusterValues.lightGridSSBO.buffer, ClusterValues.numClusters * sizeof(LightGrid), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(10, scene_manager->GetObjectDataBuffer()->buffer,
		sizeof(vkutil::GPUModelInformation) * scene_manager->GetModelCount(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(11, shadowDataBuffer.buffer, sizeof(shadowData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(engine->_device, globalDescriptor);


	//allocate bindless descriptor

	/*MaterialPipeline* lastPipeline = nullptr;
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

				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1, resource_manager.GetBindlessSet(), 0, nullptr);

			}

			//vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
				//&r.material->materialSet, 0, nullptr);
		}
		if (r.indexBuffer != lastIndexBuffer) {
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;
		push_constants.material_index = r.material->material_index;

		vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

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
	*/


	{
		for (auto pass_enum : forward_passes)
		{
			auto pass = scene_manager->GetMeshPass(pass_enum);
			if (pass->flat_objects.size() > 0)
			{
				stats.drawcall_count++;
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->flat_objects[0].material->pipeline->pipeline);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->flat_objects[0].material->pipeline->layout, 0, 1,
					&globalDescriptor, 0, nullptr);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->flat_objects[0].material->pipeline->layout, 1, 1, resource_manager->GetBindlessSet(), 0, nullptr);


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

				vkCmdBindIndexBuffer(cmd, scene_manager->GetMergedIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);
				//calculate final mesh matrix
				GPUDrawPushConstants push_constants;
				push_constants.vertexBuffer = *scene_manager->GetMergedDeviceAddress();
				vkCmdPushConstants(cmd, pass->flat_objects[0].material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
				vkCmdDrawIndexedIndirect(cmd, scene_manager->GetMeshPass(vkutil::MaterialPass::early_depth)->drawIndirectBuffer.buffer, 0,
					pass->flat_objects.size(), sizeof(SceneManager::GPUIndirectObject));

			}
		}
	}
	// we delete the draw commands now that we processed them
	drawCommands.OpaqueSurfaces.clear();
	drawCommands.TransparentSurfaces.clear();
	draws.clear();
}

void ClusteredForwardRenderer::CullLights(VkCommandBuffer cmd)
{
	CullData culling_information;

	VkDescriptorSet cullingDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, _cullLightsDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, ClusterValues.AABBVolumeGridSSBO.buffer, ClusterValues.numClusters * sizeof(VolumeTileAABB), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(1, ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(2, ClusterValues.lightSSBO.buffer, pointData.pointLights.size() * sizeof(PointLight), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;
	writer.write_buffer(3, ClusterValues.lightIndexListSSBO.buffer, sizeof(uint32_t) * totalLightCount, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(4, ClusterValues.lightGridSSBO.buffer, ClusterValues.numClusters * sizeof(LightGrid), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(5, ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].buffer, sizeof(uint32_t), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.update_set(engine->_device, cullingDescriptor);
	auto view = mainCamera.matrices.view;
	//view[1][1] *= -1;
	culling_information.view = scene_data.view;

	//culling_information.lightCount = pointData.pointLights.size();
	
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cull_lights_pso.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cull_lights_pso.layout, 0, 1, &cullingDescriptor, 0, nullptr);
	vkCmdPushConstants(cmd, cull_lights_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::mat4), &scene_data.view);
	vkutil::InsertDebugLabel(cmd, "Cull Lights", glm::vec4(1, 0, 0, 0));
	vkCmdDispatch(cmd, 16, 9, 24);

	std::vector<VkBufferMemoryBarrier2> light_cull_barriers;
	VkBufferMemoryBarrier2 lightGridBarrier = vkinit::buffer_barrier_2(ClusterValues.lightGridSSBO.buffer, engine->_graphicsQueueFamily);
	lightGridBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	lightGridBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
	lightGridBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	lightGridBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	lightGridBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	lightGridBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	lightGridBarrier.size = VK_WHOLE_SIZE;
	
	/*lightGridBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	lightGridBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	*/
	light_cull_barriers.push_back(lightGridBarrier);

	VkBufferMemoryBarrier2 light_index_barrier = vkinit::buffer_barrier_2(ClusterValues.lightIndexListSSBO.buffer, engine->_graphicsQueueFamily);
	light_index_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	light_index_barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
	light_index_barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	light_index_barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	light_index_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	light_index_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	light_index_barrier.size = VK_WHOLE_SIZE;
	light_cull_barriers.push_back(light_index_barrier);

	//vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, light_cull_barriers.size(), light_cull_barriers.data(), 0, nullptr);
	VkDependencyInfo barrier_info{
	.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
	.pNext = nullptr,
	.bufferMemoryBarrierCount = (uint32_t)light_cull_barriers.size(),
	.pBufferMemoryBarriers = light_cull_barriers.data(),
	};
	//barrier_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	//barrier_info.pBufferMemoryBarriers = light_cull_barriers.data();
	//barrier_info.bufferMemoryBarrierCount = static_cast<uint32_t>(light_cull_barriers.size());
	vkCmdPipelineBarrier2(cmd, &barrier_info);
}

void ClusteredForwardRenderer::GenerateAABB(VkCommandBuffer cmd)
{
	CullData culling_information;

	VkDescriptorSet aabbDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, _buildClustersDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, ClusterValues.AABBVolumeGridSSBO.buffer, ClusterValues.numClusters * sizeof(VolumeTileAABB), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(1, ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.update_set(engine->_device, aabbDescriptor);

	clusterParams clusterData;
	clusterData.zNear = mainCamera.getNearClip();
	clusterData.zFar = mainCamera.getFarClip();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, generate_clusters_pso.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, generate_clusters_pso.layout, 0, 1, &aabbDescriptor, 0, nullptr);

	vkCmdPushConstants(cmd, generate_clusters_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(clusterParams), &clusterData);
	vkCmdDispatch(cmd, 16, 9, 24);

	std::vector<VkBufferMemoryBarrier2> cluster_generate_barriers;
	VkBufferMemoryBarrier2 clusterBarrier = vkinit::buffer_barrier_2(ClusterValues.AABBVolumeGridSSBO.buffer, engine->_graphicsQueueFamily);
	clusterBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	clusterBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
	clusterBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	clusterBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	clusterBarrier.size = ClusterValues.AABBVolumeGridSSBO.info.size;

	/*lightGridBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	lightGridBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	*/
	cluster_generate_barriers.push_back(clusterBarrier);

	VkDependencyInfo barrier_info{
	.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
	.pNext = nullptr,
	.bufferMemoryBarrierCount = (uint32_t)cluster_generate_barriers.size(),
	.pBufferMemoryBarriers = cluster_generate_barriers.data(),
	};
	vkCmdPipelineBarrier2(cmd, &barrier_info);
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
		if (black_key::is_visible(drawCommands.OpaqueSurfaces[i], scene_data.viewproj)) {
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
	AllocatedBuffer gpuSceneDataBuffer = vkutil::create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, engine);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		vkutil::destroy_buffer(gpuSceneDataBuffer,engine);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(engine->_allocator, gpuSceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(engine->_allocator, gpuSceneDataBuffer.allocation);

	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, _gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_buffer(6, scene_manager->GetObjectDataBuffer()->buffer,
		sizeof(vkutil::GPUModelInformation) * scene_manager->GetModelCount(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.update_set(engine->_device, globalDescriptor);

	/*
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
		push_constants.material_index = r.material->material_index;

		vkCmdPushConstants(cmd, depthPrePassPSO.earlyDepthPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		};

	for (auto& r : draws) {
		draw(drawCommands.OpaqueSurfaces[r]);
	}
	*/


	{
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

		vkCmdBindIndexBuffer(cmd, scene_manager->GetMergedIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);
		//calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.vertexBuffer = *scene_manager->GetMergedDeviceAddress();
		vkCmdPushConstants(cmd, depthPrePassPSO.earlyDepthPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

		vkCmdDrawIndexedIndirect(cmd, scene_manager->GetMeshPass(vkutil::MaterialPass::early_depth)->drawIndirectBuffer.buffer, 0,
			scene_manager->GetMeshPass(vkutil::MaterialPass::early_depth)->flat_objects.size(), sizeof(SceneManager::GPUIndirectObject));
	}
}

void ClusteredForwardRenderer::Run()
{
	bool bQuit = false;


	// main loop
	while (!glfwWindowShouldClose(engine->window)) {
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
	vkDeviceWaitIdle(engine->_device);

	DestroySwapchain();

	int w, h;
	glfwGetWindowSize(engine->window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	_aspect_width = w;
	_aspect_height = h;

	CreateSwapchain(_windowExtent.width, _windowExtent.height);

	//Destroy and recreate render targets
	resource_manager->DestroyImage(_drawImage);
	resource_manager->DestroyImage(_hdrImage);
	resource_manager->DestroyImage(_resolveImage);

	VkExtent3D ImageExtent{
		_aspect_width,
		_aspect_height,
		1
	};
	_drawImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, engine, VK_IMAGE_VIEW_TYPE_2D, false, 1, VK_SAMPLE_COUNT_4_BIT);
	_hdrImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, engine);
	_resolveImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, engine);

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
						pointData.pointLights[i].position = glm::vec4(pos[0], pos[1], pos[2], 1.0f);
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
						ImGui::SliderFloat("Range", &pointData.pointLights[i].range, 0.0f, 15.0f);
						ImGui::SliderFloat("Intensity", &pointData.pointLights[i].intensity, 0.0f, 5.0f);
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
		ImGui::Checkbox("Read buffer", &readDebugBuffer);
		ImGui::Checkbox("Display buffer", &debugBuffer);
		ImGui::Checkbox("Visualize depth texure", &debugDepthTexture);
		ImGui::Checkbox("Visualize rendered light count", &debugLightClustering);
		std::string breh;
		if (debugBuffer)
		{
			auto buffer = resource_manager->GetReadBackBuffer();
			void* data_ptr = nullptr;
			std::vector<uint32_t> buffer_values;
			buffer_values.resize(buffer->info.size);
			vmaMapMemory(engine->_allocator, buffer->allocation, &data_ptr);
			uint32_t* buffer_ptr = (uint32_t*)data_ptr;
			memcpy(buffer_values.data(), buffer_ptr, buffer->info.size);
			vmaUnmapMemory(engine->_allocator, buffer->allocation);


			for (size_t i = 0; i < buffer_values.size(); i++)
			{
				auto string = std::to_string((double)buffer_values[i]);
				breh += string + " ";

				if (i % 10 == 0)
					breh += "\n";
			}

		}
		ImGui::Text(breh.c_str());
	}

	if (ImGui::CollapsingHeader("Engine Stats"))
	{
		ImGui::SeparatorText("Render timings");
		ImGui::Text("FPS %f ", 1000.0f / stats.frametime);
		ImGui::Text("frametime %f ms", stats.frametime);
		ImGui::Text("drawtime %f ms", stats.mesh_draw_time);
		ImGui::Text("Triangles: %i", stats.triangle_count);
		ImGui::Text("Indirect Draws: %i", stats.drawcall_count);
		ImGui::Text("UI render time %f ms", stats.ui_draw_time);
		ImGui::Text("Update time %f ms", stats.update_time);
		ImGui::Text("Shadow Pass time %f ms", stats.shadow_pass_time);
		ImGui::Text("Number of active point light %i", static_cast<int>(pointData.pointLights.size()));
	}
	ImGui::End();
}


void SetImguiTheme(float alpha)
{
	ImGuiStyle& style = ImGui::GetStyle();

	// light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
	style.Alpha = 1.0f;
	style.FrameRounding = 3.0f;
	style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.2f, 0.2f, 0.40f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	//ImGuiCol_::hove

	for (int i = 0; i <= ImGuiCol_COUNT; i++)
	{
		ImVec4& col = style.Colors[i];
		if (col.w < 1.00f)
		{
			col.x *= alpha;
			col.y *= alpha;
			col.z *= alpha;
			col.w *= alpha;
		}
	}
}
