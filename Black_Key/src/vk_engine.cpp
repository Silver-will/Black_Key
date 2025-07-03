#include "vk_engine.h"

#include "vk_initializers.h"
#include "vk_types.h"
#include "input_handler.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_buffer.h"
#include "vk_loader.h"
#include "Lights.h"
#include "graphics.h"
#include "UI.h"

#include <VkBootstrap.h>

#include <chrono>
#include <thread>
#include <iostream>
#include <random>

#ifdef _DEBUG
constexpr bool bUseValidationLayers = true;
#else
constexpr bool bUseValidationLayers = false;
#endif

#include "../../tracy/public/tracy/Tracy.hpp"
#include <vma/vk_mem_alloc.h>


#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <iostream>

#define USE_BINDLESS

VulkanEngine* loadedEngine = nullptr;



VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{

	mainCamera.type = Camera::CameraType::firstperson;
	//mainCamera.flipY = true;
	mainCamera.movementSpeed = 2.5f;
	mainCamera.setPerspective(60.0f, (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 1000.0f);
	mainCamera.setPosition(glm::vec3(-0.12f, -5.14f, -2.25f));
	mainCamera.setRotation(glm::vec3(-17.0f, 7.0f, 0.0f));
	// only one engine initialization is allowed with the application.
	assert(loadedEngine == nullptr);
	loadedEngine = this;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(_windowExtent.width, _windowExtent.height, "Black key", nullptr, nullptr);
	if (window == nullptr)
		throw std::exception("FATAL ERROR: Failed to create window");

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursor_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	init_vulkan();

	init_swapchain();

	init_commands();

	init_sync_structures();

	init_descriptors();

	init_default_data();

	init_buffers();

	init_pipelines();

	init_imgui();

	load_assets();

	pre_process_pass();
	_isInitialized = true;
}

void VulkanEngine::load_assets()
{

	resource_manager.init(this);
	scene_manager.Init(&resource_manager, this);

	//Load in skyBox image
	_skyImage = vkutil::load_cubemap_image("assets/textures/hdris/overcast.ktx", VkExtent3D{ 1,1,1 }, this, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, true);

	std::string structurePath{ "assets/sponza/Sponza.gltf" };
	auto structureFile = resource_manager.loadGltf(this, structurePath, true);
	assert(structureFile.has_value());
	
	std::string cubePath{ "assets/cube.gltf" };
	auto cubeFile = resource_manager.loadGltf(this, cubePath);
	assert(cubeFile.has_value());

	//std::string structurePath{ "assets/SM_Deccer_Cubes_Textured_Complex.gltf" };
	

	std::string planePath{ "assets/plane.glb" };
	auto planeFile = resource_manager.loadGltf(this, planePath);
	assert(planeFile.has_value());

	loadedScenes["sponza"] = *structureFile;
	loadedScenes["cube"] = *cubeFile;
	loadedScenes["plane"] = *planeFile;

	loadedScenes["sponza"]->Draw(glm::mat4{ 1.f }, drawCommands);
	scene_manager.RegisterMeshAssetReference("sponza");
	//Register render objects for draw indirect
	scene_manager.RegisterObjectBatch(drawCommands);
	scene_manager.MergeMeshes();
	scene_manager.PrepareIndirectBuffers();
	scene_manager.BuildBatches();

#ifdef USE_BINDLESS 1
	resource_manager.write_material_array();
#endif // USE_BINDLESS
}

void VulkanEngine::pre_process_pass()
{
	black_key::generate_irradiance_cube(this);
	black_key::generate_prefiltered_cubemap(this);
	black_key::generate_brdf_lut(this);
	black_key::build_clusters(this);

	_mainDeletionQueue.push_function([=]() {
		destroy_image(IBL._irradianceCube);
		destroy_image(IBL._preFilteredCube);
		destroy_image(IBL._lutBRDF);

		//destroy_image(_shadowDepthImage);
		vkDestroySampler(_device, IBL._irradianceCubeSampler, nullptr);
		vkDestroySampler(_device, IBL._lutBRDFSampler, nullptr);
		});
}

void VulkanEngine::cleanup()
{
	if (_isInitialized) {

		// make sure the gpu has stopped doing its things
		vkDeviceWaitIdle(_device);

		loadedScenes.clear();

		for (auto& frame : _frames) {
			frame._deletionQueue.flush();
		}

		_mainDeletionQueue.flush();

		destroy_swapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vmaDestroyAllocator(_allocator);

		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		glfwDestroyWindow(window);
	}

	// clear engine pointer
	loadedEngine = nullptr;
	glfwTerminate();
}



void VulkanEngine::draw()
{
	ZoneScoped;
	auto start_update = std::chrono::system_clock::now();
	//wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));


	auto end_update = std::chrono::system_clock::now();
	auto elapsed_update = std::chrono::duration_cast<std::chrono::microseconds>(end_update - start_update);
	stats.update_time = elapsed_update.count() / 1000.f;

	get_current_frame()._deletionQueue.flush();
	get_current_frame()._frameDescriptors.clear_pools(_device);

	//request image from the swapchain
	uint32_t swapchainImageIndex;
	VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);

	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}

	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height);
	_drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width);

	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

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

	draw_main(cmd);

	draw_post_process(cmd);

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
	draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

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
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

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

	VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}
	//increase the number of frames drawn
	_frameNumber++;

}


