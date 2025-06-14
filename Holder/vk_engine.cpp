﻿#include "vk_engine.h"

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

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <vma/vk_mem_alloc.h>

VulkanEngine* loadedEngine = nullptr;



VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(_windowExtent.width, _windowExtent.height, "Black key", nullptr, nullptr);
	if (window == nullptr)
		throw std::exception("FATAL ERROR: Failed to create window");

	init_vulkan();

	init_commands();
	
	init_sync_structures();

	init_imgui();

	_isInitialized = true;
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

//
//
//void VulkanEngine::draw()
//{
//	ZoneScoped;
//	auto start_update = std::chrono::system_clock::now();
//	//wait until the gpu has finished rendering the last frame. Timeout of 1 second
//	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
//	
//	auto end_update = std::chrono::system_clock::now();
//	auto elapsed_update = std::chrono::duration_cast<std::chrono::microseconds>(end_update - start_update);
//	stats.update_time = elapsed_update.count() / 1000.f;
//
//	get_current_frame()._deletionQueue.flush();
//	get_current_frame()._frameDescriptors.clear_pools(_device);
//	
//	//request image from the swapchain
//	uint32_t swapchainImageIndex;
//	VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
//
//	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
//		resize_requested = true;
//		return;
//	}
//
//	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height);
//	_drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width);
//
//	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));
//
//	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
//	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));
//
//	//naming it cmd for shorter writing
//	VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
//
//	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
//	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
//
//	//> draw_first
//	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
//
//	// transition our main draw image into general layout so we can write into it
//	// we will overwrite it all so we dont care about what was the older layout
//	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//	vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
//	vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//	vkutil::transition_image(cmd, _shadowDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
//	vkutil::transition_image(cmd, _hdrImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//	draw_main(cmd);
//
//	draw_post_process(cmd);
//
//	//transtion the draw image and the swapchain image into their correct transfer layouts
//	vkutil::transition_image(cmd, _hdrImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
//	//vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
//	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
//
//	//< draw_first
//	//> imgui_draw
//	// execute a copy from the draw image into the swapchain
//	vkutil::copy_image_to_image(cmd, _hdrImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
//	//vkutil::copy_image_to_image(cmd, _resolveImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
//
//	// set swapchain image layout to Attachment Optimal so we can draw it
//	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//
//	//draw imgui into the swapchain image
//	draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);
//
//	// set swapchain image layout to Present so we can draw it
//	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
//
//	//finalize the command buffer (we can no longer add commands, but it can now be executed)
//	VK_CHECK(vkEndCommandBuffer(cmd));
//
//	//prepare the submission to the queue. 
//	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
//	//we will signal the _renderSemaphore, to signal that rendering has finished
//
//	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
//
//	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
//	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);
//
//	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);
//
//	//submit command buffer to the queue and execute it.
//	// _renderFence will now block until the graphic commands finish execution
//	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));
//
//	//prepare present
//	// this will put the image we just rendered to into the visible window.
//	// we want to wait on the _renderSemaphore for that, 
//	// as its necessary that drawing commands have finished before the image is displayed to the user
//	VkPresentInfoKHR presentInfo = vkinit::present_info();
//
//	presentInfo.pSwapchains = &_swapchain;
//	presentInfo.swapchainCount = 1;
//
//	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
//	presentInfo.waitSemaphoreCount = 1;
//
//	presentInfo.pImageIndices = &swapchainImageIndex;
//
//	VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
//	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
//		resize_requested = true;
//		return;
//	}
//	//increase the number of frames drawn
//	_frameNumber++;
//	
//}


