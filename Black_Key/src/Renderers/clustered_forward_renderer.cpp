#include "clustered_forward_renderer.h"


#include <VkBootstrap.h>

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

	InitSwapchain();

	InitRenderTargets();

	init_commands();

	init_sync_structures();

	init_descriptors();

	InitDefaultData();

	init_buffers();

	init_pipelines();

	init_imgui();

	LoadAssets();

	pre_process_pass();
	_isInitialized = true;
}

void ClusteredForwardRenderer::InitSwapchain()
{
	create_swapchain(_windowExtent.width, _windowExtent.height);
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

void ClusteredForwardRenderer::create_swapchain(uint32_t width, uint32_t height)
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