void VulkanEngine::draw_post_process(VkCommandBuffer cmd)
{
	ZoneScoped;
	vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, _depthResolveImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkClearValue clear{ 1.0f, 1.0f, 1.0f, 1.0f };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_hdrImage.imageView, nullptr, &clear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo hdrRenderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &hdrRenderInfo);
	draw_hdr(cmd);
	vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_main(VkCommandBuffer cmd)
{
	ZoneScoped;
	auto main_start = std::chrono::system_clock::now();
	cullBarriers.clear();

	//Begin Compute shader culling passes
	vkutil::cullParams earlyDepthCull;
	earlyDepthCull.viewmat = scene_data.view;
	earlyDepthCull.projmat = scene_data.proj;
	earlyDepthCull.frustrumCull = true;
	earlyDepthCull.occlusionCull = true;
	earlyDepthCull.aabb = false;
	earlyDepthCull.drawDist = mainCamera.getFarClip();
	execute_compute_cull(cmd, earlyDepthCull, scene_manager.GetMeshPass(vkutil::MaterialPass::early_depth));

	vkutil::cullParams shadowCull;
	shadowCull.viewmat = cascadeData.lightViewMatrices[3];
	shadowCull.projmat = cascadeData.lightProjMatrices[3];
	shadowCull.frustrumCull = true;
	shadowCull.occlusionCull = false;
	shadowCull.aabb = true;
	glm::vec3 aabbCenter = glm::vec3(0);
	glm::vec3 aabbExtent = glm::vec3(1000);
	shadowCull.aabbmin = aabbCenter - aabbExtent;
	shadowCull.aabbmax = aabbCenter + aabbExtent;
	shadowCull.drawDist = mainCamera.getFarClip();
	execute_compute_cull(cmd, shadowCull, scene_manager.GetMeshPass(vkutil::MaterialPass::shadow_pass));


	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, cullBarriers.size(), cullBarriers.data(), 0, nullptr);
	if (readDebugBuffer)
	{
		resource_manager.ReadBackBufferData(cmd, &scene_manager.GetMeshPass(vkutil::MaterialPass::early_depth)->drawIndirectBuffer);
		readDebugBuffer = false;
	}

	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	VkRenderingInfo earlyDepthRenderInfo = vkinit::rendering_info(_windowExtent, nullptr, &depthAttachment);
	vkCmdBeginRendering(cmd, &earlyDepthRenderInfo);

	draw_early_depth(cmd);

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

		draw_shadows(cmd);

		vkCmdEndRendering(cmd);
		auto endShadow = std::chrono::system_clock::now();
		auto elapsedShadow = std::chrono::duration_cast<std::chrono::microseconds>(endShadow - startShadow);
		stats.shadow_pass_time = elapsedShadow.count() / 1000.f;
		render_shadowMap = false;
	}

	//Compute shader pass for clustered light culling
	cull_lights(cmd);

	VkClearValue geometryClear{ 1.0,1.0,1.0,1.0f };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, &_resolveImage.imageView, &geometryClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.0f;
	VkRenderingAttachmentInfo depthAttachment2 = vkinit::attachment_info(_depthImage.imageView,&_depthResolveImage.imageView,&depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD);

	vkutil::transition_image(cmd, _shadowDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment2);

	auto start = std::chrono::system_clock::now();
	vkCmdBeginRendering(cmd, &renderInfo);

	draw_geometry(cmd);

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

	draw_background(cmd);

	vkCmdEndRendering(cmd);

	reduce_depth(cmd);
}

