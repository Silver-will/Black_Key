﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include "vk_loader.h"
#include "vk_types.h"
#include "engine_util.h"
#include "vk_descriptors.h"
#include "vk_renderer.h"
//#include <vma/vk_mem_alloc.h>
#include "camera.h"
#include "engine_psos.h"
#include "shadows.h"
#include "Lights.h"
#include <chrono>
#include "scene_manager.h"
#include "resource_manager.h"
#include <ktxvulkan.h>

#define USE_BINDLESS 0

struct FrameData {

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors;
	
	DescriptorAllocator bindless_material_descriptor;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	//run main loop
	void run();

	Camera mainCamera;
	ResourceManager resource_manager;
	SceneManager scene_manager;
	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;// GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;
	ShadowPipelineResources cascadedShadows;
	SkyBoxPipelineResources skyBoxPSO;
	BloomBlurPipelineObject postProcessPSO;
	RenderImagePipelineObject HdrPSO;
	EarlyDepthPipelineObject depthPrePassPSO;

	DescriptorAllocator globalDescriptorAllocator;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSet bindless_descriptor;

	VkDescriptorSetLayout _shadowSceneDescriptorLayout;
	std::vector<vkutil::MaterialPass> forward_passes;

	bool resize_requested = false;
	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool render_shadowMap{ true };
	bool stop_rendering{ false };
	bool debugShadowMap = false;
	bool use_bindless = true;

	struct {
		float lastFrame;
	} delta;
	VkExtent2D _windowExtent{ 1920,1080 };
	float _aspect_width = 1920;
	float _aspect_height = 1080;

	bool debugBuffer = false;
	bool readDebugBuffer = false;

	GLFWwindow* window{ nullptr };

	static VulkanEngine& Get();

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	Cascade cascadeData;
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;
	DeletionQueue _mainDeletionQueue;
	VmaAllocator _allocator;
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	AllocatedImage _depthResolveImage;
	AllocatedImage _resolveImage;
	AllocatedImage _hdrImage;
	AllocatedImage _shadowDepthImage;
	AllocatedImage _presentImage;
	AllocatedImage _depthPyramid;

	struct {
		AllocatedImage _lutBRDF;
		AllocatedImage _irradianceCube;
		AllocatedImage _preFilteredCube;
		VkSampler _irradianceCubeSampler;
		VkSampler _lutBRDFSampler;

	}IBL;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;

	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;
	VkPipeline _cullLightsPipeline;
	VkPipelineLayout _cullLightsPipelineLayout;
	PipelineStateObject cull_objects_pso;
	PipelineStateObject depth_reduce_pso;

	GPUMeshBuffers rectangle;
	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	GPUSceneData scene_data;
	shadowData shadow_data;
	VkDescriptorSetLayout gpu_scene_data_descriptor_layout;
	VkDescriptorSetLayout _singleImageDescriptorLayout;
	VkDescriptorSetLayout _skyboxDescriptorLayout;
	VkDescriptorSetLayout _drawImageDescriptorLayout;
	VkDescriptorSetLayout _cullLightsDescriptorLayout;
	VkDescriptorSetLayout _buildClustersDescriptorLayout;
	VkDescriptorSetLayout bindless_descriptor_layout;
	VkDescriptorSetLayout compute_cull_descriptor_layout;
	VkDescriptorSetLayout depth_reduce_descriptor_layout;
	VkDescriptorSetLayout cascaded_shadows_descriptor_layout;
	//VkDescriptorSetLayout _

	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage storageImage;
	AllocatedImage errorCheckerboardImage;
	VkImageView depthPyramidMips[16];

	AllocatedImage _skyImage;
	ktxVulkanTexture _skyBoxImage;

	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;
	VkSampler cubeMapSampler;
	VkSampler depthSampler;
	VkSampler depthReductionSampler;
	DrawContext drawCommands;
	DrawContext skyDrawCommands;
	DrawContext imageDrawCommands;
	ShadowCascades shadows;

	uint32_t depthPyramidWidth;
	uint32_t depthPyramidHeight;
	uint32_t depthPyramidLevels;

	uint32_t shadowMapSize = 2048;
	EngineStats stats;
	VkSampleCountFlagBits msaa_samples;

	bool debugDepthTexture = false;

	std::vector<VkBufferMemoryBarrier> cullBarriers;

	//Clustered culling  values
	struct {
		//Configuration values
		const uint32_t gridSizeX = 16;
		const uint32_t gridSizeY = 9;
		const uint32_t gridSizeZ = 24;
		const uint32_t numClusters = gridSizeX * gridSizeY * gridSizeZ;
		const uint32_t maxLightsPerTile = 50;
		uint32_t sizeX, sizeY;

		//Storage Buffers
		AllocatedBuffer AABBVolumeGridSSBO;
		AllocatedBuffer screenToViewSSBO;
		AllocatedBuffer lightSSBO;
		AllocatedBuffer lightIndexListSSBO;
		AllocatedBuffer lightGridSSBO;
		AllocatedBuffer lightGlobalIndex[2];
	} ClusterValues;

	std::vector<uint32_t> draws;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	//lights
	DirectionalLight directLight;
	uint32_t maxLights = 100;

	struct PointLightData {

		uint32_t numOfLights = 6;
		//PointLight pointLights[100];
		std::vector<PointLight> pointLights;

	}pointData;
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
	//r
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	DrawContext mainDrawContext;
	//r
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

	void update_scene();
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);
	AllocatedBuffer create_and_upload(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data);
	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& img);

private:
	void ready_cull_data(VkCommandBuffer cmd);
	void ready_mesh_draw(VkCommandBuffer cmd);
	void load_assets();
	void execute_compute_cull(VkCommandBuffer cmd, vkutil::cullParams& cullParams, SceneManager::MeshPass* meshPass);
	void reduce_depth(VkCommandBuffer cmd);
	void pre_process_pass();
	void cull_lights(VkCommandBuffer cmd);
	void draw_shadows(VkCommandBuffer cmd);
	void draw_main(VkCommandBuffer cmd);
	void draw_post_process(VkCommandBuffer cmd);
	void draw_background(VkCommandBuffer cmd);
	void draw_geometry(VkCommandBuffer cmd);
	void draw_hdr(VkCommandBuffer cmd);
	void draw_early_depth(VkCommandBuffer cmd);
	void resize_swapchain();
	void init_vulkan();
	void init_mesh_pipeline();
	void init_default_data();
	void init_triangle_pipeline();
	void init_compute_pipelines();
	void init_imgui();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_buffers();
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void init_pipelines();
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void cursor_callback(GLFWwindow* window, double xpos, double ypos);
};
