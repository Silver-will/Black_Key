#include "clustered_forward_renderer.h"
#include "../graphics.h"

#include <VkBootstrap.h>

#include <chrono>
#include <thread>
#include <iostream>
#include <random>



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

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetCursorPosCallback(window, CursorCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
	vmaCreateImage(loaded_engine->_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);
	vmaSetAllocationName(loaded_engine->_allocator, _drawImage.allocation, "Draw image");

	//Create resolve image for multisampling
	VkImageCreateInfo resolve_img_info = vkinit::image_create_info(_resolveImage.imageFormat, resolveImageUsages, drawImageExtent, 1);
	vmaCreateImage(loaded_engine->_allocator, &resolve_img_info, &rimg_allocinfo, &_resolveImage.image, &_resolveImage.allocation, nullptr);
	vmaSetAllocationName(loaded_engine->_allocator, _resolveImage.allocation, "resolve image");

	vmaCreateImage(loaded_engine->_allocator, &resolve_img_info, &rimg_allocinfo, &_hdrImage.image, &_hdrImage.allocation, nullptr);
	vmaSetAllocationName(loaded_engine->_allocator, _hdrImage.allocation, "hdr image");

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	VkImageViewCreateInfo resolve_view_info = vkinit::imageview_create_info(_resolveImage.imageFormat, _resolveImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	VkImageViewCreateInfo hdr_view_info = vkinit::imageview_create_info(_hdrImage.imageFormat, _hdrImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(loaded_engine->_device, &rview_info, nullptr, &_drawImage.imageView));
	VK_CHECK(vkCreateImageView(loaded_engine->_device, &resolve_view_info, nullptr, &_resolveImage.imageView));
	VK_CHECK(vkCreateImageView(loaded_engine->_device, &hdr_view_info, nullptr, &_hdrImage.imageView));


	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthImageUsages |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent, 1, msaa_samples);

	//allocate and create the image
	vmaCreateImage(loaded_engine->_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(loaded_engine->_device, &dview_info, nullptr, &_depthImage.imageView));


	loaded_engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(loaded_engine->_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(loaded_engine->_allocator, _drawImage.image, _drawImage.allocation);

		vkDestroyImageView(loaded_engine->_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(loaded_engine->_allocator, _depthImage.image, _depthImage.allocation);

		vkDestroyImageView(loaded_engine->_device, _resolveImage.imageView, nullptr);
		vmaDestroyImage(loaded_engine->_allocator, _resolveImage.image, _resolveImage.allocation);

		vkDestroyImageView(loaded_engine->_device, _hdrImage.imageView, nullptr);
		vmaDestroyImage(loaded_engine->_allocator, _hdrImage.image, _hdrImage.allocation);
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
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
	};

	globalDescriptorAllocator.init_pool(loaded_engine->_device, 30, sizes);
	loaded_engine->_mainDeletionQueue.push_function(
		[&]() { vkDestroyDescriptorPool(loaded_engine->_device, globalDescriptorAllocator.pool, nullptr); });

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_drawImageDescriptorLayout = builder.build(loaded_engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);
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
		_gpuSceneDataDescriptorLayout = builder.build(loaded_engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		_shadowSceneDescriptorLayout = builder.build(loaded_engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	}
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_skyboxDescriptorLayout = builder.build(loaded_engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		_cullLightsDescriptorLayout = builder.build(loaded_engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		_buildClustersDescriptorLayout = builder.build(loaded_engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	loaded_engine->_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(loaded_engine->_device, _drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(loaded_engine->_device, _gpuSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(loaded_engine->_device, _skyboxDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(loaded_engine->_device, _shadowSceneDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(loaded_engine->_device, _cullLightsDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(loaded_engine->_device, _buildClustersDescriptorLayout, nullptr);
		});

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
		_frames[i]._frameDescriptors.init(loaded_engine->_device, 1000, frame_sizes);
		loaded_engine->_mainDeletionQueue.push_function([&, i]() {
			_frames[i]._frameDescriptors.destroy_pools(loaded_engine->_device);
			});
	}
}

void ClusteredForwardRenderer::InitPipelines()
{
	InitComputePipelines();
	metalRoughMaterial.build_pipelines(loaded_engine);
	cascadedShadows.build_pipelines(loaded_engine);
	skyBoxPSO.build_pipelines(loaded_engine);
	HdrPSO.build_pipelines(loaded_engine);
	depthPrePassPSO.build_pipelines(loaded_engine);
	loaded_engine->_mainDeletionQueue.push_function([&]()
		{
			depthPrePassPSO.clear_resources(loaded_engine->_device);
			metalRoughMaterial.clear_resources(loaded_engine->_device);
			cascadedShadows.clear_resources(loaded_engine->_device);
			skyBoxPSO.clear_resources(loaded_engine->_device);
			HdrPSO.clear_resources(loaded_engine->_device);
		});

	loaded_engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(loaded_engine->_device, _shadowDepthImage.imageView, nullptr);
		vmaDestroyImage(loaded_engine->_allocator, _shadowDepthImage.image, _shadowDepthImage.allocation);
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

	VK_CHECK(vkCreatePipelineLayout(loaded_engine->_device, &cullLightsLayoutInfo, nullptr, &_cullLightsPipelineLayout));

	VkShaderModule cullLightShader;
	if (!vkutil::load_shader_module("shaders/cluster_cull_light_shader.spv", loaded_engine->_device, &cullLightShader)) {
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

	VkPipeline cullingPipeline;
	//default colors

	VK_CHECK(vkCreateComputePipelines(loaded_engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_cullLightsPipeline));


	loaded_engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(loaded_engine->_device, _cullLightsPipelineLayout, nullptr);
		vkDestroyPipeline(loaded_engine->_device, _cullLightsPipeline, nullptr);
		});
}

void ClusteredForwardRenderer::InitDefaultData()
{
	directLight = DirectionalLight(glm::normalize(glm::vec4(-20.0f, -50.0f, -20.0f, 1.f)), glm::vec4(1.5f), glm::vec4(1.0f));
	//Create Shadow render target
	_shadowDepthImage = vkutil::create_image_empty(VkExtent3D(2048, 2048, 1), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, loaded_engine, VK_IMAGE_VIEW_TYPE_2D_ARRAY, false, shadows.getCascadeLevels());

	//Create default images
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = vkutil::create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, loaded_engine);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = vkutil::create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, loaded_engine);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = vkutil::create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, loaded_engine);

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

	//Load in skyBox image
	_skyImage = vkutil::load_cubemap_image("assets/textures/hdris/overcast.ktx", VkExtent3D{ 1,1,1 }, loaded_engine, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, true);

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	_errorCheckerboardImage = vkutil::create_image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, loaded_engine);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(loaded_engine->_device, &sampl, nullptr, &_defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(loaded_engine->_device, &sampl, nullptr, &_defaultSamplerLinear);

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
	vkCreateSampler(loaded_engine->_device, &cubeSampl, nullptr, &_cubeMapSampler);

	cubeSampl.maxLod = 1;
	cubeSampl.maxAnisotropy = 1.0f;
	vkCreateSampler(loaded_engine->_device, &cubeSampl, nullptr, &_depthSampler);

	//< default_img

	loaded_engine->_mainDeletionQueue.push_function([=]() {
		loaded_engine->destroy_image(_whiteImage);
		loaded_engine->destroy_image(_blackImage);
		loaded_engine->destroy_image(_greyImage);
		loaded_engine->destroy_image(_errorCheckerboardImage);
		loaded_engine->destroy_image(_skyImage);
		vkDestroySampler(loaded_engine->_device, _defaultSamplerLinear, nullptr);
		vkDestroySampler(loaded_engine->_device, _defaultSamplerNearest, nullptr);
		vkDestroySampler(loaded_engine->_device, _cubeMapSampler, nullptr);
		vkDestroySampler(loaded_engine->_device, _depthSampler, nullptr);
		});
}

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
	VK_CHECK(vkCreateDescriptorPool(loaded_engine->_device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// this initializes imgui for SDL
	ImGui_ImplGlfw_InitForVulkan(window, true);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = loaded_engine->_instance;
	init_info.PhysicalDevice = loaded_engine->_chosenGPU;
	init_info.Device = loaded_engine->_device;
	init_info.Queue = loaded_engine->_graphicsQueue;
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
	loaded_engine->_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(loaded_engine->_device, imguiPool, nullptr);
		});
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

	void* data = ClusterValues.lightSSBO.allocation->GetMappedData();
	memcpy(data, pointData.pointLights.data(), pointData.pointLights.size() * sizeof(PointLight));

	uint32_t* val = (uint32_t*)ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].allocation->GetMappedData();
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
	//auto app = reinterpret_cast<ClusteredForwardRenderer*>(glfwGetWindowUserPointer(window));
	//app->mainCamera.processKeyInput(window, key, action);
}

void ClusteredForwardRenderer::CursorCallback(GLFWwindow* window, double xpos, double ypos)
{
	//auto app = reinterpret_cast<ClusteredForwardRenderer*>(glfwGetWindowUserPointer(window));
	//app->mainCamera.processMouseMovement(window, xpos, ypos);
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

}