void VulkanEngine::cull_lights(VkCommandBuffer cmd)
{
	CullData cullingInformation;

	VkDescriptorSet cullingDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _cullLightsDescriptorLayout);

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
	writer.update_set(_device, cullingDescriptor);

	cullingInformation.view = mainCamera.matrices.view;
	cullingInformation.lightCount = pointData.pointLights.size();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _cullLightsPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _cullLightsPipelineLayout, 0, 1, &cullingDescriptor, 0, nullptr);

	vkCmdPushConstants(cmd, _cullLightsPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullData), &cullingInformation);
	vkCmdDispatch(cmd, 16, 9, 24);
}


void VulkanEngine::execute_compute_cull(VkCommandBuffer cmd, vkutil::cullParams& cullParams, SceneManager::MeshPass* meshPass)
{

	AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		destroy_buffer(gpuSceneDataBuffer);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(_allocator, gpuSceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(_allocator, gpuSceneDataBuffer.allocation);

	VkDescriptorSet computeCullDescriptor = get_current_frame()._frameDescriptors.allocate(_device, compute_cull_descriptor_layout);
	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_buffer(1, scene_manager.GetObjectDataBuffer()->buffer, sizeof(vkutil::GPUModelInformation) * scene_manager.GetModelCount(),0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(2, meshPass->drawIndirectBuffer.buffer, sizeof(SceneManager::GPUIndirectObject) * meshPass->flat_objects.size(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_image(3, _depthPyramid.imageView, depthReductionSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(_device, computeCullDescriptor);

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
		VkBufferMemoryBarrier barrier = vkinit::buffer_barrier(meshPass->drawIndirectBuffer.buffer, _graphicsQueueFamily);
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

		cullBarriers.push_back(barrier);
	}
}

inline uint32_t getGroupCount(uint32_t threadCount, uint32_t localSize)
{
	return (threadCount + localSize - 1) / localSize;
}

void VulkanEngine::reduce_depth(VkCommandBuffer cmd)
{
	VkImageMemoryBarrier depthReadBarriers[] =
	{
		vkinit::image_barrier(_depthResolveImage.image, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
	};
	
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, depthReadBarriers);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depth_reduce_pso.pipeline);

	for (int32_t i = 0; i < depthPyramidLevels; ++i)
	{
		VkDescriptorSet depthDescriptor = get_current_frame()._frameDescriptors.allocate(_device, depth_reduce_descriptor_layout);

		DescriptorWriter writer;
		
		writer.write_image(0, depthPyramidMips[i], depthReductionSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		if (i == 0)
			writer.write_image(1, _depthResolveImage.imageView, depthReductionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		else
			writer.write_image(1, depthPyramidMips[i - 1], depthReductionSampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		writer.update_set(_device, depthDescriptor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depth_reduce_pso.layout, 0, 1, &depthDescriptor, 0, nullptr);

		uint32_t levelWidth = depthPyramidWidth >> i;
		uint32_t levelHeight = depthPyramidHeight >> i;
		if (levelHeight < 1) levelHeight = 1;
		if (levelWidth < 1) levelWidth = 1;

		glm::vec2 reduceData = { glm::vec2(levelWidth, levelHeight) };

		vkCmdPushConstants(cmd, depth_reduce_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(reduceData), &reduceData);
		vkCmdDispatch(cmd, getGroupCount(levelWidth, 32), getGroupCount(levelHeight, 32), 1);


		VkImageMemoryBarrier reduceBarrier = vkinit::image_barrier(_depthPyramid.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &reduceBarrier);
	}
	VkImageMemoryBarrier depthWriteBarrier = vkinit::image_barrier(_depthResolveImage.image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &depthWriteBarrier);

}

void VulkanEngine::draw_early_depth(VkCommandBuffer cmd)
{

	//Quick frustum culling pass
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
	AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		destroy_buffer(gpuSceneDataBuffer);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(_allocator, gpuSceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(_allocator, gpuSceneDataBuffer.allocation);
	
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, gpu_scene_data_descriptor_layout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_buffer(6, scene_manager.GetObjectDataBuffer()->buffer,
		sizeof(vkutil::GPUModelInformation) * scene_manager.GetModelCount(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.update_set(_device, globalDescriptor);

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
		
		vkCmdBindIndexBuffer(cmd, scene_manager.GetMergedIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);
		//calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.vertexBuffer = *scene_manager.GetMergedDeviceAddress();
		vkCmdPushConstants(cmd, depthPrePassPSO.earlyDepthPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
		
		vkCmdDrawIndexedIndirect(cmd, scene_manager.GetMeshPass(vkutil::MaterialPass::early_depth)->drawIndirectBuffer.buffer, 0,
			scene_manager.GetMeshPass(vkutil::MaterialPass::early_depth)->flat_objects.size(), sizeof(SceneManager::GPUIndirectObject));
	}
}


void VulkanEngine::draw_shadows(VkCommandBuffer cmd)
{
	AllocatedBuffer shadowDataBuffer = create_buffer(sizeof(shadowData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		destroy_buffer(shadowDataBuffer);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(_allocator, shadowDataBuffer.allocation, &sceneDataPtr);
	shadowData* ptr = (shadowData*)sceneDataPtr;
	*ptr = shadow_data;
	//memcpy(sceneDataPtr, &shadow_data.lightSpaceMatrices, sizeof(shadowData));
	vmaUnmapMemory(_allocator, shadowDataBuffer.allocation);

	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, cascaded_shadows_descriptor_layout);

	DescriptorWriter writer;
	writer.write_buffer(0, shadowDataBuffer.buffer, sizeof(shadowData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_buffer(1, scene_manager.GetObjectDataBuffer()->buffer,
		sizeof(vkutil::GPUModelInformation) * scene_manager.GetModelCount(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.update_set(_device, globalDescriptor);

	
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

		vkCmdBindIndexBuffer(cmd, scene_manager.GetMergedIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);
		//calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.vertexBuffer = *scene_manager.GetMergedDeviceAddress();
		vkCmdPushConstants(cmd, cascadedShadows.shadowPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

		vkCmdDrawIndexedIndirect(cmd, scene_manager.GetMeshPass(vkutil::MaterialPass::shadow_pass)->drawIndirectBuffer.buffer, 0,
			scene_manager.GetMeshPass(vkutil::MaterialPass::shadow_pass)->flat_objects.size(), sizeof(SceneManager::GPUIndirectObject));
	};
	
}


void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
	ZoneScoped;
	std::vector<uint32_t> b_draws;
	b_draws.reserve(skyDrawCommands.OpaqueSurfaces.size());
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer skySceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		destroy_buffer(skySceneDataBuffer);
		});


	void* skySceneDataPtr = nullptr;
	vmaMapMemory(_allocator, skySceneDataBuffer.allocation, &skySceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)skySceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(_allocator, skySceneDataBuffer.allocation);
	//write the buffer
	//GPUSceneData* sceneUniformData = (GPUSceneData*)skySceneDataBuffer.allocation->GetMappedData();
	//*sceneUniformData = sceneData;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _skyboxDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, skySceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, _skyImage.imageView, cubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(_device, globalDescriptor);

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


void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
	ZoneScoped;
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	
	AllocatedBuffer shadowDataBuffer = create_buffer(sizeof(shadowData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	get_current_frame()._deletionQueue.push_function([=, this]() {
		destroy_buffer(gpuSceneDataBuffer);
		destroy_buffer(shadowDataBuffer);
		});


	//write the buffer
	void* shadowDataPtr = nullptr;
	vmaMapMemory(_allocator, shadowDataBuffer.allocation, &shadowDataPtr);
	shadowData* ptr = (shadowData*)shadowDataPtr;
	*ptr = shadow_data;
	//memcpy(sceneDataPtr, &shadow_data.lightSpaceMatrices, sizeof(shadowData));
	vmaUnmapMemory(_allocator, shadowDataBuffer.allocation);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	void* sceneDataPtr = nullptr;
	vmaMapMemory(_allocator, gpuSceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(_allocator, gpuSceneDataBuffer.allocation);

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, gpu_scene_data_descriptor_layout);

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
	writer.write_buffer(10, scene_manager.GetObjectDataBuffer()->buffer,
		sizeof(vkutil::GPUModelInformation) * scene_manager.GetModelCount(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	writer.write_buffer(11, shadowDataBuffer.buffer, sizeof(shadowData),0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_device, globalDescriptor);

	
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
			auto pass = scene_manager.GetMeshPass(pass_enum);
			if(pass->flat_objects.size() > 0)
			{ 
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->flat_objects[0].material->pipeline->pipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->flat_objects[0].material->pipeline->layout, 0, 1,
				&globalDescriptor, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->flat_objects[0].material->pipeline->layout, 1, 1, resource_manager.GetBindlessSet(), 0, nullptr);


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

			vkCmdBindIndexBuffer(cmd, scene_manager.GetMergedIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);
			//calculate final mesh matrix
			GPUDrawPushConstants push_constants;
			push_constants.vertexBuffer = *scene_manager.GetMergedDeviceAddress();
			vkCmdPushConstants(cmd, pass->flat_objects[0].material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
			vkCmdDrawIndexedIndirect(cmd, scene_manager.GetMeshPass(vkutil::MaterialPass::early_depth)->drawIndirectBuffer.buffer, 0,
				pass->flat_objects.size(), sizeof(SceneManager::GPUIndirectObject));

			}
		}
	}
	// we delete the draw commands now that we processed them
	drawCommands.OpaqueSurfaces.clear();
	drawCommands.TransparentSurfaces.clear();
	draws.clear();
}

void VulkanEngine::draw_hdr(VkCommandBuffer cmd)
{
	ZoneScoped;
	std::vector<uint32_t> draws;
	draws.reserve(imageDrawCommands.OpaqueSurfaces.size());

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _drawImageDescriptorLayout);

	DescriptorWriter writer;
	writer.write_image(0, _resolveImage.imageView, defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(1, _depthResolveImage.imageView, defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(_device, globalDescriptor);

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

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
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

void VulkanEngine::resize_swapchain()
{
	vkDeviceWaitIdle(_device);

	destroy_swapchain();

	int w, h;
	glfwGetWindowSize(window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	_aspect_width = w;
	_aspect_height = h;

	create_swapchain(_windowExtent.width, _windowExtent.height);

	//Destroy and recreate render targets
	destroy_image(_drawImage);
	destroy_image(_hdrImage);
	destroy_image(_resolveImage);

	VkExtent3D ImageExtent{
		_aspect_width,
		_aspect_height,
		1
	};
	_drawImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, this, VK_IMAGE_VIEW_TYPE_2D, false, 1, VK_SAMPLE_COUNT_4_BIT);
	_hdrImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, this);
	_resolveImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, this);

	resize_requested = false;
}

void VulkanEngine::run()
{
	bool bQuit = false;


	// main loop
	while (!glfwWindowShouldClose(window)) {
		auto start = std::chrono::system_clock::now();
		if (resize_requested) {
			resize_swapchain();
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
		RenderUI(this);

		//ImGui::ShowDemoWindow();
		ImGui::Render();


		auto start_update = std::chrono::system_clock::now();
		update_scene();
		auto end_update = std::chrono::system_clock::now();
		auto elapsed_update = std::chrono::duration_cast<std::chrono::microseconds>(end_update - start_update);

		draw();

		glfwPollEvents();
		auto end = std::chrono::system_clock::now();

		//convert to microseconds (integer), and then come back to miliseconds
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		stats.frametime = elapsed.count() / 1000.f;
	}
	FrameMark;
}

void VulkanEngine::init_vulkan()
{
	vkb::InstanceBuilder builder;

	//make the vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Executor")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	//grab the instance 
	_instance = vkb_inst.instance;
	_debug_messenger = vkb_inst.debug_messenger;

	//< init_instance
	// 
	//> init_device
	glfwCreateWindowSurface(_instance, window, nullptr, &_surface);

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
	

	//use vkbootstrap to select a gpu. 
	//We want a gpu that can write to the glfw surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features(baseFeatures)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_required_features_11(features11)
		.set_surface(_surface)
		.select()
		.value();


	msaa_samples = vkinit::getMaxAvailableSampleCount(physicalDevice.properties);
	if (msaa_samples > VK_SAMPLE_COUNT_4_BIT)
		msaa_samples = VK_SAMPLE_COUNT_4_BIT;

	//create the final vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;
	//< init_device

	//> init_queue
		// use vkbootstrap to get a Graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU,_device,_surface };

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

void VulkanEngine::init_swapchain()
{
	create_swapchain(_windowExtent.width, _windowExtent.height);

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

void VulkanEngine::init_commands()
{
	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		_mainDeletionQueue.push_function([=]() { vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr); });
	}

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

	// allocate the imgui command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

	_mainDeletionQueue.push_function([=]() { vkDestroyCommandPool(_device, _immCommandPool, nullptr); });
}
void VulkanEngine::init_sync_structures()
{
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));

	_mainDeletionQueue.push_function([=]() { vkDestroyFence(_device, _immFence, nullptr); });

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
			});
	}
}

void VulkanEngine::init_descriptors()
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
		cascaded_shadows_descriptor_layout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT |VK_SHADER_STAGE_GEOMETRY_BIT);
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

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VK_CHECK(vkResetFences(_device, 1, &_immFence));
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	VkCommandBuffer cmd = _immCommandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void VulkanEngine::init_pipelines()
{
	init_compute_pipelines();
	metalRoughMaterial.build_pipelines(this);
	cascadedShadows.build_pipelines(this);
	skyBoxPSO.build_pipelines(this);
	HdrPSO.build_pipelines(this);
	depthPrePassPSO.build_pipelines(this);
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

void VulkanEngine::init_compute_pipelines()
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

void VulkanEngine::init_triangle_pipeline()
{
	VkShaderModule triangleFragShader;
	if (!vkutil::load_shader_module("../../shaders/colored_triangle.frag.spv", _device, &triangleFragShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	}
	else {
		fmt::print("Triangle fragment shader succesfully loaded");
	}

	VkShaderModule triangleVertexShader;
	if (!vkutil::load_shader_module("../../shaders/colored_triangle.vert.spv", _device, &triangleVertexShader)) {
		fmt::print("Error when building the triangle vertex shader module");
	}
	else {
		fmt::print("Triangle vertex shader succesfully loaded");
	}

	//build the pipeline layout that controls the inputs/outputs of the shader
	//we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

	PipelineBuilder pipelineBuilder;

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.set_multisampling_none();
	//no blending
	pipelineBuilder.disable_blending();
	//no depth testing
	pipelineBuilder.disable_depthtest();

	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

	//finally build the pipeline
	_trianglePipeline = pipelineBuilder.build_pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
		vkDestroyPipeline(_device, _trianglePipeline, nullptr);
		});
}

void VulkanEngine::init_mesh_pipeline()
{
	VkShaderModule triangleFragShader;
	if (!vkutil::load_shader_module("shaders/tex_image.spv", _device, &triangleFragShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	}
	else {
		fmt::print("Triangle fragment shader succesfully loaded");
	}

	VkShaderModule triangleVertexShader;
	if (!vkutil::load_shader_module("shaders/colored_triangle_mesh.spv", _device, &triangleVertexShader)) {
		fmt::print("Error when building the triangle vertex shader module");
	}
	else {
		fmt::print("Triangle vertex shader succesfully loaded");
	}

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
	pipeline_layout_info.setLayoutCount = 1;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

	PipelineBuilder pipelineBuilder;

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _meshPipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.set_multisampling_none();
	//no blending
	pipelineBuilder.disable_blending();

	//pipelineBuilder.enable_blending_additive();

	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(_depthImage.imageFormat);
	//finally build the pipeline
	_meshPipeline = pipelineBuilder.build_pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
		});
}

void VulkanEngine::init_buffers()
{
	ClusterValues.AABBVolumeGridSSBO = create_buffer(ClusterValues.numClusters * sizeof(VolumeTileAABB), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
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

	ClusterValues.screenToViewSSBO = create_and_upload(sizeof(ScreenToView), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, &screen);

	ClusterValues.lightSSBO = create_and_upload(pointData.pointLights.size() * sizeof(PointLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, pointData.pointLights.data());
	auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;
	ClusterValues.lightIndexListSSBO = create_buffer(sizeof(uint32_t) * totalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	ClusterValues.lightGridSSBO = create_buffer(ClusterValues.numClusters * sizeof(LightGrid), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	uint32_t val = 0;
	for (uint32_t i = 0; i < 2; i++)
	{
		ClusterValues.lightGlobalIndex[i] = create_and_upload(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, &val);
	}
	//ClusterValues.lightIndexGlobalCountSSBO = create_and_upload(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,  VMA_MEMORY_USAGE_GPU_ONLY,&val);

	_mainDeletionQueue.push_function([=]() {
		destroy_buffer(ClusterValues.lightSSBO);
		destroy_buffer(ClusterValues.lightGridSSBO);
		destroy_buffer(ClusterValues.screenToViewSSBO);
		destroy_buffer(ClusterValues.AABBVolumeGridSSBO);
		destroy_buffer(ClusterValues.lightIndexListSSBO);
		destroy_buffer(ClusterValues.lightGlobalIndex[0]);
		destroy_buffer(ClusterValues.lightGlobalIndex[1]);
		});
}
void VulkanEngine::init_default_data() {
	forward_passes.push_back(vkutil::MaterialPass::forward);
	forward_passes.push_back(vkutil::MaterialPass::transparency);

	directLight = DirectionalLight(glm::normalize(glm::vec4(-20.0f, -50.0f, -20.0f, 1.f)), glm::vec4(1.5f), glm::vec4(1.0f));
	//Create Shadow render target
	_shadowDepthImage = vkutil::create_image_empty(VkExtent3D(2048, 2048, 1), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, this, VK_IMAGE_VIEW_TYPE_2D_ARRAY, false, shadows.getCascadeLevels());

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

	_depthPyramid = vkutil::create_image_empty(VkExtent3D(depthPyramidWidth, depthPyramidHeight,1),VK_FORMAT_R32_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		this, VK_IMAGE_VIEW_TYPE_2D, true, 1,VK_SAMPLE_COUNT_1_BIT, depthPyramidLevels);

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

void VulkanEngine::init_imgui()
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
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

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
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosenGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
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
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		});
}

void VulkanEngine::destroy_swapchain()
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {

		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	// allocate buffer
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;
	

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));

	return newBuffer;
}

AllocatedBuffer VulkanEngine::create_and_upload(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data)
{
	AllocatedBuffer stagingBuffer = create_buffer(allocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);


	void* bufferData = nullptr;
	vmaMapMemory(_allocator, stagingBuffer.allocation, &bufferData);
	memcpy(bufferData, data, allocSize);
	
	AllocatedBuffer DataBuffer = create_buffer(allocSize, usage, memoryUsage);

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy dataCopy{ 0 };
		dataCopy.dstOffset = 0;
		dataCopy.srcOffset = 0;
		dataCopy.size = allocSize;

		vkCmdCopyBuffer(cmd, stagingBuffer.buffer, DataBuffer.buffer, 1, &dataCopy);
		});
	vmaUnmapMemory(_allocator, stagingBuffer.allocation);
	destroy_buffer(stagingBuffer);
	return DataBuffer;
}


void VulkanEngine::ready_mesh_draw(VkCommandBuffer cmd)
{

}

void VulkanEngine::ready_cull_data(VkCommandBuffer cmd)
{
	
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	newSurface.vertexBuffer = create_buffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);


	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

	newSurface.indexBuffer = create_buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = nullptr;
	vmaMapMemory(_allocator, staging.allocation, &data);
	// copy vertex buffer
	memcpy(data, vertices.data(), vertexBufferSize);
	// copy index buffer
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
		});

	vmaUnmapMemory(_allocator, staging.allocation);
	destroy_buffer(staging);

	return newSurface;

}

AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	AllocatedImage newImage;
	newImage.imageFormat = format;
	newImage.imageExtent = size;

	VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
	if (mipmapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag, VK_IMAGE_VIEW_TYPE_2D);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

	return newImage;
}

AllocatedImage VulkanEngine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	size_t data_size = size.depth * size.width * size.height * 4;
	AllocatedBuffer uploadbuffer = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(uploadbuffer.info.pMappedData, data, data_size);

	AllocatedImage new_image = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	immediate_submit([&](VkCommandBuffer cmd) {
		vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copyRegion);

		if (mipmapped) {
			vkutil::generate_mipmaps(cmd, new_image.image, VkExtent2D{ new_image.imageExtent.width,new_image.imageExtent.height });
		}
		else {
			vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		});
	destroy_buffer(uploadbuffer);
	return new_image;
}