//void VulkanEngine::draw_post_process(VkCommandBuffer cmd)
//{
//	ZoneScoped;
//	vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//	//vkutil::transition_image(cmd, _presentImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//	VkClearValue clear{ 1.0f, 1.0f, 1.0f, 1.0f };
//	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_hdrImage.imageView,nullptr, &clear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//	VkRenderingInfo hdrRenderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);
//	vkCmdBeginRendering(cmd, &hdrRenderInfo);
//	draw_hdr(cmd);
//	vkCmdEndRendering(cmd);
//}
//
//void VulkanEngine::draw_main(VkCommandBuffer cmd)
//{
//	ZoneScoped;
//	auto main_start = std::chrono::system_clock::now();
//
//	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
//	VkRenderingInfo earlyDepthRenderInfo = vkinit::rendering_info(_windowExtent, nullptr, &depthAttachment);
//	vkCmdBeginRendering(cmd, &earlyDepthRenderInfo);
//	
//	draw_early_depth(cmd);
//	
//	vkCmdEndRendering(cmd);
//
//	if (render_shadowMap)
//	{
//		VkRenderingAttachmentInfo shadowDepthAttachment = vkinit::depth_attachment_info(_shadowDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
//		VkExtent2D shadowExtent{};
//		shadowExtent.width = _shadowDepthImage.imageExtent.width;
//		shadowExtent.height = _shadowDepthImage.imageExtent.height;
//		VkRenderingInfo shadowRenderInfo = vkinit::rendering_info(shadowExtent, nullptr, &shadowDepthAttachment, shadows.getCascadeLevels());
//		auto startShadow = std::chrono::system_clock::now();
//		vkCmdBeginRendering(cmd, &shadowRenderInfo);
//
//		draw_shadows(cmd);
//
//		vkCmdEndRendering(cmd);
//		auto endShadow = std::chrono::system_clock::now();
//		auto elapsedShadow = std::chrono::duration_cast<std::chrono::microseconds>(endShadow - startShadow);
//		stats.shadow_pass_time = elapsedShadow.count() / 1000.f;
//		render_shadowMap = false;
//	}
//	
//	//Cull lights
//	cull_lights(cmd);
//
//	VkClearValue geometryClear{ 1.0,1.0,1.0,1.0f };
//	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, &_resolveImage.imageView, &geometryClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//	VkRenderingAttachmentInfo depthAttachment2 = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,VK_ATTACHMENT_LOAD_OP_LOAD);
//
//	vkutil::transition_image(cmd, _shadowDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//	VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment2);
//	
//	auto start = std::chrono::system_clock::now();
//	vkCmdBeginRendering(cmd, &renderInfo);
//
//	draw_geometry(cmd);
//
//	vkCmdEndRendering(cmd);
//	auto end = std::chrono::system_clock::now();
//	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
//	stats.mesh_draw_time = elapsed.count() / 1000.f;
//	
//	auto main_end = std::chrono::system_clock::now();
//	auto main_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(main_end - main_start);
//	//stats.frametime = main_elapsed.count() / 1000.f;
//	VkRenderingAttachmentInfo colorAttachment2 = vkinit::attachment_info(_drawImage.imageView, &_resolveImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//	VkRenderingAttachmentInfo depthAttachment3 = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD,VK_ATTACHMENT_STORE_OP_DONT_CARE);
//	VkRenderingInfo backRenderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment2, &depthAttachment3);
//	vkCmdBeginRendering(cmd, &backRenderInfo);
//
//	draw_background(cmd);
//
//	vkCmdEndRendering(cmd);
//}
//
//void VulkanEngine::cull_lights(VkCommandBuffer cmd)
//{
//	CullData culling_information;
//
//	VkDescriptorSet cullingDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _cullLightsDescriptorLayout);
//
//	//write the buffer
//	//auto* pointBuffer = ClusterValues.lightSSBO.allocation->GetMappedData();
//	//memcpy(pointBuffer, pointData.pointLights.data(), pointData.pointLights.size() * sizeof(PointLight));
//	
//	DescriptorWriter writer;
//	writer.write_buffer(0, ClusterValues.AABBVolumeGridSSBO.buffer, ClusterValues.numClusters * sizeof(VolumeTileAABB), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	writer.write_buffer(1, ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	writer.write_buffer(2, ClusterValues.lightSSBO.buffer, pointData.pointLights.size() * sizeof(PointLight), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;
//	writer.write_buffer(3, ClusterValues.lightIndexListSSBO.buffer, sizeof(uint32_t) * totalLightCount, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	writer.write_buffer(4, ClusterValues.lightGridSSBO.buffer, ClusterValues.numClusters * sizeof(LightGrid), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	writer.write_buffer(5, ClusterValues.lightGlobalIndex[_frameNumber % FRAME_OVERLAP].buffer, sizeof(uint32_t), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	writer.update_set(_device, cullingDescriptor);
//
//	culling_information.view = mainCamera.matrices.view;
//	culling_information.lightCount = pointData.pointLights.size();
//	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _cullLightsPipeline);
//
//	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _cullLightsPipelineLayout, 0, 1, &cullingDescriptor, 0, nullptr);
//
//	vkCmdPushConstants(cmd, _cullLightsPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullData), &culling_information);
//	vkCmdDispatch(cmd, 16, 9, 24);
//}
//
//void VulkanEngine::draw_early_depth(VkCommandBuffer cmd)
//{
//	//Quick frustum culling pass
//	draws.reserve(drawCommands.OpaqueSurfaces.size());
//	for (int i = 0; i < drawCommands.OpaqueSurfaces.size(); i++) {
//		if (black_key::is_visible(drawCommands.OpaqueSurfaces[i], sceneData.viewproj)) {
//			draws.push_back(i);
//		}
//	}
//
//	std::sort(draws.begin(), draws.end(), [&](const auto& iA, const auto& iB) {
//		const RenderObject& A = drawCommands.OpaqueSurfaces[iA];
//		const RenderObject& B = drawCommands.OpaqueSurfaces[iB];
//		if (A.material == B.material) {
//			return A.indexBuffer < B.indexBuffer;
//		}
//		else {
//			return A.material < B.material;
//		}
//		});
//
//	//allocate a new uniform buffer for the scene data
//	AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
//
//	//add it to the deletion queue of this frame so it gets deleted once its been used
//	get_current_frame()._deletionQueue.push_function([=, this]() {
//		destroy_buffer(gpuSceneDataBuffer);
//		});
//
//	//write the buffer
//	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
//	*sceneUniformData = sceneData;
//
//	//create a descriptor set that binds that buffer and update it
//	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);
//
//	DescriptorWriter writer;
//	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
//	writer.update_set(_device, globalDescriptor);
//	
//	MaterialPipeline* lastPipeline = nullptr;
//	MaterialInstance* lastMaterial = nullptr;
//	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
//
//	auto draw = [&](const RenderObject& r) {
//		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrePassPSO.earlyDepthPipeline.pipeline);
//		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrePassPSO.earlyDepthPipeline.layout, 0, 1,
//			&globalDescriptor, 0, nullptr);
//
//		VkViewport viewport = {};
//		viewport.x = 0;
//		viewport.y = 0;
//		viewport.width = (float)_windowExtent.width;
//		viewport.height = (float)_windowExtent.height;
//		viewport.minDepth = 0.f;
//		viewport.maxDepth = 1.f;
//
//		vkCmdSetViewport(cmd, 0, 1, &viewport);
//
//		VkRect2D scissor = {};
//		scissor.offset.x = 0;
//		scissor.offset.y = 0;
//		scissor.extent.width = _windowExtent.width;
//		scissor.extent.height = _windowExtent.height;
//		vkCmdSetScissor(cmd, 0, 1, &scissor);
//		if (r.indexBuffer != lastIndexBuffer)
//		{
//			lastIndexBuffer = r.indexBuffer;
//			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
//		}
//		// calculate final mesh matrix
//		GPUDrawPushConstants push_constants;
//		push_constants.worldMatrix = r.transform;
//		push_constants.vertexBuffer = r.vertexBufferAddress;
//
//		vkCmdPushConstants(cmd, depthPrePassPSO.earlyDepthPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
//		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
//		};
//
//	for (auto& r : draws) {
//		draw(drawCommands.OpaqueSurfaces[r]);
//	}
//}
//
//
//void VulkanEngine::draw_shadows(VkCommandBuffer cmd)
//{
//	ZoneScoped;
//	//allocate a new uniform buffer for the scene data
//	AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
//
//	//add it to the deletion queue of this frame so it gets deleted once its been used
//	get_current_frame()._deletionQueue.push_function([=, this]() {
//		destroy_buffer(gpuSceneDataBuffer);
//		});
//
//	//write the buffer
//	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
//	*sceneUniformData = sceneData;
//
//	//create a descriptor set that binds that buffer and update it
//	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);
//
//	DescriptorWriter writer;
//	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
//	writer.update_set(_device, globalDescriptor);
//
//	MaterialPipeline* lastPipeline = nullptr;
//	MaterialInstance* lastMaterial = nullptr;
//	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
//
//	auto draw = [&](const RenderObject& r) {
//		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cascadedShadows.shadowPipeline.pipeline);
//		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cascadedShadows.shadowPipeline.layout, 0, 1,
//			&globalDescriptor, 0, nullptr);
//
//		VkViewport viewport = {};
//		viewport.x = 0;
//		viewport.y = 0;
//		viewport.width = (float)_shadowDepthImage.imageExtent.width;
//		viewport.height = (float)_shadowDepthImage.imageExtent.height;
//		viewport.minDepth = 0.f;
//		viewport.maxDepth = 1.f;
//
//		vkCmdSetViewport(cmd, 0, 1, &viewport);
//
//		VkRect2D scissor = {};
//		scissor.offset.x = 0;
//		scissor.offset.y = 0;
//		scissor.extent.width = _shadowDepthImage.imageExtent.width;
//		scissor.extent.height = _shadowDepthImage.imageExtent.height;
//		vkCmdSetScissor(cmd, 0, 1, &scissor);
//		//vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cascadedShadows.shadowPipeline.layout, 1, 1,
//			//&cascadedShadows.matData.materialSet, 0, nullptr);
//		if (r.indexBuffer != lastIndexBuffer)
//		{
//			lastIndexBuffer = r.indexBuffer;
//			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
//		}
//		// calculate final mesh matrix
//		GPUDrawPushConstants push_constants;
//		push_constants.worldMatrix = r.transform;
//		push_constants.vertexBuffer = r.vertexBufferAddress;
//
//		vkCmdPushConstants(cmd, cascadedShadows.shadowPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
//
//		stats.shadow_drawcall_count++;
//		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
//		};
//	for (auto& r : draws) {
//		draw(drawCommands.OpaqueSurfaces[r]);
//	}
//}
//
//
//void VulkanEngine::draw_background(VkCommandBuffer cmd)
//{
//	ZoneScoped;
//	std::vector<uint32_t> b_draws;
//	b_draws.reserve(skyDrawCommands.OpaqueSurfaces.size());
//	//allocate a new uniform buffer for the scene data
//	AllocatedBuffer skySceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
//
//	//add it to the deletion queue of this frame so it gets deleted once its been used
//	get_current_frame()._deletionQueue.push_function([=, this]() {
//		destroy_buffer(skySceneDataBuffer);
//		});
//
//	//write the buffer
//	GPUSceneData* sceneUniformData = (GPUSceneData*)skySceneDataBuffer.allocation->GetMappedData();
//	*sceneUniformData = sceneData;
//
//	//create a descriptor set that binds that buffer and update it
//	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _skyboxDescriptorLayout);
//
//	DescriptorWriter writer;
//	writer.write_buffer(0, skySceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
//	writer.write_image(1, _skyImage.imageView, _cubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//	writer.update_set(_device, globalDescriptor);
//
//	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
//	auto b_draw = [&](const RenderObject& r) {
//		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyBoxPSO.skyPipeline.pipeline);
//		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyBoxPSO.skyPipeline.layout, 0, 1,
//			&globalDescriptor, 0, nullptr);
//
//		VkViewport viewport = {};
//		viewport.x = 0;
//		viewport.y = 0;
//		viewport.width = (float)_windowExtent.width;
//		viewport.height = (float)_windowExtent.height;
//		viewport.minDepth = 0.f;
//		viewport.maxDepth = 1.f;
//
//		vkCmdSetViewport(cmd, 0, 1, &viewport);
//
//		VkRect2D scissor = {};
//		scissor.offset.x = 0;
//		scissor.offset.y = 0;
//		scissor.extent.width = _windowExtent.width;
//		scissor.extent.height = _windowExtent.height;
//		vkCmdSetScissor(cmd, 0, 1, &scissor);
//
//		if (r.indexBuffer != lastIndexBuffer)
//		{
//			lastIndexBuffer = r.indexBuffer;
//			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
//		}
//		// calculate final mesh matrix
//		GPUDrawPushConstants push_constants;
//		push_constants.worldMatrix = r.transform;
//		push_constants.vertexBuffer = r.vertexBufferAddress;
//
//		vkCmdPushConstants(cmd, skyBoxPSO.skyPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
//		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
//		};
//	b_draw(skyDrawCommands.OpaqueSurfaces[0]);
//	skyDrawCommands.OpaqueSurfaces.clear();
//}
//
//
//void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
//{
//	ZoneScoped;
//	//allocate a new uniform buffer for the scene data
//	AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
//
//	//add it to the deletion queue of this frame so it gets deleted once its been used
//	get_current_frame()._deletionQueue.push_function([=, this]() {
//		destroy_buffer(gpuSceneDataBuffer);
//		});
//
//	//write the buffer
//	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
//	*sceneUniformData = sceneData;
//
//	//create a descriptor set that binds that buffer and update it
//	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);
//
//	static auto totalLightCount = ClusterValues.maxLightsPerTile * ClusterValues.numClusters;
//
//	DescriptorWriter writer;
//	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
//	writer.write_image(2, _shadowDepthImage.imageView, _depthSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//	writer.write_image(3, IBL._irradianceCube.imageView, IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//	writer.write_image(4, IBL._lutBRDF.imageView, IBL._lutBRDFSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//	writer.write_image(5, IBL._preFilteredCube.imageView, IBL._irradianceCubeSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//	writer.write_buffer(6, ClusterValues.lightSSBO.buffer, pointData.pointLights.size() * sizeof(PointLight), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	writer.write_buffer(7, ClusterValues.screenToViewSSBO.buffer, sizeof(ScreenToView), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	writer.write_buffer(8, ClusterValues.lightIndexListSSBO.buffer, totalLightCount * sizeof(uint32_t), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//	writer.write_buffer(9, ClusterValues.lightGridSSBO.buffer, ClusterValues.numClusters * sizeof(LightGrid), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
//
//	writer.update_set(_device, globalDescriptor);
//
//	MaterialPipeline* lastPipeline = nullptr;
//	MaterialInstance* lastMaterial = nullptr;
//	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
//
//	auto draw = [&](const RenderObject& r) {
//		if (r.material != lastMaterial) {
//			lastMaterial = r.material;
//			if (r.material->pipeline != lastPipeline) {
//
//				lastPipeline = r.material->pipeline;
//				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
//				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1,
//					&globalDescriptor, 0, nullptr);
//
//				VkViewport viewport = {};
//				viewport.x = 0;
//				viewport.y = 0;
//				viewport.width = (float)_windowExtent.width;
//				viewport.height = (float)_windowExtent.height;
//				viewport.minDepth = 0.f;
//				viewport.maxDepth = 1.f;
//				vkCmdSetViewport(cmd, 0, 1, &viewport);
//
//				VkRect2D scissor = {};
//				scissor.offset.x = 0;
//				scissor.offset.y = 0;
//				scissor.extent.width = _windowExtent.width;
//				scissor.extent.height = _windowExtent.height;
//
//				vkCmdSetScissor(cmd, 0, 1, &scissor);
//			}
//
//			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
//				&r.material->materialSet, 0, nullptr);
//		}
//		if (r.indexBuffer != lastIndexBuffer) {
//			lastIndexBuffer = r.indexBuffer;
//			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
//		}
//		// calculate final mesh matrix
//		GPUDrawPushConstants push_constants;
//		push_constants.worldMatrix = r.transform;
//		push_constants.vertexBuffer = r.vertexBufferAddress;
//
//		vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
//
//		stats.drawcall_count++;
//		stats.triangle_count += r.indexCount / 3;
//		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
//		};
//
//	stats.drawcall_count = 0;
//	stats.triangle_count = 0;
//
//	for (auto& r : draws) {
//		draw(drawCommands.OpaqueSurfaces[r]);
//	}
//
//	for (auto& r : drawCommands.TransparentSurfaces) {
//		draw(r);
//	}
//
//	// we delete the draw commands now that we processed them
//	drawCommands.OpaqueSurfaces.clear();
//	drawCommands.TransparentSurfaces.clear();
//	draws.clear();
//}
//
//void VulkanEngine::draw_hdr(VkCommandBuffer cmd)
//{
//	ZoneScoped;
//	std::vector<uint32_t> draws;
//	draws.reserve(imageDrawCommands.OpaqueSurfaces.size());
//
//	//create a descriptor set that binds that buffer and update it
//	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _drawImageDescriptorLayout);
//
//	DescriptorWriter writer;
//	writer.write_image(0, _resolveImage.imageView, _defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//	writer.update_set(_device, globalDescriptor);
//
//	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
//	auto draw = [&](const RenderObject& r) {
//		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, HdrPSO.renderImagePipeline.pipeline);
//		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, HdrPSO.renderImagePipeline.layout, 0, 1,
//			&globalDescriptor, 0, nullptr);
//
//		VkViewport viewport = {};
//		viewport.x = 0;
//		viewport.y = 0;
//		viewport.width = (float)_windowExtent.width;
//		viewport.height = (float)_windowExtent.height;
//		viewport.minDepth = 0.f;
//		viewport.maxDepth = 1.f;
//		vkCmdSetViewport(cmd, 0, 1, &viewport);
//
//		VkRect2D scissor = {};
//		scissor.offset.x = 0;
//		scissor.offset.y = 0;
//		scissor.extent.width = _windowExtent.width;
//		scissor.extent.height = _windowExtent.height;
//		vkCmdSetScissor(cmd, 0, 1, &scissor);
//
//		if (r.indexBuffer != lastIndexBuffer)
//		{
//			lastIndexBuffer = r.indexBuffer;
//			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
//		}
//		// calculate final mesh matrix
//		GPUDrawPushConstants push_constants;
//		push_constants.worldMatrix = r.transform;
//		push_constants.vertexBuffer = r.vertexBufferAddress;
//
//		vkCmdPushConstants(cmd, HdrPSO.renderImagePipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
//		vkCmdDraw(cmd, 3, 1, 0, 0);
//		};
//	draw(imageDrawCommands.OpaqueSurfaces[0]);
//}
//
//void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
//{
//	auto start_imgui = std::chrono::system_clock::now();
//	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr,nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//	VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);
//
//	vkCmdBeginRendering(cmd, &renderInfo);
//
//	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
//
//	vkCmdEndRendering(cmd);
//	auto end_imgui = std::chrono::system_clock::now();
//	auto elapsed_imgui = std::chrono::duration_cast<std::chrono::microseconds>(end_imgui - start_imgui);
//	stats.ui_draw_time = elapsed_imgui.count() / 1000.f;
//}

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
	_drawImage = vkutil::create_image_empty(ImageExtent,VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,this, VK_IMAGE_VIEW_TYPE_2D, false, 1, VK_SAMPLE_COUNT_4_BIT);
	_hdrImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, this);
	_resolveImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, this);

	resize_requested = false;
}

void VulkanEngine::run()
{
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
	
	VkPhysicalDeviceFeatures baseFeatures{};
	baseFeatures.geometryShader = true;
	baseFeatures.samplerAnisotropy = true;
	baseFeatures.sampleRateShading = true;
	//use vkbootstrap to select a gpu. 
	//We want a gpu that can write to the glfw surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features(baseFeatures)
		.set_required_features_13(features)
		.set_required_features_12(features12)
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
	
}

void VulkanEngine::init_commands()
{
	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

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
	//void* bufferData = stagingBuffer.allocation->GetMappedData();

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

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	newSurface.vertexBuffer = create_buffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);


	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

	newSurface.indexBuffer = create_buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	//void* data = staging.allocation->GetMappedData();
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

VkSampleCountFlagBits VulkanEngine::GetMSAASampleCount()
{
	return msaa_samples;
}