void VulkanEngine::destroy_image(const AllocatedImage& img)
{
	vkDestroyImageView(_device, img.imageView, nullptr);
	vmaDestroyImage(_allocator, img.image, img.allocation);
}

void VulkanEngine::update_scene()
{
	float currentFrame = glfwGetTime();
	float deltaTime = currentFrame - delta.lastFrame;
	delta.lastFrame = currentFrame;
	mainCamera.update(deltaTime);
	mainDrawContext.OpaqueSurfaces.clear();

	//sceneData.view = mainCamera.getViewMatrix();
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
	vmaMapMemory(_allocator, ClusterValues.lightSSBO.allocation, &data);
	memcpy(data, pointData.pointLights.data(), pointData.pointLights.size() * sizeof(PointLight));
	vmaUnmapMemory(_allocator, ClusterValues.lightSSBO.allocation);


	//uint32_t* val = (uint32_t*)ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].allocation->GetMappedData();
	//*val = 0;
	void* val = nullptr;
	vmaMapMemory(_allocator, ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].allocation, &val);
	uint32_t* value = (uint32_t*)val;
	*value = 0;
	vmaUnmapMemory(_allocator, ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].allocation);

	
	cascadeData = shadows.getCascades(this);
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
	scene_data.ConfigData.x = mainCamera.getNearClip();
	scene_data.ConfigData.y = mainCamera.getFarClip();

	//Prepare Render objects
	loadedScenes["sponza"]->Draw(glm::mat4{ 1.f }, drawCommands);
	loadedScenes["cube"]->Draw(glm::mat4{ 1.f }, skyDrawCommands);
	loadedScenes["plane"]->Draw(glm::mat4{ 1.f }, imageDrawCommands);

	//Register render objects for draw indirect
	//scene_manager.RegisterObjectBatch(drawCommands);
}

void VulkanEngine::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
	app->mainCamera.processKeyInput(window, key, action);
}

void VulkanEngine::cursor_callback(GLFWwindow* window, double xpos, double ypos)
{
	auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
	app->mainCamera.processMouseMovement(window, xpos, ypos);
}